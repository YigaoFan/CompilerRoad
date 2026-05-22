module;
#include <catch2/catch_all.hpp>
export module C11LexerTest;
import std;
import Base;
import Lexer;
import C11Spec;

using std::vector;
using std::string;
using std::string_view;
using std::array;
using std::pair;
using std::move;
using std::ranges::views::filter;
using std::ranges::to;

static auto Lex(string_view code) -> vector<Token<TokType>>
{
    auto l = Lexer<TokType>::New(lexRules);
    return l.Lex(code)
        | filter([](auto& x) { return x.Type != TokType::Whitespace && x.Type != TokType::Newline && x.Type != TokType::Comment; })
        | to<vector<Token<TokType>>>();
}

// Lex without filtering (for testing individual token types)
static auto LexAll(string_view code) -> vector<Token<TokType>>
{
    auto l = Lexer<TokType>::New(lexRules);
    return l.Lex(code)
        | filter([](auto& x) { return x.Type != TokType::Whitespace && x.Type != TokType::Newline; })
        | to<vector<Token<TokType>>>();
}

TEST_CASE("C11 Lexer - Keywords", "[lexer]")
{
    auto toks = Lex("int");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Keyword_Int);
    REQUIRE(toks[0].Value == "int");

    toks = Lex("return");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Keyword_Return);
    REQUIRE(toks[0].Value == "return");

    toks = Lex("void");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Keyword_Void);

    toks = Lex("_Atomic");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Keyword__Atomic);

    toks = Lex("_Static_assert");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Keyword__Static_Assert);

    toks = Lex("unsigned long long int");
    REQUIRE(toks.size() == 4);
    REQUIRE(toks[0].Type == TokType::Keyword_Unsigned);
    REQUIRE(toks[1].Type == TokType::Keyword_Long);
    REQUIRE(toks[2].Type == TokType::Keyword_Long);
    REQUIRE(toks[3].Type == TokType::Keyword_Int);
}

TEST_CASE("C11 Lexer - Identifiers", "[lexer]")
{
    auto toks = Lex("main");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Identifier);
    REQUIRE(toks[0].Value == "main");

    toks = Lex("_foo");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Identifier);
    REQUIRE(toks[0].Value == "_foo");

    toks = Lex("x123");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Identifier);
    REQUIRE(toks[0].Value == "x123");

    toks = Lex("_myVar");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Identifier);
}

TEST_CASE("C11 Lexer - Constants", "[lexer]")
{
    SECTION("Integer constants")
    {
        auto toks = Lex("0");
        REQUIRE(toks.size() == 1);
        REQUIRE(toks[0].Type == TokType::Constant);

        toks = Lex("42");
        REQUIRE(toks.size() == 1);
        REQUIRE(toks[0].Type == TokType::Constant);

        toks = Lex("0xFF");
        REQUIRE(toks.size() == 1);
        REQUIRE(toks[0].Type == TokType::Constant);

        toks = Lex("077");
        REQUIRE(toks.size() == 1);
        REQUIRE(toks[0].Type == TokType::Constant);
    }

    SECTION("Floating-point constants")
    {
         auto toks = Lex("3.14");
         REQUIRE(toks.size() == 1);
         REQUIRE(toks[0].Type == TokType::Constant);

        toks = Lex("42");
         REQUIRE(toks.size() == 1);
         REQUIRE(toks[0].Type == TokType::Constant);

        toks = Lex("1e10");
        REQUIRE(toks.size() == 1);
        REQUIRE(toks[0].Type == TokType::Constant);

		toks = Lex("1.0e-5");
		REQUIRE(toks.size() == 1);
    }

    SECTION("Character constants")
    {
        auto toks = Lex("'a'");
        REQUIRE(toks.size() == 1);
        REQUIRE(toks[0].Type == TokType::Constant);

        toks = Lex("'\\n'");
        REQUIRE(toks.size() == 1);
        REQUIRE(toks[0].Type == TokType::Constant);
    }
}

