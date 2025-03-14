import std;
import Lexer;
import Base;
import Parser;
import VbaSpec;
import GrammarUnitLoader;

using std::string;
using std::pair;
using std::vector;
using std::ranges::views::filter;
using std::ranges::to;

int main()
{
    string f = "VbaGrammarUnit.vba";
    auto l = Lexer<TokType>::New(lexRules);
    auto gs = GrammarUnitLoader({ grammars.begin(), grammars.end() });
    String focusSymbol = "enum-declaration";
    for (auto const& x : LoadSource(f))
    {
        auto partGrammars = gs.LoadRelatedGrammarsOf(focusSymbol);
        auto toks = l.Lex(x.second) | filter([](auto& x) -> bool { return x.Type != TokType::Whitespace; }) | to<vector<Token<TokType>>>();
        //toks.pop_back(); // pop last EOS
        toks.push_back({ .Type = TokType::EOF, .Value = "" }); // add eof
        auto relatedSymbol = String(x.first);
        if (relatedSymbol != focusSymbol)
        {
            continue;
        }
        auto p = TableDrivenParser::ConstructFrom(focusSymbol, { partGrammars.begin(), partGrammars.end() }, terminal2IntTokenType);
        p.Parse<Token<TokType>, void>(VectorStream{ .Tokens = move(toks) }, [](auto) { }, { static_cast<int>(TokType::EOS) },
        {
            { static_cast<int>(TokType::Terminal66), { static_cast<int>(TokType::EOS) } },
        });
        std::println("parse done");
        //std::println("{}: {}", x.first, x.second);
    }
    return 0;

    auto filename = "WebClient.cls";
    std::ifstream file(filename);
    string content;
    if (file.is_open())
    {
        content.reserve(std::filesystem::file_size(filename));
        std::string line;

        while (std::getline(file, line))
        {
            content.append(move(line));
            content.push_back('\n');
        }
        file.close();
        auto toks = l.Lex(content) | filter([](auto& x) -> bool { return x.Type != TokType::Whitespace and x.Type != TokType::EOL; }) | to<vector<Token<TokType>>>();
        std::println("Hello World");
        //toks.push_back({ .Type = TokType::EOF, .Value = "" }); // add eof
    }
}