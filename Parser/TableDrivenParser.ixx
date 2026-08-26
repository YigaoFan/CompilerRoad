export module Parser:TableDrivenParser;

import std;
import Base;
import Generator;
import :ParserBase;
import :GrammarSet;
import :GrammarProcess;
import :InputStream;
import :GrammarUnitLoader;
import :Terminal;
import :HtmlLogger;

using std::vector;
using std::map;
using std::string_view;
using std::size_t;
using std::pair;
using std::logic_error;
using std::stack;
using std::set;
using std::variant;
using std::function;
using std::optional;
using std::unexpected;
using std::expected;
using std::bool_constant;
using std::tuple;
using std::move;
using std::format;
using std::println;
using std::ranges::views::reverse;
using std::ranges::views::drop;
using std::ranges::views::transform;
using std::ranges::to;

template <typename T, typename Tok, typename Result>
concept INodeCallback = requires (T callback, SyntaxTreeNode<Tok, Result>* node)
{
    { callback(node) } -> std::same_as<void>;
};

struct Symbol
{
    String Value;
    Symbol(String symbol) : Value(move(symbol))
    {
    }

    auto IsEof() const -> bool
    {
        return Value == eof;
    }
};

struct Void
{
};

export template <typename T>
struct OptionalArg : std::true_type
{
    T Value;
    constexpr OptionalArg(T value) : Value(move(value))
    {
    }
};

export template <>
struct OptionalArg<Void> : std::false_type
{
    constexpr OptionalArg(Void)
    {
    }
};

template <typename Tok, typename Result>
auto TryRemoveChildrenCausedByLeftFactor(SyntaxTreeNode<Tok, Result>* node) -> void
{
    vector<String> symbols;
    vector<variant<Tok, SyntaxTreeNode<Tok, Result>>> children;
    auto expanded = false;
    for (size_t i = 0; i < node->ChildSymbols.size(); ++i)
    {
        if (node->ChildSymbols[i].StartWith(node->Name) and node->ChildSymbols[i].EndWith(leftFactorSuffix))
        {
            expanded = true;
            SyntaxTreeNode<Tok, Result>& n = std::get<1>(node->Children[i]);
            symbols.append_range(move(n.ChildSymbols));
            move(n.Children.begin(), n.Children.end(), std::back_inserter(children));
            //children.append_range(move(n.Children)); why this trigger copy constructor which affect performance
        }
        else
        {
            symbols.push_back(move(node->ChildSymbols[i]));
            children.push_back(move(node->Children[i]));
        }
    }
    node->ChildSymbols = move(symbols);
    node->Children = move(children);

    if (expanded)
    {
        return TryRemoveChildrenCausedByLeftFactor(node);
    }
}

class LLParser
{
private:
    String const startSymbol;
    ConvertedGrammars const convertedGrammars;
    map<pair<String, int>, vector<int>> const parseTable;
    map<string_view, int> const terminal2IntTokenType;

