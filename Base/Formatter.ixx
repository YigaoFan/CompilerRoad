export module Base:Formatter;

import std;

struct NoSpecialProcessParse
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        auto it = ctx.begin();
        if (it == ctx.end())
            return it;

        if (it != ctx.end() && *it != '}')
            throw std::format_error("Invalid format args");

        return it;
    }
};

export
{
    struct NoSpecialProcessParse;
}