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
auto GetResultOfChildAs(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, int i) -> shared_ptr<T>
{
    return dynamic_pointer_cast<T>(std::get<1>(node->Children[i]).Result);
}

auto GetTokChild(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, int i) -> Token<TokType>&
{
    return std::get<0>(node->Children[i]);
}

auto GetSyntaxNodeChild(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, int i) -> SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>&
{
    return std::get<1>(node->Children[i]);
}

template <typename T>
auto LoopCollectResult(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, int moveDeepIndex, size_t selectedRuleSize, int collectChildResultIndex) -> vector<shared_ptr<T>>
{
    vector<shared_ptr<T>> results;
    for (auto baseNode = &node->GetChildAsNode(moveDeepIndex); 
        baseNode->ChildSymbols.size() == selectedRuleSize;
        baseNode = &baseNode->GetChildAsNode(moveDeepIndex))
    {
        results.push_back(GetResultOfChildAs<T>(baseNode, collectChildResultIndex));
    }
    return results;
}

struct Annotation;
struct Grammar;
struct Grammars;
struct AllGrammars;
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
    virtual auto Visit(Annotation*) -> void = 0;
    virtual auto Visit(Grammar*) -> void = 0;
    virtual auto Visit(Grammars*) -> void = 0;
    virtual auto Visit(AllGrammars*) -> void = 0;
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
struct DefaultVisitor;

template <>
struct DefaultVisitor<void> : IVisitor
{
    DefaultVisitor()
    {
    }

    auto Visit(Terminal* object) -> void override
    {
    }

    auto Visit(RegExp* object) -> void override
    {
    }

    auto Visit(Symbol* object) -> void override
    {
    }

    auto Visit(DataRange* object) -> void override
    {
    }

    auto Visit(Optional* object) -> void override
    {
    }

    auto Visit(Combine* object) -> void override
    {
    }

    auto Visit(Duplicate* object) -> void override
    {
    }

    auto Visit(Annotation* object) -> void override
    {
	}

    auto Visit(Grammar*) -> void override
    {
    }

    auto Visit(Grammars*) -> void override
    {
    }

    auto Visit(AllGrammars*) -> void override
    {
    }

    auto Visit(Productions*) -> void override
    {
    }

    auto Visit(Production*) -> void override
    {
    }
};

template <typename T>
struct DefaultVisitor : DefaultVisitor<void>
{
    T Result;
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
            vector is{ GetResultOfChildAs<Item>(node, 0) };
            is.append_range(LoopCollectResult<Item>(node, 1, 2, 0));
            return ApplyVisitor(make_shared<Production>(move(is)), visitor);
        }
        default:
            throw logic_error(format("not handled Production Item symbols {}", node->ChildSymbols));
        }
    }

    Production(vector<shared_ptr<Item>> items) : Items(move(items))
    { }

    auto Visit(IVisitor* visitor) -> void override
    {
        visitor->Visit(this);
    }
};

// TODO remove?
struct Productions : public AstNode
{
    vector<shared_ptr<Production>> Items;

    static auto Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, IVisitor* visitor) -> shared_ptr<Productions>
    {
        switch (node->ChildSymbols.size())
        {
        case 0:
            return ApplyVisitor(make_shared<Productions>(vector{ make_shared<Production>(Production({})) }), visitor);
        case 2:
        {
            vector ps{ GetResultOfChildAs<Production>(node, 0) };
            if (auto& moreProductions = node->GetChildAsNode(1); moreProductions.ChildSymbols.size() == 2)
            {
                auto innerProductions = GetResultOfChildAs<Productions>(&moreProductions, 1);
				ps.append_range(innerProductions->Items);
            }
            return ApplyVisitor(make_shared<Productions>(move(ps)), visitor);
        }
        default:
            throw logic_error(format("not handled Productions Item symbols {}", node->ChildSymbols));
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
    String Alias;
    Terminal(String value, String alias = {}) : Value(move(value)), Alias(move(alias))
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
        return GetResultOfChildAs<BasicItem>(node, 0); // not invoke now
    case 2:
        return ApplyVisitor(make_shared<Duplicate>(0, std::numeric_limits<unsigned>::max(), GetResultOfChildAs<BasicItem>(node, 1)), visitor);
    case 3:
        return ApplyVisitor(make_shared<Duplicate>(std::stoul(GetTokChild(node, 0).Value), std::numeric_limits<unsigned>::max(), GetResultOfChildAs<BasicItem>(node, 2)), visitor);
    case 4:
        return ApplyVisitor(make_shared<Duplicate>(std::stoul(GetTokChild(node, 0).Value), std::stoul(GetTokChild(node, 2).Value), GetResultOfChildAs<BasicItem>(node, 3)), visitor);
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
            return ApplyVisitor(make_shared<Terminal>(String(GetTokChild(node, 0).Value)), visitor);
        }
        else if (node->ChildSymbols.front() == "sym")
        {
            return ApplyVisitor(make_shared<Symbol>(String(GetTokChild(node, 0).Value)), visitor);
        }
        else if (node->ChildSymbols.front() == "regExp")
        {
            return ApplyVisitor(make_shared<RegExp>(String(GetTokChild(node, 0).Value)), visitor);
        }
        break;
    case 3:
        if (node->ChildSymbols.front() == "terminal") // terminal @ sym
        {
            return ApplyVisitor(make_shared<Terminal>(String(GetTokChild(node, 0).Value), String(GetTokChild(node, 2).Value)), visitor);
        }
        else if (node->ChildSymbols.front() == "digitOrAlphabet")
        {
            auto& left = GetTokChild(node, 0).Value;
            auto& right = GetTokChild(node, 2).Value;
            if (left.size() == 3 and right.size() == 3)
            {
                return ApplyVisitor(make_shared<DataRange>(left[1], right[1]), visitor);
            }
            else
            {
                throw std::out_of_range("DataRange only support three chars like 'x', there is invalid DataRange in code");
            }
        }
        else if (node->ChildSymbols.front() == "[")
        {
            return ApplyVisitor(make_shared<Optional>(GetResultOfChildAs<Productions>(node, 1)), visitor);
        }
        else if (node->ChildSymbols.front() == "(")
        {
            return ApplyVisitor(make_shared<Combine>(GetResultOfChildAs<Productions>(node, 1)), visitor);
        }
        break;
    }
    throw logic_error(format("not handled BasicItem child symbols {}", node->ChildSymbols));
}

