module;
#include <catch2/catch_all.hpp>
#undef EOF // EOF is TokType item, so need undef it
export module ParserTest;
import std;
import Base;
import Lexer;
import Parser;
import C11Spec;
import Context;
import ConflictResolver;
import ExpressionParser;

using std::vector;
using std::string;
using std::string_view;
using std::move;
using std::expected;
using std::unexpected;
using std::map;
using std::pair;
using std::ranges::views::filter;
using std::ranges::to;
using namespace std::literals;

using NodeType = SyntaxTreeNode<Token<TokType>, Context>;
using ParseResult = ParserResult<NodeType>;

static auto ParseRule(string startSymbol, string code) -> ParseResult
{
    auto l = Lexer<TokType>::New(lexRules);
    auto toks = l.Lex(code)
        | filter([](auto& x) { return x.Type != TokType::Whitespace && x.Type != TokType::Newline && x.Type != TokType::Comment; })
        | to<vector<Token<TokType>>>();
    toks.push_back({ .Type = TokType::EOF, .Value = "" });

    auto resolver = C11ConflictResolver();
    ExpressionParser expParser;

    auto p = LLParser::ConstructFrom(String(startSymbol), grammars, terminal2IntTokenType, resolver, expParser);
    return p.Parse<Context>(VectorStream{ .Tokens = move(toks) }, [](auto) {}, {}, resolver, expParser);
}

static auto ExpectParses(string startSymbol, string code) -> void
{
    auto result = ParseRule(startSymbol, code);
    INFO("Parsing [" << startSymbol << "]: " << code);
    if (!result.has_value())
    {
        UNSCOPED_INFO("Parse failed: " << result.error().Message);
    }
    REQUIRE(result.has_value());
}

// ===== declaration =====
TEST_CASE("C11 - declaration", "[parser]")
{
    ExpectParses("declaration", "int x;");
    ExpectParses("declaration", "int x, y;");
    ExpectParses("declaration", "int x = 1;");
    ExpectParses("declaration", "struct S { int x; };");
    ExpectParses("declaration", "int main(void);");
    ExpectParses("declaration", "_Static_assert(1, \"msg\");");
    ExpectParses("declaration", "static int x;");
    ExpectParses("declaration", "const int x;");
}

// ===== declaration-specifiers (tested via declaration) =====
TEST_CASE("C11 - declaration-specifiers", "[parser]")
{
    ExpectParses("declaration", "int x;");
    ExpectParses("declaration", "static int x;");
    ExpectParses("declaration", "const int x;");
    ExpectParses("declaration", "static const int x;");
    ExpectParses("declaration", "inline void f(void);");
}

// ===== init-declarator-list =====
TEST_CASE("C11 - init-declarator-list", "[parser]")
{
    ExpectParses("declaration", "int x;");
    ExpectParses("declaration", "int x, y;");
    ExpectParses("declaration", "int x, y, z;");
    ExpectParses("declaration", "int x = 1, y = 2;");
}

// ===== init-declarator =====
TEST_CASE("C11 - init-declarator", "[parser]")
{
    ExpectParses("declaration", "int x;");
    ExpectParses("declaration", "int x = 1;");
    ExpectParses("declaration", "int *p = 0;");
}

// ===== storage-class-specifier =====
TEST_CASE("C11 - storage-class-specifier", "[parser]")
{
    ExpectParses("declaration", "typedef int I;");
    ExpectParses("declaration", "extern int x;");
    ExpectParses("declaration", "static int x;");
    ExpectParses("declaration", "_Thread_local int x;");
    ExpectParses("declaration", "auto int x;");
    ExpectParses("declaration", "register int x;");
}

