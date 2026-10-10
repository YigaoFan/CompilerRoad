import std;
import Lexer;
import Parser;
import TokType;
import Ast;
import Transformer;
import Checker;
import CodeEmitter; // affect the bottom format code
import Parser;
import GrammarSetup;

using std::string;
using std::pair;
using std::vector;
using std::size_t;
using std::shared_ptr;
using std::map;
using std::format;
using std::move;

void TerminateHandler()
{
    std::cerr << "Terminate called after throwing an instance of an exception.\n";
    std::cerr << "Stacktrace at crash:\n";
    std::cerr << std::stacktrace::current() << '\n';
    std::abort();
}

int main(int argc, char* argv[])
{
	std::set_terminate(TerminateHandler);

    using std::ranges::views::filter;
    using std::ranges::to;

    if (argc < 2)
    {
        std::println("usage: ABNF2SimpleGrammar <abnf-filename> [spec-output-path] [--debug] [--check-conflicts]");
        return 1;
    }
    auto abnfPath = std::filesystem::path(argv[1]);
    if (!std::filesystem::exists(abnfPath))
    {
        std::println("file not found: {}", argv[1]);
        return 1;
    }
    auto enableDebug = false;
    auto enableCheckConflicts = false;
    auto language = abnfPath.stem().generic_string();
    auto specPath = abnfPath.parent_path() / (language + "-spec.ixx");
    for (int i = 2; i < argc; ++i)
    {
		auto arg = std::string_view(argv[i]);
        if (arg == "--debug")
        {
            enableDebug = true;
        }
        else if (arg == "--check-conflicts")
        {
            enableCheckConflicts = true;
        }
        else
        {
            specPath = argv[i];
        }
    }
    std::println("abnf file: {}, spec output file: {}, debug: {}", abnfPath.generic_string(), specPath.generic_string(), enableDebug);
    auto astPath = specPath.parent_path() / (language + "-ast.ixx");
    auto l = BuildLexer();
	auto p = BuildParser();

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
        auto checker = DefinitionChecker();
        auto st = p.Parse<shared_ptr<AstNode>>(VectorStream{ .Tokens = move(toks) }, [&checker](auto n) -> void
        {
            AstFactory::Create(&checker, n);
        }); // TODO std::bind(AstFactory::Create, &checker)
        if (st.has_value())
        {
            std::println("parse done");
            using std::dynamic_pointer_cast;

            //std::println("ast: {}", st.value());
            auto tokRefChecker = PrivateTokenRefChecker();
            auto starArrowChecker = StarArrowChecker();
            auto ast = dynamic_pointer_cast<AllGrammars>(std::get<1>(st.value().Children.front()).Result);
            std::ofstream astDefFile{ astPath };
            AstGenerator::GenerateFrom(ast->ParseRules.get(), astDefFile);
            starArrowChecker.CheckLexRules(ast->LexRules.get());
            starArrowChecker.CheckParseRules(ast->ParseRules.get());
            tokRefChecker.Check(ast.get());
            auto grammarsInfo = ParseRule2SimpleGrammarTransformer::Transform(ast->ParseRules.get());
            auto terminals = LexRule2RegExpTransformer::MergeTokInfo(LexRule2RegExpTransformer::Transform(ast->LexRules.get(), enableDebug), move(grammarsInfo.ToksInfo));

			std::ofstream codeFile{ specPath };
			std::print(codeFile, "export module {}Spec;\n", language);
			std::print(codeFile, "\n");
			std::print(codeFile, "import std;\n");
			std::print(codeFile, "import Parser;\n");
			std::print(codeFile, "using namespace std;\n");
			std::print(codeFile, "\n");
			std::print(codeFile, "{}\n", CppCodeForm{ .Value = terminals });
			std::print(codeFile, "{}\n", CppCodeForm{ .Value = grammarsInfo.Grammars });
			std::print(codeFile, "export ParseInfo parseInfo =\n");
			std::print(codeFile, "{{\n");
			std::print(codeFile, "   .Grammars = grammars,\n");
			std::print(codeFile, "   .Terminal2IntTokenType = terminal2IntTokenType,\n");
			std::print(codeFile, "}};");
			std::println("code generate done");

            if (enableCheckConflicts)
            {
                using std::make_move_iterator;
                std::println("start with {}, conflicts detected:\n", ast->StartSymbolOfParseRules);
                // calculate the start symbol of grammar, assume there exists the main symbol
                auto conflicts = DetectConflicts(ast->StartSymbolOfParseRules, map{ make_move_iterator(grammarsInfo.Grammars.begin()), make_move_iterator(grammarsInfo.Grammars.end()) });
                std::println("{}", conflicts);
            }
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
