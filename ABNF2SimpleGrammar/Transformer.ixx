export module Transformer;

import std;
import Base;
import Parser;
import Ast;
import CodeEmitter;

using std::vector;
using std::string;
using std::set;
using std::pair;
using std::string_view;
using std::map;
using std::formatter;
using std::format;
using std::move;
using std::println;
using std::ranges::views::transform;

constexpr string_view regExpOperators = "?*+|-[^]()";

auto AppendCppStringLiteralSlash(string& str) -> void
{
    str.append("\\\\");
}

/// <returns>without quote around</returns>
auto NormalEscape2RegExpAsPrintLiteral(String normalString) -> String
{
    string regExp;
    regExp.reserve(normalString.Length());

    auto s = string_view(normalString);
    for (size_t i = 1, validLen = s.size() - 1; i < validLen; ++i) // ignore the first and last double quote
    {
        switch (auto c = s[i]; c)
        {
        case '\\':
            if (i + 1 >= validLen)
            {
                throw std::runtime_error("syntax error: only slash in the string, not escape anything");
            }
            else
            {
                ++i;
                auto nextCh = s[i];
                if (nextCh == '\\')
                {
                    AppendCppStringLiteralSlash(regExp);
                    AppendCppStringLiteralSlash(regExp);
                }
                else
                {
                    regExp.push_back('\\');
                    regExp.push_back(nextCh);
                }
            }
            break;
        default:
            if (regExpOperators.contains(c))
            {
                AppendCppStringLiteralSlash(regExp);
            }
            regExp.push_back(c);
            break;
        }
    }
    return String(regExp);
}

/// <returns>without quote around</returns>
auto RegExpEscape2PrintLiteral(String regExpInAbnf) -> String
{
    string regExp;
    regExp.reserve(regExpInAbnf.Length());
    auto s = string_view(regExpInAbnf);
    for (size_t i = 2, validLen = s.size() - 1; i < validLen; ++i) // ignore the first(r") and last double quote
    {
        switch (auto c = s[i]; c)
        {
        case '\\':
            if (i + 1 >= validLen)
            {
                throw std::runtime_error("syntax error: only slash in the string, not escape anything");
            }
            else
            {
                ++i;
                // handle \\, \r\n, \-\[
                switch (auto nextCh = s[i]; nextCh)
                {
                case '\\':
                    AppendCppStringLiteralSlash(regExp);
                    AppendCppStringLiteralSlash(regExp);
                    break;
                default:
                    if (regExpOperators.contains(nextCh))
                    {
                        AppendCppStringLiteralSlash(regExp);
                        regExp.push_back(nextCh);
                    }
                    else
                    {
                        regExp.push_back('\\');
                        regExp.push_back(nextCh);
                    }
                }
            }
            break;
        default:
            regExp.push_back(c);
            break;
        }
    }
    return String(regExp);
}

auto GetEnumName(String symbolName, bool capitalize1stCharFlag) -> String
{
    using std::toupper;

    auto s = string(symbolName);
    if (capitalize1stCharFlag)
    {
        s[0] = toupper(s[0]);
    }
    for (size_t i = 1; i < s.size(); ++i)
    {
        if (s[i] == '-' or s[i] == '_')
        {
            s[i + 1] = toupper(s[i + 1]);
        }
    }
    s.erase(std::remove(s.begin(), s.end(), '-'), s.end());
    return String(s);
}

struct TokensInfo
{
    map<String, pair<String, String>> Symbol2EnumNameRegExp;
    vector<String> PrioritySymbols;
};

template<>
struct formatter<CppCodeForm<TokensInfo>, char>
{
    constexpr auto parse(std::format_parse_context& ctx) -> std::format_parse_context::iterator
    {
        auto it = ctx.begin();
        return it;
    }