	mutable HtmlLogger logger;
public:
    // how to distinguish nonterminal and terminal(which has enum type from Lexer) in grammar
    // do we need convert nonterminal and terminal to int to make program litter faster
    /// <summary>
    /// attention: make string_view in terminal2IntTokenType is alive when parse
    /// </summary>
	template <typename Arg0 = Void, typename Arg1 = Void>
    static auto ConstructFrom(String startSymbol, SimpleGrammars grammars, map<string_view, int> terminal2IntTokenType, Arg0&& conflictResolver = Void{}, Arg1&& externalParser = Void{}) -> LLParser
    {
        auto optionalConflictResolver = OptionalArg(std::forward<Arg0>(conflictResolver));
        auto optionalExternalParser = OptionalArg(std::forward<Arg1>(externalParser));

        // First remove left recursion, then left-factor (so newly created ' rules are also factored)
        auto convertedGrammars = RemoveIndirectLeftRecur(move(grammars));

        vector<SimpleGrammar> newAddGrammars;
        for (auto& g : convertedGrammars.Grammars)
        {
            auto [newG, addGrammars] = LeftFactor(move(g));
            g.second = move(newG.second);
            if (addGrammars.has_value())
            {
                newAddGrammars.append_range(move(addGrammars.value()));
            }
        }
        convertedGrammars.Grammars.insert_range(move(newAddGrammars));

        map<pair<String, int>, vector<int>> parseTable;
        
        map<String, set<String>> externalFirstSet;
        if constexpr (optionalExternalParser)
        {
            externalFirstSet = optionalExternalParser.Value.FirstSet();
        }
        auto grammarSet = Starts(startSymbol, convertedGrammars.Grammars, move(externalFirstSet));
        auto const& grammarsWithStartSet = grammarSet.GrammarsWithStartSet;

        // handle e-production, focus <- pop() TODO
        for (auto const& g : grammarsWithStartSet)
        {
            auto const& nontermin = g.first;
            auto const& rulesWithStart = g.second;

            for (auto j = 0; auto const& r : rulesWithStart)
            {
                for (Terminal const& termin : r.second)
                {
                    if (not terminal2IntTokenType.contains(static_cast<String>(termin)))
                    {
                        throw logic_error(format("terminal2IntTokenType not contain termin item: {}", termin));
                    }
                    auto tokType = terminal2IntTokenType.at(static_cast<String>(termin));
                    auto key = pair{ nontermin, tokType };
                    if (parseTable.contains(key) and not parseTable.at(key).empty())
                    {
						auto& otherJs = parseTable.at(key);
                        otherJs.push_back(j);

                        // 0: termin.Sources(), count x, 1: termin.Sources(), count y, check x*y possiblities as below
                       /* if (resolvedConflicts.contains({ nontermin, otherJ, j }))
                        {
                            continue;
						}*/
                        if constexpr (optionalConflictResolver)
                        {
                            if (optionalConflictResolver.Value.Resolvable(nontermin, tokType))
                            {
                                /*resolvedConflicts.insert({ nontermin, otherJ, j });
                                resolvedConflicts.insert({ nontermin, j, otherJ });*/
                                continue;
                            }
							/*auto const& otherTermin = rulesWithStart.at(otherJ).second.At(termin);
                            // TODO below check condition is not complete, rethink the conflict rules
                            if (termin.Sources() == otherTermin.Sources())
                            {
                                auto const& resolvedConflicts = optionalConflictResolver.Value.ResolvedConflicts();
                                for (auto const& s : termin.Sources())
                                {
                                    if (not resolvedConflicts.contains({ s.Nontermin, static_cast<std::remove_reference_t<decltype(resolvedConflicts)>::key_type::second_type>(tokType)}))
                                    {
                                        std::println("high level {} rule conflict not resolved: {} and {}", nontermin, termin, otherTermin);
                                        goto WarnConflict;
                                    }
                                }
                                std::println("high level {} rule conflict resolved: {} and {}", nontermin, termin, otherTermin);
                                continue;
                            }*/
                        }
                    //WarnConflict:
                        auto rulesPrint = otherJs
                            | transform([&rulesWithStart](int j) -> String { return String(format("\n  {}", rulesWithStart[j].first)); })
							| to<vector>();
						throw logic_error(format("grammar isn't LL(1), {{{}, {}}} point to multiple grammar: {}", nontermin, static_cast<String>(termin), rulesPrint));
                    }
                    else
                    {
                        //println("when come {}, move to {} -> {}", key, grammars[i].first, grammars[i].second[j]);
                        parseTable[move(key)].push_back(j);
                    }
                }
                ++j;
            }
        }

        //std::println("parse table: {}", parseTable);
        auto logger = HtmlLogger::NewFromCurrentTime();
        return LLParser(move(startSymbol), move(convertedGrammars), move(parseTable), move(terminal2IntTokenType), move(logger));
    }

    LLParser(String startSymbol, ConvertedGrammars grammars, map<pair<String, int>, vector<int>> parseTable, map<string_view, int> terminal2IntTokenType, HtmlLogger logger)
		: startSymbol(move(startSymbol)), convertedGrammars(move(grammars)), parseTable(move(parseTable)), terminal2IntTokenType(move(terminal2IntTokenType)), logger(move(logger))
    { }

