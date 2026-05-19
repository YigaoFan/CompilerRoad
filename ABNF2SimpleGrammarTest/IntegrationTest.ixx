module;
#include <catch2/catch_all.hpp>
#undef eof
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

using std::vector;
using std::string;
using std::string_view;
using std::shared_ptr;
using std::pair;
using std::map;
using std::move;
using std::dynamic_pointer_cast;
using std::ranges::views::filter;
using std::ranges::to;

static auto BuildLexerAndParser() -> pair<Lexer<TokType>, LLParser>
{
    std::array rules =
    {
        pair<TokType, string>{ TokType::StarArrow, "\\-\\*\\->" },
        pair<TokType, string>{ TokType::Arrow, "\\->" },
        pair<TokType, string>{ TokType::At, "@" },
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
    {
        { "all-grammars", {
            { "lex-header", "grammars", "parse-header", "grammars" },
            { },
        }},
        { "grammars", {
            { "optional-newlines", "grammar", "more-grammars", },
            { },
        }},
        { "more-grammars", {
            { "newlines", "grammar", "more-grammars"},
            { "newlines" },
            { },
        }},
        { "grammar", {
            { "sym", "arrow", "productions" },
        }},
        { "arrow", {
            { "->" },
            { "-*->" },
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
        { "production", {
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
            { "terminal", "@", "sym" },
            { "terminal" },
            { "sym" },
            { "digitOrAlphabet", "-", "digitOrAlphabet" },
            { "[", "productions", "]" },
            { "(", "productions", ")" },
        }},
    },
    {
        { "->", static_cast<int>(TokType::Arrow) },
        { "-*->", static_cast<int>(TokType::StarArrow) },
        { "@", static_cast<int>(TokType::At) },
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
        { "\0", static_cast<int>(TokType::EOF) },
        { "newline" , static_cast<int>(TokType::Newline) },
        { "terminal" , static_cast<int>(TokType::Terminal) },
        { "digitOrAlphabet" , static_cast<int>(TokType::QutotedDigitOrAlphabet) },
        { "regExp" , static_cast<int>(TokType::RegularExpression) },
        { "lex-header" , static_cast<int>(TokType::LexRuleHeader) },
        { "parse-header" , static_cast<int>(TokType::ParseRuleHeader) },
    });
    return { move(l), move(p) };
}

static auto ParseABNF(Lexer<TokType>& l, LLParser& p, string_view abnfText) -> shared_ptr<AllGrammars>
{
    auto toks = l.Lex(abnfText)
        | filter([](auto& x) -> bool { return x.Type != TokType::Whitespace and x.Type != TokType::Comment; })
        | to<vector<Token<TokType>>>();
    toks.push_back({ .Type = TokType::EOF, .Value = "" });

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
    auto [l, p] = BuildLexerAndParser();
    auto abnf = R"(- Lex -
Keyword -*-> "if" | "else" | "while"

- Parse -
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
    auto [l, p] = BuildLexerAndParser();
    auto abnf = R"(- Lex -
Punctuator -*-> "==" @Equal | "!=" @NotEqual | "<" @Less | ">" @Greater

- Parse -
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
    auto [l, p] = BuildLexerAndParser();
    auto abnf = R"(- Lex -
Type -> "int"

- Parse -
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
