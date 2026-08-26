module;
#include <catch2/catch_all.hpp>
export module ExpressionParserTest;
import std;
import Base;
import Lexer;
import Parser;
import C11Spec;
import ExpressionParser;

using std::vector;
using std::string;
using std::move;
using std::format;
using std::get;
using std::ranges::views::filter;
using std::ranges::to;

using Tok = Token<TokType>;
using NodeType = SyntaxTreeNode<Tok, void>;
using ParseResult = ParserResult<NodeType>;

static auto MakeTokens(std::initializer_list<Tok> list) -> vector<Tok>
{
    return vector<Tok>(list);
}

static auto MakeStream(vector<Tok>& toks) -> VectorStream<Tok>
{
    return VectorStream<Tok>{ .Tokens = move(toks) };
}

// Helper: get the name of a child node at index
static auto ChildName(NodeType const& node, size_t index) -> String
{
    return get<NodeType>(node.Children[index]).Name;
}

// Helper: get the value of a child token at index
static auto ChildTokenValue(NodeType const& node, size_t index) -> string const&
{
    return get<Tok>(node.Children[index]).Value;
}

// Helper: get the type of a child token at index
static auto ChildTokenType(NodeType const& node, size_t index) -> TokType
{
    return get<Tok>(node.Children[index]).Type;
}

// Helper: check if a child at index is a token (not a node)
static auto ChildIsToken(NodeType const& node, size_t index) -> bool
{
    return std::holds_alternative<Tok>(node.Children[index]);
}

// Helper: check if a child at index is a node (not a token)
static auto ChildIsNode(NodeType const& node, size_t index) -> bool
{
    return std::holds_alternative<NodeType>(node.Children[index]);
}

// Helper: get child node reference by index
static auto ChildNode(NodeType const& node, size_t index) -> NodeType const&
{
    return get<NodeType>(node.Children[index]);
}

TEST_CASE("ExpressionParser - Parsable", "[expression-parser]")
{
    ExpressionParser ep;
    REQUIRE(ep.Parsable("expression") == true);
    REQUIRE(ep.Parsable("assignment-expression") == true);
    REQUIRE(ep.Parsable("constant-expression") == true);
    REQUIRE(ep.Parsable("statement") == false);
    REQUIRE(ep.Parsable("logical-or-expression") == false);
    REQUIRE(ep.Parsable("") == false);
}

// ========== Atom parsing ==========

TEST_CASE("ExpressionParser - Atom: identifier", "[expression-parser]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "x" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "atom");
    CHECK(result->Children.size() == 1);
    CHECK(ChildIsToken(*result, 0));
    CHECK(ChildTokenType(*result, 0) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 0) == "x");
    CHECK(stream.Index == 0);
}

TEST_CASE("ExpressionParser - Atom: integer constant", "[expression-parser]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::IntegerConstant, .Value = "42" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "atom");
    CHECK(ChildTokenType(*result, 0) == TokType::IntegerConstant);
    CHECK(ChildTokenValue(*result, 0) == "42");
    CHECK(stream.Index == 0);
}

TEST_CASE("ExpressionParser - Atom: string literal", "[expression-parser]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::StringLiteral, .Value = "\"hello\"" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "atom");
    CHECK(ChildTokenType(*result, 0) == TokType::StringLiteral);
    CHECK(stream.Index == 0);
}

// ========== Prefix operators ==========

TEST_CASE("ExpressionParser - Prefix: unary minus", "[expression-parser]")
{
    // -x => Punctuator_Minus-expression [atom(x)]
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_Minus, .Value = "-" },
        { .Type = TokType::Identifier, .Value = "x" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Minus-expression");
    CHECK(result->Children.size() == 2); // op token + rhs node
    CHECK(ChildIsToken(*result, 0));
    CHECK(ChildTokenType(*result, 0) == TokType::Punctuator_Minus);
    CHECK(ChildIsToken(*result, 1));
    CHECK(ChildTokenType(*result, 1) == TokType::Identifier);
    CHECK(stream.Index == 1);
}

TEST_CASE("ExpressionParser - Prefix: unary plus", "[expression-parser]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "y" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Plus-expression");
    CHECK(ChildTokenType(*result, 1) == TokType::Identifier);
    CHECK(stream.Index == 1);
}

TEST_CASE("ExpressionParser - Prefix: increment", "[expression-parser]")
{
    // ++x => Punctuator_Increment-expression [atom(x)]
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_Increment, .Value = "++" },
        { .Type = TokType::Identifier, .Value = "x" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Increment-expression");
    CHECK(stream.Index == 1);
}

