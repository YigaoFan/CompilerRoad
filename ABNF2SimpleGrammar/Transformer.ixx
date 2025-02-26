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

// if name is occupied, append number which
class GrammarTransformer
{
public:
    struct SimpleGrammarsInfo
    {
        vector<SimpleGrammar> Grammars;
        map<String, String> Terminals;
    };
    struct GrammarTransformInfo
    {
        String Left;
        vector<SimpleRightSide> MainRights;
        // use below 3 items to share some grammars construct, which should not be clear in low level, only allow to add
        vector<SimpleGrammar>& OtherGrammars;
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
            if (Terminals.contains(terminalValue))
            {
                return Terminals.at(terminalValue);
            }
            auto [it, _] = Terminals.insert({ move(terminalValue), String(format("terminal_{}", Terminals.size())) });
            return it->second;
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
    struct TransformVisitor : IVisitor
    {
        GrammarTransformInfo* Info;
        TransformVisitor(GrammarTransformInfo* info) : Info(info)
        { }

        virtual auto Visit(Terminal* object) -> void
        {
            Transform(object, Info);
        }

        virtual auto Visit(RegExp* object) -> void
        {
            Transform(object, Info);
        }

        virtual auto Visit(Symbol* object) -> void
        {
            Transform(object, Info);
        }

        virtual auto Visit(DataRange* object) -> void
        {
            Transform(object, Info);
        }

        virtual auto Visit(Optional* object) -> void
        {
            Transform(object, Info);
        }

        virtual auto Visit(Combine* object) -> void
        {
            Transform(object, Info);
        }

        virtual auto Visit(Duplicate* object) -> void
        {
            Transform(object, Info);
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
        map<String, String> terminals;
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

export
{
    class GrammarTransformer;
}