// ===== type-specifier =====
TEST_CASE("C11 - type-specifier", "[parser]")
{
    ExpectParses("declaration", "void f(void);");
    ExpectParses("declaration", "char x;");
    ExpectParses("declaration", "short x;");
    ExpectParses("declaration", "int x;");
    ExpectParses("declaration", "long x;");
    ExpectParses("declaration", "float x;");
    ExpectParses("declaration", "double x;");
    ExpectParses("declaration", "signed x;");
    ExpectParses("declaration", "unsigned x;");
    ExpectParses("declaration", "_Bool x;");
    ExpectParses("declaration", "_Complex x;");
    ExpectParses("declaration", "struct S { int a; } x;");
    ExpectParses("declaration", "enum E { A } x;");
    ExpectParses("declaration", "I x;");
}

// ===== struct-or-union-specifier =====
TEST_CASE("C11 - struct-or-union-specifier", "[parser]")
{
    ExpectParses("declaration", "struct S { int x; };");
    ExpectParses("declaration", "struct { int x; } s;");
    ExpectParses("declaration", "struct S;");
    ExpectParses("declaration", "union U { int x; };");
    ExpectParses("declaration", "union { int x; } u;");
    ExpectParses("declaration", "union U;");
}

// ===== struct-or-union =====
TEST_CASE("C11 - struct-or-union", "[parser]")
{
    ExpectParses("declaration", "struct S { int x; };");
    ExpectParses("declaration", "union U { int x; };");
}

// ===== struct-declaration-list =====
TEST_CASE("C11 - struct-declaration-list", "[parser]")
{
    ExpectParses("declaration", "struct S { int x; };");
    ExpectParses("declaration", "struct S { int x; int y; };");
    ExpectParses("declaration", "struct S { int x; char y; float z; };");
}

// ===== struct-declaration =====
TEST_CASE("C11 - struct-declaration", "[parser]")
{
    ExpectParses("declaration", "struct S { int x; };");
    ExpectParses("declaration", "struct S { const int x; };");
    ExpectParses("declaration", "struct S { int x, y; };");
}

// ===== specifier-qualifier-list (tested via struct members) =====
TEST_CASE("C11 - specifier-qualifier-list", "[parser]")
{
    ExpectParses("declaration", "struct S { int x; };");
    ExpectParses("declaration", "struct S { const int x; };");
    ExpectParses("declaration", "struct S { const volatile int x; };");
}

// ===== struct-declarator-list =====
TEST_CASE("C11 - struct-declarator-list", "[parser]")
{
    ExpectParses("declaration", "struct S { int x; };");
    ExpectParses("declaration", "struct S { int x, y; };");
}

// ===== struct-declarator =====
TEST_CASE("C11 - struct-declarator", "[parser]")
{
    ExpectParses("declaration", "struct S { int x; };");
    ExpectParses("declaration", "struct S { int x : 3; };");
    ExpectParses("declaration", "struct S { int : 3; };");
}

// ===== enum-specifier =====
TEST_CASE("C11 - enum-specifier", "[parser]")
{
    ExpectParses("declaration", "enum E { A, B, C };");
    ExpectParses("declaration", "enum E;");
    ExpectParses("declaration", "enum E { A, B, };");
}

// ===== enumerator-list =====
TEST_CASE("C11 - enumerator-list", "[parser]")
{
    ExpectParses("declaration", "enum E { A };");
    ExpectParses("declaration", "enum E { A, B, C };");
}

// ===== enumerator =====
TEST_CASE("C11 - enumerator", "[parser]")
{
    ExpectParses("declaration", "enum E { A };");
    ExpectParses("declaration", "enum E { A = 10 };");
    ExpectParses("declaration", "enum E { A = 1 + 2 };");
}

// ===== enumeration-constant (tested via enum) =====
TEST_CASE("C11 - enumeration-constant", "[parser]")
{
    ExpectParses("declaration", "enum E { A };");
}

// ===== atomic-type-specifier =====
TEST_CASE("C11 - atomic-type-specifier", "[parser]")
{
    ExpectParses("declaration", "_Atomic(int) x;");
    ExpectParses("declaration", "_Atomic(struct S) x;");
}

