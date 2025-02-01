export module Transformer;

import std;
import Base;
import Parser;
import Ast;

using std::vector;
using std::string;
using std::format;

class GrammarTransformer
{
public:
    struct GrammarTransformInfo
    {
        String Left;
        vector<SimpleRightSide> MainRights;
        vector<SimpleGrammar> OtherGrammars;
        vector<String> Terminals;
        int Counter = 0;

        auto AppendOnLastRule(String symbol) -> void
        {
            if (MainRights.empty())
            {
                MainRights.push_back({});
            }
            MainRights.back().push_back(symbol);
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

        auto Transform(Duplicate const* duplicate, GrammarTransformInfo* info) -> void
        {
            String basicItemGrammarName{ format("{}_dup_basicItem_{}", info->Left, info->Counter++) };
            GrammarTransformInfo subInfo{ .Left = basicItemGrammarName, };
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
            info->OtherGrammars.append_range(move(subInfo.OtherGrammars));
            info->AppendOnLastRule(auxGrammarName);
        }

        auto Transform(Combine const* combine, GrammarTransformInfo* info) -> void
        {
            String auxGrammarName{ format("{}_com_{}", info->Left, info->Counter++) };
            GrammarTransformInfo subInfo{ .Left = auxGrammarName, };
            GrammarTransformer::Transform(combine->Production.get(), &subInfo);

            info->OtherGrammars.push_back({ subInfo.Left, move(subInfo.MainRights) });
            info->OtherGrammars.append_range(move(subInfo.OtherGrammars));
            info->AppendOnLastRule(auxGrammarName);
        }

        auto Transform(Optional const* optional, GrammarTransformInfo* info) -> void
        {
            String auxGrammarName{ format("{}_op_{}", info->Left, info->Counter++) };
            GrammarTransformInfo subInfo{ .Left = auxGrammarName, };
            GrammarTransformer::Transform(optional->Productions.get(), &subInfo);
            subInfo.MainRights.push_back({}); // make it optional

            info->OtherGrammars.push_back({ subInfo.Left, move(subInfo.MainRights) });
            info->OtherGrammars.append_range(move(subInfo.OtherGrammars));
            info->AppendOnLastRule(auxGrammarName);
        }

        auto Transform(DataRange const* dataRange, GrammarTransformInfo* info) -> void
        {
            String auxGrammarName{ format("{}_dr_{}", info->Left, info->Counter++) };
            vector<SimpleRightSide> rss;
            for (auto i = dataRange->Left; i <= dataRange->Right; ++i)
            {
                String s{ static_cast<char>(i) };
                info->AppendOnLastRule(s);
                info->Terminals.push_back(s);
                rss.push_back({ s });
            }
            info->OtherGrammars.push_back({ auxGrammarName, move(rss) });
            info->AppendOnLastRule(auxGrammarName);
        }

        auto Transform(Symbol const* symbol, GrammarTransformInfo* info) -> void
        {
            info->AppendOnLastRule(symbol->Value);
        }

        auto Transform(Terminal const* terminal, GrammarTransformInfo* info) -> void
        {
            // TODO remove quote
            info->AppendOnLastRule(terminal->Value);
            info->Terminals.push_back(terminal->Value);
        }
    };

    static auto Transform(Grammar const* grammar) -> vector<SimpleGrammar>
    {
        GrammarTransformInfo info{ .Left = grammar->Left };
        Transform(grammar->Productions.get(), &info);
        // TODO handle terminal
        vector<SimpleGrammar> result{ { info.Left, move(info.MainRights) } };
        result.append_range(move(info.OtherGrammars));
        return result;
    }

    static auto Transform(Grammars const* grammars) -> vector<SimpleGrammar>
    {
        vector<SimpleGrammar> result;
        for (auto const& g : grammars->Items)
        {
            result.append_range(Transform(g.get()));
        }
        return result;
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
            GrammarTransformInfo subInfo{ .Left = info->Left, };
            Transform(p.get(), &subInfo);
            info->MainRights.append_range(move(subInfo.MainRights));
            info->OtherGrammars.append_range(move(subInfo.OtherGrammars));
            info->Terminals.append_range(move(subInfo.Terminals));
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