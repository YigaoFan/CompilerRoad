module;
#include <catch2/catch_all.hpp>
#undef EOF
export module IntegrationTest;
import std;
import Base;
import Lexer;
import Parser;
import TokType;
import Ast;
import Transformer;
import Checker;
import GrammarSetup;

using std::vector;
using std::string_view;
using std::shared_ptr;
using std::move;
using std::dynamic_pointer_cast;
using std::ranges::views::filter;
using std::ranges::to;

static auto ParseABNF(Lexer<TokType>& l, LLParser& p, string_view abnfText) -> shared_ptr<AllGrammars>
{
    auto toks = l.Lex(abnfText)
        | filter([](auto& x) -> bool { return x.Type != TokType::Whitespace and x.Type != TokType::Comment; })
        | to<vector<Token<TokType>>>();
    toks.push_back({ .Type = TokType::EOF, .Value = "EOF" });

    auto checker = DefinitionChecker();
    auto st = p.Parse<shared_ptr<AstNode>>(VectorStream{ .Tokens = move(toks) }, [&checker](auto n)
    {
        AstFactory::Create(&checker, n);
    });
    REQUIRE(st.has_value());
    auto ast = dynamic_pointer_cast<AllGrammars>(std::get<1>(st.value().Children.front()).Result);
    REQUIRE(ast != nullptr);
    return ast;
}

TEST_CASE("Integration - Simple lex rules", "[integration]")
{
    auto l = BuildLexer();
    auto p = BuildParser();
    auto abnf = R"(- Lex -
Keyword -*-> "if" | "else" | "while"

- Parse -
@start stmt EOF
stmt -> sym
)";
    auto ast = ParseABNF(l, p, abnf);

    REQUIRE(ast->LexRules != nullptr);
    REQUIRE(ast->LexRules->Items.size() == 1);
    REQUIRE(ast->LexRules->Items[0]->IsStarArrow == true);

    REQUIRE(ast->ParseRules != nullptr);
    REQUIRE(ast->ParseRules->Items.size() == 1);
    REQUIRE(ast->ParseRules->Items[0]->IsStarArrow == false);

    auto tokInfo = LexRule2RegExpTransformer::Transform(ast->LexRules.get());
    // Star arrow with lowercase literals: now preserved thanks to Generated flag
    REQUIRE(tokInfo.Symbol2EnumNameRegExp.size() == 3);
    REQUIRE(tokInfo.PrioritySymbols.size() == 3);
}

TEST_CASE("Integration - Alias syntax @", "[integration]")
{
    auto l = BuildLexer();
    auto p = BuildParser();
    auto abnf = R"(- Lex -
Punctuator -*-> "==" @Equal | "!=" @NotEqual | "<" @Less | ">" @Greater

- Parse -
@start expr EOF
expr -> sym
)";
    auto ast = ParseABNF(l, p, abnf);

    auto tokInfo = LexRule2RegExpTransformer::Transform(ast->LexRules.get());
    REQUIRE(tokInfo.Symbol2EnumNameRegExp.size() == 4);

    auto eqIt = tokInfo.Symbol2EnumNameRegExp.find(String("=="));
    REQUIRE(eqIt != tokInfo.Symbol2EnumNameRegExp.end());
    REQUIRE(string_view(eqIt->second.EnumName) == "Punctuator_Equal");

    auto neIt = tokInfo.Symbol2EnumNameRegExp.find(String("!="));
    REQUIRE(neIt != tokInfo.Symbol2EnumNameRegExp.end());
    REQUIRE(string_view(neIt->second.EnumName) == "Punctuator_NotEqual");
}

TEST_CASE("Integration - Full pipeline with lex and parse", "[integration]")
{
    auto l = BuildLexer();
    auto p = BuildParser();
    auto abnf = R"(- Lex -
Type -> "int"

- Parse -
@start decl EOF
decl -> Type sym
)";
    auto ast = ParseABNF(l, p, abnf);

    REQUIRE(ast->LexRules != nullptr);
    REQUIRE(ast->ParseRules != nullptr);

    auto starDefinitionChecker = StarArrowChecker();
    starDefinitionChecker.CheckLexRules(ast->LexRules.get());
    starDefinitionChecker.CheckParseRules(ast->ParseRules.get());

    auto tokRefDefinitionChecker = PrivateTokenRefChecker();
    tokRefDefinitionChecker.Check(ast.get());

    auto grammarsInfo = ParseRule2SimpleGrammarTransformer::Transform(ast->ParseRules.get());
    REQUIRE(grammarsInfo.Grammars.size() >= 1);
    REQUIRE(grammarsInfo.Grammars[0].first == "decl");
}
