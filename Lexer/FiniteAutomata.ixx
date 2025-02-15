export module FiniteAutomata;

#include "Util.h"
import std;
import Base;
import Graph;

using std::string;
using std::vector;
using std::pair;
using std::map;
using std::set;
using std::optional;
using std::out_of_range;
using std::runtime_error;
using std::move_iterator;
using std::logic_error;
using std::back_inserter;
using std::move;
using std::format;
using std::ranges::views::drop;

enum class Strategy : int
{
    PassOne,
    PassRange,
    BlockOne,
    BlockRange,
};

template <>
struct std::formatter<Strategy, char>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        auto it = ctx.begin();
        if (it == ctx.end())
            return it;

        if (it != ctx.end() && *it != '}')
            throw std::format_error("Invalid format args for QuotableString.");

        return it;
    }

    template<class FormatContext>
    constexpr auto format(Strategy t, FormatContext& fc) const
    {
        string_view s;
        switch (t)
        {
        case Strategy::PassOne:
            s = nameof(Strategy::PassOne);
            break;
        case Strategy::BlockOne:
            s = nameof(Strategy::BlockOne);
            break;
        case Strategy::PassRange:
            s = nameof(Strategy::PassRange);
            break;
        case Strategy::BlockRange:
            s = nameof(Strategy::BlockRange);
            break;
        default:
            break;
        }
        
        return std::format_to(fc.out(), "{}", s);
    }
};

template <typename T>
struct Step
{
public:
    Strategy Signal = Strategy::PassOne;
    // TODO maybe merge below 3 field in the future
    T Data;
    T Left;
    T Right;

    /// <summary>
    /// for Step as key ordering in map, also important for iterate item in result of Graph::OutStepsOf--PassOne, PassRange... first, which is same as the order in Strategy definition
    /// </summary>
    auto operator< (Step const& that) const -> bool
    {
        if (Signal == that.Signal)
        {
            return OrderValue() < that.OrderValue();
        }
        else
        {
            return Signal < that.Signal;
        }
    }

    /// <summary>
    /// for compare in FiniteAutomataCraft.Run
    /// </summary>
    auto operator== (Step const& that) const -> bool
    {
        if (Signal == that.Signal)
        {
            switch (Signal)
            {
            case Strategy::PassOne:
            case Strategy::BlockOne:
                return Data == that.Data;
            case Strategy::PassRange:
            case Strategy::BlockRange:
                return Left == that.Left and Right == that.Right;
            }
        }
        return false;
    }

    auto operator!= (Step const& that) const -> bool
    {
        return not operator==(that);
    }

    /// <summary>
    /// for support not same type map.contains
    /// </summary>
    /// TODO remove in the future
    auto operator< (char c) const -> bool
    {
        // by actual test, below compare act as expected TODO think
        // should match the order semantic defined by Step < Step
        // user OrderValue to control compare branch correct, how to present pass and block
        // how to control pass or 
        switch (Signal)
        {
        case Strategy::PassOne:
            if (c == Data)
            {
                return false;
            }
            else if (c < Data)
            {
                return true;
            }
            else
            {
                return false;
            }
        case Strategy::BlockOne: 

            return Data == c;
        case Strategy::PassRange:
            if (c < Left)
            {
                return true;
            }
            else if (c > Right)
            {

            }
            if (c >= Left and c <= Right)
            {
                return true;
            }
            return false;
        }
        throw logic_error(std::format("not handled Signal: {}", static_cast<int>(Signal)));
    }
private:
    auto OrderValue() const -> float
    {
        float v = 0;
        switch (Signal)
        {
        case Strategy::PassOne:
            v = Data;
            v += 0.5;
            break;
        case Strategy::BlockOne:
            v = Data;
            break;
        case Strategy::PassRange:
            v = (Left + Right) / 2;
            v += 0.5;
            break;
        case Strategy::BlockRange:
            v = (Left + Right) / 2;
            break;
        default:
            throw logic_error(std::format("not handled Signal: {}", static_cast<int>(Signal)));
        }
        return v;
    }
};

