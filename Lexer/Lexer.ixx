export module Lexer;

import std;
import Graph;

using std::array;
using std::pair;
using std::string;
using std::vector;
using std::size_t;
using std::move;

//auto RegExp2NFA(string regExp) -> Graph<char>
//{
//    throw;
//}
//
//auto NFA2DFA(Graph<char> nfa) -> Graph<char>
//{
//    throw;
//}
//
//auto Minimize(Graph<char> dfa) -> Graph<char>
//{
//    throw;
//}


template <typename T, size_t Size>
class Lexer
{
private:
    array<pair<string, T>, Size> idGroup;
public:
    Lexer(array<pair<string, T>, Size> identifyGroup) : idGroup(move(identifyGroup))
    { }
    auto Lex(string code) -> vector<pair<string, T>> const
    { }
};

export
{
    template <typename T, size_t Size>
    class Lexer;
}