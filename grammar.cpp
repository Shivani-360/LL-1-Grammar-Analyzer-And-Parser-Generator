#include "grammar.h"
#include <sstream>
#include <algorithm>
#include <cctype>

using namespace std;

string trimSpaces(string s) {
    int start = 0, end = (int)s.size() - 1;
    while (start <= end && s[start] == ' ') start++;
    while (end >= start && s[end] == ' ') end--;
    if (start > end) return "";
    return s.substr(start, end - start + 1);
}

vector<string> splitString(string s, char ch) {
    vector<string> parts;
    string current = "";
    for (int i = 0; i < (int)s.size(); i++) {
        if (s[i] == ch) {
            string part = trimSpaces(current);
            if (part != "") parts.push_back(part);
            current = "";
        } else {
            current += s[i];
        }
    }
    string last = trimSpaces(current);
    if (last != "") parts.push_back(last);
    return parts;
}

bool looksLikeNonTerminal(const string& s) {
    if (s.empty()) return false;
    if (!isupper((unsigned char)s[0])) return false;
    for (int i = 1; i < (int)s.size(); i++) {
        char c = s[i];
        if (!isupper((unsigned char)c) && !isdigit((unsigned char)c) && c != '\'') return false;
    }
    return true;
}

Grammar buildGrammar(vector<string> lines, string start) {
    Grammar g;
    g.startSymbol = start;

    for (int i = 0; i < (int)lines.size(); i++) {
        string line = lines[i];
        if (line == "") continue;
        int arrowPos = (int)line.find("->");
        if (arrowPos == -1) continue;
        string leftSide = trimSpaces(line.substr(0, arrowPos));
        if (leftSide.empty()) continue;
        g.nonTerminals.insert(leftSide);
        bool alreadyIn = false;
        for (int j = 0; j < (int)g.ntList.size(); j++)
            if (g.ntList[j] == leftSide) alreadyIn = true;
        if (!alreadyIn) g.ntList.push_back(leftSide);
    }

    for (int i = 0; i < (int)lines.size(); i++) {
        string line = lines[i];
        if (line == "") continue;
        int arrowPos = (int)line.find("->");
        if (arrowPos == -1) continue;
        string leftSide  = trimSpaces(line.substr(0, arrowPos));
        string rightSide = trimSpaces(line.substr(arrowPos + 2));
        vector<string> options = splitString(rightSide, '|');
        for (int j = 0; j < (int)options.size(); j++) {
            string option = trimSpaces(options[j]);
            if (option == "#" || option == "eps" || option == "epsilon" || option == "\xce\xb5")
                option = "ε";
            g.rules[leftSide].push_back(option);
        }
    }

    vector<string> sortedNTs(g.nonTerminals.begin(), g.nonTerminals.end());
    sort(sortedNTs.begin(), sortedNTs.end(), [](const string& a, const string& b) {
        return a.size() > b.size();
    });

    for (auto& rulePair : g.rules) {
        for (auto& option : rulePair.second) {
            if (option == "ε") continue;
            int pos = 0;
            while (pos < (int)option.size()) {
                if (option[pos] == ' ' || option[pos] == '#') { pos++; continue; }
                bool isNT = false;
                for (auto& nt : sortedNTs) {
                    if (pos + (int)nt.size() <= (int)option.size() &&
                        option.substr(pos, nt.size()) == nt) {
                        isNT = true;
                        pos += nt.size();
                        break;
                    }
                }
                if (!isNT) {
                    if (islower((unsigned char)option[pos])) {
                        int j = pos;
                        while (j < (int)option.size() && islower((unsigned char)option[j])) j++;
                        if (j - pos > 1) { g.terminals.insert(option.substr(pos, j - pos)); pos = j; continue; }
                    }
                    string ch(1, option[pos]);
                    if (ch != " ") g.terminals.insert(ch);
                    pos++;
                }
            }
        }
    }
    return g;
}

