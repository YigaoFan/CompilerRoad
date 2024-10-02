export module TopDownParser;

import std;
import ParserBase;

using std::vector;
using std::string;
using std::pair;
using std::move;

using LeftSide = string;
using RightSide = vector<string>;
using Grammar = pair<LeftSide, vector<RightSide>>;

class TableDrivenParser
{
public:
    // how to distinguish nonterminal and terminal(which has enum type) in grammar
    auto ConstructFrom(vector<Grammar> grammars) -> TableDrivenParser
    {

    }

    auto Parse() -> ParserResult<int>
    {

    }
};


auto DirectLeftRecur2RightRecur(Grammar const& grammar) -> vector<Grammar>
{
    auto const& left = grammar.first;
    vector<RightSide> leftRecurs;
    vector<RightSide> nonLeftRecurs;
    auto rightRecurNonTerm = left + '\'';

    for (auto right : grammar.second)
    {
        if (not right.empty() and right.front() == left)
        {
            right.erase(right.begin());
            right.push_back(rightRecurNonTerm);
            leftRecurs.push_back(move(right));
        }
        else
        {
            right.push_back(rightRecurNonTerm);
            nonLeftRecurs.push_back(move(right));
        }
    }

    leftRecurs.push_back({});
    return
    {
        { left, move(nonLeftRecurs) },
        { rightRecurNonTerm, move(leftRecurs) },
    };
}

auto LeftFactor()
{

}