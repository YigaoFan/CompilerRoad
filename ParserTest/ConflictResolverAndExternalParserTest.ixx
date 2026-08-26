module;
#include <catch2/catch_all.hpp>
export module ConflictResolverAndExternalParserTest;
import std;
import Base;
import Parser;

using std::vector;
using std::map;
using std::pair;
using std::set;
using std::string;
using std::string_view;
using std::expected;
using std::unexpected;
using std::variant;
using std::array;
using std::stack;
using std::move;
using std::format;
using std::ranges::views::filter;
using std::ranges::to;

// ============================================================================
// Mock token types & token
// ============================================================================

enum class MockTokType { Int, Char, Id, Plus, Star, LPar, RPar, Semicolon, Eof };

struct MockToken
{
    MockTokType Type;
    string Value;
    auto IsEof() const -> bool { return Type == MockTokType::Eof; }
};

template<>
struct std::formatter<MockToken, char> : std::formatter<std::string, char>
{
    auto format(MockToken const& t, std::format_context& fc) const
    {
        return std::format_to(fc.out(), "{}", t.Value);
    }
};

using Node = SyntaxTreeNode<MockToken, void>;
using ParseResult = ParserResult<Node>;

// ============================================================================
// Helpers
// ============================================================================

static auto S(const char* s) -> String { return String(s); }

static auto Rs(std::initializer_list<const char*> symbols) -> SimpleRightSide
{
    SimpleRightSide r;
    for (auto* s : symbols) r.push_back(S(s));
    return r;
}

static auto MakeMockTokens(std::initializer_list<pair<MockTokType, string>> items) -> vector<MockToken>
{
    vector<MockToken> toks;
    for (auto& [type, val] : items)
    {
        toks.push_back({ type, val });
    }
    return toks;
}

// ============================================================================
// No-op stubs for when only one mechanism is under test
// ============================================================================

struct NoExternalParser
{
    auto Parsable(String) const -> bool { return false; }
    auto FirstSet() const -> map<String, set<String>> { return {}; }

    template <typename Result, template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto Parse(String, ActualStream<Tok>&) const -> ParserResult<SyntaxTreeNode<Tok, Result>>
    {
        return unexpected(ParseFailResult{ .Message = "NoExternalParser: not implemented" });
    }
};

class NoConflictResolver
{
public:
    auto Resolvable(String, int) const -> bool { return false; }

    template <template <typename> class ActualStream, typename Token>
        requires Stream<ActualStream, Token>
    auto Resolve(stack<SyntaxTreeNode<Token, void>*>&, String nontermin, MockTokType,
        vector<SimpleRightSide> const&, ActualStream<Token>&) const -> expected<int, ParseFailResult>
    {
        return unexpected(ParseFailResult{ .Message = "NoConflictResolver: not implemented" });
    }
};

// ============================================================================
// MockConflictResolver
//
// A minimal conflict resolver that handles one conflict:
//   nonterminal = "program", tokType = Id
// Resolution: peek at next token — if it's also Id → rule 0 (decl), else → rule 1 (stmt)
// ============================================================================

class MockConflictResolver
{
public:
    auto Resolvable(String nontermin, int tokType) const -> bool
    {
        return nontermin == "program" && static_cast<MockTokType>(tokType) == MockTokType::Id;
    }

    template <template <typename> class ActualStream, typename Token>
        requires Stream<ActualStream, Token>
    auto Resolve(stack<SyntaxTreeNode<Token, void>*>&, String nontermin, MockTokType tokType,
        vector<SimpleRightSide> const& options, ActualStream<Token>& stream) const -> expected<int, ParseFailResult>
    {
        if (nontermin == "program" && tokType == MockTokType::Id)
        {
            // Peek at next token. If it's Id → two consecutive identifiers (e.g. "MyType x") → decl (index 0).
            // Otherwise → stmt (index 1).
            if (stream.MoveNext() && stream.Current().Type == MockTokType::Id)
            {
                return 0; // decl
            }
            return 1; // stmt
        }
        return unexpected(ParseFailResult{ .Message = format("unhandled conflict: {} with {}", nontermin, static_cast<int>(tokType)) });
    }
};

