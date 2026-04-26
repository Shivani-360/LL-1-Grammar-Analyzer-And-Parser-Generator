#include <algorithm>
#pragma once
#include "grammar.h"
#include <map>
#include <set>
#include <string>

using namespace std;

// This struct holds first and follow sets for all non terminals
struct FirstFollow {
    // FIRST set for each non terminal
    // Example: FIRST(E) = {id, (}
    map<string, set<string>> firstSets;

    // FOLLOW set for each non terminal
    // Example: FOLLOW(E) = {), $}
    map<string, set<string>> followSets;
};

// Function to compute all FIRST sets
map<string, set<string>> computeFirst(Grammar g);

// Function to compute all FOLLOW sets
map<string, set<string>> computeFollow(Grammar g, map<string, set<string>> firstSets);

// Helper to get FIRST of a whole string like "TE'"
set<string> getFirstOfString(string str, Grammar g, map<string, set<string>> firstSets);