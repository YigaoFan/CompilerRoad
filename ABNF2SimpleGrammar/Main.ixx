import std;
import Lexer;
import Parser;
import TokType;
import Ast;
import Transformer;
import Checker;
import CodeEmitter; // affect the bottom format code
import Parser;

using std::string;
using std::pair;
using std::vector;
using std::size_t;
using std::shared_ptr;
using std::map;
using std::format;
using std::move;

int main(int argc, char* argv[])
{
    using std::ranges::views::filter;
    using std::ranges::to;

    if (argc < 2)
    {
        std::println("usage: ABNF2SimpleGrammar <abnf-filename> [spec-output-path]");
        return 1;
    }
    auto abnfPath = std::filesystem::path(argv[1]);
    if (!std::filesystem::exists(abnfPath))
    {
        std::println("file not found: {}", argv[1]);
        return 1;
    }
    auto language = abnfPath.stem().generic_string();
    auto specPath = argc >= 3
        ? std::filesystem::path(argv[2])
        : abnfPath.parent_path() / (language + "-spec.ixx");
    auto astPath = specPath.parent_path() / (language + "-ast.ixx");

    std::array rules =
    {
        pair<TokType, string>{ TokType::Arrow, "\\->" },
        pair<TokType, string>{ TokType::LeftAngle, "<" },
        pair<TokType, string>{ TokType::RightAngle, ">" },
        pair<TokType, string>{ TokType::LeftSquare, "\\[" },
        pair<TokType, string>{ TokType::RightSquare, "\\]" },
        pair<TokType, string>{ TokType::LeftBracket, "{" },
        pair<TokType, string>{ TokType::RightBracket, "}" },
        pair<TokType, string>{ TokType::StarMark, "\\*" },
        pair<TokType, string>{ TokType::PipeMark, "\n?( |\t)*\\|" },
        pair<TokType, string>{ TokType::Whitespace, " |\t" },
        pair<TokType, string>{ TokType::Symbol, "[a-zA-Z][a-zA-Z0-9_\\-]*" },
        pair<TokType, string>{ TokType::Number, "[1-9][0-9]*" },
        pair<TokType, string>{ TokType::Comment, ";[^\n]*" },
        pair<TokType, string>{ TokType::Hyphen, "\\-" },
        pair<TokType, string>{ TokType::LeftParen, "\\(" },
        pair<TokType, string>{ TokType::RightParen, "\\)" },
        pair<TokType, string>{ TokType::Newline, "\n" },
        pair<TokType, string>{ TokType::Terminal, "\"((\\\\[^\n])|[^\\\\\"\n])*\"" },
        pair<TokType, string>{ TokType::QutotedDigitOrAlphabet, "'[a-zA-Z0-9]'" },
        pair<TokType, string>{ TokType::RegularExpression, "r\"((\\\\[^\n])|[^\\\\\"\n])*\"" },
        pair<TokType, string>{ TokType::LexRuleHeader, "\\- Lex \\-\n" },
        pair<TokType, string>{ TokType::ParseRuleHeader, "\\- Parse \\-\n" },
    };
    auto l = Lexer<TokType>::New(rules);
    auto p = LLParser::ConstructFrom("all-grammars",
    {// how to represent empty in current grammar
        { "all-grammars", {
            { "lex-header", "grammars", "parse-header", "grammars" },
            { },
        }},
        { "grammars", { // cannot place newlines if lex or parse part is empty
            { "optional-newlines", "grammar", "more-grammars", },
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
            { "|", "productions" },
            { },
        }},
        { "production", { // once production occurs, it means not empty, so production doesn't have {} right side
            { "item", "more-items" }, // use () to represent empty
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
        { "lex-header" , static_cast<int>(TokType::LexRuleHeader) },
        { "parse-header" , static_cast<int>(TokType::ParseRuleHeader) },
    });

    std::ifstream file(abnfPath);
    string content;
    if (file.is_open())
    {
        content.reserve(std::filesystem::file_size(abnfPath));
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
        auto st = p.Parse<shared_ptr<AstNode>>(VectorStream{ .Tokens = move(toks) }, [&checker](auto n) -> void
        {
            AstFactory::Create(&checker, n);
        }); // TODO std::bind(AstFactory::Create, &checker)
        if (st.has_value())
        {
            using std::dynamic_pointer_cast;

            //std::println("ast: {}", st.value());
            auto tokRefChecker = PrivateTokenRefChecker();
            auto ast = dynamic_pointer_cast<AllGrammars>(std::get<1>(st.value().Children.front()).Result);
            std::ofstream astDefFile{ astPath };
            AstGenerator::GenerateFrom(ast->ParseRules.get(), astDefFile);
            tokRefChecker.Check(ast.get());
            auto grammarsInfo = ParseRule2SimpleGrammarTransformer::Transform(ast->ParseRules.get());

            //auto starts = Starts("", { grammarsInfo.Grammars.begin(), grammarsInfo.Grammars.end() });
            //map<pair<String, String>, vector<int>> conflicts;
            //for (auto i = 0; auto const& g : grammarsInfo.Grammars)
            //{
            //    auto const& symbol = g.first;
            //    auto const& start = starts.at(i);
            //    if (start.size() != g.second.size())
            //    {
            //        throw std::logic_error("start set for rules size is not same as the grammar rules");
            //    }
            //    for (auto j = 0; j < start.size(); ++j)
            //    {
            //        for (auto const& termin : start.at(j))
            //        {
            //            conflicts[pair{ symbol, String(termin) }].push_back(j);
            //        }
            //    }
            //    ++i;
            //}
            //vector<pair<String, String>> toRemoves;
            //for (auto const& x : conflicts)
            //{
            //    if (x.second.size() <= 1)
            //    {
            //        toRemoves.push_back(x.first);
            //    }
            //}
            //for (auto const& x : toRemoves)
            //{
            //    conflicts.erase(x);
            //}
            //auto leftFactoredGrammar = DeepLeftFactor(conflicts, { grammarsInfo.Grammars.begin(), grammarsInfo.Grammars.end() });

            //std::println("simple grammar: {}", grammarsInfo);
            std::ofstream codeFile{ specPath };
            std::print(codeFile, "export module {}Spec;\n", language);
            std::print(codeFile, "\n");
            std::print(codeFile, "import std;\n");
            std::print(codeFile, "import Parser;\n");
            std::print(codeFile, "using namespace std;\n");
            std::print(codeFile, "\n");
            auto terminals = LexRule2RegExpTransformer::MergeTokInfo(LexRule2RegExpTransformer::Transform(ast->LexRules.get()), move(grammarsInfo.ToksInfo));
            std::print(codeFile, "{}\n", CppCodeForm{ .Value = terminals });
            std::print(codeFile, "{}\n", CppCodeForm{ .Value = grammarsInfo.Grammars });
            std::print(codeFile, "export ParseInfo parseInfo =\n");
            std::print(codeFile, "{{\n");
            std::print(codeFile, "   .Grammars = grammars,\n");
            std::print(codeFile, "   .Terminal2IntTokenType = terminal2IntTokenType,\n");
            std::print(codeFile, "}};");
            std::println("code generate done");
        }
        else
        {
			std::println("parse failed: {}", st.error().Message);
        }
        checker.Check();
    }
    else
    {
        std::println("open file({}) failed", argv[1]);
    }
    return 0;
}
