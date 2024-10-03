export module TopDownParser;

import std;
import ParserBase;

using std::vector;
using std::string;
using std::pair;
using std::size_t;
using std::map;
using std::set;
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


auto DirectLeftRecur2RightRecur(Grammar grammar) -> vector<Grammar>
{
    auto const& left = grammar.first;
    vector<RightSide> leftRecurs;
    vector<RightSide> nonLeftRecurs;
    auto rightRecurNonTerm = left + '\'';

    for (auto& right : grammar.second)
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

// develop a shared const string?

auto RemoveIndirectLeftRecur(vector<Grammar> grammars) -> vector<Grammar>
{
    using std::ranges::views::transform;
    using std::ranges::views::filter;
    using std::ranges::views::join;
    using std::ranges::to;

    auto nontermins = grammars | transform([](auto& e) { return e.first; });
    for (size_t i = 0; i < nontermins.size(); ++i)
    {
        auto firsts = 
            grammars[i].second 
            | filter([](auto& i) { return not i.empty(); }) // filter affect the below index
            | transform([](auto& i) -> string { return i.front(); });
        // find grammars[i].first -> grammars[s].first form
        map<string, set<size_t>> first2Indexes;
        for (size_t j = 0; auto&& f : firsts)
        {
            // not sure the firsts will copy the first word from grammar or just a ref.
            // if a ref, the move will empty the first word of grammar which is unexpected.
            // I use explicit return type(string) to explicit copy it
            first2Indexes[f].insert(move(j));
            ++j;
        }

        // can we check if need to setup a new Grammar? or we can jump over below
        Grammar newGrammar{ grammars[i].first, {} }; // move grammars[i].first here is OK? no, grammar[s] will use it! but the latter one how to use this processed one
        for (size_t s = 0; s < i; ++s) // this means the ith grammar will be changed not only once
        {
            auto&& destFirst = grammars[s].first;
            if (first2Indexes.contains(destFirst))
            {
                // mark delete? check with book
                // do replace
                auto&& indexes = first2Indexes[destFirst];
                for (size_t j = 0; j < grammars[i].second.size(); ++j)
                {
                    auto&& rs = grammars[i].second[j];
                    if (indexes.contains(j))
                    {
                        rs.erase(rs.begin());
                        for (auto rs1 : grammars[s].second)
                        {
                            rs1.append_range(rs);
                            newGrammar.second.push_back(rs1);
                        }
                    }
                    else
                    {
                        newGrammar.second.push_back(move(rs));
                    }
                }
            }
        }
    }
    
    return move(grammars) // not trigger copy or copy?
        | transform([](auto&& i) { return DirectLeftRecur2RightRecur(move(i)); }) 
        | join | to<vector<Grammar>>();
}

auto LeftFactor()
{

}