TEST_CASE("ExpressionParser - Prefix: decrement", "[expression-parser]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_Decrement, .Value = "--" },
        { .Type = TokType::Identifier, .Value = "x" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Decrement-expression");
    CHECK(stream.Index == 1);
}

TEST_CASE("ExpressionParser - Prefix: logical not", "[expression-parser]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_Exclamation, .Value = "!" },
        { .Type = TokType::Identifier, .Value = "flag" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Exclamation-expression");
    CHECK(stream.Index == 1);
}

TEST_CASE("ExpressionParser - Prefix: bitwise not", "[expression-parser]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_Tilde, .Value = "~" },
        { .Type = TokType::Identifier, .Value = "mask" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Tilde-expression");
    CHECK(stream.Index == 1);
}

TEST_CASE("ExpressionParser - Prefix: dereference", "[expression-parser]")
{
    // *ptr => Punctuator_Star-expression [atom(ptr)]
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_Star, .Value = "*" },
        { .Type = TokType::Identifier, .Value = "ptr" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Star-expression");
    CHECK(stream.Index == 1);
}

TEST_CASE("ExpressionParser - Prefix: address-of", "[expression-parser]")
{
    // &x => Punctuator_Ampersand-expression [atom(x)]
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_Ampersand, .Value = "&" },
        { .Type = TokType::Identifier, .Value = "x" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Ampersand-expression");
    CHECK(stream.Index == 1);
}

TEST_CASE("ExpressionParser - Prefix: nested", "[expression-parser]")
{
    // --x => Punctuator_Decrement-expression [Punctuator_Decrement-expression [atom(x)]]
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_Decrement, .Value = "--" },
        { .Type = TokType::Punctuator_Decrement, .Value = "--" },
        { .Type = TokType::Identifier, .Value = "x" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Decrement-expression");
    CHECK(ChildIsNode(*result, 1));
    CHECK(ChildName(*result, 1) == "Punctuator_Decrement-expression");
    auto const& inner = ChildNode(*result, 1);
    CHECK(ChildTokenType(inner, 1) == TokType::Identifier);
    CHECK(stream.Index == 2);
}

// ========== Parenthesized expressions ==========

TEST_CASE("ExpressionParser - Paren: simple (x)", "[expression-parser]")
{
    // (x) => atom(x) — parentheses are transparent, result is the inner expression
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Identifier, .Value = "x" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "atom");
    CHECK(ChildTokenValue(*result, 0) == "x");
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - Paren: (a + b)", "[expression-parser]")
{
    // (a + b) => Punctuator_Plus-expression [atom(a), atom(b)]
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Plus-expression");
    CHECK(ChildTokenType(*result, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 1) == "a");
    CHECK(ChildTokenType(*result, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 2) == "b");
    CHECK(stream.Index == 4);
}

TEST_CASE("ExpressionParser - Paren: nested ((x))", "[expression-parser]")
{
    // ((x)) => atom(x)
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Identifier, .Value = "x" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "atom");
    CHECK(ChildTokenValue(*result, 0) == "x");
    CHECK(stream.Index == 4);
}

TEST_CASE("ExpressionParser - Paren: precedence override (a + b) * c", "[expression-parser]")
{
    // (a + b) * c => Punctuator_Star-expression [Punctuator_Plus-expression [a, b], atom(c)]
    // Parentheses make + bind tighter than *
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
        { .Type = TokType::Punctuator_Star, .Value = "*" },
        { .Type = TokType::Identifier, .Value = "c" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    // top: multiplication
    CHECK(result->Name == "Punctuator_Star-expression");
    // lhs: parenthesized addition
    CHECK(ChildIsNode(*result, 1));
    CHECK(ChildName(*result, 1) == "Punctuator_Plus-expression");
    // rhs: atom c
    CHECK(ChildIsToken(*result, 2));
    CHECK(ChildTokenType(*result, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 2) == "c");
    CHECK(stream.Index == 6);
}

TEST_CASE("ExpressionParser - Paren: in infix position a * (b + c)", "[expression-parser]")
{
    // a * (b + c) => Punctuator_Star-expression [Token(a), Punctuator_Plus-expression [b, c]]
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Star, .Value = "*" },
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "c" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Star-expression");
    CHECK(ChildTokenType(*result, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 1) == "a");
    CHECK(ChildIsNode(*result, 2));
    CHECK(ChildName(*result, 2) == "Punctuator_Plus-expression");
    CHECK(stream.Index == 6);
}

TEST_CASE("ExpressionParser - Paren: missing right paren", "[expression-parser]")
{
    // (a + b  => error: stream empty after infix operator (never reaches rightParen check)
    // Because ParseExpression consumes everything, the infix handler hits EOF first.
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE_FALSE(result.has_value());
    CHECK(stream.Index == 3);
}

TEST_CASE("ExpressionParser - Paren: wrong token instead of right paren", "[expression-parser]")
{
    // (a ; => error: after parsing 'a', stream advances to ';', not a ')'
    // ParseExpression returns atom(a), then the paren handler checks Current() = ';' and fails
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Semicolon, .Value = ";" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().Message.find("right parenthesis") != string::npos);
    CHECK(stream.Index == 2);
}

// ========== Infix operators ==========

TEST_CASE("ExpressionParser - Infix: addition", "[expression-parser]")
{
    // a + b => Punctuator_Plus-expression [atom(a), atom(b)]
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Plus-expression");
    CHECK(result->Children.size() == 3); // op token + lhs node + rhs node
    CHECK(ChildIsToken(*result, 0));
    CHECK(ChildTokenType(*result, 0) == TokType::Punctuator_Plus);
    CHECK(ChildIsToken(*result, 1));
    CHECK(ChildTokenType(*result, 1) == TokType::Identifier);
    CHECK(ChildIsToken(*result, 2));
    CHECK(ChildTokenType(*result, 2) == TokType::Identifier);
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - Infix: subtraction", "[expression-parser]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Minus, .Value = "-" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Minus-expression");
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - Infix: multiplication", "[expression-parser]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Star, .Value = "*" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Star-expression");
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - Infix: division", "[expression-parser]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Slash, .Value = "/" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Slash-expression");
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - Infix: modulo", "[expression-parser]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Percent, .Value = "%" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Percent-expression");
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - Infix: logical and", "[expression-parser]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_LogicalAnd, .Value = "&&" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_LogicalAnd-expression");
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - Infix: logical or", "[expression-parser]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_LogicalOr, .Value = "||" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_LogicalOr-expression");
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - Infix: equality", "[expression-parser]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Equal, .Value = "==" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Equal-expression");
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - Infix: not equal", "[expression-parser]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_NotEqual, .Value = "!=" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_NotEqual-expression");
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - Infix: less than", "[expression-parser]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Less, .Value = "<" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Less-expression");
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - Infix: bitwise or", "[expression-parser]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Pipe, .Value = "|" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Pipe-expression");
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - Infix: bitwise xor", "[expression-parser]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Caret, .Value = "^" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Caret-expression");
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - Infix: bitwise and", "[expression-parser]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Ampersand, .Value = "&" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Ampersand-expression");
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - Infix: left shift", "[expression-parser]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_LeftShift, .Value = "<<" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_LeftShift-expression");
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - Infix: right shift", "[expression-parser]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_RightShift, .Value = ">>" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_RightShift-expression");
    CHECK(stream.Index == 2);
}

// ========== Postfix operators ==========

TEST_CASE("ExpressionParser - Postfix: increment", "[expression-parser]")
{
    // x++ => Punctuator_Increment-expression [atom(x)]
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "x" },
        { .Type = TokType::Punctuator_Increment, .Value = "++" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Increment-expression");
    CHECK(result->Children.size() == 2); // op token + lhs node
    CHECK(ChildIsToken(*result, 0));
    CHECK(ChildTokenType(*result, 0) == TokType::Punctuator_Increment);
    CHECK(ChildIsToken(*result, 1));
    CHECK(ChildTokenType(*result, 1) == TokType::Identifier);
    CHECK(stream.Index == 1);
}

TEST_CASE("ExpressionParser - Postfix: decrement", "[expression-parser]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "x" },
        { .Type = TokType::Punctuator_Decrement, .Value = "--" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Decrement-expression");
    CHECK(ChildTokenType(*result, 1) == TokType::Identifier);
    CHECK(stream.Index == 1);
}

TEST_CASE("ExpressionParser - Postfix: dot member access", "[expression-parser]")
{
    // obj.field => Punctuator_Dot-expression [atom(obj)]
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "obj" },
        { .Type = TokType::Punctuator_Dot, .Value = "." },
        { .Type = TokType::Identifier, .Value = "field" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Dot-expression");
    CHECK(result->Children.size() == 3); // op token + lhs token + rhs token
    CHECK(ChildIsToken(*result, 1));
    CHECK(ChildTokenType(*result, 1) == TokType::Identifier);
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - Postfix: arrow member access", "[expression-parser]")
{
    // ptr->field => Punctuator_Arrow-expression [atom(ptr)]
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "ptr" },
        { .Type = TokType::Punctuator_Arrow, .Value = "->" },
        { .Type = TokType::Identifier, .Value = "field" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Arrow-expression");
    CHECK(result->Children.size() == 3);
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - Postfix: chained", "[expression-parser]")
{
    // x++-- => (x++)-- => Punctuator_Decrement-expression [Punctuator_Increment-expression [atom(x)]]
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "x" },
        { .Type = TokType::Punctuator_Increment, .Value = "++" },
        { .Type = TokType::Punctuator_Decrement, .Value = "--" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    // outer: decrement
    CHECK(result->Name == "Punctuator_Decrement-expression");
    CHECK(ChildIsNode(*result, 1));
    // inner: increment
    CHECK(ChildName(*result, 1) == "Punctuator_Increment-expression");
    auto const& inner = ChildNode(*result, 1);
    CHECK(ChildTokenType(inner, 1) == TokType::Identifier);
    CHECK(stream.Index == 2);
}

// ========== Precedence ==========

TEST_CASE("ExpressionParser - Precedence: mul binds tighter than add", "[expression-parser]")
{
    // a + b * c => a + (b * c)
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_Star, .Value = "*" },
        { .Type = TokType::Identifier, .Value = "c" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Plus-expression");
    // lhs: atom a
    CHECK(ChildIsToken(*result, 1));
    CHECK(ChildTokenType(*result, 1) == TokType::Identifier);
    // rhs: star expression
    CHECK(ChildIsNode(*result, 2));
    CHECK(ChildName(*result, 2) == "Punctuator_Star-expression");
    CHECK(stream.Index == 4);
}

TEST_CASE("ExpressionParser - Precedence: add binds tighter than comparison", "[expression-parser]")
{
    // a < b + c => a < (b + c)
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Less, .Value = "<" },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "c" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    // top: less-than
    CHECK(result->Name == "Punctuator_Less-expression");
    // lhs: atom a
    CHECK(ChildTokenType(*result, 1) == TokType::Identifier);
    // rhs: plus expression (b + c)
    CHECK(ChildIsNode(*result, 2));
    CHECK(ChildName(*result, 2) == "Punctuator_Plus-expression");
    CHECK(stream.Index == 4);
}

TEST_CASE("ExpressionParser - Precedence: mul/div are left-associative", "[expression-parser]")
{
    // a * b / c => (a * b) / c
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Star, .Value = "*" },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_Slash, .Value = "/" },
        { .Type = TokType::Identifier, .Value = "c" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    // outer: division
    CHECK(result->Name == "Punctuator_Slash-expression");
    // lhs: multiplication (a * b)
    CHECK(ChildIsNode(*result, 1));
    CHECK(ChildName(*result, 1) == "Punctuator_Star-expression");
    // rhs: atom c
    CHECK(ChildIsToken(*result, 2));
    CHECK(ChildTokenType(*result, 2) == TokType::Identifier);
    CHECK(stream.Index == 4);
}

TEST_CASE("ExpressionParser - Precedence: add/sub are left-associative", "[expression-parser]")
{
    // a + b - c => (a + b) - c
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_Minus, .Value = "-" },
        { .Type = TokType::Identifier, .Value = "c" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Minus-expression");
    CHECK(ChildName(*result, 1) == "Punctuator_Plus-expression");
    CHECK(ChildTokenType(*result, 2) == TokType::Identifier);
    CHECK(stream.Index == 4);
}

TEST_CASE("ExpressionParser - Precedence: prefix binds tighter than infix", "[expression-parser]")
{
    // -a + b => (-a) + b
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_Minus, .Value = "-" },
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Plus-expression");
    // lhs: prefix minus
    CHECK(ChildIsNode(*result, 1));
    CHECK(ChildName(*result, 1) == "Punctuator_Minus-expression");
    // rhs: atom b
    CHECK(ChildIsToken(*result, 2));
    CHECK(ChildTokenType(*result, 2) == TokType::Identifier);
    CHECK(stream.Index == 3);
}

TEST_CASE("ExpressionParser - Precedence: postfix binds tighter than infix", "[expression-parser]")
{
    // a++ + b => (a++) + b
    // Postfix ++ (Level15) is applied first, then + (Level11) is applied as infix
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Increment, .Value = "++" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Plus-expression");
    // lhs: postfix increment (a++)
    CHECK(ChildName(*result, 1) == "Punctuator_Increment-expression");
    // rhs: atom b
    CHECK(ChildTokenType(*result, 2) == TokType::Identifier);
    CHECK(stream.Index == 3);
}

TEST_CASE("ExpressionParser - Precedence: complex expression", "[expression-parser]")
{
    // -a * b + c => ((-a) * b) + c
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_Minus, .Value = "-" },
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Star, .Value = "*" },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "c" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    // top: plus
    CHECK(result->Name == "Punctuator_Plus-expression");
    // lhs: star
    CHECK(ChildName(*result, 1) == "Punctuator_Star-expression");
    auto const& mul = ChildNode(*result, 1);
    // mul's lhs: prefix minus
    CHECK(ChildName(mul, 1) == "Punctuator_Minus-expression");
    // mul's rhs: atom b
    CHECK(ChildTokenType(mul, 2) == TokType::Identifier);
    // rhs: atom c
    CHECK(ChildTokenType(*result, 2) == TokType::Identifier);
    CHECK(stream.Index == 5);
}