// ============================================================================
// MockExternalParser
//
// A minimal Pratt parser that handles the "expr" nonterminal.
// Supports: atoms (Id), binary + (low prec), binary * (high prec).
// ============================================================================

class MockExternalParser
{
public:
    auto Parsable(String nontermin) const -> bool
    {
        return nontermin == "expr";
    }

    auto FirstSet() const -> map<String, set<String>>
    {
        return { { "expr", { "id", "+", "*" } } };
    }

    template <typename Result, template <typename> class ActualStream, typename Tok>
        requires Stream<ActualStream, Tok>
    auto Parse(String nontermin, ActualStream<Tok>& stream) const -> ParserResult<SyntaxTreeNode<Tok, Result>>
    {
        // TODO pass logger into here
        if (not Parsable(nontermin))
        {
            return unexpected(ParseFailResult{ .Message = format("nonterminal({}) isn't parsable by MockExternalParser", nontermin) });
        }
        return ParseExpr<Result>(stream, 0);
    }

private:
    template <typename Result, template <typename> class ActualStream, typename Tok>
    auto ParseExpr(ActualStream<Tok>& stream, int minPrec) const -> ParserResult<SyntaxTreeNode<Tok, Result>>
    {
        auto lhs = ParseAtom<Result>(stream);
        if (!lhs.has_value()) return lhs;

        for (;;)
        {
            if (!stream.MoveNext()) break;
            auto const& op = stream.Current();
            int prec = GetInfixPrecedence(op.Type);
            if (prec < 0 || prec < minPrec) { stream.Rollback(); break; }

            if (not stream.MoveNext())
            {
                return unexpected(ParseFailResult{ .Message = format("input stream is empty after infix operator({})", op) });
            }
            auto rhs = ParseExpr<Result>(stream, prec + 1);
            if (!rhs.has_value()) return rhs;

            // Build node: op-expr -> [lhs, rhs]
            // SyntaxTreeNode is non-copyable, so build children by pushing individually
            //auto node = ;

            // Left child
            //if (lhs->ChildSymbols.empty()) // leaf: single token child
            //{
            //    node.ChildSymbols.push_back(S(lhs->GetChildAsToken(0).Value.c_str()));
            //    node.Children.push_back(lhs->GetChildAsToken(0));
            //}
            //else
            //{
            //    node.ChildSymbols.push_back(lhs->Name);
            //    node.Children.push_back(move(*lhs));
            //}

            //// Right child
            //if (rhs->ChildSymbols.empty())
            //{
            //    node.ChildSymbols.push_back(S(rhs->GetChildAsToken(0).Value.c_str()));
            //    node.Children.push_back(rhs->GetChildAsToken(0));
            //}
            //else
            //{
            //    node.ChildSymbols.push_back(rhs->Name);
            //    node.Children.push_back(move(*rhs));
            //}

            lhs = Cons(op, array{ move(lhs.value()), move(rhs.value()) });
        }
        return lhs;
    }

    template <typename Result, template <typename> class ActualStream, typename Tok>
    auto ParseAtom(ActualStream<Tok>& stream) const -> ParserResult<SyntaxTreeNode<Tok, Result>>
    {
        auto const& tok = stream.Current();
        if (tok.Type == MockTokType::Id)
        {
            SyntaxTreeNode<Tok, Result> node(S("atom"), { S(tok.Value.c_str()) });
            node.Children.push_back(tok);
            return node;
        }
        return unexpected(ParseFailResult{ .Message = format("unexpected token in expression: {}", tok.Value) });
    }

    template <typename Tok, typename Result, size_t Size>
    static auto Cons(Tok op, array<SyntaxTreeNode<Tok, Result>, Size> items) -> SyntaxTreeNode<Tok, Result>
    {
        auto n = SyntaxTreeNode<Tok, Result>(String(format("{}-expr", op.Value)), { });
        //n.Children.push_back(move(op));

        for (auto i = 0; auto& x : items)
        {
            n.ChildSymbols.push_back(String(format("{}-operand", i++)));
            if (x.Name == "atom")
            {
                n.Children.push_back(move(x.GetChildAsToken(0)));
                continue;
            }
            n.Children.push_back(move(x));
        }
        return n;
    }

    static auto GetInfixPrecedence(MockTokType type) -> int
    {
        switch (type)
        {
        case MockTokType::Plus: return 1;
        case MockTokType::Star: return 2;
        default: return -1;
        }
    }
};

