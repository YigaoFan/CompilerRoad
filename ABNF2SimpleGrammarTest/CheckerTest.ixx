module;
#include <catch2/catch_all.hpp>
export module DefinitionCheckerTest;
import std;
import Base;
import Ast;
import Checker;

using std::vector;
using std::string;
using std::shared_ptr;
using std::make_shared;
using std::move;

TEST_CASE("StarArrowChecker - valid star arrow lex rule", "[checker]")
{
    auto grammar = make_shared<Grammar>(
        String("Keyword"),
        make_shared<Productions>(vector<shared_ptr<Production>>{
            make_shared<Production>(vector<shared_ptr<Item>>{ make_shared<Terminal>(String("\"if\"")) }),
            make_shared<Production>(vector<shared_ptr<Item>>{ make_shared<Terminal>(String("\"else\"")) }),
        }),
        true
    );
    auto grammars = make_shared<Grammars>(vector<shared_ptr<Grammar>>{ grammar });

    auto checker = StarArrowChecker();
    REQUIRE_NOTHROW(checker.CheckLexRules(grammars.get()));
}

TEST_CASE("StarArrowChecker - star arrow with alias allows special chars", "[checker]")
{
    auto grammar = make_shared<Grammar>(
        String("Punctuator"),
        make_shared<Productions>(vector<shared_ptr<Production>>{
            make_shared<Production>(vector<shared_ptr<Item>>{ make_shared<Terminal>(String("\"==\""), String("Equal")) }),
            make_shared<Production>(vector<shared_ptr<Item>>{ make_shared<Terminal>(String("\"<=\""), String("LessEqual")) }),
        }),
        true
    );
    auto grammars = make_shared<Grammars>(vector<shared_ptr<Grammar>>{ grammar });

    auto checker = StarArrowChecker();
    REQUIRE_NOTHROW(checker.CheckLexRules(grammars.get()));
}

TEST_CASE("StarArrowChecker - star arrow with special char no alias throws", "[checker]")
{
    auto grammar = make_shared<Grammar>(
        String("Punctuator"),
        make_shared<Productions>(vector<shared_ptr<Production>>{
            make_shared<Production>(vector<shared_ptr<Item>>{ make_shared<Terminal>(String("\"==\"")) }),
        }),
        true
    );
    auto grammars = make_shared<Grammars>(vector<shared_ptr<Grammar>>{ grammar });

    auto checker = StarArrowChecker();
    REQUIRE_THROWS_AS(checker.CheckLexRules(grammars.get()), std::invalid_argument);
}

TEST_CASE("StarArrowChecker - star arrow with multiple items per production throws", "[checker]")
{
    auto grammar = make_shared<Grammar>(
        String("Keyword"),
        make_shared<Productions>(vector<shared_ptr<Production>>{
            make_shared<Production>(vector<shared_ptr<Item>>{
                make_shared<Terminal>(String("\"if\"")),
                make_shared<Terminal>(String("\"then\"")),
            }),
        }),
        true
    );
    auto grammars = make_shared<Grammars>(vector<shared_ptr<Grammar>>{ grammar });

    auto checker = StarArrowChecker();
    REQUIRE_THROWS_AS(checker.CheckLexRules(grammars.get()), std::invalid_argument);
}

TEST_CASE("StarArrowChecker - star arrow with non-terminal item throws", "[checker]")
{
    auto grammar = make_shared<Grammar>(
        String("Keyword"),
        make_shared<Productions>(vector<shared_ptr<Production>>{
            make_shared<Production>(vector<shared_ptr<Item>>{ make_shared<Symbol>(String("other")) }),
        }),
        true
    );
    auto grammars = make_shared<Grammars>(vector<shared_ptr<Grammar>>{ grammar });

    auto checker = StarArrowChecker();
    REQUIRE_THROWS_AS(checker.CheckLexRules(grammars.get()), std::invalid_argument);
}

TEST_CASE("StarArrowChecker - parse rule with star arrow throws", "[checker]")
{
    auto grammar = make_shared<Grammar>(
        String("stmt"),
        make_shared<Productions>(vector<shared_ptr<Production>>{
            make_shared<Production>(vector<shared_ptr<Item>>{ make_shared<Symbol>(String("sym")) }),
        }),
        true // IsStarArrow = true in parse rules is invalid
    );
    auto grammars = make_shared<Grammars>(vector<shared_ptr<Grammar>>{ grammar });

    auto checker = StarArrowChecker();
    REQUIRE_THROWS_AS(checker.CheckParseRules(grammars.get()), std::invalid_argument);
}

