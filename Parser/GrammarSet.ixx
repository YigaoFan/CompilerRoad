export module Parser:GrammarSet;

import std;
import :ParserBase;
import :Terminal;
import Base;

using std::vector;
using std::string_view;
using std::pair;
using std::size_t;
using std::map;
using std::set;
using std::queue;
using std::array;
using std::tuple;
using std::move;
using std::ranges::views::transform;
using std::ranges::views::filter;
using std::ranges::to;
using std::format;
using std::ranges::views::keys;

//template <template <typename...> class Container, typename Value>
//auto SetUnion(Container<Value>&& set1, Container<Value>&& set2) -> Container<Value>
//{
//    using std::ranges::set_union;
//    using std::forward;
//
//    Container<Value> un;
//    set_union(forward<Container<Value>>(set1), forward<Container<Value>>(set2), std::inserter(un, un.begin()));
//    return un;
//}

/// <param name="changed">care about item in result is changed</param>
auto SetUnion(MergeSet<Terminal> set1, MergeSet<Terminal> const& set2, bool& changed) -> MergeSet<Terminal>
{
    for (auto& x : set2)
    {
        if (set1.Add(move(x)))
        {
            changed = true;
}
    }
    return set1;
}

auto SetUnion(MergeSet<Terminal> set1, MergeSet<Terminal> const& set2) -> MergeSet<Terminal>
{
    for (auto& x : set2)
    {
        set1.Add(move(x));
}
    return set1;
}

auto RemoveEpsilon(MergeSet<Terminal> s) -> MergeSet<Terminal>
{
    s.Remove(epsilon); // TODO it seems epsilon is same with EOF
    return s;
}

auto MarkWith(MergeSet<Terminal> s, Coordinate const& coordinate) -> MergeSet<Terminal>
{
    for (auto& x : s)
    {
        x.MarkWith(coordinate);
    }
    return s;
}

template <bool MarkWithLocationWhenNontermin = true>
auto GenAllSymbolFirstSetGetter(map<String, MergeSet<Terminal>> const& nonterminFirstSets)
{
    return [&nonterminFirstSets](String const& symbol, Coordinate currentLocation) -> MergeSet<Terminal>
    {
        if (nonterminFirstSets.contains(symbol))
        {
            auto s = nonterminFirstSets.at(symbol);
            if constexpr (MarkWithLocationWhenNontermin)
            {
                for (auto& x : s)
                {
                    x.MarkWith(currentLocation);
        }
            }
            return s;
        }
        return { Terminal(symbol, move(currentLocation)) };
        // not sure here temp memory allocation is big or small
    };
}

auto FirstSets(SimpleGrammars const& grammars, map<String, MergeSet<Terminal>> initFirstSets) -> map<String, MergeSet<Terminal>>
{
    using std::println;

    map<String, MergeSet<Terminal>> firstSets{ move(initFirstSets) };

    /// if it's possible terminal symbol, use this to read
    auto FirstsOf = GenAllSymbolFirstSetGetter(firstSets);

    for (auto changing = true; changing; )
    {
        //println("\nstart iteration");
        changing = false;
        for (auto const& g : grammars)
        {
            //println("process first set of {}", g.first);

            for (auto const& rule : g.second)
            {
                auto ruleIndex = static_cast<int>(&rule - &g.second.front());

                if (rule.empty())
                {
                    if (not firstSets.at(g.first).Contains(epsilon))
                    {
                        //println("add epsilon");
                        firstSets.at(g.first).Add(Terminal{ String(epsilon), Coordinate(g.first, ruleIndex, -1) });
                        changing = true;
                    }
                    continue;
                }
                auto rhs = FirstsOf(rule[0], Coordinate(g.first, ruleIndex, 0));
                //println("first set of first symbol: {}", rhs);

                for (size_t i = 1; i < rule.size(); ++i)
                {
                    if (rhs.Contains(epsilon))
                    {
                        if (auto nextFirst = FirstsOf(rule[i], Coordinate(g.first, ruleIndex, static_cast<int>(i))); nextFirst.Contains(epsilon))
                        {
                            //println("union next first set: {}", nextFirst);
							rhs = SetUnion(move(rhs), move(nextFirst));
                            //println("after union: {}", rhs);
                    }
                    else
                    {
                            //println("union next first set: {}", nextFirst);
                            rhs = SetUnion(move(rhs), move(nextFirst));
                            rhs = RemoveEpsilon(move(rhs));
                            //println("after union then remove epsilon: {}", rhs);
                        break;
                    }
                }
                    else
                {
                        break;
                }
                }

                auto itemChanged = false;
                auto newFirsts = SetUnion(firstSets[g.first], move(rhs), itemChanged);
                //println("try to cal new {} firsts: {}", g.first, newFirsts);
                if (newFirsts.Count() > firstSets[g.first].Count())
                {
                    //std::println("{} firsts updated", g.first);
                    firstSets[g.first] = move(newFirsts);
                    changing = true;
                }
            }
        }
    }

    return firstSets;
}