struct Annotation : public AstNode
{
    String const Type;
    String const Value;

    static auto Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, IVisitor* visitor) -> shared_ptr<Annotation>
    {
        return ApplyVisitor(make_shared<Annotation>(String(GetTokChild(node, 1).Value), String(GetTokChild(node, 2).Value)), visitor);
    }

    Annotation(String type, String value) : Type(move(type)), Value(move(value))
    {
    }

    auto Visit(IVisitor* visitor) -> void override
    {
		visitor->Visit(this);
    }
};

struct Grammar : public AstNode
{
	vector<shared_ptr<Annotation>> Annotations;
    String Left;
    shared_ptr<Productions> Productions;
    bool IsStarArrow;

    static auto Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, IVisitor* visitor) -> shared_ptr<Grammar>
    {
		vector<shared_ptr<Annotation>> annotations;
        if (node->GetChildAsNode(0).ChildSymbols.size() == 3)
        {
			// grammar[0] -> optional-annotations
            auto& optAnnotations = node->GetChildAsNode(0);
            //     optional-annotations[0] -> annotation
			annotations.push_back(GetResultOfChildAs<Annotation>(&optAnnotations, 0));
			// base node is annotations node, loop to get all annotation(s)
            //     optional-annotations[1] -> annotations
			annotations.append_range(LoopCollectResult<Annotation>(&optAnnotations, 1, 2, 0));
        }
        auto& arrowSubtree = GetSyntaxNodeChild(node, 2);
        auto isStarArrow = GetTokChild(&arrowSubtree, 0).Type == TokType::StarArrow;
        return ApplyVisitor(make_shared<Grammar>(annotations, String(GetTokChild(node, 1).Value), GetResultOfChildAs<::Productions>(node, 3), isStarArrow), visitor);
    }

    Grammar(String left, shared_ptr<::Productions> productions, bool isStarArrow = false)
        : Grammar({}, move(left), move(productions), isStarArrow)
    { }

    Grammar(vector<shared_ptr<Annotation>> annotations, String left, shared_ptr<::Productions> productions, bool isStarArrow = false)
        : Annotations(move(annotations)), Left(move(left)), Productions(move(productions)), IsStarArrow(isStarArrow)
    { }

    auto Visit(IVisitor* visitor) -> void override
    {
        visitor->Visit(this);
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
            vector gs{ GetResultOfChildAs<Grammar>(node, 1) };
            gs.append_range(LoopCollectResult<Grammar>(node, 2, 3, 1));
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

    auto HaveGrammarOf(String const& left) const -> bool
    {
        for (auto const& g : Items)
        {
            if (g->Left == left)
            {
                return true;
            }
        }
        return false;
	}
};

struct AllGrammars : public AstNode
{
    shared_ptr<Grammars> LexRules;
    String StartSymbolOfParseRules;
    shared_ptr<Grammars> ParseRules;

    static auto Construct(SyntaxTreeNode<Token<TokType>, shared_ptr<AstNode>>* node, IVisitor* visitor) -> shared_ptr<AllGrammars>
    {
        switch (node->ChildSymbols.size())
        {
        case 6:
        {
            auto& startInfoNode = GetSyntaxNodeChild(node, 3);
            auto const& startSymbolName = GetTokChild(&startInfoNode, 2).Value;
            return ApplyVisitor(make_shared<AllGrammars>(GetResultOfChildAs<Grammars>(node, 1), String{ startSymbolName },
                GetResultOfChildAs<Grammars>(node, 4)), visitor);
        }
        default:
            throw std::logic_error(format("Grammars not support {}", node->ChildSymbols.size()));
        }
    }

    AllGrammars(shared_ptr<Grammars> lexRules, String startSymbolOfParseRules, shared_ptr<Grammars> parseRules)
        : LexRules(move(lexRules)), StartSymbolOfParseRules(move(startSymbolOfParseRules)), ParseRules(move(parseRules))
    {
    }

    auto Visit(IVisitor* visitor) -> void override
    {
        visitor->Visit(this);
    }
};

struct AstFactory
{
    using TypeConfigs = List<Pair<"all-grammars", AllGrammars>, Pair<"grammars", Grammars>, Pair<"grammar", Grammar>, Pair<"annotation", Annotation>, Pair<"productions", Productions>, Pair<"production", Production>,
        Pair<"production", Production>, Pair<"item", Item>, Pair<"item_0", BasicItem>>;

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
    struct AllGrammars;
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
    template <typename T>
    struct DefaultVisitor;
}