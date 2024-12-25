export module Parser:GrammarSet;

import std;
import :ParserBase;
import Base;

using std::vector;
using std::string_view;
using std::pair;
using std::size_t;
using std::map;
using std::set;
using std::queue;
using std::move;
using std::ranges::views::transform;
using std::ranges::views::filter;
using std::ranges::views::drop;
using std::ranges::to;
using std::format;

export auto Nontermins(vector<Grammar> const& grammars)
{
    auto nontermins = grammars | transform([](auto& e) { return e.first; });
    return nontermins;
}

/// <returns>.first is original noterminal, .second is new nonterminal</returns>
auto DirectLeftRecur2RightRecur(Grammar grammar) -> pair<bool, pair<Grammar, Grammar>>
{
    auto const& left = grammar.first;
    vector<RightSide> rightRecurs;
    vector<RightSide> originalRss;
    auto rightRecurNonTerm = left + '\'';
    auto leftRecursive = false;

    for (auto rs : grammar.second)
    {
        if ((not rs.empty()) and rs.front() == left)
        {
            leftRecursive = true;
            rs.erase(rs.begin());
            rs.push_back(rightRecurNonTerm);
            rightRecurs.push_back(move(rs));
        }
        else
        {
            rs.push_back(rightRecurNonTerm);
            originalRss.push_back(move(rs));
        }
    }

    if (leftRecursive)
    {
        rightRecurs.push_back({});
        return
        {
            true,
            {
                { left, move(originalRss) },
                { rightRecurNonTerm, move(rightRecurs) },
            }
        };
    }
    else
    {
        return
        {
            false,
            { grammar, { "", {} }}
        };
    }
}

auto RemoveIndirectLeftRecur(String startSymbol, vector<Grammar> grammars) -> vector<Grammar>
{
    using std::ranges::views::zip;
    using std::ranges::views::iota;
    using std::ranges::fold_left;
    using std::ranges::views::reverse;

    auto nontermins = Nontermins(grammars) | to<vector<String>>();
    for (size_t i = 0; i < nontermins.size(); ++i)
    {
        auto& focus = grammars[i];
        auto first2Indexes = fold_left(
            zip(focus.second, iota(0))
            | filter([](auto i) -> bool { return not std::get<0>(i).empty(); })
            | transform([](auto i) -> pair<String, int> { return { std::get<0>(i).front(), std::get<1>(i) }; }),
            map<String, vector<int>>{}, [](map<String, vector<int>> result, pair<String, int> item) -> map<String, vector<int>> { result[item.first].push_back(item.second); return result; });

        vector<int> laterRemoves;
        for (size_t s = 0; s < i; ++s) // grammar[i] will be changed multiple times
        {
            auto&& searchFirst = nontermins[s];
            // find grammars[i].first -> grammars[s].first xxx form
            if (first2Indexes.contains(searchFirst))
            {
                for (auto j : first2Indexes[searchFirst])
                {
                    if (not grammars[s].second.empty()) // TODO need to handle when false?
                    {
                        auto postfix = focus.second[j];
                        postfix.erase(postfix.begin());
                        vector<RightSide> newRs;
                        for (auto copy : grammars[s].second)
                        {
                            copy.append_range(postfix);
                            newRs.push_back(move(copy));
                        }
                        focus.second.append_range(move(newRs));
                    }
                }
                laterRemoves.append_range(move(first2Indexes[searchFirst]));
            }
        }
        for (auto j : reverse(laterRemoves))
        {
            focus.second.erase(focus.second.begin() + j);
        }
        auto [recursive, gs] = DirectLeftRecur2RightRecur(move(focus));
        auto&& [original, newNontermin] = gs;
        focus = move(original);
        if (recursive)
        {
            grammars.push_back(move(newNontermin));
        }
    }
    
    set<String> usingNonTerms;
    for (auto const& oldNonTers : Nontermins(grammars))
    {
        for (auto const& g : grammars)
        {
            for (auto const& rs : g.second)
            {
                for (auto const& x : rs)
                {
                    if (x == oldNonTers)
                    {
                        usingNonTerms.insert(oldNonTers);
                        goto NextNonTers;
                    }
                }
            }
        }
    NextNonTers:
        continue;
    }

    return grammars | filter([&](auto x) -> bool { return x.first == startSymbol or usingNonTerms.contains(x.first); }) | to<vector<Grammar>>();
}