auto operator! (Strategy const& a) -> Strategy
{
    switch (a)
    {
    case Strategy::BlockOne:
        return Strategy::PassOne;
    case Strategy::PassOne:
        return Strategy::BlockOne;
    case Strategy::PassRange:
        return Strategy::BlockRange;
    case Strategy::BlockRange:
        return Strategy::PassRange;
    default:
        throw logic_error(std::format("not handled Signal: {}", static_cast<int>(a)));
    }
}

template <typename T>
auto operator< (char b, Step<T> const& a) -> bool
{
    return not (a < b);
}

template <typename Input, typename AcceptStateResult>
class FiniteAutomata
{
private:
    Graph<Step<Input>> transitionTable;
    map<State, AcceptStateResult> acceptState2Result;

public:
    State const StartState;
    FiniteAutomata(Graph<Step<Input>> transitionTable, State startState, map<State, AcceptStateResult> acceptState2Result)
        : transitionTable(move(transitionTable)), StartState(startState), acceptState2Result(move(acceptState2Result))
    {
    }

    auto RunFromStart(char input, unsigned& stepCounter) const -> optional<pair<State, optional<AcceptStateResult>>>
    {
        return Run(StartState, input, stepCounter);
    }

    template <typename InputLike>
    auto Run(State from, InputLike input, unsigned& stepCounter) const -> optional<pair<State, optional<AcceptStateResult>>>
    {
        auto HandleResult = [&](State s)
        {
            if (acceptState2Result.contains(s))
            {
                return pair<State, optional<AcceptStateResult>>{ move(s), acceptState2Result.at(s) };
            }
            else
            {
                return pair<State, optional<AcceptStateResult>>{ move(s), {} };
            }
        };
        auto& steps = transitionTable.OutStepsOf(from);
        // temp not implement roll back in different steps
        for (auto& p : steps | drop(stepCounter))
        {
            ++stepCounter;
            if (stepCounter == steps.size())
            {
                stepCounter = std::numeric_limits<unsigned>::max();
            }
            Step<Input> const& step = p.first;
            // PassXXX first in order of steps which is ensured by Step::operator<'s result
            switch (step.Signal)
            {
            case Strategy::PassOne:
                if (step.Data == input)
                {
                    return HandleResult(p.second);
                }
                break;
            case Strategy::PassRange:
                if (input >= step.Left and input <= step.Right)
                {
                    return HandleResult(p.second);
                }
                break;
            case Strategy::BlockOne:
                if (input != step.Data)
                {
                    return HandleResult(p.second);
                }
                break;
            case Strategy::BlockRange:
                if (input < step.Left or input > step.Right)
                {
                    return HandleResult(p.second);
                }
                break;
            default:
                throw logic_error(format("not handled Strategy {}", static_cast<int>(step.Signal)));
            }
        }
        stepCounter = std::numeric_limits<unsigned>::max();
        return {};
    }
};

template <typename Input, typename AcceptStateResult>
class FiniteAutomataDraft
{
private:
    template <typename T, typename Char>
    friend struct std::formatter;
    template <typename Input, typename AcceptStateResult>
    friend auto NFA2DFA(FiniteAutomataDraft<Input, AcceptStateResult> nfa) -> FiniteAutomataDraft<Input, AcceptStateResult>;
    template <bool DivideAccepts, typename Input, typename AcceptStateResult>
    friend auto Minimize(FiniteAutomataDraft<Input, AcceptStateResult> dfa) -> FiniteAutomataDraft<Input, AcceptStateResult>;
    template <typename Input, typename AcceptStateResult>
    friend auto OrWithoutMergeAcceptState(vector<FiniteAutomataDraft<Input, AcceptStateResult>> fas) -> FiniteAutomataDraft<Input, AcceptStateResult>;

