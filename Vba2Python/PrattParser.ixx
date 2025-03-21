export module PrattParser;

import std;
import Base;
import Parser;
import VbaSpec;

using std::pair;
using std::set;
using std::map;
using std::unexpected;
using std::move;
using std::format;

template <IToken Tok, typename Result>
auto Cons(Tok op, SyntaxTreeNode<Tok, Result> lhs, SyntaxTreeNode<Tok, Result> rhs) -> SyntaxTreeNode<Tok, Result>
{
    auto n = SyntaxTreeNode<Tok, Result>(String(format("{}-expression", op.Value)), { "op", "lhs", "rhs" }); // TODO
    n.Children.push_back(move(op));
    n.Children.push_back(move(lhs));
    n.Children.push_back(move(rhs));
    return n;
}

template <IToken Tok, typename Result>
auto ParseUnit(Stream<Tok> auto& stream) -> ParserResult<SyntaxTreeNode<Tok, Result>>
{
    auto item = stream.NextItem();

    switch (item.Type)
    {
    // atom type
    case TokType::Integer:
    case TokType::Float:
    case TokType::DateOrTime:
    case TokType::String:
    case TokType::Identifier:
    {
        auto n = SyntaxTreeNode<Tok, Result>("atom", { "atom" }); // TODO
        n.Children.push_back(move(item));
        return move(n);
    }
    default:
        return unexpected(ParseFailResult{ .Message = format("token type ({}) isn't allowed atom in expression", item.Type) });
    }
}

auto InfixBindingPower(TokType op) -> pair<int, int>
{
    switch (op)
    {
    case TokType::Terminal155:
    case TokType::Terminal18:
        return { 1, 2 };
    case TokType::Terminal43:
    case TokType::Terminal156:
        return { 3, 4 };
    case TokType::Terminal78:
        return { 6, 5 };
    default:
        throw std::out_of_range(format("unknown operator({}) when get the binding power", op));
    }
}

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
        auto op = stream.NextItem();
        switch (op.Type)
        {
        case TokType::EOF:
            stream.Rollback(); // reserve EOF for out recursive call to exit
            return lhs;
        // below is operator
        case TokType::Terminal18:
        case TokType::Terminal155:
        case TokType::Terminal43:
        case TokType::Terminal156:
        case TokType::Terminal78:
        {
            auto [lbp, rbp] = InfixBindingPower(op.Type);
            if (lbp < minBindingPower)
            {
                // due to not handle not handle this op, roll back the stream here
                stream.Rollback();
                return lhs;
            }
            auto rhs = ParseExp<Tok, Result>(stream, rbp);
            if (not rhs.has_value())
            {
                return rhs;
            }
            lhs = Cons(op, move(lhs.value()), move(rhs.value()));
            break;
        }
        default:
            return unexpected(ParseFailResult{ .Message = format("unhandled operator({})", op) });
        }
    }
}

// para should be TokType
//auto PrefixBindingPower(char op) -> pair<int, int>
//{
//
//}

//auto PostfixBindingPower(char op) -> pair<int, int>
//{
//}

export
{
    template <IToken Tok, typename Result>
    auto ParseExp(Stream<Tok> auto& stream, int minBindingPower) -> ParserResult<SyntaxTreeNode<Tok, Result>>;
}