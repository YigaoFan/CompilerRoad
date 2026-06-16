export module Parser:GrammarProcess;

import std;
import Base;
import :ParserBase;
import :GrammarSet;

using std::vector;
using std::pair;
using std::map;
using std::set;
using std::variant;
using std::optional;
using std::stack;
using std::logic_error;
using std::shared_ptr;
using std::move;
using std::ranges::to;
using std::ranges::views::filter;
using std::ranges::views::transform;
using std::ranges::views::drop;
using std::ranges::views::reverse;
using std::format;
using std::holds_alternative;
using std::get;

export constexpr auto rightRecurSuffix = "'";

struct ConvertedGrammar
{
    /// <summary>
    /// original nonterminal with new right hand sides
    /// </summary>
    SimpleGrammar Original;
    /// <summary>
	/// auxiliary grammar for implementing the right recursion.
    /// The left hand side is the new nonterminal which is referenced in Original's right hand sides
    /// </summary>
    SimpleGrammar Auxiliary;
};

/// <returns>.first is original nonterminal, .second is new nonterminal</returns>
auto DirectLeftRecur2RightRecur(SimpleGrammar const& grammar) -> optional<ConvertedGrammar>
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
        return ConvertedGrammar
        {
            .Original = { left, move(originalRss) },
            .Auxiliary = { move(rightRecurNonTerm), move(rightRecurs) },
        };
    }

    return {};
}

export struct History
{
public:
    struct IConvertUnit
    {
        virtual ~IConvertUnit() = default;
    };
    struct ExpandFront : IConvertUnit
    {
        /// <summary>
        /// nonterminal being expanded
        /// </summary>
        String const Nonterminal;
        /// <summary>
        /// right side of @Nonterminal is using to expand
        /// </summary>
        SimpleRightSide const RightSide;
		/// <summary>
		/// expanded right side after applying the expansion
		/// </summary>
		SimpleRightSide const ExpandedRightSide;

        ExpandFront(String nonterminal, SimpleRightSide rightSide, SimpleRightSide expandedRightSide)
            : Nonterminal(move(nonterminal)), RightSide(move(rightSide)), ExpandedRightSide(move(expandedRightSide))
        {
        }

        template <typename Tok, typename Result>
        auto Undo(SyntaxTreeNode<Tok, Result> node) const -> decltype(node)
        {
            Assert(std::ranges::starts_with(node.ChildSymbols, RightSide), "RecoverSyntaxNode: node.ChildSymbols does not start with RightSide");
            auto packedChild = node.PackOutChildrenAsNode(static_cast<int>(RightSide.size()), Nonterminal);
            node.Children.insert(node.Children.begin(), move(packedChild));
            node.ChildSymbols.insert(node.ChildSymbols.begin(), Nonterminal);
            return node;
        }
    };
    struct Left2RightRecur : IConvertUnit
    {
        /// <summary>
        /// feel free to copy this object, the inner data is shared which is only stored one instance
        /// the base data to recover the children of Left-Recursion AST node
        /// </summary>
        struct Context
        {
            /// <summary>
            /// the original left recursive grammar
            /// </summary>
            SimpleGrammar OriginalGrammar;
            /// <summary>
            /// the auxiliary grammar for implementing the right recursion grammar
            /// </summary>
            SimpleGrammar AuxiliaryGrammar;
            /// <summary>
            /// Assume all is ExpandFront which is used to recover the original grammar after left recursion is recoverd.
            /// </summary>
            map<int, History> OriginalFrontReplaceHistory;

            auto Find(SimpleRightSide const& symbols) const -> optional<int>
            {
                auto it = std::ranges::find(OriginalGrammar.second, symbols);
				if (it != OriginalGrammar.second.end())
				{
					return static_cast<int>(std::ranges::distance(OriginalGrammar.second.begin(), it));
				}
				return {};
            }
        };

        Context Ctx;

        Left2RightRecur(Context ctx) : Ctx(move(ctx)) {}