/// <returns>follow set not contains epsilon</returns>
auto FollowSets(String startSymbol, SimpleGrammars const& grammars, map<String, MergeSet<Terminal>> const& firstSets) -> map<String, MergeSet<Terminal>>
{
    map<String, MergeSet<Terminal>> followSets;
    for (auto const& nt : keys(firstSets))
    {
        followSets.insert({ nt, {} });
    }
    followSets.at(startSymbol) = { Terminal{ String(eof), Coordinate("", -1, -1) } };

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
                auto ruleIndex = static_cast<int>(&rule - &g.second.front());
                for (int i = static_cast<int>(rule.size() - 1); i >= 0; --i)
                {
                    auto& b = rule[i];
                    if (firstSets.contains(b))
                    {
                        bool itemChanged = false;
                        if (auto newFollows = SetUnion(followSets[b], trailer, itemChanged); newFollows.Count() > followSets[b].Count())
                        {
                            followSets[b] = move(newFollows);
                            changing = true;
                        }
                        if (auto const& fs = firstSets.at(b); fs.Contains(epsilon))
                        {
                            trailer = SetUnion(move(trailer), MarkWith(RemoveEpsilon(fs), Coordinate(g.first, ruleIndex, i)));
                            trailer = SetUnion(trailer, fs);
                        }
                        else
                        {
                            trailer = firstSets.at(b);
                        }
                    }
                    else
                    {
                        trailer = { Terminal(b, Coordinate(g.first, ruleIndex, i))};
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
auto StartSet(SimpleGrammar const& grammar, map<String, MergeSet<Terminal>> const& firstSets, map<String, MergeSet<Terminal>> const& followSets) -> vector<MergeSet<Terminal>>
{
    vector<MergeSet<Terminal>> start;
    auto FirstsOf = GenAllSymbolFirstSetGetter<true>(firstSets);
    auto FollowsOf = [&followSets](String const& symbol, Coordinate currentLocation) -> MergeSet<Terminal>
    {
        if (followSets.contains(symbol))
        {
            auto s = followSets.at(symbol);
            for (auto& x : s)
            {
                x.MarkWith(currentLocation);
			}
            return s;
        }
        throw std::out_of_range(format("{} is not non-terminal", symbol));
    };

    for (auto const& rule : grammar.second)
    {
        auto ruleIndex = static_cast<int>(&rule - &grammar.second.front());
        start.push_back({});
        for (auto const& sym : rule)
        {
            if (auto f = FirstsOf(sym, Coordinate(grammar.first, ruleIndex, static_cast<int>(&sym - &rule.front())));
                f.Contains(epsilon))
            {
                f = RemoveEpsilon(move(f));
                start.back() = SetUnion(move(f), move(start.back()));
            }
            else
            {
                start.back() = SetUnion(move(f), move(start.back()));
                goto NextRule;
            }
        }
        start.back() = SetUnion(move(start.back()), FollowsOf(grammar.first, Coordinate(grammar.first, ruleIndex, -1)));
    NextRule:
        continue;
    }
    return start;
}

struct GrammarSet
{
    map<String, MergeSet<Terminal>> FirstSets;
    map<String, MergeSet<Terminal>> FollowSets;
    SimpleGrammarsWithStartSet GrammarsWithStartSet;
    };
/// <returns>match the hierarchy of grammars, can use same index to access it</returns>
auto Starts(String startSymbol, SimpleGrammars const& grammars, map<String, set<String>> externalSymbolFirsts) -> GrammarSet
    {
	map<String, MergeSet<Terminal>> initFirstSets;
    for (auto const& externalSymbolFirst : externalSymbolFirsts)
        {
		MergeSet<Terminal> s;
		for (auto const& t : externalSymbolFirst.second)
            {
			s.Add(Terminal{ t, Coordinate("external-symbol", -1, -1) });
                    }
		initFirstSets.insert({ externalSymbolFirst.first, move(s) });
                }
    for (auto const& nt : keys(grammars))
{
        if (not initFirstSets.contains(nt))
    {
            initFirstSets.insert({ nt, {} });
        }
    }
    auto firsts = FirstSets(grammars, move(initFirstSets));
    auto follows = FollowSets(startSymbol, grammars, firsts);
    SimpleGrammarsWithStartSet starts;

    for (auto const& g : grammars)
{
        auto it = starts.insert({ g.first, {} });
        auto start = StartSet(g, firsts, follows);
        for (auto i = 0; auto& s : start)
    {
			it.first->second.push_back({ grammars.at(g.first).at(i), move(s)});
            ++i;
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
    return { .FirstSets = move(firsts), .FollowSets = move(follows), .GrammarsWithStartSet = move(starts) };
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

auto FillActionGotoTable(string_view startSymbol, vector<SimpleGrammar> const& grammars, map<set<Lr1Item>, size_t> const& cc, map<pair<set<Lr1Item>, string_view>, set<Lr1Item>> transitions)
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
        for (auto const& n : keys(grammars)) // non-terminals
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
    struct First2Set;
    SimpleGrammarsWithStartSet;
    auto Starts(String startSymbol, SimpleGrammars const& grammars, map<String, set<String>> externalSymbolFirsts = {}) -> GrammarSet;
}