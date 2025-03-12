export module CodeEmitter;

import std;
import Base;
import Parser;

using std::vector;
using std::ofstream;
using std::formatter;
using std::set;
using std::map;
using std::string;
using std::pair;
using std::format;
using std::format_to;

template <typename T>
struct CppCodeForm
{
    T const& Value;
};

template<>
struct formatter<CppCodeForm<vector<SimpleGrammar>>, char>
{
    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator
    {
        auto it = ctx.begin();
        return it;
    }

    template<class FormatContext>
    constexpr auto format(CppCodeForm<vector<SimpleGrammar>> const& t, FormatContext& fc) const -> FormatContext::iterator
    {
        std::range_formatter<std::remove_cvref_t<std::ranges::range_reference_t<vector<SimpleRightSide>>>> rangeFmt;
        rangeFmt.set_brackets("{", "}");
        rangeFmt.underlying().set_brackets("{", "}");
        std::basic_format_parse_context<char> fpc{":#}"};
        rangeFmt.underlying().parse(fpc);

        format_to(fc.out(), "export vector<SimpleGrammar> grammars =\n");
        format_to(fc.out(), "{{\n");
        for (auto const& x : t.Value)
        {
            format_to(fc.out(), "    {{ {:#}, ", x.first);
            rangeFmt.format(x.second, fc);
            format_to(fc.out(), " }},\n");
        }
        format_to(fc.out(), "}};");
        return fc.out();
    }
};

export
{
    template<>
    struct std::formatter<CppCodeForm<vector<SimpleGrammar>>, char>;
    template <typename T>
    struct CppCodeForm;
}