CheckResult checkGrammar(Grammar g) {
    CheckResult result;
    result.isValid = true;

    if (g.rules.empty()) {
        result.isValid = false;
        result.errorList.push_back("Grammar is empty. Please add rules.");
        return result;
    }

    if (!g.startSymbol.empty() && !looksLikeNonTerminal(g.startSymbol)) {
        result.isValid = false;
        result.errorList.push_back(
            "Start symbol '" + g.startSymbol + "' is not a valid non-terminal. "
            "Non-terminals must start with an uppercase letter (e.g. S, E, T).");
        return result;
    }

    for (auto& rulePair : g.rules) {
        string lhs = rulePair.first;
        if (!looksLikeNonTerminal(lhs)) {
            result.isValid = false;
            string suggestion = lhs;
            suggestion[0] = toupper(suggestion[0]);
            result.errorList.push_back(
                "Left-hand side '" + lhs + "' is not a valid non-terminal. "
                "Non-terminals must start with an uppercase letter. "
                "Did you mean '" + suggestion + "'?");
        }
    }

    if (g.rules.find(g.startSymbol) == g.rules.end()) {
        result.isValid = false;
        result.errorList.push_back(
            "Start symbol '" + g.startSymbol + "' has no production rule. "
            "Make sure the start symbol exactly matches the left-hand side of a rule.");
    }

    if (!result.isValid) return result;

    for (auto& rulePair : g.rules) {
        string lhs = rulePair.first;
        for (auto& option : rulePair.second) {
            if (option != "ε") {
                if (option.find('#') != string::npos) {
                    result.isValid = false;
                    result.errorList.push_back(
                        "Error in rule '" + lhs + "->" + option + "': "
                        "Epsilon '#' cannot be mixed with other symbols. "
                        "Epsilon must appear alone as a separate production.");
                }
            }
        }
    }

    if (!result.isValid) return result;

    vector<string> sortedNTs(g.nonTerminals.begin(), g.nonTerminals.end());
    sort(sortedNTs.begin(), sortedNTs.end(), [](const string& a, const string& b) {
        return a.size() > b.size();
    });

    set<string> undefinedNTs;
    for (auto& rulePair : g.rules) {
        for (auto& option : rulePair.second) {
            if (option == "ε") continue;
            int pos = 0;
            while (pos < (int)option.size()) {
                if (option[pos] == ' ') { pos++; continue; }
                bool foundNT = false;
                for (auto& nt : sortedNTs) {
                    if (pos + (int)nt.size() <= (int)option.size() &&
                        option.substr(pos, nt.size()) == nt) {
                        if (g.rules.find(nt) == g.rules.end())
                            undefinedNTs.insert(nt);
                        pos += nt.size();
                        foundNT = true;
                        break;
                    }
                }
                if (!foundNT) {
                    if (isupper((unsigned char)option[pos])) {
                        int j = pos + 1;
                        while (j < (int)option.size() && (option[j] == '\'' || isdigit((unsigned char)option[j]))) j++;
                        string sym = option.substr(pos, j - pos);
                        if (g.rules.find(sym) == g.rules.end()) undefinedNTs.insert(sym);
                        pos = j;
                    } else if (islower((unsigned char)option[pos])) {
                        int j = pos;
                        while (j < (int)option.size() && islower((unsigned char)option[j])) j++;
                        pos = j > pos + 1 ? j : pos + 1;
                    } else {
                        pos++;
                    }
                }
            }
        }
    }

    for (auto& nt : undefinedNTs) {
        result.isValid = false;
        result.errorList.push_back(
            "Symbol '" + nt + "' appears in a production but has no rule defined. "
            "If it is a non-terminal, please add its production rule. "
            "If it is a terminal, rename it to lowercase.");
    }

    if (!result.isValid) return result;

    // Check left recursion
    vector<string> lrProblems;
    if (checkLeftRecursion(g, lrProblems)) {
        for (auto& p : lrProblems) {
            result.warningList.push_back(
                "Left recursion detected in: " + p);
        }
    }

    // Check left factoring
    vector<string> lfProblems;
    if (checkLeftFactoring(g, lfProblems)) {
        for (auto& p : lfProblems) {
            result.warningList.push_back(
                "Left factoring needed in: " + p);
        }
    }

    return result;
}

