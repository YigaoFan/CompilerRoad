export module Graph;

import std;
using std::vector;
using std::pair;
using std::size_t;
using std::set;
using std::string;
using std::optional;
using std::map;
using std::move;

export using State = size_t;
export using StateComp = std::less<void>;

/// <summary>
/// support DFA now
/// </summary>
/// <typeparam name="InputItem"></typeparam>
template <typename InputItem>
class Graph
{
private:
    map<State, map<InputItem, State, StateComp>> transitions;
public:
    Graph(map<State, map<InputItem, State, StateComp>> transitions) : transitions(move(transitions))
    {
    }

    /// <summary>
    /// recommend for DFA which <param>from</param> and <param>input</param> are identical to one path
    /// </summary>
    template <typename InputLike>
    auto Run(State from, InputLike input) const -> optional<State>
    {
        if (auto& t = transitions.at(from); t.contains(input))
        {
            return t.find(input)->second; // template <class Key> at(Key const&) is supported from C++26
        }
        return {};
    }

    auto OutStepsOf(State s) const -> map<InputItem, State, StateComp> const&
    {
        if (transitions.contains(s))
        {
            return transitions.at(s);
        }
        throw std::logic_error(std::format("no state {} in Graph", s));
    }
};

template <typename InputItem>
class GraphDraft
{
private:
    template <typename T, typename Char>
    friend struct std::formatter;
    vector<pair<State, vector<pair<InputItem, State>>>> transitions;

public:
    GraphDraft() = default;
    GraphDraft(vector<pair<int, vector<pair<InputItem, int>>>> transitions) : transitions(move(transitions))
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

    auto Merge(GraphDraft b) -> size_t
    {
        auto offset = this->StatesCount();
        b.OffsetStates(offset);
        transitions.reserve(transitions.size() + b.transitions.size());
        transitions.append_range(move(b.transitions));
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
    auto Run(State from, InputItem input) const -> set<State>
    {
        set<State> nexts;
        for (auto& t : transitions[from].second)
        {
            if (t.first == input)
            {
                nexts.insert(t.second);
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

    /// <summary>
    /// support DFA freeze currently
    /// </summary>
    auto Freeze() const -> Graph<InputItem>
    {
        map<State, map<InputItem, State, StateComp>> transitionMap;
        for (auto& t : transitions)
        {
            map<InputItem, State, StateComp> rh;
            for (auto& i : t.second)
            {
                if (not rh.contains(i.first))
                {
                    rh.insert(i);
                }
                else
                {
                    throw std::logic_error(std::format("already exist transition and its value {}", rh.at(i.first)));
                }
            }
            transitionMap.insert({ t.first, move(rh) });
        }
        return Graph(move(transitionMap));
    }

    template <typename Func>
    auto Traverse(Func func) -> void
    {
        for (auto& x : transitions)
        {
            func(x);
        }
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


template<typename T>
struct std::formatter<GraphDraft<T>, char>
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
    constexpr auto format(GraphDraft<T>& t, FormatContext& fc) const
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
    class GraphDraft;
    template <typename InputItem>
    class Graph;

    template<>
    struct std::formatter<GraphDraft<char>>;
}