    LLParser(LLParser const&) = delete;
    auto operator= (LLParser const&) -> LLParser = delete;
    LLParser(LLParser&&) = default;
    auto operator= (LLParser&& that) -> LLParser& = default;

    template <typename Result, template <typename> class ActualStream, IToken Tok, typename Callback, typename Arg0 = Void, typename Arg1 = Void>
		requires Stream<ActualStream, Tok>
            and INodeCallback<Callback, Tok, Result>
	        and (std::is_same_v<Arg0, Void> or ICustomParser<Arg0, ActualStream, Tok, Result>)
	        and (std::is_same_v<Arg1, Void> or IConflictResolve<Arg1, ActualStream, Tok, Result>)
    auto Parse(ActualStream<Tok> stream, Callback callback, set<int> const& ignorableTokenTypes = {}, Arg1&& conflictResolver = Void{},
        Arg0&& externalParser = Void{}) const
        -> ParserResult<SyntaxTreeNode<Tok, Result>>
    {
        using std::ranges::views::reverse;
        using std::unexpected;

        logger.LogCode(stream);

        /// <summary>
        /// Only work for terminal symbol or eof
        /// </summary>
        auto MatchTerminal = [this](Symbol const& symbol, Tok const& token) -> bool
        {
            if (auto dest = terminal2IntTokenType.at(symbol.Value); dest == static_cast<int>(token.Type))
            {
                return true;
            }
            return symbol.Value == token.Value;
        };
        auto IsTerminal = [this](Symbol const& t) { return terminal2IntTokenType.contains(t.Value); };
        stack<Symbol> symbolStack;
        symbolStack.push(String(eof));
        symbolStack.push(startSymbol);
        auto word = stream.Current();
        SyntaxTreeNode<Tok, Result> root{ "root", { startSymbol } }; // TODO why "root" is shown as "???" in VS debugger
        stack<SyntaxTreeNode<Tok, Result>*> workingNodes;
        workingNodes.push(&root);
        auto PopAllFilledNodes = [&workingNodes, &callback, &stream, this]()
        {
            while (not workingNodes.empty())
            {   
                if (auto working = workingNodes.top(); working->Children.size() == working->ChildSymbols.size())
                {
                    if (convertedGrammars.ConvertHistory.contains(working->Name))
                    {
                        auto const& history = convertedGrammars.ConvertHistory.at(working->Name);
                        if (not history.empty())
                        {
                            for (auto i = 0; auto const& rule : convertedGrammars.Grammars.at(working->Name))
                            {
                                if (rule == working->ChildSymbols)
                                {
                                    if (history.contains(i))
                                    {
								        *working = history.at(i).Undo(move(*working));
                                    }
								    break;
                                }
                                ++i;
                            }
                        }
					}
                    TryRemoveChildrenCausedByLeftFactor(working);
                    if (not (working->Name.EndWith(leftFactorSuffix) or working->Name.EndWith(rightRecurSuffix)))
                    {
                        callback(working);
                    }
                    logger.Log(Level::Out, "parse done for node: {}, current stream position: {}", working->Name, stream.CurrentPosition());
                    workingNodes.pop();
                }
                else
                {
                    break;
                }
            }
        };
        auto DoWhenGotChild = [&]<bool IsFulfilledChild, bool ConsumedWord>(variant<Tok, SyntaxTreeNode<Tok, Result>> child, bool_constant<IsFulfilledChild>, bool_constant<ConsumedWord>)
        {
			symbolStack.pop();
            workingNodes.top()->Children.push_back(move(child));
            if constexpr (not IsFulfilledChild)
            {
                // if not fulfilled, we should push it workingNodes to continue working on this node
                workingNodes.push(&std::get<SyntaxTreeNode<Tok, Result>>(workingNodes.top()->Children.back()));
            }
            PopAllFilledNodes();
            if constexpr (ConsumedWord) // if consumed word in parse this child's progress, we should update it
            {
                stream.MoveNext();
                word = stream.Current();
                logger.Log(Level::Here, "stream move forward, got: {}", word);
            }
        };
        auto ExpandRule = [&symbolStack, &DoWhenGotChild, this](Symbol const& focus, Tok const& tok, int expandRuleIndex, string_view reason)
        {
            auto const& rule = convertedGrammars.Grammars.at(focus.Value).at(expandRuleIndex);
            logger.Log(Level::In, "{} expand ({}, {}) with rule{}: {}", reason, focus.Value, tok.Value, expandRuleIndex, rule);
            DoWhenGotChild(SyntaxTreeNode<Tok, Result>{ focus.Value, rule }, bool_constant<false>{}, bool_constant<false>{});
            if (not rule.empty())
            {
                for (auto const& b : reverse(rule))
                {
                    symbolStack.push(b);
                }
            }
		};
        while (true)
        {
            auto const& focus = symbolStack.top();

            if (focus.IsEof() and MatchTerminal(focus, word))
            {
                return root;
            }
            else if (IsTerminal(focus) or focus.IsEof())
            {
                if (MatchTerminal(focus, word))
                {
					DoWhenGotChild(move(word), bool_constant<true>{}, bool_constant<true>{});
                }
                else if (ignorableTokenTypes.contains(static_cast<int>(word.Type)))
                {
                    stream.MoveNext();
                    word = stream.Current();
                    logger.Log(Level::Here, "ignore a token, stream move forward, got: {}", word);
                }
                else
                {
					logger.Log(Level::Out, "cannot found token for terminal symbol({}) when parse, stream position at {}", focus.Value, stream.CurrentPosition());
                    return unexpected(ParseFailResult{ .Message = format("cannot found token for terminal symbol({}) when parse", focus.Value) });
                }
            }
            else
            {
				if (auto dest = pair{ focus.Value, static_cast<int>(word.Type) }; parseTable.contains(dest))
				{
					if (parseTable.at(dest).size() == 1)
					{
						auto expandRuleIndex = parseTable.at(dest).front();
						ExpandRule(focus, word, expandRuleIndex, "parse table");
                        continue;
					}
					else if constexpr (auto optionalArg = OptionalArg(std::forward<Arg1>(conflictResolver)); optionalArg)
						if (optionalArg.Value.Resolvable(focus.Value, static_cast<int>(word.Type)))
						{
							auto streamPos = stream.CurrentPosition();
							auto options = parseTable.at(dest)
								| transform([&](int j) -> SimpleRightSide { return convertedGrammars.Grammars.at(focus.Value).at(j); })
								| to<vector>();
							auto selectResult = optionalArg.Value.template Resolve<ActualStream, Tok>(workingNodes, focus.Value, word.Type, options, stream);
							if (selectResult.has_value())
							{
								stream.RollbackTo(streamPos);
								ExpandRule(focus, word, parseTable.at(dest)[selectResult.value()], format("conflict resolver resolve {}", options));
                                continue;
							}
							else
							{
                                logger.Log(Level::Out, "resolve conflict failed when parse (nonterminal symbol: {}, word: {}) in options {}: {}", focus.Value, word, options, selectResult.error().Message);
								return unexpected(ParseFailResult{ .Message = format("resolve conflict failed when parse (nonterminal symbol: {}, word: {})", focus.Value, word) });
							}
						}
				}
				else if constexpr (auto optionalArg = OptionalArg(std::forward<Arg0>(externalParser)); optionalArg)
				{
					if (optionalArg.Value.Parsable(focus.Value))
					{
                        // let the stream position on the last consumed token
						auto r = optionalArg.Value.Parse<Result>(focus.Value, stream, logger);
						if (not r.has_value())
						{
							logger.Log(Level::Out, "use external parser parse ({}, {}) failed: {}", focus.Value, word, r.error().Message);
							return unexpected(ParseFailResult{ .Message = format("use external parser parse ({}, {}) failed: {}", focus.Value, word, r.error().Message) });
						}
                        if (r.value().ChildSymbols.size() != r.value().Children.size())
                        {
                            // warning
                        }
                        logger.Log(Level::Here, "use external parser parse ({}, {}) done", focus.Value, word);
						DoWhenGotChild(move(r.value()), bool_constant<true>{}, bool_constant<true>{});
						continue;
					}
				}
				if (ignorableTokenTypes.contains(static_cast<int>(word.Type)))
				{
					stream.MoveNext();
					word = stream.Current();
                    logger.Log(Level::Here, "ignore a token, stream move forward, got: {}", word);
					continue;
				}

				logger.Log(Level::Out, "cannot expand (nonterminal symbol: {}, word: {}) when parse", focus.Value, word);
				return unexpected(ParseFailResult{ .Message = format("cannot expand (nonterminal symbol: {}, word: {}) when parse", focus.Value, word) });
            }
        }
    }
};

