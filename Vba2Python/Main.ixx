import std;
import Lexer;
import Base;
import Parser;
import VbaSpec;
import GrammarUnitLoader;
import PrattParser;
import Generator;

using std::string;
using std::pair;
using std::vector;
using std::ranges::views::filter;
using std::ranges::to;

int main()
{
    auto gs = GrammarUnitLoader({ grammars.begin(), grammars.end() });
    //auto [g0, g1] = gs.SeparateGrammarBaseOn("expression", "Procedural-module");
    auto l = Lexer<TokType>::New(lexRules);
    //{
    //    auto toks = l.Lex("1 + 2 * -3 + a.b / (4 + 5)") | filter([](auto& x) -> bool { return x.Type != TokType::Whitespace; }) | to<vector<Token<TokType>>>();
    //    toks.push_back({ .Type = TokType::EOF, .Value = "" }); // attention: add eof
    //    auto vs = VectorStream{ .Tokens = move(toks) };
    //    auto st = ParseExp<Token<TokType>, void>(vs, 0);
    //    std::println("done");
    //}

    string f = "VbaGrammarUnit.vba";
    //String focusSymbol = "enum-declaration";
    //String focusSymbol = "public-type-declaration";
    String focusSymbol = "function-declaration";
    //String focusSymbol = "if-statement";
    auto [_, partGrammars] = gs.SeparateGrammarBaseOn("expression", focusSymbol);
    for (auto const& x : LoadSource(f))
    {
        auto toks = l.Lex(x.second) | filter([](auto& x) -> bool { return x.Type != TokType::Whitespace; }) | to<vector<Token<TokType>>>();
        //toks.pop_back(); // pop last EOS
        toks.push_back({ .Type = TokType::EOF, .Value = "" }); // attention: add eof
        auto relatedSymbol = String(x.first);
        if (relatedSymbol != focusSymbol)
        {
            continue;
        }
        // { "Reserved-identifier", }
        auto p = GLLParser::ConstructFrom(focusSymbol, partGrammars, terminal2IntTokenType, 
            { static_cast<int>(TokType::CommentEndOfLine), static_cast<int>(TokType::RemStatement), },
            {
                // terminalXXX will change if change vba.abnf file, We can specify a name for the terminal in abnf
                { static_cast<int>(TokType::Terminal209)/*0*/, { static_cast<int>(TokType::Integer), static_cast<int>(TokType::Expression), } }, // Integer also can replace Expression. All Expression atom's prefix can replace/trigger Expression
                { static_cast<int>(TokType::Integer), { static_cast<int>(TokType::Expression), } },
            });
        auto st = p.Parse<Token<TokType>, void>(VectorStream{ .Tokens = move(toks) }, [](auto n) { /*std::println("encounter {}", n->Name);*/ },
            {
                { "expression", [](auto& stream) { return ParseExp<Token<TokType>, void>(stream, 0); }},
            }
        );

        if (st.has_value())
        {
            std::println("ast:\n{}", st.value());
            std::println("---------parse done---------");
        }
        else
        {
            std::println("failed: {}", st.error().Message);
            std::println("---------parse failed---------");
        }
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
        //auto toks = l.Lex(content) | filter([](auto& x) -> bool { return x.Type != TokType::Whitespace and x.Type != TokType::EOL; }) | to<vector<Token<TokType>>>();
        std::println("Hello World");
        //toks.push_back({ .Type = TokType::EOF, .Value = "" }); // add eof
    }
}