    template<class FormatContext>
    constexpr auto format(CppCodeForm<TokensInfo> const& t, FormatContext& fc) const -> FormatContext::iterator
    {
        format_to(fc.out(), "export enum class TokType : int\n");
        format_to(fc.out(), "{{\n");
        for (auto const& x : t.Value.PrioritySymbols)
        {
            format_to(fc.out(), "    {},\n", t.Value.Symbol2EnumNameRegExp.at(x).first); // print by priority
        }
        format_to(fc.out(), "}};\n");

        format_to(fc.out(), "export array lexRules =\n");
        format_to(fc.out(), "{{\n");
        for (auto const& x : t.Value.Symbol2EnumNameRegExp)
        {
            format_to(fc.out(), "    pair<string, TokType>{{ \"{}\", TokType::{} }},\n", x.second.second, x.second.first);
        }
        format_to(fc.out(), "}};\n");

        format_to(fc.out(), "export map<string_view, int> terminal2IntTokenType =\n");
        format_to(fc.out(), "{{\n");
        for (auto const& x : t.Value.Symbol2EnumNameRegExp)
        {
            format_to(fc.out(), "    {{ \"{}\", static_cast<int>(TokType::{}) }},\n", x.first, x.second.first);
        }
        format_to(fc.out(), "}};");
        return fc.out();
    }
};

// if name is occupied, append number which
class ParseRule2SimpleGrammarTransformer
{
public:
    struct SimpleGrammarsInfo
    {
        vector<SimpleGrammar> Grammars;
        TokensInfo ToksInfo;
    };
    struct GrammarTransformInfo
    {
        String Left;
        vector<SimpleRightSide> MainRights;
        // use below 3 items to share some grammars construct, which should not be clear in low level, only allow to add
        vector<SimpleGrammar>& OtherGrammars;
        TokensInfo& ToksInfo;
        map<String, String>& Terminals; // terminal also has priority, exact match is higher than others
        int& Counter;

        auto AppendOnLastRule(String symbol) -> void
        {
            if (MainRights.empty())
            {
                MainRights.push_back({});
            }
            MainRights.back().push_back(symbol);
        }

        auto RegisterTerminalName(String terminalValue) -> String
        {
            if (not Terminals.contains(terminalValue))
            {
                int i = Terminals.size();
                auto symName = String(format("terminal-{}", i));
                Terminals.insert({ terminalValue, symName });
                ToksInfo.PrioritySymbols.push_back(symName);
                ToksInfo.Symbol2EnumNameRegExp.insert({ symName, { GetEnumName(symName, true), terminalValue } });
            }
            return Terminals.at(terminalValue);
        }

        auto CreateSubInfo(String left = {}) const -> GrammarTransformInfo
        {
            return GrammarTransformInfo{ .Left = move(left), .OtherGrammars = OtherGrammars, .ToksInfo = ToksInfo, .Terminals = Terminals, .Counter = Counter };
        }

        auto ResetCurrentGrammarInfo()
        {
            Left = String();
            MainRights.clear();
        }
    };
    struct TransformVisitor : DefaultVisitor<void>
    {
        GrammarTransformInfo* Info;
        TransformVisitor(GrammarTransformInfo* info) : Info(info)
        { }

        auto Visit(Terminal* object) -> void override
        {
            Transform(object, Info);
        }

        auto Visit(RegExp* object) -> void override
        {
            Transform(object, Info);
        }

        auto Visit(Symbol* object) -> void override
        {
            Transform(object, Info);
        }

        auto Visit(DataRange* object) -> void override
        {
            Transform(object, Info);
        }

        auto Visit(Optional* object) -> void override
        {
            Transform(object, Info);
        }

        auto Visit(Combine* object) -> void override
        {
            Transform(object, Info);
        }

        auto Visit(Duplicate* object) -> void override
        {
            Transform(object, Info);
        }

        auto Transform(Duplicate const* duplicate, GrammarTransformInfo* info) -> void
        {
            String basicItemGrammarName{ format("{}_dup_basicItem_{}", info->Left, info->Counter++) };
            GrammarTransformInfo subInfo = info->CreateSubInfo(basicItemGrammarName);
            ParseRule2SimpleGrammarTransformer::Transform(duplicate->BasicItem.get(), &subInfo);

            String auxGrammarName{ format("{}_dup_{}", info->Left, info->Counter++) };
            vector<SimpleRightSide> rss;
            // if max is int_max, use right recursive
            if (duplicate->High == std::numeric_limits<unsigned>::max())
            {
                rss.push_back(vector(duplicate->Low, basicItemGrammarName));
                rss.push_back({ basicItemGrammarName, auxGrammarName });
            }
            else
            {
                for (auto i = duplicate->Low; i <= duplicate->High; ++i)
                {
                    rss.push_back(vector(i, basicItemGrammarName));
                }
            }

            info->OtherGrammars.push_back({ auxGrammarName, move(rss) });
            info->OtherGrammars.push_back({ subInfo.Left, move(subInfo.MainRights) });
            info->AppendOnLastRule(auxGrammarName);
        }