    static inline auto epsilon = Step<char>{ .Signal = Strategy::PassOne, .Data = 0 };
    GraphDraft<Step<Input>> transitionTable;

public:
    State StartState;
    vector<State> AcceptingStates;
    vector<pair<State, AcceptStateResult>> AcceptState2Result;

    static auto From(char c) -> FiniteAutomataDraft
    {
        auto transitionTable = GraphDraft<Step<Input>>();
        auto s0 = transitionTable.AllocateState();
        auto s1 = transitionTable.AllocateState();
        transitionTable.AddTransition(s0, Step{ .Data = c }, s1);

        return FiniteAutomataDraft(s0, { s1 }, move(transitionTable), {});
    }

    static auto Not(FiniteAutomataDraft a) -> FiniteAutomataDraft
    {
        a.transitionTable.Traverse([](pair<State, vector<pair<Step<Input>, State>>>& item)
        {
            for (auto& e : item.second)
            {
                if (e.first != epsilon)
                {
                    e.first.Signal = not e.first.Signal;
                }
            }
        });

        return a;
    }

    // follow priority order to run FiniteAutomataDraft<char> combination, parentheses, closure, concatenation and alternation
    static auto Or(FiniteAutomataDraft a, FiniteAutomataDraft b) -> FiniteAutomataDraft
    {
        auto newStart = a.transitionTable.AllocateState();
        auto newAccept = a.transitionTable.AllocateState();

        auto offset = a.transitionTable.Merge(move(b.transitionTable));
        a.transitionTable.AddTransition(newStart, epsilon, a.StartState);
        a.transitionTable.AddTransition(newStart, epsilon, b.StartState + offset);

        for (auto accept : a.AcceptingStates)
        {
            a.transitionTable.AddTransition(accept, epsilon, newAccept);
        }
        for (auto accept : b.AcceptingStates)
        {
            a.transitionTable.AddTransition(accept + offset, epsilon, newAccept);
        }

        a.StartState = newStart;
        a.AcceptingStates = { newAccept };
        return a;
    }

    static auto ZeroOrMore(FiniteAutomataDraft a) -> FiniteAutomataDraft
    {
        auto newStart = a.transitionTable.AllocateState();
        auto newAccept = a.transitionTable.AllocateState();

        a.transitionTable.AddTransition(newStart, epsilon, a.StartState);
        a.transitionTable.AddTransition(newStart, epsilon, newAccept);

        for (auto accept : a.AcceptingStates)
        {
            a.transitionTable.AddTransition(accept, epsilon, a.StartState);
            a.transitionTable.AddTransition(accept, epsilon, newAccept);
        }

        a.StartState = newStart;
        a.AcceptingStates = { newAccept };
        return a;
    }

    static auto Concat(FiniteAutomataDraft a, FiniteAutomataDraft b) -> FiniteAutomataDraft
    {
        auto offset = a.transitionTable.Merge(move(b.transitionTable));
        for (auto accept : a.AcceptingStates)
        {
            a.transitionTable.AddTransition(accept, epsilon, b.StartState + offset);
        }

        for (auto& accept : b.AcceptingStates)
        {
            accept += offset;
        }
        a.AcceptingStates = move(b.AcceptingStates);
        return a;
    }

    static auto Range(char a, char b) -> FiniteAutomataDraft
    {
        if (not (std::isalnum(a) and std::isalnum(b) and a < b))
        {
            throw std::out_of_range(format("{} and {} should be number or alphabet", a, b));
        }
        auto transitionTable = GraphDraft<Step<Input>>();
        auto start = transitionTable.AllocateState();
        auto accept = transitionTable.AllocateState();

        auto n0 = transitionTable.AllocateState();
        auto n1 = transitionTable.AllocateState();

        transitionTable.AddTransition(start, epsilon, n0);
        transitionTable.AddTransition(n0, Step{ .Signal = Strategy::PassRange, .Left = a, .Right = b }, n1);
        transitionTable.AddTransition(n1, epsilon, accept);

        return FiniteAutomataDraft(start, { accept }, move(transitionTable), {});
    }