// ===== type-qualifier =====
TEST_CASE("C11 - type-qualifier", "[parser]")
{
    ExpectParses("declaration", "const int x;");
    ExpectParses("declaration", "int *restrict p;");
    ExpectParses("declaration", "volatile int x;");
    ExpectParses("declaration", "_Atomic int x;");
}

// ===== function-specifier =====
TEST_CASE("C11 - function-specifier", "[parser]")
{
    ExpectParses("declaration", "inline void f(void);");
    ExpectParses("declaration", "_Noreturn void f(void);");
}

// ===== alignment-specifier =====
TEST_CASE("C11 - alignment-specifier", "[parser]")
{
    ExpectParses("declaration", "_Alignas(4) int x;");
    ExpectParses("declaration", "_Alignas(int) int x;");
}

// ===== declarator =====
TEST_CASE("C11 - declarator", "[parser]")
{
    ExpectParses("declaration", "int x;");
    ExpectParses("declaration", "int *x;");
    ExpectParses("declaration", "int **x;");
    ExpectParses("declaration", "int x[10];");
    ExpectParses("declaration", "int f(void);");
    ExpectParses("declaration", "int (x);");
}

// ===== direct-declarator =====
TEST_CASE("C11 - direct-declarator", "[parser]")
{
    ExpectParses("declaration", "int x;");
    ExpectParses("declaration", "int (x);");
    ExpectParses("declaration", "int x[10];");
    ExpectParses("declaration", "int x[const 10];");
    ExpectParses("declaration", "int f(int a);");
    ExpectParses("declaration", "int f();");
    ExpectParses("declaration", "int x[10][20];");
}

// ===== pointer =====
TEST_CASE("C11 - pointer", "[parser]")
{
    ExpectParses("declaration", "int *x;");
    ExpectParses("declaration", "int **x;");
    ExpectParses("declaration", "int *const x;");
    ExpectParses("declaration", "const int *x;");
}

// ===== type-qualifier-list =====
TEST_CASE("C11 - type-qualifier-list", "[parser]")
{
    ExpectParses("declaration", "const int *x;");
    ExpectParses("declaration", "const volatile int *x;");
}

// ===== parameter-type-list =====
TEST_CASE("C11 - parameter-type-list", "[parser]")
{
    ExpectParses("declaration", "void f(int a);");
    ExpectParses("declaration", "void f(int a, int b);");
    ExpectParses("declaration", "void f(int a, ...);");
    ExpectParses("declaration", "void f(void);");
}

// ===== parameter-list =====
TEST_CASE("C11 - parameter-list", "[parser]")
{
    ExpectParses("declaration", "void f(int a);");
    ExpectParses("declaration", "void f(int a, int b, int c);");
    ExpectParses("declaration", "void f(int *a);");
}

// ===== parameter-declaration =====
TEST_CASE("C11 - parameter-declaration", "[parser]")
{
    ExpectParses("declaration", "void f(int a);");
    ExpectParses("declaration", "void f(int);");
    ExpectParses("declaration", "void f(int *);");
    ExpectParses("declaration", "void f(int []);");
}

// ===== identifier-list =====
TEST_CASE("C11 - identifier-list", "[parser]")
{
    // K&R-style identifier lists tested indirectly
    ExpectParses("declaration", "int f();");
}

// ===== type-name (tested via alignment-specifier) =====
TEST_CASE("C11 - type-name", "[parser]")
{
    ExpectParses("declaration", "_Alignas(int) int x;");
    ExpectParses("declaration", "_Alignas(int *) int x;");
}

// ===== abstract-declarator (tested via parameter-declaration) =====
TEST_CASE("C11 - abstract-declarator", "[parser]")
{
    ExpectParses("declaration", "void f(int *);");
    ExpectParses("declaration", "void f(int []);");
    ExpectParses("declaration", "void f(int (*)(int));");
}