bool checkLeftRecursion(Grammar g, vector<string>& problems) {
    bool found = false;
    vector<string> sortedNTs(g.nonTerminals.begin(), g.nonTerminals.end());
    sort(sortedNTs.begin(), sortedNTs.end(), [](const string& a, const string& b) {
        return a.size() > b.size();
    });
    for (auto& rulePair : g.rules) {
        string nt = rulePair.first;
        for (auto& option : rulePair.second) {
            if ((int)option.size() >= (int)nt.size() && option.substr(0, nt.size()) == nt) {
                problems.push_back(nt + "->" + option);
                found = true;
            }
        }
    }
    return found;
}

bool checkLeftFactoring(Grammar g, vector<string>& problems) {
    bool found = false;
    for (auto& rulePair : g.rules) {
        string nt = rulePair.first;
        vector<string>& opts = rulePair.second;
        // Check if any two alternatives share a common prefix
        for (int i = 0; i < (int)opts.size(); i++) {
            for (int j = i + 1; j < (int)opts.size(); j++) {
                if (opts[i] == "ε" || opts[j] == "ε") continue;
                // Find common prefix length (character-level, at least 1 char)
                int minLen = min(opts[i].size(), opts[j].size());
                int commonLen = 0;
                while (commonLen < minLen && opts[i][commonLen] == opts[j][commonLen])
                    commonLen++;
                if (commonLen > 0) {
                    string prefix = opts[i].substr(0, commonLen);
                    string desc = nt + "->" + opts[i] + " | " + opts[j] + " (common prefix: \"" + prefix + "\")";
                    // Avoid duplicate reports
                    bool already = false;
                    for (auto& p : problems) if (p == desc) already = true;
                    if (!already) {
                        problems.push_back(desc);
                        found = true;
                    }
                }
            }
        }
    }
    return found;
}

// ── Parse a production into a list of symbols ─────────────────────────────────
// Splits "TE'" into ["T","E'"], "a+B" into ["a","+","B"], etc.
static vector<string> parseProduction(const string& prod, const set<string>& nonTerminals) {
    vector<string> result;
    if (prod == "ε") { result.push_back("ε"); return result; }

    vector<string> sortedNTs(nonTerminals.begin(), nonTerminals.end());
    sort(sortedNTs.begin(), sortedNTs.end(), [](const string& a, const string& b) {
        return a.size() > b.size();
    });

    int pos = 0;
    while (pos < (int)prod.size()) {
        if (prod[pos] == ' ') { pos++; continue; }
        bool foundNT = false;
        for (auto& nt : sortedNTs) {
            if (pos + (int)nt.size() <= (int)prod.size() &&
                prod.substr(pos, nt.size()) == nt) {
                result.push_back(nt);
                pos += nt.size();
                foundNT = true;
                break;
            }
        }
        if (!foundNT) {
            if (islower((unsigned char)prod[pos])) {
                int j = pos;
                while (j < (int)prod.size() && islower((unsigned char)prod[j])) j++;
                if (j - pos > 1) { result.push_back(prod.substr(pos, j - pos)); pos = j; continue; }
            }
            result.push_back(string(1, prod[pos]));
            pos++;
        }
    }
    return result;
}

static string joinSymbols(const vector<string>& syms) {
    string r;
    for (auto& s : syms) r += s;
    return r;
}

