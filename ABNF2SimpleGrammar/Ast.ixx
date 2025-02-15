export module Ast;

import std;
import Base;
import Lexer;
import Parser;
import TokType;

using std::vector;
using std::index_sequence;
using std::shared_ptr;
using std::string_view;
using std::logic_error;
using std::move;
using std::make_shared;
using std::format;
using std::ranges::views::transform;
using std::dynamic_pointer_cast;
using std::make_shared;


template <size_t N>
struct StringLiteral
{
    char p[N]{};

    constexpr StringLiteral(char const(&pp)[N])
    {
        std::ranges::copy(pp, p);
    }

    auto operator== (String const& s) const -> bool
    {
        return s == string_view(p);
    }
};

template <typename T0, typename T1>
struct Cons
{
    using Head = T0;
    using Tail = T1;
};

template <StringLiteral Key, typename T>
struct Pair
{
    static constexpr auto Key = Key;
    using Value = T;
};

template <typename...>
struct List
{ };

template <typename T, typename... Ts>
struct List<T, Ts...> : Cons<T, List<Ts...>>
{

};

struct Nil
{
};

template <>
struct List<> : public Nil
{

};

template <typename>
struct ArgsOf;

template <typename Return, typename Cls, typename... Args>
class ArgsOf<Return(Cls::*)(Args...)>
{
    using Result = List<Args...>;
};

struct IVisitor;
struct AstNode
{
    virtual auto Visit(IVisitor* visitor) -> void = 0; // maybe delete in the future
    virtual ~AstNode() = default;
};

template <typename T>
auto GetResultOfAstChildAs(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, int i) -> shared_ptr<T>
{
    return dynamic_pointer_cast<T>(std::get<1>(node->Children[i]).Result);
}

auto GetResultOfTokChildAs(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, int i) -> Token<TokType>
{
    return std::get<0>(node->Children[i]);
}

struct Grammar;
struct Grammars;
struct Duplicate;
struct Combine;
struct Optional;
struct DataRange;
struct Symbol;
struct Terminal;
struct RegExp;
struct Productions;
struct Production;
struct IVisitor
{
    // not support Visit Item and BasicItem now
    virtual auto Visit(Grammar*) -> void = 0;
    virtual auto Visit(Grammars*) -> void = 0;
    virtual auto Visit(Duplicate*) -> void = 0;
    virtual auto Visit(Combine*) -> void = 0;
    virtual auto Visit(Optional*) -> void = 0;
    virtual auto Visit(DataRange*) -> void = 0;
    virtual auto Visit(Symbol*) -> void = 0;
    virtual auto Visit(Terminal*) -> void = 0;
    virtual auto Visit(RegExp*) -> void = 0;
    virtual auto Visit(Productions*) -> void = 0;
    virtual auto Visit(Production*) -> void = 0;
};

template <typename T>
auto ApplyVisitor(shared_ptr<T> tPtr, IVisitor* visitor) -> shared_ptr<T>
{
    visitor->Visit(tPtr.get());
    return tPtr;
}

struct Item : public AstNode
{
    static auto Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, IVisitor* visitor) -> shared_ptr<Item>;
};

struct MoreItems : public AstNode
{
    vector<shared_ptr<Item>> Items;

    static auto Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, IVisitor*) -> shared_ptr<MoreItems>
    {
        switch (node->ChildSymbols.size())
        {
        case 0:
            return make_shared<MoreItems>(vector<shared_ptr<Item>>{});
        case 2:
        {
            vector is{ GetResultOfAstChildAs<Item>(node, 0) };
            is.append_range(GetResultOfAstChildAs<MoreItems>(node, 1)->Items);
            return make_shared<MoreItems>(move(is));
        }
        }
    }

    MoreItems(vector<shared_ptr<Item>> items) : Items(move(items))
    { }

    auto Visit(IVisitor* visitor) -> void override
    {
    }
};

struct Production : public AstNode
{
    vector<shared_ptr<Item>> Items;

    static auto Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, IVisitor* visitor) -> shared_ptr<Production>
    {
        switch (node->ChildSymbols.size())
        {
        case 0:
            return ApplyVisitor(make_shared<Production>(vector<shared_ptr<Item>>{}), visitor);
        case 2:
        {
            vector is{ GetResultOfAstChildAs<Item>(node, 0) };
            is.append_range(GetResultOfAstChildAs<MoreItems>(node, 1)->Items);
            return ApplyVisitor(make_shared<Production>(move(is)), visitor);
        }
        }
    }

    Production(vector<shared_ptr<Item>> items) : Items(move(items))
    { }

    auto Visit(IVisitor* visitor) -> void override
    {
        visitor->Visit(this);
    }
};

struct Productions;
struct MoreProductions : public AstNode
{
    /// <summary>
    /// Attention: it's possible nullptr
    /// </summary>
    shared_ptr<Productions> Productions;