// ===== direct-abstract-declarator =====
TEST_CASE("C11 - direct-abstract-declarator", "[parser]")
{
    ExpectParses("declaration", "void f(int []);");
    ExpectParses("declaration", "void f(int [10]);");
    ExpectParses("declaration", "void f(int (int));");
    ExpectParses("declaration", "void f(int (*)(int));");
}

// ===== typedef-name =====
TEST_CASE("C11 - typedef-name", "[parser]")
{
    ExpectParses("declaration", "I x;");
    ExpectParses("declaration", "S_t x;");
}

// ===== initializer =====
TEST_CASE("C11 - initializer", "[parser]")
{
    ExpectParses("declaration", "int x = 1;");
    ExpectParses("declaration", "int x[] = {1, 2, 3};");
    ExpectParses("declaration", "int x[][2] = {{1, 2}, {3, 4}};");
}

// ===== initializer-list =====
TEST_CASE("C11 - initializer-list", "[parser]")
{
    ExpectParses("declaration", "int x[] = {1};");
    ExpectParses("declaration", "int x[] = {1, 2, 3};");
    ExpectParses("declaration", "int x[] = {1, 2, 3, };");
    ExpectParses("declaration", "int x[] = {[0] = 1, [1] = 2};");
}

// ===== designation =====
TEST_CASE("C11 - designation", "[parser]")
{
    ExpectParses("declaration", "int x[] = {[0] = 1};");
    ExpectParses("declaration", "struct S { int a; } s = {.a = 1};");
}

// ===== designator-list =====
TEST_CASE("C11 - designator-list", "[parser]")
{
    ExpectParses("declaration", "int x[] = {[0] = 1};");
    ExpectParses("declaration", "struct S { int a; } s = {.a = 1};");
}

// ===== designator =====
TEST_CASE("C11 - designator", "[parser]")
{
    ExpectParses("declaration", "int x[] = {[0] = 1};");
    ExpectParses("declaration", "struct S { int a; } s = {.a = 1};");
}

// ===== static_assert-declaration =====
TEST_CASE("C11 - static_assert-declaration", "[parser]")
{
    ExpectParses("declaration", "_Static_assert(1, \"ok\");");
    ExpectParses("declaration", "_Static_assert(sizeof(int) == 4, \"size\");");
}

// ===== expression =====
TEST_CASE("C11 - expression", "[parser]")
{
    ExpectParses("expression", "x");
    ExpectParses("expression", "x, y");
    ExpectParses("expression", "x = 1");
    ExpectParses("expression", "a + b * c");
}

// ===== constant-expression =====
TEST_CASE("C11 - constant-expression", "[parser]")
{
    ExpectParses("constant-expression", "42");
    ExpectParses("constant-expression", "1 + 2");
    ExpectParses("constant-expression", "1 ? 2 : 3");
}

// ===== expression (conditional-expression) =====
TEST_CASE("C11 - expression (conditional-expression)", "[parser]")
{
    ExpectParses("expression", "a ? b : c");
    ExpectParses("expression", "a ? b ? c : d : e");
}

// ===================================================================
// ConflictResolver conflict resolution scenarios
// Each section tests a specific LL(1) conflict that the resolver handles.
// ===================================================================

// conflict: declaration-specifiers_com_1 + _Atomic
// _Atomic followed by "(" → atomic-type-specifier (type-specifier), else type-qualifier
TEST_CASE("C11 ConflictResolver - _Atomic as type-specifier vs type-qualifier", "[parser][conflict]")
{
    SECTION("atomic-type-specifier: _Atomic(type)") { ExpectParses("declaration", "_Atomic(int) x;"); }
    SECTION("type-qualifier: _Atomic type") { ExpectParses("declaration", "_Atomic int x;"); }
    SECTION("atomic in specifier-qualifier-list") { ExpectParses("declaration", "struct S { _Atomic(int) x; };"); }
    SECTION("atomic qualifier in struct") { ExpectParses("declaration", "struct S { _Atomic int x; };"); }
    SECTION("atomic in parameter") { ExpectParses("declaration", "void f(_Atomic(int) a);"); }
    SECTION("atomic qualifier in parameter") { ExpectParses("declaration", "void f(_Atomic int a);"); }
}