TEST_CASE("C11 Lexer - String Literals", "[lexer]")
{
    auto toks = Lex("\"hello\"");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::StringLiteral);

    toks = Lex("u8\"str\"");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::StringLiteral);

    toks = Lex("L\"wide\"");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::StringLiteral);

    toks = Lex("\"escaped\\\"quote\"");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::StringLiteral);
}

TEST_CASE("C11 Lexer - Punctuators", "[lexer]")
{
    auto toks = Lex("(");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Punctuator_LeftParen);

    toks = Lex(")");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Punctuator_RightParen);

    toks = Lex("{");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Punctuator_LeftBrace);

    toks = Lex("}");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Punctuator_RightBrace);

    toks = Lex("->");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Punctuator_Arrow);

    toks = Lex("++");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Punctuator_Increment);

    toks = Lex("==");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Punctuator_Equal);

    toks = Lex("...");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Punctuator_Ellipsis);

    toks = Lex(";");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Punctuator_Semicolon);

    toks = Lex(",");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Punctuator_Comma);

    toks = Lex("=");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Punctuator_Assign);

    toks = Lex("*");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Punctuator_Star);

    toks = Lex("[");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Punctuator_LeftBracket);

    toks = Lex("]");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Punctuator_RightBracket);
}

TEST_CASE("C11 Lexer - Comments", "[lexer]")
{
    auto toks = LexAll("// this is a comment");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Comment);

    toks = LexAll("// comment with special chars: @#$%");
    REQUIRE(toks.size() == 1);
    REQUIRE(toks[0].Type == TokType::Comment);
}