        auto Transform(Combine const* combine, GrammarTransformInfo* info) -> void
        {
            String auxGrammarName{ format("{}_com_{}", info->Left, info->Counter++) };
            GrammarTransformInfo subInfo = info->CreateSubInfo(auxGrammarName);
            ParseRule2SimpleGrammarTransformer::Transform(combine->Productions.get(), &subInfo);

            info->OtherGrammars.push_back({ subInfo.Left, move(subInfo.MainRights) });
            info->AppendOnLastRule(auxGrammarName);
        }

        auto Transform(Optional const* optional, GrammarTransformInfo* info) -> void
        {
            String auxGrammarName{ format("{}_op_{}", info->Left, info->Counter++) };
            GrammarTransformInfo subInfo = info->CreateSubInfo(auxGrammarName);
            ParseRule2SimpleGrammarTransformer::Transform(optional->Productions.get(), &subInfo);
            subInfo.MainRights.push_back({}); // make it optional

            info->OtherGrammars.push_back({ subInfo.Left, move(subInfo.MainRights) });
            info->AppendOnLastRule(auxGrammarName);
        }

        auto Transform(DataRange const* dataRange, GrammarTransformInfo* info) -> void
        {
            String auxGrammarName{ format("{}_dr_{}", info->Left, info->Counter++) };
            // use regular expression to express DataRange
            info->RegisterTerminalName(String(format("\"{}-{}\"", (char)dataRange->Left, (char)dataRange->Right)));
            info->AppendOnLastRule(auxGrammarName);
        }

        auto Transform(Symbol const* symbol, GrammarTransformInfo* info) -> void
        {
            info->AppendOnLastRule(symbol->Value);
        }

        auto Transform(Terminal const* terminal, GrammarTransformInfo* info) -> void
        {
            auto name = info->RegisterTerminalName(NormalEscape2RegExpAsPrintLiteral(terminal->Value));
            info->AppendOnLastRule(name);
        }

        auto Transform(RegExp const* regExp, GrammarTransformInfo* info) -> void
        {
            auto name = info->RegisterTerminalName(RegExpEscape2PrintLiteral(regExp->Value));
            info->AppendOnLastRule(name);
        }
    };

    /// <param name="info">caller provide the info with counter set, other properties not set</param>
    static auto Transform(Grammar const* grammar, GrammarTransformInfo* info) -> SimpleGrammar
    {
        info->Left = grammar->Left;
        Transform(grammar->Productions.get(), info);
        return { info->Left, move(info->MainRights) };
    }

    static auto Transform(Grammars const* grammars) -> SimpleGrammarsInfo
    {
        SimpleGrammarsInfo grammarsInfo;
        auto counter = 0;
        map<String, String> terminals;
        vector<SimpleGrammar> otherGrammars;

        GrammarTransformInfo rootInfo{ .OtherGrammars = otherGrammars, .ToksInfo = grammarsInfo.ToksInfo, .Terminals = terminals, .Counter = counter, };
        for (auto const& g : grammars->Items)
        {
            auto sg = Transform(g.get(), &rootInfo);
            grammarsInfo.Grammars.push_back(move(sg));
            rootInfo.ResetCurrentGrammarInfo();
        }
        grammarsInfo.Grammars.append_range(move(otherGrammars));
        return grammarsInfo;
    }

    static auto Transform(Item* item, GrammarTransformInfo* info) -> void
    {
        TransformVisitor v(info);
        item->Visit(&v);
    }

    static auto Transform(Productions const* productions, GrammarTransformInfo* info) -> void
    {
        for (auto const& p : productions->Items)
        {
            auto subInfo = info->CreateSubInfo(info->Left);
            Transform(p.get(), &subInfo);
            info->MainRights.append_range(move(subInfo.MainRights));
        }
    }

    static auto Transform(Production const* production, GrammarTransformInfo* info) -> void
    {
        for (auto const& item : production->Items)
        {
            Transform(item.get(), info);
        }
    }
};