// conflict: declaration-specifiers_op_2 + Identifier
// Identifier is a typedef-name → continue declaration-specifiers; otherwise → start init-declarator
TEST_CASE("C11 ConflictResolver - typedef-name vs init-declarator", "[parser][conflict]")
{
    SECTION("typedef name continues declaration-specifiers") { ExpectParses("declaration", "I x;"); }
    SECTION("typedef struct alias") { ExpectParses("declaration", "S_t x;"); }
    SECTION("regular identifier starts init-declarator") { ExpectParses("declaration", "int x;"); }
}

// conflict: specifier-qualifier-list_op_5/op_6 + Identifier
// Same typedef-name ambiguity inside struct (specifier-qualifier-list context)
TEST_CASE("C11 ConflictResolver - typedef-name in specifier-qualifier-list", "[parser][conflict]")
{
    SECTION("typedef in struct member") { ExpectParses("declaration", "struct S { I x; };"); }
    SECTION("non-typedef in struct member") { ExpectParses("declaration", "struct S { int x; };"); }
}

// conflict: enum-specifier-enum-suffix + Identifier
// Identifier followed by "{" → enum definition; otherwise → enum forward declaration
TEST_CASE("C11 ConflictResolver - enum definition vs forward declaration", "[parser][conflict]")
{
    SECTION("enum definition with body") { ExpectParses("declaration", "enum E { A, B };"); }
    SECTION("enum forward declaration") { ExpectParses("declaration", "enum E;"); }
    SECTION("enum variable declaration") { ExpectParses("declaration", "enum E x;"); }
}

// conflict: enumerator-list_op_12 + ","  and  enumerator-list' + ","
// "," followed by Identifier → continue list; "," followed by "}" → trailing comma
TEST_CASE("C11 ConflictResolver - enumerator list trailing comma", "[parser][conflict]")
{
    SECTION("continue after comma") { ExpectParses("declaration", "enum E { A, B, C };"); }
    SECTION("trailing comma") { ExpectParses("declaration", "enum E { A, B, };"); }
}

// conflict: initializer-list_op_40 + ","  and  initializer-list' + ","
// "," followed by initializer-start → continue; "," followed by "}" → trailing comma
TEST_CASE("C11 ConflictResolver - initializer list trailing comma", "[parser][conflict]")
{
    SECTION("continue after comma") { ExpectParses("declaration", "int x[] = {1, 2, 3};"); }
    SECTION("trailing comma") { ExpectParses("declaration", "int x[] = {1, 2, 3, };"); }
    SECTION("with designation") { ExpectParses("declaration", "int x[] = {[0] = 1, [1] = 2};"); }
    SECTION("trailing comma with designation") { ExpectParses("declaration", "int x[] = {[0] = 1, [1] = 2, };"); }
}

// conflict: parameter-list_op_24 + ","  and  parameter-list' + ","
// "," followed by param-start → continue; else → trailing comma (not valid in C, but resolver handles it)
TEST_CASE("C11 ConflictResolver - parameter list comma", "[parser][conflict]")
{
    SECTION("multiple params") { ExpectParses("declaration", "void f(int a, int b, int c);"); }
    SECTION("variadic") { ExpectParses("declaration", "void f(int a, ...);"); }
}

// conflict: parameter-declaration-declaration-specifiers-suffix + (/ *
// * followed by non-typedef Identifier → declarator (int *p); otherwise → abstract-declarator (int *)
// ( followed by )/type-keyword/typedef → abstract-declarator; otherwise → declarator
TEST_CASE("C11 ConflictResolver - declarator vs abstract-declarator in parameter", "[parser][conflict]")
{
    SECTION("pointer declarator (named)") { ExpectParses("declaration", "void f(int *p);"); }
    SECTION("pointer abstract-declarator (unnamed)") { ExpectParses("declaration", "void f(int *);"); }
    SECTION("function pointer declarator") { ExpectParses("declaration", "void f(int (*fp)(int));"); }
    SECTION("function pointer abstract") { ExpectParses("declaration", "void f(int (*)(int));"); }
    SECTION("abstract array param") { ExpectParses("declaration", "void f(int []);"); }
    SECTION("named array param") { ExpectParses("declaration", "void f(int a[]);"); }
    SECTION("abstract function param") { ExpectParses("declaration", "void f(int (int));"); }
}