        /// <summary>
        /// expect node.Name is the left recursive nonterminal
        /// </summary>
        template <typename Tok, typename Result>
        auto Undo(SyntaxTreeNode<Tok, Result> node) const -> decltype(node)
        {
            auto SplitOutLastChild = [](SyntaxTreeNode<Tok, Result>& node) -> SyntaxTreeNode<Tok, Result>
            {
                node.ChildSymbols.pop_back();
                auto lastChild = move(node.Children.back());
                node.Children.pop_back();
                return move(std::get<SyntaxTreeNode<Tok, Result>>(lastChild));
            };
            auto RecoverExpand = [this](SyntaxTreeNode<Tok, Result>& node)->void
            {
                if (auto i = Ctx.Find(node.ChildSymbols); i.has_value())
                {
                    if (Ctx.OriginalFrontReplaceHistory.contains(i.value()))
                    {
                        auto const& history = Ctx.OriginalFrontReplaceHistory.at(i.value());
                            node = history.Undo(move(node));
                        }
                    }
            };
			auto Iter = [this, leftRecurName = node.Name, &SplitOutLastChild, &RecoverExpand](this auto&& self, SyntaxTreeNode<Tok, Result> node, SyntaxTreeNode<Tok, Result> toAddChild) -> SyntaxTreeNode<Tok, Result>
			{
				auto InsertAtHead = [](SyntaxTreeNode<Tok, Result>& node, SyntaxTreeNode<Tok, Result> toAddChild)
				{
					node.ChildSymbols.insert(node.ChildSymbols.begin(), toAddChild.Name);
					node.Children.insert(node.Children.begin(), move(toAddChild));
				};
				node.Name = self.leftRecurName;
				if ((not node.ChildSymbols.empty()) and node.ChildSymbols.back() == Ctx.AuxiliaryGrammar.first)
				{
					auto lastChild = SplitOutLastChild(node);
					InsertAtHead(node, move(toAddChild));
                    RecoverExpand(node);
					return self(move(lastChild), move(node));
				}
				else
				{
					return toAddChild;
				}
			};
			// pass a reconstruct info structure as an argument
			if (not node.ChildSymbols.empty()) // Do we need this when A -> A a | empty? Iter has empty judge
			{
				auto lastChild = SplitOutLastChild(node);
                RecoverExpand(node);
				return Iter(move(lastChild), move(node));
			}

			return node;
		}
	};

private:
	vector<shared_ptr<IConvertUnit>> convertHistory;

public:
	History() = default;
	auto Count() const -> size_t { return convertHistory.size(); }
	auto At(size_t index) const -> IConvertUnit const& { return *convertHistory.at(index); }

    auto Merge(History const& that) -> void
    {
        // ExpandFront and ExpandFront: OK
        // ExpandFront and Left2RightRecur: OK
		// Left2RightRecur and ExpandFront: 
        // should not happen, because Left2RightRecur is used to convert direct left recursive grammar,
        // which should be the last step in the conversion process, and after that there should be no more ExpandFront added into history
		convertHistory.append_range(that.convertHistory);
	}

    auto Add(shared_ptr<IConvertUnit> convertUnit) -> void
    {
		convertHistory.push_back(move(convertUnit));
	}

	template <typename Tok, typename Result>
    auto Undo(SyntaxTreeNode<Tok, Result> node) const -> decltype(node)
    {
        for (auto it = convertHistory.rbegin(); it != convertHistory.rend(); ++it)
        {
			auto converter = (*it).get();
            if (auto expand = dynamic_cast<ExpandFront*>(converter))
            {
                node = expand->Undo(move(node));
			} else if (auto left2RightRecur = dynamic_cast<Left2RightRecur*>(converter))
            {
                node = left2RightRecur->Undo(move(node));
			}
        }
        return node;
	}
};

struct ConvertResult
{
    SimpleGrammars ConvertedGrammars;
    /// <summary>
	/// Item for each nonterminal in ConvertHistory is ensured. If no replacement happens for a rule, its value does not exist.
    /// </summary>
    map<String,  map<int, History>> ConvertHistory;
};

