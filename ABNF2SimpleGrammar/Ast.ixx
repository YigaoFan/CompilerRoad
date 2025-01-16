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
using std::ranges::to;
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
struct List;

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

struct AstNode
{
    virtual ~AstNode() = default;
};

// TODO fix production
template <typename T>
auto GetResultOfAstChildAs(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, int i) -> shared_ptr<T>
{
    return dynamic_pointer_cast<T>(std::get<1>(node->Children[i]).Result);
}

auto GetResultOfTokChildAs(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, int i) -> Token<TokType>
{
    return std::get<0>(node->Children[i]);
}

struct Item : public AstNode
{
    static auto Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node) -> shared_ptr<Item>;
};

struct MoreItems : public AstNode
{
    vector<shared_ptr<Item>> Items;

    static auto Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node) -> MoreItems
    {
        switch (node->ChildSymbols.size())
        {
        case 0:
            return MoreItems({});
        case 2:
        {
            vector is{ GetResultOfAstChildAs<Item>(node, 0) };
            is.append_range(GetResultOfAstChildAs<MoreItems>(node, 1)->Items);
            return MoreItems({ move(is) });
        }
        }
    }

    MoreItems(vector<shared_ptr<Item>> items) : Items(move(items))
    { }
};

struct Production : public AstNode
{
    vector<shared_ptr<Item>> Items;

    static auto Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node) -> Production
    {
        switch (node->ChildSymbols.size())
        {
        case 0:
            return Production({});
        case 2:
        {
            vector is{ GetResultOfAstChildAs<Item>(node, 0) };
            is.append_range(GetResultOfAstChildAs<MoreItems>(node, 1)->Items);
            return Production(move(is));
        }
        }
    }

    Production(vector<shared_ptr<Item>> items) : Items(move(items))
    { }
};

struct Productions;
struct MoreProductions : public AstNode
{
    shared_ptr<Productions> Productions;

    static auto Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node) -> MoreProductions
    {
        switch (node->ChildSymbols.size())
        {
        case 0:
            return MoreProductions(nullptr);
        case 2:
            return MoreProductions(GetResultOfAstChildAs<::Productions>(node, 1));
        }
    }

    MoreProductions(shared_ptr<::Productions> productions) : Productions(move(productions))
    {
    }
};

struct Productions : public AstNode
{
    vector<shared_ptr<Production>> Items;

    static auto Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node) -> Productions
    {
        switch (node->ChildSymbols.size())
        {
        case 0:
            return Productions({});
        case 2:
        {
            vector ps{ GetResultOfAstChildAs<Production>(node, 0) };
            ps.append_range(GetResultOfAstChildAs<MoreProductions>(node, 1)->Productions->Items);
            return Productions(move(ps));
        }
        }
    }

    Productions(vector<shared_ptr<Production>> items) : Items(move(items))
    { }
};

struct BasicItem : public Item
{
    static auto Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node) -> shared_ptr<BasicItem>;
};

struct Terminal : public BasicItem
{
    String Value;
    Terminal(String value) : Value(move(value))
    { }
};

struct Symbol : public BasicItem
{
    String Value;
    Symbol(String value) : Value(move(value))
    { }
};

struct DataRange : public BasicItem
{
    int Left;
    int Right;
    DataRange(int left, int right) : Left(left), Right(right)
    { }
};

struct Optional : public BasicItem
{
    shared_ptr<Productions> Productions;
    Optional(shared_ptr<::Productions> productions) : Productions(move(productions))
    { }
};

struct Combine : public BasicItem
{
    shared_ptr<Production> Production;
    Combine(shared_ptr<::Production> production) : Production(move(production))
    { }
};

struct Duplicate : public Item
{
    unsigned Low;
    unsigned High;
    shared_ptr<BasicItem> BasicItem;

    Duplicate(unsigned low, unsigned high, shared_ptr<::BasicItem> basicItem)
        : Low(low), High(high), BasicItem(move(basicItem))
    { }
};


