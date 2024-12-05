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
using std::optional;
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

        set<pair<State, size_t>> failed;
        for (size_t i = 0, tokenStart = i; i < code.length();)
        {
            vector<pair<pair<State, optional<T>>, size_t>> stack;

            // here is little different with the figure 2.13 which has init state s0 from start
            auto r = dfa.RunFromStart(code[i]);
        CheckState:
            if (r.has_value())
            {
                auto& s = r.value();
                if (failed.contains({ s.first, i }))
                {
                    stack.pop_back(); // need do this? current state is not pushed into stack, why pop up last state pair?
                    break;
                }
                if (s.second.has_value())
                {
                    stack.clear();
                }
                auto nextState = s.first;
                stack.push_back({ move(s), i });
                if (++i < code.length())
                {
                    r = dfa.Run(nextState, code[i]);
                    goto CheckState;
                }
                else
                {
                    break;
                }
            }
            if (stack.empty())
            {
                break;
            }
            for (auto& state = stack.back().first; not (state.second.has_value() or stack.size() <= 1); stack.pop_back())
            {
            }

            if (auto& result = stack.back().first; result.second.has_value())
            {
                failed.insert({ result.first, i });
                auto lexemeLen = stack.back().second - tokenStart + 1;
                toks.push_back(Token{ .Type = result.second.value(), .Value = string(code.substr(tokenStart, lexemeLen)), });
                tokenStart = stack.back().second + 1;
                i = tokenStart;
            }
            else
            {
                break;
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