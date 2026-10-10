module;
#include <catch2/catch_all.hpp>
export module ParserCombinatorTest;
import std;
import Base;
import Parser;
import Lexer;
import C11Spec;
import ParserCombinator;

using std::vector;
using std::string;
using std::move;
using std::format;
using std::unexpected;
using std::holds_alternative;
using std::get;
using std::nullptr_t;

using Tok = Token<TokType>;
using NodeType = SyntaxTreeNode<Tok, nullptr_t>;
using ParseResult = ParserResult<NodeType>;

static auto MakeTokens(std::initializer_list<Tok> list) -> vector<Tok>
{
    vector<Tok> toks{ list };
    toks.insert(toks.begin(), { .Type = TokType::Punctuator_Semicolon, .Value = ";" }); // consumed by prior parse
    return toks;
}

static auto MakeStream(vector<Tok>& toks) -> VectorStream<Tok>
{
    return VectorStream<Tok>{ .Tokens = move(toks) };
}

// ========== Basic combinators: Nullize, SelectLeft, SelectRight, Id ==========

TEST_CASE("ParserCombinator - Nullize returns nullptr", "[parser-combinator]")
{
    auto result = ParserCombinator::Nullize(TokType::Identifier);
    CHECK(result == nullptr);
}

TEST_CASE("ParserCombinator - SelectLeft picks left", "[parser-combinator]")
{
    auto result = ParserCombinator::SelectLeft(42, 99);
    CHECK(result == 42);
}

TEST_CASE("ParserCombinator - SelectRight picks right", "[parser-combinator]")
{
    auto result = ParserCombinator::SelectRight(42, 99);
    CHECK(result == 99);
}

TEST_CASE("ParserCombinator - Id returns the value", "[parser-combinator]")
{
    auto result = ParserCombinator::Id(42);
    CHECK(result == 42);
}

// ========== ParseWord ==========

// NOTE: ParseWord internally calls MoveNext() first, so the stream must already
// have at least one token consumed before ParseWord is invoked. This matches
// real usage where ParsePrefix reads Current() before calling ParseWord combinators.
// We use a dummy leading token (";") to position the stream correctly.

TEST_CASE("ParserCombinator - ParseWord: matching token type", "[parser-combinator]")
{
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;

    auto parseWord = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
    });
    auto stream = MakeStream(toks);
    auto result = parseWord(stream);
    REQUIRE(result.has_value());
    CHECK(result.value() == nullptr);
    CHECK(stream.Index == 1);
}

TEST_CASE("ParserCombinator - ParseWord: non-matching token type", "[parser-combinator]")
{
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;

    auto parseWord = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "x" },
    });
    auto stream = MakeStream(toks);
    auto result = parseWord(stream);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().Message.find("expect") != string::npos);
}

TEST_CASE("ParserCombinator - ParseWord: empty stream", "[parser-combinator]")
{
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;

    auto parseWord = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto toks = MakeTokens({});
    auto stream = MakeStream(toks);
    auto result = parseWord(stream);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().Message.find("empty") != string::npos);
}

TEST_CASE("ParserCombinator - ParseWord: result process returns custom value", "[parser-combinator]")
{
    using ParserCombinator::ParseWord;

    auto customProcess = [](TokType tt) -> int { return static_cast<int>(tt) * 2; };
    auto parseWord = ParseWord(TokType::Punctuator_LeftParen, customProcess);
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
    });
    auto stream = MakeStream(toks);
    auto result = parseWord(stream);
    REQUIRE(result.has_value());
    CHECK(result.value() == static_cast<int>(TokType::Punctuator_LeftParen) * 2);
}

// ========== Concat ==========

TEST_CASE("ParserCombinator - Concat: two successful parsers", "[parser-combinator]")
{
    using ParserCombinator::Concat;
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;

    auto leftParen = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto rightParen = ParseWord(TokType::Punctuator_RightParen, Nullize);
    auto combiner = [](nullptr_t, nullptr_t) -> int { return 42; };
    auto concat = Concat(combiner, leftParen, rightParen);

    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
    });
    auto stream = MakeStream(toks);
    auto result = concat(stream);
    REQUIRE(result.has_value());
    CHECK(result.value() == 42);
}