    static auto Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, IVisitor*) -> shared_ptr<MoreProductions>
    {
        switch (node->ChildSymbols.size())
        {
        case 0:
            return make_shared<MoreProductions>(nullptr);
        case 2:
            return make_shared<MoreProductions>(GetResultOfAstChildAs<::Productions>(node, 1));
        }
    }

    MoreProductions(shared_ptr<::Productions> productions) : Productions(move(productions))
    {
    }

    auto Visit(IVisitor* visitor) -> void override
    {
    }
};

struct Productions : public AstNode
{
    vector<shared_ptr<Production>> Items;

    static auto Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, IVisitor* visitor) -> shared_ptr<Productions>
    {
        switch (node->ChildSymbols.size())
        {
        case 0:
            return ApplyVisitor(make_shared<Productions>(vector<shared_ptr<Production>>{}), visitor);
        case 2:
        {
            vector ps{ GetResultOfAstChildAs<Production>(node, 0) };
            if (auto mps = GetResultOfAstChildAs<MoreProductions>(node, 1)->Productions; mps != nullptr)
            {
                ps.append_range(mps->Items);
            }
            return ApplyVisitor(make_shared<Productions>(move(ps)), visitor);
        }
        }
    }

    Productions(vector<shared_ptr<Production>> items) : Items(move(items))
    { }

    auto Visit(IVisitor* visitor) -> void override
    {
        visitor->Visit(this);
    }
};

struct BasicItem : public Item
{
    static auto Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, IVisitor* visitor) -> shared_ptr<BasicItem>;
};

struct Terminal : public BasicItem
{
    String Value;
    Terminal(String value) : Value(move(value))
    { }

    auto Visit(IVisitor* visitor) -> void override
    {
        visitor->Visit(this);
    }
};

struct RegExp : public BasicItem
{
    String Value;
    RegExp(String value) : Value(move(value))
    {
    }

    auto Visit(IVisitor* visitor) -> void override
    {
        visitor->Visit(this);
    }
};

struct Symbol : public BasicItem
{
    String Value;
    Symbol(String value) : Value(move(value))
    { }

    auto Visit(IVisitor* visitor) -> void override
    {
        visitor->Visit(this);
    }
};

struct DataRange : public BasicItem
{
    int Left;
    int Right;
    DataRange(int left, int right) : Left(left), Right(right)
    { }

    auto Visit(IVisitor* visitor) -> void override
    {
        visitor->Visit(this);
    }
};

struct Optional : public BasicItem
{
    shared_ptr<Productions> Productions;
    Optional(shared_ptr<::Productions> productions) : Productions(move(productions))
    { }

    auto Visit(IVisitor* visitor) -> void override
    {
        visitor->Visit(this);
    }
};

struct Combine : public BasicItem
{
    shared_ptr<Productions> Productions;
    Combine(shared_ptr<::Productions> productions) : Productions(move(productions))
    { }

    auto Visit(IVisitor* visitor) -> void override
    {
        visitor->Visit(this);
    }
};

struct Duplicate : public Item
{
    unsigned Low;
    unsigned High;
    shared_ptr<BasicItem> BasicItem;

    Duplicate(unsigned low, unsigned high, shared_ptr<::BasicItem> basicItem)
        : Low(low), High(high), BasicItem(move(basicItem))
    { }

    auto Visit(IVisitor* visitor) -> void override
    {
        visitor->Visit(this);
    }
};

auto Item::Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, IVisitor* visitor) -> shared_ptr<Item>
{
    switch (node->ChildSymbols.size())
    {
    case 1:
        return GetResultOfAstChildAs<BasicItem>(node, 0); // not invoke now
    case 2:
        return ApplyVisitor(make_shared<Duplicate>(0, std::numeric_limits<unsigned>::max(), GetResultOfAstChildAs<BasicItem>(node, 1)), visitor);
    case 3:
        return ApplyVisitor(make_shared<Duplicate>(std::stoul(GetResultOfTokChildAs(node, 0).Value), std::numeric_limits<unsigned>::max(), GetResultOfAstChildAs<BasicItem>(node, 2)), visitor);
    case 4:
        return ApplyVisitor(make_shared<Duplicate>(std::stoul(GetResultOfTokChildAs(node, 0).Value), std::stoul(GetResultOfTokChildAs(node, 2).Value), GetResultOfAstChildAs<BasicItem>(node, 3)), visitor);
    default:
        throw logic_error(format("not handled BasicItem Item symbols {}", node->ChildSymbols));
    }
}

auto BasicItem::Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, IVisitor* visitor) -> shared_ptr<BasicItem>
{
    switch (node->ChildSymbols.size())
    {
    case 1:
        if (node->ChildSymbols.front() == "terminal")
        {
            return ApplyVisitor(make_shared<Terminal>(String(GetResultOfTokChildAs(node, 0).Value)), visitor);
        }
        else if (node->ChildSymbols.front() == "sym")
        {
            return ApplyVisitor(make_shared<Symbol>(String(GetResultOfTokChildAs(node, 0).Value)), visitor);
        }
        else if (node->ChildSymbols.front() == "regExp")
        {
            return ApplyVisitor(make_shared<RegExp>(String(GetResultOfTokChildAs(node, 0).Value)), visitor);
        }
        break;
    case 3:
    {
        if (node->ChildSymbols.front() == "digitOrAlphabet")
        {
            return ApplyVisitor(make_shared<DataRange>(GetResultOfTokChildAs(node, 0).Value.front(), GetResultOfTokChildAs(node, 2).Value.front()), visitor);
        }
        else if (node->ChildSymbols.front() == "[")
        {
            return ApplyVisitor(make_shared<Optional>(GetResultOfAstChildAs<Productions>(node, 1)), visitor);
        }
        else if (node->ChildSymbols.front() == "(")
        {
            return ApplyVisitor(make_shared<Combine>(GetResultOfAstChildAs<Productions>(node, 1)), visitor);
        }
        break;
    }
    }
    throw logic_error(format("not handled BasicItem child symbols {}", node->ChildSymbols));
}

