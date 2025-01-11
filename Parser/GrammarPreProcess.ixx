export module Parser:GrammarPreProcess;

import std;
import :ParserBase;
import :GrammarSet;

using std::vector;
using std::pair;
using std::map;
using std::set;
using std::variant;
using std::optional;
using std::move;
using std::ranges::to;
using std::ranges::views::filter;
using std::ranges::views::transform;
using std::ranges::views::drop;
using std::ranges::views::reverse;
using std::format;

/// <returns>.first is original noterminal, .second is new nonterminal</returns>
auto DirectLeftRecur2RightRecur(SimpleGrammar grammar) -> pair<bool, pair<SimpleGrammar, SimpleGrammar>>
{
    auto const& left = grammar.first;
    vector<SimpleRightSide> rightRecurs;
    vector<SimpleRightSide> originalRss;
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

auto RemoveIndirectLeftRecur(String startSymbol, vector<SimpleGrammar> grammars) -> vector<SimpleGrammar>
{
    using std::ranges::views::zip;
    using std::ranges::views::iota;
    using std::ranges::fold_left;

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
                        vector<SimpleRightSide> newRs;
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

    return grammars | filter([&](auto x) -> bool { return x.first == startSymbol or usingNonTerms.contains(x.first); }) | to<vector<SimpleGrammar>>();
}

/// <returns>.first is original noterminal, .second is new nonterminal</returns>
auto LeftFactor(SimpleGrammar grammar) -> pair<SimpleGrammar, optional<vector<SimpleGrammar>>>
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

    vector<SimpleGrammar> newGrammars;
    for (auto& [prefix, ids] : prefix2Indexes | filter([](auto& i) { return i.second.size() > 1; }))
    {
        auto suffixOfCommon = String(format("{}-{}-suffix", grammar.first, prefix));
        SimpleGrammar suffixGrammar{ suffixOfCommon, {} };
        // keep the first item of ids in grammar.second
        // drop the remain items
        auto& oldRs = grammar.second[ids.front()];
        suffixGrammar.second.push_back(SimpleRightSide{ make_move_iterator(oldRs.begin() + 1), make_move_iterator(oldRs.end()) });
        oldRs.erase(oldRs.begin() + 1, oldRs.end());
        oldRs.push_back(move(suffixOfCommon));

        for (auto i : reverse(ids | drop(1)))
        {
            auto& rs = grammar.second[i];
            rs.erase(rs.begin());
            suffixGrammar.second.push_back(move(rs));
            grammar.second.erase(grammar.second.begin() + i);
        }
        auto [newSuffixGrammar, addedGrammars] = LeftFactor(move(suffixGrammar));
        newGrammars.push_back(move(newSuffixGrammar));
        if (addedGrammars.has_value())
        {
            newGrammars.append_range(move(addedGrammars.value()));
        }
    }

    if (not newGrammars.empty())
    {
        return { move(grammar), move(newGrammars) };
    }
    else
    {
        return { move(grammar), {} };
    }
}

export
{
    auto LeftFactor(SimpleGrammar grammar) -> pair<SimpleGrammar, optional<vector<SimpleGrammar>>>;
    auto RemoveIndirectLeftRecur(String startSymbol, vector<SimpleGrammar> grammars) -> vector<SimpleGrammar>;
}