TEST_CASE("ParserCombinator - Concat: first parser fails", "[parser-combinator]")
{
    using ParserCombinator::Concat;
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;

    auto leftParen = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto rightParen = ParseWord(TokType::Punctuator_RightParen, Nullize);
    auto combiner = [](nullptr_t, nullptr_t) -> int { return 42; };
    auto concat = Concat(combiner, leftParen, rightParen);

    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "x" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
    });
    auto stream = MakeStream(toks);
    auto result = concat(stream);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("ParserCombinator - Concat: second parser fails", "[parser-combinator]")
{
    using ParserCombinator::Concat;
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;

    auto leftParen = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto rightParen = ParseWord(TokType::Punctuator_RightParen, Nullize);
    auto combiner = [](nullptr_t, nullptr_t) -> int { return 42; };
    auto concat = Concat(combiner, leftParen, rightParen);

    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Identifier, .Value = "x" },
    });
    auto stream = MakeStream(toks);
    auto result = concat(stream);
    REQUIRE_FALSE(result.has_value());
}

// ========== Or ==========

TEST_CASE("ParserCombinator - Or: first parser succeeds", "[parser-combinator]")
{
    using ParserCombinator::Or;
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;
    using ParserCombinator::Id;

    auto leftParen = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto rightParen = ParseWord(TokType::Punctuator_RightParen, Nullize);
    auto p = Or(Id, leftParen, rightParen);

    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
    });
    auto stream = MakeStream(toks);
    auto result = p(stream);
    REQUIRE(result.has_value());
    CHECK(result.value() == nullptr);
}

TEST_CASE("ParserCombinator - Or: second parser succeeds", "[parser-combinator]")
{
    using ParserCombinator::Or;
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;
    using ParserCombinator::Id;

    auto leftParen = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto rightParen = ParseWord(TokType::Punctuator_RightParen, Nullize);
    auto p = Or(Id, leftParen, rightParen);

    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
    });
    auto stream = MakeStream(toks);
    auto result = p(stream);
    REQUIRE(result.has_value());
    CHECK(result.value() == nullptr);
}

TEST_CASE("ParserCombinator - Or: all parsers fail", "[parser-combinator]")
{
    using ParserCombinator::Or;
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;
    using ParserCombinator::Id;

    auto leftParen = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto rightParen = ParseWord(TokType::Punctuator_RightParen, Nullize);
    auto p = Or(Id, leftParen, rightParen);

    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "x" },
    });
    auto stream = MakeStream(toks);
    auto result = p(stream);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().Message.find("all options failed") != string::npos);
}

TEST_CASE("ParserCombinator - Or: rollback on failure", "[parser-combinator]")
{
    using ParserCombinator::Or;
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;
    using ParserCombinator::Id;

    auto leftParen = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto rightParen = ParseWord(TokType::Punctuator_RightParen, Nullize);
    auto p = Or(Id, leftParen, rightParen);

    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
    });
    auto stream = MakeStream(toks);
    auto result = p(stream);
    REQUIRE(result.has_value());
    CHECK(result.value() == nullptr);
}

TEST_CASE("ParserCombinator - Or: three options", "[parser-combinator]")
{
    using ParserCombinator::Or;
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;
    using ParserCombinator::Id;

    auto a = ParseWord(TokType::Keyword_Sizeof, Nullize);
    auto b = ParseWord(TokType::Keyword_Alignof, Nullize);
    auto c = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto p = Or(Id, a, b, c);

    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
    });
    auto stream = MakeStream(toks);
    auto result = p(stream);
    REQUIRE(result.has_value());
    CHECK(result.value() == nullptr);
}

// ========== From ==========

TEST_CASE("ParserCombinator - From: basic usage", "[parser-combinator]")
{
    using ParserCombinator::From;
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;

    auto leftParen = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto from = From(leftParen);

    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
    });
    auto stream = MakeStream(toks);
    auto result = from(stream);
    REQUIRE(result.has_value());
    CHECK(result.value() == nullptr);
}

TEST_CASE("ParserCombinator - From: parser fails", "[parser-combinator]")
{
    using ParserCombinator::From;
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;

    auto leftParen = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto from = From(leftParen);

    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "x" },
    });
    auto stream = MakeStream(toks);
    auto result = from(stream);
    REQUIRE_FALSE(result.has_value());
}

// ========== From.RightWith ==========

TEST_CASE("ParserCombinator - From.RightWith: SelectLeft keeps first result", "[parser-combinator]")
{
    using ParserCombinator::From;
    using ParserCombinator::SelectLeft;
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;

    auto leftParen = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto rightParen = ParseWord(TokType::Punctuator_RightParen, Nullize);
    auto p = From(leftParen).RightWith(rightParen, SelectLeft);

    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
    });
    auto stream = MakeStream(toks);
    auto result = p(stream);
    REQUIRE(result.has_value());
    CHECK(result.value() == nullptr);
}

