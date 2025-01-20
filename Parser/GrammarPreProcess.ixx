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
using std::stack;
using std::move;
using std::ranges::to;
using std::ranges::views::filter;
using std::ranges::views::transform;
using std::ranges::views::drop;
using std::ranges::views::reverse;
using std::format;

export constexpr auto rightRecurSuffix = "'";

/// <returns>.first is original noterminal, .second is new nonterminal</returns>
auto DirectLeftRecur2RightRecur(SimpleGrammar grammar) -> pair<bool, pair<SimpleGrammar, SimpleGrammar>>
{
    auto const& left = grammar.first;
    vector<SimpleRightSide> rightRecurs;
    vector<SimpleRightSide> originalRss;
    auto rightRecurNonTerm = left + rightRecurSuffix;
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
/// <summary>
/// also handle the direct left recursive
/// </summary>
auto RemoveIndirectLeftRecur(String startSymbol, vector<SimpleGrammar> grammars) -> vector<SimpleGrammar>
{
    auto nontermins = Nontermins(grammars) | to<vector<String>>();
    map<size_t, map<size_t, stack<pair<size_t, size_t>>>> replaceHistory;
    map<String, size_t> previousNontermins;

    for (size_t i = 0; i < nontermins.size(); previousNontermins.insert({ nontermins[i], i }), ++i)
    {
        auto& focus = grammars[i];
        vector<SimpleRightSide> newRss;
        for (auto& x : focus.second)
        {
            if ((not x.empty()) and previousNontermins.contains(x.front()))
            {
                // expand items into RightSides
                auto postfix = move(x);
                auto first = move(postfix.front());
                postfix.erase(postfix.begin());
                auto expandGrammarIndex = previousNontermins.at(first);
                auto const& expandGrammar = grammars[expandGrammarIndex];
                for (size_t k = 0; k < expandGrammar.second.size(); ++k)
                {
                    auto copy = expandGrammar.second[k];
                    copy.append_range(postfix);
                    replaceHistory[i][newRss.size()].push({ expandGrammarIndex, k });// because here use .size() as index, so it should before below push_back
                    newRss.push_back(move(copy));
                }
            }
            else
            {
                newRss.push_back(move(x));
            }
        }
        focus.second = move(newRss);
        
        auto [recursive, gs] = DirectLeftRecur2RightRecur(move(focus));
        auto&& [original, newNontermin] = gs;
        focus = move(original);
        if (recursive)
        {
            grammars.push_back(move(newNontermin));
        }
    }
    for (auto& [i, subHistory] : replaceHistory)
    {
        // it expand original grammar in the past, when we rollback, we need merge same rhs
        for (auto& [j, replaceOps] : subHistory)
        {
            if (auto& rhs = grammars[i].second[j]; not rhs.back().EndWith(rightRecurSuffix))
            {
                for (; not replaceOps.empty(); replaceOps.pop())
                {
                    auto const& op = replaceOps.top();
                    rhs.erase(rhs.begin(), rhs.begin() + grammars[op.first].second[op.second].size()); // if it's possible the op item is also replaced
                    rhs.insert(rhs.begin(), grammars[op.first].first);
                }
            }
        }
        using std::make_move_iterator;
        auto& rss = grammars[i].second;
        auto s = set(make_move_iterator(rss.begin()), make_move_iterator(rss.end()));// remove duplicate, maybe remove above calculate directly in the future
        grammars[i].second = vector(make_move_iterator(s.begin()), make_move_iterator(s.end()));
    }
    return grammars;
}

export constexpr auto leftFactorSuffix = "suffix";

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
        auto suffixOfCommon = String(format("{}-{}-{}", grammar.first, prefix, leftFactorSuffix));
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