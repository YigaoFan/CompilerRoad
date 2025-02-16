export module Formatter;

import std;
import Parser;

using std::vector;

template<>
struct std::formatter<vector<SimpleGrammar>, char>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        // not implement
        auto it = ctx.begin();
        if (it == ctx.end())
            return it;

        if (it != ctx.end() && *it != '}')
            throw std::format_error("Invalid format args for QuotableString.");

        return it;
    }

    template<class FormatContext>
    constexpr auto format(vector<SimpleGrammar> const& t, FormatContext& fc) const
    {
        std::formatter<SimpleGrammar> pairFmt;
        pairFmt.set_brackets("{", "}");
        for (auto const& x : t)
        {
            pairFmt.format(x, fc);
            std::format_to(fc.out(), ", ");
        }
        return fc.out();
    }
};

export
{
    template<>
    struct std::formatter<vector<SimpleGrammar>, char>;
}