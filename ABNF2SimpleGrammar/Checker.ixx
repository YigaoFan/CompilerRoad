export module Checker;

import std;
import Base;
import Ast;

using std::map;
using std::set;
using std::println;
using std::format;
using std::string_view;

struct DefinitionChecker : DefaultVisitor<void>
{
    map<String, unsigned> symbols;

    DefinitionChecker() : DefaultVisitor(), symbols()
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

/// <summary>
/// Star arrow rule '-*->' is a special syntax sugar for lex rules, it means the production of this rule is a list of string literals, 
/// and each literal will produce a token with the same name as the literal (or the alias provided by @name).
//  This checker is to limit the usage of star arrow rule and make sure it is used correctly
/// </summary>
struct StarArrowChecker
{
    auto CheckLexRules(Grammars const* lexRules) -> void
    {
        for (auto const& g : lexRules->Items)
        {
            if (not g->IsStarArrow) continue;
            // check: each production must have exactly one Terminal item
            for (auto const& production : g->Productions->Items)
            {
                if (production->Items.size() != 1)
                {
                    throw std::invalid_argument(format("star arrow rule '{}' must have exactly one literal per alternative", g->Left));
                }
                auto terminal = dynamic_cast<Terminal const*>(production->Items.front().get());
                if (terminal == nullptr)
                {
                    throw std::invalid_argument(format("star arrow rule '{}' must only contain string literals", g->Left));
                }
                if (terminal->Alias.Empty())
                {
                    auto literal = terminal->Value.Substring(1, terminal->Value.Length() - 2); // strip surrounding quotes
                    for (auto c : literal)
                    {
                        // only allow English letters, digits and '_'
                        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
                        {
                            throw std::invalid_argument(format("star arrow rule '{}' literal \"{}\" contains invalid character '{}', only English letters, digits and '_' are allowed (or provide a @name)", g->Left, literal, c));
                        }
                    }
                }
            }
        }
    }

    auto CheckParseRules(Grammars const* parseRules) -> void
    {
        for (auto const& g : parseRules->Items)
        {
            if (g->IsStarArrow)
            {
                throw std::invalid_argument(format("star arrow rule '-*->' is only allowed in lex rules, found in parse rule '{}'", g->Left));
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
    struct DefinitionChecker;
    struct StarArrowChecker;
    struct PrivateTokenRefChecker;
}