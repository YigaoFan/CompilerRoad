module;
#include <catch2/catch_all.hpp>
export module GrammarHistoryUndoTest;
import std;
import Base;
import Parser;

using std::vector;
using std::variant;
using std::map;
using std::stack;
using std::pair;
using std::size_t;
using std::move;
using std::string;
using std::string_view;

enum class TokType { Ident, Plus, Num, LPar, RPar };

struct MockToken
{
    TokType Type;
    string Value;
    auto IsEof() const -> bool { return false; }
};

using Node = SyntaxTreeNode<MockToken, void>;

static auto MakeToken(TokType type, string value) -> MockToken
{
    return { type, move(value) };
}

static auto Leaf(MockToken tok) -> variant<MockToken, Node>
{
    return tok;
}

static auto Branch(Node node) -> variant<MockToken, Node>
{
    return move(node);
}

static auto NoOpCallback = [](Node*) {};

/// <summary>
/// helper: build a vector of children from variadic variant arguments
/// (avoids initializer_list which can't hold move-only types)
/// </summary>
static auto MakeChildren() -> vector<variant<MockToken, Node>>
{
    return {};
}

template<typename... Rest>
static auto MakeChildren(variant<MockToken, Node> first, Rest&&... rest) -> vector<variant<MockToken, Node>>
{
    vector<variant<MockToken, Node>> v;
    v.reserve(1 + sizeof...(rest));
    v.push_back(move(first));
    (v.push_back(move(rest)), ...);
    return v;
}

/// <summary>
/// helper: build a node with name, childSymbols, and children
/// </summary>
static auto MakeNode(String name, vector<String> childSymbols, vector<variant<MockToken, Node>> children = {}) -> Node
{
    return Node{ move(name), move(childSymbols), move(children) };
}

/// <summary>
/// helper: get Node pointer from variant (nullptr if it's a token)
/// </summary>
static auto AsNode(variant<MockToken, Node>& v) -> Node*
{
    return std::get_if<Node>(&v);
}

/// <summary>
/// helper: get MockToken pointer from variant (nullptr if it's a node)
/// </summary>
static auto AsToken(variant<MockToken, Node>& v) -> MockToken*
{
    return std::get_if<MockToken>(&v);
}

static auto S(const char* s) -> String { return String(s); }
static auto Rs(std::initializer_list<const char*> symbols) -> SimpleRightSide
{
    SimpleRightSide r;
    for (auto* s : symbols) r.push_back(S(s));
    return r;
}

static auto HasKey(SimpleGrammars const& g, const char* key) -> bool
{
    return g.contains(S(key));
}

// ============================================================================
// ExpandFront::Undo tests
// ============================================================================

// Single-level expansion: B -> [a, b] with ExpandFront(A, ["a"])
//
// Before undo:
//   B
//   ├── a
//   └── b
//
// After undo:
//   B
//   ├── A
//   │   └── a
//   └── b
TEST_CASE("ExpandFront::Undo - single-level expansion", "[parser][undo]")
{
    History history;
    history.Add(std::make_shared<History::ExpandFront>(S("A"), Rs({"a"}), Rs({"a", "b"})));

    auto root = MakeNode(S("B"), {S("a"), S("b")},
        MakeChildren(Leaf(MakeToken(TokType::Ident, "a")), Leaf(MakeToken(TokType::Ident, "b"))));

    auto result = history.Undo(move(root));

    // RightSide = ["a"] (size 1), packs 1 child as A
    REQUIRE(result.Name == "B");
    REQUIRE(result.ChildSymbols.size() == 2);
    REQUIRE(result.ChildSymbols[0] == "A");
    REQUIRE(result.ChildSymbols[1] == "b");

    auto* aPtr = AsNode(result.Children[0]);
    REQUIRE(aPtr != nullptr);
    REQUIRE(aPtr->Name == "A");
    REQUIRE(aPtr->ChildSymbols.size() == 1);
    REQUIRE(aPtr->ChildSymbols[0] == "a");
}

