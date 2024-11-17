export module TableDrivenParser;

import std;
import ParserBase;
import GrammarSet;
import InputStream;
import String;

using std::vector;
using std::map;
using std::string_view;
using std::size_t;
using std::pair;
using std::logic_error;
using std::string;
using std::stack;
using std::set;
using std::move;
using std::format;

class TableDrivenParser
{
private:
    string startSymbol;
    vector<Grammar> grammars;
    map<pair<string_view, int>, pair<int, int>> parseTable;
public:
    // how to distinguish nonterminal and terminal(which has enum type from Lexer) in grammar
    // do we need convert nonterminal and terminal to int to make program litter faster
    static auto ConstructFrom(string startSymbol, vector<Grammar> grammars, map<string, int> terminal2IntTokenType) -> TableDrivenParser
    {
        map<pair<string_view, int>, pair<int, int>> parseTable;

        auto starts = Starts(startSymbol, grammars);

        for (size_t i = 0; auto const& g : grammars)
        {
            auto const& nontermin = g.first;
            auto const& start = starts.at(i);
            ++i;
            for (auto j = 0; j < start.size(); ++j)
            {
                for (auto const& termin : start.at(j))
                {
                    auto tokenType = terminal2IntTokenType[string(termin)];
                    if (parseTable.contains({ nontermin, tokenType }))
                    {
                        throw logic_error(format("grammar isn't LL(1), {{{}, {}}} point to multiple grammar: {}, {}", nontermin, termin, parseTable[{ nontermin, termin }], j));
                    }
                    parseTable.insert({ { nontermin, tokenType }, { i, j } });
                }
            }

        }
        return TableDrivenParser(move(startSymbol), move(grammars), move(parseTable));
    }

    TableDrivenParser(string startSymbol, vector<Grammar> grammars, map<pair<string_view, string_view>, pair<int, int>> parseTable)
        : startSymbol(move(startSymbol)), grammars(move(grammars)), parseTable(move(parseTable))
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

        struct Symbol
        {
            string Value;
            Symbol(string symbol) : Value(move(symbol))
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
        auto Match = [&terminal2IntTokenType](Symbol const& symbol, Tok const& token) -> bool
        {
            if (symbol.IsEof() and token.IsEof())
            {
                return true;
            }
            return terminal2IntTokenType[symbol] == token.Type;
        };
        auto IsTerminal = [nontermins = Nontermins(grammars) | to<set<string_view>>()](Symbol const& t) { return not nontermains.contains(t); };
        stack<Symbol> stack;
        stack.push(eof);
        stack.push(startSymbol);
        auto word = stream.NextItem();
        AstNode root;
        stack<AstNode*> workingNodes{};
        workingNodes.push(&root);

        while (true)
        {
            auto const& focus = stack.top();

            if (focus.IsEof() and Match(focus, word))
            {
                return ParseSuccessResult<AstNode>{ .Result = move(root), .Remain = "" };
            }
            else if (IsTerminal(focus) or focus.IsEof())
            {
                if (Match(focus, word))
                {
                    stack.pop();
                    // pop means match here
                    // combine focus info and word info into AstNode
                    workNodes.top()->Children.push_back(word);
                    workingNodes.push()
                    word = stream.NextItem();
                }
                else
                {
                    return ParseFailResult{ .Message = format("cannot found token for terminal symbol({}) when parse", focus) };
                }
            }
            else
            {
                // how to construct AST
                if (parseTable.contains({ focus, word })) // word here actually is word.Type
                {
                    auto [i, j] = parseTable[{ focus, word }];
                    stack.pop(); // note: pop will change focus value, so it move to the bottom of the previous step
                    auto const& rule = grammars[i].second[j];
                    for (auto const& b : reverse(rule))
                    {
                        if (b != epsilon) // should we jump over the first item in rule?
                        {
                            stack.push(b);
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