auto ExpandFrontWithPreviousNontermins(SimpleRightSide rule, History initBaseHistory, SimpleGrammars const& grammars, vector<String> const& previousNontermins, map<String, map<int, History>>& globalReplaceHistory) -> vector<pair<SimpleRightSide, History>>
{
	bool expanded = false;
	vector<pair<SimpleRightSide, History>> workingRightSides;
	workingRightSides.push_back({ move(rule), move(initBaseHistory) });

    for (auto const& prev : previousNontermins)
    {
        vector<pair<SimpleRightSide, History>> newWorkingRightSides;
        for (auto& [rule, baseHistory] : workingRightSides)
        {
            if (rule.front() != prev)
            {
                newWorkingRightSides.push_back({ move(rule), move(baseHistory) });
                continue;
            }

			// expand items into RightSides
            SimpleRightSide postfix{ move(rule) };
            String frontSymbol{ move(postfix.front()) };
            postfix.erase(postfix.begin());
            for (int i = 0; auto const& rightSide : grammars.at(frontSymbol))
            {
				expanded = true;
                SimpleRightSide copyRightSide{ rightSide };
                // merge all accumulated histories into one
                History history{ move(baseHistory) };
                if (globalReplaceHistory.at(frontSymbol).contains(i))
                {
                    history.Merge(globalReplaceHistory.at(frontSymbol).at(i));
                }
                ++i;
                history.Add(std::make_shared<History::ExpandFront>(frontSymbol, rightSide, copyRightSide));

                copyRightSide.append_range(postfix);
                newWorkingRightSides.push_back({ move(copyRightSide), {move(history)} });
            }
        }
        workingRightSides = move(newWorkingRightSides);
    }

    if (expanded)
    {
        return workingRightSides;
    }
    return {};
}