struct LexRule2RegExpTransformer
{
    /// <summary>
    /// outside(ref place) is responsible for adding parentheses, inside not add
    /// when ref other regExp, chose to add paren or not
    /// </summary>
    struct ItemTransfomer : DefaultVisitor<string>
    {
        map<String, string> const& OtherSymbolRegExps;

        ItemTransfomer(map<String, string> const& otherSymbolRegExps) : OtherSymbolRegExps(otherSymbolRegExps)
        {
        }

        auto Visit(Terminal* object) -> void override
        {
            Result = string(NormalEscape2RegExpAsPrintLiteral(object->Value));
        }

        auto Visit(RegExp* object) -> void override
        {
            Result = string(RegExpEscape2PrintLiteral(object->Value));
        }

        auto Visit(Symbol* object) -> void override
        {
            if (OtherSymbolRegExps.contains(object->Value))
            {
                Result = OtherSymbolRegExps.at(object->Value);
                return;
            }
            throw std::out_of_range(format("not regexp definition for {}", object->Value));
        }

        auto Visit(DataRange* object) -> void override
        {
            Result = format("{}-{}", (char)object->Left, (char)object->Right);
        }

        auto Visit(Optional* object) -> void override
        {
            Result = format("({})?", Transform(object->Productions.get(), OtherSymbolRegExps));
        }

        auto Visit(Combine* object) -> void override
        {
            Result = format("{}", Transform(object->Productions.get(), OtherSymbolRegExps));
        }

        auto Visit(Duplicate* object) -> void override
        {
            auto item = Transform(object->BasicItem.get(), OtherSymbolRegExps);
            auto Dup = [&](unsigned n) -> string
            {
                string s;
                for (unsigned i = 0; i < n; ++i)
                {
                    s.append(format("({})", item));
                }
                return s;
            };
            if (object->High == std::numeric_limits<unsigned>::max())
            {
                if (object->Low > 0)
                {
                    // Dup already add (), so not add () around here
                    Result.append(format("{}", Dup(object->Low)));
                }
                Result.append(format("({})*", item));
                return;
            }

            for (auto i = object->Low; i <= object->High; ++i)
            {
                Result.append(format("{}", Dup(i)));
                Result.push_back('|');
            }
            Result.pop_back(); // remove the extra | char
        }
    };
    struct SymbolCollector : DefaultVisitor<set<String>>
    {
        String LeftOfCurrentGrammar;

        SymbolCollector(String leftOfCurrentGrammar) : LeftOfCurrentGrammar(leftOfCurrentGrammar)
        {
        }