// conflict: struct-declarator + Identifier/(/ *
// Identifier followed by ":" → bit-field; otherwise → declarator
// * and ( → always declarator (bit-fields with pointer/grouped name are extremely rare)
TEST_CASE("C11 ConflictResolver - struct declarator vs bit-field", "[parser][conflict]")
{
    SECTION("named member") { ExpectParses("declaration", "struct S { int x; };"); }
    SECTION("bit-field with name") { ExpectParses("declaration", "struct S { int x : 3; };"); }
    SECTION("anonymous bit-field") { ExpectParses("declaration", "struct S { int : 3; };"); }
    SECTION("pointer member") { ExpectParses("declaration", "struct S { int *p; };"); }
    SECTION("multiple declarators") { ExpectParses("declaration", "struct S { int x, y; };"); }
}

// conflict: direct-declarator-[-suffix + const/restrict/volatile/_Atomic/*
// "static" → static array; "]" → simple array; "*" followed by "]" → VLA; else → qualified array
TEST_CASE("C11 ConflictResolver - direct-declarator bracket suffix", "[parser][conflict]")
{
    SECTION("simple array") { ExpectParses("declaration", "int x[10];"); }
    SECTION("static array") { ExpectParses("declaration", "int x[static 10];"); }
    SECTION("qualified array") { ExpectParses("declaration", "int x[const 10];"); }
    SECTION("qualified static array") { ExpectParses("declaration", "int x[const static 10];"); }
    SECTION("VLA star") { ExpectParses("declaration", "int x[*];"); }
    SECTION("multi-dim with qualifier") { ExpectParses("declaration", "int x[const 10][20];"); }
}

// conflict: direct-declarator-(-suffix + Identifier
// Identifier is typedef-name → parameter-type-list; otherwise → identifier-list
TEST_CASE("C11 ConflictResolver - direct-declarator paren suffix", "[parser][conflict]")
{
    SECTION("function with typed params") { ExpectParses("declaration", "int f(int a, int b);"); }
    SECTION("function with void") { ExpectParses("declaration", "int f(void);"); }
    SECTION("function no params") { ExpectParses("declaration", "int f();"); }
}

// conflict: direct-abstract-declarator-[-suffix + const/restrict/volatile/_Atomic/*
// Same bracket suffix pattern for abstract declarators
TEST_CASE("C11 ConflictResolver - direct-abstract-declarator bracket suffix", "[parser][conflict]")
{
    SECTION("abstract array") { ExpectParses("declaration", "void f(int []);"); }
    SECTION("abstract sized array") { ExpectParses("declaration", "void f(int [10]);"); }
    SECTION("abstract qualified array") { ExpectParses("declaration", "void f(int [const 10]);"); }
    SECTION("abstract VLA") { ExpectParses("declaration", "void f(int [*]);"); }
}

// conflict: alignment-specifier-_Alignas-suffix-(-suffix + Identifier
// Identifier is typedef-name → type-name; otherwise → constant-expression
TEST_CASE("C11 ConflictResolver - _Alignas type-name vs constant-expression", "[parser][conflict]")
{
    SECTION("alignas with type keyword") { ExpectParses("declaration", "_Alignas(int) int x;"); }
    SECTION("alignas with constant") { ExpectParses("declaration", "_Alignas(4) int x;"); }
    SECTION("alignas with expression") { ExpectParses("declaration", "_Alignas(2 + 2) int x;"); }
}
