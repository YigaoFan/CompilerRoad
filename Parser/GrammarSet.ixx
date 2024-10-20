export module GrammarSet;

import std;
import ParserBase;

using std::vector;
using std::string;
using std::string_view;
using std::pair;
using std::size_t;
using std::map;
using std::set;
using std::move;
using std::ranges::views::transform;
using std::ranges::views::filter;
using std::ranges::views::drop;
using std::ranges::to;

auto Nontermins(vector<Grammar> const& grammars)
{
    auto nontermins = grammars | transform([](auto& e) { return e.first; });
    return nontermins;
}

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
    using std::ranges::views::join;

    auto nontermins = Nontermins(grammars);
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
        // abc, abd, aed how to process max common prefix
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
        auto [newG, subNewGrammars] = LeftFactor(move(g));
        newGrammars.push_back(move(newG));
        if (not subNewGrammars.empty())
        {
            newGrammars.append_range(move(subNewGrammars));
        }
    }

    return { move(grammar), move(newGrammars) };
}

// TODO refine below two function's type to make it support move value from rvalue arg
template <template <typename...> class Container, typename Value>
auto SetDifference(Container<Value> const& set1, Container<Value> const& set2) -> Container<Value>
{
    using std::ranges::set_difference;
    Container<Value> diff;
    set_difference(set1, set2, std::inserter(diff, diff.begin()));
    return diff;
}

template <template <typename...> class Container, typename Value>
auto SetUnion(Container<Value> const& set1, Container<Value> const& set2) -> Container<Value>
{
    using std::ranges::set_union;
    Container<Value> un;
    set_union(set1, set2, std::inserter(un, un.begin()));
    return un;
}

auto RemoveEpsilon(set<string_view> s) -> set<string_view>
{
    s.erase(epsilon);
    return s;
}

auto GenAllSymbolFirstSetGetter(map<string_view, set<string_view>> const& nonterminFirstSets)
{
    return [&nonterminFirstSets](string const& symbol) -> set<string_view>
    {
        if (nonterminFirstSets.contains(symbol))
        {
            return nonterminFirstSets.at(symbol);
        }
        return { symbol };
        // not sure here temp memory allocation is big or small
    };
}

/// <returns>because of using string_view which is constructed from the string in grammars, 
/// so the return value should only be used while grammars is alive</returns>
auto FirstSets(vector<Grammar> const& grammars) -> map<string_view, set<string_view>>
{
    map<string_view, set<string_view>> firstSets;

    auto nontermins = Nontermins(grammars) | to<set<string_view>>();
    for (auto const& nt : nontermins)
    {
        firstSets.insert({ nt, {} });
    }

    /// if it's possible terminal symbol, use this to read
    auto FirstsOf = GenAllSymbolFirstSetGetter(firstSets);

    for (auto changing = false; changing; changing = false)
    {
        for (auto const& g : grammars)
        {
            for (auto const& rule : g.second)
            {
                if (rule.empty())
                {
                    continue;
                }
                auto rhs = RemoveEpsilon(FirstsOf(rule[0]));
                auto trailing = true;
                for (size_t i = 0; i < rule.size() - 1; ++i)
                {
                    if (FirstsOf(rule[i]).contains(epsilon))
                    {
                        rhs = SetUnion(rhs, RemoveEpsilon(FirstsOf(rule[i + 1])));
                    }
                    else
                    {
                        trailing = false;
                        break;
                    }
                }
                if (trailing and FirstsOf(rule.back()).contains(epsilon))
                {
                    rhs.insert(epsilon);
                }

                // how to remove below copy caused by union operation
                if (auto newFirsts = SetUnion(firstSets[g.first], rhs); newFirsts.size() > firstSets[g.first].size())
                {
                    firstSets[g.first] = move(newFirsts);
                    changing = true;
                }
            }
        }
    }

    return firstSets;
}

auto FollowSets(string_view startSymbol, vector<Grammar> const& grammars, map<string_view, set<string_view>> const& firstSets) -> map<string_view, set<string_view>>
{
    map<string_view, set<string_view>> followSets;
    auto nontermins = Nontermins(grammars) | to<set<string_view>>();
    for (auto const& nt : nontermins)
    {
        followSets.insert({ nt, {} });
    }
    followSets[startSymbol] = { eof };
    /// if it's possible terminal symbol, use this to read
    auto FirstsOf = GenAllSymbolFirstSetGetter(firstSets);

    for (auto changing = false; changing; changing = false)
    {
        for (auto const& g : grammars)
        {
            auto trailer = followSets[g.first];
            for (auto const& rule : g.second)
            {
                if (rule.empty())
                {
                    continue;
                }
                for (auto i = rule.size() - 1; i >= 0; --i)
                {
                    auto& b = rule[i];
                    if (nontermins.contains(b))
                    {
                        // how to remove below copy caused by union operation
                        if (auto newFollows = SetUnion(followSets[b], trailer); newFollows.size() > followSets[b].size())
                        {
                            followSets[b] = move(newFollows);
                            changing = true;
                        }
                        if (auto fs = FirstsOf(b); fs.contains(epsilon))
                        {
                            fs = RemoveEpsilon(move(fs));
                            trailer = SetUnion(trailer, fs);
                        }
                        else
                        {
                            trailer = firstSets.at(b);
                        }
                    }
                    else
                    {
                        trailer = { b };
                    }
                }
            }
        }
    }

    return followSets;
}

/// <summary>
/// start is for rule, first and follow are for terminal/nonterminal symbol
/// </summary>
auto StartSet(Grammar const& grammar, map<string_view, set<string_view>> const& firstSets, map<string_view, set<string_view>> const& followSets) -> vector<set<string_view>>
{
    vector<set<string_view>> starts;
    auto FirstsOf = GenAllSymbolFirstSetGetter(firstSets);
    for (auto const& rule : grammar.second)
    {
        starts.push_back({});
        if (not rule.empty())
        {
            for (auto const& sym : rule)
            {
                if (auto f = FirstsOf(sym); f.contains(epsilon))
                {
                    f = RemoveEpsilon(move(f));
                    starts.back() = SetUnion(f, starts.back());
                }
                else
                {
                    starts.back() = SetUnion(f, starts.back());
                    goto NextRule;
                }
            }
            starts.back() = SetUnion(starts.back(), followSets.at(grammar.first));
        }
    NextRule:
        continue;
    }
    return starts;
}

// TODO actual need startSymbol?
/// <returns>match the hierarchy of grammars, can use same index to access it</returns>
auto Starts(string_view startSymbol, vector<Grammar> const& grammars) -> vector<vector<set<string_view>>>
{
    auto firsts = FirstSets(grammars);
    auto follows = FollowSets(startSymbol, grammars, firsts);
    vector<vector<set<string_view>>> starts;
    starts.reserve(grammars.size());

    for (auto const& g : grammars)
    {
        starts.push_back(StartSet(g, firsts, follows));
    }
    return starts;
}

export
{
    auto Nontermins(vector<Grammar> const& grammars);
    auto Starts(string_view startSymbol, vector<Grammar> const& grammars) -> vector<vector<set<string_view>>>;
}