// ============================================================================
// Test 1: Conflict Resolver only
//
// Grammar (mini declaration language with LL(1) conflict):
//   program  -> decl | stmt
//   decl     -> "int" "id" ";" | "char" "id" ";" | "id" "id" ";"
//   stmt     -> "id" ";"
//
// Conflict: Identifier at "program" matches both decl (rule 2) and stmt.
// MockConflictResolver peeks: two consecutive Ids → decl, else → stmt.
// ============================================================================

TEST_CASE("ConflictResolver - without resolver, grammar is not LL(1)", "[conflict]")
{
    SimpleGrammars grammars
    {
        { "program", { Rs({"decl"}), Rs({"stmt"}) } },
        { "decl", {
            Rs({"int", "id", ";"}),
            Rs({"char", "id", ";"}),
            Rs({"id", "id", ";"})
        }},
        { "stmt", { Rs({"id", ";"}) } },
    };

    map<string_view, int> terminal2IntTokenType
    {
        { "\0",   static_cast<int>(MockTokType::Eof) },
        { "int",  static_cast<int>(MockTokType::Int) },
        { "char", static_cast<int>(MockTokType::Char) },
        { "id",   static_cast<int>(MockTokType::Id) },
        { ";",    static_cast<int>(MockTokType::Semicolon) },
    };

    REQUIRE_THROWS_AS(
        LLParser::ConstructFrom(S("program"), move(grammars), terminal2IntTokenType),
        std::logic_error
    );
}

TEST_CASE("ConflictResolver - typedef-like declaration", "[conflict]")
{
    SimpleGrammars grammars
    {
        { "program", { Rs({"decl"}), Rs({"stmt"}) } },
        { "decl", {
            Rs({"int", "id", ";"}),
            Rs({"char", "id", ";"}),
            Rs({"id", "id", ";"})
        }},
        { "stmt", { Rs({"id", ";"}) } },
    };

    map<string_view, int> terminal2IntTokenType
    {
        { "\0",   static_cast<int>(MockTokType::Eof) },
        { "int",  static_cast<int>(MockTokType::Int) },
        { "char", static_cast<int>(MockTokType::Char) },
        { "id",   static_cast<int>(MockTokType::Id) },
        { ";",    static_cast<int>(MockTokType::Semicolon) },
    };

    auto resolver = MockConflictResolver();
    auto noExtParser = NoExternalParser();
    auto p = LLParser::ConstructFrom(S("program"), move(grammars), terminal2IntTokenType, resolver, noExtParser);

    // "MyType x;" → two consecutive Ids → resolver picks decl (rule 2)
    auto toks = MakeMockTokens({
        { MockTokType::Id,  "MyType" },
        { MockTokType::Id,  "x" },
        { MockTokType::Semicolon, ";" },
        { MockTokType::Eof, "" },
    });

    auto result = p.Parse<void>(VectorStream{ .Tokens = move(toks) }, [](auto){}, {}, resolver, noExtParser);
    INFO("Parsing 'MyType x;' should succeed via conflict resolver choosing decl");
    REQUIRE(result.has_value());
    REQUIRE(result->Name == "root");

    // Verify tree: root -> program -> decl -> [MyType, x, ;]
    auto& declProg = result->GetChildAsNode(0);
    REQUIRE(declProg.Name == "program");
    REQUIRE(declProg.ChildSymbols.size() == 1);
    REQUIRE(declProg.ChildSymbols[0] == "decl");

    auto& declNode = declProg.GetChildAsNode(0);
    REQUIRE(declNode.Name == "decl");
    REQUIRE(declNode.ChildSymbols.size() == 3);
    REQUIRE(declNode.GetChildAsToken(0).Value == "MyType");
    REQUIRE(declNode.GetChildAsToken(1).Value == "x");
    REQUIRE(declNode.GetChildAsToken(2).Value == ";");
}