class GLLParser
{
private:
    friend struct UnitParser;
    String const startSymbol;
    SimpleGrammars const grammars;
    map<pair<String, int>, vector<int>> const parseTable;
    map<string_view, int> const terminal2IntTokenType;
    set<int> const ignorableTokenTypes;
    map<int, set<int>> const replaceableTokenTypes;
public:
    /// <summary>
    /// attention: make string_view in terminal2IntTokenType is alive when parse
    /// </summary>
    static auto ConstructFrom(String startSymbol, SimpleGrammars grammars, map<string_view, int> terminal2IntTokenType, set<int> ignorableTokenTypes = {}, map<int, set<int>> replaceableTokenTypes = {}) -> GLLParser
    {
        vector<SimpleGrammar> newAddGrammars;
        for (auto& g : grammars)
        {
            auto [newG, addGrammars] = LeftFactor(move(g));
            g.second = move(newG.second);
            if (addGrammars.has_value())
            {
                newAddGrammars.append_range(move(addGrammars.value()));
            }
        }
        grammars.insert_range(move(newAddGrammars));
        //std::println("after left refactor: {}", grammars);
        map<pair<String, int>, vector<int>> parseTable;
        auto grammarSet = Starts(startSymbol, grammars); // string_view here is from grammars
        auto const& grammarsWithStartSet = grammarSet.GrammarsWithStartSet;

        // handle e-production, focus <- pop() TODO
        for (auto const& g : grammarsWithStartSet)
        {
            auto const& nontermin = g.first;
            auto const& rulesWithStart = g.second;
            for (auto j = 0; auto const& r : rulesWithStart)
            {
                for (String const& termin : r.second)
                {
                    if (not terminal2IntTokenType.contains(termin))
                    {
                        throw std::out_of_range(format("terminal2IntTokenType not include token type for {}", termin));
                    }
                    auto tokenType = terminal2IntTokenType.at(termin);
                    auto key = pair{ nontermin, tokenType };
                    if (parseTable.contains(key))
                    {
                        parseTable.at(key).push_back(j);
                    }
                    else
                    {
                        //println("when come {}, move to {} -> {}", key, grammars[i].first, grammars[i].second[j]);
                        parseTable.insert({ move(key), { j } });
                    }
                }
                ++j;
            }
        }
        //std::println("parse table: {}", parseTable);
        return GLLParser(move(startSymbol), move(grammars), move(parseTable), move(terminal2IntTokenType), move(ignorableTokenTypes), move(replaceableTokenTypes));
    }

