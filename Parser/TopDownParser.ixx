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
using std::ranges::views::filter;
using std::ranges::views::drop;

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

/// <returns>.first is original noterminal, .second is new nonterminal</returns>
auto DirectLeftRecur2RightRecur(Grammar grammar) -> pair<Grammar, Grammar>
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

auto RemoveIndirectLeftRecur(vector<Grammar> grammars) -> vector<Grammar>
{
    using std::ranges::views::transform;
    using std::ranges::views::join;
    using std::ranges::to;

    auto nontermins = grammars | transform([](auto& e) { return e.first; });
    for (size_t i = 0; i < nontermins.size(); ++i)
    {
        auto& focus = grammars[i];
        auto firsts = 
            focus.second 
            | filter([](auto& i) { return not i.empty(); }) // filter affect the below index
            | transform([](auto& i) -> string { return i.front(); });
        map<string, set<size_t>> first2Indexes;
        for (size_t j = 0; auto&& f : firsts)
        {
            // not sure the firsts will copy the first word from grammar or just a ref.
            // if a ref, the move will empty the first word of grammar which is unexpected.
            // I use explicit return type(string) to explicit copy it
            first2Indexes[f].insert(move(j));
            ++j;
        }

        for (size_t s = 0; s < i; ++s) // grammar[i] will be changed multiple times
        {
            auto&& destFirst = grammars[s].first;
            // find grammars[i].first -> grammars[s].first form
            if (first2Indexes.contains(destFirst))
            {
                auto&& destIndexes = first2Indexes[destFirst];
                for (size_t j = 0; j < focus.second.size(); ++j)
                {
                    auto& rs = focus.second[j];
                    if (destIndexes.contains(j))
                    {
                        rs.erase(rs.begin());

                        if (not grammars[s].second.empty())
                        {
                            for (auto replace : grammars[s].second | drop(1))
                            {
                                replace.append_range(rs);
                                focus.second.push_back(replace);
                            }
                            // delay the 1st item operation, because it will change rs itself which is used in above
                            rs.insert_range(rs.begin(), grammars[s].second[0]);
                        }
                    }
                }
            }
        }
        auto [original, newNontermin] = DirectLeftRecur2RightRecur(move(focus));
        focus = move(original);
        grammars.push_back(move(newNontermin));
    }
    
    return grammars;
}

/// <returns>.first is original noterminal, .second is new nonterminal</returns>
auto LeftFactor(Grammar grammar) -> pair<Grammar, vector<Grammar>>
{
    using std::format;
    using std::make_move_iterator;

    map<string, vector<size_t>> prefix2Indexes;
    for (size_t i = 0; i < grammar.second.size(); ++i)
    {
        // should get max common prefix TODO
        auto& rs = grammar.second[i];
        if (not rs.empty())
        {
            prefix2Indexes[rs.front()].push_back(i);
        }
    }

    vector<Grammar> newGrammars;
    for (auto& [prefix, ids] : prefix2Indexes | filter([](auto& i) { return i.second.size() > 1; }))
    {
        auto newNonterminName = format("{}_lf_{}", grammar.first, prefix);
        Grammar g{ newNonterminName, {} };
        // keep the first item of ids in grammar.second
        // drop the remain items
        auto& rs = grammar.second[ids[0]];
        g.second.push_back(RightSide{ make_move_iterator(rs.begin() + 1), make_move_iterator(rs.end()) });
        rs.erase(rs.begin() + 1, rs.end());
        rs.push_back(move(newNonterminName));

        for (auto i : ids | drop(1))
        {
            auto& rs = grammar.second[i];
            rs.erase(rs.begin());
            g.second.push_back(move(rs));
            grammar.second.erase(grammar.second.begin() + i);
        }
        newGrammars.push_back(move(g));
    }

    return { move(grammar), move(newGrammars) };
}