TEST_CASE("ConflictResolver - expression statement", "[conflict]")
{
    SimpleGrammars grammars
    {
        { "program", { Rs({"decl"}), Rs({"stmt"}) } },
        { "decl", {
            Rs({"int", "id", ";"}),
            Rs({"char", "id", ";"}),
            Rs({"id", "id", ";"})
        }},
        { "stmt", { Rs({"id", ";"}) } },
    };

    map<string_view, int> terminal2IntTokenType
    {
        { "\0",   static_cast<int>(MockTokType::Eof) },
        { "int",  static_cast<int>(MockTokType::Int) },
        { "char", static_cast<int>(MockTokType::Char) },
        { "id",   static_cast<int>(MockTokType::Id) },
        { ";",    static_cast<int>(MockTokType::Semicolon) },
    };

    auto resolver = MockConflictResolver();
    auto noExtParser = NoExternalParser();
    auto p = LLParser::ConstructFrom(S("program"), move(grammars), terminal2IntTokenType, resolver, noExtParser);

    // "x;" → single Id → resolver picks stmt (rule 1)
    auto toks = MakeMockTokens({
        { MockTokType::Id,  "x" },
        { MockTokType::Semicolon, ";" },
        { MockTokType::Eof, "" },
    });

    auto result = p.Parse<void>(VectorStream{ .Tokens = move(toks) }, [](auto){}, {}, resolver, noExtParser);
    INFO("Parsing 'x;' should succeed via conflict resolver choosing stmt");
    REQUIRE(result.has_value());

    // Verify tree: root -> program -> stmt -> [x, ;]
    auto& stmtProg = result->GetChildAsNode(0);
    REQUIRE(stmtProg.Name == "program");
    REQUIRE(stmtProg.ChildSymbols.size() == 1);
    REQUIRE(stmtProg.ChildSymbols[0] == "stmt");

    auto& stmtNode = stmtProg.GetChildAsNode(0);
    REQUIRE(stmtNode.Name == "stmt");
    REQUIRE(stmtNode.ChildSymbols.size() == 2);
    REQUIRE(stmtNode.GetChildAsToken(0).Value == "x");
    REQUIRE(stmtNode.GetChildAsToken(1).Value == ";");
}

// ============================================================================
// Test 2: External Parser only
//
// Grammar (mini statement language, expressions delegated to Pratt parser):
//   program -> "expr" ";"
//
// "expr" has NO rules in the grammar — MockExternalParser handles it entirely.
// ============================================================================

TEST_CASE("ExternalParser - single atom", "[external-parser]")
{
    SimpleGrammars grammars
    {
        { "program", { Rs({"expr", ";"}) } },
    };

    map<string_view, int> terminal2IntTokenType
    {
        { "\0",  static_cast<int>(MockTokType::Eof) },
        { "id",  static_cast<int>(MockTokType::Id) },
        { "+",   static_cast<int>(MockTokType::Plus) },
        { "*",   static_cast<int>(MockTokType::Star) },
        { ";",   static_cast<int>(MockTokType::Semicolon) },
    };

    auto noResolver = NoConflictResolver();
    auto extParser = MockExternalParser();
    auto p = LLParser::ConstructFrom(S("program"), move(grammars), terminal2IntTokenType, noResolver, extParser);

    // "id;"
    auto toks = MakeMockTokens({
        { MockTokType::Id, "x" },
        { MockTokType::Semicolon, ";" },
        { MockTokType::Eof, "" },
    });

    auto result = p.Parse<void>(VectorStream{ .Tokens = move(toks) }, [](auto){}, {}, noResolver, extParser);
    INFO("Parsing 'x;' should succeed via external parser");
    if (!result.has_value())
    {
        UNSCOPED_INFO("Parse error: " << result.error().Message);
    }
    REQUIRE(result.has_value());

    // Verify tree: root -> program -> [expr, ;]
    auto& atomProg = result->GetChildAsNode(0);
    REQUIRE(atomProg.Name == "program");
    REQUIRE(atomProg.ChildSymbols.size() == 2);
    REQUIRE(atomProg.ChildSymbols[0] == "expr");
    REQUIRE(atomProg.ChildSymbols[1] == ";");

    // expr is an external-parsed node
    auto& atomExpr = atomProg.GetChildAsNode(0);
    REQUIRE(atomExpr.Name == "atom");
}

