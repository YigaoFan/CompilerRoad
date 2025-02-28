import std;
import Lexer;
import Parser;
import TokType;
import Ast;
import Transformer;
import Checker;
import CodeEmitter; // affect the bottom format code

using std::string;
using std::pair;
using std::vector;
using std::size_t;
using std::shared_ptr;

template <typename T>
struct VectorStream
{
    vector<T> Tokens;
    size_t Index = 0;

    auto NextItem() -> T
    {
        return Tokens[Index++];
    }

    auto Eof() const -> bool
    {
        return Index >= Tokens.size();
    }
};

int main()
{
    using std::ranges::views::filter;
    using std::ranges::to;

    std::array rules =
    {
        pair<string, TokType>{ "\\->", TokType::Arrow },
        pair<string, TokType>{ "<", TokType::LeftAngle },
        pair<string, TokType>{ ">", TokType::RightAngle },
        pair<string, TokType>{ "\\[", TokType::LeftSquare },
        pair<string, TokType>{ "\\]", TokType::RightSquare },
        pair<string, TokType>{ "{", TokType::LeftBracket },
        pair<string, TokType>{ "}", TokType::RightBracket },
        pair<string, TokType>{ "\\*", TokType::StarMark },
        pair<string, TokType>{ "\\|", TokType::PipeMark },
        pair<string, TokType>{ " |\t", TokType::Whitespace },
        pair<string, TokType>{ "[a-zA-Z][a-zA-Z0-9_\\-]*", TokType::Symbol },
        pair<string, TokType>{ "[1-9][0-9]*", TokType::Number },
        pair<string, TokType>{ ";[^\n]*", TokType::Comment },
        pair<string, TokType>{ "\\-", TokType::Hyphen },
        pair<string, TokType>{ "\\(", TokType::LeftParen },
        pair<string, TokType>{ "\\)", TokType::RightParen },
        pair<string, TokType>{ "\n", TokType::Newline },
        pair<string, TokType>{ "\"((\\\\[^\n])|[^\\\\\"\n])*\"", TokType::Terminal },
        pair<string, TokType>{ "'[a-zA-Z0-9]'", TokType::QutotedDigitOrAlphabet },
        pair<string, TokType>{ "r\"((\\\\[^\n])|[^\\\\\"\n])*\"", TokType::RegularExpression },
    };
    auto l = Lexer<TokType>::New(rules);
    auto p = TableDrivenParser::ConstructFrom("grammars",
    {// how to represent empty in current grammar
        { "grammars", {
            { "optional-newlines", "grammar", "more-grammars", }, // TODO support newline after grammar
            { },
        }},
        { "more-grammars", {
            { "newlines", "grammar", "more-grammars"},
            { "newlines" },
            { },
        }},
        { "grammar", {
            { "sym", "->", "productions", "optional-comment" },
        }},
        { "optional-comment", {
            { "comment" },
            { },
        }},
        { "optional-newlines", {
            { "newlines", },
            { },
        }},
        { "newlines", {
            { "newline", "more-newlines" },
        }},
        { "more-newlines", {
            { "newline", "more-newlines" },
            { },
        }},
        { "productions", {
            { "production", "more-productions" },
            { },
        }},
        { "more-productions", {
            { "|", "productions" }, // ref productions to support "| [end]" to represent empty production
            { },
        }},
        { "production", { // once production occurs, it means not empty, so production doesn't have {} right side
            { "item", "more-items" },
        }},
        { "more-items", {
            { "item", "more-items" },
            { },
        }},
        { "item", {
            { "item_0" },
            { "*", "item_0" },
            { "num", "*", "num", "item_0" },
            { "num", "*", "item_0" },
        }},
        { "item_0", {
            { "regExp" },
            { "terminal" },
            { "sym" },
            { "digitOrAlphabet", "-", "digitOrAlphabet" },
            { "[", "productions", "]" },
            { "(", "productions", ")" },
        }},
    },
    {
        { "->", static_cast<int>(TokType::Arrow) },
        { "|", static_cast<int>(TokType::PipeMark) },
        { "<", static_cast<int>(TokType::LeftAngle) },
        { ">", static_cast<int>(TokType::RightAngle) },
        { "(", static_cast<int>(TokType::LeftParen) },
        { ")", static_cast<int>(TokType::RightParen) },
        { "[", static_cast<int>(TokType::LeftSquare) },
        { "]", static_cast<int>(TokType::RightSquare) },
        { "{", static_cast<int>(TokType::LeftBracket) },
        { "}", static_cast<int>(TokType::RightBracket) },
        { "*", static_cast<int>(TokType::StarMark) },
        { "|", static_cast<int>(TokType::PipeMark) },
        { "sym", static_cast<int>(TokType::Symbol) },
        { "num", static_cast<int>(TokType::Number) },
        { "-", static_cast<int>(TokType::Hyphen) },
        { "comment", static_cast<int>(TokType::Comment) },
        { eof, static_cast<int>(TokType::EOF) },
        { "newline" , static_cast<int>(TokType::Newline) },
        { "terminal" , static_cast<int>(TokType::Terminal) },
        { "digitOrAlphabet" , static_cast<int>(TokType::QutotedDigitOrAlphabet) },
        { "regExp" , static_cast<int>(TokType::RegularExpression) },
    });

    auto filename = "vba.abnf";
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
        auto toks = l.Lex(content) | filter([](auto& x) -> bool { return x.Type != TokType::Whitespace and x.Type != TokType::Comment; }) | to<vector<Token<TokType>>>();
        toks.push_back({ .Type = TokType::EOF, .Value = "" }); // add eof
        // add line and word info in toks
        auto checker = Checker();
        auto st = p.Parse<Token<TokType>, shared_ptr<AstNode>>(VectorStream{ .Tokens = move(toks) }, [&checker](auto n) -> void
        {
            AstFactory::Create(&checker, n);
        }); // TODO std::bind(AstFactory::Create, &checker)
        if (st.has_value())
        {
            using std::dynamic_pointer_cast;

            std::println("ast: {}", st.value());
            auto ast = dynamic_pointer_cast<Grammars>(std::get<1>(st.value().Children.front()).Result);
            auto grammarsInfo = GrammarTransformer::Transform(ast.get());
            //std::println("simple grammar: {}", grammarsInfo);
            std::ofstream codeFile{ "vba-spec.ixx" };
            std::print(codeFile, "export module VbaSpec;\n");
            std::print(codeFile, "import std;\n");
            std::print(codeFile, "import Parser;\n");
            std::print(codeFile, "using namespace std;\n");
            std::print(codeFile, "\n");
            std::print(codeFile, "{}\n", CppCodeForm{ .Value = grammarsInfo.Terminals });
            std::print(codeFile, "{}\n", CppCodeForm{ .Value = grammarsInfo.Grammars });
            std::print(codeFile, "{}\n", CppCodeForm{ .Value = grammarsInfo.Terminals });
        }
        checker.Check();
    }
    else
    {
        std::println("open file({}) failed", filename);
    }
    return 0;
}
