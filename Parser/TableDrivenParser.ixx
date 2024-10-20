export module TableDrivenParser;

import std;
import ParserBase;
import GrammarSet;
import InputStream;

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
    map<pair<string_view, string_view>, pair<int, int>> parseTable;
public:
    // how to distinguish nonterminal and terminal(which has enum type) in grammar
    // do we need convert nonterminal and terminal to int to make program litter faster
    static auto ConstructFrom(string startSymbol, vector<Grammar> grammars) -> TableDrivenParser
    {
        map<pair<string_view, string_view>, pair<int, int>> parseTable;

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
                    if (parseTable.contains({ nontermin, termin }))
                    {
                        throw logic_error(format("grammar isn't LL(1), {{{}, {}}} point to multiple grammar: {}, {}", nontermin, termin, parseTable[{ nontermin, termin }], j));
                    }
                    parseTable.insert({ { nontermin, termin }, { i, j } });
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
    auto Parse(Stream<Tok> auto stream) -> ParserResult<AstNode>
    {
        using std::ranges::to;

        auto nontermins = Nontermins(grammars) | to<set<string_view>>();
        auto word = stream.NextToken();
        stack<string> stack;
        stack.push(eof);
        stack.push(startSymbol);

        while (true)
        {
            auto const& focus = stack.top();
            if (focus == eof and word == eof) // TODO word should not compare with eof directly
            {
                return ParseSuccessResult<AstNode>{};
            }
            else if ((not nontermins.contains(focus)) or focus == eof)
            {
                // TODO compare
                if (focus == word)
                {
                    stack.pop();
                    word = stream.NextItem();
                }
                else
                {
                    return ParseFailResult{ .Message = format("cannot found token for terminal({}) when parse", focus) };
                }
            }
            else
            {
                // how to construct AST
                if (parseTable.contains({ focus, word }))
                {
                    stack.pop();
                    auto [i, j] = parseTable[{ focus, word }];
                    auto const& rule = grammars[i].second[j];
                    for (auto const& b : rule)
                    {
                        if (b != epsilon)
                        {
                            stack.push(b);
                        }
                    }
                }
                else
                {
                    return ParseFailResult{ .Message = format("cannot expand (nonterminal: {}, word: {}) when parse", focus, word) };
                }
            }
        }

    }
};

export
{
    class TableDrivenParser;
}