export module Parser:TableDrivenParser;

import std;
import :ParserBase;
import :GrammarSet;
import :InputStream;
import Base;

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

class TableDrivenParser
{
private:
    String startSymbol;
    vector<Grammar> grammars;
    map<pair<String, int>, pair<int, int>> parseTable;
    map<string_view, int> terminal2IntTokenType;
public:
    // how to distinguish nonterminal and terminal(which has enum type from Lexer) in grammar
    // do we need convert nonterminal and terminal to int to make program litter faster
    /// <summary>
    /// attention: make string_view in terminal2IntTokenType is alive when parse
    /// </summary>
    static auto ConstructFrom(String startSymbol, vector<Grammar> grammars, map<string_view, int> terminal2IntTokenType) -> TableDrivenParser
    {
        map<pair<String, int>, pair<int, int>> parseTable;

        auto starts = Starts(startSymbol, grammars); // string_view here is from grammar

        for (auto i = 0; auto const& g : grammars)
        {
            auto const& nontermin = g.first;
            auto const& start = starts.at(i);
            ++i;
            for (auto j = 0; j < start.size(); ++j)
            {
                for (auto const& termin : start.at(j))
                {
                    auto tokenType = terminal2IntTokenType[termin];
                    if (parseTable.contains({ nontermin, tokenType }))
                    {
                        throw logic_error(format("grammar isn't LL(1), {{{}, {}}} point to multiple grammar: {}, {}", nontermin, termin, parseTable[{ nontermin, tokenType }], j));
                    }
                    parseTable.insert({ { nontermin, tokenType }, { i, j } });
                }
            }

        }
        return TableDrivenParser(move(startSymbol), move(grammars), move(parseTable), move(terminal2IntTokenType));
    }

    TableDrivenParser(String startSymbol, vector<Grammar> grammars, map<pair<String, int>, pair<int, int>> parseTable, map<string_view, int> terminal2IntTokenType)
        : startSymbol(move(startSymbol)), grammars(move(grammars)), parseTable(move(parseTable)), terminal2IntTokenType(move(terminal2IntTokenType))
    { }

    TableDrivenParser(TableDrivenParser const&) = delete;
    auto operator= (TableDrivenParser const&) -> TableDrivenParser = delete;
    TableDrivenParser(TableDrivenParser&&) = default;
    auto operator= (TableDrivenParser&& that) -> TableDrivenParser&
    {
        startSymbol = move(that.startSymbol);
        grammars = move(that.grammars);
        parseTable = move(that.parseTable);
        return *this;
    }

    // how to cooperate with the type from lexer
    template <IToken Tok>
    auto Parse(Stream<Tok> auto stream) -> ParserResult<AstNode<Tok>>
    {
        using std::ranges::to;
        using std::ranges::views::reverse;
        using std::ranges::views::filter;

        struct Symbol
        {
            String Value;
            Symbol(String symbol) : Value(move(symbol))
            { }
            
            operator string const& () const
            {
                return Value;
            }

            auto IsEof() const -> bool
            {
                return Value == eof;
            }
        };
        /// <summary>
        /// Only work for terminal symbol or eof
        /// </summary>
        auto MatchTerminal = [&terminal2IntTokenType](Symbol const& symbol, Tok const& token) -> bool
        {
            if (symbol.IsEof() and token.IsEof())
            {
                return true;
            }
            return terminal2IntTokenType[symbol] == token.Type;
        };
        auto IsTerminal = [nontermins = Nontermins(grammars) | to<set<string_view>>()](Symbol const& t) { return not nontermins.contains(t); };
        stack<Symbol> symbolStack;
        symbolStack.push(eof);
        symbolStack.push(startSymbol);
        auto word = stream.NextItem();
        AstNode<Tok> root;
        stack<AstNode<Tok>*> workingNodes{};
        workingNodes.push(&root);

        while (true)
        {
            auto const& focus = symbolStack.top();

            if (focus.IsEof() and MatchTerminal(focus, word))
            {
                return ParseSuccessResult<AstNode>{ .Result = move(root), .Remain = "" };
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
                    return ParseFailResult{ .Message = format("cannot found token for terminal symbol({}) when parse", focus) };
                }
            }
            else
            {
                if (parseTable.contains({ focus, word.Type }))
                {
                    auto [i, j] = parseTable[{ focus, word.Type }];
                    symbolStack.pop(); // note: pop will change focus value, so it move to the bottom of the previous step
                    auto const& rule = grammars[i].second[j];
                    auto filteredRule = rule | filter([](auto x) { return x != epsilon; });
                    workingNodes.top()->Children.push_back(AstNode<Tok>{ .ChildSymbols = filteredRule, .Children = {} }); // maybe need to<vector<String>>()
                    workingNodes.push(&workingNodes.top()->Children.back());

                    for (auto const& b : reverse(filteredRule))
                    {
                        if (b != epsilon)
                        {
                            symbolStack.push(b);
                        }
                    }
                }
                else
                {
                    return ParseFailResult{ .Message = format("cannot expand (nonterminal symbol: {}, word: {}) when parse", focus, word) };
                }
            }
        }

    }
};

export
{
    class TableDrivenParser;
}