// ── Remove Direct Left Recursion ──────────────────────────────────────────────
// For A -> A α | β  becomes  A -> β A'   and  A' -> α A' | ε
Grammar removeLeftRecursion(Grammar g, vector<string>& steps, vector<string>& details) {
    Grammar ng = g; // start with copy

    // We iterate over ntList to preserve order
    for (auto& nt : g.ntList) {
        vector<string>& opts = ng.rules[nt];
        vector<string> recursive, nonRecursive;

        for (auto& opt : opts) {
            if (opt == "ε") { nonRecursive.push_back(opt); continue; }
            if ((int)opt.size() >= (int)nt.size() && opt.substr(0, nt.size()) == nt) {
                recursive.push_back(opt);
            } else {
                nonRecursive.push_back(opt);
            }
        }

        if (recursive.empty()) continue;

        details.push_back(nt);

        // Create new NT: nt + "'"  (if taken, add extra ')
        string newNT = nt + "'";
        while (ng.nonTerminals.count(newNT)) newNT += "'";

        steps.push_back("Left Recursion in " + nt + ":");
        steps.push_back("  Recursive alternatives:    " + nt + " -> " + [&]() {
            string s; for (auto& r : recursive) { if (!s.empty()) s += " | "; s += r; } return s; }());
        steps.push_back("  Non-recursive alternatives: " + nt + " -> " + [&]() {
            string s; for (auto& r : nonRecursive) { if (!s.empty()) s += " | "; s += r; } return s; }());
        steps.push_back("  Introduce new non-terminal: " + newNT);

        // New alternatives for nt: each non-recursive followed by newNT
        vector<string> newOptsNT;
        for (auto& beta : nonRecursive) {
            if (beta == "ε") newOptsNT.push_back(newNT);
            else             newOptsNT.push_back(beta + newNT);
        }
        if (newOptsNT.empty()) newOptsNT.push_back(newNT);

        // New alternatives for newNT: each alpha (stripped of leading nt) followed by newNT, plus ε
        vector<string> newOptsNewNT;
        for (auto& alpha : recursive) {
            string tail = alpha.substr(nt.size()); // strip leading NT
            if (trimSpaces(tail).empty()) newOptsNewNT.push_back(newNT);
            else                          newOptsNewNT.push_back(trimSpaces(tail) + newNT);
        }
        newOptsNewNT.push_back("ε");

        steps.push_back("  Result: " + nt + " -> " + [&]() {
            string s; for (auto& r : newOptsNT) { if (!s.empty()) s += " | "; s += r; } return s; }());
        steps.push_back("          " + newNT + " -> " + [&]() {
            string s; for (auto& r : newOptsNewNT) { if (!s.empty()) s += " | "; s += r; } return s; }());

        ng.rules[nt] = newOptsNT;
        ng.rules[newNT] = newOptsNewNT;
        ng.nonTerminals.insert(newNT);
        ng.ntList.push_back(newNT);
    }

    // Rebuild terminals
    ng.terminals.clear();
    vector<string> sortedNTs(ng.nonTerminals.begin(), ng.nonTerminals.end());
    sort(sortedNTs.begin(), sortedNTs.end(), [](const string& a, const string& b) { return a.size() > b.size(); });
    for (auto& rulePair : ng.rules) {
        for (auto& option : rulePair.second) {
            if (option == "ε") continue;
            int pos = 0;
            while (pos < (int)option.size()) {
                if (option[pos] == ' ') { pos++; continue; }
                bool isNT = false;
                for (auto& nt2 : sortedNTs) {
                    if (pos + (int)nt2.size() <= (int)option.size() && option.substr(pos, nt2.size()) == nt2) {
                        isNT = true; pos += nt2.size(); break;
                    }
                }
                if (!isNT) {
                    if (islower((unsigned char)option[pos])) {
                        int j = pos;
                        while (j < (int)option.size() && islower((unsigned char)option[j])) j++;
                        if (j - pos > 1) { ng.terminals.insert(option.substr(pos, j-pos)); pos = j; continue; }
                    }
                    string ch(1, option[pos]); if (ch != " ") ng.terminals.insert(ch); pos++;
                }
            }
        }
    }
    return ng;
}