        auto Visit(Symbol* object) -> void override
        {
            if (object->Value == LeftOfCurrentGrammar)
            {
                throw std::invalid_argument(format("not support recur in lex grammar rule, found: {}", object->Value));
            }
            //println("using {}", object->Value);
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

    static auto Transform(Grammar const* grammar, map<String, string> const& convertedRegExps) -> string
    {
        return Transform(grammar->Productions.get(), convertedRegExps);
    }

    /// <returns>pair in map is (rule name, printable string literal without quote)</returns>
    static auto Transform(Grammars const* grammars) -> TokensInfo
    {
        using std::tuple;
        using std::queue;
        using std::get;
        using std::ranges::views::reverse;
        using std::ranges::views::filter;
        using std::ranges::fold_left;
        using std::ranges::to;

        auto ReferingIn = [](Grammar* grammar) -> set<String>
        {
            SymbolCollector collector{ grammar->Left };
            grammar->Visit(&collector);
            //println("using symbol collect: {}", collector.Result);
            return collector.Result;
        };

        map<String, tuple<set<String>, int, Grammar const*>> refInfo;
        map<size_t, String> priority2Symbols;
        auto InitIfNotExist = [&refInfo](String symbolName) -> tuple<set<String>, int, Grammar const*>&
        {
            if (not refInfo.contains(symbolName))
            {
                refInfo.insert({ symbolName, { {}, 0, nullptr } });
            }
            return refInfo.at(symbolName);
        };
        for (auto priority = 0; auto const& g : grammars->Items)
        {
            //println("analyse grammar: {}", g->Left);
            priority2Symbols.insert({ priority++, g->Left });
            get<2>(refInfo[g->Left]) = g.get();
            auto refs = ReferingIn(g.get());
            for (auto const& r : refs)
            {
                ++get<1>(refInfo[r]);
            }
            get<0>(refInfo[g->Left]) = move(refs);
        }

        queue<String> workings;
        for (auto const& x : refInfo)
        {
            if (get<1>(x.second) == 0)
            {
                workings.push(x.first);
            }
        }
        vector<Grammar const*> ordereds; // higher first
        while (not workings.empty())
        {
            auto symbol = move(workings.front());
            workings.pop();
            auto& info = refInfo.at(symbol);
            ordereds.push_back(get<2>(info));

            for (auto const& ref : get<0>(info))
            {
                auto refCount = --get<1>(refInfo.at(ref));
                if (refCount == 0)
                {
                    workings.push(ref);
                }
            }
        }

        if (ordereds.size() < refInfo.size())
        {
            throw std::invalid_argument("there exists recur reference in lex rules");
        }

        map<String, string> regExps;

        for (auto g : reverse(ordereds))
        {
            regExps.insert({ g->Left, Transform(g, regExps) });
        }

        auto debug = false;
        if (not debug)
        {
            // only reserve the capitalized first char one which is thought as final token type
            regExps = regExps | filter([](pair<String, string> const& x) -> bool { return std::isupper(x.first[0]); }) | to<map<String, string>>();
        }

        TokensInfo tokInfo
        {
            .Symbol2EnumNameRegExp =
                fold_left(regExps | transform([](pair<String, string> const& x) -> pair<String, pair<String, String>> { return { x.first, { GetEnumName(x.first, false), String(x.second) } }; }),
                    map<String, pair<String, String>>(), [](map<String, pair<String, String>> xs, pair<String, pair<String, String>> x) -> map<String, pair<String, String>> { xs.insert(move(x)); return xs; }),
        };
        tokInfo.PrioritySymbols.reserve(tokInfo.Symbol2EnumNameRegExp.size());
        for (auto const& x : priority2Symbols)
        {
            if (tokInfo.Symbol2EnumNameRegExp.contains(x.second))
            {
                tokInfo.PrioritySymbols.push_back(x.second);
            }
        }

        return tokInfo;
    }

    static auto Transform(Item* item, map<String, string> const& convertedRegExps) -> string
    {
        ItemTransfomer v{ convertedRegExps };
        item->Visit(&v);
        return v.Result;
    }

    static auto Transform(Productions const* productions, map<String, string> const& convertedRegExps) -> string
    {
        string regExp;
        for (auto first = true; auto const& p : productions->Items)
        {
            if (first)
            {
                first = false;
                regExp.append(format("{}", Transform(p.get(), convertedRegExps)));
            }
            else // option operator | is lowest priority operator, so we remove the parentheses which is used to assure order before
            {
                regExp.append(format("|{}", Transform(p.get(), convertedRegExps)));
            }
        }
        return regExp;
    }

    static auto Transform(Production const* production, map<String, string> const& convertedRegExps) -> string
    {
        string regExp;
        for (auto const& item : production->Items)
        {
            regExp.append(format("{}", Transform(item.get(), convertedRegExps)));
        }
        return regExp;
    }

    // TODO return value should be vector to control the print order which means priority

    /// <returns>(symbol name, (enum name, regular expression))</returns>
    static auto MergeTokInfo(TokensInfo tokInfoFromLexRules, TokensInfo tokInfoFromParseRules) -> TokensInfo
    {
        tokInfoFromParseRules.PrioritySymbols.reserve(tokInfoFromLexRules.PrioritySymbols.size() + tokInfoFromParseRules.PrioritySymbols.size());
        tokInfoFromParseRules.PrioritySymbols.append_range(move(tokInfoFromLexRules.PrioritySymbols));
        tokInfoFromParseRules.Symbol2EnumNameRegExp.merge(move(tokInfoFromLexRules.Symbol2EnumNameRegExp));
        return tokInfoFromParseRules;
    }
};

export
{
    class ParseRule2SimpleGrammarTransformer;
    struct LexRule2RegExpTransformer;
    template<>
    struct formatter<CppCodeForm<TokensInfo>, char>;
}