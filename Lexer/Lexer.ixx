export module Lexer;

import std;
import FiniteAutomata;
import Graph;
import Base;

using std::array;
using std::pair;
using std::string;
using std::string_view;
using std::vector;
using std::map;
using std::set;
using std::pair;
using std::size_t;
using std::move;

template <typename T>
class Lexer
{
private:
    RefineFiniteAutomata dfa;
public:
    struct Token
    {
        T Type;
        string Value;
        auto IsEof() const -> bool
        {
            return Value.empty(); // TODO: maybe change in the future
        }
    };
    template <size_t Size>
    static auto New(array<pair<string, T>, Size> const& identifyGroup) -> Lexer
    {
        vector<FiniteAutomata<size_t>> nfas{};
        vector<pair<set<State>, T>> accepts2TokenType;
        map<pair<char, char>, size_t> classification{};
        for (auto& i : identifyGroup)
        {
            nfas.push_back(ConstructNFAFrom(i.first, classification));
        }

        // do we change accepts below? TODO
        auto fullNfa = OrWithoutMergeAcceptState(move(nfas));
        auto dfa = NFA2DFA(move(fullNfa));
        auto mdfa = Minimize<true>(move(dfa));
        return Lexer(RefineFiniteAutomata(move(mdfa), move(classification)));
    }

    Lexer(RefineFiniteAutomata dfa) : dfa(move(dfa))
    { }

    // TODO change to generator when VS release 17.13 which will support std::generator
    // this function should obtain the ownership of the code, so use string. We can transfer to Stream interface in the future
    auto Lex(string code) -> vector<Token> const
    {
        vector<Token> toks{};
        vector<pair<set<State>, T>> accepts2TokenType;

        for (auto c : code)
        {
            auto states = dfa.RunFromStart(c);
            Assert(states.length() == 0 or states.length() == 1, "DFA has one next state at most");
            for (auto const& [accepts, tokenType] : accepts2TokenType)
            {
                if (accepts.contains(states[0])
                {

                }

            }

        }
    }
};

export
{
    template <typename T>
    class Lexer;
}