TEST_CASE("ParserCombinator - From.RightWith: SelectRight keeps second result", "[parser-combinator]")
{
    using ParserCombinator::From;
    using ParserCombinator::SelectRight;
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;

    auto leftParen = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto rightParen = ParseWord(TokType::Punctuator_RightParen, Nullize);
    auto p = From(leftParen).RightWith(rightParen, SelectRight);

    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
    });
    auto stream = MakeStream(toks);
    auto result = p(stream);
    REQUIRE(result.has_value());
    CHECK(result.value() == nullptr);
}

TEST_CASE("ParserCombinator - From.RightWith: custom combiner", "[parser-combinator]")
{
    using ParserCombinator::From;
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;

    auto leftParen = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto rightParen = ParseWord(TokType::Punctuator_RightParen, Nullize);
    auto combiner = [](nullptr_t, nullptr_t) -> string { return "matched"; };
    auto p = From(leftParen).RightWith(rightParen, combiner);

    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
    });
    auto stream = MakeStream(toks);
    auto result = p(stream);
    REQUIRE(result.has_value());
    CHECK(result.value() == "matched");
}

TEST_CASE("ParserCombinator - From.RightWith: first parser fails", "[parser-combinator]")
{
    using ParserCombinator::From;
    using ParserCombinator::SelectLeft;
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;

    auto leftParen = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto rightParen = ParseWord(TokType::Punctuator_RightParen, Nullize);
    auto p = From(leftParen).RightWith(rightParen, SelectLeft);

    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "x" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
    });
    auto stream = MakeStream(toks);
    auto result = p(stream);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("ParserCombinator - From.RightWith: second parser fails", "[parser-combinator]")
{
    using ParserCombinator::From;
    using ParserCombinator::SelectLeft;
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;

    auto leftParen = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto rightParen = ParseWord(TokType::Punctuator_RightParen, Nullize);
    auto p = From(leftParen).RightWith(rightParen, SelectLeft);

    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Identifier, .Value = "x" },
    });
    auto stream = MakeStream(toks);
    auto result = p(stream);
    REQUIRE_FALSE(result.has_value());
}

// ========== Chained From.RightWith (like ParsePrefix patterns) ==========

TEST_CASE("ParserCombinator - Chained RightWith: a .RightWith(b).RightWith(c)", "[parser-combinator]")
{
    using ParserCombinator::From;
    using ParserCombinator::SelectLeft;
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;

    auto leftParen = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto expr = ParseWord(TokType::Identifier, [](TokType) -> int { return 99; });
    auto rightParen = ParseWord(TokType::Punctuator_RightParen, Nullize);
    auto p = From(leftParen).RightWith(expr, ParserCombinator::SelectRight).RightWith(rightParen, SelectLeft);

    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Identifier, .Value = "x" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
    });
    auto stream = MakeStream(toks);
    auto result = p(stream);
    REQUIRE(result.has_value());
    CHECK(result.value() == 99);
}

TEST_CASE("ParserCombinator - Chained RightWith: middle parser fails", "[parser-combinator]")
{
    using ParserCombinator::From;
    using ParserCombinator::SelectLeft;
    using ParserCombinator::SelectRight;
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;

    auto leftParen = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto expr = ParseWord(TokType::Identifier, [](TokType) -> int { return 99; });
    auto rightParen = ParseWord(TokType::Punctuator_RightParen, Nullize);
    auto p = From(leftParen).RightWith(expr, SelectRight).RightWith(rightParen, SelectLeft);

    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
    });
    auto stream = MakeStream(toks);
    auto result = p(stream);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("ParserCombinator - Chained RightWith: last parser fails", "[parser-combinator]")
{
    using ParserCombinator::From;
    using ParserCombinator::SelectLeft;
    using ParserCombinator::SelectRight;
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;

    auto leftParen = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto expr = ParseWord(TokType::Identifier, [](TokType) -> int { return 99; });
    auto rightParen = ParseWord(TokType::Punctuator_RightParen, Nullize);
    auto p = From(leftParen).RightWith(expr, SelectRight).RightWith(rightParen, SelectLeft);

    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Identifier, .Value = "x" },
        { .Type = TokType::Punctuator_Semicolon, .Value = ";" },
    });
    auto stream = MakeStream(toks);
    auto result = p(stream);
    REQUIRE_FALSE(result.has_value());
}