TEST_CASE("C11 Lexer - Combined Code Snippets", "[lexer]")
{
    SECTION("Basic function")
    {
        auto toks = Lex("int main() { return 0; }");
        REQUIRE(toks.size() == 9);
        REQUIRE(toks[0].Type == TokType::Keyword_Int);     // int
        REQUIRE(toks[0].Value == "int");
        REQUIRE(toks[1].Type == TokType::Identifier);      // main
        REQUIRE(toks[1].Value == "main");
        REQUIRE(toks[2].Type == TokType::Punctuator_LeftParen);  // (
        REQUIRE(toks[3].Type == TokType::Punctuator_RightParen); // )
        REQUIRE(toks[4].Type == TokType::Punctuator_LeftBrace);  // {
        REQUIRE(toks[5].Type == TokType::Keyword_Return);  // return
        REQUIRE(toks[5].Value == "return");
        REQUIRE(toks[6].Type == TokType::Constant);        // 0
        REQUIRE(toks[7].Type == TokType::Punctuator_Semicolon);  // ;
        REQUIRE(toks[8].Type == TokType::Punctuator_RightBrace); // }
    }

    SECTION("Compound type specifier")
    {
        auto toks = Lex("unsigned long long int x = 42;");
        REQUIRE(toks.size() == 8);
        REQUIRE(toks[0].Type == TokType::Keyword_Unsigned);
        REQUIRE(toks[0].Value == "unsigned");
        REQUIRE(toks[1].Type == TokType::Keyword_Long);
        REQUIRE(toks[1].Value == "long");
        REQUIRE(toks[2].Type == TokType::Keyword_Long);
        REQUIRE(toks[2].Value == "long");
        REQUIRE(toks[3].Type == TokType::Keyword_Int);
        REQUIRE(toks[3].Value == "int");
        REQUIRE(toks[4].Type == TokType::Identifier);
        REQUIRE(toks[4].Value == "x");
        REQUIRE(toks[5].Type == TokType::Punctuator_Assign); // =
        REQUIRE(toks[6].Type == TokType::Constant);           // 42
        REQUIRE(toks[7].Type == TokType::Punctuator_Semicolon); // ;
    }

    SECTION("Struct definition")
    {
        auto toks = Lex("struct Point { int x; int y; };");
        REQUIRE(toks.size() == 11);
        REQUIRE(toks[0].Type == TokType::Keyword_Struct);     // struct
        REQUIRE(toks[1].Type == TokType::Identifier);          // Point
        REQUIRE(toks[2].Type == TokType::Punctuator_LeftBrace); // {
        REQUIRE(toks[3].Type == TokType::Keyword_Int);         // int
        REQUIRE(toks[4].Type == TokType::Identifier);          // x
        REQUIRE(toks[5].Type == TokType::Punctuator_Semicolon); // ;
        REQUIRE(toks[6].Type == TokType::Keyword_Int);         // int
        REQUIRE(toks[7].Type == TokType::Identifier);          // y
        REQUIRE(toks[8].Type == TokType::Punctuator_Semicolon); // ;
        REQUIRE(toks[9].Type == TokType::Punctuator_RightBrace); // }
        REQUIRE(toks[10].Type == TokType::Punctuator_Semicolon); // ;
    }

    SECTION("Enum definition")
    {
        auto toks = Lex("enum Color { RED, GREEN, BLUE };");
        REQUIRE(toks.size() == 10);
        REQUIRE(toks[0].Type == TokType::Keyword_Enum);      // enum
        REQUIRE(toks[1].Type == TokType::Identifier);         // Color
        REQUIRE(toks[2].Type == TokType::Punctuator_LeftBrace); // {
        REQUIRE(toks[3].Type == TokType::Identifier);         // RED
        REQUIRE(toks[4].Type == TokType::Punctuator_Comma);   // ,
        REQUIRE(toks[5].Type == TokType::Identifier);         // GREEN
        REQUIRE(toks[6].Type == TokType::Punctuator_Comma);   // ,
        REQUIRE(toks[7].Type == TokType::Identifier);         // BLUE
        REQUIRE(toks[8].Type == TokType::Punctuator_RightBrace); // }
        REQUIRE(toks[9].Type == TokType::Punctuator_Semicolon); // ;
    }

    SECTION("Function pointer declaration")
    {
        auto toks = Lex("int (*fp)(int, int);");
        REQUIRE(toks.size() == 11);
        REQUIRE(toks[0].Type == TokType::Keyword_Int);     // int
        REQUIRE(toks[1].Type == TokType::Punctuator_LeftParen);  // (
        REQUIRE(toks[2].Type == TokType::Punctuator_Star);       // *
        REQUIRE(toks[3].Type == TokType::Identifier);       // fp
        REQUIRE(toks[4].Type == TokType::Punctuator_RightParen); // )
        REQUIRE(toks[5].Type == TokType::Punctuator_LeftParen);  // (
        REQUIRE(toks[6].Type == TokType::Keyword_Int);     // int
        REQUIRE(toks[7].Type == TokType::Punctuator_Comma);      // ,
        REQUIRE(toks[8].Type == TokType::Keyword_Int);     // int
        REQUIRE(toks[9].Type == TokType::Punctuator_RightParen); // )
        REQUIRE(toks[10].Type == TokType::Punctuator_Semicolon); // ;
    }

    SECTION("Multiple qualifiers")
    {
        auto toks = Lex("const volatile int * const p;");
        REQUIRE(toks.size() == 7);
        REQUIRE(toks[0].Type == TokType::Keyword_Const);    // const
        REQUIRE(toks[1].Type == TokType::Keyword_Volatile);  // volatile
        REQUIRE(toks[2].Type == TokType::Keyword_Int);       // int
        REQUIRE(toks[3].Type == TokType::Punctuator_Star);   // *
        REQUIRE(toks[4].Type == TokType::Keyword_Const);     // const
        REQUIRE(toks[5].Type == TokType::Identifier);        // p
        REQUIRE(toks[6].Type == TokType::Punctuator_Semicolon); // ;
    }

    SECTION("Atomic type specifier")
    {
        auto toks = Lex("_Atomic(struct S) a;");
        REQUIRE(toks.size() == 7);
        REQUIRE(toks[0].Type == TokType::Keyword__Atomic);   // _Atomic
        REQUIRE(toks[1].Type == TokType::Punctuator_LeftParen); // (
        REQUIRE(toks[2].Type == TokType::Keyword_Struct);     // struct
        REQUIRE(toks[3].Type == TokType::Identifier);         // S
        REQUIRE(toks[4].Type == TokType::Punctuator_RightParen); // )
        REQUIRE(toks[5].Type == TokType::Identifier);         // a
        REQUIRE(toks[6].Type == TokType::Punctuator_Semicolon); // ;
    }
}
