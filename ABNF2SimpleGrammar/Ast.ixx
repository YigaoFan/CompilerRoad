export module Ast;

import std;
import Base;

using std::vector;
using std::unique_ptr;
using std::index_sequence;

struct Node
{

    template <typename... Args>
    auto Invoke(vector<Node*> children) -> Node*
    {
        auto argIndexs = std::index_sequence_for<Args...>{};
        return DoInvoke<Args...>(move(children), argIndexs); // need specify Self template arg?
    }

    template <typename... Args, std::size_t... idxs>
    auto DoInvoke(this auto&& self, vector<Node*> children, index_sequence<idxs...>)
    {
        using std::apply;
        using std::bind_front;
        using Self = decltype(self);
        auto args = std::make_tuple(static_cast<Args>(children[idxs])...);
        return apply(bind_front(&Self::Construct, self), args);
    }

    // subclass should implement
    auto Construct()
    {

    }
};

struct Item
{
    virtual ~Item() = default;
};

struct Production
{
    vector<Item> Items;
};

struct BasicItem : public Item
{
    virtual ~BasicItem() = default;
};

struct Terminal : public BasicItem
{
    String Value;
};

struct Symbol : public BasicItem
{
    String Value;
};

struct DataRange : public BasicItem
{
    int Left;
    int Right;
};

struct Optional : public BasicItem
{
    vector<unique_ptr<Production>> Productions;
};

struct Combine : public BasicItem
{
    Production Production;
};

struct Duplicate : public Item
{
    unsigned Low;
    unsigned High;
    unique_ptr<BasicItem> BasicItem;
};

struct Grammar
{
    String Left;
    vector<unique_ptr<Production>> Productions;
};

//auto ConsGrammars_0(vector<int> items) -> vector<Grammar>
//{
//
//}
//
//auto ConsGrammars_1() -> vector<Grammar>
//{
//
//}
//
//auto ConsGrammars_2() -> vector<Grammar>
//{
//
//}

using std::tuple;

template <size_t N>
struct StringLiteral
{
    char p[N]{};

    constexpr StringLiteral(char const(&pp)[N])
    {
        std::ranges::copy(pp, p);
    }
};

//template <ZeroOrMoreItems A>
//constexpr auto operator""s()
//{
//
//}

template <typename T0, typename T1>
struct Cons
{
    using Head = T0;
    using Tail = T1;
};

// Key is literal value as int, so cannot use Cons::T0 which is typename to implement
template <StringLiteral Key, typename T>
struct Pair
{
    static constexpr auto Key = Key;
    using Value = T;
};

template <typename T, typename... Ts>
struct List : Cons<T, List<Ts...>>
{

};

struct Nil
{
};

template <typename T>
struct List<T> : Cons<T, Nil>
{

};

using typelist = List<Pair<"Production", Production>, Pair<"Grammar", Grammar>>;

auto Func()
{
    typelist::Head::Key;
}

template <typename T>
concept IList = requires ()
{
    T::Head;
    T::Tail;
};

export
{

}