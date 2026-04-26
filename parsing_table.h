
#pragma once
#include "grammar.h"
#include "first_follow.h"
#include <map>
#include <string>

using namespace std;

// This struct holds the LL1 parsing table
struct ParsingTable {
    // table[nonTerminal][terminal] = rule to apply
    // Example: table["E"]["id"] = "E->TE'"
    map<string, map<string, string>> table;

    // if same cell has two rules it means grammar is not LL1
    // we save those conflicts here
    vector<string> conflicts;

    // tells if grammar is LL1 or not
    bool isLL1;
};

// Function to build the parsing table
ParsingTable buildParsingTable(Grammar g,
    map<string, set<string>> firstSets,
    map<string, set<string>> followSets);