struct Grammar : public AstNode
{
    String Left;
    shared_ptr<Productions> Productions;

    static auto Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, IVisitor* visitor) -> shared_ptr<Grammar>
    {
        return ApplyVisitor(make_shared<Grammar>(String(GetResultOfTokChildAs(node, 0).Value), GetResultOfAstChildAs<::Productions>(node, 2)), visitor);
    }
    
    Grammar(String left, shared_ptr<::Productions> productions) : Left(move(left)), Productions(move(productions))
    { }

    auto Visit(IVisitor* visitor) -> void override
    {
        visitor->Visit(this);
    }
};

struct MoreGrammars : public AstNode
{
    vector<shared_ptr<Grammar>> Items;

    static auto Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, IVisitor*) -> shared_ptr<MoreGrammars>
    {
        switch (node->ChildSymbols.size())
        {
        case 0:
        case 1:
            return make_shared<MoreGrammars>(vector<shared_ptr<Grammar>>{});
        case 3:
        {
            vector gs{ GetResultOfAstChildAs<Grammar>(node, 1) };
            gs.append_range(GetResultOfAstChildAs<MoreGrammars>(node, 2)->Items);
            return make_shared<MoreGrammars>(move(gs));
        }
        default:
            throw std::logic_error(format("MoreGrammars not support {}", node->ChildSymbols.size()));
        }
    }

    MoreGrammars(vector<shared_ptr<Grammar>> grammars) : Items(move(grammars))
    {
    }

    auto Visit(IVisitor* visitor) -> void override
    {
    }
};

struct Grammars : public AstNode
{
    vector<shared_ptr<Grammar>> Items;

    static auto Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, IVisitor* visitor) -> shared_ptr<Grammars>
    {
        switch (node->ChildSymbols.size())
        {
        case 0:
            return ApplyVisitor(make_shared<Grammars>(vector<shared_ptr<Grammar>>{}), visitor);
        case 3:
        {
            vector gs{ GetResultOfAstChildAs<Grammar>(node, 1) };
            gs.append_range(GetResultOfAstChildAs<MoreGrammars>(node, 2)->Items);
            return ApplyVisitor(make_shared<Grammars>(move(gs)), visitor);
        }
        default:
            throw std::logic_error(format("Grammars not support {}", node->ChildSymbols.size()));
        }
    }

    Grammars(vector<shared_ptr<Grammar>> grammars) : Items(move(grammars))
    { }

    auto Visit(IVisitor* visitor) -> void override
    {
        visitor->Visit(this);
    }
};

struct AstFactory
{
    using TypeConfigs = List<Pair<"grammars", Grammars>, Pair<"more-grammars", MoreGrammars>, Pair<"grammar", Grammar>, Pair<"productions", Productions>, Pair<"production", Production>, Pair<"more-productions", MoreProductions>,
        Pair<"production", Production>, Pair<"more-items", MoreItems>, Pair<"item", Item>, Pair<"item_0", BasicItem>>;

    static auto Create(IVisitor* visitor, SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node) -> void
    {
        //std::println("AstFactory handle {}", node->Name);
        auto r = Iterate(TypeConfigs{}, node, visitor);
        //r->Visit(visitor);
        node->Result = r;
    }

    template <typename... Ts>
    static auto Iterate(List<Ts...> configs, SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, IVisitor* visitor) -> shared_ptr<AstNode>
    {
        using Types = decltype(configs);
        if (node->Name == Types::Head::Key)
        {
            return DoCreate<typename Types::Head::Value>(node, visitor);
        }
        else if constexpr (not std::is_convertible_v<typename Types::Tail, Nil>)
        {
            return Iterate(typename Types::Tail{}, node, visitor);
        }
        else
        {
            // not handle
            return nullptr;
        }
    }

    template <typename Object>
    static auto DoCreate(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, IVisitor* visitor) -> shared_ptr<AstNode>
    {
        auto r = Object::Construct(node, visitor);
        return r;
    }
};

export
{
    struct AstFactory;
    struct AstNode;
    struct Grammar;
    struct Grammars;
    struct Item;
    struct BasicItem;
    struct Duplicate;
    struct Combine;
    struct Optional;
    struct DataRange;
    struct Symbol;
    struct Terminal;
    struct RegExp;
    struct Productions;
    struct Production;
    struct IVisitor;
}