/// <returns>.first is original noterminal, .second is new nonterminal</returns>
auto LeftFactor(Grammar grammar) -> pair<Grammar, vector<Grammar>>
{
    // should only left factor which is not left recursive
    using std::make_move_iterator;

    map<String, vector<size_t>> prefix2Indexes;
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
        auto newNonterminName = String(format("{}_lf_{}", grammar.first, prefix));
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
    return [&nonterminFirstSets](String const& symbol) -> set<string_view>
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

    for (auto changing = true; changing; )
    {
        changing = false;
        for (auto const& g : grammars)
        {
            for (auto const& rule : g.second)
            {
                if (rule.empty())
                {
                    if (not firstSets[g.first].contains(epsilon))
                    {
                        firstSets[g.first].insert(epsilon);
                        changing = true;
                    }
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
                    std::println("{} firsts changed: {}", g.first, newFirsts);
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

    for (auto changing = true; changing; )
    {
        changing = false;
        for (auto const& g : grammars)
        {
            for (auto const& rule : g.second)
            {
                if (rule.empty())
                {
                    continue;
                }
                auto trailer = followSets[g.first];
                for (int i = static_cast<int>(rule.size() - 1); i >= 0; --i)
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
    NextRule:
        continue;
    }
    return starts;
}

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


auto GrammarOf(string_view nonterminal, vector<Grammar> const& grammars) -> Grammar const&
{
    for (auto const& i : grammars)
    {
        if (i.first == nonterminal)
        {
            return i;
        }
    }
    throw std::out_of_range(format("not found grammar for {}", nonterminal));
}

using Lr1Item = std::tuple<pair<LeftSide, RightSide>, int, string_view>;
auto Closure(set<Lr1Item> s, vector<Grammar> const& grammars, map<string_view, set<string_view>> const& firstSets) -> set<Lr1Item>
{
    set<string_view> const nonterminals = Nontermins(grammars) | to<set<string_view>>();
    auto First = [&firstSets](this auto&& self, vector<string_view> const& rs) -> set<string_view>
    {
        if (rs.empty())
        {
            return { epsilon };
        }
        if (not self.firstSets.contains(rs.front())) // it's terminal
        {
            return { rs.front() };
        }
        else
        {
            auto f = self.firstSets.at(rs.front());
            if (f.contains(epsilon))
            {
                f.insert_range(self(vector<string_view>{ rs.begin() + 1, rs.end() }));
            }
            return f;
        }
    };

    for (auto changing = true; changing;)
    {
        changing = false;
        for (auto const& [rule, i, _] : s)
        {
            if (auto const& expect = rule.second[i]; nonterminals.contains(expect))
            {
                vector<string_view> lookahead{ rule.second.begin() + i + 1, rule.second.end() };
                for (auto const& p : GrammarOf(expect, grammars).second)
                {
                    for (auto b : First(lookahead))
                    {
                        auto [_, inserted] = s.insert({ pair{ expect, p }, 0, move(b) });
                        changing = inserted;
                    }
                }
            }
        }
    }
    return s;
}

auto Goto(set<Lr1Item> s, string_view x, vector<Grammar> const& grammars, map<string_view, set<string_view>> const& firstSets) -> set<Lr1Item>
{
    set<Lr1Item> t;
    for (auto const& [rule, i, lookahead] : s)
    {
        if (rule.second[i] == x)
        {
            t.insert({ rule, i + 1, lookahead });
        }
    }
    return Closure(move(t), grammars, firstSets);
}

auto BuildCanonicalCollectionOfSetsOfLr1Items(string_view startSymbol, vector<Grammar> const& grammars) -> pair<map<set<Lr1Item>, size_t>, map<pair<set<Lr1Item>, string_view>, set<Lr1Item>>>
{
    set<Lr1Item> cc0;
    auto const& g = GrammarOf(startSymbol, grammars);
    for (auto const& i : g.second)
    {
        cc0.insert({ pair{ g.first, i }, 0, eof });
    }
    auto firsts = FirstSets(grammars);
    cc0 = Closure(move(cc0), grammars, firsts);

    map<set<Lr1Item>, size_t> cc;
    queue<set<Lr1Item>> workingList;
    workingList.push(move(cc0));
    map<pair<set<Lr1Item>, string_view>, set<Lr1Item>> transitions;

    for (; not workingList.empty();)
    {
        auto cci = move(workingList.front());
        workingList.pop();
        cc.insert({ cci, cc.size() });
        auto afterPlaceholderSymbols = cci
            | filter([](Lr1Item const& x) -> bool { return std::get<1>(x) < std::get<0>(x).second.size() - 1; })
            | transform([](Lr1Item const& x) -> string_view { return std::get<0>(x).second[static_cast<size_t>(std::get<1>(x)) + 1]; })
            | to<set<string_view>>();
        for (auto x : afterPlaceholderSymbols)
        {
            set<Lr1Item> temp = Goto(cci, x, grammars, firsts);
            if (not cc.contains(temp))
            {
                workingList.push(temp);
            }
            transitions.insert({ pair{ move(cci), x }, move(temp) });
        }
    }
    return { move(cc), move(transitions) };
}


struct Action
{
    enum class Type
    {
        Shift,
        Reduce,
        Accept,
    } Type;
    union
    {
        size_t J;
        pair<string_view, size_t> ItemsToWhat;
    };
};

auto FillActionGotoTable(string_view startSymbol, vector<Grammar> const& grammars, map<set<Lr1Item>, size_t> const& cc, map<pair<set<Lr1Item>, string_view>, set<Lr1Item>> transitions)
    -> pair<map<pair<size_t, string_view>, Action>, map<pair<size_t, string_view>, size_t>>
{
    map<pair<size_t, string_view>, Action> actions;
    map<pair<size_t, string_view>, size_t> gotos;

    for (auto const& cci : cc)
    {
        for (auto const& [rule, pos, lookahead] : cci.first)
        {
            if (pos < rule.second.size())
            {
                string_view sym = rule.second[pos];
                if (auto p = pair{ cci.first, sym }; transitions.contains(p))
                {
                    actions.insert({ { cci.second, sym }, Action{.Type = Action::Type::Shift, .J = cc.at(transitions.at(p)) } });
                }
            }
            else if (pos == rule.second.size())
            {
                if (rule.first == startSymbol and lookahead == eof)
                {
                    actions.insert({ { cci.second, eof }, Action{.Type = Action::Type::Accept } });
                }
                else
                {
                    actions.insert({ { cci.second, lookahead }, Action{.Type = Action::Type::Reduce, .ItemsToWhat = { rule.first, rule.second.size() }} });
                }
            }
        }
        for (auto const& n : Nontermins(grammars))
        {
            if (auto p = pair{ cci.first, n }; transitions.contains(p))
            {
                gotos.insert({ { cci.second, n}, cc.at(transitions.at(p)) });
            }
        }
    }

    return { move(actions), move(gotos) };
}

export
{
    auto Starts(string_view startSymbol, vector<Grammar> const& grammars) -> vector<vector<set<string_view>>>;
    auto LeftFactor(Grammar grammar) -> pair<Grammar, vector<Grammar>>;
    auto RemoveIndirectLeftRecur(String startSymbol, vector<Grammar> grammars) -> vector<Grammar>;
}