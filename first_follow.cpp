#include <algorithm>
#include "first_follow.h"

using namespace std;

// This function finds FIRST set of a whole string
// Example: FIRST("TE'") = FIRST(T) and if T has epsilon then also FIRST(E')
set<string> getFirstOfString(string str, Grammar g, map<string, set<string>> firstSets) {
    set<string> result;

    // if string is empty or epsilon return epsilon
    if (str == "" || str == "ε") {
        result.insert("ε");
        return result;
    }

    // sort NTs longest first so E' matches before E (fixes apostrophe bug)
    vector<string> sortedNTs(g.nonTerminals.begin(), g.nonTerminals.end());
    sort(sortedNTs.begin(), sortedNTs.end(), [](const string& a, const string& b){ return a.size() > b.size(); });
    int pos = 0;
    while (pos < str.size()) {

        // find which symbol is at current position
        string currentSymbol = "";

        // check if it matches a non terminal
        bool foundNT = false;
        for (auto& nt : sortedNTs) {
            if (str.substr(pos, nt.size()) == nt) {
                currentSymbol = nt;
                foundNT = true;
                pos += nt.size();
                break;
            }
        }

        if (!foundNT) {
            // it is a terminal symbol
            int j = pos;
            while (j < str.size() && islower(str[j])) j++;
            if (j - pos > 1) {
                currentSymbol = str.substr(pos, j - pos);
                pos = j;
            } else {
                currentSymbol += str[pos];
                pos++;
            }
        }

        if (currentSymbol == "ε") {
            result.insert("ε");
            break;
        }

        // if current symbol is a terminal just add it and stop
        if (g.nonTerminals.find(currentSymbol) == g.nonTerminals.end()) {
            result.insert(currentSymbol);
            break;
        }

        // current symbol is a non terminal
        // add its FIRST set but not epsilon
        set<string> firstOfCurrent = firstSets[currentSymbol];
        for (auto& sym : firstOfCurrent) {
            if (sym != "ε") result.insert(sym);
        }

        // if epsilon is NOT in FIRST of current symbol we stop here
        if (firstOfCurrent.find("ε") == firstOfCurrent.end()) {
            break;
        }

        // if we reach end of string and all had epsilon then add epsilon
        if (pos >= str.size()) {
            result.insert("ε");
        }
    }

    return result;
}

// This function computes FIRST set for all non terminals
map<string, set<string>> computeFirst(Grammar g) {
    map<string, set<string>> firstSets;

    // start with empty sets for all non terminals
    for (auto& nt : g.nonTerminals) {
        firstSets[nt] = set<string>();
    }

    // sort NTs longest first so E' matches before E
    vector<string> sortedNTs(g.nonTerminals.begin(), g.nonTerminals.end());
    sort(sortedNTs.begin(), sortedNTs.end(), [](const string& a, const string& b){ return a.size() > b.size(); });

    // keep repeating until nothing changes
    // this is called fixed point iteration
    bool changed = true;
    while (changed) {
        changed = false;

        // go through each rule
        for (auto& pair : g.rules) {
            string nt = pair.first;
            vector<string> options = pair.second;

            for (int i = 0; i < options.size(); i++) {
                string option = options[i];

                // if right side is epsilon add epsilon to FIRST
                if (option == "ε") {
                    if (firstSets[nt].find("ε") == firstSets[nt].end()) {
                        firstSets[nt].insert("ε");
                        changed = true;
                    }
                    continue;
                }

                // go through each symbol in this option
                int pos = 0;
                bool allHadEpsilon = true;

                while (pos < option.size()) {
                    // find current symbol
                    string sym = "";
                    bool isNT = false;

                    for (auto& n : sortedNTs) {
                        if (option.substr(pos, n.size()) == n) {
                            sym = n;
                            isNT = true;
                            pos += n.size();
                            break;
                        }
                    }

                    if (!isNT) {
                        // terminal symbol
                        int j = pos;
                        while (j < option.size() && islower(option[j])) j++;
                        if (j - pos > 1) {
                            sym = option.substr(pos, j - pos);
                            pos = j;
                        } else {
                            sym += option[pos];
                            pos++;
                        }
                    }

                    // if symbol is terminal add it and stop
                    if (!isNT) {
                        if (firstSets[nt].find(sym) == firstSets[nt].end()) {
                            firstSets[nt].insert(sym);
                            changed = true;
                        }
                        allHadEpsilon = false;
                        break;
                    }

                    // symbol is non terminal
                    // add its first set except epsilon
                    for (auto& s : firstSets[sym]) {
                        if (s != "ε") {
                            if (firstSets[nt].find(s) == firstSets[nt].end()) {
                                firstSets[nt].insert(s);
                                changed = true;
                            }
                        }
                    }

                    // if this NT does not have epsilon we stop
                    if (firstSets[sym].find("ε") == firstSets[sym].end()) {
                        allHadEpsilon = false;
                        break;
                    }
                }

                // if all symbols had epsilon then add epsilon
                if (allHadEpsilon) {
                    if (firstSets[nt].find("ε") == firstSets[nt].end()) {
                        firstSets[nt].insert("ε");
                        changed = true;
                    }
                }
            }
        }
    }

    return firstSets;
}

