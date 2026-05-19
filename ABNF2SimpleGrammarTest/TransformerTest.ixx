module;
#include <catch2/catch_all.hpp>
export module TransformerTest;
import std;
import Base;
import Parser;
import Ast;
import Transformer;

using std::vector;
using std::string;
using std::string_view;
using std::shared_ptr;
using std::make_shared;
using std::map;
using std::move;

TEST_CASE("GetEnumName - basic conversion", "[transformer]")
{
    REQUIRE(string_view(GetEnumName(String("keyword"), false)) == "keyword");
    REQUIRE(string_view(GetEnumName(String("keyword"), true)) == "Keyword");
}

TEST_CASE("GetEnumName - hyphen removal with camelCase", "[transformer]")
{
    REQUIRE(string_view(GetEnumName(String("my-var"), false)) == "myVar");
    REQUIRE(string_view(GetEnumName(String("my-var"), true)) == "MyVar");
    REQUIRE(string_view(GetEnumName(String("a-b-c"), true)) == "ABC");
}

TEST_CASE("GetEnumName - underscore with camelCase", "[transformer]")
{
    // Note: underscore triggers camelCase but is NOT removed (only '-' is removed)
    REQUIRE(string_view(GetEnumName(String("my_var"), false)) == "my_Var");
    REQUIRE(string_view(GetEnumName(String("my_var"), true)) == "My_Var");
}

TEST_CASE("NormalEscape2RegExpAsPrintLiteral - simple string", "[transformer]")
{
    // input includes surrounding double quotes
    auto result = NormalEscape2RegExpAsPrintLiteral(String("\"hello\""));
    REQUIRE(string_view(result) == "hello");
}

TEST_CASE("NormalEscape2RegExpAsPrintLiteral - regex special chars escaped", "[transformer]")
{
    // Note: '.' is NOT in regExpOperators, so it's not escaped
    // Each regex operator gets two literal backslashes appended
    auto result = NormalEscape2RegExpAsPrintLiteral(String("\"a.b*c+d\""));
    REQUIRE(string_view(result) == R"(a.b\\*c\\+d)");
}

TEST_CASE("NormalEscape2RegExpAsPrintLiteral - backslash escape", "[transformer]")
{
    auto result = NormalEscape2RegExpAsPrintLiteral(String("\"a\\\\b\""));
    REQUIRE(string_view(result) == "a\\\\\\\\b");
}

TEST_CASE("RegExpEscape2PrintLiteral - simple regex", "[transformer]")
{
    // input: r"..." where the r prefix is stripped
    auto result = RegExpEscape2PrintLiteral(String("r\"[a-z]+\""));
    REQUIRE(string_view(result) == "[a-z]+");
}

TEST_CASE("LexRule2RegExpTransformer - star arrow grammar", "[transformer]")
{
    auto keywordGrammar = make_shared<Grammar>(
        String("Keyword"),
        make_shared<Productions>(vector<shared_ptr<Production>>{
            make_shared<Production>(vector<shared_ptr<Item>>{ make_shared<Terminal>(String("\"if\"")) }),
            make_shared<Production>(vector<shared_ptr<Item>>{ make_shared<Terminal>(String("\"else\"")) }),
        }),
        true // IsStarArrow
    );
    auto grammars = make_shared<Grammars>(vector<shared_ptr<Grammar>>{ keywordGrammar });

    auto tokInfo = LexRule2RegExpTransformer::Transform(grammars.get());
    REQUIRE(tokInfo.Symbol2EnumNameRegExp.size() == 2);
    REQUIRE(tokInfo.PrioritySymbols.size() == 2);

    auto ifIt = tokInfo.Symbol2EnumNameRegExp.find(String("if"));
    REQUIRE(ifIt != tokInfo.Symbol2EnumNameRegExp.end());
    REQUIRE(string_view(ifIt->second.EnumName) == "Keyword_If");
    REQUIRE(ifIt->second.Generated == true);

    auto elseIt = tokInfo.Symbol2EnumNameRegExp.find(String("else"));
    REQUIRE(elseIt != tokInfo.Symbol2EnumNameRegExp.end());
    REQUIRE(string_view(elseIt->second.EnumName) == "Keyword_Else");
}

