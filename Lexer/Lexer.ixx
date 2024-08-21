export module Lexer;

import std;
import Graph;
using std::array;
using std::pair;
using std::string;
using std::move;
using std::vector;

auto RegExp2NFA(string regExp) -> Graph
{

}

auto NFA2DFA(Graph nfa) -> Graph
{

}

auto Minimize(Graph dfa) -> Graph
{

}


template <typename T>
class Lexer
{
private:
    array<pair<string, T>> idGroup;
public:
    Lexer(array<pair<string, T>> identifyGroup) : idGroup(move(identifyGroup))
    { }
    auto Lex(string code) -> vector<pair<string, T>> const
    { }
};

export
{
    template <typename T>
    class Lexer;
}