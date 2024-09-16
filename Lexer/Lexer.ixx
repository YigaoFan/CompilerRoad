export module Lexer;

import std;
import FiniteAutomata;

using std::array;
using std::pair;
using std::string;
using std::vector;
using std::map;
using std::size_t;
using std::move;

template <typename T>
class Lexer
{
public:
    RefineFiniteAutomata dfa;
public:
    template <size_t Size>
    static auto New(array<pair<string, T>, Size> const& identifyGroup) -> Lexer
    {
        vector<FiniteAutomata<size_t>> nfas{};
        map<pair<char, char>, size_t> classification{};
        for (auto& i : identifyGroup)
        {
            nfas.push_back(ConstructNFAFrom(i.first, classification));
        }

        auto fullNfa = OrWithoutMergeAcceptState(move(nfas));
        auto dfa = NFA2DFA(move(fullNfa));
        auto mdfa = Minimize<true>(move(dfa));
        return Lexer(RefineFiniteAutomata(move(mdfa), move(classification)));
    }

    Lexer(RefineFiniteAutomata dfa) : dfa(move(dfa))
    { }

    auto Lex(string code) -> vector<pair<string, T>> const
    {

    }
};

export
{
    template <typename T>
    class Lexer;
}