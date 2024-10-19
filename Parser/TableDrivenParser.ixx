export module TableDrivenParser;

import std;
import ParserBase;
import GrammarSet;

using std::vector;
using std::map;
using std::string_view;
using std::size_t;
using std::pair;
using std::logic_error;
using std::move;
using std::format;

class TableDrivenParser
{
private:
    vector<Grammar> grammars;
    map<pair<string_view, string_view>, int> parseTable;
public:
    // how to distinguish nonterminal and terminal(which has enum type) in grammar
    // do we need convert nonterminal and terminal to int to make program litter faster
    auto ConstructFrom(string_view startSymbol, vector<Grammar> grammars) -> TableDrivenParser
    {
        map<pair<string_view, string_view>, int> parseTable;

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
                    parseTable.insert({ { nontermin, termin }, j });
                }
            }

        }
        return TableDrivenParser(move(grammars), move(parseTable));
    }

    TableDrivenParser(vector<Grammar> grammars, map<pair<string_view, string_view>, int> parseTable)
        : grammars(move(grammars)), parseTable(move(parseTable))
    { }

    TableDrivenParser(TableDrivenParser const&) = delete;
    auto operator= (TableDrivenParser const&) -> TableDrivenParser = delete;
    TableDrivenParser(TableDrivenParser&&) = default;
    auto operator= (TableDrivenParser&& that) -> TableDrivenParser&
    {
        grammars = move(that.grammars);
        parseTable = move(that.parseTable);
    }

    auto Parse(ITokenStream auto stream) -> ParserResult<int>
    {
        throw;
    }
};