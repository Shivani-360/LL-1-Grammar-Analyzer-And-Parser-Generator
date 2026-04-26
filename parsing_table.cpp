#include "parsing_table.h"

using namespace std;

// This function builds the LL1 parsing table
ParsingTable buildParsingTable(Grammar g,
    map<string, set<string>> firstSets,
    map<string, set<string>> followSets) {

    ParsingTable pt;
    pt.isLL1 = true;

    // go through each rule in grammar
    for (auto& pair : g.rules) {
        string nt = pair.first;
        vector<string> options = pair.second;

        for (int i = 0; i < options.size(); i++) {
            string option = options[i];

            // the rule we will put in table
            string rule = nt + "->" + option;

            if (option == "ε") {
                // rule 2: if option is epsilon
                // add this rule to table[nt][x] for each x in FOLLOW(nt)
                for (auto& followSym : followSets[nt]) {
                    if (pt.table[nt][followSym] != "") {
                        // conflict found two rules in same cell
                        string conflict = "Conflict at [" + nt + "][" + followSym + "] between " +
                            pt.table[nt][followSym] + " and " + rule;
                        pt.conflicts.push_back(conflict);
                        pt.isLL1 = false;
                    } else {
                        pt.table[nt][followSym] = rule;
                    }
                }
            } else {
                // rule 1: add rule to table[nt][x] for each x in FIRST(option)
                // but not epsilon
                set<string> firstOfOption;

                // get first set of this option
                // go symbol by symbol
                int pos = 0;
                bool allEps = true;

                while (pos < option.size()) {
                    string sym = "";
                    bool isNT = false;

                    // check if current pos is a non terminal
                    for (auto& n : g.nonTerminals) {
                        if (option.substr(pos, n.size()) == n) {
                            sym = n;
                            isNT = true;
                            pos += n.size();
                            break;
                        }
                    }

                    if (!isNT) {
                        // it is a terminal
                        int j = pos;
                        while (j < option.size() && islower(option[j])) j++;
                        if (j - pos > 1) {
                            sym = option.substr(pos, j - pos);
                            pos = j;
                        } else {
                            sym += option[pos];
                            pos++;
                        }
                        // terminal goes directly to first set
                        firstOfOption.insert(sym);
                        allEps = false;
                        break;
                    }

                    // non terminal: add its first except epsilon
                    for (auto& s : firstSets[sym]) {
                        if (s != "ε") firstOfOption.insert(s);
                    }

                    // if no epsilon in first of sym we stop
                    if (firstSets[sym].find("ε") == firstSets[sym].end()) {
                        allEps = false;
                        break;
                    }
                }

                // if all symbols had epsilon add epsilon too
                if (allEps) firstOfOption.insert("ε");

                // now fill table for each terminal in firstOfOption
                for (auto& terminal : firstOfOption) {
                    if (terminal == "ε") {
                        // if epsilon in first then use follow set
                        for (auto& followSym : followSets[nt]) {
                            if (pt.table[nt][followSym] != "") {
                                string conflict = "Conflict at [" + nt + "][" + followSym + "] between " +
                                    pt.table[nt][followSym] + " and " + rule;
                                pt.conflicts.push_back(conflict);
                                pt.isLL1 = false;
                            } else {
                                pt.table[nt][followSym] = rule;
                            }
                        }
                    } else {
                        if (pt.table[nt][terminal] != "") {
                            string conflict = "Conflict at [" + nt + "][" + terminal + "] between " +
                                pt.table[nt][terminal] + " and " + rule;
                            pt.conflicts.push_back(conflict);
                            pt.isLL1 = false;
                        } else {
                            pt.table[nt][terminal] = rule;
                        }
                    }
                }
            }
        }
    }

    return pt;
}