// ========== Or with From.RightWith (like sizeof/alignof pattern in ParsePrefix) ==========

TEST_CASE("ParserCombinator - Or with From.RightWith: first alternative matches", "[parser-combinator]")
{
    using ParserCombinator::From;
    using ParserCombinator::SelectLeft;
    using ParserCombinator::SelectRight;
    using ParserCombinator::Or;
    using ParserCombinator::Id;
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;

    auto leftParen = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto rightParen = ParseWord(TokType::Punctuator_RightParen, Nullize);
    auto expr = ParseWord(TokType::Identifier, [](TokType) -> int { return 77; });
    auto parenExpr = From(leftParen).RightWith(expr, SelectRight).RightWith(rightParen, SelectLeft);
    auto simpleExpr = ParseWord(TokType::IntegerConstant, [](TokType) -> int { return 88; });

    auto p = Or(Id, parenExpr, simpleExpr);

    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Identifier, .Value = "x" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
    });
    auto stream = MakeStream(toks);
    auto result = p(stream);
    REQUIRE(result.has_value());
    CHECK(result.value() == 77);
}

TEST_CASE("ParserCombinator - Or with From.RightWith: second alternative matches", "[parser-combinator]")
{
    using ParserCombinator::From;
    using ParserCombinator::SelectLeft;
    using ParserCombinator::SelectRight;
    using ParserCombinator::Or;
    using ParserCombinator::Id;
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;

    auto leftParen = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto rightParen = ParseWord(TokType::Punctuator_RightParen, Nullize);
    auto expr = ParseWord(TokType::Identifier, [](TokType) -> int { return 77; });
    auto parenExpr = From(leftParen).RightWith(expr, SelectRight).RightWith(rightParen, SelectLeft);
    auto simpleExpr = ParseWord(TokType::IntegerConstant, [](TokType) -> int { return 88; });

    auto p = Or(Id, parenExpr, simpleExpr);

    auto toks = MakeTokens({
        { .Type = TokType::IntegerConstant, .Value = "42" },
    });
    auto stream = MakeStream(toks);
    auto result = p(stream);
    REQUIRE(result.has_value());
    CHECK(result.value() == 88);
}

TEST_CASE("ParserCombinator - Or with From.RightWith: all alternatives fail", "[parser-combinator]")
{
    using ParserCombinator::From;
    using ParserCombinator::SelectLeft;
    using ParserCombinator::SelectRight;
    using ParserCombinator::Or;
    using ParserCombinator::Id;
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;

    auto leftParen = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto rightParen = ParseWord(TokType::Punctuator_RightParen, Nullize);
    auto expr = ParseWord(TokType::Identifier, [](TokType) -> int { return 77; });
    auto parenExpr = From(leftParen).RightWith(expr, SelectRight).RightWith(rightParen, SelectLeft);
    auto simpleExpr = ParseWord(TokType::IntegerConstant, [](TokType) -> int { return 88; });

    auto p = Or(Id, parenExpr, simpleExpr);

    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_Semicolon, .Value = ";" },
    });
    auto stream = MakeStream(toks);
    auto result = p(stream);
    REQUIRE_FALSE(result.has_value());
}

// ========== Or with lambda parsers (like typeName/ExpOf patterns in ParsePrefix) ==========

TEST_CASE("ParserCombinator - Or with lambda parsers", "[parser-combinator]")
{
    using ParserCombinator::Or;
    using ParserCombinator::Id;

    // Each "parser" is a lambda that checks the current token
    auto parseA = [](auto& stream) -> ParserResult<int>
    {
        if (not stream.MoveNext())
            return unexpected(ParseFailResult{ .Message = "empty" });
        if (stream.Current().Type == TokType::Identifier)
            return 1;
        return unexpected(ParseFailResult{ .Message = "not identifier" });
    };
    auto parseB = [](auto& stream) -> ParserResult<int>
    {
        if (not stream.MoveNext())
            return unexpected(ParseFailResult{ .Message = "empty" });
        if (stream.Current().Type == TokType::IntegerConstant)
            return 2;
        return unexpected(ParseFailResult{ .Message = "not integer" });
    };
    auto p = Or(Id, parseA, parseB);

    SECTION("first matches")
    {
        auto toks = MakeTokens({
            { .Type = TokType::Identifier, .Value = "x" },
        });
        auto stream = MakeStream(toks);
        auto result = p(stream);
        REQUIRE(result.has_value());
        CHECK(result.value() == 1);
    }

    SECTION("second matches")
    {
        auto toks = MakeTokens({
            { .Type = TokType::IntegerConstant, .Value = "42" },
        });
        auto stream = MakeStream(toks);
        auto result = p(stream);
        REQUIRE(result.has_value());
        CHECK(result.value() == 2);
    }

    SECTION("neither matches")
    {
        auto toks = MakeTokens({
            { .Type = TokType::Punctuator_Semicolon, .Value = ";" },
        });
        auto stream = MakeStream(toks);
        auto result = p(stream);
        REQUIRE_FALSE(result.has_value());
    }
}

