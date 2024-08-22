export module Graph;

import std;
using std::vector;
using std::pair;

class Graph
{
private:
    vector<pair<int, vector<pair<char, int>>>> relations;
};

export
{
    class Graph;
}