// Two-level expansion: only the outermost Undo is applied.
// Inner ExpandFront doesn't match after outer undo (known design limitation).
TEST_CASE("ExpandFront::Undo - two-level chained expansion", "[parser][undo]")
{
    History history;
    history.Add(std::make_shared<History::ExpandFront>(S("A"), Rs({"a"}), Rs({"a", "c"})));
    history.Add(std::make_shared<History::ExpandFront>(S("B"), Rs({"a", "c"}), Rs({"a", "c", "d"})));

    auto root = MakeNode(S("C"), {S("a"), S("c"), S("d")},
        MakeChildren(
            Leaf(MakeToken(TokType::Ident, "a")),
            Leaf(MakeToken(TokType::Ident, "c")),
            Leaf(MakeToken(TokType::Ident, "d"))));

    // Outermost undo (B) succeeds, inner (A) fails because ChildSymbols no longer match
    REQUIRE_THROWS(history.Undo(move(root)));
}

// Empty history: Undo returns node unchanged
TEST_CASE("ExpandFront::Undo - empty history returns unchanged", "[parser][undo]")
{
    History history;

    auto root = MakeNode(S("A"), {S("a")},
        MakeChildren(Leaf(MakeToken(TokType::Ident, "a"))));

    auto result = history.Undo(move(root));

    REQUIRE(result.Name == "A");
    REQUIRE(result.ChildSymbols.size() == 1);
}

// ============================================================================

// Multiple children in RightSide: packs multiple children as A
//
// Before undo:
//   B
//   ├── a
//   ├── b
//   └── c
//
// After undo:
//   B
//   ├── A
//   │   ├── a
//   │   └── b
//   └── c
TEST_CASE("ExpandFront::Undo - multiple children in RightSide", "[parser][undo]")
{
    History history;
    history.Add(std::make_shared<History::ExpandFront>(S("A"), Rs({"a", "b"}), Rs({"a", "b", "c"})));

    auto root = MakeNode(S("B"), {S("a"), S("b"), S("c")},
        MakeChildren(
            Leaf(MakeToken(TokType::Ident, "a")),
            Leaf(MakeToken(TokType::Ident, "b")),
            Leaf(MakeToken(TokType::Ident, "c"))));

    auto result = history.Undo(move(root));

    // RightSide = ["a", "b"] (size 2), packs 2 children as A
    REQUIRE(result.Name == "B");
    REQUIRE(result.ChildSymbols.size() == 2);
    REQUIRE(result.ChildSymbols[0] == "A");
    REQUIRE(result.ChildSymbols[1] == "c");

    auto* aPtr = AsNode(result.Children[0]);
    REQUIRE(aPtr != nullptr);
    REQUIRE(aPtr->Name == "A");
    REQUIRE(aPtr->ChildSymbols.size() == 2);
    REQUIRE(aPtr->ChildSymbols[0] == "a");
    REQUIRE(aPtr->ChildSymbols[1] == "b");
}
// ============================================================================

// Helper to build a minimal Left2RightRecur context (Undo doesn't use it for chain restructuring)
static auto MakeMinimalCtx() -> History::Left2RightRecur::Context
{
    return
    {
        .OriginalGrammar = { S("A"), { Rs({"a"}) } },
        .AuxiliaryGrammar = { S("A'"), { {} } },
        .OriginalFrontReplaceHistory = {} ,
    };
}

// Simple two-level chain:
//
// Before undo:
//   A
//   ├── tok1
//   └── A'
//       ├── tok2
//       └── A' (ε)
//
// After undo:
//   A
//   ├── A
//   │   └── tok1
//   └── tok2
TEST_CASE("Left2RightRecur::Undo - simple two-level chain", "[parser][undo]")
{
    auto epsilon = MakeNode(S("A'"), {}, {});
    auto innerA = MakeNode(S("A'"), {S("tok2"), S("A'")},
        MakeChildren(Leaf(MakeToken(TokType::Ident, "tok2")), Branch(move(epsilon))));
    auto root = MakeNode(S("A"), {S("tok1"), S("A'")},
        MakeChildren(Leaf(MakeToken(TokType::Ident, "tok1")), Branch(move(innerA))));

    History history;
    history.Add(std::make_shared<History::Left2RightRecur>(MakeMinimalCtx()));
    auto result = history.Undo(move(root));

    // else returns toAddChild (accumulated result)
    REQUIRE(result.Name == "A");
    REQUIRE(result.ChildSymbols.size() == 2);
    REQUIRE(result.ChildSymbols[0] == "A");
    REQUIRE(result.ChildSymbols[1] == "tok2");

    auto* a1 = AsNode(result.Children[0]);
    REQUIRE(a1 != nullptr);
    REQUIRE(a1->Name == "A");
    REQUIRE(a1->ChildSymbols.size() == 1);
    REQUIRE(a1->ChildSymbols[0] == "tok1");
    REQUIRE(AsToken(a1->Children[0])->Value == "tok1");
    REQUIRE(AsToken(result.Children[1])->Value == "tok2");
}