// ── Remove Left Factoring ─────────────────────────────────────────────────────
// For A -> α β1 | α β2 | γ  becomes  A -> α A'  |  γ   and  A' -> β1 | β2
Grammar removeLeftFactoring(Grammar g, vector<string>& steps, vector<string>& details) {
    Grammar ng = g;
    bool anyChange = true;

    while (anyChange) {
        anyChange = false;

        // Copy ntList so we can extend it
        vector<string> currentNTs = ng.ntList;

        for (auto& nt : currentNTs) {
            if (ng.rules.find(nt) == ng.rules.end()) continue;
            vector<string>& opts = ng.rules[nt];

            // Find longest common prefix among any subset of alternatives
            // Approach: group alternatives by their first character/symbol
            // Then for each group with size > 1, factor out longest common prefix

            bool changed = false;
            for (int i = 0; i < (int)opts.size() && !changed; i++) {
                if (opts[i] == "ε") continue;
                // Find all opts[j] that share a prefix with opts[i]
                string baseOpt = opts[i];
                vector<int> shareIdx;
                shareIdx.push_back(i);

                for (int j = i+1; j < (int)opts.size(); j++) {
                    if (opts[j] == "ε") continue;
                    int minLen = min(baseOpt.size(), opts[j].size());
                    int cl = 0;
                    while (cl < (int)minLen && baseOpt[cl] == opts[j][cl]) cl++;
                    if (cl > 0) shareIdx.push_back(j);
                }

                if (shareIdx.size() < 2) continue;

                // Compute longest common prefix among all in shareIdx
                string prefix = opts[shareIdx[0]];
                for (int k = 1; k < (int)shareIdx.size(); k++) {
                    string& s = opts[shareIdx[k]];
                    int cl = 0, minL = min(prefix.size(), s.size());
                    while (cl < (int)minL && prefix[cl] == s[cl]) cl++;
                    prefix = prefix.substr(0, cl);
                }
                if (prefix.empty()) continue;

                // Generate new NT name
                string newNT = nt + "'";
                while (ng.nonTerminals.count(newNT)) newNT += "'";

                details.push_back(nt);

                steps.push_back("Left Factoring in " + nt + ":");
                steps.push_back("  Common prefix: \"" + prefix + "\"");
                steps.push_back("  Alternatives sharing prefix:");
                for (int idx : shareIdx) steps.push_back("    " + nt + " -> " + opts[idx]);
                steps.push_back("  Introduce new non-terminal: " + newNT);

                // Suffixes after the common prefix
                vector<string> newNewNTOpts;
                for (int idx : shareIdx) {
                    string suffix = trimSpaces(opts[idx].substr(prefix.size()));
                    if (suffix.empty()) suffix = "ε";
                    newNewNTOpts.push_back(suffix);
                }
                ng.rules[newNT] = newNewNTOpts;
                ng.nonTerminals.insert(newNT);
                ng.ntList.push_back(newNT);

                // Remove factored alternatives and add factored form
                vector<string> remaining;
                set<int> shareSet(shareIdx.begin(), shareIdx.end());
                for (int k = 0; k < (int)opts.size(); k++)
                    if (!shareSet.count(k)) remaining.push_back(opts[k]);
                remaining.push_back(prefix + newNT);
                ng.rules[nt] = remaining;

                steps.push_back("  Result: " + nt + " -> " + [&]() {
                    string s; for (auto& r : ng.rules[nt]) { if (!s.empty()) s += " | "; s += r; } return s; }());
                steps.push_back("          " + newNT + " -> " + [&]() {
                    string s; for (auto& r : ng.rules[newNT]) { if (!s.empty()) s += " | "; s += r; } return s; }());

                changed = true;
                anyChange = true;
            }
        }
    }

    // Rebuild terminals
    ng.terminals.clear();
    vector<string> sortedNTs(ng.nonTerminals.begin(), ng.nonTerminals.end());
    sort(sortedNTs.begin(), sortedNTs.end(), [](const string& a, const string& b) { return a.size() > b.size(); });
    for (auto& rulePair : ng.rules) {
        for (auto& option : rulePair.second) {
            if (option == "ε") continue;
            int pos = 0;
            while (pos < (int)option.size()) {
                if (option[pos] == ' ') { pos++; continue; }
                bool isNT = false;
                for (auto& nt2 : sortedNTs) {
                    if (pos + (int)nt2.size() <= (int)option.size() && option.substr(pos, nt2.size()) == nt2) {
                        isNT = true; pos += nt2.size(); break;
                    }
                }
                if (!isNT) {
                    if (islower((unsigned char)option[pos])) {
                        int j = pos;
                        while (j < (int)option.size() && islower((unsigned char)option[j])) j++;
                        if (j - pos > 1) { ng.terminals.insert(option.substr(pos, j-pos)); pos = j; continue; }
                    }
                    string ch(1, option[pos]); if (ch != " ") ng.terminals.insert(ch); pos++;
                }
            }
        }
    }
    return ng;
}

