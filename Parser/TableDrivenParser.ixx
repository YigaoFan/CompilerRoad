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
using std::move;
using std::format;
using std::println;

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
        grammars = RemoveIndirectLeftRecur(startSymbol, move(grammars));
        auto starts = Starts(startSymbol, grammars); // string_view here is from grammar
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
        std::println("parse table: {}", parseTable);
        return TableDrivenParser(move(startSymbol), move(grammars), move(parseTable), move(terminal2IntTokenType));
    }

    TableDrivenParser(String startSymbol, vector<SimpleGrammar> grammars, map<pair<String, int>, pair<int, int>> parseTable, map<string_view, int> terminal2IntTokenType)
        : startSymbol(move(startSymbol)), grammars(move(grammars)), parseTable(move(parseTable)), terminal2IntTokenType(move(terminal2IntTokenType))
    { }

    TableDrivenParser(TableDrivenParser const&) = delete;
    auto operator= (TableDrivenParser const&) -> TableDrivenParser = delete;
    TableDrivenParser(TableDrivenParser&&) = default;
    auto operator= (TableDrivenParser&& that) -> TableDrivenParser& = default;

    template <IToken Tok>
    auto Parse(Stream<Tok> auto stream) -> ParserResult<AstNode<Tok>>
    {
        using std::ranges::to;
        using std::ranges::views::reverse;
        using std::ranges::views::filter;
        using std::unexpected;

        /// <summary>
        /// Only work for terminal symbol or eof
        /// </summary>
        auto MatchTerminal = [this](Symbol const& symbol, Tok const& token) -> bool
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

            return static_cast<int>(terminal2IntTokenType.at(symbol.Value)) == static_cast<int>(token.Type); // TODO compare the actual value, like the Keyword include multiple values
        };
        auto IsTerminal = [nontermins = Nontermins(grammars) | to<set<string_view>>()](Symbol const& t) { return not nontermins.contains(t.Value); };
        stack<Symbol> symbolStack;
        symbolStack.push(String(eof));
        symbolStack.push(startSymbol);
        auto word = stream.NextItem();
        AstNode<Tok> root{ .Name = "root", .ChildSymbols = { startSymbol } }; // TODO why "root" is shown as "???" in VS debugger
        stack<AstNode<Tok>*> workingNodes;
        workingNodes.push(&root);

        while (true)
        {
            auto const& focus = symbolStack.top();

            if (focus.IsEof() and MatchTerminal(focus, word))
            {
                return ParseSuccessResult<AstNode<Tok>>{ .Result = move(root), .Remain = "" };
            }
            else if (IsTerminal(focus) or focus.IsEof())
            {
                if (MatchTerminal(focus, word))
                {
                    symbolStack.pop();
                    // combine focus info and word info into AstNode
                    // validate the token here with the info in AstNode
                    workingNodes.top()->Children.push_back(word);
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
                    // add log here
                    auto [i, j] = parseTable[dest];
                    symbolStack.pop();
                    auto const& rule = grammars[i].second[j];
                    workingNodes.top()->Children.push_back(AstNode<Tok>{.Name = grammars[i].first, .ChildSymbols = rule, .Children = {} });
                    workingNodes.push(&std::get<AstNode<Tok>>(workingNodes.top()->Children.back()));
                PopAllFilledNodes:
                    if (not workingNodes.empty())
                    {
                        if (auto working = workingNodes.top(); working->Children.size() == working->ChildSymbols.size())
                        {
                            workingNodes.pop();
                            goto PopAllFilledNodes;
                        }
                    }

                    if (not rule.empty())
                    {
                        for (auto const& b : reverse(rule))
                        {
                            symbolStack.push(b);
                        }
                    }
                }
                else
                {
                    return unexpected(ParseFailResult{ .Message = format("cannot expand (nonterminal symbol: {}, word: {}) when parse", focus.Value, word) });
                }
            }
        }

    }
};

class LrTableDrivenParser
{

};

export
{
    class TableDrivenParser;
}