// Three-level chain:
//
// Before undo:
//   A
//   ├── tok1
//   └── A'
//       ├── tok2
//       └── A'
//           ├── tok3
//           └── A' (ε)
//
// After undo:
//   A
//   ├── A
//   │   ├── A
//   │   │   └── tok1
//   │   └── tok2
//   └── tok3
TEST_CASE("Left2RightRecur::Undo - three-level chain", "[parser][undo]")
{
    auto epsilon = MakeNode(S("A'"), {}, {});
    auto l3 = MakeNode(S("A'"), {S("tok3"), S("A'")},
        MakeChildren(Leaf(MakeToken(TokType::Ident, "tok3")), Branch(move(epsilon))));
    auto l2 = MakeNode(S("A'"), {S("tok2"), S("A'")},
        MakeChildren(Leaf(MakeToken(TokType::Ident, "tok2")), Branch(move(l3))));
    auto root = MakeNode(S("A"), {S("tok1"), S("A'")},
        MakeChildren(Leaf(MakeToken(TokType::Ident, "tok1")), Branch(move(l2))));

    History history;
    history.Add(std::make_shared<History::Left2RightRecur>(MakeMinimalCtx()));
    auto result = history.Undo(move(root));

    REQUIRE(result.Name == "A");
    REQUIRE(result.ChildSymbols.size() == 2);
    REQUIRE(result.ChildSymbols[1] == "tok3");

    auto* l1 = AsNode(result.Children[0]);
    REQUIRE(l1 != nullptr);
    REQUIRE(l1->Name == "A");
    REQUIRE(l1->ChildSymbols.size() == 2);
    REQUIRE(l1->ChildSymbols[1] == "tok2");

    auto* l2r = AsNode(l1->Children[0]);
    REQUIRE(l2r != nullptr);
    REQUIRE(l2r->Name == "A");
    REQUIRE(l2r->ChildSymbols.size() == 1);
    REQUIRE(l2r->ChildSymbols[0] == "tok1");

    REQUIRE(AsToken(l2r->Children[0])->Value == "tok1");
    REQUIRE(AsToken(l1->Children[1])->Value == "tok2");
    REQUIRE(AsToken(result.Children[1])->Value == "tok3");
}