vector<string> grammarToLines(Grammar g) {
    vector<string> lines;
    for (auto& nt : g.ntList) {
        if (g.rules.find(nt) == g.rules.end()) continue;
        string line = nt + " -> ";
        auto& opts = g.rules[nt];
        for (int i = 0; i < (int)opts.size(); i++) {
            if (i > 0) line += " | ";
            line += opts[i];
        }
        lines.push_back(line);
    }
    return lines;
}

FixResult autoFixGrammar(Grammar g) {
    FixResult fr;
    fr.wasFixed = false;
    fr.hadLeftRecursion = false;
    fr.hadLeftFactoring = false;
    fr.hadAmbiguity = false;
    fr.fixedGrammar = g;

    // Step 1: Check and fix left recursion
    vector<string> lrProblems;
    if (checkLeftRecursion(g, lrProblems)) {
        fr.hadLeftRecursion = true;
        fr.leftRecursionDetails = lrProblems;
        fr.fixSteps.push_back("=== STEP 1: LEFT RECURSION DETECTED ===");
        fr.fixSteps.push_back("Left recursion means a non-terminal can derive itself as the leftmost symbol.");
        fr.fixSteps.push_back("This causes LL(1) parsers to loop infinitely.");
        fr.fixSteps.push_back("Fix: For A -> A alpha | beta, rewrite as A -> beta A'  and  A' -> alpha A' | epsilon");

        vector<string> lrSteps, lrDetails;
        Grammar fixed1 = removeLeftRecursion(g, lrSteps, lrDetails);
        for (auto& s : lrSteps) fr.fixSteps.push_back(s);
        fr.fixedGrammar = fixed1;
        fr.wasFixed = true;
        g = fixed1;
    } else {
        fr.fixSteps.push_back("=== STEP 1: LEFT RECURSION CHECK ===");
        fr.fixSteps.push_back("No left recursion found. Grammar is safe from infinite loops.");
    }

    // Step 2: Check and fix left factoring
    vector<string> lfProblems;
    if (checkLeftFactoring(g, lfProblems)) {
        fr.hadLeftFactoring = true;
        fr.leftFactoringDetails = lfProblems;
        fr.fixSteps.push_back("=== STEP 2: LEFT FACTORING NEEDED ===");
        fr.fixSteps.push_back("Left factoring is needed when two or more alternatives of a non-terminal share a common prefix.");
        fr.fixSteps.push_back("This causes LL(1) conflicts because the parser cannot decide which rule to use by looking at one token.");
        fr.fixSteps.push_back("Fix: Factor out the common prefix into a new non-terminal.");

        vector<string> lfSteps, lfDetails;
        Grammar fixed2 = removeLeftFactoring(g, lfSteps, lfDetails);
        for (auto& s : lfSteps) fr.fixSteps.push_back(s);
        fr.fixedGrammar = fixed2;
        fr.wasFixed = true;
        g = fixed2;
    } else {
        fr.fixSteps.push_back("=== STEP 2: LEFT FACTORING CHECK ===");
        fr.fixSteps.push_back("No left factoring issues found. All alternatives have unique first tokens.");
    }

    // Step 3: Ambiguity (reported via parsing table conflicts after fixing)
    // Ambiguity check: if after fixing LR and LF, there are still conflicts
    // in the parsing table, the grammar may be inherently ambiguous.
    fr.fixSteps.push_back("=== STEP 3: AMBIGUITY CHECK ===");
    fr.fixSteps.push_back("Ambiguity means a string can be derived in more than one way (multiple parse trees).");
    fr.fixSteps.push_back("Detected as: multiple rules in the same parsing table cell [NT][Terminal].");
    fr.fixSteps.push_back("After removing left recursion and left factoring, compute the parsing table.");
    fr.fixSteps.push_back("If conflicts still exist, the grammar may be inherently ambiguous — no automatic fix exists.");
    fr.fixSteps.push_back("You must manually redesign the grammar to remove ambiguity.");

    return fr;
}
