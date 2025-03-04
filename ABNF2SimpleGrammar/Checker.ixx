export module Checker;

import std;
import Base;
import Ast;

using std::map;

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