export module Graph;

import std;
using std::vector;
using std::pair;
using std::size_t;
using std::set;
using std::string;
using std::move;

using State = size_t;

template <typename InputItem>
class Graph
{
private:
    template <typename T, typename Char>
    friend struct std::formatter;
    vector<pair<State, vector<pair<InputItem, State>>>> transitions;

public:
    Graph() = default;
    Graph(vector<pair<int, vector<pair<InputItem, int>>>> transitions) : transitions(move(transitions))
    { }

    auto operator[](State from) const -> vector<pair<InputItem, State>> const&
    {
        return transitions[from].second;
    }
    /// <summary>
    /// state should keep [0..n] continuously
    /// </summary>
    /// <returns></returns>
    auto AllocateState() -> State
    {
        auto s = transitions.size();
        transitions.push_back({ s, {} });
        return s;
    }

    auto AddTransition(State from, InputItem input, State to) -> void
    {
        transitions[from].second.push_back({ input, to });
    }

    auto Merge(Graph b) -> size_t
    {
        auto offset = this->StatesCount();
        b.OffsetStates(offset);
        transitions.reserve(transitions.size() + b.transitions.size());
        transitions.append_range(move(b.transitions));
        //move(b.transitions.begin(), b.transitions.end(), std::back_inserter(transitions));
        return offset;
    }

    auto AllPossibleInputs() const -> set<InputItem>
    {
        auto inputs = set<InputItem>();
        for (auto& x : transitions)
        {
            for (auto& y : x.second)
            {
                inputs.insert(y.first);
            }
        }
        return inputs;
    }

    auto AllStates() const -> set<State>
    {
        auto states = set<State>();
        for (auto& x : transitions)
        {
            states.insert(x.first);
        }
        return states;
    }

    /// <summary>
    /// for NFA, identical from and input may goto multiple states
    /// </summary>
    auto Run(State from, InputItem input) const -> vector<State>
    {
        vector<State> nexts;
        for (auto& t : transitions[from].second)
        {
            if (t.first == input)
            {
                nexts.push_back(t.second);
            }
        }
        return nexts;
    }

    /// <summary>
    /// Only for DFA
    /// </summary>
    auto ReverseRun(State to, InputItem input) const -> vector<State>
    {
        vector<State> pres;
        for (auto& x : transitions)
        {
            for (auto& y : x.second)
            {
                if (y.first == input && y.second == to)
                {
                    pres.push_back(x.first);
                    break;
                }
            }
        }
        return pres;
    }
private:
    auto OffsetStates(size_t offset) -> void
    {
        for (auto& x : transitions)
        {
            x.first += offset;
            for (auto& y : x.second)
            {
                y.second += offset;
            }
        }
    }

    auto StatesCount() const -> size_t
    {
        return transitions.size();
    }
};


template<>
struct std::formatter<Graph<char>, char> // TODO after VS 17.12 it will support partial specification in other module
{
    template<class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext& ctx)
    {
        auto it = ctx.begin();
        if (it == ctx.end())
            return it;

        if (it != ctx.end() && *it != '}')
            throw std::format_error("Invalid format args for QuotableString.");

        return it;
    }

    template<class FormatContext>
    constexpr auto format(Graph<char>& t, FormatContext& fc) const
    {
        using std::back_inserter;
        using std::format_to;
        std::string out;
        for (auto& t : t.transitions)
        {
            format_to(back_inserter(out), "{}:\n", t.first);
            for (auto i = 0; auto & rule : t.second)
            {
                format_to(back_inserter(out), "-{}-> {};\n", rule.first, rule.second);
            }
        }
        return format_to(fc.out(), "{}", out);
    }
};

export
{
    template <typename InputItem>
    class Graph;
    using State = size_t;
    template<>
    struct std::formatter<Graph<char>>;
}