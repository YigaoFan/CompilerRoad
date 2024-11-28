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
    RefineFiniteAutomata<T> dfa;
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
        vector<FiniteAutomataDraft<size_t, T>> nfas{};
        vector<pair<set<State>, T>> accepts2TokenType;
        map<pair<char, char>, size_t> classification{};
        for (auto& i : identifyGroup)
        {
            nfas.push_back(ConstructNFAFrom(i.first, i.second, classification));
        }

        // add constructing and freeze state to code, then we can simple store the (state, result) pair in code, then freeze to map when need to be running
        auto fullNfa = OrWithoutMergeAcceptState(move(nfas));
        auto dfa = NFA2DFA(move(fullNfa));
        auto mdfa = Minimize<true>(move(dfa));
        return Lexer(RefineFiniteAutomata(mdfa.Freeze(), move(classification)));
    }

    Lexer(RefineFiniteAutomata<T> dfa) : dfa(move(dfa))
    { }

    // TODO change to generator when VS release 17.13 which will support std::generator
    // this function should obtain the ownership of the code, so use string. We can transfer to Stream interface in the future
    auto Lex(string_view code) -> vector<Token> const
    {
        vector<Token> toks{};
        if (code.empty())
        {
            return toks;
        }

        size_t i = 1;
        for (auto r = dfa.RunFromStart(code.front()); r.has_value(); )
        {
            if (auto& s = r.value(); s.second.has_value())
            {
                toks.push_back(Token{ .Type = s.second.value(), .Value = "", });
            }
            else
            {
                r = dfa.Run(s.first, code[i++]);
            }
        }
        return toks;
    }
};

export
{
    template <typename T>
    class Lexer;
}