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
struct Token
{
    T Type;
    string Value;
    auto IsEof() const -> bool
    {
        return Value.empty(); // TODO: maybe change in the future
    }
};

template <typename T>
class Lexer
{
private:
    RefineFiniteAutomata<T> dfa;
    using Token = Token<T>;
public:
    template <size_t Size>
    static auto New(array<pair<string, T>, Size> const& identifyGroup) -> Lexer
    {
        vector<FiniteAutomataDraft<Step, T>> fas{};
        vector<pair<set<State>, T>> accepts2TokenType;
        map<pair<char, char>, size_t> classification{};
        for (auto& i : identifyGroup)
        {
            fas.push_back(Minimize<false>(NFA2DFA(ConstructNFAFrom(i.first, i.second, classification))));
        }

        auto fullFa = OrWithoutMergeAcceptState(move(fas));
        auto dfa = NFA2DFA(move(fullFa));
        auto mdfa = Minimize<true>(move(dfa));
        return Lexer(RefineFiniteAutomata(mdfa.Freeze(), move(classification)));
    }

    Lexer(RefineFiniteAutomata<T> dfa) : dfa(move(dfa))
    { }

    // TODO change to generator when VS release 17.13 which will support std::generator
    // this function should obtain the ownership of the code, so use string. We can transfer to Stream interface in the future
    auto Lex(string_view code) const -> vector<Token>
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
                    std::println("no need to continue state {} at {}", s.first, i);
                    goto RollBack2Accept;
                }
                if (s.second.has_value())
                {
                    stack.clear();
                }
                auto nextState = s.first;
                stack.push_back({ move(s), i });
                if (++i < code.length())
                {
                    std::println("run state {} by {}", nextState, i);
                    r = dfa.Run(nextState, code[i]);
                    goto CheckState;
                }
            }
        RollBack2Accept:
            if (stack.empty())
            {
                break;
            }
            for (auto state = &stack.back().first; not (state->second.has_value() or stack.size() <= 1); stack.pop_back(), state = &stack.back().first)
            {
                std::println("failed record: state {} at {}", state->first, stack.back().second);
                failed.insert({ state->first, stack.back().second });
            }

            if (auto& result = stack.back().first; result.second.has_value())
            {
                failed.insert({ result.first, i });
                auto lexemeLen = stack.back().second - tokenStart + 1;
                toks.push_back(Token{ .Type = result.second.value(), .Value = string(code.substr(tokenStart, lexemeLen)), });
                tokenStart = stack.back().second + 1;
                i = tokenStart;
            }
        }

        return toks;
    }
};


template<typename T>
struct std::formatter<Token<T>, char>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        // not implement
        auto it = ctx.begin();
        if (it == ctx.end())
            return it;

        if (it != ctx.end() && *it != '}')
            throw std::format_error("Invalid format args for QuotableString.");

        return it;
    }

    template<class FormatContext>
    constexpr auto format(Token<T> const& t, FormatContext& fc) const
    {
        return std::format_to(fc.out(), "Type: {}, Value: {}", t.Type, t.Value);
    }
};

export
{
    template <typename T>
    class Lexer;
    template <typename T>
    struct Token;
    template<typename T>
    struct std::formatter<Token<T>, char>;
}