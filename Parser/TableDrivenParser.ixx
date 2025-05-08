export module Parser:TableDrivenParser;

import std;
import Base;
import Generator;
import :ParserBase;
import :GrammarSet;
import :GrammarPreProcess;
import :InputStream;

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
using std::bool_constant;
using std::tuple;
using std::move;
using std::format;
using std::println;
using std::ranges::views::reverse;
using std::ranges::views::drop;

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

class LLParser
{
private:
    String const startSymbol;
    SimpleGrammars const grammars;
    map<pair<String, int>, int> const parseTable;
    map<string_view, int> const terminal2IntTokenType;
public:
    // how to distinguish nonterminal and terminal(which has enum type from Lexer) in grammar
    // do we need convert nonterminal and terminal to int to make program litter faster
    /// <summary>
    /// attention: make string_view in terminal2IntTokenType is alive when parse
    /// </summary>
    static auto ConstructFrom(String startSymbol, SimpleGrammars grammars, map<string_view, int> terminal2IntTokenType, set<string_view> bypassConflictSymbols = {}) -> LLParser
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
        map<pair<String, int>, int> parseTable;
        //grammars = RemoveIndirectLeftRecur(startSymbol, move(grammars));
        auto starts = Starts(startSymbol, grammars); // string_view here is from grammars
        //std::println("after remove left recur grammar: {}", grammars);