// Four-level chain:
//
// Before undo:
//   A
//   ├── tok1
//   └── A'
//       ├── tok2
//       └── A'
//           ├── tok3
//           └── A'
//               ├── tok4
//               └── A' (ε)
//
// After undo:
//   A
//   ├── A
//   │   ├── A
//   │   │   ├── A
//   │   │   │   └── tok1
//   │   │   └── tok2
//   │   └── tok3
//   └── tok4
TEST_CASE("Left2RightRecur::Undo - four-level chain", "[parser][undo]")
{
    auto epsilon = MakeNode(S("A'"), {}, {});
    auto l4 = MakeNode(S("A'"), {S("tok4"), S("A'")},
        MakeChildren(Leaf(MakeToken(TokType::Num, "tok4")), Branch(move(epsilon))));
    auto l3 = MakeNode(S("A'"), {S("tok3"), S("A'")},
        MakeChildren(Leaf(MakeToken(TokType::Num, "tok3")), Branch(move(l4))));
    auto l2 = MakeNode(S("A'"), {S("tok2"), S("A'")},
        MakeChildren(Leaf(MakeToken(TokType::Num, "tok2")), Branch(move(l3))));
    auto root = MakeNode(S("A"), {S("tok1"), S("A'")},
        MakeChildren(Leaf(MakeToken(TokType::Num, "tok1")), Branch(move(l2))));

    History history;
    history.Add(std::make_shared<History::Left2RightRecur>(MakeMinimalCtx()));
    auto result = history.Undo(move(root));

    REQUIRE(result.Name == "A");
    REQUIRE(result.ChildSymbols[1] == "tok4");

    auto* a1 = AsNode(result.Children[0]);
    REQUIRE(a1 != nullptr);
    REQUIRE(a1->Name == "A");
    REQUIRE(a1->ChildSymbols[1] == "tok3");

    auto* a2 = AsNode(a1->Children[0]);
    REQUIRE(a2 != nullptr);
    REQUIRE(a2->Name == "A");
    REQUIRE(a2->ChildSymbols[1] == "tok2");

    auto* a3 = AsNode(a2->Children[0]);
    REQUIRE(a3 != nullptr);
    REQUIRE(a3->Name == "A");
    REQUIRE(a3->ChildSymbols[0] == "tok1");

    REQUIRE(AsToken(a3->Children[0])->Value == "tok1");
    REQUIRE(AsToken(a2->Children[1])->Value == "tok2");
    REQUIRE(AsToken(a1->Children[1])->Value == "tok3");
    REQUIRE(AsToken(result.Children[1])->Value == "tok4");
}

// Empty root: no children, no-op
//
// Before undo: A (no children)
// After undo:  A (no children)
TEST_CASE("Left2RightRecur::Undo - empty root node", "[parser][undo]")
{
    auto root = MakeNode(S("A"), {}, {});

    History history;
    history.Add(std::make_shared<History::Left2RightRecur>(MakeMinimalCtx()));
    auto result = history.Undo(move(root));

    REQUIRE(result.Name == "A");
    REQUIRE(result.Children.empty());
    REQUIRE(result.ChildSymbols.empty());
}

// Single terminal child: SplitOutLastChild calls std::get<Node> on a token → throws
//
// Before undo:
//   A
//   └── tok1 (token, not a node → bad_variant_access)
TEST_CASE("Left2RightRecur::Undo - single token throws", "[parser][undo]")
{
    auto root = MakeNode(S("A"), {S("tok1")},
        MakeChildren(Leaf(MakeToken(TokType::Ident, "tok1"))));

    History history;
    history.Add(std::make_shared<History::Left2RightRecur>(MakeMinimalCtx()));
    REQUIRE_THROWS_AS(history.Undo(move(root)), std::bad_variant_access);
}

// A' has no right-recursive child: A -> [tok1, A'], A' -> [tok2]
//
// Before undo:
//   A
//   ├── tok1
//   └── A'
//       └── tok2
//
// After undo (else returns toAddChild):
//   A
//   └── tok1
TEST_CASE("Left2RightRecur::Undo - no right-recur child", "[parser][undo]")
{
    auto innerA = MakeNode(S("A'"), {S("tok2")},
        MakeChildren(Leaf(MakeToken(TokType::Ident, "tok2"))));
    auto root = MakeNode(S("A"), {S("tok1"), S("A'")},
        MakeChildren(Leaf(MakeToken(TokType::Ident, "tok1")), Branch(move(innerA))));

    History history;
    history.Add(std::make_shared<History::Left2RightRecur>(MakeMinimalCtx()));
    auto result = history.Undo(move(root));

    // else returns toAddChild = A[tok1] (A' had no auxiliary last child)
    REQUIRE(result.Name == "A");
    REQUIRE(result.ChildSymbols.size() == 1);
    REQUIRE(result.ChildSymbols[0] == "tok1");
    REQUIRE(AsToken(result.Children[0])->Value == "tok1");
}

