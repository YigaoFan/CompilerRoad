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

    auto SeparateGrammarBaseOn(String root0, String root1) -> pair<map<LeftSide, vector<SimpleRightSide>>, map<LeftSide, vector<SimpleRightSide>>>
    {
        auto gs = LoadRelatedGrammarsOf(root0);
        auto remain = RefineGrammarAfterRemove(root0, root1);
        return { move(gs), move(remain) };
    }
private:
    auto RefineGrammarAfterRemove(String symbol, String root) -> map<LeftSide, vector<SimpleRightSide>>
    {
        // load the old root related grammars with not expand the this symbol
        //auto rules = move(grammars.at(symbol));
        // check reference change? no.
        auto l = GrammarUnitLoader(grammars); // copy grammars to isolate changes
        l.grammars.erase(symbol);
        return l.LoadRelatedGrammarsOf(root);
    }
};

export
{
    auto LoadSource(string const& grammarUnitFilename) -> generator<pair<string, string>>;
    class GrammarUnitLoader;
}