/// <summary>
/// also handle the direct left recursive
/// </summary>
auto RemoveIndirectLeftRecur(SimpleGrammars grammars) -> ConvertResult
{
    SimpleGrammars generatedGrammars;
    map<String, map<int, History>> convertHistory;
    vector<String> previousNontermins; // order is important, so use vector

    for (auto& focusRule : grammars)
    {
        vector<SimpleRightSide> newRightSides;
        map<int, History>& focusHistory = convertHistory.emplace(focusRule.first, map<int, History>{}).first->second;

        for (auto& rule : focusRule.second)
        {
            if (not rule.empty())
            {
                auto expandedRules = ExpandFrontWithPreviousNontermins(rule, {/* empty replace history */}, grammars, previousNontermins, convertHistory);
                if (not expandedRules.empty())
                {
                    for (auto& [expandedRule, history] : expandedRules)
                    {
						focusHistory.insert({ static_cast<int>(newRightSides.size()), move(history) });
						newRightSides.push_back(move(expandedRule));
                    }
                    continue;
                }
            }

			// default: if not expanded, keep the original rule
			newRightSides.push_back(move(rule));
        }
        focusRule.second = move(newRightSides);

        auto converted = DirectLeftRecur2RightRecur(focusRule);
        // add direct left recursive conversion into replace history
        if (converted.has_value())
        {
            using std::make_shared;
			History::Left2RightRecur::Context ctx
            {
                .OriginalGrammar = { focusRule.first, move(focusRule.second) },
                .AuxiliaryGrammar = converted->Auxiliary,
                .OriginalFrontReplaceHistory = move(focusHistory),
            };
			History newHistory;
			newHistory.Add(std::make_shared<History::Left2RightRecur>(ctx));
            focusRule.second.clear();
            focusHistory.clear();

            focusRule.second.reserve(converted->Original.second.size());
            for (auto& rs : converted->Original.second)
            {
				focusHistory.insert({ static_cast<int>(focusRule.second.size()), newHistory });
			    focusRule.second.push_back(move(rs));
            }
            generatedGrammars.insert(move(converted->Auxiliary));
        }

		previousNontermins.push_back(focusRule.first);
    }
	grammars.insert_range(move(generatedGrammars));
    return { .ConvertedGrammars = move(grammars), .ConvertHistory = move(convertHistory) };
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
    vector<size_t> toRemoves;
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

        for (auto i : ids | drop(1))
        {
            auto& rs = grammar.second[i];
            rs.erase(rs.begin());
            suffixGrammar.second.push_back(move(rs));
            toRemoves.push_back(i);
        }
        auto [newSuffixGrammar, addedGrammars] = LeftFactor(move(suffixGrammar));
        newGrammars.push_back(move(newSuffixGrammar));
        if (addedGrammars.has_value())
        {
            newGrammars.append_range(move(addedGrammars.value()));
        }
    }
    
    std::sort(toRemoves.begin(), toRemoves.end(), std::greater<size_t>());
    for (auto i : toRemoves)
    {
        grammar.second.erase(grammar.second.begin() + i);
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


// precondition: this grammar has conflict start of rules, not left-recursive grammar

/// <param name="conflicts">(Nonterminal, Terminal) to conflict rule indexes</param>
auto DeepLeftFactor(map<pair<String, String>, vector<int>> conflicts, map<LeftSide, vector<SimpleRightSide>> grammars) -> map<LeftSide, vector<SimpleRightSide>>
{
    using std::ranges::views::drop;
    using std::ranges::reverse;

    map<String, vector<int>> toRemoves;
    map<String, vector<SimpleRightSide>> toAdds;
    for (auto const& conflict : conflicts)
    {
        // only need to focusRule on the initial rules
        vector<SimpleRightSide> reversedIssueRules; // add expand history progress here
        for (auto const i : conflict.second)
        {
            auto rule = grammars.at(conflict.first.first).at(i); // make a copy to isolate change, TODO maybe not need
            toRemoves[conflict.first.first].push_back(i);
            reverse(rule);
            reversedIssueRules.push_back(move(rule)); // only part of rules, so we cannot assign it back directly TODO
        }

        set<pair<String, size_t>> expandHistory;
        for (auto changed = true; changed;)
        {
            changed = false;
            for (size_t i = 0, size = reversedIssueRules.size(); i < size; ++i)
            {
                // handle the left recursive item
                auto& rule = reversedIssueRules.at(i);
                if (rule.empty())
                {
                    continue;
                }
                if (auto symbol = rule.back(); symbol != conflict.first.second and grammars.contains(symbol) and not expandHistory.contains({ symbol, static_cast<int>(i) }))// when grammars is map, we don't need get NonTermins manually TODO
                {
                    changed = true;
                    expandHistory.insert({ symbol, i });

                    rule.pop_back();

                    for (auto rule : grammars.at(symbol) | drop(1))
                    {
                        reversedIssueRules.push_back(rule); // may change the valid of reference rule
                        reverse(rule);
                        reversedIssueRules.back().append_range(move(rule));
                    }

                    auto firstRule = grammars.at(symbol).front();
                    reverse(firstRule);
                    reversedIssueRules.at(i).append_range(move(firstRule));
                }
            }
        }

        for (auto& x : reversedIssueRules)
        {
            reverse(x);
        }

        auto [newG, addGrammars] = LeftFactor({ conflict.first.first, move(reversedIssueRules) });
        // Left Factor will make the reduce the rules, how to make it not affect other rules index. possible some rule here is left recursive
        toAdds[newG.first] = move(newG.second);
        if (addGrammars.has_value())
        {
            toAdds.insert_range(move(addGrammars.value()));
        }
    }

    for (auto& x : toRemoves)
    {
        std::ranges::sort(x.second, std::ranges::greater());
        for (auto i : x.second)
        {
            auto& rules = grammars.at(x.first);
            rules.erase(rules.begin() + i);
        }
    }
    for (auto& x : toAdds)
    {
        grammars[x.first].append_range(move(x.second));
    }

    return grammars;
}

/// <summary>
/// pass confirmed left-recursive node. invoke the callback inner
/// </summary>
//template <typename Tok, typename Result>
//auto ConvertRightRecur2LeftRecur(SyntaxTreeNode<Tok, Result> node, auto&& callback) -> SyntaxTreeNode<Tok, Result>
//{
//    auto SplitOutLastChild = [](SyntaxTreeNode<Tok, Result>& node) -> SyntaxTreeNode<Tok, Result>
//    {
//        node.ChildSymbols.pop_back();
//        auto lastChild = move(node.Children.back());
//        node.Children.pop_back();
//        return move(std::get<SyntaxTreeNode<Tok, Result>>(lastChild));
//    };
//    auto Iter = [leftRecurName = node.Name, &callback, &SplitOutLastChild](this auto&& self, SyntaxTreeNode<Tok, Result> node, SyntaxTreeNode<Tok, Result> toAddChild) -> SyntaxTreeNode<Tok, Result>
//    {
//        auto InsertAtHead = [&callback](SyntaxTreeNode<Tok, Result>& node, SyntaxTreeNode<Tok, Result> toAddChild)
//        {
//            node.ChildSymbols.insert(node.ChildSymbols.begin(), toAddChild.Name);
//            node.Children.insert(node.Children.begin(), move(toAddChild));
//            callback(&node);
//        };
//        // node.Children might be empty?
//        // make sure it's the right side which is after remove direct left recursive
//        if ((not node.ChildSymbols.empty()) and node.ChildSymbols.back().StartWith(node.Name) and node.ChildSymbols.back().EndWith(rightRecurSuffix))// might end with multiple times rightRecurSuffix
//        {
//            // restrict the node->Name to right recursive symbol name
//            node.Name = self.leftRecurName; // all names change correct?
//            auto lastChild = SplitOutLastChild(node);
//            InsertAtHead(node, move(toAddChild));
//            return self(move(lastChild), move(node));
//        }
//        else
//        {
//            InsertAtHead(node, move(toAddChild));
//            return node;
//        }
//    };
//    // pass a reconstruct info structure as an argument
//    if (not node.ChildSymbols.empty()) // Do we need this when A -> A a | empty? Iter has empty judge
//    {
//        auto lastChild = SplitOutLastChild(node);
//        callback(&node);
//        return Iter(move(lastChild), move(node));
//    }
//    callback(&node);
//    return node;
//}
//

//
///// <summary>
///// Focus on recovering the replacement of front nonterminal item for a nonterminal grammar.
///// The recover of right recursive item is @ConvertRightRecur2LeftRecur's work.
///// </summary>
//template <typename Tok, typename Result>
//auto RecoverSyntaxNode(SyntaxTreeNode<Tok, Result> node, SimpleGrammars const& currentGrammars, map<String, map<int, ConvertResult::HistoryItem>> const& replaceHistory) -> SyntaxTreeNode<Tok, Result>
//{
//	if (replaceHistory.contains(node.Name))
//	{
//		auto const& history = replaceHistory.at(node.Name);
//		auto ruleIndex = Find(currentGrammars.at(node.Name), node.ChildSymbols);
//		if (not ruleIndex.has_value())
//		{
//			throw logic_error(format("cannot find the rule({}) for node {} in current grammars", node.ChildSymbols, node.Name));
//		}
//		if (not history.contains(ruleIndex.value()))
//		{
//			return node; // rule not expanded, no recovery needed
//		}
//		auto& frontReplaceHistory = history.at(ruleIndex.value()).FrontReplaceHistory;
//
//		for (auto const& [nonterminal, expandRule] : std::ranges::views::reverse(frontReplaceHistory))
//		{
//            Assert(std::ranges::starts_with(node.ChildSymbols, expandRule), "RecoverSyntaxNode: node.ChildSymbols does not start with expandRule");
//			auto packedChild = node.PackOutChildrenAsNode(static_cast<int>(expandRule.size()), nonterminal);
//			node.Children.insert(node.Children.begin(), move(packedChild));
//			node.ChildSymbols.insert(node.ChildSymbols.begin(), nonterminal);
//		}
//	}
//	return node;
//}

export
{
    auto LeftFactor(SimpleGrammar grammar) -> pair<SimpleGrammar, optional<vector<SimpleGrammar>>>;
    //auto DeepLeftFactor(map<pair<String, String>, vector<int>> conflicts, map<LeftSide, vector<SimpleRightSide>> grammars) -> map<LeftSide, vector<SimpleRightSide>>;
    auto RemoveIndirectLeftRecur(SimpleGrammars grammars) -> ConvertResult;
}