TEST_CASE("ExpressionParser - Precedence: all levels", "[expression-parser]")
{
    // a || b && c | d ^ e & f == g < h << i + j * k
    // => a || (b && (c | (d ^ (e & (f == (g < (h << (i + (j * k)))))))))
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_LogicalOr, .Value = "||" },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_LogicalAnd, .Value = "&&" },
        { .Type = TokType::Identifier, .Value = "c" },
        { .Type = TokType::Punctuator_Pipe, .Value = "|" },
        { .Type = TokType::Identifier, .Value = "d" },
        { .Type = TokType::Punctuator_Caret, .Value = "^" },
        { .Type = TokType::Identifier, .Value = "e" },
        { .Type = TokType::Punctuator_Ampersand, .Value = "&" },
        { .Type = TokType::Identifier, .Value = "f" },
        { .Type = TokType::Punctuator_Equal, .Value = "==" },
        { .Type = TokType::Identifier, .Value = "g" },
        { .Type = TokType::Punctuator_Less, .Value = "<" },
        { .Type = TokType::Identifier, .Value = "h" },
        { .Type = TokType::Punctuator_LeftShift, .Value = "<<" },
        { .Type = TokType::Identifier, .Value = "i" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "j" },
        { .Type = TokType::Punctuator_Star, .Value = "*" },
        { .Type = TokType::Identifier, .Value = "k" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    // top level: ||
    CHECK(result->Name == "Punctuator_LogicalOr-expression");
    // rhs: &&
    auto const& rhs1 = ChildNode(*result, 2);
    CHECK(rhs1.Name == "Punctuator_LogicalAnd-expression");
    // rhs: |
    auto const& rhs2 = ChildNode(rhs1, 2);
    CHECK(rhs2.Name == "Punctuator_Pipe-expression");
    // rhs: ^
    auto const& rhs3 = ChildNode(rhs2, 2);
    CHECK(rhs3.Name == "Punctuator_Caret-expression");
    // rhs: &
    auto const& rhs4 = ChildNode(rhs3, 2);
    CHECK(rhs4.Name == "Punctuator_Ampersand-expression");
    // rhs: ==
    auto const& rhs5 = ChildNode(rhs4, 2);
    CHECK(rhs5.Name == "Punctuator_Equal-expression");
    // rhs: <
    auto const& rhs6 = ChildNode(rhs5, 2);
    CHECK(rhs6.Name == "Punctuator_Less-expression");
    // rhs: <<
    auto const& rhs7 = ChildNode(rhs6, 2);
    CHECK(rhs7.Name == "Punctuator_LeftShift-expression");
    // rhs: +
    auto const& rhs8 = ChildNode(rhs7, 2);
    CHECK(rhs8.Name == "Punctuator_Plus-expression");
    // rhs: *
    auto const& rhs9 = ChildNode(rhs8, 2);
    CHECK(rhs9.Name == "Punctuator_Star-expression");
    CHECK(stream.Index == 20);
}

