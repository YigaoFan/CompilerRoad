export module Parser:TableDrivenParser;

import std;
import Base;
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
using std::move;
using std::format;
using std::println;

template <typename T, typename Tok, typename Result>
concept INodeCallback = requires (T callback, SyntaxTreeNode<Tok, Result>* node)
{
    { callback(node) } -> std::same_as<void>;
};

class TableDrivenParser
{
private:
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
    String startSymbol;
    vector<SimpleGrammar> grammars;
    map<pair<String, int>, pair<int, int>> parseTable;
    map<string_view, int> terminal2IntTokenType;
public:
    // how to distinguish nonterminal and terminal(which has enum type from Lexer) in grammar
    // do we need convert nonterminal and terminal to int to make program litter faster
    /// <summary>
    /// attention: make string_view in terminal2IntTokenType is alive when parse
    /// </summary>
    static auto ConstructFrom(String startSymbol, vector<SimpleGrammar> grammars, map<string_view, int> terminal2IntTokenType) -> TableDrivenParser
    {
        vector<SimpleGrammar> newAddGrammars;
        for (auto& g : grammars)
        {
            auto [newG, addGrammars] = LeftFactor(move(g));
            g = move(newG);
            if (addGrammars.has_value())
            {
                newAddGrammars.append_range(move(addGrammars.value()));
            }
        }
        grammars.append_range(move(newAddGrammars));
        //std::println("after left refactor: {}", grammars);
        map<pair<String, int>, pair<int, int>> parseTable;
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
                        auto const& [otherI, otherJ] = parseTable.at(key);
                        throw logic_error(format("grammar isn't LL(1), {{{}, {}}} point to multiple grammar: {}, {}", nontermin, termin, parseTable[{ nontermin, tokenType }], j));
                    }
                    //println("when come {}, move to {} -> {}", key, grammars[i].first, grammars[i].second[j]);
                    parseTable.insert({ move(key), { i, j }});
                }
            }
            ++i;
        }
        //std::println("parse table: {}", parseTable);
        return TableDrivenParser(move(startSymbol), move(grammars), move(parseTable), move(terminal2IntTokenType));
    }

    TableDrivenParser(String startSymbol, vector<SimpleGrammar> grammars, map<pair<String, int>, pair<int, int>> parseTable, map<string_view, int> terminal2IntTokenType)
        : startSymbol(move(startSymbol)), grammars(move(grammars)), parseTable(move(parseTable)), terminal2IntTokenType(move(terminal2IntTokenType))
    { }

    TableDrivenParser(TableDrivenParser const&) = delete;
    auto operator= (TableDrivenParser const&) -> TableDrivenParser = delete;
    TableDrivenParser(TableDrivenParser&&) = default;
    auto operator= (TableDrivenParser&& that) -> TableDrivenParser& = default;

    template <IToken Tok, typename Result>
    auto Parse(Stream<Tok> auto stream, INodeCallback<Tok, Result> auto callback, set<int> ignorableTokenTypes = {}, map<int, set<int>> replaceableTokenTypes = {}, map<String, function<ParserResult<SyntaxTreeNode<Tok, Result>>(decltype(stream)&)>> externalParsers = {})
        -> ParserResult<SyntaxTreeNode<Tok, Result>>
    {
        using std::ranges::to;
        using std::ranges::views::reverse;
        using std::ranges::views::filter;
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
        auto IsTerminal = [nontermins = Nontermins(grammars) | to<set<string_view>>()](Symbol const& t) { return not nontermins.contains(t.Value); };
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
                auto subResult = externalParsers.at(focus.Value)(stream);
                if (not subResult.has_value())
                {
                    return subResult;
                }
                // almost same as handle terminal
                symbolStack.pop();
                workingNodes.top()->Children.push_back(move(subResult.value()));
                PopAllFilledNodes();
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
                    auto [i, j] = parseTable[dest];
                    symbolStack.pop();
                    auto const& rule = grammars[i].second[j];
                    //std::println("focus: {}, add rule: {}->{}", focus.Value, grammars[i].first, rule);
                    workingNodes.top()->Children.push_back(SyntaxTreeNode<Tok, Result>{ grammars[i].first, rule, });
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

class LrTableDrivenParser
{

};

export
{
    class TableDrivenParser;
}