TEST_CASE("ExternalParser - addition", "[external-parser]")
{
    SimpleGrammars grammars
    {
        { "program", { Rs({"expr", ";"}) } },
    };

    map<string_view, int> terminal2IntTokenType
    {
        { "\0",  static_cast<int>(MockTokType::Eof) },
        { "id",  static_cast<int>(MockTokType::Id) },
        { "+",   static_cast<int>(MockTokType::Plus) },
        { "*",   static_cast<int>(MockTokType::Star) },
        { ";",   static_cast<int>(MockTokType::Semicolon) },
    };

    auto noResolver = NoConflictResolver();
    auto extParser = MockExternalParser();
    auto p = LLParser::ConstructFrom(S("program"), move(grammars), terminal2IntTokenType, noResolver, extParser);

    // "a + b;"
    auto toks = MakeMockTokens({
        { MockTokType::Id, "a" },
        { MockTokType::Plus, "+" },
        { MockTokType::Id, "b" },
        { MockTokType::Semicolon, ";" },
        { MockTokType::Eof, "" },
    });

    auto result = p.Parse<void>(VectorStream{ .Tokens = move(toks) }, [](auto){}, {}, noResolver, extParser);
    INFO("Parsing 'a + b;' should succeed via external parser");
    REQUIRE(result.has_value());
    if (!result.has_value())
    {
        UNSCOPED_INFO("Parse failed: " << result.error().Message);
    }

    // Verify expr tree: + -> [a, b]
    auto& addProg = result->GetChildAsNode(0);
    auto& addExpr = addProg.GetChildAsNode(0);
    REQUIRE(addExpr.Name == "+-expr");
    REQUIRE(addExpr.ChildSymbols.size() == 2);
    REQUIRE(addExpr.GetChildAsToken(0).Value == "a");
    REQUIRE(addExpr.GetChildAsToken(1).Value == "b");
}

TEST_CASE("ExternalParser - precedence: a + b * c", "[external-parser]")
{
    SimpleGrammars grammars
    {
        { "program", { Rs({"expr", ";"}) } },
    };

    map<string_view, int> terminal2IntTokenType
    {
        { "\0",  static_cast<int>(MockTokType::Eof) },
        { "id",  static_cast<int>(MockTokType::Id) },
        { "+",   static_cast<int>(MockTokType::Plus) },
        { "*",   static_cast<int>(MockTokType::Star) },
        { ";",   static_cast<int>(MockTokType::Semicolon) },
    };

    auto noResolver = NoConflictResolver();
    auto extParser = MockExternalParser();
    auto p = LLParser::ConstructFrom(S("program"), move(grammars), terminal2IntTokenType, noResolver, extParser);

    // "a + b * c;" → should parse as a + (b * c)
    auto toks = MakeMockTokens({
        { MockTokType::Id, "a" },
        { MockTokType::Plus, "+" },
        { MockTokType::Id, "b" },
        { MockTokType::Star, "*" },
        { MockTokType::Id, "c" },
        { MockTokType::Semicolon, ";" },
        { MockTokType::Eof, "" },
    });

    auto result = p.Parse<void>(VectorStream{ .Tokens = move(toks) }, [](auto){}, {}, noResolver, extParser);
    INFO("Parsing 'a + b * c;' should succeed with correct precedence");
    REQUIRE(result.has_value());

    // Verify expr tree: +-expr -> [a, *-expr -> [b, c]]
    // MockExternalParser stores operands directly (no operator token)
    auto& mulProg = result->GetChildAsNode(0);
    auto& mulExpr = mulProg.GetChildAsNode(0);
    REQUIRE(mulExpr.Name == "+-expr");

    // Left operand: token "a"
    REQUIRE(mulExpr.GetChildAsToken(0).Value == "a");

    // Right operand: *-expr -> [b, c]
    auto& mulMul = mulExpr.GetChildAsNode(1);
    REQUIRE(mulMul.Name == "*-expr");
    REQUIRE(mulMul.GetChildAsToken(0).Value == "b");
    REQUIRE(mulMul.GetChildAsToken(1).Value == "c");
}

