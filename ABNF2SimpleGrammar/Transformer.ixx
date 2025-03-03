export module Transformer;

import std;
import Base;
import Parser;
import Ast;

using std::vector;
using std::string;
using std::set;
using std::pair;
using std::string_view;
using std::map;
using std::format;
using std::move;
using std::println;

auto AppendCppStringLiteralSlash(string& str) -> void
{
    str.append("\\\\");
}

// sub String assign back to original String variable, will have issue
auto Escape2RegExp(String normalString) -> String
{
    string regExp = "\"";
    regExp.reserve(normalString.Length());

    string_view operators = "*+|-[^]()";
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
            if (operators.contains(c))
            {
                AppendCppStringLiteralSlash(regExp);
            }
            regExp.push_back(c);
            break;
        }
    }
    regExp.push_back('"');
    return String(regExp);
}

auto Escape2StringLiteral(String regExpInAbnf) -> String
{
    string regExp = "\"";
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
                string_view operators = "*+|-[^]()";
                switch (auto nextCh = s[i]; nextCh)
                {
                case '\\':
                    AppendCppStringLiteralSlash(regExp);
                    AppendCppStringLiteralSlash(regExp);
                    break;
                default:
                    if (operators.contains(nextCh))
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
    regExp.push_back('"');
    return String(regExp);
}

template <typename T>
struct DefaultVisitor : IVisitor
{
    T Result;
    DefaultVisitor() : Result()
    { }

    auto Visit(Terminal* object) -> void override
    {
    }

    auto Visit(RegExp* object) -> void override
    {
    }

    auto Visit(Symbol* object) -> void override
    {
    }

    auto Visit(DataRange* object) -> void override
    {
    }

    auto Visit(Optional* object) -> void override
    {
    }

    auto Visit(Combine* object) -> void override
    {
    }

    auto Visit(Duplicate* object) -> void override
    {
    }

    auto Visit(Grammar*) -> void override
    {
    }

    auto Visit(Grammars*) -> void override
    {
    }

    auto Visit(Productions*) -> void override
    {
    }

    auto Visit(Production*) -> void override
    {
    }
};

template <>
struct DefaultVisitor<void> : IVisitor
{
    DefaultVisitor()
    {
    }

    auto Visit(Terminal* object) -> void override
    {
    }

    auto Visit(RegExp* object) -> void override
    {
    }

    auto Visit(Symbol* object) -> void override
    {
    }

    auto Visit(DataRange* object) -> void override
    {
    }

    auto Visit(Optional* object) -> void override
    {
    }

    auto Visit(Combine* object) -> void override
    {
    }

    auto Visit(Duplicate* object) -> void override
    {
    }

    auto Visit(Grammar*) -> void override
    {
    }

    auto Visit(Grammars*) -> void override
    {
    }

    auto Visit(Productions*) -> void override
    {
    }

    auto Visit(Production*) -> void override
    {
    }
};

// if name is occupied, append number which
class GrammarTransformer
{
public:
    struct SimpleGrammarsInfo
    {
        vector<SimpleGrammar> Grammars;
        map<String, int> Terminals;
    };
    struct GrammarTransformInfo
    {
        String Left;
        vector<SimpleRightSide> MainRights;
        // use below 3 items to share some grammars construct, which should not be clear in low level, only allow to add
        vector<SimpleGrammar>& OtherGrammars;
        map<String, int>& Terminals; // terminal also has priority, exact match is higher than others
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
            if (Terminals.contains(terminalValue))
            {
                return String(format("terminal_{}", Terminals.at(terminalValue)));
            }
            auto i = Terminals.size();
            auto [it, _] = Terminals.insert({ move(terminalValue), i });
            return String(format("terminal_{}", i));
        }

        auto CreateSubInfo(String left = {}) const -> GrammarTransformInfo
        {
            return GrammarTransformInfo{ .Left = move(left), .OtherGrammars = OtherGrammars, .Terminals = Terminals, .Counter = Counter };
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
            GrammarTransformer::Transform(duplicate->BasicItem.get(), &subInfo);

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
            GrammarTransformer::Transform(combine->Productions.get(), &subInfo);

            info->OtherGrammars.push_back({ subInfo.Left, move(subInfo.MainRights) });
            info->AppendOnLastRule(auxGrammarName);
        }

        auto Transform(Optional const* optional, GrammarTransformInfo* info) -> void
        {
            String auxGrammarName{ format("{}_op_{}", info->Left, info->Counter++) };
            GrammarTransformInfo subInfo = info->CreateSubInfo(auxGrammarName);
            GrammarTransformer::Transform(optional->Productions.get(), &subInfo);
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
            auto name = info->RegisterTerminalName(Escape2RegExp(terminal->Value));
            info->AppendOnLastRule(name);
        }

        auto Transform(RegExp const* regExp, GrammarTransformInfo* info) -> void
        {
            auto name = info->RegisterTerminalName(Escape2StringLiteral(regExp->Value));
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
        map<String, int> terminals;
        vector<SimpleGrammar> otherGrammars;

        GrammarTransformInfo rootInfo{ .OtherGrammars = otherGrammars, .Terminals = terminals, .Counter = counter, };
        for (auto const& g : grammars->Items)
        {
            auto sg = Transform(g.get(), &rootInfo);
            grammarsInfo.Grammars.push_back(move(sg));
            rootInfo.ResetCurrentGrammarInfo();
        }
        grammarsInfo.Terminals.insert_range(move(terminals));
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
            Result = string(object->Value.Substring(1, object->Value.Length() - 2));
        }

        auto Visit(RegExp* object) -> void override
        {
            Result = string(object->Value.Substring(2, object->Value.Length() - 3));
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
            Result = format("({})|[]", Transform(object->Productions.get(), OtherSymbolRegExps)); // is it ok to write empty [] here TODO check
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
                Result.append(format("({})", Dup(object->Low)));
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

    static auto Transform(Grammars const* grammars) -> map<String, string>
    {
        using std::tuple;
        using std::queue;
        using std::get;
        using std::ranges::views::reverse;
        using std::ranges::views::filter;
        using std::ranges::to;

        auto ReferingIn = [](Grammar* grammar) -> set<String>
        {
            SymbolCollector collector{ grammar->Left };
            grammar->Visit(&collector);
            //println("using symbol collect: {}", collector.Result);
            return collector.Result;
        };

        map<String, tuple<set<String>, int, Grammar const*>> refInfo;
        auto InitIfNotExist = [&refInfo](String symbolName) -> tuple<set<String>, int, Grammar const*>&
        {
            if (not refInfo.contains(symbolName))
            {
                refInfo.insert({ symbolName, { {}, 0, nullptr } });
            }
            return refInfo.at(symbolName);
        };
        for (auto const& g : grammars->Items)
        {
            //println("analyse grammar: {}", g->Left);
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
        if (debug)
        {
            return regExps;
        }
        // only reserve the capitalized first char one which is thought as final token type
        return regExps | filter([](pair<String, string> const& x) -> bool { return std::isupper(x.first[0]); }) | to<map<String, string>>();
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
};

export
{
    class GrammarTransformer;
    struct LexRule2RegExpTransformer;
}