        // handle e-production, focus <- pop() TODO
        for (auto i = 0; auto const& g : grammars)
        {
            auto const& nontermin = g.first;
            auto const& start = starts.at(i);
            for (auto j = 0; j < start.size(); ++j)
            {
                for (auto const& termin : start.at(j))
                {
                    if (not terminal2IntTokenType.contains(termin))
                    {
                        throw std::out_of_range(format("terminal2IntTokenType not include token type for {}", termin));
                    }
                    auto tokenType = terminal2IntTokenType.at(termin);
                    auto key = pair{ nontermin, tokenType };
                    if (parseTable.contains(key))
                    {
                        if (bypassConflictSymbols.contains(nontermin))
                        {
                            continue;
                        }
                        auto otherJ = parseTable.at(key);
                        throw logic_error(format("grammar isn't LL(1), {{{}, {}}} point to multiple grammar: {}, {}", nontermin, termin, parseTable[{ nontermin, tokenType }], j));
                    }
                    else
                    {
                        //println("when come {}, move to {} -> {}", key, grammars[i].first, grammars[i].second[j]);
                        parseTable.insert({ move(key), j });
                    }
                }
            }
            ++i;
        }
        //std::println("parse table: {}", parseTable);
        return LLParser(move(startSymbol), move(grammars), move(parseTable), move(terminal2IntTokenType));
    }

    LLParser(String startSymbol, SimpleGrammars grammars, map<pair<String, int>, int> parseTable, map<string_view, int> terminal2IntTokenType)
        : startSymbol(move(startSymbol)), grammars(move(grammars)), parseTable(move(parseTable)), terminal2IntTokenType(move(terminal2IntTokenType))
    { }

    LLParser(LLParser const&) = delete;
    auto operator= (LLParser const&) -> LLParser = delete;
    LLParser(LLParser&&) = default;
    auto operator= (LLParser&& that) -> LLParser& = default;

    template <IToken Tok, typename Result>
    auto Parse(Stream<Tok> auto stream, INodeCallback<Tok, Result> auto callback, set<int> ignorableTokenTypes = {}, map<int, set<int>> replaceableTokenTypes = {}, map<String, function<ParserResult<SyntaxTreeNode<Tok, Result>>(decltype(stream)&)>> externalParsers = {}) const
        -> ParserResult<SyntaxTreeNode<Tok, Result>>
    {
        using std::ranges::to;
        using std::ranges::views::reverse;
        using std::unexpected;

        /// <summary>
        /// Only work for terminal symbol or eof
        /// </summary>
        auto MatchTerminal = [this, &replaceableTokenTypes](Symbol const& symbol, Tok const& token) -> bool
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
            if (auto dest = terminal2IntTokenType.at(symbol.Value); dest == static_cast<int>(token.Type))
            {
                return true;
            }
            else if (auto current = static_cast<int>(token.Type); replaceableTokenTypes.contains(current))
            {
                return replaceableTokenTypes.at(current).contains(dest);
            }
            return false;
        };
        auto IsTerminal = [this](Symbol const& t) { return not grammars.contains(t.Value); };
        stack<Symbol> symbolStack;
        symbolStack.push(String(eof));
        symbolStack.push(startSymbol);
        auto word = stream.NextItem();
        SyntaxTreeNode<Tok, Result> root{ "root", { startSymbol } }; // TODO why "root" is shown as "???" in VS debugger
        stack<SyntaxTreeNode<Tok, Result>*> workingNodes;
        workingNodes.push(&root);
        auto PopAllFilledNodes = [&workingNodes, &callback]()
        {
            while (not workingNodes.empty())
            {   
                if (auto working = workingNodes.top(); working->Children.size() == working->ChildSymbols.size())
                {
                    TryRemoveChildrenCausedByLeftFactor(working);
                    if (not (working->Name.EndWith(leftFactorSuffix) or working->Name.EndWith(rightRecurSuffix)))
                    {
                        callback(working);
                    }

                    workingNodes.pop();
                }
                else
                {
                    break;
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
            else if (externalParsers.contains(focus.Value))
            {
                stream.Rollback(); // return the current word, to let external parser read it
                auto subResult = externalParsers.at(focus.Value)(stream);
                if (not subResult.has_value())
                {
                    return subResult;
                }
                // almost same as handle terminal
                symbolStack.pop();
                workingNodes.top()->Children.push_back(move(subResult.value()));
                PopAllFilledNodes();
                word = stream.NextItem();
            }
            else if (IsTerminal(focus) or focus.IsEof())
            {
                if (MatchTerminal(focus, word))
                {
                    symbolStack.pop();
                    // combine focus info and word info into SyntaxTreeNode
                    // validate the token here with the info in SyntaxTreeNode
                    workingNodes.top()->Children.push_back(word);
                    PopAllFilledNodes();
                    word = stream.NextItem();
                }
                else if (ignorableTokenTypes.contains(static_cast<int>(word.Type)))
                {
                    word = stream.NextItem();
                }
                else
                {
                    return unexpected(ParseFailResult{ .Message = format("cannot found token for terminal symbol({}) when parse", focus.Value) });
                }
            }
            else
            {
                if (auto dest = pair{ focus.Value, static_cast<int>(word.Type) }; parseTable.contains(dest))
                {
                ExpandRule:
                    // add log here
                    auto j = parseTable.at(dest);
                    symbolStack.pop();
                    auto const& rule = grammars.at(focus.Value).at(j);
                    //std::println("focus: {}, add rule: {}->{}", focus.Value, grammars[i].first, rule);
                    workingNodes.top()->Children.push_back(SyntaxTreeNode<Tok, Result>{ focus.Value, rule, });
                    workingNodes.push(&std::get<SyntaxTreeNode<Tok, Result>>(workingNodes.top()->Children.back()));
                    PopAllFilledNodes();

                    if (not rule.empty())
                    {
                        for (auto const& b : reverse(rule))
                        {
                            symbolStack.push(b);
                        }
                    }
                }
                else if (auto current = static_cast<int>(word.Type); replaceableTokenTypes.contains(current))
                {
                    auto const& replaces = replaceableTokenTypes.at(current);
                    for (auto x : replaces)
                    {
                        dest.second = x;
                        if (parseTable.contains(dest))
                        {
                            goto ExpandRule;
                        }
                    }
                    return unexpected(ParseFailResult{ .Message = format("cannot expand (nonterminal symbol: {}, replaceable word: {}) when parse", focus.Value, word) });
                }
                else if (ignorableTokenTypes.contains(static_cast<int>(word.Type)))
                {
                    word = stream.NextItem();
                }
                else
                {
                    return unexpected(ParseFailResult{ .Message = format("cannot expand (nonterminal symbol: {}, word: {}) when parse", focus.Value, word) });
                }
            }
        }
    }

private:
    template <typename Tok, typename Result>
    static auto TryRemoveChildrenCausedByLeftFactor(SyntaxTreeNode<Tok, Result>* node) -> void
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
};