TEST_CASE("ExternalParser - precedence: a * b + c", "[external-parser]")
{
    SimpleGrammars grammars
    {
        { "program", { Rs({"expr", ";"}) } },
    };

    map<string_view, int> terminal2IntTokenType
    {
        { "\0",  static_cast<int>(MockTokType::Eof) },
        { "id",  static_cast<int>(MockTokType::Id) },
        { "+",   static_cast<int>(MockTokType::Plus) },
        { "*",   static_cast<int>(MockTokType::Star) },
        { ";",   static_cast<int>(MockTokType::Semicolon) },
    };

    auto noResolver = NoConflictResolver();
    auto extParser = MockExternalParser();
    auto p = LLParser::ConstructFrom(S("program"), move(grammars), terminal2IntTokenType, noResolver, extParser);

    // "a * b + c;" → should parse as (a * b) + c
    auto toks = MakeMockTokens({
        { MockTokType::Id, "a" },
        { MockTokType::Star, "*" },
        { MockTokType::Id, "b" },
        { MockTokType::Plus, "+" },
        { MockTokType::Id, "c" },
        { MockTokType::Semicolon, ";" },
        { MockTokType::Eof, "" },
    });

    auto result = p.Parse<void>(VectorStream{ .Tokens = move(toks) }, [](auto){}, {}, noResolver, extParser);
    INFO("Parsing 'a * b + c;' should succeed with correct precedence");
    REQUIRE(result.has_value());

    // Verify expr tree: +-expr -> [*-expr -> [a, b], c]
    // MockExternalParser stores operands directly (no operator token)
    auto& pbrProg = result->GetChildAsNode(0);
    auto& pbrExpr = pbrProg.GetChildAsNode(0);
    REQUIRE(pbrExpr.Name == "+-expr");

    // Left operand: *-expr -> [a, b]
    auto& pbrMul = pbrExpr.GetChildAsNode(0);
    REQUIRE(pbrMul.Name == "*-expr");
    REQUIRE(pbrMul.GetChildAsToken(0).Value == "a");
    REQUIRE(pbrMul.GetChildAsToken(1).Value == "b");

    // Right operand: token "c"
    REQUIRE(pbrExpr.GetChildAsToken(1).Value == "c");
}

// ============================================================================
// Test 3: Conflict Resolver + External Parser combined
//
// Grammar (declarations + expression statements):
//   program  -> decl stmt | stmt
//   decl     -> "int" "id" ";" | "char" "id" ";" | "id" "id" ";"
//   stmt     -> expr ";"
//
// Conflict: Identifier at "program" matches both decl (rule 2) and stmt
//   (via external parser FIRST set containing "id").
// MockConflictResolver resolves; MockExternalParser handles "expr".
// ============================================================================

TEST_CASE("ConflictResolver + ExternalParser - declaration then expression", "[conflict][external-parser]")
{
    SimpleGrammars grammars
    {
        { "program", { Rs({"decl", "stmt"}), Rs({"stmt"}) } },
        { "decl", {
            Rs({"int", "id", ";"}),
            Rs({"char", "id", ";"}),
            Rs({"id", "id", ";"})
        }},
        { "stmt", { Rs({"expr", ";"}) } },
    };

    map<string_view, int> terminal2IntTokenType
    {
        { "\0",   static_cast<int>(MockTokType::Eof) },
        { "int",  static_cast<int>(MockTokType::Int) },
        { "char", static_cast<int>(MockTokType::Char) },
        { "id",   static_cast<int>(MockTokType::Id) },
        { "+",    static_cast<int>(MockTokType::Plus) },
        { "*",    static_cast<int>(MockTokType::Star) },
        { ";",    static_cast<int>(MockTokType::Semicolon) },
    };

    auto resolver = MockConflictResolver();
    auto extParser = MockExternalParser();
    auto p = LLParser::ConstructFrom(S("program"), move(grammars), terminal2IntTokenType, resolver, extParser);

    // "MyType x; a + b;" → decl (typedef-like) then stmt (expression)
    auto toks = MakeMockTokens({
        { MockTokType::Id, "MyType" },
        { MockTokType::Id, "x" },
        { MockTokType::Semicolon, ";" },
        { MockTokType::Id, "a" },
        { MockTokType::Plus, "+" },
        { MockTokType::Id, "b" },
        { MockTokType::Semicolon, ";" },
        { MockTokType::Eof, "" },
    });

    auto result = p.Parse<void>(VectorStream{ .Tokens = move(toks) }, [](auto){}, {}, resolver, extParser);
    INFO("Parsing 'MyType x; a + b;' should succeed with both resolver and external parser");
    REQUIRE(result.has_value());
    if (!result.has_value())
    {
        UNSCOPED_INFO("Parse failed: " << result.error().Message);
    }

    // Verify tree: root -> program -> [decl, stmt]
    auto& combProg = result->GetChildAsNode(0);
    REQUIRE(combProg.Name == "program");
    REQUIRE(combProg.ChildSymbols.size() == 2);
    REQUIRE(combProg.ChildSymbols[0] == "decl");
    REQUIRE(combProg.ChildSymbols[1] == "stmt");

    // decl: [MyType, x, ;]
    auto& combDecl = combProg.GetChildAsNode(0);
    REQUIRE(combDecl.Name == "decl");
    REQUIRE(combDecl.GetChildAsToken(0).Value == "MyType");
    REQUIRE(combDecl.GetChildAsToken(1).Value == "x");
    REQUIRE(combDecl.GetChildAsToken(2).Value == ";");

    // stmt: [expr, ;] where MockExternalParser returns +-expr directly
    auto& combStmt = combProg.GetChildAsNode(1);
    REQUIRE(combStmt.Name == "stmt");
    REQUIRE(combStmt.ChildSymbols.size() == 2);
    REQUIRE(combStmt.ChildSymbols[0] == "expr");
    REQUIRE(combStmt.ChildSymbols[1] == ";");

    // MockExternalParser produces "+-expr" directly (no intermediate "expr" wrapper)
    auto& combExpr = combStmt.GetChildAsNode(0);
    REQUIRE(combExpr.Name == "+-expr");
    REQUIRE(combExpr.GetChildAsToken(0).Value == "a");
    REQUIRE(combExpr.GetChildAsToken(1).Value == "b");
}

