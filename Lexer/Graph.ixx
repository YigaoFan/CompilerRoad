export module Graph;

import std;
using std::vector;
using std::pair;
using std::size_t;
using std::move;

using State = size_t;

template <typename InputItem>
class Graph
{
private:
    vector<pair<State, vector<pair<InputItem, State>>>> transitions;

public:
    Graph() = default;
    Graph(vector<pair<int, vector<pair<InputItem, int>>>> transitions) : transitions(move(transitions))
    { }

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

export
{
    template <typename InputItem>
    class Graph;
    using State = size_t;
}