    GLLParser(String startSymbol, SimpleGrammars grammars, map<pair<String, int>, vector<int>> parseTable, map<string_view, int> terminal2IntTokenType, set<int> ignorableTokenTypes, map<int, set<int>> replaceableTokenTypes)
        : startSymbol(move(startSymbol)), grammars(move(grammars)), parseTable(move(parseTable)), terminal2IntTokenType(move(terminal2IntTokenType)), 
        ignorableTokenTypes(move(ignorableTokenTypes)), replaceableTokenTypes(move(replaceableTokenTypes))
    {
    }

    GLLParser(GLLParser const&) = delete;
    auto operator= (GLLParser const&) -> GLLParser = delete;
    GLLParser(GLLParser&&) = default;
    auto operator= (GLLParser&& that) -> GLLParser& = default;

    template <typename Result, template <typename> class ActualStream, IToken Tok>
        requires Stream<ActualStream, Tok>
    auto Parse(ActualStream<Tok> stream, INodeCallback<Tok, Result> auto callback, map<String, function<ParserResult<SyntaxTreeNode<Tok, Result>>(decltype(stream)&)>> externalParsers = {}) const
        -> ParserResult<SyntaxTreeNode<Tok, Result>>
    {
        stack<Symbol> symbolStack;
        symbolStack.push(String(eof));
        symbolStack.push(startSymbol);
        SyntaxTreeNode<Tok, Result> root{ "root", { startSymbol } }; // TODO why "root" is shown as "???" in VS debugger
        //map<tuple<String, int, size_t>, int> tryMemory;
        // add a list to store the parse path
        //stack<pair<tuple<String, int, size_t>, int>> parsePath; // focus, token type, token index in stream

        return ParseWith<Tok, Result>(move(root), move(symbolStack), stream, callback, externalParsers);
    }

private:
    template <typename Result, template <typename> class ActualStream, IToken Tok>
        requires Stream<ActualStream, Tok>
    auto ParseWith(SyntaxTreeNode<Tok, Result> root, stack<Symbol> initSymbolStack, ActualStream<Tok>& stream, INodeCallback<Tok, Result> auto const& callback, map<String, function<ParserResult<SyntaxTreeNode<Tok, Result>>(decltype(stream)&)>> const& externalParsers) const
        -> ParserResult<SyntaxTreeNode<Tok, Result>>
    {
        struct SubPartResult
        {
            vector<SyntaxTreeNode<Tok, Result>> Nodes;
        };
        struct ParseTarget
        {
            int ParentId;
            String StartSymbol;
            SimpleRightSide StartRule;
        };

        struct UnitParser // use to store then recover parse
        {
            SyntaxTreeNode<Tok, Result> Root; // attention: move will make this root empty which maybe used in future retry
            decltype(stream)& TokStream; // attention: attention when retry parse
            stack<Symbol> SymbolStack;

            decltype(callback)& Callback;
            map<String, function<ParserResult<SyntaxTreeNode<Tok, Result>>(decltype(TokStream)&)>> const& ExternalParsers;
            GLLParser const* TopParser;

            auto ParseWithContinuation(vector<ParseTarget> continuations) -> ParserResult<SubPartResult>
            {
                stack<SyntaxTreeNode<Tok, Result>*> workingNodes;
                workingNodes.push(&Root);

                auto word = TokStream.Current();
                auto PopAllFilledNodes = [this, &workingNodes]()
                {
                    while (not workingNodes.empty())
                    {
                        if (auto working = workingNodes.top(); working->Children.size() == working->ChildSymbols.size()) // here may have issue
                        {
                            TryRemoveChildrenCausedByLeftFactor(working);
                            if (not (working->Name.EndWith(leftFactorSuffix) or working->Name.EndWith(rightRecurSuffix)))
                            {
                                Callback(working);
                            }

                            workingNodes.pop();
                        }
                        else
                        {
                            break;
                        }
                    }
                };
                auto DoWhenGotChild = [&]<bool IsTerminal, bool ContinueUseWord>(variant<Tok, SyntaxTreeNode<Tok, Result>> child, bool_constant<IsTerminal>, bool_constant<ContinueUseWord>)
                {
                    if (not SymbolStack.empty()) // TODO temp condition
                    {
                        // and node's Name is remain
                        SymbolStack.pop();
                    }
                    workingNodes.top()->Children.push_back(move(child));
                    if constexpr (not IsTerminal)
                    {
                        workingNodes.push(&std::get<SyntaxTreeNode<Tok, Result>>(workingNodes.top()->Children.back()));
                    }
                    PopAllFilledNodes();
                    if constexpr (IsTerminal and ContinueUseWord) // if continue use word, we should update it
                    {
                        TokStream.MoveNext();
                        word = TokStream.Current();
                    }
                };
                while (true)
                {
                    if (SymbolStack.empty())
                    {
                        return ContinueWith(move(continuations))
                            .or_else([this](ParseFailResult x) -> ParserResult<SubPartResult>
                            {
                                println("remain parse failed: {}", x.Message);
                                return unexpected(move(x));
                            })
                            .and_then([this](SubPartResult x) -> ParserResult<SubPartResult>
                            {
                                x.Nodes.push_back(move(Root));
                                return move(x);
                            });
                    }
                    auto const focus = SymbolStack.top();
                    //println("focus on {} at token {}", focus.Value, word);

                    if (focus.IsEof() and MatchTerminal(focus, word))
                    {
                        TokStream.MoveNext();
                        Assert(continuations.empty(), "continuations should be empty when encounter EOF");
                        auto subResult = SubPartResult{};
                        subResult.Nodes.push_back(move(Root));
                        return subResult;
                    }
                    else if (ExternalParsers.contains(focus.Value))
                    {
                        auto subResult = ExternalParsers.at(focus.Value)(TokStream);
                        if (not subResult.has_value())
                        {
                            return unexpected(move(subResult.error()));
                        }
                        DoWhenGotChild(move(subResult.value()), bool_constant<true>{}, bool_constant<true>{}); // handle as terminal
                    }
                    else if (IsTerminal(focus) or focus.IsEof())
                    {
                        if (MatchTerminal(focus, word))
                        {
                            DoWhenGotChild(move(word), bool_constant<true>{}, bool_constant<true>{});
                        }
                        else if (TopParser->ignorableTokenTypes.contains(static_cast<int>(word.Type)))
                        {
                            TokStream.MoveNext();
                            word = TokStream.Current();
                        }
                        else
                        {
                            return unexpected(ParseFailResult{ .Message = format("cannot found token for terminal symbol({}) when parse in {}", focus.Value, Root.Name) });
                        }
                    }
                    else
                    {
                        if (auto dest = pair{ focus.Value, static_cast<int>(word.Type) }; TopParser->parseTable.contains(dest))
                        {
                        ExpandRule:
                            auto& js = TopParser->parseTable.at(dest);
                            if (js.size() == 1)
                            {
                                auto const& rule = TopParser->grammars.at(focus.Value).at(js.front());
                                DoWhenGotChild(SyntaxTreeNode<Tok, Result>{ focus.Value, rule, }, bool_constant<false>{}, bool_constant<true>{});

                                if (not rule.empty())
                                {
                                    for (auto const& b : reverse(rule))
                                    {
                                        SymbolStack.push(b);
                                    }
                                }
                            }
                            else
                            {
                                auto pos = TokStream.CurrentPosition();

                                SymbolStack.pop();
                                SimpleRightSide rs;
                                while (not SymbolStack.empty())
                                {
                                    rs.push_back(move(SymbolStack.top().Value));
                                    SymbolStack.pop();
                                }
                                auto hasContinuation = false;
                                auto continuationName = String(format("remain-after-{}-in-{}", focus.Value, Root.Name));
                                if (not rs.empty())
                                {
                                    hasContinuation = true;
                                    continuations.push_back({ .StartSymbol = continuationName, .StartRule = move(rs), });
                                }

                                for (auto i = 1; auto j : js)
                                {
                                    println("start parse with {} option rule of {} options of {} at token {}", i++, js.size(), focus.Value, pos);

                                    auto const& rule = TopParser->grammars.at(focus.Value).at(j);
                                    auto p = Construct(TopParser, focus.Value, rule, TokStream, Callback, ExternalParsers);
                                    auto subResult = p.ParseWithContinuation(continuations); // make continuations copied to isolate change
                                    if (subResult.has_value())
                                    {
                                        return move(subResult).and_then([this, hasContinuation, continuationName, &DoWhenGotChild](SubPartResult x) -> ParserResult<SubPartResult>
                                        {
                                            Assert(not x.Nodes.empty(), "subResult shouldn't be empty");

                                            DoWhenGotChild(move(x.Nodes.back()), bool_constant<true>{}, bool_constant<false>{});
                                            x.Nodes.pop_back();

                                            if (hasContinuation)
                                            {
                                                Assert(x.Nodes.back().Name == continuationName, "node name should be same");
                                                for (auto& i : x.Nodes.back().Children)
                                                {
                                                    DoWhenGotChild(move(i), bool_constant<true>{}, bool_constant<false>{});
                                                }
                                                x.Nodes.pop_back();
                                            }

                                            x.Nodes.push_back(move(Root));
                                            return move(x);
                                        });
                                    }
                                    println("sub parse failed: {}", subResult.error().Message);
                                    TokStream.RollbackTo(pos);
                                }
                                return unexpected(ParseFailResult{ .Message = format("try parse ambiguous (nonterminal: {}, word: {}) failed in {}", focus.Value, word, Root.Name) });
                            }
                        }
                        else if (auto current = static_cast<int>(word.Type); TopParser->replaceableTokenTypes.contains(current))
                        {
                            auto const& replaces = TopParser->replaceableTokenTypes.at(current);
                            for (auto x : replaces)
                            {
                                dest.second = x;
                                if (TopParser->parseTable.contains(dest))
                                {
                                    goto ExpandRule;
                                }
                            }
                            return unexpected(ParseFailResult{ .Message = format("cannot expand (nonterminal symbol: {}, replaceable word: {}) when parse in {}", focus.Value, word, Root.Name) });
                        }
                        else if (TopParser->ignorableTokenTypes.contains(static_cast<int>(word.Type)))
                        {
                            TokStream.MoveNext();
                            word = TokStream.Current();
                        }
                        else
                        {
                            return unexpected(ParseFailResult{ .Message = format("cannot expand (nonterminal symbol: {}, word: {}) when parse in {}", focus.Value, word, Root.Name) });
                        }
                    }
                }
            }

            auto ContinueWith(vector<ParseTarget> continuations) const -> ParserResult<SubPartResult>
            {
                if (continuations.empty())
                {
                    println("parse success");
                    return SubPartResult{};
                }

                auto target = move(continuations.back());
                continuations.pop_back();
                auto parser = Construct(TopParser, target.StartSymbol, move(target.StartRule), TokStream, Callback, ExternalParsers);
                //parsePath.push_back(move(parser)); // ParseWithContinuation will change the back of vector
                return parser.ParseWithContinuation(move(continuations));
            }

            /// <summary>
            /// Only work for terminal symbol or eof
            /// </summary>
            auto MatchTerminal(Symbol const& symbol, Tok const& token) -> bool
            {
                if (symbol.IsEof())
                {
                    if (token.IsEof())
                    {
                        return true;
                    }
                    else
                    {
                        return false;
                    }
                }

                // TODO compare the actual value, like the Keyword include multiple values
                if (auto dest = TopParser->terminal2IntTokenType.at(symbol.Value); dest == static_cast<int>(token.Type))
                {
                    return true;
                }
                else if (auto current = static_cast<int>(token.Type); TopParser->replaceableTokenTypes.contains(current))
                {
                    return TopParser->replaceableTokenTypes.at(current).contains(dest);
                }
                return false;
            }

            auto IsTerminal(Symbol const& t) -> bool
            {
                return not TopParser->grammars.contains(t.Value);
            }

            static auto Construct(GLLParser const* topParser, String startSymbol, SimpleRightSide startRule, decltype(TokStream)& stream, decltype(Callback)& callback, map<String, function<ParserResult<SyntaxTreeNode<Tok, Result>>(decltype(stream)&)>> const& externalParsers)
                -> UnitParser
            {
                stack<Symbol> symbolStack;
                for (auto x : reverse(startRule))
                {
                    symbolStack.push(move(x));
                }
                SyntaxTreeNode<Tok, Result> root{ startSymbol, move(startRule) };

                auto p = UnitParser
                {
                    .Root = move(root),
                    .TokStream = stream,
                    .SymbolStack = move(symbolStack),
                    .Callback = callback,
                    .ExternalParsers = externalParsers,
                    .TopParser = topParser,
                };
                return p;
            }
        };

        vector<ParseTarget> continuations;

        auto p = UnitParser
        {
            .Root = move(root),
            .TokStream = stream,
            .SymbolStack = move(initSymbolStack),
            .Callback = callback,
            .ExternalParsers = externalParsers,
            .TopParser = this, 
        };
        auto r = p.ParseWithContinuation(continuations);
        return move(r).and_then([](SubPartResult x) -> ParserResult<SyntaxTreeNode<Tok, Result>>
        {
            Assert(x.Nodes.size() == 1, "nodes size should be only 1");
            return move(x.Nodes.back());
        });
    }
};

export
{
    class LLParser;
    class GLLParser;
}