TEST_CASE("ConflictResolver + ExternalParser - expression only (no decl)", "[conflict][external-parser]")
{
    SimpleGrammars grammars
    {
        { "program", { Rs({"decl", "stmt"}), Rs({"stmt"}) } },
        { "decl", {
            Rs({"int", "id", ";"}),
            Rs({"char", "id", ";"}),
            Rs({"id", "id", ";"})
        }},
        { "stmt", { Rs({"expr", ";"}) } },
    };

    map<string_view, int> terminal2IntTokenType
    {
        { "\0",   static_cast<int>(MockTokType::Eof) },
        { "int",  static_cast<int>(MockTokType::Int) },
        { "char", static_cast<int>(MockTokType::Char) },
        { "id",   static_cast<int>(MockTokType::Id) },
        { "+",    static_cast<int>(MockTokType::Plus) },
        { "*",    static_cast<int>(MockTokType::Star) },
        { ";",    static_cast<int>(MockTokType::Semicolon) },
    };

    auto resolver = MockConflictResolver();
    auto extParser = MockExternalParser();
    auto p = LLParser::ConstructFrom(S("program"), move(grammars), terminal2IntTokenType, resolver, extParser);

    // "x;" → resolver picks stmt (single Id), external parser handles "x"
    auto toks = MakeMockTokens({
        { MockTokType::Id, "x" },
        { MockTokType::Semicolon, ";" },
        { MockTokType::Eof, "" },
    });

    auto result = p.Parse<void>(VectorStream{ .Tokens = move(toks) }, [](auto){}, {}, resolver, extParser);
    INFO("Parsing 'x;' should succeed via resolver choosing stmt + external parser handling expr");
    REQUIRE(result.has_value());

    // Verify tree: root -> program -> stmt -> [expr, ;]
    auto& exprProg = result->GetChildAsNode(0);
    REQUIRE(exprProg.Name == "program");
    REQUIRE(exprProg.ChildSymbols.size() == 1);
    REQUIRE(exprProg.ChildSymbols[0] == "stmt");

    auto& exprStmt = exprProg.GetChildAsNode(0);
    REQUIRE(exprStmt.Name == "stmt");
    REQUIRE(exprStmt.ChildSymbols[0] == "expr");
    REQUIRE(exprStmt.ChildSymbols[1] == ";");

    // MockExternalParser produces "atom" directly (no intermediate "expr" wrapper)
    auto& exprExpr = exprStmt.GetChildAsNode(0);
    REQUIRE(exprExpr.Name == "atom");
}