// This function computes FOLLOW set for all non terminals
map<string, set<string>> computeFollow(Grammar g, map<string, set<string>> firstSets) {
    map<string, set<string>> followSets;

    // start with empty sets
    for (auto& nt : g.nonTerminals) {
        followSets[nt] = set<string>();
    }

    // rule 1: add $ to follow of start symbol
    followSets[g.startSymbol].insert("$");

    // keep repeating until nothing changes
    vector<string> sortedNTs(g.nonTerminals.begin(), g.nonTerminals.end());
    sort(sortedNTs.begin(), sortedNTs.end(), [](const string& a, const string& b){ return a.size() > b.size(); });

    bool changed = true;
    while (changed) {
        changed = false;

        // go through each rule
        for (auto& pair : g.rules) {
            string nt = pair.first;
            vector<string> options = pair.second;

            for (int i = 0; i < options.size(); i++) {
                string option = options[i];
                if (option == "ε") continue;

                // sort NTs longest first so E' matches before E
                vector<string> sortedNTs(g.nonTerminals.begin(), g.nonTerminals.end());
                sort(sortedNTs.begin(), sortedNTs.end(), [](const string& a, const string& b){ return a.size() > b.size(); });

                // go through each position in this option
                int pos = 0;
                while (pos < option.size()) {

                    // find symbol at current position
                    string sym = "";
                    bool isNT = false;

                    for (auto& n : sortedNTs) {
                        if (option.substr(pos, n.size()) == n) {
                            sym = n;
                            isNT = true;
                            pos += n.size();
                            break;
                        }
                    }

                    if (!isNT) {
                        // terminal so skip it
                        int j = pos;
                        while (j < option.size() && islower(option[j])) j++;
                        if (j - pos > 1) pos = j;
                        else pos++;
                        continue;
                    }

                    // sym is a non terminal
                    // find what comes after it in this option
                    string rest = option.substr(pos);

                    // get FIRST of what comes after sym
                    set<string> firstOfRest = getFirstOfString(rest, g, firstSets);

                    // rule 2: add FIRST(rest) except epsilon to FOLLOW(sym)
                    for (auto& s : firstOfRest) {
                        if (s != "ε") {
                            if (followSets[sym].find(s) == followSets[sym].end()) {
                                followSets[sym].insert(s);
                                changed = true;
                            }
                        }
                    }

                    // rule 3: if epsilon is in FIRST(rest) add FOLLOW(nt) to FOLLOW(sym)
                    if (firstOfRest.find("ε") != firstOfRest.end()) {
                        for (auto& s : followSets[nt]) {
                            if (followSets[sym].find(s) == followSets[sym].end()) {
                                followSets[sym].insert(s);
                                changed = true;
                            }
                        }
                    }
                }
            }
        }
    }

    return followSets;
}