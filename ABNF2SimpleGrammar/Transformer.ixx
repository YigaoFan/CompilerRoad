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
    };
    auto Transform(Grammar const* grammar) -> vector<SimpleGrammar>
    {
        GrammarTransformInfo info{ .Left = grammar->Left };
        Transform(grammar->Productions.get(), &info);
        // TODO handle terminal
        vector<SimpleGrammar> result{ { info.Left, move(info.MainRights) } };
        result.append_range(move(info.OtherGrammars));
        return result;
    }

    auto Transform(Grammars const* grammars) -> vector<SimpleGrammar>
    {
        vector<SimpleGrammar> result;
        for (auto const& g : grammars->Items)
        {
            result.append_range(Transform(g.get()));
        }
        return result;
    }

    auto Transform(Duplicate const* duplicate, GrammarTransformInfo* info) -> void
    {
        String basicItemGrammarName{ format("{}_dup_basicItem_{}", info->Left, info->Counter++) };
        GrammarTransformInfo subInfo{ .Left = basicItemGrammarName, };
        Transform(duplicate->BasicItem.get(), &subInfo);

        String auxGrammarName{ format("{}_dup_{}", info->Left, info->Counter++) };
        vector<SimpleRightSide> rss;
        // if max is int_max, use left recursive
        if (duplicate->High == std::numeric_limits<unsigned>::max())
        {
            rss.push_back({ auxGrammarName });
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
        info->MainRights.back().push_back(auxGrammarName);
    }

    auto Transform(Combine const* combine, GrammarTransformInfo* info) -> void
    {
        String auxGrammarName{ format("{}_com_{}", info->Left, info->Counter++) };
        GrammarTransformInfo subInfo{ .Left = auxGrammarName, };
        Transform(combine->Production.get(), &subInfo);

        info->OtherGrammars.push_back({ subInfo.Left, move(subInfo.MainRights) });
        info->OtherGrammars.append_range(move(subInfo.OtherGrammars));
        info->MainRights.back().push_back(auxGrammarName);
    }

    auto Transform(Optional const* optional, GrammarTransformInfo* info) -> void
    {
        String auxGrammarName{ format("{}_op_{}", info->Left, info->Counter++) };
        GrammarTransformInfo subInfo{ .Left = auxGrammarName, .MainRights = {{}} };
        Transform(optional->Productions.get(), &subInfo);

        info->OtherGrammars.push_back({ subInfo.Left, move(subInfo.MainRights) });
        info->OtherGrammars.append_range(move(subInfo.OtherGrammars));
        info->MainRights.back().push_back(auxGrammarName);
    }

    auto Transform(DataRange const* dataRange, GrammarTransformInfo* info) -> void
    {
        String auxGrammarName{ format("{}_dr_{}", info->Left, info->Counter++) };
        vector<SimpleRightSide> rss;
        for (auto i = dataRange->Left; i <= dataRange->Right; ++i)
        {
            String s{ static_cast<char>(i) };
            info->Terminals.push_back(s);
            rss.push_back({ s });
        }
        info->OtherGrammars.push_back({ auxGrammarName, move(rss) });
        info->MainRights.back().push_back(auxGrammarName);
    }

    auto Transform(Symbol const* symbol, GrammarTransformInfo* info) -> void
    {
        info->MainRights.back().push_back(symbol->Value);
    }

    auto Transform(Terminal const* terminal, GrammarTransformInfo* info) -> void
    {
        // TODO remove quote
        info->Terminals.push_back(terminal->Value);
    }

    auto Transform(BasicItem const* basicItem, GrammarTransformInfo* info) -> void
    {

    }

    auto Transform(Productions const* productions, GrammarTransformInfo* info) -> void
    {

    }

    auto Transform(Production const* production, GrammarTransformInfo* info) -> void
    {

    }
};