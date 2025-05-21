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
        auto word = stream.Current();
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
                //stream.Rollback(); // return the current word, to let external parser read it
                auto subResult = externalParsers.at(focus.Value)(stream);
                if (not subResult.has_value())
                {
                    return subResult;
                }
                // almost same as handle terminal
                symbolStack.pop();
                workingNodes.top()->Children.push_back(move(subResult.value()));
                PopAllFilledNodes();
                word = stream.Current(); // TODO check
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
                    stream.MoveNext();
                    word = stream.Current();
                }
                else if (ignorableTokenTypes.contains(static_cast<int>(word.Type)))
                {
                    stream.MoveNext();
                    word = stream.Current();
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
                    stream.MoveNext();
                    word = stream.Current();
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
    set<int> const repeatableTokenTypes;
public:
    /// <summary>
    /// attention: make string_view in terminal2IntTokenType is alive when parse
    /// </summary>
    static auto ConstructFrom(String startSymbol, SimpleGrammars grammars, map<string_view, int> terminal2IntTokenType, set<int> ignorableTokenTypes = {}, map<int, set<int>> replaceableTokenTypes = {}, set<int> repeatableTokenTypes = {}) -> GLLParser
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
        return GLLParser(move(startSymbol), move(grammars), move(parseTable), move(terminal2IntTokenType), move(ignorableTokenTypes), move(replaceableTokenTypes), move(repeatableTokenTypes));
    }

    GLLParser(String startSymbol, SimpleGrammars grammars, map<pair<String, int>, vector<int>> parseTable, map<string_view, int> terminal2IntTokenType, set<int> ignorableTokenTypes, map<int, set<int>> replaceableTokenTypes, set<int> repeatableTokenTypes)
        : startSymbol(move(startSymbol)), grammars(move(grammars)), parseTable(move(parseTable)), terminal2IntTokenType(move(terminal2IntTokenType)), 
        ignorableTokenTypes(move(ignorableTokenTypes)), replaceableTokenTypes(move(replaceableTokenTypes)), repeatableTokenTypes(move(repeatableTokenTypes))
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
                        if (not TopParser->repeatableTokenTypes.contains(static_cast<int>(word.Type)))
                        {
                            TokStream.MoveNext();
                            word = TokStream.Current();
                        }
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
                        else if (TopParser->ignorableTokenTypes.contains(static_cast<int>(word.Type)) or TopParser->repeatableTokenTypes.contains(static_cast<int>(word.Type)))
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
                        else if (TopParser->ignorableTokenTypes.contains(static_cast<int>(word.Type)) or TopParser->repeatableTokenTypes.contains(static_cast<int>(word.Type)))
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