    FiniteAutomataDraft(State StartState, vector<State> acceptingStates, GraphDraft<Step<Input>> transitionTable, vector<pair<State, AcceptStateResult>> acceptState2Result)
        : StartState(StartState), AcceptingStates(move(acceptingStates)), transitionTable(move(transitionTable)), AcceptState2Result(move(acceptState2Result))
    { }

    auto SetAcceptStateResult(AcceptStateResult acceptStateResult) -> void
    {
        for (auto s : AcceptingStates)
        {
            AcceptState2Result.push_back({ s, acceptStateResult });
        }
    }

    auto AcceptStateResultOf(State state) const -> AcceptStateResult
    {
        for (auto& p : AcceptState2Result)
        {
            if (p.first == state)
            {
                return p.second;
            }
        }
        throw runtime_error(format("cannot find accept state result for {}", state));
    }

    auto Run(State from, Step<Input> input) const -> set<State>
    {
        return transitionTable.Run(from, input);
    }

    auto Freeze() const -> FiniteAutomata<Input, AcceptStateResult>
    {
        // maybe some states don't have result? allow or not allow
        return FiniteAutomata<Input, AcceptStateResult>(transitionTable.Freeze(), StartState, map(move_iterator(AcceptState2Result.begin()), move_iterator(AcceptState2Result.end())));
    }
};

template<typename Input, typename Result>
struct std::formatter<FiniteAutomataDraft<Input, Result>, char>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        auto it = ctx.begin();
        if (it == ctx.end())
            return it;

        if (it != ctx.end() && *it != '}')
            throw std::format_error("Invalid format args for QuotableString.");

        return it;
    }

    template<class FormatContext>
    constexpr auto format(FiniteAutomataDraft<Input, Result>& t, FormatContext& fc) const
    {
        using std::format_to;
        std::string out;
        format_to(back_inserter(out), "startState: {}\n", t.StartState);
        format_to(back_inserter(out), "acceptingStates: ");
        for (auto s : t.AcceptingStates)
        {
            format_to(back_inserter(out), "{} ", s);
        }

        format_to(back_inserter(out), "\ntransitions: \n{}", t.transitionTable);
        return format_to(fc.out(), "{}", out);
    }
};

template<typename T>
struct std::formatter<Step<T>, char>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        auto it = ctx.begin();
        if (it == ctx.end())
            return it;

        if (it != ctx.end() && *it != '}')
            throw std::format_error("Invalid format args for QuotableString.");

        return it;
    }

    template<class FormatContext>
    constexpr auto format(Step<T>& t, FormatContext& fc) const
    {
        using std::format_to;
        std::string out;

        switch (t.Signal)
        {
        case Strategy::BlockOne:
        case Strategy::PassOne:
            format_to(back_inserter(out), "({}: {})", t.Signal, t.Data);
            break;
        case Strategy::BlockRange:
        case Strategy::PassRange:
            format_to(back_inserter(out), "({}: {}-{})", t.Signal, t.Left, t.Right);
            break;
        default:
            break;
        }
        return format_to(fc.out(), "{}", out);
    }
};

export
{
    template <typename T>
    struct Step;
    template <typename T>
    auto operator< (char b, Step<T> const& a) -> bool;
    auto operator! (Strategy const& a) -> Strategy;
    template <typename Input, typename Result>
    class FiniteAutomataDraft;
    template <typename Input, typename Result>
    class FiniteAutomata;
    template<typename Input, typename Result>
    struct std::formatter<FiniteAutomataDraft<Input, Result>, char>;
    template <>
    struct std::formatter<Strategy, char>;
    template<typename T>
    struct std::formatter<Step<T>, char>;
}