TEST_CASE("StarArrowChecker - parse rule without star arrow passes", "[checker]")
{
    auto grammar = make_shared<Grammar>(
        String("stmt"),
        make_shared<Productions>(vector<shared_ptr<Production>>{
            make_shared<Production>(vector<shared_ptr<Item>>{ make_shared<Symbol>(String("sym")) }),
        }),
        false
    );
    auto grammars = make_shared<Grammars>(vector<shared_ptr<Grammar>>{ grammar });

    auto checker = StarArrowChecker();
    REQUIRE_NOTHROW(checker.CheckParseRules(grammars.get()));
}

TEST_CASE("PrivateTokenRefChecker - parse rule referencing private token reports", "[checker]")
{
    // Lex rules: "keyword" (lowercase = private), "Identifier" (uppercase = public)
    auto lexGrammars = make_shared<Grammars>(vector<shared_ptr<Grammar>>{
        make_shared<Grammar>(String("keyword"), make_shared<Productions>(vector<shared_ptr<Production>>{
            make_shared<Production>(vector<shared_ptr<Item>>{ make_shared<Terminal>(String("\"if\"")) }),
        }), true),
        make_shared<Grammar>(String("Identifier"), make_shared<Productions>(vector<shared_ptr<Production>>{
            make_shared<Production>(vector<shared_ptr<Item>>{ make_shared<RegExp>(String("r\"[a-zA-Z]\"")) }),
        }), false),
    });

    // Parse rule references "keyword" (private token)
    auto parseGrammars = make_shared<Grammars>(vector<shared_ptr<Grammar>>{
        make_shared<Grammar>(String("stmt"), make_shared<Productions>(vector<shared_ptr<Production>>{
            make_shared<Production>(vector<shared_ptr<Item>>{
                make_shared<Symbol>(String("keyword")),
                make_shared<Symbol>(String("Identifier")),
            }),
        }), false),
    });

    auto allGrammars = make_shared<AllGrammars>(lexGrammars, parseGrammars);
    auto checker = PrivateTokenRefChecker();
    // Should not throw, just prints warning
    REQUIRE_NOTHROW(checker.Check(allGrammars.get()));
}

TEST_CASE("DefinitionChecker - detects undefined symbols via AstFactory", "[checker]")
{
    // In the real pipeline, DefinitionChecker is passed to AstFactory::Create which calls
    // the visitor on EVERY AST node during construction (including Symbol nodes).
    // Here we simulate that by manually visiting each node.
    auto symbol = make_shared<Symbol>(String("undefined_sym"));
    auto grammar = make_shared<Grammar>(
        String("stmt"),
        make_shared<Productions>(vector<shared_ptr<Production>>{
            make_shared<Production>(vector<shared_ptr<Item>>{ symbol }),
        }),
        false
    );

    auto checker = DefinitionChecker();
    // Simulate AstFactory: visit Grammar node (registers "stmt" with count 1)
    grammar->Visit(&checker);
    // Simulate AstFactory: visit Symbol node (registers "undefined_sym" with count 0)
    symbol->Visit(&checker);

    REQUIRE(checker.symbols.size() == 2);
    REQUIRE(checker.symbols[String("stmt")] == 1);
    REQUIRE(checker.symbols[String("undefined_sym")] == 0);
}

TEST_CASE("DefinitionChecker - duplicate definition detected", "[checker]")
{
    auto grammar1 = make_shared<Grammar>(
        String("stmt"),
        make_shared<Productions>(vector<shared_ptr<Production>>{
            make_shared<Production>(vector<shared_ptr<Item>>{ make_shared<Symbol>(String("a")) }),
        }),
        false
    );
    auto grammar2 = make_shared<Grammar>(
        String("stmt"),
        make_shared<Productions>(vector<shared_ptr<Production>>{
            make_shared<Production>(vector<shared_ptr<Item>>{ make_shared<Symbol>(String("b")) }),
        }),
        false
    );

    auto checker = DefinitionChecker();
    grammar1->Visit(&checker);
    grammar2->Visit(&checker);

    REQUIRE(checker.symbols[String("stmt")] == 2);
}
