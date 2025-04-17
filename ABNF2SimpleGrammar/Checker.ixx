export module Checker;

import std;
import Base;
import Ast;

using std::map;
using std::set;
using std::println;

struct Checker : DefaultVisitor<void>
{
    map<String, unsigned> symbols;

    Checker() : DefaultVisitor(), symbols()
    { }

    virtual auto Visit(Symbol* object) -> void
    {
        if (not symbols.contains(object->Value))
        {
            symbols.insert({ object->Value, 0 });
        }
    }

    virtual auto Visit(Grammar* object) -> void
    {
        ++symbols[object->Left];
    }

    auto Check() -> void
    {
        for (auto const& p : symbols)
        {
            switch (p.second)
            {
            case 0:
                println("{} doesn't have definition, please find similar symbol in grammar file to fix it", p.first);
                break;
            case 1:
                break;
            default:
                println("{} has more than one definitions", p.first);
                break;
            }
        }
    }
};

struct PrivateTokenRefChecker
{
    struct SymbolCollector : DefaultVisitor<set<String>>
    {
        auto Visit(Symbol* object) -> void override
        {
            Result.insert(object->Value);
        }

        auto Visit(Optional* object) -> void override
        {
            Visit(object->Productions.get());
        }

        auto Visit(Combine* object) -> void override
        {
            Visit(object->Productions.get());
        }

        auto Visit(Duplicate* object) -> void override
        {
            object->BasicItem->Visit(this);
        }

        auto Visit(Grammar* object) -> void override
        {
            Visit(object->Productions.get());
        }

        auto Visit(Productions* object) -> void override
        {
            for (auto const& p : object->Items)
            {
                Visit(p.get());
            }
        }

        auto Visit(Production* object) -> void override
        {
            for (auto const& p : object->Items)
            {
                p->Visit(this);
            }
        }
    };

    auto Check(AllGrammars* allGrammars) -> void
    {
        using std::shared_ptr;
        using std::ranges::views::transform;
        using std::ranges::to;
        using std::ranges::fold_left;

        auto toks = allGrammars->LexRules->Items | transform([](shared_ptr<Grammar> const& g) { return g->Left; }) | to<set<String>>();
        auto refSymbols = fold_left(allGrammars->ParseRules->Items | transform([](shared_ptr<Grammar> const& g) { auto c = SymbolCollector{}; g->Visit(&c); return c.Result; }),
            set<String>{}, [](set<String> allRefSyms, set<String> item) -> set<String> { allRefSyms.insert_range(move(item)); return allRefSyms; }); 
        for (auto const& x : refSymbols)
        {
            if (toks.contains(x) and not std::isupper(x[0]))
            {
                println("parse rules ref private token type: {}", x);
            }
        }
    }
};

export
{
    struct Checker;
    struct PrivateTokenRefChecker;
}