/// <summary>
/// invoke callback inner
/// </summary>
// TODO rename method
template <typename Tok, typename Result>
static auto ReconstructSyntaxTreeAffectedByRemoveLeftRecursive(SyntaxTreeNode<Tok, Result> node, auto&& callback, map<size_t, map<size_t, stack<pair<size_t, size_t>>>> replaceHistory) -> SyntaxTreeNode<Tok, Result>
{
    if (not node.ChildSymbols.empty())
    {
        // make sure it's the right side which is after remove direct left recursive
        if (node.ChildSymbols.back().StartWith(node.Name) and node.ChildSymbols.back().EndWith(rightRecurSuffix))// might end with multiple times rightRecurSuffix
        {
            node.ChildSymbols.pop_back();
            auto remain = move(node.Children.back());
            node.Children.pop_back();
            callback(&node);
            
            // save and pop the last item, and insert above item in symbol and children
            // "a"(repeat item) may not one item TODO check
            auto recurToBottom = [leftRecurName=node.Name, &callback](this auto&& self, SyntaxTreeNode<Tok, Result> node, SyntaxTreeNode<Tok, Result> toAddChild)
            {
                if (node.ChildSymbols.back().StartWith(node.Name) and node.ChildSymbols.back().EndWith(rightRecurSuffix))// might end with multiple times rightRecurSuffix
                {
                    // restrict the node->Name to right recursive symbol name
                    node.Name = self.leftRecurName;
                    node.ChildSymbols.pop_back();
                    auto remain = move(node.Children.back());
                    node.Children.pop_back();

                    node.ChildSymbols.insert(node.ChildSymbols.begin(), self.leftRecurName);
                    node.Children.insert(node.Children.begin(), move(toAddChild));
                    callback(&node);
                    self(move(remain), move(node));
                }
                else
                {
                    node.ChildSymbols.insert(node.ChildSymbols.begin(), self.leftRecurName);
                    node.Children.insert(node.Children.begin(), move(toAddChild));
                    callback(&node);
                }
            };
            recurToBottom(move(remain), move(node));
        }
    }

    // handle indirect left recursive
    //for (size_t i = 0; i < node->ChildSymbols.size(); ++i)
    //{
    //    if (node->ChildSymbols[i].StartWith(node->Name) and node->ChildSymbols[i].EndWith(rightRecurSuffix))
    //    {
    //        expanded = true;
    //        auto& n = std::get<1>(node->Children[i]);
    //        symbols.append_range(move(n.ChildSymbols));
    //        children.append_range(move(n.Children));
    //    }
    //    else
    //    {
    //        symbols.push_back(move(node->ChildSymbols[i]));
    //        children.push_back(move(node->Children[i]));
    //    }
    //}
}

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
        auto starts = Starts(startSymbol, grammars); // string_view here is from grammars

        // handle e-production, focus <- pop() TODO
        for (auto i = 0; auto const& g : grammars)
        {
            auto const& nontermin = g.first;
            auto const& start = starts.at(i);
            for (auto j = 0; j < start.size(); ++j)
            {
                for (auto const& termin : start.at(j))
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
            }
            ++i;
        }
        //std::println("parse table: {}", parseTable);
        return GLLParser(move(startSymbol), move(grammars), move(parseTable), move(terminal2IntTokenType), move(ignorableTokenTypes), move(replaceableTokenTypes));
    }

    GLLParser(String startSymbol, SimpleGrammars grammars, map<pair<String, int>, vector<int>> parseTable, map<string_view, int> terminal2IntTokenType, set<int> ignorableTokenTypes, map<int, set<int>> replaceableTokenTypes)
        : startSymbol(move(startSymbol)), grammars(move(grammars)), parseTable(move(parseTable)), terminal2IntTokenType(move(terminal2IntTokenType)), ignorableTokenTypes(move(ignorableTokenTypes)), replaceableTokenTypes(move(replaceableTokenTypes))
    {
    }

    GLLParser(GLLParser const&) = delete;
    auto operator= (GLLParser const&) -> GLLParser = delete;
    GLLParser(GLLParser&&) = default;
    auto operator= (GLLParser&& that) -> GLLParser& = default;

    template <IToken Tok, typename Result>
    auto Parse(Stream<Tok> auto stream, INodeCallback<Tok, Result> auto callback, map<String, function<ParserResult<SyntaxTreeNode<Tok, Result>>(decltype(stream)&)>> externalParsers = {}) const
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
    template <IToken Tok, typename Result>
    auto ParseWith(SyntaxTreeNode<Tok, Result> root, stack<Symbol> initSymbolStack, Stream<Tok> auto& stream, INodeCallback<Tok, Result> auto const& callback, map<String, function<ParserResult<SyntaxTreeNode<Tok, Result>>(decltype(stream)&)>> const& externalParsers) const
        -> ParserResult<SyntaxTreeNode<Tok, Result>>
    {
        enum class UnitContinuation
        {
            Done, // followed by co_return
            OkOnThisChoice,
            Wait2ParseNewUnit,
        };

        enum class SubPartResult
        {
            Success2ContinueRemain,
            Fail,
            AllFail,
        };

        struct UnitParser // use to store then recover parse
        {
            SyntaxTreeNode<Tok, Result> Root; // attention: move will make this root empty which maybe used in future retry
            decltype(stream)& TokStream; // attention: attention when retry parse
            stack<Symbol> SymbolStack;

            decltype(callback)& Callback;
            map<String, function<ParserResult<SyntaxTreeNode<Tok, Result>>(decltype(TokStream)&)>> const& ExternalParsers;
            GLLParser const* const TopParser;

            auto Parse(vector<UnitParser>& parsePath) -> Exchanger<ParserResult<UnitContinuation>, SubPartResult>
            {
                stack<SyntaxTreeNode<Tok, Result>*> workingNodes;
                workingNodes.push(&Root);

                auto word = TokStream.NextItem();
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
                        word = TokStream.NextItem();
                    }
                };
                while (true)
                {
                    if (SymbolStack.empty())
                    {
                        TokStream.Rollback();
                        co_yield UnitContinuation::Done;
                        co_return;
                    }
                    auto const& focus = SymbolStack.top();

                    if (focus.IsEof() and MatchTerminal(focus, word))
                    {
                        co_yield UnitContinuation::Done;
                        co_return;
                    }
                    else if (ExternalParsers.contains(focus.Value))
                    {
                        TokStream.Rollback(); // return the current word, to let external parser read it
                        auto subResult = ExternalParsers.at(focus.Value)(TokStream);
                        if (not subResult.has_value())
                        {
                            co_yield unexpected(move(subResult.error()));
                            co_return;
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
                            word = TokStream.NextItem();
                        }
                        else
                        {
                            co_yield unexpected(ParseFailResult{ .Message = format("cannot found token for terminal symbol({}) when parse", focus.Value) });
                            co_return;
                        }
                    }
                    else
                    {
                        if (auto dest = pair{ focus.Value, static_cast<int>(word.Type) }; TopParser->parseTable.contains(dest))
                        {
                        ExpandRule:
                            // add log here
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
                                TokStream.Rollback();
                                auto pos = TokStream.CurrentPosition();
                                tuple k{ dest.first, dest.second, pos };

                                for (auto i = 1; auto j : js)
                                {
                                    println("start parse with {} option rule of {} options of {}", i++, js.size(), focus.Value);

                                    // use coroutines to implement below
                                    auto const& rule = TopParser->grammars.at(focus.Value).at(j);
                                    auto p = Construct(TopParser, focus.Value, rule, TokStream, Callback, ExternalParsers);
                                    parsePath.push_back(move(p));
                                    println("push a unit");
                                    co_yield UnitContinuation::Wait2ParseNewUnit;

                                    for (auto& r = co_await false; r.HasValue();) // TODO will re-ask when sub part retry
                                    {
                                        switch (r.Get())
                                        {
                                        case SubPartResult::Success2ContinueRemain:
                                            break;
                                        case SubPartResult::AllFail:
                                            goto NextOption;
                                        }
                                        println("continue parse with sub result");
                                        //TokStream.Rollback();
                                        // handle as terminal, self filled children. 
                                        // And we're going to use remain to handle remain symbols, so we don't continue use word
                                        //DoWhenGotChild(move(r.Get()), bool_constant<true>{}, bool_constant<false>{}); // pop symbol inner
                                        // TODO not merge sub result
                                        if (not SymbolStack.empty()) // TODO check if need
                                        {
                                            SymbolStack.pop(); // TODO need to recover when future trigger retry
                                        }

                                        if (SymbolStack.empty())
                                        {
                                            co_yield UnitContinuation::OkOnThisChoice;
                                        }
                                        else
                                        {
                                            SimpleRightSide rs;
                                            while (not SymbolStack.empty())
                                            {
                                                rs.push_back(move(SymbolStack.top().Value));
                                                SymbolStack.pop();
                                            }
                                            auto remainParser = Construct(TopParser, "remain", rs, TokStream, Callback, ExternalParsers);
                                            parsePath.push_back(move(remainParser));
                                            println("push a remain unit after {}", dest.first);
                                            co_yield UnitContinuation::Wait2ParseNewUnit;
                                            if (auto& remainResult = co_await false; remainResult.HasValue() and remainResult.Get() == SubPartResult::Success2ContinueRemain)
                                            {
                                                println("remain parse done");
                                                //DoWhenGotChild(move(remainResult.Get()), bool_constant<true>{}, bool_constant<false>{});
                                                // TODO not merge remain
                                                co_yield UnitContinuation::OkOnThisChoice; // TODO here should not be this kind value
                                            }

                                            // failed or recover from OkOnThisChoice, we need to recover the symbol stack
                                            for (auto const& b : reverse(rs))
                                            {
                                                SymbolStack.push(b);
                                            }
                                            SymbolStack.push(dest.first); // the focus value
                                        }
                                    }
                                NextOption:
                                    TokStream.RollbackTo(pos);
                                }
                                co_yield unexpected(ParseFailResult{ .Message = format("try parse ambiguous (nonterminal: {}, word: {}) failed", focus.Value, word) });
                                co_return;
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
                            co_yield unexpected(ParseFailResult{ .Message = format("cannot expand (nonterminal symbol: {}, replaceable word: {}) when parse", focus.Value, word) });
                            co_return;
                        }
                        else if (TopParser->ignorableTokenTypes.contains(static_cast<int>(word.Type)))
                        {
                            word = TokStream.NextItem();
                        }
                        else
                        {
                            co_yield unexpected(ParseFailResult{ .Message = format("cannot expand (nonterminal symbol: {}, word: {}) when parse", focus.Value, word) });
                            co_return;
                        }
                    }
                }
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

            static auto Construct(GLLParser const* const topParser, String startSymbol, SimpleRightSide startRule, decltype(TokStream)& stream, decltype(Callback)& callback, map<String, function<ParserResult<SyntaxTreeNode<Tok, Result>>(decltype(stream)&)>> const& externalParsers)
                -> UnitParser
            {
                stack<Symbol> symbolStack;
                for (auto x : reverse(startRule))
                {
                    symbolStack.push(move(x)); // TODO when symbolStack is empty, exit parse method
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

        vector<UnitParser> units;
        vector<pair<Exchanger<ParserResult<UnitContinuation>, SubPartResult>, int>> parsings; // use list
        units.reserve(100);
        parsings.reserve(100);
        auto current = 0;

        auto p = UnitParser
        {
            .Root = move(root),
            .TokStream = stream,
            .SymbolStack = move(initSymbolStack),
            .Callback = callback,
            .ExternalParsers = externalParsers,
            .TopParser = this, 
        };
        units.push_back(move(p));
        parsings.push_back({ units.back().Parse(units), -1 });
        for (;;)
        {
            println("current parsing: {} of {}, units size: {}", current, parsings.size(), units.size());
            auto& currentUnit = units.at(current);
            auto& currentParsing = parsings.at(current).first; // this address changed, because parsing is the item in dynamic memory
            auto const currentReturnAddr = parsings.at(current).second;
            println("current parsing: {}({} symbols, parsing addr {}), tok pos: {}", currentUnit.Root.Name, currentUnit.SymbolStack.size(), static_cast<void*>(std::addressof(currentParsing)), currentUnit.TokStream.CurrentPosition());
            if (currentParsing.MoveNext())
            {
                if (currentParsing.Current().has_value())
                {
                    switch (currentParsing.Current().value())
                    {
                    case UnitContinuation::Wait2ParseNewUnit:
                        println("Wait2ParseNewUnit");
                        // each unit has fixed return address which is current
                        parsings.push_back({ units.back().Parse(units), current });
                        current = parsings.size() - 1;
                        break;
                    case UnitContinuation::OkOnThisChoice:
                        println("OkOnThisChoice");
                    case UnitContinuation::Done:
                        println("Done");
                        if (parsings.size() == 1 and units.size() == 1) // if all symbol parsed, all is done, return the result
                        {
                            return move(units.front().Root);
                        }
                        current = currentReturnAddr;
                        if (current == -1)
                        {
                            return move(units.front().Root);
                        }
                        parsings.at(current).first.Input(SubPartResult::Success2ContinueRemain);
                        break;
                    }
                }
                else
                {
                    //if (current != parsings.size() - 1)
                    //{
                    //    throw std::logic_error(format("failed at {} of {} which is expected on top of stack", current, parsings.size()));
                    //}

                    println("parse failed: {}", currentParsing.Current().error().Message); // TODO may not error
                    // pop to continue last parsing
                    if (current == parsings.size() - 1)
                    {
                        units.pop_back();
                        parsings.pop_back();
                    }
                    current = currentReturnAddr;

                    // Input fail to return address parsing
                    if (units.empty() and parsings.empty())
                    {
                        return unexpected(ParseFailResult{ .Message = "par" }); // TODO refine message
                    }
                    else if (current == -1)
                    {
                        for (current = parsings.size() - 1;;)
                        {
                            switch (parsings.at(current).first.Current().value())
                            {
                            case UnitContinuation::OkOnThisChoice:
                                goto ContinueRetry;
                            case UnitContinuation::Done:
                                current = parsings.at(current).second;
                                parsings.pop_back();
                                break;
                            default:
                                throw std::logic_error("not handled UnitContinuation");
                            }
                        }
                    ContinueRetry:
                        ;
                    }
                    else
                    {
                        parsings.at(current).first.Input(SubPartResult::Fail); // may trigger non top item fail which is voilate the line 797 error
                    }
                }
            }
            else
            {
                throw std::logic_error("cannot move");
            }
        }
    }

    template <typename Tok, typename Result>
    static auto TryRemoveChildrenCausedByLeftFactor(SyntaxTreeNode<Tok, Result>* node) -> void
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
};

export
{
    class LLParser;
    class GLLParser;
}