// ========== Or with three lambda parsers (like sizeof/alignof pattern) ==========

TEST_CASE("ParserCombinator - Or with three lambda parsers (Id/paren/expr pattern)", "[parser-combinator]")
{
    using ParserCombinator::From;
    using ParserCombinator::SelectLeft;
    using ParserCombinator::SelectRight;
    using ParserCombinator::Or;
    using ParserCombinator::Id;
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;

    auto leftParen = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto rightParen = ParseWord(TokType::Punctuator_RightParen, Nullize);
    auto innerA = ParseWord(TokType::Identifier, [](TokType) -> int { return 10; });
    auto innerB = ParseWord(TokType::Keyword_Int, [](TokType) -> int { return 20; });
    auto innerOr = Or(Id, innerA, innerB);
    auto parenPattern = From(leftParen).RightWith(innerOr, SelectRight).RightWith(rightParen, SelectLeft);
    auto fallback = ParseWord(TokType::IntegerConstant, [](TokType) -> int { return 30; });

    auto p = Or(Id, parenPattern, fallback);

    SECTION("paren with identifier inside")
    {
        auto toks = MakeTokens({
            { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
            { .Type = TokType::Identifier, .Value = "x" },
            { .Type = TokType::Punctuator_RightParen, .Value = ")" },
        });
        auto stream = MakeStream(toks);
        auto result = p(stream);
        REQUIRE(result.has_value());
        CHECK(result.value() == 10);
    }

    SECTION("paren with keyword inside")
    {
        auto toks = MakeTokens({
            { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
            { .Type = TokType::Keyword_Int, .Value = "int" },
            { .Type = TokType::Punctuator_RightParen, .Value = ")" },
        });
        auto stream = MakeStream(toks);
        auto result = p(stream);
        REQUIRE(result.has_value());
        CHECK(result.value() == 20);
    }

    SECTION("fallback (integer constant)")
    {
        auto toks = MakeTokens({
            { .Type = TokType::IntegerConstant, .Value = "42" },
        });
        auto stream = MakeStream(toks);
        auto result = p(stream);
        REQUIRE(result.has_value());
        CHECK(result.value() == 30);
    }

    SECTION("nothing matches")
    {
        auto toks = MakeTokens({
            { .Type = TokType::Punctuator_Semicolon, .Value = ";" },
        });
        auto stream = MakeStream(toks);
        auto result = p(stream);
        REQUIRE_FALSE(result.has_value());
    }
}

// ========== Or with custom resultProcess (like BindOpCons pattern) ==========

TEST_CASE("ParserCombinator - Or with custom resultProcess", "[parser-combinator]")
{
    using ParserCombinator::Or;
    using ParserCombinator::ParseWord;
    using ParserCombinator::Nullize;

    auto process = [](nullptr_t) -> string { return "processed"; };
    auto leftParen = ParseWord(TokType::Punctuator_LeftParen, Nullize);
    auto rightParen = ParseWord(TokType::Punctuator_RightParen, Nullize);
    auto p = Or(process, leftParen, rightParen);

    SECTION("left paren processed")
    {
        auto toks = MakeTokens({
            { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        });
        auto stream = MakeStream(toks);
        auto result = p(stream);
        REQUIRE(result.has_value());
        CHECK(result.value() == "processed");
    }

    SECTION("right paren processed")
    {
        auto toks = MakeTokens({
            { .Type = TokType::Punctuator_RightParen, .Value = ")" },
        });
        auto stream = MakeStream(toks);
        auto result = p(stream);
        REQUIRE(result.has_value());
        CHECK(result.value() == "processed");
    }
}
