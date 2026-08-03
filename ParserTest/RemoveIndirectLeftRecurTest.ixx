module;
#include <catch2/catch_all.hpp>
export module RemoveIndirectLeftRecurTest;
import std;
import Base;
import Parser;

using std::vector;
using std::variant;
using std::map;
using std::pair;
using std::set;
using std::move;
using std::string;
using std::string_view;
using std::size_t;

// ============================================================================
// Mock token & helpers for syntax tree undo tests
// ============================================================================

enum class TokType { Ident, Comma, Eof };

struct MockToken
{
    TokType Type;
    string Value;
    auto IsEof() const -> bool { return Type == TokType::Eof; }
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

template<typename... Rest>
static auto MakeChildren(variant<MockToken, Node> first, Rest&&... rest) -> vector<variant<MockToken, Node>>
{
    vector<variant<MockToken, Node>> v;
    v.reserve(1 + sizeof...(rest));
    v.push_back(move(first));
    (v.push_back(move(rest)), ...);
    return v;
}

static auto MakeNode(String name, vector<String> childSymbols, vector<variant<MockToken, Node>> children = {}) -> Node
{
    return Node{ move(name), move(childSymbols), move(children) };
}

static auto AsNode(variant<MockToken, Node>& v) -> Node*
{
    return std::get_if<Node>(&v);
}

static auto AsToken(variant<MockToken, Node>& v) -> MockToken*
{
    return std::get_if<MockToken>(&v);
}

// helpers to build grammar data concisely
static auto S(const char* s) -> String { return String(s); }
static auto Rs(std::initializer_list<const char*> symbols) -> SimpleRightSide
{
    SimpleRightSide r;
    for (auto* s : symbols) r.push_back(S(s));
    return r;
}

/// build a one-key SimpleGrammars from a left side and rules
static auto G(const char* left, std::initializer_list<SimpleRightSide> rules) -> SimpleGrammars
{
    SimpleGrammars g;
    vector<SimpleRightSide> rs;
    for (auto& r : rules) rs.push_back(move(r));
    g[S(left)] = move(rs);
    return g;
}

/// helper: check if a grammar contains a specific nonterminal key
static auto HasKey(SimpleGrammars const& g, const char* key) -> bool
{
    return g.contains(S(key));
}

/// helper: get rule count for a nonterminal
static auto RuleCount(SimpleGrammars const& g, const char* key) -> size_t
{
    return g.at(S(key)).size();
}

/// helper: check if a specific rule exists for a nonterminal
static auto HasRule(SimpleGrammars const& g, const char* key, SimpleRightSide const& expected) -> bool
{
    auto& rules = g.at(S(key));
    for (auto& r : rules)
    {
        if (r.size() != expected.size()) continue;
        bool same = true;
        for (size_t i = 0; i < r.size(); ++i)
        {
            if (r[i] != expected[i]) { same = false; break; }
        }
        if (same) return true;
    }
    return false;
}

// ============================================================================
// Test cases for RemoveIndirectLeftRecur
//
// Algorithm behavior notes:
// - SimpleGrammars is a map, so iteration is alphabetical by key.
// - Each nonterminal N is added to previousNontermins BEFORE its rules are
//   processed, so self-references (A → A a) also trigger expansion.
// - Expansion uses grammars.at(), which reflects already-processed nonterminals.
// - DirectLeftRecur2RightRecur is called after expansion to eliminate any
//   remaining direct left recursion.
// ============================================================================

TEST_CASE("RemoveIndirectLeftRecur - no recursion", "[parser][indirect-left-recur]")
{
    // A -> a b | c d
    // B -> e f
    auto grammars = G("A", { Rs({"a", "b"}), Rs({"c", "d"}) });
    grammars.merge(G("B", { Rs({"e", "f"}) }));

    auto result = RemoveIndirectLeftRecur(move(grammars));
    auto& g = result.Grammars;

    INFO("grammars should be unchanged");
    REQUIRE(HasKey(g, "A"));
    REQUIRE(HasKey(g, "B"));
    REQUIRE(RuleCount(g, "A") == 2);
    REQUIRE(RuleCount(g, "B") == 1);
    REQUIRE(HasRule(g, "A", Rs({"a", "b"})));
    REQUIRE(HasRule(g, "A", Rs({"c", "d"})));
    REQUIRE(HasRule(g, "B", Rs({"e", "f"})));
    REQUIRE_FALSE(HasKey(g, "A'"));
    REQUIRE_FALSE(HasKey(g, "B'"));
    // try_emplace creates empty entries for each nonterminal, so check no actual history
    for (auto& [k, hist] : result.ConvertHistory)
    {
        REQUIRE(hist.empty());
    }
}

static auto DumpGrammars(SimpleGrammars const& g) -> string
{
    string result;
    for (auto& [k, rules] : g)
    {
        string_view ksv = k;
        result += string(ksv) + " ->";
        for (auto& r : rules)
        {
            result += " [";
            for (size_t i = 0; i < r.size(); ++i)
            {
                if (i > 0) result += " ";
                string_view sv = r[i];
                result += string(sv);
            }
            result += "]";
        }
        result += "\n";
    }
    return result;
}

TEST_CASE("RemoveIndirectLeftRecur - direct left recursion only", "[parser][indirect-left-recur]")
{
    // A -> A a | b
    // previousNontermins is empty when processing A (insert happens after loop).
    // So "A a" is NOT expanded. DirectLeftRecur2RightRecur detects direct left recursion:
    //   A → b A', A' → a A' | ε
    auto grammars = G("A", { Rs({"A", "a"}), Rs({"b"}) });

    auto result = RemoveIndirectLeftRecur(move(grammars));
    auto& g = result.Grammars;

    REQUIRE(HasKey(g, "A"));
    REQUIRE(HasKey(g, "A'"));
    REQUIRE(RuleCount(g, "A") == 1);
    REQUIRE(HasRule(g, "A", Rs({"b", "A'"})));
    REQUIRE(RuleCount(g, "A'") == 2);
    REQUIRE(HasRule(g, "A'", Rs({"a", "A'"})));
    REQUIRE(HasRule(g, "A'", Rs({})));
}

TEST_CASE("RemoveIndirectLeftRecur - simple indirect left recursion (A,B)", "[parser][indirect-left-recur]")
{
    // A -> B a | b
    // B -> A b | a
    // Map order: A first, B second.
    // A: B not in previousNontermins -> no expansion. A = {B a, b}. No left recursion.
    // B: A IS in previousNontermins. ExpandFront "A b" using grammars.at("A") = {B a, b}:
    //   B a + b = B a b
    //   b   + b = b b
    // Rule "a" kept.
    // newRss = {B a b, b b, a}
    // DirectLeftRecur2RightRecur: B a b is left-recursive
    //   B → b b B' | a B'
    //   B' → a b B' | ε
    auto grammars = G("A", { Rs({"B", "a"}), Rs({"b"}) });
    grammars.merge(G("B", { Rs({"A", "b"}), Rs({"a"}) }));

    auto result = RemoveIndirectLeftRecur(move(grammars));
    auto& g = result.Grammars;

    INFO("B expands A, discovers direct left recursion");
    REQUIRE(HasKey(g, "A"));
    REQUIRE(HasKey(g, "B"));
    REQUIRE(HasKey(g, "B'"));
    REQUIRE_FALSE(HasKey(g, "A'"));

    // A unchanged
    REQUIRE(RuleCount(g, "A") == 2);
    REQUIRE(HasRule(g, "A", Rs({"B", "a"})));
    REQUIRE(HasRule(g, "A", Rs({"b"})));

    // B after expansion + conversion
    REQUIRE(RuleCount(g, "B") == 2);
    REQUIRE(HasRule(g, "B", Rs({"b", "b", "B'"})));
    REQUIRE(HasRule(g, "B", Rs({"a", "B'"})));

    // B' auxiliary
    REQUIRE(RuleCount(g, "B'") == 2);
    REQUIRE(HasRule(g, "B'", Rs({"a", "b", "B'"})));
    REQUIRE(HasRule(g, "B'", Rs({})));

    // ConvertHistory for B[0]: Left2RightRecur (direct left recursion was converted)
    REQUIRE(result.ConvertHistory.at(S("B")).at(0).Count() == 1);

    // ConvertHistory for B[1]: Left2RightRecur (direct left recursion was converted)
    REQUIRE(result.ConvertHistory.at(S("B")).at(1).Count() == 1);
}

TEST_CASE("RemoveIndirectLeftRecur - forward chain of 3 no left recursion", "[parser][indirect-left-recur]")
{
    // A -> B a | a
    // B -> C b | b
    // C -> c
    // Map order: A, B, C. None have left recursion.
    // A: B not in set -> unchanged.
    // B: C not in set -> unchanged.
    // C: c not in set -> unchanged.
    auto grammars = G("A", { Rs({"B", "a"}), Rs({"a"}) });
    grammars.merge(G("B", { Rs({"C", "b"}), Rs({"b"}) }));
    grammars.merge(G("C", { Rs({"c"}) }));

    auto result = RemoveIndirectLeftRecur(move(grammars));
    auto& g = result.Grammars;

    INFO("no left recursion at all - grammar unchanged");
    REQUIRE(RuleCount(g, "A") == 2);
    REQUIRE(HasRule(g, "A", Rs({"B", "a"})));
    REQUIRE(HasRule(g, "A", Rs({"a"})));
    REQUIRE(RuleCount(g, "B") == 2);
    REQUIRE(HasRule(g, "B", Rs({"C", "b"})));
    REQUIRE(HasRule(g, "B", Rs({"b"})));
    REQUIRE(RuleCount(g, "C") == 1);
    REQUIRE(HasRule(g, "C", Rs({"c"})));
    REQUIRE_FALSE(HasKey(g, "A'"));
    REQUIRE_FALSE(HasKey(g, "B'"));
    REQUIRE_FALSE(HasKey(g, "C'"));
}

TEST_CASE("RemoveIndirectLeftRecur - expansion reveals direct left recursion", "[parser][indirect-left-recur]")
{
    // S -> A a | b
    // A -> S a | c
    // Map order: A, S.
    // A: S not in previousNontermins -> unchanged. A = {S a, c}.
    // S: A IS in previousNontermins. ExpandFront "A a" using grammars.at("A") = {S a, c}:
    //   S a + a = S a a
    //   c   + a = c a
    // Rule "b" kept.
    // newRss = {S a a, c a, b}
    // DirectLeftRecur2RightRecur: S a a is left-recursive
    //   S → c a S' | b S'
    //   S' → a a S' | ε
    auto grammars = G("S", { Rs({"A", "a"}), Rs({"b"}) });
    grammars.merge(G("A", { Rs({"S", "a"}), Rs({"c"}) }));

    auto result = RemoveIndirectLeftRecur(move(grammars));
    auto& g = result.Grammars;

    REQUIRE(HasKey(g, "A"));
    REQUIRE(HasKey(g, "S"));
    REQUIRE(HasKey(g, "S'"));
    REQUIRE_FALSE(HasKey(g, "A'"));

    // A unchanged
    REQUIRE(RuleCount(g, "A") == 2);
    REQUIRE(HasRule(g, "A", Rs({"S", "a"})));
    REQUIRE(HasRule(g, "A", Rs({"c"})));

    // S after expansion + conversion
    REQUIRE(RuleCount(g, "S") == 2);
    REQUIRE(HasRule(g, "S", Rs({"c", "a", "S'"})));
    REQUIRE(HasRule(g, "S", Rs({"b", "S'"})));

    // S' auxiliary
    REQUIRE(RuleCount(g, "S'") == 2);
    REQUIRE(HasRule(g, "S'", Rs({"a", "a", "S'"})));
    REQUIRE(HasRule(g, "S'", Rs({})));

    // S[0] history: Left2RightRecur (direct left recursion was converted)
    REQUIRE(result.ConvertHistory.at(S("S")).at(0).Count() == 1);

    // S[1] history: Left2RightRecur (direct left recursion was converted)
    REQUIRE(result.ConvertHistory.at(S("S")).at(1).Count() == 1);
}

TEST_CASE("RemoveIndirectLeftRecur - no expansion when dependency not yet processed", "[parser][indirect-left-recur]")
{
    // A -> B a
    // B -> b
    // Map order: A, B. When processing A, B is not in previousNontermins.
    // So A stays as {B a}. No expansion, no left recursion.
    auto grammars = G("A", { Rs({"B", "a"}) });
    grammars.merge(G("B", { Rs({"b"}) }));

    auto result = RemoveIndirectLeftRecur(move(grammars));
    auto& g = result.Grammars;

    INFO("A references B but B is processed later, so no expansion");
    REQUIRE(HasKey(g, "A"));
    REQUIRE(HasKey(g, "B"));
    REQUIRE_FALSE(HasKey(g, "A'"));
    REQUIRE_FALSE(HasKey(g, "B'"));
    REQUIRE(RuleCount(g, "A") == 1);
    REQUIRE(HasRule(g, "A", Rs({"B", "a"})));
    REQUIRE(RuleCount(g, "B") == 1);
    REQUIRE(HasRule(g, "B", Rs({"b"})));
    for (auto& [k, hist] : result.ConvertHistory)
    {
        REQUIRE(hist.empty());
    }
}

TEST_CASE("RemoveIndirectLeftRecur - grammar with auxiliary nonterminal present", "[parser][indirect-left-recur]")
{
    // A -> b A' | b
    // A' -> a A' | ε
    // B -> A b | a
    // Map order: A, A', B.
    // No left recursion cycle between B and A (A does not reference B).
    // B: A IS in previousNontermins, but startsWith[A] does not contain B -> no expansion.
    auto grammars = G("A", { Rs({"b", "A'"}), Rs({"b"}) });
    grammars["A'"] = { Rs({"a", "A'"}), Rs({}) };
    grammars.merge(G("B", { Rs({"A", "b"}), Rs({"a"}) }));

    auto result = RemoveIndirectLeftRecur(move(grammars));
    auto& g = result.Grammars;

    REQUIRE(HasKey(g, "A"));
    REQUIRE(HasKey(g, "A'"));
    REQUIRE(HasKey(g, "B"));
    REQUIRE_FALSE(HasKey(g, "B'"));

    REQUIRE(RuleCount(g, "A") == 2);
    REQUIRE(HasRule(g, "A", Rs({"b", "A'"})));
    REQUIRE(HasRule(g, "A", Rs({"b"})));

    REQUIRE(RuleCount(g, "A'") == 2);
    REQUIRE(HasRule(g, "A'", Rs({"a", "A'"})));
    REQUIRE(HasRule(g, "A'", Rs({})));

    // B unchanged (no cycle, no expansion)
    REQUIRE(RuleCount(g, "B") == 2);
    REQUIRE(HasRule(g, "B", Rs({"A", "b"})));
    REQUIRE(HasRule(g, "B", Rs({"a"})));

    // No expansion history
    for (auto& [k, hist] : result.ConvertHistory)
    {
        REQUIRE(hist.empty());
    }
}

TEST_CASE("RemoveIndirectLeftRecur - self-referential via another nonterminal", "[parser][indirect-left-recur]")
{
    // A -> B a | b
    // B -> A b | ε
    // Map order: A, B.
    // A: B not in previousNontermins -> unchanged. A = {B a, b}.
    // B: A IS in previousNontermins. ExpandFront "A b" using grammars.at("A") = {B a, b}:
    //   B a + b = B a b
    //   b   + b = b b
    // Rule ε (empty): x.empty() is true, so NOT expanded. Kept as ε.
    // newRss = {B a b, b b, ε}
    // DirectLeftRecur2RightRecur: B a b starts with B → left-recursive
    //   B → b b B' | B'   (ε + B' = B')
    //   B' → a b B' | ε
    auto grammars = G("A", { Rs({"B", "a"}), Rs({"b"}) });
    grammars.merge(G("B", { Rs({"A", "b"}), Rs({}) }));

    auto result = RemoveIndirectLeftRecur(move(grammars));
    auto& g = result.Grammars;

    REQUIRE(HasKey(g, "A"));
    REQUIRE(HasKey(g, "B"));
    REQUIRE(HasKey(g, "B'"));
    REQUIRE_FALSE(HasKey(g, "A'"));

    // A unchanged
    REQUIRE(RuleCount(g, "A") == 2);
    REQUIRE(HasRule(g, "A", Rs({"B", "a"})));
    REQUIRE(HasRule(g, "A", Rs({"b"})));

    // B after expansion + conversion
    REQUIRE(RuleCount(g, "B") == 2);
    REQUIRE(HasRule(g, "B", Rs({"b", "b", "B'"})));
    REQUIRE(HasRule(g, "B", Rs({"B'"})));

    // B' auxiliary
    REQUIRE(RuleCount(g, "B'") == 2);
    REQUIRE(HasRule(g, "B'", Rs({"a", "b", "B'"})));
    REQUIRE(HasRule(g, "B'", Rs({})));
}

TEST_CASE("RemoveIndirectLeftRecur - mixed direct and indirect recursion", "[parser][indirect-left-recur]")
{
    // A -> A a | B b | c
    // B -> B d | e
    // Map order: A, B.
    //
    // Processing A: A not in previousNontermins yet. "A a" not expanded.
    //   DirectLeftRecur2RightRecur: A a is left-recursive
    //   A → B b A' | c A', A' → a A' | ε
    //
    // Processing B: B not in previousNontermins yet. "B d" not expanded.
    //   DirectLeftRecur2RightRecur: B d is left-recursive
    //   B → e B', B' → d B' | ε
    auto grammars = G("A", { Rs({"A", "a"}), Rs({"B", "b"}), Rs({"c"}) });
    grammars.merge(G("B", { Rs({"B", "d"}), Rs({"e"}) }));

    auto result = RemoveIndirectLeftRecur(move(grammars));
    auto& g = result.Grammars;

    REQUIRE(HasKey(g, "A"));
    REQUIRE(HasKey(g, "A'"));
    REQUIRE(HasKey(g, "B"));
    REQUIRE(HasKey(g, "B'"));

    // A after direct left-rec removal
    REQUIRE(RuleCount(g, "A") == 2);
    REQUIRE(HasRule(g, "A", Rs({"B", "b", "A'"})));
    REQUIRE(HasRule(g, "A", Rs({"c", "A'"})));

    // A' auxiliary
    REQUIRE(RuleCount(g, "A'") == 2);
    REQUIRE(HasRule(g, "A'", Rs({"a", "A'"})));
    REQUIRE(HasRule(g, "A'", Rs({})));

    // B after direct left-rec removal
    REQUIRE(RuleCount(g, "B") == 1);
    REQUIRE(HasRule(g, "B", Rs({"e", "B'"})));

    // B' auxiliary
    REQUIRE(RuleCount(g, "B'") == 2);
    REQUIRE(HasRule(g, "B'", Rs({"d", "B'"})));
    REQUIRE(HasRule(g, "B'", Rs({})));
}

TEST_CASE("RemoveIndirectLeftRecur - three nonterminals indirect expansion", "[parser][indirect-left-recur]")
{
    // A -> B a | a
    // B -> C b | b
    // C -> A c | c
    // Map order: A, B, C.
    // A: no expansion. A = {B a, a}.
    // B: A in previousNontermins, but B's rules don't start with A. No expansion.
    // C: previousNontermins = ["A", "B"]. ExpandFront "A c":
    //   Pass 1 (prev=A): expand using A's rules {B a, a}:
    //     B a + c = B a c,  a + c = a c
    //   Pass 2 (prev=B): B a c starts with B, expand using B's rules {C b, b}:
    //     C b + a c = C b a c,  b + a c = b a c
    //     a c: front "a" != B, keep.
    //   Result: {C b a c, b a c, a c}
    // Rule "c" kept. newRightSides = {C b a c, b a c, a c, c}
    // DirectLeftRecur2RightRecur: C b a c starts with C → left-recursive
    //   C → b a c C' | a c C' | c C'
    //   C' → b a c C' | ε
    auto grammars = G("A", { Rs({"B", "a"}), Rs({"a"}) });
    grammars.merge(G("B", { Rs({"C", "b"}), Rs({"b"}) }));
    grammars.merge(G("C", { Rs({"A", "c"}), Rs({"c"}) }));

    auto result = RemoveIndirectLeftRecur(move(grammars));
    auto& g = result.Grammars;

    REQUIRE(HasKey(g, "A"));
    REQUIRE(HasKey(g, "B"));
    REQUIRE(HasKey(g, "C"));
    REQUIRE(HasKey(g, "C'"));
    REQUIRE_FALSE(HasKey(g, "A'"));
    REQUIRE_FALSE(HasKey(g, "B'"));

    // A unchanged
    REQUIRE(RuleCount(g, "A") == 2);
    REQUIRE(HasRule(g, "A", Rs({"B", "a"})));
    REQUIRE(HasRule(g, "A", Rs({"a"})));

    // B unchanged
    REQUIRE(RuleCount(g, "B") == 2);
    REQUIRE(HasRule(g, "B", Rs({"C", "b"})));
    REQUIRE(HasRule(g, "B", Rs({"b"})));

    // C after full expansion + left-rec removal
    REQUIRE(RuleCount(g, "C") == 3);
    REQUIRE(HasRule(g, "C", Rs({"b", "a", "c", "C'"})));
    REQUIRE(HasRule(g, "C", Rs({"a", "c", "C'"})));
    REQUIRE(HasRule(g, "C", Rs({"c", "C'"})));

    // C' auxiliary
    REQUIRE(RuleCount(g, "C'") == 2);
    REQUIRE(HasRule(g, "C'", Rs({"b", "a", "c", "C'"})));
    REQUIRE(HasRule(g, "C'", Rs({})));

    // ConvertHistory for C's expanded rules
    // After DirectLeftRecur2RightRecur, all ExpandFront entries are replaced by 1 Left2RightRecur
    REQUIRE(result.ConvertHistory.at(S("C")).at(0).Count() == 1);
    REQUIRE(result.ConvertHistory.at(S("C")).at(1).Count() == 1);
    REQUIRE(result.ConvertHistory.at(S("C")).at(2).Count() == 1);

    // C[3] = c: after conversion, C has only 3 rules (b a c C', a c C', c C')
    REQUIRE_FALSE(result.ConvertHistory.at(S("C")).contains(3));
}

TEST_CASE("RemoveIndirectLeftRecur - ConvertHistory chain across multiple levels", "[parser][indirect-left-recur]")
{
    // C -> B a
    // B -> A b
    // A -> a
    // Map order: A, B, C.
    // No left recursion cycle (linear chain). No expansion.
    auto grammars = G("C", { Rs({"B", "a"}) });
    grammars.merge(G("B", { Rs({"A", "b"}) }));
    grammars.merge(G("A", { Rs({"a"}) }));

    auto result = RemoveIndirectLeftRecur(move(grammars));
    auto& g = result.Grammars;

    REQUIRE(HasKey(g, "A"));
    REQUIRE(HasKey(g, "B"));
    REQUIRE(HasKey(g, "C"));

    // All unchanged
    REQUIRE(RuleCount(g, "A") == 1);
    REQUIRE(HasRule(g, "A", Rs({"a"})));
    REQUIRE(RuleCount(g, "B") == 1);
    REQUIRE(HasRule(g, "B", Rs({"A", "b"})));
    REQUIRE(RuleCount(g, "C") == 1);
    REQUIRE(HasRule(g, "C", Rs({"B", "a"})));

    // No expansion history
    for (auto& [k, hist] : result.ConvertHistory)
    {
        REQUIRE(hist.empty());
    }
}

TEST_CASE("RemoveIndirectLeftRecur - chain with 4 nonterminals", "[parser][indirect-left-recur]")
{
    // D -> C d | d
    // C -> B c | c
    // B -> A b | b
    // A -> a
    // Map order: A, B, C, D.
    // No left recursion cycle (linear chain). No expansion.
    auto grammars = G("D", { Rs({"C", "d"}), Rs({"d"}) });
    grammars.merge(G("C", { Rs({"B", "c"}), Rs({"c"}) }));
    grammars.merge(G("B", { Rs({"A", "b"}), Rs({"b"}) }));
    grammars.merge(G("A", { Rs({"a"}) }));

    auto result = RemoveIndirectLeftRecur(move(grammars));
    auto& g = result.Grammars;

    REQUIRE(HasKey(g, "A"));
    REQUIRE(HasKey(g, "B"));
    REQUIRE(HasKey(g, "C"));
    REQUIRE(HasKey(g, "D"));

    // All unchanged
    REQUIRE(RuleCount(g, "A") == 1);
    REQUIRE(HasRule(g, "A", Rs({"a"})));

    REQUIRE(RuleCount(g, "B") == 2);
    REQUIRE(HasRule(g, "B", Rs({"A", "b"})));
    REQUIRE(HasRule(g, "B", Rs({"b"})));

    REQUIRE(RuleCount(g, "C") == 2);
    REQUIRE(HasRule(g, "C", Rs({"B", "c"})));
    REQUIRE(HasRule(g, "C", Rs({"c"})));

    REQUIRE(RuleCount(g, "D") == 2);
    REQUIRE(HasRule(g, "D", Rs({"C", "d"})));
    REQUIRE(HasRule(g, "D", Rs({"d"})));

    // No expansion history
    for (auto& [k, hist] : result.ConvertHistory)
    {
        REQUIRE(hist.empty());
    }
}

TEST_CASE("RemoveIndirectLeftRecur - only nonterminal references, all expanded", "[parser][indirect-left-recur]")
{
    // C -> B
    // B -> A
    // A -> a
    // Map order: A, B, C.
    // No left recursion cycle (linear chain). No expansion.
    auto grammars = G("C", { Rs({"B"}) });
    grammars.merge(G("B", { Rs({"A"}) }));
    grammars.merge(G("A", { Rs({"a"}) }));

    auto result = RemoveIndirectLeftRecur(move(grammars));
    auto& g = result.Grammars;

    // All unchanged
    REQUIRE(RuleCount(g, "A") == 1);
    REQUIRE(HasRule(g, "A", Rs({"a"})));
    REQUIRE(RuleCount(g, "B") == 1);
    REQUIRE(HasRule(g, "B", Rs({"A"})));
    REQUIRE(RuleCount(g, "C") == 1);
    REQUIRE(HasRule(g, "C", Rs({"B"})));
}

TEST_CASE("RemoveIndirectLeftRecur - expansion of multiple rules of same nonterminal", "[parser][indirect-left-recur]")
{
    // B -> A a | A b | c
    // A -> d
    // Map order: A, B.
    // No left recursion cycle. No expansion.
    auto grammars = G("B", { Rs({"A", "a"}), Rs({"A", "b"}), Rs({"c"}) });
    grammars.merge(G("A", { Rs({"d"}) }));

    auto result = RemoveIndirectLeftRecur(move(grammars));
    auto& g = result.Grammars;

    REQUIRE(RuleCount(g, "A") == 1);
    REQUIRE(HasRule(g, "A", Rs({"d"})));

    // B unchanged
    REQUIRE(RuleCount(g, "B") == 3);
    REQUIRE(HasRule(g, "B", Rs({"A", "a"})));
    REQUIRE(HasRule(g, "B", Rs({"A", "b"})));
    REQUIRE(HasRule(g, "B", Rs({"c"})));

    // No expansion history
    for (auto& [k, hist] : result.ConvertHistory)
    {
        REQUIRE(hist.empty());
    }
}

TEST_CASE("RemoveIndirectLeftRecur - epsilon rule preserved through expansion", "[parser][indirect-left-recur]")
{
    // B -> A a | ε
    // A -> b
    // Map order: A, B.
    // No left recursion cycle. No expansion.
    auto grammars = G("B", { Rs({"A", "a"}), Rs({}) });
    grammars.merge(G("A", { Rs({"b"}) }));

    auto result = RemoveIndirectLeftRecur(move(grammars));
    auto& g = result.Grammars;

    // B unchanged
    REQUIRE(RuleCount(g, "B") == 2);
    REQUIRE(HasRule(g, "B", Rs({"A", "a"})));
    REQUIRE(HasRule(g, "B", Rs({})));

    // No expansion history
    for (auto& [k, hist] : result.ConvertHistory)
    {
        REQUIRE(hist.empty());
    }
}

TEST_CASE("RemoveIndirectLeftRecur - indirect recursion B via A, with A direct left-rec", "[parser][indirect-left-recur]")
{
    // A -> A a | B b | c
    // B -> A d | e
    // Map order: A, B.
    //
    // Processing A: A not in previousNontermins. "A a" not expanded.
    //   DirectLeftRecur2RightRecur: A a is left-recursive
    //   A → B b A' | c A', A' → a A' | ε
    //
    // Processing B: A IS in previousNontermins. ExpandFront "A d":
    //   grammars.at("A") = {B b A', c A'}  (A already converted)
    //   B b A' + d = B b A' d
    //   c A' + d = c A' d
    // Rule "e" kept.
    // newRss = {B b A' d, c A' d, e}
    // DirectLeftRecur2RightRecur: B b A' d starts with B → left-recursive
    //   B → c A' d B' | e B'
    //   B' → b A' d B' | ε
    auto grammars = G("A", { Rs({"A", "a"}), Rs({"B", "b"}), Rs({"c"}) });
    grammars.merge(G("B", { Rs({"A", "d"}), Rs({"e"}) }));

    auto result = RemoveIndirectLeftRecur(move(grammars));
    auto& g = result.Grammars;

    REQUIRE(HasKey(g, "A"));
    REQUIRE(HasKey(g, "A'"));
    REQUIRE(HasKey(g, "B"));
    REQUIRE(HasKey(g, "B'"));

    // A after direct left-rec removal
    REQUIRE(RuleCount(g, "A") == 2);
    REQUIRE(HasRule(g, "A", Rs({"B", "b", "A'"})));
    REQUIRE(HasRule(g, "A", Rs({"c", "A'"})));

    // A' auxiliary
    REQUIRE(RuleCount(g, "A'") == 2);
    REQUIRE(HasRule(g, "A'", Rs({"a", "A'"})));
    REQUIRE(HasRule(g, "A'", Rs({})));

    // B after expanding A's converted rules + left-rec removal
    REQUIRE(RuleCount(g, "B") == 2);
    REQUIRE(HasRule(g, "B", Rs({"c", "A'", "d", "B'"})));
    REQUIRE(HasRule(g, "B", Rs({"e", "B'"})));

    // B' auxiliary
    REQUIRE(RuleCount(g, "B'") == 2);
    REQUIRE(HasRule(g, "B'", Rs({"b", "A'", "d", "B'"})));
    REQUIRE(HasRule(g, "B'", Rs({})));
}

// ============================================================================
// Syntax tree undo tests: verify left-recursive tree recovery
//
// Grammar: L -> a | L "," a
// After RemoveIndirectLeftRecur:
//   L -> a L'
//   L' -> "," a L' | ε
//
// Parsing "a , a , a" with converted grammar produces right-recursive tree:
//   L
//   ├── a
//   └── L'
//       ├── ","
//       ├── a
//       └── L'
//           ├── ","
//           ├── a
//           └── L' (ε)
//
// After undo, should recover left-recursive tree:
//   L
//   ├── L
//   │   ├── L
//   │   │   └── a
//   │   └── ","
//   │   └── a
//   └── ","
//   └── a
// ============================================================================

TEST_CASE("Left2RightRecur::Undo - initializer-list style left-recursive recovery", "[parser][undo][indirect-left-recur]")
{
    // L -> a | L "," a
    SimpleGrammars grammars;
    grammars[S("L")] = { Rs({"a"}), Rs({"L", ",", "a"}) };

    auto [converted, history] = RemoveIndirectLeftRecur(move(grammars));

    // After conversion: L -> a L', L' -> "," a L' | ε
    REQUIRE(HasKey(converted, "L"));
    REQUIRE(HasKey(converted, "L'"));
    REQUIRE(RuleCount(converted, "L") == 1);
    REQUIRE(HasRule(converted, "L", Rs({"a", "L'"})));
    REQUIRE(RuleCount(converted, "L'") == 2);
    REQUIRE(HasRule(converted, "L'", Rs({",", "a", "L'"})));
    REQUIRE(HasRule(converted, "L'", Rs({})));

    // ConvertHistory for L[0] should have Left2RightRecur
    REQUIRE(history.at(S("L")).contains(0));
    REQUIRE(history.at(S("L")).at(0).Count() == 1);

    // Build parse tree for "a , a , a" as the parser would produce
    // using the converted grammar L -> a L', L' -> "," a L' | ε
    auto eps = MakeNode(S("L'"), {}, {});
    auto innerL = MakeNode(S("L'"), {S(","), S("a"), S("L'")},
        MakeChildren(
            Leaf(MakeToken(TokType::Comma, ",")),
            Leaf(MakeToken(TokType::Ident, "a")),
            Branch(move(eps))));
    auto outerL = MakeNode(S("L'"), {S(","), S("a"), S("L'")},
        MakeChildren(
            Leaf(MakeToken(TokType::Comma, ",")),
            Leaf(MakeToken(TokType::Ident, "a")),
            Branch(move(innerL))));
    auto root = MakeNode(S("L"), {S("a"), S("L'")},
        MakeChildren(
            Leaf(MakeToken(TokType::Ident, "a")),
            Branch(move(outerL))));

    // Apply undo
    auto bHist = history.at(S("L")).at(0);
    auto result = bHist.Undo(move(root));

    // Should recover left-recursive tree:
    // L[L[L[a], ",", a], ",", a]
    INFO(DumpGrammars(converted));

    REQUIRE(result.Name == "L");
    REQUIRE(result.ChildSymbols.size() == 3);
    REQUIRE(result.ChildSymbols[0] == "L");
    REQUIRE(result.ChildSymbols[1] == ",");
    REQUIRE(result.ChildSymbols[2] == "a");

    // Middle level: L[L[a], ",", a]
    auto* mid = AsNode(result.Children[0]);
    REQUIRE(mid != nullptr);
    REQUIRE(mid->Name == "L");
    REQUIRE(mid->ChildSymbols.size() == 3);
    REQUIRE(mid->ChildSymbols[0] == "L");
    REQUIRE(mid->ChildSymbols[1] == ",");
    REQUIRE(mid->ChildSymbols[2] == "a");

    // Innermost level: L[a]
    auto* inner = AsNode(mid->Children[0]);
    REQUIRE(inner != nullptr);
    REQUIRE(inner->Name == "L");
    REQUIRE(inner->ChildSymbols.size() == 1);
    REQUIRE(inner->ChildSymbols[0] == "a");
    REQUIRE(AsToken(inner->Children[0])->Value == "a");

    // Verify comma and ident tokens at each level
    REQUIRE(AsToken(mid->Children[1])->Value == ",");
    REQUIRE(AsToken(mid->Children[2])->Value == "a");
    REQUIRE(AsToken(result.Children[1])->Value == ",");
    REQUIRE(AsToken(result.Children[2])->Value == "a");
}

TEST_CASE("Left2RightRecur::Undo - single element no recursion", "[parser][undo][indirect-left-recur]")
{
    // L -> a | L "," a
    SimpleGrammars grammars;
    grammars[S("L")] = { Rs({"a"}), Rs({"L", ",", "a"}) };

    auto [converted, history] = RemoveIndirectLeftRecur(move(grammars));

    // Build parse tree for "a" (single element, L' -> ε)
    auto eps = MakeNode(S("L'"), {}, {});
    auto root = MakeNode(S("L"), {S("a"), S("L'")},
        MakeChildren(
            Leaf(MakeToken(TokType::Ident, "a")),
            Branch(move(eps))));

    auto bHist = history.at(S("L")).at(0);
    auto result = bHist.Undo(move(root));

    // Single element: L[a]
    REQUIRE(result.Name == "L");
    REQUIRE(result.ChildSymbols.size() == 1);
    REQUIRE(result.ChildSymbols[0] == "a");
    REQUIRE(AsToken(result.Children[0])->Value == "a");
}

// ============================================================================
// End-to-end parser integration tests
//
// These tests construct a full LLParser, feed token streams through it,
// and verify that the parsed syntax tree (after undo) has the correct
// left-recursive structure.
// ============================================================================

static auto MakeEof() -> MockToken
{
    return { TokType::Eof, "eof" };
}

/// <summary>
/// Build an LLParser for the grammar L -> a | L "," a
/// </summary>
static auto BuildListParser() -> LLParser
{
    SimpleGrammars grammars;
    grammars[S("L")] = { Rs({"a"}), Rs({"L", ",", "a"}) };

    map<string_view, int> terminal2IntTokenType;
    terminal2IntTokenType["a"] = static_cast<int>(TokType::Ident);
    terminal2IntTokenType[","] = static_cast<int>(TokType::Comma);
    terminal2IntTokenType[eof] = static_cast<int>(TokType::Eof);

    return LLParser::ConstructFrom(S("L"), move(grammars), terminal2IntTokenType);
}

TEST_CASE("LLParser integration - three elements left-recursive tree", "[parser][integration]")
{
    auto parser = BuildListParser();

    // Token stream: a , a , a , eof
    vector<MockToken> tokens;
    tokens.push_back(MakeToken(TokType::Ident, "a"));
    tokens.push_back(MakeToken(TokType::Comma, ","));
    tokens.push_back(MakeToken(TokType::Ident, "a"));
    tokens.push_back(MakeToken(TokType::Comma, ","));
    tokens.push_back(MakeToken(TokType::Ident, "a"));
    tokens.push_back(MakeEof());

    auto result = parser.Parse<void>(VectorStream{ .Tokens = move(tokens) },
        [](Node*) {});

    REQUIRE(result.has_value());

    // Root wraps the start symbol; actual result is root's child
    auto& root = result.value();
    REQUIRE(root.Children.size() == 1);
    auto* L = AsNode(root.Children[0]);
    REQUIRE(L != nullptr);
    REQUIRE(L->Name == "L");

    // After undo: L[L[L[a], ",", a], ",", a]
    REQUIRE(L->ChildSymbols.size() == 3);
    REQUIRE(L->ChildSymbols[0] == "L");
    REQUIRE(L->ChildSymbols[1] == ",");
    REQUIRE(L->ChildSymbols[2] == "a");

    // Middle level
    auto* mid = AsNode(L->Children[0]);
    REQUIRE(mid != nullptr);
    REQUIRE(mid->Name == "L");
    REQUIRE(mid->ChildSymbols.size() == 3);
    REQUIRE(mid->ChildSymbols[0] == "L");
    REQUIRE(mid->ChildSymbols[1] == ",");
    REQUIRE(mid->ChildSymbols[2] == "a");

    // Innermost level: L[a]
    auto* inner = AsNode(mid->Children[0]);
    REQUIRE(inner != nullptr);
    REQUIRE(inner->Name == "L");
    REQUIRE(inner->ChildSymbols.size() == 1);
    REQUIRE(inner->ChildSymbols[0] == "a");
    REQUIRE(AsToken(inner->Children[0])->Value == "a");

    // Tokens at each level
    REQUIRE(AsToken(mid->Children[1])->Value == ",");
    REQUIRE(AsToken(mid->Children[2])->Value == "a");
    REQUIRE(AsToken(L->Children[1])->Value == ",");
    REQUIRE(AsToken(L->Children[2])->Value == "a");
}

TEST_CASE("LLParser integration - single element", "[parser][integration]")
{
    auto parser = BuildListParser();

    // Token stream: a , eof
    vector<MockToken> tokens;
    tokens.push_back(MakeToken(TokType::Ident, "a"));
    tokens.push_back(MakeEof());

    auto result = parser.Parse<void>(VectorStream{ .Tokens = move(tokens) },
        [](Node*) {});

    REQUIRE(result.has_value());

    auto& root = result.value();
    REQUIRE(root.Children.size() == 1);
    auto* L = AsNode(root.Children[0]);
    REQUIRE(L != nullptr);
    REQUIRE(L->Name == "L");

    // After undo: L[a]
    REQUIRE(L->ChildSymbols.size() == 1);
    REQUIRE(L->ChildSymbols[0] == "a");
    REQUIRE(AsToken(L->Children[0])->Value == "a");
}

TEST_CASE("LLParser integration - two elements", "[parser][integration]")
{
    auto parser = BuildListParser();

    // Token stream: a , a , eof
    vector<MockToken> tokens;
    tokens.push_back(MakeToken(TokType::Ident, "a"));
    tokens.push_back(MakeToken(TokType::Comma, ","));
    tokens.push_back(MakeToken(TokType::Ident, "a"));
    tokens.push_back(MakeEof());

    auto result = parser.Parse<void>(VectorStream{ .Tokens = move(tokens) },
        [](Node*) {});

    REQUIRE(result.has_value());

    auto& root = result.value();
    REQUIRE(root.Children.size() == 1);
    auto* L = AsNode(root.Children[0]);
    REQUIRE(L != nullptr);
    REQUIRE(L->Name == "L");

    // After undo: L[L[a], ",", a]
    REQUIRE(L->ChildSymbols.size() == 3);
    REQUIRE(L->ChildSymbols[0] == "L");
    REQUIRE(L->ChildSymbols[1] == ",");
    REQUIRE(L->ChildSymbols[2] == "a");

    auto* inner = AsNode(L->Children[0]);
    REQUIRE(inner != nullptr);
    REQUIRE(inner->Name == "L");
    REQUIRE(inner->ChildSymbols.size() == 1);
    REQUIRE(inner->ChildSymbols[0] == "a");
    REQUIRE(AsToken(inner->Children[0])->Value == "a");

    REQUIRE(AsToken(L->Children[1])->Value == ",");
    REQUIRE(AsToken(L->Children[2])->Value == "a");
}