// ========== Mixed prefix + postfix ==========

TEST_CASE("ExpressionParser - Mixed: prefix and postfix on same atom", "[expression-parser]")
{
    // ++x++ => ++(x++) because postfix binds tighter than prefix
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_Increment, .Value = "++" },
        { .Type = TokType::Identifier, .Value = "x" },
        { .Type = TokType::Punctuator_Increment, .Value = "++" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    // outer: prefix increment
    CHECK(result->Name == "Punctuator_Increment-expression");
    CHECK(ChildIsNode(*result, 1));
    // inner: postfix increment
    CHECK(ChildName(*result, 1) == "Punctuator_Increment-expression");
    auto const& inner = ChildNode(*result, 1);
    CHECK(ChildTokenType(inner, 1) == TokType::Identifier);
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - Mixed: prefix minus with infix", "[expression-parser]")
{
    // -a + -b => (-a) + (-b)
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_Minus, .Value = "-" },
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Punctuator_Minus, .Value = "-" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Plus-expression");
    // lhs: prefix minus
    CHECK(ChildName(*result, 1) == "Punctuator_Minus-expression");
    // rhs: prefix minus
    CHECK(ChildName(*result, 2) == "Punctuator_Minus-expression");
    CHECK(stream.Index == 4);
}

TEST_CASE("ExpressionParser - Precedence: dereference binds tighter than multiplication", "[expression-parser]")
{
    // *ptr * 10 => (*ptr) * 10
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_Star, .Value = "*" },
        { .Type = TokType::Identifier, .Value = "ptr" },
        { .Type = TokType::Punctuator_Star, .Value = "*" },
        { .Type = TokType::IntegerConstant, .Value = "10" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    // top: infix multiplication
    CHECK(result->Name == "Punctuator_Star-expression");
    // lhs: prefix dereference (*ptr)
    CHECK(ChildIsNode(*result, 1));
    CHECK(ChildName(*result, 1) == "Punctuator_Star-expression");
    auto const& inner = ChildNode(*result, 1);
    CHECK(ChildTokenType(inner, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(inner, 1) == "ptr");
    // rhs: integer 10
    CHECK(ChildIsToken(*result, 2));
    CHECK(ChildTokenType(*result, 2) == TokType::IntegerConstant);
    CHECK(ChildTokenValue(*result, 2) == "10");
    CHECK(stream.Index == 3);
}

// ========== Ternary operator ==========

TEST_CASE("ExpressionParser - Ternary: simple a ? b : c", "[expression-parser]")
{
    // a ? b : c => Punctuator_Question-expression [atom(a), atom(b), atom(c)]
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Question, .Value = "?" },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_Colon, .Value = ":" },
        { .Type = TokType::Identifier, .Value = "c" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Question-expression");
    CHECK(result->Children.size() == 4); // op + lhs + middle + rhs
    CHECK(ChildTokenType(*result, 0) == TokType::Punctuator_Question);
    CHECK(ChildTokenType(*result, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 1) == "a");
    CHECK(ChildTokenType(*result, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 2) == "b");
    CHECK(ChildTokenType(*result, 3) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 3) == "c");
    CHECK(stream.Index == 4);
}

TEST_CASE("ExpressionParser - Ternary: right-associative a ? b : c ? d : e", "[expression-parser]")
{
    // a ? b : c ? d : e => a ? b : (c ? d : e)
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Question, .Value = "?" },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_Colon, .Value = ":" },
        { .Type = TokType::Identifier, .Value = "c" },
        { .Type = TokType::Punctuator_Question, .Value = "?" },
        { .Type = TokType::Identifier, .Value = "d" },
        { .Type = TokType::Punctuator_Colon, .Value = ":" },
        { .Type = TokType::Identifier, .Value = "e" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    // outer: a ? b : (c ? d : e)
    CHECK(result->Name == "Punctuator_Question-expression");
    CHECK(ChildTokenType(*result, 0) == TokType::Punctuator_Question);
    CHECK(ChildTokenType(*result, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 1) == "a");
    CHECK(ChildTokenType(*result, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 2) == "b");
    // rhs should be nested ternary
    CHECK(ChildIsNode(*result, 3));
    auto const& inner = ChildNode(*result, 3);
    CHECK(inner.Name == "Punctuator_Question-expression");
    CHECK(ChildTokenValue(inner, 1) == "c");
    CHECK(ChildTokenValue(inner, 2) == "d");
    CHECK(ChildTokenValue(inner, 3) == "e");
    CHECK(stream.Index == 8);
}

