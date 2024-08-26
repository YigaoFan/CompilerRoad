export module Graph;

import std;
using std::vector;
using std::pair;
using std::move;

class Graph
{
private:
    vector<pair<int, vector<pair<int, int>>>> transitions;

public:
    Graph() = default;
    Graph(vector<pair<int, vector<pair<int, int>>>> transitions) : transitions(move(transitions))
    { }

    auto AllocateState() -> int
    {
        auto s = transitions.size();
        transitions.push_back({ s, {} });
        return s;
    }

    auto AddTransition(int from, int charKind, int to) -> void
    {
        transitions[from].second.push_back({ charKind, to });
    }
};

export
{
    class Graph;
}