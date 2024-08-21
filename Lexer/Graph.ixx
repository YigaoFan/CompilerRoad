export module Graph;

import std;
using std::vector;
using std::pair;

class Graph
{
private:
    vector<pair<char, vector<pair<char, char>>>> relations;
};

export
{
    class Graph;
}