// Realistic grammar: A -> B a | b, B -> A b | a
// After RemoveIndirectLeftRecur:
// Original: A -> B a | b, B -> A b | a
// Step 1: Expand B -> A b with A's rules:
//   - A -> B a  => B -> B a b
//   - A -> b    => B -> b b
//   - Keep B -> a (unchanged)
// Step 2: Convert left recursion in B -> B a b | b b | a:
//   - Base (non-left-recursive): B -> b b B' | a B'
//   - Recursive: B' -> a b B' | ε
// Step 3: Update A using converted B rules:
//   - A -> B a  => A -> (b b B' | a) a  => A -> b b B' a | a a
//   - A -> b    (unchanged)
//
// Auxiliary grammar for B (used in converted rule [b, b, B']):
//   B' -> a b B' | ε
//
// Converted rule under test: B -> [b, b, B']
//
// Before undo:
//   B
//   ├── b
//   ├── b
//   └── B' (ε)
//
// After undo:
//   B
//   ├── A
//   │   └── b
//   └── b
TEST_CASE("Left2RightRecur::Undo - realistic grammar via RemoveIndirectLeftRecur", "[parser][undo]")
{
    SimpleGrammars grammars;
    grammars[S("A")] = { Rs({"B", "a"}), Rs({"b"}) };
    grammars[S("B")] = { Rs({"A", "b"}), Rs({"a"}) };
    auto [convertedGrammars, replaceHistory] = RemoveIndirectLeftRecur(move(grammars));

    // B's first rule in converted grammar (right-recursive form)
    auto& bRules = convertedGrammars.at(S("B"));
    REQUIRE(bRules.size() >= 1);
    auto& rule = bRules[0];

    // Build a parse tree from the converted rule
    vector<String> childSymbols;
    vector<variant<MockToken, Node>> children;
    for (auto const& sym : rule)
    {
        childSymbols.push_back(sym);
        if (convertedGrammars.contains(sym))
            children.push_back(Branch(MakeNode(sym, {}, {})));
        else
        {
            string_view sv = sym;
            children.push_back(Leaf(MakeToken(TokType::Ident, string(sv))));
        }
    }
    auto root = MakeNode(S("B"), move(childSymbols), move(children));

    // Undo using the history from RemoveIndirectLeftRecur
    auto bHist = replaceHistory.at(S("B")).at(0);
    auto result = bHist.Undo(move(root));

    // RecoverExpand: Find([b, b]) matches OriginalGrammar index 1
    // ExpandFront(A, RightSide=[b]) packs 1 child as A → B[A[b], b]
    REQUIRE(result.Name == "B");
    REQUIRE(result.ChildSymbols.size() == 2);
    REQUIRE(result.ChildSymbols[0] == "A");
    REQUIRE(result.ChildSymbols[1] == "b");

    auto* innerA = AsNode(result.Children[0]);
    REQUIRE(innerA != nullptr);
    REQUIRE(innerA->Name == "A");
    REQUIRE(innerA->ChildSymbols.size() == 1);
    REQUIRE(innerA->ChildSymbols[0] == "b");
}

// Grammar: A -> a, B -> A b | B c
// Converted: B -> a b B', B' -> c B' | ε
//
// Before undo:
//   B
//   ├── a
//   ├── b
//   └── B' (ε)
//
// After undo:
//   B
//   ├── A
//   │   └── a
//   └── b
TEST_CASE("Left2RightRecur::Undo - OriginalFrontReplaceHistory lookup", "[parser][undo]")
{
    SimpleGrammars grammars;
    grammars[S("A")] = { Rs({"a"}) };
    grammars[S("B")] = { Rs({"A", "b"}), Rs({"B", "c"}) };
    auto [convertedGrammars, replaceHistory] = RemoveIndirectLeftRecur(move(grammars));

    REQUIRE(HasKey(convertedGrammars, "B"));
    REQUIRE(HasKey(convertedGrammars, "B'"));

    // Build parse tree from B's first converted rule: [a, b, B']
    auto& bRules = convertedGrammars.at(S("B"));
    auto& rule = bRules[0];
    auto epsilonB = MakeNode(String(rule[2]), {}, {});
    auto root = MakeNode(S("B"), {String(rule[0]), String(rule[1]), String(rule[2])},
        MakeChildren(
            Leaf(MakeToken(TokType::Ident, string(string_view(rule[0])))),
            Leaf(MakeToken(TokType::Ident, string(string_view(rule[1])))),
            Branch(move(epsilonB))));

    auto bHist = replaceHistory.at(S("B")).at(0);
    auto result = bHist.Undo(move(root));

    // sentinel returns toAddChild, then RecoverExpand on root
    // RightSide.size()=1 → packs 1 child as A → B[A[a], b]
    REQUIRE(result.Name == "B");
    REQUIRE(result.ChildSymbols.size() == 2);
    REQUIRE(result.ChildSymbols[0] == "A");
    REQUIRE(result.ChildSymbols[1] == "b");

    auto* aPtr = AsNode(result.Children[0]);
    REQUIRE(aPtr != nullptr);
    REQUIRE(aPtr->Name == "A");
    REQUIRE(aPtr->ChildSymbols.size() == 1);
    REQUIRE(aPtr->ChildSymbols[0] == "a");
}

