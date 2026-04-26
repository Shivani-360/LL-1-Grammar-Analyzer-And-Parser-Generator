#define _WIN32_WINNT 0x0A00
#define WINVER 0x0A00
#include <iostream>
#include "httplib.h"
#include "json.hpp"
#include "grammar.h"
#include "first_follow.h"
#include "parsing_table.h"

using namespace std;
using json = nlohmann::json;

// ── LL(1) Predictive Parser ──────────────────────────────────────────────────
struct ParseStep {
    string stack;
    string input;
    string action;
    bool   isError;
};

vector<string> tokenise(const string& inputStr, const set<string>& terminals, string& tokenError) {
    vector<string> tokens;
    vector<string> sorted(terminals.begin(), terminals.end());
    sort(sorted.begin(), sorted.end(), [](const string& a, const string& b){ return a.size() > b.size(); });

    int pos = 0;
    while (pos < (int)inputStr.size()) {
        if (inputStr[pos] == ' ') { pos++; continue; }
        bool matched = false;
        for (auto& t : sorted) {
            if (pos + (int)t.size() <= (int)inputStr.size() &&
                inputStr.substr(pos, t.size()) == t) {
                tokens.push_back(t);
                pos += t.size();
                matched = true;
                break;
            }
        }
        if (!matched) {
            string ch(1, inputStr[pos]);
            tokens.push_back(ch);
            pos++;
        }
    }
    tokens.push_back("$");
    return tokens;
}

vector<ParseStep> ll1Parse(
    const string& inputStr, Grammar& g,
    const map<string, set<string>>& firstSets,
    const map<string, set<string>>& followSets,
    const ParsingTable& pt, bool& accepted)
{
    vector<ParseStep> steps;
    accepted = false;

    string tokenError;
    vector<string> tokens = tokenise(inputStr, g.terminals, tokenError);

    vector<string> stack;
    stack.push_back("$");
    stack.push_back(g.startSymbol);

    int ip = 0;

    auto stackToString = [&]() {
        string s;
        for (int i = (int)stack.size()-1; i >= 0; i--) s += stack[i] + " ";
        return s;
    };
    auto remainingInput = [&]() {
        string s;
        for (int i = ip; i < (int)tokens.size(); i++) s += tokens[i] + " ";
        return s;
    };

    while (true) {
        string top = stack.back();
        string curToken = (ip < (int)tokens.size()) ? tokens[ip] : "$";

        ParseStep step;
        step.stack = stackToString();
        step.input = remainingInput();
        step.isError = false;

        if (top == "$" && curToken == "$") {
            step.action = "Accept — string is valid!";
            steps.push_back(step);
            accepted = true;
            break;
        }
        if (top == "$") {
            step.action = "Error — stack empty but input remains: " + curToken;
            step.isError = true;
            steps.push_back(step);
            break;
        }

        if (g.nonTerminals.find(top) == g.nonTerminals.end()) {
            if (top == curToken) {
                step.action = "Match terminal '" + top + "', advance input";
                stack.pop_back();
                ip++;
            } else {
                step.action = "Error — expected '" + top + "' but found '" + curToken + "'";
                step.isError = true;
                steps.push_back(step);
                break;
            }
            steps.push_back(step);
            continue;
        }

        auto rowIt = pt.table.find(top);
        if (rowIt == pt.table.end() || rowIt->second.find(curToken) == rowIt->second.end() ||
            rowIt->second.at(curToken) == "") {
            step.action = "Error — no rule in table[" + top + "][" + curToken + "]";
            step.isError = true;
            steps.push_back(step);
            break;
        }

        string rule = rowIt->second.at(curToken);
        step.action = "Apply rule: " + rule;
        steps.push_back(step);
        stack.pop_back();

        int arrowPos = (int)rule.find("->");
        string rhs = rule.substr(arrowPos + 2);

        if (rhs == "ε") continue;

        vector<string> sortedNTs(g.nonTerminals.begin(), g.nonTerminals.end());
        sort(sortedNTs.begin(), sortedNTs.end(), [](const string& a, const string& b){ return a.size() > b.size(); });

        vector<string> rhsSymbols;
        int pos2 = 0;
        while (pos2 < (int)rhs.size()) {
            if (rhs[pos2] == ' ') { pos2++; continue; }
            bool foundNT = false;
            for (auto& nt : sortedNTs) {
                if (pos2 + (int)nt.size() <= (int)rhs.size() && rhs.substr(pos2, nt.size()) == nt) {
                    rhsSymbols.push_back(nt);
                    pos2 += nt.size();
                    foundNT = true;
                    break;
                }
            }
            if (!foundNT) {
                if (islower((unsigned char)rhs[pos2])) {
                    int j = pos2;
                    while (j < (int)rhs.size() && islower((unsigned char)rhs[j])) j++;
                    if (j - pos2 > 1) { rhsSymbols.push_back(rhs.substr(pos2, j-pos2)); pos2 = j; continue; }
                }
                rhsSymbols.push_back(string(1, rhs[pos2]));
                pos2++;
            }
        }
        for (int i = (int)rhsSymbols.size()-1; i >= 0; i--)
            stack.push_back(rhsSymbols[i]);
    }
    return steps;
}
// ─────────────────────────────────────────────────────────────────────────────