TEST_CASE("LexRule2RegExpTransformer - star arrow with alias", "[transformer]")
{
    auto punctGrammar = make_shared<Grammar>(
        String("Punctuator"),
        make_shared<Productions>(vector<shared_ptr<Production>>{
            make_shared<Production>(vector<shared_ptr<Item>>{ make_shared<Terminal>(String("\"==\""), String("Equal")) }),
            make_shared<Production>(vector<shared_ptr<Item>>{ make_shared<Terminal>(String("\"!=\""), String("NotEqual")) }),
        }),
        true
    );
    auto grammars = make_shared<Grammars>(vector<shared_ptr<Grammar>>{ punctGrammar });

    auto tokInfo = LexRule2RegExpTransformer::Transform(grammars.get());
    REQUIRE(tokInfo.Symbol2EnumNameRegExp.size() == 2);

    auto eqIt = tokInfo.Symbol2EnumNameRegExp.find(String("=="));
    REQUIRE(eqIt != tokInfo.Symbol2EnumNameRegExp.end());
    REQUIRE(string_view(eqIt->second.EnumName) == "Punctuator_Equal");

    auto neIt = tokInfo.Symbol2EnumNameRegExp.find(String("!="));
    REQUIRE(neIt != tokInfo.Symbol2EnumNameRegExp.end());
    REQUIRE(string_view(neIt->second.EnumName) == "Punctuator_NotEqual");
}

TEST_CASE("LexRule2RegExpTransformer - non-star-arrow grammar with terminals", "[transformer]")
{
    // A simple non-star-arrow grammar: Stmt -> "if" sym "then" sym
    auto grammar = make_shared<Grammar>(
        String("Stmt"),
        make_shared<Productions>(vector<shared_ptr<Production>>{
            make_shared<Production>(vector<shared_ptr<Item>>{
                make_shared<Terminal>(String("\"if\"")),
                make_shared<Symbol>(String("Expr")),
                make_shared<Terminal>(String("\"then\"")),
                make_shared<Symbol>(String("Expr")),
            }),
        }),
        false
    );
    // Expr is a simple terminal-only grammar
    auto exprGrammar = make_shared<Grammar>(
        String("Expr"),
        make_shared<Productions>(vector<shared_ptr<Production>>{
            make_shared<Production>(vector<shared_ptr<Item>>{
                make_shared<Terminal>(String("\"0\"")),
            }),
        }),
        false
    );
    auto grammars = make_shared<Grammars>(vector<shared_ptr<Grammar>>{ grammar, exprGrammar });

    auto tokInfo = LexRule2RegExpTransformer::Transform(grammars.get());
    // Expr starts with uppercase so it's kept; Stmt also starts with uppercase
    REQUIRE(tokInfo.Symbol2EnumNameRegExp.size() == 2);

    auto stmtIt = tokInfo.Symbol2EnumNameRegExp.find(String("Stmt"));
    REQUIRE(stmtIt != tokInfo.Symbol2EnumNameRegExp.end());
    REQUIRE(string_view(stmtIt->second.EnumName) == "Stmt");
    REQUIRE(stmtIt->second.RegExp.Length() > 0);

    auto exprIt = tokInfo.Symbol2EnumNameRegExp.find(String("Expr"));
    REQUIRE(exprIt != tokInfo.Symbol2EnumNameRegExp.end());
    REQUIRE(string_view(exprIt->second.EnumName) == "Expr");
}

TEST_CASE("MergeTokInfo - combines lex and parse tokens", "[transformer]")
{
    TokensInfo lexInfo;
    lexInfo.Symbol2EnumNameRegExp.insert({ String("Keyword"), { String("keyword"), String("if|else") } });
    lexInfo.PrioritySymbols.push_back(String("Keyword"));

    TokensInfo parseInfo;
    parseInfo.Symbol2EnumNameRegExp.insert({ String("terminal-0"), { String("Terminal0"), String("=") } });
    parseInfo.PrioritySymbols.push_back(String("terminal-0"));

    auto merged = LexRule2RegExpTransformer::MergeTokInfo(move(lexInfo), move(parseInfo));
    REQUIRE(merged.Symbol2EnumNameRegExp.size() == 2);
    REQUIRE(merged.PrioritySymbols.size() == 2);
}