// Multi-level right-recursive chain:
// Grammar: A -> a, B -> A b | B c
// Converted: B -> a b B', B' -> c B' | ε
//
// Before undo:
//   B
//   ├── a
//   ├── b
//   └── B'
//       ├── c
//       └── B'
//           ├── c
//           └── B' (ε)
//
// After undo:
//   B
//   ├── B
//   │   ├── B
//   │   │   ├── A
//   │   │   │   └── a
//   │   │   └── b
//   │   └── c
//   └── c
//
// line 165 Find: [B, c] matches OriginalGrammar index 1 — no ExpandFront entry
//                [B', c] → B' not in OriginalGrammar → no match
//     → InsertAtHead → B'ε[B'[B'[B[a, b], c], c]]
TEST_CASE("Left2RightRecur::Undo - multi-level chain with OriginalFrontReplaceHistory lookup", "[parser][undo]")
{
    SimpleGrammars grammars;
    grammars[S("A")] = { Rs({"a"}) };
    grammars[S("B")] = { Rs({"A", "b"}), Rs({"B", "c"}) };
    auto [convertedGrammars, replaceHistory] = RemoveIndirectLeftRecur(move(grammars));

    // Build parse tree: B -> [a, b, B'[c, B'[c, B'ε]]]
    auto innerBEpsilon = MakeNode(S("B'"), {}, {});
    auto innerB = MakeNode(S("B'"), {S("c"), S("B'")},
        MakeChildren(Leaf(MakeToken(TokType::Ident, "c")), Branch(move(innerBEpsilon))));
    auto outerB = MakeNode(S("B'"), {S("c"), S("B'")},
        MakeChildren(Leaf(MakeToken(TokType::Ident, "c")), Branch(move(innerB))));
    auto root = MakeNode(S("B"), {S("a"), S("b"), S("B'")},
        MakeChildren(
            Leaf(MakeToken(TokType::Ident, "a")),
            Leaf(MakeToken(TokType::Ident, "b")),
            Branch(move(outerB))));

    auto bHist = replaceHistory.at(S("B")).at(0);
    auto result = bHist.Undo(move(root));

    // sentinel returns toAddChild; if branch renames intermediate B' to B
    REQUIRE(result.Name == "B");
    REQUIRE(result.ChildSymbols.size() == 2);
    REQUIRE(result.ChildSymbols[1] == "c");

    // B (from second level) with [B, c]
    auto* l1 = AsNode(result.Children[0]);
    REQUIRE(l1 != nullptr);
    REQUIRE(l1->Name == "B");
    REQUIRE(l1->ChildSymbols.size() == 2);
    REQUIRE(l1->ChildSymbols[0] == "B");
    REQUIRE(l1->ChildSymbols[1] == "c");

    // RecoverExpand on root: Find([a, b]) matches OriginalGrammar index 0
    // ExpandFront(A, RightSide=[a]) packs 1 child as A → B[A[a], b]
    auto* l2 = AsNode(l1->Children[0]);
    REQUIRE(l2 != nullptr);
    REQUIRE(l2->Name == "B");
    REQUIRE(l2->ChildSymbols.size() == 2);
    REQUIRE(l2->ChildSymbols[0] == "A");
    REQUIRE(l2->ChildSymbols[1] == "b");

    auto* aPtr = AsNode(l2->Children[0]);
    REQUIRE(aPtr != nullptr);
    REQUIRE(aPtr->Name == "A");
    REQUIRE(aPtr->ChildSymbols.size() == 1);
    REQUIRE(aPtr->ChildSymbols[0] == "a");
}