int main() {
    httplib::Server server;
    cout << "Server starting on port 8080..." << endl;

    // ── /parse endpoint ──────────────────────────────────────────────────────
    server.Post("/parse", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");

        try {
            json inputData = json::parse(req.body);
            vector<string> grammarLines;
            for (auto& line : inputData["grammar"]) grammarLines.push_back(line);
            string startSymbol = inputData["start_symbol"];

            Grammar g = buildGrammar(grammarLines, startSymbol);
            CheckResult check = checkGrammar(g);

            json response;
            response["errors"]   = check.errorList;
            response["warnings"] = check.warningList;

            if (!check.isValid) {
                response["valid"] = false;
                response["stopped_reason"] = "Grammar has errors. Fix them before computing sets.";
                res.set_content(response.dump(), "application/json");
                return;
            }

            if (check.warningList.size() > 0) {
                response["valid"] = false;
                string reasons;
                for (auto& w : check.warningList) reasons += w + "; ";
                response["stopped_reason"] = "Grammar issues detected: " + reasons +
                    " Use the 'Auto-Fix Grammar' button to automatically fix these issues.";
                res.set_content(response.dump(), "application/json");
                return;
            }

            map<string, set<string>> firstSets  = computeFirst(g);
            map<string, set<string>> followSets = computeFollow(g, firstSets);
            ParsingTable pt = buildParsingTable(g, firstSets, followSets);

            response["valid"]          = pt.isLL1;
            response["stopped_reason"] = "";

            json firstJson;
            for (auto& p : firstSets) { vector<string> v(p.second.begin(), p.second.end()); firstJson[p.first] = v; }
            response["first_sets"] = firstJson;

            json followJson;
            for (auto& p : followSets) { vector<string> v(p.second.begin(), p.second.end()); followJson[p.first] = v; }
            response["follow_sets"] = followJson;

            json tableJson;
            for (auto& row : pt.table)
                for (auto& col : row.second)
                    tableJson[row.first][col.first] = col.second;
            response["parsing_table"] = tableJson;
            response["conflicts"]     = pt.conflicts;
            response["isLL1"]         = pt.isLL1;

            res.set_content(response.dump(), "application/json");

        } catch (exception& e) {
            json err;
            err["valid"]  = false;
            err["errors"] = vector<string>{"Server error: " + string(e.what())};
            res.set_content(err.dump(), "application/json");
        }
    });

    // ── /auto-fix endpoint ───────────────────────────────────────────────────
    server.Post("/auto-fix", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");

        try {
            json inputData = json::parse(req.body);
            vector<string> grammarLines;
            for (auto& line : inputData["grammar"]) grammarLines.push_back(line);
            string startSymbol = inputData["start_symbol"];

            Grammar g = buildGrammar(grammarLines, startSymbol);
            CheckResult check = checkGrammar(g);

            json response;

            // Only run auto-fix if grammar structure is valid (no undefined NTs etc.)
            if (!check.isValid) {
                response["success"] = false;
                response["errors"]  = check.errorList;
                res.set_content(response.dump(), "application/json");
                return;
            }

            FixResult fr = autoFixGrammar(g);

            response["success"]              = true;
            response["had_left_recursion"]   = fr.hadLeftRecursion;
            response["had_left_factoring"]   = fr.hadLeftFactoring;
            response["fix_steps"]            = fr.fixSteps;
            response["was_fixed"]            = fr.wasFixed;

            // Produce the fixed grammar lines
            vector<string> fixedLines = grammarToLines(fr.fixedGrammar);
            response["fixed_grammar_lines"]  = fixedLines;
            response["fixed_start_symbol"]   = fr.fixedGrammar.startSymbol;

            // Now compute FIRST, FOLLOW and parsing table for the fixed grammar
            map<string, set<string>> firstSets  = computeFirst(fr.fixedGrammar);
            map<string, set<string>> followSets = computeFollow(fr.fixedGrammar, firstSets);
            ParsingTable pt = buildParsingTable(fr.fixedGrammar, firstSets, followSets);

            response["isLL1"]    = pt.isLL1;
            response["conflicts"] = pt.conflicts;

            if (pt.isLL1) {
                response["ambiguity_message"] = "No conflicts in parsing table. Fixed grammar is LL(1) — not ambiguous!";
                fr.hadAmbiguity = false;
            } else {
                response["ambiguity_message"] =
                    "Conflicts still exist after fixing left recursion and left factoring. "
                    "This grammar may be inherently ambiguous. "
                    "Ambiguity cannot be removed automatically — you must redesign the grammar manually.";
                fr.hadAmbiguity = true;
            }
            response["had_ambiguity"] = fr.hadAmbiguity;

            json firstJson;
            for (auto& p : firstSets) { vector<string> v(p.second.begin(), p.second.end()); firstJson[p.first] = v; }
            response["first_sets"] = firstJson;

            json followJson;
            for (auto& p : followSets) { vector<string> v(p.second.begin(), p.second.end()); followJson[p.first] = v; }
            response["follow_sets"] = followJson;

            json tableJson;
            for (auto& row : pt.table)
                for (auto& col : row.second)
                    tableJson[row.first][col.first] = col.second;
            response["parsing_table"] = tableJson;

            res.set_content(response.dump(), "application/json");

        } catch (exception& e) {
            json err;
            err["success"] = false;
            err["errors"]  = vector<string>{"Server error: " + string(e.what())};
            res.set_content(err.dump(), "application/json");
        }
    });

    // ── /parse-string endpoint ───────────────────────────────────────────────
    server.Post("/parse-string", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");

        try {
            json inputData = json::parse(req.body);
            vector<string> grammarLines;
            for (auto& line : inputData["grammar"]) grammarLines.push_back(line);
            string startSymbol = inputData["start_symbol"];
            string inputStr    = inputData["input_string"];

            Grammar g = buildGrammar(grammarLines, startSymbol);
            CheckResult check = checkGrammar(g);

            json response;
            if (!check.isValid || check.warningList.size() > 0) {
                response["success"] = false;
                response["error"]   = "Grammar is not valid or has left recursion/factoring issues. Fix grammar first.";
                res.set_content(response.dump(), "application/json");
                return;
            }

            map<string, set<string>> firstSets  = computeFirst(g);
            map<string, set<string>> followSets = computeFollow(g, firstSets);
            ParsingTable pt = buildParsingTable(g, firstSets, followSets);

            if (!pt.isLL1) {
                response["success"] = false;
                response["error"]   = "Grammar has conflicts — not LL(1). Cannot parse string.";
                res.set_content(response.dump(), "application/json");
                return;
            }

            bool accepted;
            vector<ParseStep> steps = ll1Parse(inputStr, g, firstSets, followSets, pt, accepted);

            json stepsJson = json::array();
            for (auto& step : steps) {
                json s;
                s["stack"]   = step.stack;
                s["input"]   = step.input;
                s["action"]  = step.action;
                s["isError"] = step.isError;
                stepsJson.push_back(s);
            }

            response["success"]  = true;
            response["accepted"] = accepted;
            response["steps"]    = stepsJson;
            res.set_content(response.dump(), "application/json");

        } catch (exception& e) {
            json err;
            err["success"] = false;
            err["error"]   = "Server error: " + string(e.what());
            res.set_content(err.dump(), "application/json");
        }
    });

    // ── OPTIONS for CORS ─────────────────────────────────────────────────────
    auto corsHandler = [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 204;
    };
    server.Options("/parse", corsHandler);
    server.Options("/auto-fix", corsHandler);
    server.Options("/parse-string", corsHandler);

    cout << "Server running at http://localhost:8080" << endl;
    cout << "Endpoints: /parse  /auto-fix  /parse-string" << endl;
    cout << "Press Ctrl+C to stop" << endl;
    server.listen("localhost", 8080);
    return 0;
}
