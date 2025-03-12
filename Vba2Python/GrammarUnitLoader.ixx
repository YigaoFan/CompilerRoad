export module GrammarUnitLoader;
/// <summary>
/// mainly this module is for debugging and test
/// </summary>

import std;
import Parser;

using std::string;
using std::generator;
using std::pair;
using std::vector;
using std::map;
using std::move_iterator;
using std::queue;
using std::move;

auto LoadSource(string const& grammarUnitFilename) -> generator<pair<string, string>>
{
    std::ifstream file(grammarUnitFilename);

    if (file.is_open())
    {
        string grammarUnitContent;
        string grammarUnitName;
        string line;
        
        while (std::getline(file, line))
        {
            if (line == "-- --")
            {
                co_yield { move(grammarUnitName), move(grammarUnitContent) };
            }
            else if (line.starts_with("-- ") and line.ends_with(" --")) // pattern check -- xxx --
            {
                grammarUnitContent.clear();
                grammarUnitName.clear();
                auto start = line.find_first_of(' ') + 1;
                auto size = line.find_last_of(' ') - start;
                grammarUnitName = line.substr(start, size);
            }
            else
            {
                grammarUnitContent.append(move(line));
                grammarUnitContent.push_back('\n');
            }
        }
        file.close();
    }
    co_return;
}

class GrammarUnitLoader
{
private:
    map<LeftSide, vector<SimpleRightSide>> grammars;

public:
    GrammarUnitLoader(map<LeftSide, vector<SimpleRightSide>> grammars) : grammars(move(grammars))
    {
    }

    auto LoadRelatedGrammarsOf(String symbol) -> map<LeftSide, vector<SimpleRightSide>>
    {
        queue<String> workingSymbols;
        workingSymbols.push(symbol);
        map<LeftSide, vector<SimpleRightSide>> relateds;

        while (not workingSymbols.empty())
        {
            auto focus = move(workingSymbols.front());
            workingSymbols.pop();

            relateds.insert({ focus, grammars.at(focus) });
            for (auto& rule : grammars.at(focus))
            {
                for (auto& symbol : rule)
                {
                    if (grammars.contains(symbol) and (not relateds.contains(symbol)))
                    {
                        workingSymbols.push(symbol);
                    }
                }
            }
        }
        return relateds;
    }
};

export
{
    auto LoadSource(string const& grammarUnitFilename) -> generator<pair<string, string>>;
    class GrammarUnitLoader;
}