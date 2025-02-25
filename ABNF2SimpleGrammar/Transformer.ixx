export module Transformer;

import std;
import Base;
import Parser;
import Ast;

using std::vector;
using std::string;
using std::set;
using std::pair;
using std::format;

// if name is occupied, append number which
class GrammarTransformer
{
public:
    struct SimpleGrammarsInfo
    {
        vector<SimpleGrammar> Grammars;
        set<String> Terminals;
    };
    struct GrammarTransformInfo
    {
        String Left;
        vector<SimpleRightSide> MainRights;
        vector<SimpleGrammar> OtherGrammars;
        vector<String> Terminals; // terminal also has priority, exact match is higher than others
        int& Counter;

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
            GrammarTransformInfo subInfo{ .Left = basicItemGrammarName, .Counter = info->Counter };
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
            GrammarTransformInfo subInfo{ .Left = auxGrammarName, .Counter = info->Counter };
            GrammarTransformer::Transform(combine->Productions.get(), &subInfo);

            info->OtherGrammars.push_back({ subInfo.Left, move(subInfo.MainRights) });
            info->OtherGrammars.append_range(move(subInfo.OtherGrammars));
            info->AppendOnLastRule(auxGrammarName);
        }

        auto Transform(Optional const* optional, GrammarTransformInfo* info) -> void
        {
            String auxGrammarName{ format("{}_op_{}", info->Left, info->Counter++) };
            GrammarTransformInfo subInfo{ .Left = auxGrammarName, .Counter = info->Counter };
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
            info->AppendOnLastRule(String(format("terminal_{}", info->Counter++)));
            info->Terminals.push_back(terminal->Value);
        }

        auto Transform(RegExp const* regExp, GrammarTransformInfo* info) -> void
        {
            info->AppendOnLastRule(String(format("terminal_{}", info->Counter++)));
            info->Terminals.push_back(regExp->Value);
        }
    };

    /// <param name="info">caller provide the info with counter set, other properties not set</param>
    static auto Transform(Grammar const* grammar, GrammarTransformInfo* info) -> SimpleGrammarsInfo
    {
        info->Left = grammar->Left;
        Transform(grammar->Productions.get(), info);
        // TODO handle terminal
        SimpleGrammarsInfo grammarsInfo{ .Grammars = pair{ info->Left, move(info->MainRights) } };
        grammarsInfo.Grammars.append_range(move(info->OtherGrammars));
        grammarsInfo.Terminals.insert_range(move(info->Terminals));
        return grammarsInfo;
    }

    static auto Transform(Grammars const* grammars) -> SimpleGrammarsInfo
    {
        SimpleGrammarsInfo grammarsInfo;
        auto counter = 0;
        GrammarTransformInfo subInfo{ .Counter = counter };
        for (auto const& g : grammars->Items)
        {
            auto r = Transform(g.get(), &subInfo);
            grammarsInfo.Grammars.append_range(move(r.Grammars));
            grammarsInfo.Terminals.insert_range(move(r.Terminals));
            subInfo.Left = String();
            subInfo.MainRights.clear();
            subInfo.OtherGrammars.clear();
            subInfo.Terminals.clear();
        }
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
            GrammarTransformInfo subInfo{ .Left = info->Left, .Counter = info->Counter };
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