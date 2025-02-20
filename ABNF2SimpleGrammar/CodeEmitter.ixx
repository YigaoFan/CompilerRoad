export module CodeEmitter;

import std;
import Base;
import Parser;

using std::vector;
using std::ofstream;
using std::formatter;
using std::pair;
using std::format;
using std::format_to;

template <typename T>
struct CppCodeForm
{
    T Value;
};

auto Emit(vector<SimpleGrammar> const& grammars, ofstream& ostream) -> void
{
    struct OnlyOutFormatContext
    {
        ofstream& Output;
        using iterator = std::ostream_iterator<char> ;
        OnlyOutFormatContext(ofstream& output) : Output(output)
        { }

        auto out()
        {
            return std::ostream_iterator<char>(Output);
        }
    };
    ostream << "{";
    std::range_formatter<std::remove_cvref_t<std::ranges::range_reference_t<vector<SimpleRightSide>>>> rsFmt;
    rsFmt.set_brackets("{", "}");
    rsFmt.underlying().set_brackets("{", "}");
    rsFmt.underlying();
    for (auto& x : grammars)
    {
        ostream << format("{{ {:#}, }},\n", x.first);
        //format("{}", rsFmt, x.second);
        std::basic_format_context<std::ostream_iterator<char>, char>* fpc = nullptr;
        auto fc = OnlyOutFormatContext{ ostream, };
        rsFmt.format(x.second, fc);
    }
    ostream << "}";
}

template<>
struct formatter<CppCodeForm<vector<SimpleGrammar>>, char>
{
    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator
    {
        //RangeFmt.underlying().parse(ctx);
        auto it = ctx.begin();
        return it;
    }

    template<class... Args>
    static auto GetFormatString(std::format_string<Args...> fmt, Args&&... args) -> std::format_string<Args...>
    {
        return fmt;
    }

    template<class FormatContext>
    constexpr auto format(CppCodeForm<vector<SimpleGrammar>> const& t, FormatContext& fc) const -> FormatContext::iterator
    {
        std::range_formatter<std::remove_cvref_t<std::ranges::range_reference_t<vector<SimpleRightSide>>>> rangeFmt;
        rangeFmt.set_brackets("{", "}");
        rangeFmt.underlying().set_brackets("{", "}");
        std::basic_format_parse_context<char> fpc{":#}"};
        rangeFmt.underlying().parse(fpc);

        format_to(fc.out(), "{{");
        for (auto const& x : t.Value)
        {
            format_to(fc.out(), "{{ {:#}, ", x.first);
            rangeFmt.format(x.second, fc);
            format_to(fc.out(), "}},\n");
        }
        format_to(fc.out(), "}}");
        return fc.out();
    }
};

export
{
    template<>
    struct std::formatter<CppCodeForm<vector<SimpleGrammar>>, char>;
    template <typename T>
    struct CppCodeForm;
    auto Emit(vector<SimpleGrammar> const& grammars, ofstream& ostream) -> void;
}