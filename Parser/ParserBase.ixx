export module Parser:ParserBase;

import std;
import Base;
import :InputStream;

using std::string;
using std::string_view;
using std::expected;
using std::vector;
using std::pair;
using std::variant;
using std::size_t;
using std::stack;
using std::map;
using std::move;

template <typename A, typename B>
concept ExplicitConvertibleTo = requires (A a) { static_cast<B>(a); };

template <size_t N>
struct ZeroOrMoreItems
{
    char p[N]{};

    constexpr ZeroOrMoreItems(char const(&pp)[N])
    {
        std::ranges::copy(pp, p);
    }
};

//template <ZeroOrMoreItems A>
//constexpr auto operator""s()
//{
//
//}

template<class... Ts>
struct overloads : Ts... { using Ts::operator()...; };

export
{
    constexpr string_view epsilon = "";
    /// \0 in string means eof, note only work in grammar representation
    constexpr string_view eof = "\0";
    //using Input = string; // TODO change

    struct ParseFailResult
    {
        // maybe add failed position later
        string Message;
    };
    template <typename T>
    using ParserResult = expected<T, ParseFailResult>;
    using LeftSide = String;
    using SimpleRightSide = vector<String>;
    using SimpleGrammar = pair<LeftSide, vector<SimpleRightSide>>;
    using SimpleGrammars = map<LeftSide, vector<SimpleRightSide>>;
    struct ParseInfo
    {
        SimpleGrammars Grammars;
		map<string_view, int> Terminal2IntTokenType; // terminal symbol -> int token type
        map<string_view, vector<string_view>> Starts; // symbol -> starts
        map<string_view, vector<string_view>> Firsts; // symbol -> firsts
        map<string_view, vector<string_view>> Follows; // symbol -> follows
    };
    template <typename T>
    concept IToken = requires (T t)
    {
        { t.Type } -> ExplicitConvertibleTo<int>; // when lex comes with conflict, it will chose the one with lower int value
        { t.Value } -> std::convertible_to<string>;
        { t.IsEof() } -> std::same_as<bool>;
    };

    template <IToken Token, typename ChildSyntaxNode>
    struct SyntaxTreeNodeBase
    {
        String Name;
        vector<String> ChildSymbols;
        vector<variant<Token, ChildSyntaxNode>> Children;// put all SyntaxTreeNode in a vector, here use raw pointer or deconstruct manually from bottom

        SyntaxTreeNodeBase(String name, vector<String> childSymbols, vector<variant<Token, ChildSyntaxNode>> children = {})
            : Name(move(name)), ChildSymbols(move(childSymbols)), Children(move(children)) // typo here, Kern invoke me
        {
            if (Name.Empty())
            {
                throw std::invalid_argument("SyntaxTreeNode::Name is empty");
            }
        }

        SyntaxTreeNodeBase(SyntaxTreeNodeBase&& that) = default;

        SyntaxTreeNodeBase& operator= (SyntaxTreeNodeBase const& that) = delete;
        SyntaxTreeNodeBase& operator= (SyntaxTreeNodeBase&& that) = default;

        /// <summary>
        /// due to this is recursive data structure, copy directly will cause deep recursive call in actual usage.
        /// so delete it explicitly
        /// </summary>
        SyntaxTreeNodeBase(SyntaxTreeNodeBase const& that) = delete;

        ~SyntaxTreeNodeBase()
        {
            using std::println;
            stack<ChildSyntaxNode> workingNodes;
            for (; not Children.empty(); Children.pop_back())
            {
                auto& back = Children.back();
                std::visit(overloads
                {
                    [&workingNodes](ChildSyntaxNode& n) -> void { workingNodes.push(move(n)); },
                    [](Token) -> void {},
                }, back);
            }

            for (; not workingNodes.empty();)
            {
                //println("working nodes size: {}", workingNodes.size());
                auto working = move(workingNodes.top());
                workingNodes.pop();
                for (; not working.Children.empty(); working.Children.pop_back())
                {
                    auto& back = working.Children.back();
                    std::visit(overloads
                    {
                        [&workingNodes](ChildSyntaxNode& n) -> void { workingNodes.push(move(n)); },
                        [](Token) -> void {},
                    }, back);
                }
            }
        }

        template <typename Self, typename Node = std::decay_t<Self>>
        auto PackOutChildrenAsNode(this Self&& self, int count, String name) -> Node
        {
            using std::make_move_iterator;
            auto&& ChildSymbols = self.ChildSymbols;
			auto&& Children = self.Children;

			if (count > self.ChildSymbols.size())
			{
				throw std::invalid_argument("count is larger than ChildSymbols size");
			}
			vector<String> childSymbols;
			childSymbols.reserve(count);
			childSymbols.insert(childSymbols.end(), make_move_iterator(ChildSymbols.begin()), make_move_iterator(ChildSymbols.begin() + count));
			ChildSymbols.erase(ChildSymbols.begin(), ChildSymbols.begin() + count);
			vector<variant<Token, Node>> children;
			children.reserve(count);
			children.insert(children.end(), make_move_iterator(Children.begin()), make_move_iterator(Children.begin() + count));
			Children.erase(Children.begin(), Children.begin() + count);
			Node node{ move(name), move(childSymbols), move(children) };
			return node;
        }
    };
    template <IToken Token, typename Result>
    struct SyntaxTreeNode;
    template <IToken Token>
	struct SyntaxTreeNode<Token, void> : SyntaxTreeNodeBase<Token, SyntaxTreeNode<Token, void>>
    {
        using Base = SyntaxTreeNodeBase<Token, SyntaxTreeNode<Token, void>>;
        using Base::Base;
        using Base::operator=;
    };
    template <IToken Token, typename ResultType>
	struct SyntaxTreeNode : SyntaxTreeNodeBase<Token, SyntaxTreeNode<Token, ResultType>>
    {
        ResultType Result;

        using Base = SyntaxTreeNodeBase<Token, SyntaxTreeNode<Token, ResultType>>;
        using Base::Base;
        using Base::operator=;
    };

    template <IToken Token, typename Result>
    struct std::formatter<SyntaxTreeNode<Token, Result>, char>
    {
        constexpr auto parse(std::format_parse_context& ctx)
        {
            auto it = ctx.begin();
            if (it == ctx.end())
                return it;

            if (it != ctx.end() && *it != '}')
                throw std::format_error("Invalid format args for QuotableString.");

            return it;
        }

        template<class... Ts>
        struct overloads : Ts... { using Ts::operator()...; };

        template <class FormatContext>
        constexpr auto format(SyntaxTreeNode<Token, Result> const& t, FormatContext& fc) const
        {
            using std::back_inserter;
            using std::format_to;
            using std::format;
            using std::holds_alternative;
            using std::get;
            using std::ranges::views::reverse;

            stack<variant<Token, SyntaxTreeNode<Token, Result>> const*> workings;
            string s;

            format_to(back_inserter(s), "{{\n");
            format_to(back_inserter(s), "Name: {}\n", t.Name);
            if (t.Children.empty())
            {
                format_to(back_inserter(s), "Children: [");
            }
            else
            {
                format_to(back_inserter(s), "Children: [\n");
                workings.push(nullptr); // to notify encounter current children's end
                for (auto& i : reverse(t.Children))
                {
                    workings.push(&i);
                }
            }
            for (; not workings.empty();)
            {
                variant<Token, SyntaxTreeNode<Token, Result>> const* working = workings.top();
                workings.pop();
                if (working == nullptr)
                {
                    format_to(back_inserter(s), "]\n");
                    format_to(back_inserter(s), "}}\n");
                }
                else if (holds_alternative<Token>(*working))// no specialization formatter<variant>, so we need to do it manually
                {
                    format_to(back_inserter(s), "{}\n", get<0>(*working));
                }
                else if (holds_alternative<SyntaxTreeNode<Token, Result>>(*working))
                {
                    auto& node = get<1>(*working);
                    format_to(back_inserter(s), "{{\n");
                    format_to(back_inserter(s), "Name: {}\n", node.Name);
                    if (node.Children.empty())
                    {
                        format_to(back_inserter(s), "Children: []\n");
                        format_to(back_inserter(s), "}}\n");
                    }
                    else
                    {
                        format_to(back_inserter(s), "Children: [\n");
                        workings.push(nullptr);
                        for (auto& i : reverse(node.Children))
                        {
                            workings.push(&i);
                        }
                    }
                }
            }
            // enclosing for the entire object
            format_to(back_inserter(s), "]\n");
            format_to(back_inserter(s), "}}\n");

            return std::format_to(fc.out(), "{}", s);
        }
    };

    template <typename T>
    concept IConflictResolvable = requires (T t, String nontermin, String termin)
    {
        { t.Resolvable(nontermin, termin) } -> std::same_as<bool>;
    };

    template <typename T, template <typename> class ActualStream, typename Tok, typename Result, typename Parse, typename Callback>
    concept ICustomParser = requires (T t, String nontermin, String termin, ActualStream<Tok> stream, Parse const& parse, Callback const& callback)
    {
		requires Stream<ActualStream, Tok>;
	    { parse(nontermin, stream) } -> std::same_as<ParserResult<SyntaxTreeNode<Tok, Result>>>;
        //requires std::invocable<decltype(parse), String, decltype(stream)>; // maybe cannot deduce the Stream concept and Tok in concept type here, should use other way
        { t.template Parse<Result>(nontermin, termin, stream, parse, callback) } -> std::same_as<ParserResult<SyntaxTreeNode<Tok, Result>>>; // error here, also cannot deduce the type arg for Parse
    };
    template <size_t N1>
    auto operator| (string_view left, char const(&right)[N1]) -> vector<SimpleRightSide>
    {
        return { { String(left) }, { right } };
    }

    template <size_t N0>
    auto operator| (vector<SimpleRightSide> left, char const(&right)[N0]) -> vector<SimpleRightSide>
    {
        left.push_back({ right });
        return left;
    }
}