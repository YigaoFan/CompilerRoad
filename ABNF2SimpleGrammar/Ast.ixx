export module Ast;

import std;
import Base;

using std::vector;
using std::unique_ptr;

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

using list = List<Pair<"Production", Production>, Pair<"Grammar", Grammar>>;
list::Head::Value p;
//template <typename T0, typename T1, typename... Ts>
//auto Func(int i, Cons<T0, T1> typeMap, vector<std::string> rule, std::tuple<Ts...> args, auto visitor) -> std::variant<Ts...> // Ts in return value may not correct
//{
//    if (i == rule.size())
//    {
//        visitor.Invoke(args);
//    }
//    using Item = Cons<T0, T1>;
//    // iterate map items
//    if (rule[i] == Item::Head)
//    {
//        // map A into a type, so A must be a compile time string
//        Func(i + 1, rule, tuple(/*append casted item to args*/), visitor);
//    }
//}

export
{

}