export module Lexer;

import std;
import Graph;

using std::array;
using std::pair;
using std::string;
using std::vector;
using std::size_t;
using std::move;

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