auto Item::Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node) -> shared_ptr<Item>
{
    if (node->ChildSymbols.front() == "num")
    {
        switch (node->ChildSymbols.size())
        {
        case 3:
            return make_shared<Duplicate>(std::stoul(GetResultOfTokChildAs(node, 0).Value), std::numeric_limits<unsigned>::max(), GetResultOfAstChildAs<BasicItem>(node, 3));
        case 4:
            return make_shared<Duplicate>(std::stoul(GetResultOfTokChildAs(node, 0).Value), std::stoul(GetResultOfTokChildAs(node, 2).Value), GetResultOfAstChildAs<BasicItem>(node, 3));
        }
    }
    else if (node->ChildSymbols.front() == "*")
    {
        return BasicItem::Construct(node);
    }
    throw logic_error(format("not handled BasicItem Item symbols {}", node->ChildSymbols));
}

auto BasicItem::Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node) -> shared_ptr<BasicItem>
{
    switch (node->ChildSymbols.size())
    {
    case 1:
        if (node->ChildSymbols.front() == "terminal")
        {
            return make_shared<Terminal>(String(GetResultOfTokChildAs(node, 0).Value));
        }
        else if (node->ChildSymbols.front() == "sym")
        {
            return make_shared<Symbol>(String(GetResultOfTokChildAs(node, 0).Value));
        }
        break;
    case 3:
    {
        if (node->ChildSymbols.front() == "digitOrAlphabet")
        {
            return make_shared<DataRange>(GetResultOfTokChildAs(node, 0).Value.front(), GetResultOfTokChildAs(node, 2).Value.front());
        }
        else if (node->ChildSymbols.front() == "[")
        {
            return make_shared<Optional>(GetResultOfAstChildAs<Productions>(node, 1));
        }
        else if (node->ChildSymbols.front() == "(")
        {
            return make_shared<Combine>(GetResultOfAstChildAs<Production>(node, 1));
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

    static auto Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node) -> Grammar
    {
        return Grammar(String(GetResultOfTokChildAs(node, 0).Value), GetResultOfAstChildAs<::Productions>(node, 2));
    }
    
    Grammar(String left, shared_ptr<::Productions> productions) : Left(move(left)), Productions(move(productions))
    { }
};

struct Grammars : public AstNode
{
    vector<shared_ptr<Grammar>> Items;

    static auto Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node) -> Grammars
    {
        switch (node->ChildSymbols.size())
        {
        case 0:
            return Grammars({});
        case 1:
            return Grammars(vector{ GetResultOfAstChildAs<Grammar>(node, 0) });
        case 3:
        {
            vector gs{ GetResultOfAstChildAs<Grammar>(node, 0) };
            gs.append_range(GetResultOfAstChildAs<Grammars>(node, 2)->Items);
            return Grammars(move(gs));
        }
        default:
            throw std::logic_error(format("Grammars not support {}", node->ChildSymbols.size()));
        }
    }

    Grammars(vector<shared_ptr<Grammar>> grammars) : Items(move(grammars))
    { }

    ~Grammars() override = default;
};

struct AstFactory
{
    using TypeConfigs = List<Pair<"grammars", Grammars>, Pair<"grammar", Grammar>, Pair<"productions", Productions>, Pair<"production", Production>, Pair<"more-productions", MoreProductions>,
        Pair<"production", Production>, Pair<"more-items", MoreItems>, Pair<"item", Item>, Pair<"item_0", BasicItem>>;

    static auto Create(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node) -> void
    {
        std::println("AstFactory handle {}", node->Name);
        auto r = Iterate(TypeConfigs{}, node);
        node->Result = r;
    }

    template <typename... Ts>
    static auto Iterate(List<Ts...> configs, SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node) -> shared_ptr<AstNode>
    {
        using Types = decltype(configs);
        if (node->Name == Types::Head::Key)
        {
            return DoCreate<typename Types::Head::Value>(node);
        }
        else if constexpr (not std::is_convertible_v<typename Types::Tail, Nil>)
        {
            return Iterate(typename Types::Tail{}, node);
        }
        else
        {
            // not handle
        }
    }

    template <typename Object>
    static auto DoCreate(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node) -> shared_ptr<AstNode>
    {
        auto r = Object::Construct(node);
        if constexpr (std::is_convertible_v<decltype(r), shared_ptr<AstNode>>)
        {
            return r;
        }
        else
        {
            return make_shared<Object>(move(r));
        }
    }
};

export
{
    struct AstFactory;
    struct AstNode;
}