TEST_CASE("ExpressionParser - Ternary: nested condition a ? b ? c : d : e", "[expression-parser]")
{
    // a ? b ? c : d : e => a ? (b ? c : d) : e
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Question, .Value = "?" },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_Question, .Value = "?" },
        { .Type = TokType::Identifier, .Value = "c" },
        { .Type = TokType::Punctuator_Colon, .Value = ":" },
        { .Type = TokType::Identifier, .Value = "d" },
        { .Type = TokType::Punctuator_Colon, .Value = ":" },
        { .Type = TokType::Identifier, .Value = "e" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    // outer: a ? (b ? c : d) : e
    CHECK(result->Name == "Punctuator_Question-expression");
    CHECK(ChildTokenType(*result, 0) == TokType::Punctuator_Question);
    // condition: a
    CHECK(ChildTokenType(*result, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 1) == "a");
    // middle: nested ternary b ? c : d
    CHECK(ChildIsNode(*result, 2));
    auto const& middle = ChildNode(*result, 2);
    CHECK(middle.Name == "Punctuator_Question-expression");
    CHECK(ChildTokenType(middle, 0) == TokType::Punctuator_Question);
    CHECK(ChildTokenType(middle, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(middle, 1) == "b");
    CHECK(ChildTokenType(middle, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(middle, 2) == "c");
    CHECK(ChildTokenType(middle, 3) == TokType::Identifier);
    CHECK(ChildTokenValue(middle, 3) == "d");
    // rhs: e
    CHECK(ChildTokenType(*result, 3) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 3) == "e");
    CHECK(stream.Index == 8);
}

TEST_CASE("ExpressionParser - Ternary: with arithmetic in branches", "[expression-parser]")
{
    // a ? b + c : d * e
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Question, .Value = "?" },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "c" },
        { .Type = TokType::Punctuator_Colon, .Value = ":" },
        { .Type = TokType::Identifier, .Value = "d" },
        { .Type = TokType::Punctuator_Star, .Value = "*" },
        { .Type = TokType::Identifier, .Value = "e" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Question-expression");
    CHECK(ChildTokenType(*result, 0) == TokType::Punctuator_Question);
    // condition: a
    CHECK(ChildTokenType(*result, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 1) == "a");
    // middle: b + c
    CHECK(ChildIsNode(*result, 2));
    CHECK(ChildName(*result, 2) == "Punctuator_Plus-expression");
    auto const& plus = ChildNode(*result, 2);
    CHECK(ChildTokenType(plus, 0) == TokType::Punctuator_Plus);
    CHECK(ChildTokenType(plus, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(plus, 1) == "b");
    CHECK(ChildTokenType(plus, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(plus, 2) == "c");
    // rhs: d * e
    CHECK(ChildIsNode(*result, 3));
    CHECK(ChildName(*result, 3) == "Punctuator_Star-expression");
    auto const& star = ChildNode(*result, 3);
    CHECK(ChildTokenType(star, 0) == TokType::Punctuator_Star);
    CHECK(ChildTokenType(star, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(star, 1) == "d");
    CHECK(ChildTokenType(star, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(star, 2) == "e");
    CHECK(stream.Index == 8);
}

TEST_CASE("ExpressionParser - Ternary: condition with comparison", "[expression-parser]")
{
    // a > b ? c : d
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Greater, .Value = ">" },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_Question, .Value = "?" },
        { .Type = TokType::Identifier, .Value = "c" },
        { .Type = TokType::Punctuator_Colon, .Value = ":" },
        { .Type = TokType::Identifier, .Value = "d" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Question-expression");
    CHECK(ChildTokenType(*result, 0) == TokType::Punctuator_Question);
    // condition: a > b
    CHECK(ChildIsNode(*result, 1));
    CHECK(ChildName(*result, 1) == "Punctuator_Greater-expression");
    auto const& greater = ChildNode(*result, 1);
    CHECK(ChildTokenType(greater, 0) == TokType::Punctuator_Greater);
    CHECK(ChildTokenType(greater, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(greater, 1) == "a");
    CHECK(ChildTokenType(greater, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(greater, 2) == "b");
    // true branch: c
    CHECK(ChildTokenType(*result, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 2) == "c");
    // false branch: d
    CHECK(ChildTokenType(*result, 3) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 3) == "d");
    CHECK(stream.Index == 6);
}

// ========== Comma operator ==========

TEST_CASE("ExpressionParser - Comma: simple a, b", "[expression-parser]")
{
    // a , b => Punctuator_Comma-expression [op(,), atom(a), atom(b)]
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Comma, .Value = "," },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Comma-expression");
    CHECK(result->Children.size() == 3);
    CHECK(ChildTokenType(*result, 0) == TokType::Punctuator_Comma);
    CHECK(ChildTokenType(*result, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 1) == "a");
    CHECK(ChildTokenType(*result, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 2) == "b");
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - Comma: left-associative a, b, c", "[expression-parser]")
{
    // a , b , c => (a , b) , c
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Comma, .Value = "," },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_Comma, .Value = "," },
        { .Type = TokType::Identifier, .Value = "c" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    // outer: (a, b) , c
    CHECK(result->Name == "Punctuator_Comma-expression");
    CHECK(ChildTokenType(*result, 0) == TokType::Punctuator_Comma);
    // lhs: inner comma (a, b)
    CHECK(ChildIsNode(*result, 1));
    CHECK(ChildName(*result, 1) == "Punctuator_Comma-expression");
    auto const& inner = ChildNode(*result, 1);
    CHECK(ChildTokenType(inner, 0) == TokType::Punctuator_Comma);
    CHECK(ChildTokenType(inner, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(inner, 1) == "a");
    CHECK(ChildTokenType(inner, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(inner, 2) == "b");
    // rhs: c
    CHECK(ChildTokenType(*result, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 2) == "c");
    CHECK(stream.Index == 4);
}

TEST_CASE("ExpressionParser - Comma: lowest precedence", "[expression-parser]")
{
    // a + b, c * d => (a + b), (c * d)
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_Comma, .Value = "," },
        { .Type = TokType::Identifier, .Value = "c" },
        { .Type = TokType::Punctuator_Star, .Value = "*" },
        { .Type = TokType::Identifier, .Value = "d" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Comma-expression");
    CHECK(ChildTokenType(*result, 0) == TokType::Punctuator_Comma);
    // lhs: a + b
    CHECK(ChildIsNode(*result, 1));
    CHECK(ChildName(*result, 1) == "Punctuator_Plus-expression");
    auto const& plus = ChildNode(*result, 1);
    CHECK(ChildTokenType(plus, 0) == TokType::Punctuator_Plus);
    CHECK(ChildTokenType(plus, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(plus, 1) == "a");
    CHECK(ChildTokenType(plus, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(plus, 2) == "b");
    // rhs: c * d
    CHECK(ChildIsNode(*result, 2));
    CHECK(ChildName(*result, 2) == "Punctuator_Star-expression");
    auto const& star = ChildNode(*result, 2);
    CHECK(ChildTokenType(star, 0) == TokType::Punctuator_Star);
    CHECK(ChildTokenType(star, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(star, 1) == "c");
    CHECK(ChildTokenType(star, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(star, 2) == "d");
    CHECK(stream.Index == 6);
}

// ========== Assignment operators ==========

TEST_CASE("ExpressionParser - Assign: simple a = b", "[expression-parser]")
{
    // a = b => Punctuator_Assign-expression [op(=), atom(a), atom(b)]
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Assign, .Value = "=" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Assign-expression");
    CHECK(result->Children.size() == 3);
    CHECK(ChildTokenType(*result, 0) == TokType::Punctuator_Assign);
    CHECK(ChildTokenType(*result, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 1) == "a");
    CHECK(ChildTokenType(*result, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 2) == "b");
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - Assign: right-associative a = b = c", "[expression-parser]")
{
    // a = b = c => a = (b = c)
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Assign, .Value = "=" },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_Assign, .Value = "=" },
        { .Type = TokType::Identifier, .Value = "c" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Assign-expression");
    CHECK(ChildTokenType(*result, 0) == TokType::Punctuator_Assign);
    // lhs: a
    CHECK(ChildTokenType(*result, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 1) == "a");
    // rhs: inner assignment b = c
    CHECK(ChildIsNode(*result, 2));
    CHECK(ChildName(*result, 2) == "Punctuator_Assign-expression");
    auto const& inner = ChildNode(*result, 2);
    CHECK(ChildTokenType(inner, 0) == TokType::Punctuator_Assign);
    CHECK(ChildTokenType(inner, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(inner, 1) == "b");
    CHECK(ChildTokenType(inner, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(inner, 2) == "c");
    CHECK(stream.Index == 4);
}

TEST_CASE("ExpressionParser - Assign: lower precedence than ternary", "[expression-parser]")
{
    // a = b ? c : d => a = (b ? c : d)
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Assign, .Value = "=" },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_Question, .Value = "?" },
        { .Type = TokType::Identifier, .Value = "c" },
        { .Type = TokType::Punctuator_Colon, .Value = ":" },
        { .Type = TokType::Identifier, .Value = "d" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    // top: assignment
    CHECK(result->Name == "Punctuator_Assign-expression");
    CHECK(ChildTokenType(*result, 0) == TokType::Punctuator_Assign);
    // lhs: a
    CHECK(ChildTokenType(*result, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 1) == "a");
    // rhs: ternary b ? c : d
    CHECK(ChildIsNode(*result, 2));
    CHECK(ChildName(*result, 2) == "Punctuator_Question-expression");
    auto const& ternary = ChildNode(*result, 2);
    CHECK(ChildTokenType(ternary, 0) == TokType::Punctuator_Question);
    CHECK(ChildTokenType(ternary, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(ternary, 1) == "b");
    CHECK(ChildTokenType(ternary, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(ternary, 2) == "c");
    CHECK(ChildTokenType(ternary, 3) == TokType::Identifier);
    CHECK(ChildTokenValue(ternary, 3) == "d");
    CHECK(stream.Index == 6);
}

// ========== Mixed ternary + comma + assignment ==========

TEST_CASE("ExpressionParser - Mixed: ternary with comma in branch", "[expression-parser]")
{
    // a ? b, c : d => a ? (b, c) : d
    // comma inside ternary's middle has MinLevel, so it binds
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Question, .Value = "?" },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_Comma, .Value = "," },
        { .Type = TokType::Identifier, .Value = "c" },
        { .Type = TokType::Punctuator_Colon, .Value = ":" },
        { .Type = TokType::Identifier, .Value = "d" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Question-expression");
    CHECK(ChildTokenType(*result, 0) == TokType::Punctuator_Question);
    // condition: a
    CHECK(ChildTokenType(*result, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 1) == "a");
    // middle: b, c (comma expression)
    CHECK(ChildIsNode(*result, 2));
    CHECK(ChildName(*result, 2) == "Punctuator_Comma-expression");
    auto const& comma = ChildNode(*result, 2);
    CHECK(ChildTokenType(comma, 0) == TokType::Punctuator_Comma);
    CHECK(ChildTokenType(comma, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(comma, 1) == "b");
    CHECK(ChildTokenType(comma, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(comma, 2) == "c");
    // rhs: d
    CHECK(ChildTokenType(*result, 3) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 3) == "d");
    CHECK(stream.Index == 6);
}

// ========== Error cases ==========

// NOTE: Empty stream is not tested — ParsePrefix reads Current() at Index=0
// without a prior MoveNext(), so an empty vector would be UB. Callers should
// check stream emptiness before calling Parse.

TEST_CASE("ExpressionParser - Non-parsable nonterminal", "[expression-parser]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "x" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("statement", stream);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().Message.find("isn't parsable") != string::npos);
    CHECK(stream.Index == 0);
}

TEST_CASE("ExpressionParser - Prefix needs operand", "[expression-parser]")
{
    // - <eof> => error
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_Minus, .Value = "-" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE_FALSE(result.has_value());
    CHECK(stream.Index == 0);
}

TEST_CASE("ExpressionParser - Infix operator without rhs returns error", "[expression-parser]")
{
    // a + <eof> => error: stream empty after infix operator
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE_FALSE(result.has_value());
    CHECK(stream.Index == 1);
}

// ========== Stream position ==========

TEST_CASE("ExpressionParser - Stream position: stops at unrecognized token", "[expression-parser]")
{
    // a ; => parses atom(a), stops at semicolon (not a recognized infix/postfix)
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Semicolon, .Value = ";" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "atom");
    CHECK(stream.Index == 0);
}

// ========== expression — allows all operators ==========

TEST_CASE("ExpressionParser - expression: comma a, b", "[expression-parser][expression-type]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Comma, .Value = "," },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Comma-expression");
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - expression: assignment a = b", "[expression-parser][expression-type]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Assign, .Value = "=" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Assign-expression");
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - expression: stops at semicolon", "[expression-parser][expression-type]")
{
    // a + b ; c => parses a + b, stops before ;
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_Semicolon, .Value = ";" },
        { .Type = TokType::Identifier, .Value = "c" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Plus-expression");
    CHECK(stream.Index == 2);
}

// ========== assignment-expression — blocks comma, allows assignment ==========

TEST_CASE("ExpressionParser - assignment-expression: simple a = b", "[expression-parser][expression-type]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Assign, .Value = "=" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("assignment-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Assign-expression");
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - assignment-expression: arithmetic a + b", "[expression-parser][expression-type]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("assignment-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Plus-expression");
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - assignment-expression: stops at comma", "[expression-parser][expression-type]")
{
    // a , b => only parses a, comma is blocked
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Comma, .Value = "," },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("assignment-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "atom");
    CHECK(ChildTokenValue(*result, 0) == "a");
    CHECK(stream.Index == 0);
}

TEST_CASE("ExpressionParser - assignment-expression: stops at comma after assignment", "[expression-parser][expression-type]")
{
    // a = b , c => parses a = b, stops before comma
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Assign, .Value = "=" },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_Comma, .Value = "," },
        { .Type = TokType::Identifier, .Value = "c" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("assignment-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Assign-expression");
    CHECK(stream.Index == 2);
}

// ========== constant-expression — blocks comma and assignment ==========

TEST_CASE("ExpressionParser - constant-expression: arithmetic a + b", "[expression-parser][expression-type]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("constant-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Plus-expression");
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - constant-expression: ternary a ? b : c", "[expression-parser][expression-type]")
{
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Question, .Value = "?" },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_Colon, .Value = ":" },
        { .Type = TokType::Identifier, .Value = "c" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("constant-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Question-expression");
    CHECK(stream.Index == 4);
}

TEST_CASE("ExpressionParser - constant-expression: stops at comma", "[expression-parser][expression-type]")
{
    // a , b => only parses a, comma is blocked
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Comma, .Value = "," },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("constant-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "atom");
    CHECK(ChildTokenValue(*result, 0) == "a");
    CHECK(stream.Index == 0);
}

TEST_CASE("ExpressionParser - constant-expression: stops at assignment", "[expression-parser][expression-type]")
{
    // a = b => only parses a, assignment is blocked
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Assign, .Value = "=" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("constant-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "atom");
    CHECK(ChildTokenValue(*result, 0) == "a");
    CHECK(stream.Index == 0);
}

TEST_CASE("ExpressionParser - constant-expression: stops at assignment after arithmetic", "[expression-parser][expression-type]")
{
    // a + b = c => parses a + b, stops before =
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_Assign, .Value = "=" },
        { .Type = TokType::Identifier, .Value = "c" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("constant-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Plus-expression");
    CHECK(stream.Index == 2);
}

// ========== sizeof / alignof ==========

TEST_CASE("ExpressionParser - constant-expression: sizeof applied to identifier", "[expression-parser]")
{
    // sizeof x => Keyword_Sizeof-expression [atom(x)]
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Keyword_Sizeof, .Value = "sizeof" },
        { .Type = TokType::Identifier, .Value = "x" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("constant-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Keyword_Sizeof-expression");
    CHECK(result->Children.size() == 2);
    CHECK(ChildIsToken(*result, 0));
    CHECK(ChildTokenType(*result, 0) == TokType::Keyword_Sizeof);
    CHECK(ChildIsToken(*result, 1));
    CHECK(ChildTokenType(*result, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 1) == "x");
    CHECK(stream.Index == 1);
}

TEST_CASE("ExpressionParser - constant-expression: sizeof with parenthesized expression", "[expression-parser]")
{
    // sizeof(x) => Keyword_Sizeof-expression [token(x)]
    // Parentheses are transparent; Cons unpacks atom into its token.
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Keyword_Sizeof, .Value = "sizeof" },
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Identifier, .Value = "x" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("constant-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Keyword_Sizeof-expression");
    CHECK(ChildIsToken(*result, 1));
    CHECK(ChildTokenType(*result, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 1) == "x");
    CHECK(stream.Index == 3);
}

TEST_CASE("ExpressionParser - constant-expression: sizeof(int) with parens", "[expression-parser]")
{
    // sizeof(int) => Keyword_Sizeof-expression [token(int)]
    // Parentheses are transparent: Cons unpacks type-name node into its token.
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Keyword_Sizeof, .Value = "sizeof" },
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Keyword_Int, .Value = "int" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("constant-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Keyword_Sizeof-expression");
    CHECK(result->Children.size() == 2);
    CHECK(ChildIsToken(*result, 0));
    CHECK(ChildTokenType(*result, 0) == TokType::Keyword_Sizeof);
    CHECK(ChildIsToken(*result, 1));
    CHECK(ChildTokenType(*result, 1) == TokType::Keyword_Int);
    CHECK(ChildTokenValue(*result, 1) == "int");
    CHECK(stream.Index == 3);
}

TEST_CASE("ExpressionParser - constant-expression: sizeof int without parens", "[expression-parser]")
{
    // sizeof int => Keyword_Sizeof-expression [token(int)]
    // sizeof can be applied to a type keyword directly, no parens required.
    // Cons unpacks type-name node into its token.
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Keyword_Sizeof, .Value = "sizeof" },
        { .Type = TokType::Keyword_Int, .Value = "int" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("constant-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Keyword_Sizeof-expression");
    CHECK(result->Children.size() == 2);
    CHECK(ChildIsToken(*result, 0));
    CHECK(ChildTokenType(*result, 0) == TokType::Keyword_Sizeof);
    CHECK(ChildIsToken(*result, 1));
    CHECK(ChildTokenType(*result, 1) == TokType::Keyword_Int);
    CHECK(ChildTokenValue(*result, 1) == "int");
    CHECK(stream.Index == 1);
}

TEST_CASE("ExpressionParser - constant-expression: sizeof(char) with parens", "[expression-parser]")
{
    // sizeof(char) => Keyword_Sizeof-expression [token(char)]
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Keyword_Sizeof, .Value = "sizeof" },
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Keyword_Char, .Value = "char" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("constant-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Keyword_Sizeof-expression");
    CHECK(ChildIsToken(*result, 1));
    CHECK(ChildTokenType(*result, 1) == TokType::Keyword_Char);
    CHECK(ChildTokenValue(*result, 1) == "char");
    CHECK(stream.Index == 3);
}

TEST_CASE("ExpressionParser - constant-expression: sizeof char without parens", "[expression-parser]")
{
    // sizeof char => Keyword_Sizeof-expression [token(char)]
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Keyword_Sizeof, .Value = "sizeof" },
        { .Type = TokType::Keyword_Char, .Value = "char" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("constant-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Keyword_Sizeof-expression");
    CHECK(ChildIsToken(*result, 1));
    CHECK(ChildTokenType(*result, 1) == TokType::Keyword_Char);
    CHECK(ChildTokenValue(*result, 1) == "char");
    CHECK(stream.Index == 1);
}

TEST_CASE("ExpressionParser - constant-expression: sizeof(double) with parens", "[expression-parser]")
{
    // sizeof(double) => Keyword_Sizeof-expression [token(double)]
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Keyword_Sizeof, .Value = "sizeof" },
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Keyword_Double, .Value = "double" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("constant-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Keyword_Sizeof-expression");
    CHECK(ChildIsToken(*result, 1));
    CHECK(ChildTokenType(*result, 1) == TokType::Keyword_Double);
    CHECK(ChildTokenValue(*result, 1) == "double");
    CHECK(stream.Index == 3);
}

TEST_CASE("ExpressionParser - constant-expression: sizeof binds tighter than addition", "[expression-parser]")
{
    // sizeof a + b => (sizeof a) + b
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Keyword_Sizeof, .Value = "sizeof" },
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("constant-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Plus-expression");
    CHECK(ChildIsNode(*result, 1));
    CHECK(ChildName(*result, 1) == "Keyword_Sizeof-expression");
    auto const& inner = ChildNode(*result, 1);
    CHECK(ChildTokenType(inner, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(inner, 1) == "a");
    CHECK(ChildIsToken(*result, 2));
    CHECK(ChildTokenType(*result, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 2) == "b");
    CHECK(stream.Index == 3);
}

TEST_CASE("ExpressionParser - constant-expression: sizeof with dereference", "[expression-parser]")
{
    // sizeof *ptr => sizeof (*ptr)
    // *ptr parses as prefix dereference, sizeof wraps the result
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Keyword_Sizeof, .Value = "sizeof" },
        { .Type = TokType::Punctuator_Star, .Value = "*" },
        { .Type = TokType::Identifier, .Value = "ptr" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("constant-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Keyword_Sizeof-expression");
    CHECK(ChildIsNode(*result, 1));
    CHECK(ChildName(*result, 1) == "Punctuator_Star-expression");
    auto const& inner = ChildNode(*result, 1);
    CHECK(ChildTokenType(inner, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(inner, 1) == "ptr");
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - constant-expression: sizeof sizeof x", "[expression-parser]")
{
    // sizeof sizeof x => sizeof(sizeof(x))
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Keyword_Sizeof, .Value = "sizeof" },
        { .Type = TokType::Keyword_Sizeof, .Value = "sizeof" },
        { .Type = TokType::Identifier, .Value = "x" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("constant-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Keyword_Sizeof-expression");
    CHECK(ChildIsNode(*result, 1));
    CHECK(ChildName(*result, 1) == "Keyword_Sizeof-expression");
    auto const& inner = ChildNode(*result, 1);
    CHECK(ChildTokenType(inner, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(inner, 1) == "x");
    CHECK(stream.Index == 2);
}

TEST_CASE("ExpressionParser - constant-expression: sizeof int + b", "[expression-parser]")
{
    // sizeof int + b => (sizeof int) + b
    // sizeof (Level14) binds tighter than + (Level11)
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Keyword_Sizeof, .Value = "sizeof" },
        { .Type = TokType::Keyword_Int, .Value = "int" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("constant-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Plus-expression");
    CHECK(ChildIsNode(*result, 1));
    CHECK(ChildName(*result, 1) == "Keyword_Sizeof-expression");
    CHECK(ChildIsToken(*result, 2));
    CHECK(ChildTokenType(*result, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 2) == "b");
    CHECK(stream.Index == 3);
}

TEST_CASE("ExpressionParser - constant-expression: sizeof(int) + b", "[expression-parser]")
{
    // sizeof(int) + b => (sizeof(int)) + b
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Keyword_Sizeof, .Value = "sizeof" },
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Keyword_Int, .Value = "int" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("constant-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Plus-expression");
    CHECK(ChildIsNode(*result, 1));
    CHECK(ChildName(*result, 1) == "Keyword_Sizeof-expression");
    CHECK(ChildIsToken(*result, 2));
    CHECK(ChildTokenType(*result, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 2) == "b");
    CHECK(stream.Index == 5);
}

TEST_CASE("ExpressionParser - constant-expression: sizeof in multiplication", "[expression-parser]")
{
    // sizeof a * b => (sizeof a) * b
    // sizeof (Level14) binds tighter than * (Level13)
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Keyword_Sizeof, .Value = "sizeof" },
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Star, .Value = "*" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("constant-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Punctuator_Star-expression");
    CHECK(ChildIsNode(*result, 1));
    CHECK(ChildName(*result, 1) == "Keyword_Sizeof-expression");
    CHECK(ChildIsToken(*result, 2));
    CHECK(ChildTokenType(*result, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 2) == "b");
    CHECK(stream.Index == 3);
}

TEST_CASE("ExpressionParser - constant-expression: alignof applied to identifier", "[expression-parser]")
{
    // alignof x => Keyword_Alignof-expression [atom(x)]
    // alignof behaves the same as sizeof as a prefix operator
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Keyword_Alignof, .Value = "alignof" },
        { .Type = TokType::Identifier, .Value = "x" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("constant-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Keyword_Alignof-expression");
    CHECK(result->Children.size() == 2);
    CHECK(ChildTokenType(*result, 0) == TokType::Keyword_Alignof);
    CHECK(ChildTokenType(*result, 1) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 1) == "x");
    CHECK(stream.Index == 1);
}

TEST_CASE("ExpressionParser - constant-expression: alignof(int) with parens", "[expression-parser]")
{
    // alignof(int) => Keyword_Alignof-expression [token(int)]
    // Cons unpacks type-name node into its token.
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Keyword_Alignof, .Value = "alignof" },
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Keyword_Int, .Value = "int" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("constant-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Keyword_Alignof-expression");
    CHECK(ChildIsToken(*result, 1));
    CHECK(ChildTokenType(*result, 1) == TokType::Keyword_Int);
    CHECK(ChildTokenValue(*result, 1) == "int");
    CHECK(stream.Index == 3);
}

TEST_CASE("ExpressionParser - constant-expression: alignof int without parens", "[expression-parser]")
{
    // alignof int => Keyword_Alignof-expression [token(int)]
    // alignof can be applied to a type keyword directly, no parens required.
    // Cons unpacks type-name node into its token.
    ExpressionParser ep;
    auto toks = MakeTokens({
        { .Type = TokType::Keyword_Alignof, .Value = "alignof" },
        { .Type = TokType::Keyword_Int, .Value = "int" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<void>("constant-expression", stream);
    REQUIRE(result.has_value());
    CHECK(result->Name == "Keyword_Alignof-expression");
    CHECK(ChildIsToken(*result, 1));
    CHECK(ChildTokenType(*result, 1) == TokType::Keyword_Int);
    CHECK(ChildTokenValue(*result, 1) == "int");
    CHECK(stream.Index == 1);
}
