export module Parser:ConflictDetector;

import std;
import Base;
import :ParserBase;
import :GrammarSet;
import :GrammarProcess;

using std::vector;
using std::map;
using std::move;

// generate all conflict pairs
struct ConflictDetail
{
    struct Item
    {
        String Nontermin;
        String Terminal;
        vector<SimpleRightSide> ConflictRules;

        auto operator< (Item const& that) const -> bool
        {
            if (Nontermin != that.Nontermin)
            {
                return Nontermin < that.Nontermin;
            }
            else if (Terminal != that.Terminal)
            {
                return Terminal < that.Terminal;
            }
            else
            {
                return ConflictRules < that.ConflictRules;
            }
        }
    };
    vector<Item> Items;
};

template <>
struct std::formatter<ConflictDetail, char> : NoSpecialProcessParse
{
    template<class FormatContext>
    constexpr auto format(ConflictDetail const& t, FormatContext& fc) const
    {
        for (auto const& x : t.Items)
        {
            std::format_to(fc.out(), "Conflict:\n  Nonterminal: {}\n  Terminal:    {}\n  Rules:\n", x.Nontermin, x.Terminal);
            for (auto const& rule : x.ConflictRules)
            {
                std::format_to(fc.out(), "    -");
                for (auto const& symbol : rule)
                {
                    std::format_to(fc.out(), " {}", symbol);
                }
                std::format_to(fc.out(), "\n");
            }
        }
        return fc.out();
    }
};

auto DetectConflicts(String startSymbol, SimpleGrammars grammars) -> ConflictDetail
{
    using std::ranges::views::filter;

    vector<SimpleGrammar> newAddGrammars;
    for (auto& g : grammars)
    {
        auto [newG, addGrammars] = LeftFactor(move(g));
        g.second = move(newG.second);
        if (addGrammars.has_value())
        {
            newAddGrammars.append_range(move(addGrammars.value()));
        }
    }
    grammars.insert_range(move(newAddGrammars));

	auto starts = Starts(startSymbol, grammars);
    ConflictDetail conflicts;
    for (auto const& g : starts.GrammarsWithStartSet)
    {
        auto const& nontermin = g.first;
        auto const& rulesWithstart = g.second;
		map<String, vector<SimpleRightSide>> terminal2RuleIndex;

        for (auto const& r : rulesWithstart)
        {
            for (String const& termin : r.second)
            {
				terminal2RuleIndex[termin].push_back(r.first);
            }
        }

        for (auto& [termin, rules] : terminal2RuleIndex | filter([](auto& i) { return i.second.size() > 1; }))
        {
			conflicts.Items.push_back({ .Nontermin = nontermin, .Terminal = termin, .ConflictRules = move(rules) });
        }
    }
	return conflicts;
}

export
{
    struct ConflictDetail;
	auto DetectConflicts(String startSymbol, SimpleGrammars grammars) -> ConflictDetail;
}