export module Checker;

import std;
import Base;
import Ast;

using std::map;

struct Checker : IVisitor
{
    map<String, unsigned> symbols;

    Checker() : symbols()
    { }

    virtual auto Visit(Terminal* object) -> void
    {
    }

    virtual auto Visit(Symbol* object) -> void
    {
        if (not symbols.contains(object->Value))
        {
            symbols.insert({ object->Value, 0 });
        }
    }

    virtual auto Visit(DataRange* object) -> void
    {
    }

    virtual auto Visit(Optional* object) -> void
    {
    }

    virtual auto Visit(Combine* object) -> void
    {
    }

    virtual auto Visit(Duplicate* object) -> void
    {
    }

    virtual auto Visit(Grammar* object) -> void
    {
        ++symbols[object->Left];
    }

    virtual auto Visit(Grammars* object) -> void
    {

    }

    virtual auto Visit(Production* object) -> void
    {

    }

    virtual auto Visit(Productions* object) -> void
    {

    }

    auto Check() -> void
    {
        using std::println;

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

export
{
    struct Checker;
}