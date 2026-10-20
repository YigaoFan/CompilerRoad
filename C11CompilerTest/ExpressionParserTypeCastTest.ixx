// Type cast expression tests are placed in this separate file.
// Because type cast parsing (e.g. (int)x) requires an external parser
// to resolve type-name, unlike other expression tests that can run
// with NotImplementedParse alone.

module;
#include <catch2/catch_all.hpp>
export module ExpressionParserTypeCastTest;
import std;
import Base;
import Lexer;
import Parser;
import C11Spec;
import ExpressionParser;
import ConflictResolver;
import Context;

using std::vector;
using std::string;
using std::move;
using std::format;
using std::get;

using Tok = Token<TokType>;
using NodeType = SyntaxTreeNode<Tok, Context>;
using ParseResult = ParserResult<NodeType>;

static auto MakeTokens(std::initializer_list<Tok> list) -> vector<Tok>
{
    return vector<Tok>(list);
}

static auto MakeStream(vector<Tok>& toks) -> VectorStream<Tok>
{
    return VectorStream<Tok>{ .Tokens = move(toks) };
}

// Minimal external parser that recognizes type-name for testing type cast expressions.
// Only handles built-in type keywords (int, char, double, float, void).
static auto MinimalTypeCastExternalParse = [](String const& nontermin, VectorStream<Tok>& stream) -> ParseResult
{
    if (nontermin == "type-name")
    {
        auto const& tok = stream.Current();
        auto isBuiltinType = [](TokType t) {
            return t == TokType::Keyword_Int
                || t == TokType::Keyword_Char
                || t == TokType::Keyword_Double
                || t == TokType::Keyword_Float
                || t == TokType::Keyword_Void;
        };
        if (isBuiltinType(tok.Type))
        {
            auto n = NodeType("type-name", { "value" });
            n.Children.push_back(tok);
            return n;
        }
        return std::unexpected(ParseFailResult{ format("expected type-name, got {}", tok) });
    }
    return std::unexpected(ParseFailResult{ format("unsupported nonterminal: {}", nontermin) });
};

static auto FullTypeCastExternalParse = [](String const& nontermin, VectorStream<Tok>& stream) -> ParseResult
{
    if (nontermin == "type-name")
    {
        auto resolver = C11ConflictResolver();
        ExpressionParser expParser; // TODO let ExpressionParser out from here

        auto p = LLParser::ConstructFrom(String(nontermin), grammars, terminal2IntTokenType, resolver, expParser);
        auto r = p.ParseWith<Context>(stream, [](auto) {}, {});
		return r;
    }
    return std::unexpected(ParseFailResult{ format("unsupported nonterminal: {}", nontermin) });
};

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

// Factory: create ExpressionParser with HtmlLogger enabled
static auto MakeExpressionParser() -> ExpressionParser
{
    auto ep = ExpressionParser();
    ep.Logger(std::make_shared<HtmlLogger>(HtmlLogger::NewFromCurrentTime()));
    return ep;
}

// Helper: get child node reference by index
static auto ChildNode(NodeType const& node, size_t index) -> NodeType const&
{
    return get<NodeType>(node.Children[index]);
}

// Helper: recursively find the first token type by descending into the first child node
static auto FindFirstTokenType(NodeType const& node) -> TokType
{
    if (node.Children.empty())
    {
        throw std::runtime_error("FindFirstTokenType: no children");
    }
    auto const& first = node.Children[0];
    if (std::holds_alternative<Tok>(first))
    {
        return std::get<Tok>(first).Type;
    }
    return FindFirstTokenType(std::get<NodeType>(first));
}

