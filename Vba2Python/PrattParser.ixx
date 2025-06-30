export module PrattParser;

import std;
import Base;
import Parser;
import VbaSpec;

using std::pair;
using std::set;
using std::map;
using std::unexpected;
using std::array;
using std::optional;
using std::move;
using std::format;

template <IToken Tok, typename Result, size_t Size>
auto Cons(Tok op, array<SyntaxTreeNode<Tok, Result>, Size> items) -> SyntaxTreeNode<Tok, Result>
{
    auto n = SyntaxTreeNode<Tok, Result>(String(format("{}-expression", op.Value)), { "op", "lhs", "rhs" }); // TODO
    n.Children.push_back(move(op));

    for (auto& x : items)
    {
        n.Children.push_back(move(x));
    }
    return n;
}

template <IToken Tok, typename Result>
auto ParseUnit(Stream<Tok> auto& stream) -> ParserResult<SyntaxTreeNode<Tok, Result>>
{
    auto item = stream.Current();

    switch (item.Type)
    {
    // atom type
    case TokType::Integer:
    case TokType::Float:
    case TokType::DateOrTime:
    case TokType::NormalString:
    //case TokType::QuotedIdentifier:
    case TokType::Identifier:
    case TokType::Terminal209: // 0
    {
        auto n = SyntaxTreeNode<Tok, Result>("atom", { "atom" }); // TODO
        n.Children.push_back(move(item));
        return move(n);
    }
    case TokType::Terminal185: //-
    {
        auto [_, rbp] = PrefixBindingPower(item.Type);
        auto rhs = ParseExp<Tok, Result>(stream, rbp);
        if (not rhs.has_value())
        {
            return rhs;
        }
        return Cons(item, array{ move(rhs.value()) });
    }
    case TokType::Terminal186: //(
    {
        auto exp = ParseExp<Tok, Result>(stream, 0);
        if (not exp.has_value())
        {
            return exp;
        }
        stream.MoveNext();
        auto next = stream.Current();
        Assert(next.Type == TokType::Terminal151, "right parentheses not matched");
        return exp;
    }
    default:
        return unexpected(ParseFailResult{ .Message = format("token type ({}) isn't allowed atom in expression", item.Type) });
    }
}

auto InfixBindingPower(TokType op) -> pair<int, int>
{
    switch (op)
    {
    case TokType::Terminal220:
    case TokType::Terminal185:
        return { 1, 2 };
    case TokType::Terminal188:
    case TokType::Terminal221:
        return { 3, 4 };
    case TokType::Terminal198:
        return { 8, 7 };
    default:
        throw std::out_of_range(format("unknown operator({}) when get the binding power", op));
    }
}

auto PrefixBindingPower(TokType op) -> pair<optional<int>, int>
{
    switch (op)
    {
    case TokType::Terminal185:
        return { {}, 5 };
    default:
        throw std::out_of_range(format("unknown operator({}) when get the binding power", op));
    }
}

//auto PostfixBindingPower(TokType op) -> optional<pair<int, int>>
//{
//}

template <IToken Tok, typename Result>
auto ParseExp(Stream<Tok> auto& stream, int minBindingPower) -> ParserResult<SyntaxTreeNode<Tok, Result>>
{
    auto lhs = ParseUnit<Tok, Result>(stream);
    if (not lhs.has_value())
    {
        return lhs;
    }

    for (;;)
    {
        auto op = stream.Current();
        switch (op.Type)
        {
        // below is operator
        case TokType::Terminal220: //+
        case TokType::Terminal185: //-
        case TokType::Terminal188: //*
        case TokType::Terminal221: // /
        case TokType::Terminal198: //.
        {
            auto [lbp, rbp] = InfixBindingPower(op.Type);
            if (lbp < minBindingPower)
            {
                // due to not handle not handle this op, roll back the stream here
                //stream.Rollback(); // TODO need it?
                return lhs;
            }
            auto rhs = ParseExp<Tok, Result>(stream, rbp);
            if (not rhs.has_value())
            {
                return rhs;
            }
            lhs = Cons(op, array{ move(lhs.value()), move(rhs.value()) });
            break;
        }
        case TokType::EOF:
        default:
            //stream.Rollback(); // reserve for outer to handle or trigger exit
            return lhs;
        }
    }
}

export
{
    template <IToken Tok, typename Result>
    auto ParseExp(Stream<Tok> auto& stream, int minBindingPower) -> ParserResult<SyntaxTreeNode<Tok, Result>>;
}