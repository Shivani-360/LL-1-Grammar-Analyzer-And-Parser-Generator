#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
using namespace std;

struct Grammar {
    map<string, vector<string>> rules;
    set<string> nonTerminals;
    set<string> terminals;
    string startSymbol;
    vector<string> ntList;
};

struct CheckResult {
    bool isValid;
    vector<string> errorList;
    vector<string> warningList;
};

struct FixResult {
    bool wasFixed;
    bool hadLeftRecursion;
    bool hadLeftFactoring;
    bool hadAmbiguity;
    vector<string> leftRecursionDetails;
    vector<string> leftFactoringDetails;
    vector<string> ambiguityDetails;
    vector<string> fixSteps;
    Grammar fixedGrammar;
};

Grammar buildGrammar(vector<string> lines, string start);
CheckResult checkGrammar(Grammar g);
bool checkLeftRecursion(Grammar g, vector<string>& problems);
bool checkLeftFactoring(Grammar g, vector<string>& problems);
Grammar removeLeftRecursion(Grammar g, vector<string>& steps, vector<string>& details);
Grammar removeLeftFactoring(Grammar g, vector<string>& steps, vector<string>& details);
vector<string> grammarToLines(Grammar g);
FixResult autoFixGrammar(Grammar g);
string trimSpaces(string s);
vector<string> splitString(string s, char ch);