TEST_CASE("ExpressionParser - Type cast: (int)x", "[expression-parser][type-cast]")
{
    // (int)x => type-cast-expression [type: atom(int), value: atom(x)]
    auto ep = MakeExpressionParser();
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Keyword_Int, .Value = "int" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
        { .Type = TokType::Identifier, .Value = "x" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<Context>("expression", stream, FullTypeCastExternalParse);
    REQUIRE(result.has_value());
    CHECK(result->Name == "type-cast-expression");
    CHECK(result->Children.size() == 2);
    CHECK(result->ChildSymbols[0] == "type");
    CHECK(result->ChildSymbols[1] == "value");
    // type: int
    CHECK(ChildIsNode(*result, 0));
    CHECK(FindFirstTokenType(ChildNode(*result, 0)) == TokType::Keyword_Int);
    // value: x
    CHECK(ChildIsNode(*result, 1));
    CHECK(ChildName(*result, 1) == "atom");
    CHECK(ChildTokenType(ChildNode(*result, 1), 0) == TokType::Identifier);
    CHECK(ChildTokenValue(ChildNode(*result, 1), 0) == "x");
    CHECK(stream.Index == 3);
}

TEST_CASE("ExpressionParser - Type cast: (char)x", "[expression-parser][type-cast]")
{
    // (char)x => type-cast-expression [type: atom(char), value: atom(x)]
    auto ep = MakeExpressionParser();
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Keyword_Char, .Value = "char" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
        { .Type = TokType::Identifier, .Value = "x" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<Context>("expression", stream, FullTypeCastExternalParse);
    REQUIRE(result.has_value());
    CHECK(result->Name == "type-cast-expression");
    CHECK(FindFirstTokenType(ChildNode(*result, 0)) == TokType::Keyword_Char);
    CHECK(ChildTokenValue(ChildNode(*result, 1), 0) == "x");
    CHECK(stream.Index == 3);
}

TEST_CASE("ExpressionParser - Type cast: (double)x", "[expression-parser][type-cast]")
{
    // (double)x => type-cast-expression [type: atom(double), value: atom(x)]
    auto ep = MakeExpressionParser();
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Keyword_Double, .Value = "double" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
        { .Type = TokType::Identifier, .Value = "x" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<Context>("expression", stream, FullTypeCastExternalParse);
    REQUIRE(result.has_value());
    CHECK(result->Name == "type-cast-expression");
    CHECK(FindFirstTokenType(ChildNode(*result, 0)) == TokType::Keyword_Double);
    CHECK(ChildTokenValue(ChildNode(*result, 1), 0) == "x");
    CHECK(stream.Index == 3);
}

TEST_CASE("ExpressionParser - Type cast: (float)x", "[expression-parser][type-cast]")
{
    // (float)x => type-cast-expression [type: atom(float), value: atom(x)]
    auto ep = MakeExpressionParser();
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Keyword_Float, .Value = "float" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
        { .Type = TokType::Identifier, .Value = "x" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<Context>("expression", stream, FullTypeCastExternalParse);
    REQUIRE(result.has_value());
    CHECK(result->Name == "type-cast-expression");
    CHECK(FindFirstTokenType(ChildNode(*result, 0)) == TokType::Keyword_Float);
    CHECK(ChildTokenValue(ChildNode(*result, 1), 0) == "x");
    CHECK(stream.Index == 3);
}

TEST_CASE("ExpressionParser - Type cast: (void)x", "[expression-parser][type-cast]")
{
    // (void)x => type-cast-expression [type: atom(void), value: atom(x)]
    auto ep = MakeExpressionParser();
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Keyword_Void, .Value = "void" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
        { .Type = TokType::Identifier, .Value = "x" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<Context>("expression", stream, FullTypeCastExternalParse);
    REQUIRE(result.has_value());
    CHECK(result->Name == "type-cast-expression");
    CHECK(FindFirstTokenType(ChildNode(*result, 0)) == TokType::Keyword_Void);
    CHECK(ChildTokenValue(ChildNode(*result, 1), 0) == "x");
    CHECK(stream.Index == 3);
}

TEST_CASE("ExpressionParser - Type cast: precedence vs addition", "[expression-parser][type-cast]")
{
    // (int)a + b => ((int)a) + b
    // Type cast (Level14) binds tighter than + (Level11)
    auto ep = MakeExpressionParser();
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Keyword_Int, .Value = "int" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<Context>("expression", stream, FullTypeCastExternalParse);
    REQUIRE(result.has_value());
    // top: addition
    CHECK(result->Name == "Punctuator_Plus-expression");
    // lhs: type cast
    CHECK(ChildIsNode(*result, 1));
    CHECK(ChildName(*result, 1) == "type-cast-expression");
    auto const& cast = ChildNode(*result, 1);
    CHECK(FindFirstTokenType(ChildNode(cast, 0)) == TokType::Keyword_Int);
    CHECK(ChildTokenValue(ChildNode(cast, 1), 0) == "a");
    // rhs: atom b
    CHECK(ChildIsToken(*result, 2));
    CHECK(ChildTokenType(*result, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 2) == "b");
    CHECK(stream.Index == 5);
}

TEST_CASE("ExpressionParser - Type cast: precedence vs multiplication", "[expression-parser][type-cast]")
{
    // (int)a * b => ((int)a) * b
    auto ep = MakeExpressionParser();
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Keyword_Int, .Value = "int" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Star, .Value = "*" },
        { .Type = TokType::Identifier, .Value = "b" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<Context>("expression", stream, FullTypeCastExternalParse);
    REQUIRE(result.has_value());
    // top: multiplication
    CHECK(result->Name == "Punctuator_Star-expression");
    // lhs: type cast
    CHECK(ChildIsNode(*result, 1));
    CHECK(ChildName(*result, 1) == "type-cast-expression");
    // rhs: atom b
    CHECK(ChildIsToken(*result, 2));
    CHECK(ChildTokenType(*result, 2) == TokType::Identifier);
    CHECK(ChildTokenValue(*result, 2) == "b");
    CHECK(stream.Index == 5);
}

TEST_CASE("ExpressionParser - Type cast: with expression operand", "[expression-parser][type-cast]")
{
    // (int)(a + b) => type-cast-expression [type: int, value: Plus-expression]
    auto ep = MakeExpressionParser();
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Keyword_Int, .Value = "int" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Identifier, .Value = "a" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "b" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<Context>("expression", stream, FullTypeCastExternalParse);
    REQUIRE(result.has_value());
    CHECK(result->Name == "type-cast-expression");
    // type: int
    CHECK(FindFirstTokenType(ChildNode(*result, 0)) == TokType::Keyword_Int);
    // value: parenthesized expression (a + b)
    CHECK(ChildIsNode(*result, 1));
    CHECK(ChildName(*result, 1) == "Punctuator_Plus-expression");
    CHECK(stream.Index == 7);
}

TEST_CASE("ExpressionParser - Type cast: chained (int)(char)x", "[expression-parser][type-cast]")
{
    // (int)(char)x => (int)((char)x) => type-cast-expression [int, type-cast-expression [char, x]]
    auto ep = MakeExpressionParser();
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Keyword_Int, .Value = "int" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Keyword_Char, .Value = "char" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
        { .Type = TokType::Identifier, .Value = "x" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<Context>("expression", stream, FullTypeCastExternalParse);
    REQUIRE(result.has_value());
    // outer: (int)...
    CHECK(result->Name == "type-cast-expression");
    CHECK(FindFirstTokenType(ChildNode(*result, 0)) == TokType::Keyword_Int);
    // inner: (char)x
    CHECK(ChildIsNode(*result, 1));
    CHECK(ChildName(*result, 1) == "type-cast-expression");
    auto const& inner = ChildNode(*result, 1);
    CHECK(FindFirstTokenType(ChildNode(inner, 0)) == TokType::Keyword_Char);
    CHECK(ChildTokenValue(ChildNode(inner, 1), 0) == "x");
    CHECK(stream.Index == 6);
}

TEST_CASE("ExpressionParser - Type cast: parenthesized expression not type cast", "[expression-parser][type-cast]")
{
    // (x + y) => Punctuator_Plus-expression, NOT type-cast
    // Because x is an identifier, not a type keyword
    auto ep = MakeExpressionParser();
    auto toks = MakeTokens({
        { .Type = TokType::Punctuator_LeftParen, .Value = "(" },
        { .Type = TokType::Identifier, .Value = "x" },
        { .Type = TokType::Punctuator_Plus, .Value = "+" },
        { .Type = TokType::Identifier, .Value = "y" },
        { .Type = TokType::Punctuator_RightParen, .Value = ")" },
    });
    auto stream = MakeStream(toks);
    auto result = ep.Parse<Context>("expression", stream, FullTypeCastExternalParse);
    REQUIRE(result.has_value());
    // Should be a plus expression, not type-cast
    CHECK(result->Name == "Punctuator_Plus-expression");
    CHECK(stream.Index == 4);
}
