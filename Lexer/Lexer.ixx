export module Lexer;

import std;
import FiniteAutomata;
import FiniteAutomataBuilder;
import Graph;
import Base;

using std::array;
using std::string;
using std::string_view;
using std::vector;
using std::map;
using std::set;
using std::pair;
using std::size_t;
using std::optional;
using std::generator;
using std::tuple;
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
    FiniteAutomata<char, T> dfa;
    using Token = Token<T>;
public:
    template <size_t Size>
    static auto New(array<pair<string, T>, Size> const& identifyGroup) -> Lexer
    {
        vector<FiniteAutomataDraft<char, T>> fas{};
        vector<pair<set<State>, T>> accepts2TokenType;
        for (auto& i : identifyGroup)
        {
            fas.push_back(Minimize<false>(NFA2DFA(ConstructNFAFrom(i.first, i.second))));
        }

        auto fullFa = OrWithoutMergeAcceptState(move(fas));
        auto dfa = NFA2DFA(move(fullFa));
        auto mdfa = Minimize<true>(move(dfa));
        return Lexer(mdfa.Freeze());
    }

    Lexer(FiniteAutomata<char, T> dfa) : dfa(move(dfa))
    { }

    auto Lex(string_view code) const -> vector<Token>
    {
        using std::get;

        vector<Token> toks{};
        if (code.empty())
        {
            return toks;
        }

        set<pair<State, size_t>> failed;
        for (size_t i = 0, tokenStart = i; i < code.length();)
        {
            vector<tuple<pair<State, optional<T>>, size_t, unsigned>> stack;

            // here is little different with the figure 2.13 which has init state s0 from start
            stack.push_back({ pair<State, optional<T>>{ dfa.StartState, {} }, i, 0 }); // assume the StartState is not accept state
            auto r = dfa.RunFromStart(code[i], get<2>(stack.back()));
        CheckState:
            if (r.has_value())
            {
                auto& s = r.value();
                if (s.second.has_value())
                {
                    stack.clear();
                }
                if (++i < code.length())
                {
                    //std::println("run state {} by {}", nextState, i);
                    if (failed.contains({ s.first, i }))
                    {
                        std::println("no need to continue state {} at {}", s.first, i);
                        goto RollBack2RetryOtherStep;
                    }
                    auto nextState = s.first;
                    stack.push_back({ move(s), i, 0 });
                    r = dfa.Run(nextState, code[i], get<2>(stack.back()));
                    goto CheckState;
                }
                else
                {
                    stack.push_back({ move(s), i, 0 });
                }
            }
        RollBack2RetryOtherStep:
            if (stack.empty())
            {
                break;
            }
            for (auto state = &get<0>(stack.back()); not (state->second.has_value() or stack.size() <= 1); stack.pop_back(), state = &get<0>(stack.back()))
            {
                if (get<2>(stack.back()) != std::numeric_limits<unsigned>::max())
                {
                    auto [s, retryStart, _] = stack.back();
                    i = retryStart;
                    r = dfa.Run(s.first, code[i], get<2>(stack.back()));
                    goto CheckState;
                }
                std::println("failed record: state {} at {}", state->first, get<1>(stack.back()));
                failed.insert({ state->first, get<1>(stack.back()) });
            }

            if (auto& result = get<0>(stack.back()); result.second.has_value())
            // back() item: 
            // ((current state, result corresponding current state which is last run result), code position which is already refreshed to next position, steps retried)
            {
                auto lexemeLen = get<1>(stack.back()) - tokenStart;
                toks.push_back(Token{ .Type = result.second.value(), .Value = string(code.substr(tokenStart, lexemeLen)), });
                tokenStart = get<1>(stack.back());
                i = tokenStart;
            }
            else
            {
                if (get<2>(stack.back()) != std::numeric_limits<unsigned>::max())
                {
                    auto [s, retryStart, _] = stack.back();
                    i = retryStart;
                    r = dfa.Run(s.first, code[i], get<2>(stack.back()));
                    goto CheckState;
                }
                failed.insert({ result.first, get<1>(stack.back()) });
                break;
            }
        }

        return toks;
    }

    // TODO change to generator when VS release 17.13 which will support std::generator
    // this function should obtain the ownership of the code, so use string. We can transfer to Stream interface in the future
    //auto Lex(generator<char> code) const -> generator<Token>
    //{
    //    vector<Token> toks{};
    //    if (code.empty())
    //    {
    //        co_return;
    //    }

    //    set<pair<State, size_t>> failed;
    //    for (size_t i = 0, tokenStart = i; i < code.length();)
    //    {
    //        vector<pair<pair<State, optional<T>>, size_t>> stack;

    //        // here is little different with the figure 2.13 which has init state s0 from start
    //        auto r = dfa.RunFromStart(code[i]);
    //    CheckState:
    //        if (r.has_value())
    //        {
    //            auto& s = r.value();
    //            if (failed.contains({ s.first, i }))
    //            {
    //                std::println("no need to continue state {} at {}", s.first, i);
    //                goto RollBack2Accept;
    //            }
    //            if (s.second.has_value())
    //            {
    //                stack.clear();
    //            }
    //            auto nextState = s.first;
    //            stack.push_back({ move(s), i });
    //            if (++i < code.length())
    //            {
    //                //std::println("run state {} by {}", nextState, i);
    //                r = dfa.Run(nextState, code[i]);
    //                goto CheckState;
    //            }
    //        }
    //    RollBack2Accept:
    //        // how to roll back for generator
    //        if (stack.empty())
    //        {
    //            break;
    //        }
    //        for (auto state = &stack.back().first; not (state->second.has_value() or stack.size() <= 1); stack.pop_back(), state = &stack.back().first)
    //        {
    //            std::println("failed record: state {} at {}", state->first, stack.back().second);
    //            failed.insert({ state->first, stack.back().second });
    //        }

    //        if (auto& result = stack.back().first; result.second.has_value())
    //        {
    //            auto lexemeLen = stack.back().second - tokenStart + 1;
    //            toks.push_back(Token{ .Type = result.second.value(), .Value = string(code.substr(tokenStart, lexemeLen)), });
    //            tokenStart = stack.back().second + 1;
    //            i = tokenStart;
    //        }
    //        else
    //        {
    //            failed.insert({ result.first, stack.back().second });
    //            break;
    //        }
    //    }

    //    return toks;
    //}
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