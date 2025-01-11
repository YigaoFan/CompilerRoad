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
using std::ranges::to;
using std::format;

export auto Nontermins(vector<SimpleGrammar> const& grammars)
{
    auto nontermins = grammars | transform([](auto& e) { return e.first; });
    return nontermins;
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
auto FirstSets(vector<SimpleGrammar> const& grammars) -> map<string_view, set<string_view>>
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

auto FollowSets(string_view startSymbol, vector<SimpleGrammar> const& grammars, map<string_view, set<string_view>> const& firstSets) -> map<string_view, set<string_view>>
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
auto StartSet(SimpleGrammar const& grammar, map<string_view, set<string_view>> const& firstSets, map<string_view, set<string_view>> const& followSets) -> vector<set<string_view>>
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
auto Starts(string_view startSymbol, vector<SimpleGrammar> const& grammars) -> vector<vector<set<string_view>>>
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


auto GrammarOf(string_view nonterminal, vector<SimpleGrammar> const& grammars) -> SimpleGrammar const&
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

using Lr1Item = std::tuple<pair<LeftSide, SimpleRightSide>, int, string_view>;
auto Closure(set<Lr1Item> s, vector<SimpleGrammar> const& grammars, map<string_view, set<string_view>> const& firstSets) -> set<Lr1Item>
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

auto Goto(set<Lr1Item> s, string_view x, vector<SimpleGrammar> const& grammars, map<string_view, set<string_view>> const& firstSets) -> set<Lr1Item>
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

auto BuildCanonicalCollectionOfSetsOfLr1Items(string_view startSymbol, vector<SimpleGrammar> const& grammars) -> pair<map<set<Lr1Item>, size_t>, map<pair<set<Lr1Item>, string_view>, set<Lr1Item>>>
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

auto FillActionGotoTable(string_view startSymbol, vector<SimpleGrammar> const& grammars, map<set<Lr1Item>, size_t> const& cc, map<pair<set<Lr1Item>, string_view>, set<Lr1Item>> transitions)
    -> pair<map<pair<size_t, string_view>, Action>, map<pair<size_t, string_view>, size_t>>
{
    map<pair<size_t, string_view>, Action> actions;
    map<pair<size_t, string_view>, size_t> gotos;
    auto nontermins = Nontermins(grammars);

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
        for (auto const& n : nontermins)
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
    auto Starts(string_view startSymbol, vector<SimpleGrammar> const& grammars) -> vector<vector<set<string_view>>>;
}