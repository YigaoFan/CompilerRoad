export module ExpressionParser;

import std;
import Base;
import Lexer;
import Parser;
import C11Spec;
import ParserCombinator;

using std::map;
using std::unexpected;
using std::array;
using std::pair;
using std::map;
using std::set;
using std::vector;
using std::string_view;
using std::shared_ptr;
using std::format;
using std::move;

/// <summary>
/// ensure the interval 2 between levels' value
/// </summary>
enum Power : int
{
	MinLevel = 0,
	Level0 = 2,
	Level1 = 4,
	Level2 = 6,
	Level3 = 8,
	Level4 = 10,
	Level5 = 12,
	Level6 = 14,
	Level7 = 16,
	Level8 = 18,
	Level9 = 20,
	Level10 = 22,
	Level11 = 24,
	Level12 = 26,
	Level13 = 28,
	Level14 = 30,
	Level15 = 32,
	MaxLevel = 34,
};

struct OperatorInfo
{
	/// <summary>
	/// -1 means not setting
	/// </summary>
	int LeftBindingPower = -1;
	/// <summary>
	/// -1 means not setting
	/// </summary>
	int RightBindingPower = -1;
};

#define MoveOrReturnFailMsg(stream, msg, ...) \
if (not stream.MoveNext()) { return unexpected(ParseFailResult{ .Message = format(msg __VA_OPT__(,) __VA_ARGS__) }); }

#define ReturnFailMsg(logLevel, msg, ...) \
do { Log(logLevel, msg __VA_OPT__(,) __VA_ARGS__); return unexpected(ParseFailResult{ .Message = format(msg __VA_OPT__(,) __VA_ARGS__) }); } while(0)

#define ReturnWithLog(result, logLevel, msg) \
do { Log(logLevel, msg" parse result: {}", result); return result; } while(0)

#define MoveOrReturnFailMsgWithLog(stream, logLevel, msg, ...) \
if (not stream.MoveNext()) { Log(logLevel, msg __VA_OPT__(,) __VA_ARGS__); return unexpected(ParseFailResult{ .Message = format(msg __VA_OPT__(,) __VA_ARGS__) }); }

class ExpressionParser
{
private:
	// no left binding power for postfix operators
	inline static map<TokType, OperatorInfo> PrefixOperators =
	{
		{ TokType::Punctuator_Minus,       {.RightBindingPower = Power::Level14, }},
		{ TokType::Punctuator_Plus,        {.RightBindingPower = Power::Level14, }},
		{ TokType::Punctuator_Increment,   {.RightBindingPower = Power::Level14, }},
		{ TokType::Punctuator_Decrement,   {.RightBindingPower = Power::Level14, }},
		{ TokType::Punctuator_Exclamation, {.RightBindingPower = Power::Level14, }},
		{ TokType::Punctuator_Tilde,       {.RightBindingPower = Power::Level14, }},
		{ TokType::Punctuator_Star,        {.RightBindingPower = Power::Level14, }},
		{ TokType::Punctuator_Ampersand,   {.RightBindingPower = Power::Level14, }},
		{ TokType::Keyword_Sizeof,         {.RightBindingPower = Power::Level14, }},
		{ TokType::Keyword_Alignof,        {.RightBindingPower = Power::Level14, }},
		{ TokType::Punctuator_LeftParen,   {.RightBindingPower = Power::Level14, }}, // parenthesized expression / type cast
	};
	inline static map<TokType, OperatorInfo> InfixOperators =
	{
		{ TokType::Punctuator_Comma,            {.LeftBindingPower = Power::Level0, .RightBindingPower = Power::Level0 + 1, }},

		// assign is right binding
		{ TokType::Punctuator_Assign,           {.LeftBindingPower = Power::Level1, .RightBindingPower = Power::Level1 - 1, }},
		{ TokType::Punctuator_MulAssign,        {.LeftBindingPower = Power::Level1, .RightBindingPower = Power::Level1 - 1, }},
		{ TokType::Punctuator_DivAssign,        {.LeftBindingPower = Power::Level1, .RightBindingPower = Power::Level1 - 1, }},
		{ TokType::Punctuator_ModAssign,        {.LeftBindingPower = Power::Level1, .RightBindingPower = Power::Level1 - 1, }},
		{ TokType::Punctuator_AddAssign,        {.LeftBindingPower = Power::Level1, .RightBindingPower = Power::Level1 - 1, }},
		{ TokType::Punctuator_SubAssign,        {.LeftBindingPower = Power::Level1, .RightBindingPower = Power::Level1 - 1, }},
		{ TokType::Punctuator_LeftShiftAssign,  {.LeftBindingPower = Power::Level1, .RightBindingPower = Power::Level1 - 1, }},
		{ TokType::Punctuator_RightShiftAssign, {.LeftBindingPower = Power::Level1, .RightBindingPower = Power::Level1 - 1, }},
		{ TokType::Punctuator_AndAssign,        {.LeftBindingPower = Power::Level1, .RightBindingPower = Power::Level1 - 1, }},
		{ TokType::Punctuator_OrAssign,         {.LeftBindingPower = Power::Level1, .RightBindingPower = Power::Level1 - 1, }},
		{ TokType::Punctuator_XorAssign,        {.LeftBindingPower = Power::Level1, .RightBindingPower = Power::Level1 - 1, }},
		
		// ternary operator is right binding
		{ TokType::Punctuator_Question,         {.LeftBindingPower = Power::Level2, .RightBindingPower = Power::Level2 - 1, }},

		{ TokType::Punctuator_LogicalOr,    {.LeftBindingPower = Power::Level3, .RightBindingPower = Power::Level3 + 1, }},
		{ TokType::Punctuator_LogicalAnd,   {.LeftBindingPower = Power::Level4, .RightBindingPower = Power::Level4 + 1, }},
		{ TokType::Punctuator_Pipe,         {.LeftBindingPower = Power::Level5, .RightBindingPower = Power::Level5 + 1, }},
		{ TokType::Punctuator_Caret,        {.LeftBindingPower = Power::Level6, .RightBindingPower = Power::Level6 + 1, }},
		{ TokType::Punctuator_Ampersand,    {.LeftBindingPower = Power::Level7, .RightBindingPower = Power::Level7 + 1, }},
		{ TokType::Punctuator_Equal,        {.LeftBindingPower = Power::Level8, .RightBindingPower = Power::Level8 + 1, }},
		{ TokType::Punctuator_NotEqual,     {.LeftBindingPower = Power::Level8, .RightBindingPower = Power::Level8 + 1, }},
		{ TokType::Punctuator_Less,         {.LeftBindingPower = Power::Level9, .RightBindingPower = Power::Level9 + 1, }},
		{ TokType::Punctuator_Greater,      {.LeftBindingPower = Power::Level9, .RightBindingPower = Power::Level9 + 1, }},
		{ TokType::Punctuator_LessEqual,    {.LeftBindingPower = Power::Level9, .RightBindingPower = Power::Level9 + 1, }},
		{ TokType::Punctuator_GreaterEqual, {.LeftBindingPower = Power::Level9, .RightBindingPower = Power::Level9 + 1, }},
		{ TokType::Punctuator_LeftShift,    {.LeftBindingPower = Power::Level10, .RightBindingPower = Power::Level10 + 1, }},
		{ TokType::Punctuator_RightShift,   {.LeftBindingPower = Power::Level10, .RightBindingPower = Power::Level10 + 1, }},
		{ TokType::Punctuator_Plus,         {.LeftBindingPower = Power::Level11, .RightBindingPower = Power::Level11 + 1, }},
		{ TokType::Punctuator_Minus,        {.LeftBindingPower = Power::Level11, .RightBindingPower = Power::Level11 + 1, }},
		{ TokType::Punctuator_Star,         {.LeftBindingPower = Power::Level12, .RightBindingPower = Power::Level12 + 1, }},
		{ TokType::Punctuator_Slash,        {.LeftBindingPower = Power::Level12, .RightBindingPower = Power::Level12 + 1, }},
		{ TokType::Punctuator_Percent,      {.LeftBindingPower = Power::Level12, .RightBindingPower = Power::Level12 + 1, }},

	};
	// no right binding power for postfix operators
	inline static map<TokType, OperatorInfo> PostfixOperators =
	{
		{ TokType::Punctuator_Increment,   {.LeftBindingPower = Power::Level15, }},
		{ TokType::Punctuator_Decrement,   {.LeftBindingPower = Power::Level15, }},
		{ TokType::Punctuator_LeftParen,   {.LeftBindingPower = Power::Level15, }},
		{ TokType::Punctuator_LeftBracket, {.LeftBindingPower = Power::Level15, }},
		{ TokType::Punctuator_Dot,         {.LeftBindingPower = Power::Level15, }},
		{ TokType::Punctuator_Arrow,       {.LeftBindingPower = Power::Level15, }},
	};
	struct ExpressionParseConfig
	{
		int MinInfixBindingPower;
	};
	inline static map<String, ExpressionParseConfig> ExpressionParseConfigs =
	{
		{ "expression",            {.MinInfixBindingPower = Power::MinLevel, } },
		{ "constant-expression",   {.MinInfixBindingPower = Power::Level1,/* same as assign op */ } }, 
		{ "assignment-expression", {.MinInfixBindingPower = Power::Level0,/* same as comma op  */ } },
	};
	shared_ptr<HtmlLogger> logger = nullptr;
public:
	auto FirstSet() const -> map<String, set<String>>
	{
		map<String, set<String>> firsts
		{
			{ "expression", {} },
			{ "constant-expression", {} },
			{ "assignment-expression", {} },
		};
		// union of prefix operators and atoms that can start an expression
		// prefix operators: -, +, ++, --, !, ~, *, &, sizeof, alignof, (
		for (auto const& [tokType, _] : PrefixOperators)
		{
			for (auto const& [term, type] : terminal2IntTokenType)
			{
				if (type == static_cast<int>(tokType))
				{
					firsts.at("expression").insert(String(term));
					firsts.at("constant-expression").insert(String(term));
					firsts.at("assignment-expression").insert(String(term));
					break;
				}
			}
		}
		// atoms: primary-expression first
		firsts.at("expression").insert("Identifier");
		firsts.at("expression").insert("Integer-constant");
		firsts.at("expression").insert("Floating-constant");
		firsts.at("expression").insert("Character-constant");
		firsts.at("expression").insert("StringLiteral");

		firsts.at("constant-expression").insert("Identifier");
		firsts.at("constant-expression").insert("Integer-constant");
		firsts.at("constant-expression").insert("Floating-constant");
		firsts.at("constant-expression").insert("Character-constant");
		firsts.at("constant-expression").insert("StringLiteral");

		firsts.at("assignment-expression").insert("Identifier");
		firsts.at("assignment-expression").insert("Integer-constant");
		firsts.at("assignment-expression").insert("Floating-constant");
		firsts.at("assignment-expression").insert("Character-constant");
		firsts.at("assignment-expression").insert("StringLiteral");

		return firsts;
	}

	auto Parsable(String nontermin) const -> bool
	{
		return nontermin == "expression" or nontermin == "constant-expression" or nontermin == "assignment-expression";
	}

	auto Logger(shared_ptr<HtmlLogger> logger)
	{
		this->logger = move(logger);
	}

	template <typename Result, template <typename> class ActualStream, IToken Tok>
		requires Stream<ActualStream, Tok>
	auto Parse(String nontermin, ActualStream<Tok>& stream, auto parse) const -> ParserResult<SyntaxTreeNode<Tok, Result>>
	{
		if (not Parsable(nontermin))
		{
			return unexpected(ParseFailResult{ .Message = format("nonterminal({}) isn't parsable by ExpressionParser", nontermin) });
		}
		auto parseWrapper = [this, &parse](String nontermin, ActualStream<Tok>& stream) -> ParserResult<SyntaxTreeNode<Tok, Result>>
		{
			auto r = parse(nontermin, stream);
			if (r.has_value())
			{
				stream.Rollback(); // bugfix: stream pos should be move back one token
				if (r->Name == "root")
				{
					return move(r->GetChildAsNode(0));
				}
			}
			return r;
		};
		// pass the min binding power corresponding the nonterminal to ParseExpression
		// so that the parser can stop at the right position when parsing infix operators
		return ParseExpression<Result>(stream, ExpressionParseConfigs.at(nontermin).MinInfixBindingPower, parseWrapper);
	}

	 template <typename Result, template <typename> class ActualStream, IToken Tok>
	 	requires Stream<ActualStream, Tok>
	 auto Parse(String nontermin, ActualStream<Tok>& stream, auto parse, HtmlLogger& logger) const -> ParserResult<SyntaxTreeNode<Tok, Result>>
	 {
	 	auto parseWrapper = [&parse, this](String nontermin, ActualStream<Tok>& stream) -> ParserResult<SyntaxTreeNode<Tok, Result>>
	 	{
			auto r = parse(nontermin, stream);
			if (r.has_value())
			{
				stream.Rollback(); // bugfix: stream pos should be move back one token
				if (r->Name == "root")
				{
					return move(r->GetChildAsNode(0));
				}
			}
			return r;
	 	};
	 	if (not Parsable(nontermin))
	 	{
	 		return unexpected(ParseFailResult{ .Message = format("nonterminal({}) isn't parsable by ExpressionParser", nontermin) });
	 	}
	 	return ParseExpression<Result>(stream, ExpressionParseConfigs.at(nontermin).MinInfixBindingPower, parseWrapper);
	 }
private:
	template <typename Result, template <typename> class ActualStream, typename Tok>
	auto ParseExpression(ActualStream<Tok>& stream, int minBindingPower, auto parse) const -> ParserResult<SyntaxTreeNode<Tok, Result>>
	{
		Log(Level::In, "ParseExpression with minBindingPower: {}", minBindingPower);
		auto lhs = ParsePrefix<Result>(stream, parse);
		if (not lhs.has_value())
		{
			Log(Level::Out, "ParsePrefix failed: {}", lhs.error().Message);
			return lhs;
		}

		for (;;)
		{
			if (not stream.MoveNext())
			{
				break;
			}
			auto const& op = stream.Current();
			// postfix operators have higher priority than infix operators, so check it firstly
			if (PostfixOperators.contains(op.Type))
			{
				auto lbp = PostfixBindingPower(op.Type);
				if (lbp <= minBindingPower)
				{
					// rollback to let the remain expression parser can read this operator
					stream.Rollback();
					break;
				}
				switch (op.Type)
				{
				case TokType::Punctuator_Dot:
				case TokType::Punctuator_Arrow:
					MoveOrReturnFailMsgWithLog(stream, Level::Out, "input stream is empty after postfix operator({})", op);
					if (stream.Current().Type != TokType::Identifier)
					{
						ReturnFailMsg(Level::Out, "expect identifier after postfix operator({}), but got {}", op, stream.Current());
					}
					lhs = Cons(op, array{ move(lhs.value()), ConsAtom<Result>(stream.Current()) });
					break;
				case TokType::Punctuator_LeftParen:
				{
					MoveOrReturnFailMsgWithLog(stream, Level::Out, "input stream is empty after postfix operator({})", op);
					auto args = ParseArgumentList<Result>(stream, parse);
					if (not args.has_value())
					{
						Log(Level::Out, "ParseArgumentList failed: {}", args.error().Message);
						return args;
					}
					MoveOrReturnFailMsgWithLog(stream, Level::Out,"input stream is empty after parsing argument expression list");
					if (stream.Current().Type != TokType::Punctuator_RightParen)
					{
						ReturnFailMsg(Level::Out, "expect right parenthesis after parsing argument expression list, but got {}", stream.Current());
					}
					lhs = Cons(op, array{ move(lhs.value()), move(args.value()) });
					break;
				}
				case TokType::Punctuator_LeftBracket:
				{
					MoveOrReturnFailMsgWithLog(stream, Level::Out, "input stream is empty after postfix operator({})", op);
					auto indexExp = ParseExpression<Result>(stream, Power::MinLevel, parse);
					if (not indexExp.has_value())
					{
						Log(Level::Out, "ParseExpression for index failed: {}", indexExp.error().Message);
						return indexExp;
					}
					MoveOrReturnFailMsgWithLog(stream, Level::Out,"input stream is empty after parsing index expression");
					if (stream.Current().Type != TokType::Punctuator_RightBracket)
					{
						ReturnFailMsg(Level::Out, "expect right bracket after parsing index expression, but got {}", stream.Current());
					}
					lhs = Cons(op, array{ move(lhs.value()), move(indexExp.value()) });
					break;
				}
				default:
					lhs = Cons(op, array{ move(lhs.value()) });
					break;
				}
				continue;
			}
			if (InfixOperators.contains(op.Type))
			{
				auto [lbp, rbp] = InfixBindingPower(op.Type);
				if (lbp <= minBindingPower)
				{
					// same as above
					stream.Rollback();
					break;
				}
				switch (op.Type)
				{
				case TokType::Punctuator_Question:
				{
					MoveOrReturnFailMsgWithLog(stream, Level::Out, "input stream is empty after question operator({})", op);
					auto leftExp = ParseExpression<Result>(stream, Power::MinLevel, parse);
					if (not leftExp.has_value())
					{
						Log(Level::Out, "parse left value of 3-operands expression failed: {}", leftExp.error().Message);
						return leftExp;
					}
					MoveOrReturnFailMsgWithLog(stream, Level::Out, "input stream is empty after parsing question mark of 'expression?'")
					auto const& colon = stream.Current();
					if (colon.Type != TokType::Punctuator_Colon)
					{
						ReturnFailMsg(Level::Out, "expect colon after parsing 'a?b', but got {}", colon);
					}
					MoveOrReturnFailMsgWithLog(stream, Level::Out, "input stream is empty after parsing 'a?b:'");
					auto rightExp = ParseExpression<Result>(stream, rbp, parse);
					if (not rightExp.has_value())
					{
						Log(Level::Out, "parse right value of 3-operands expression failed: {}", rightExp.error().Message);
						return rightExp;
					}
					lhs = Cons(op, array{ move(lhs.value()), move(leftExp.value()), move(rightExp.value()) });
					continue;
				}
				default:
				{
					MoveOrReturnFailMsgWithLog(stream, Level::Out, "input stream is empty after infix operator({})", op);
					auto rhs = ParseExpression<Result>(stream, rbp, parse);
					if (not rhs.has_value())
					{
						Log(Level::Out, "parse right value of infix expression failed: {}", rhs.error().Message);
						return rhs;
					}
					lhs = Cons(op, array{ move(lhs.value()), move(rhs.value()) });
					continue;
				}
				}
			}
			stream.Rollback();
			break;
		}
		ReturnWithLog(lhs, Level::Out, "expression");
	}

	template <typename Result, template <typename> class ActualStream, typename Tok>
	auto ParsePrefix(ActualStream<Tok>& stream, auto parse) const -> ParserResult<SyntaxTreeNode<Tok, Result>>
	{
		using ParserCombinator::SelectRight;
		using ParserCombinator::SelectLeft;
		using ParserCombinator::Concat;
		using ParserCombinator::Or;
		using ParserCombinator::Nullize;
		using ParserCombinator::ParseWord;
		using ParserCombinator::From;
		using ParserCombinator::Id;

		Log(Level::In, "ParsePrefix");
		auto const& op = stream.Current();
		if (PrefixOperators.contains(op.Type))
		{
			auto leftParen = ParseWord(TokType::Punctuator_LeftParen, Nullize);
			auto rightParen = ParseWord(TokType::Punctuator_RightParen, Nullize);
			auto typeName = [&](auto& stream) -> ParserResult<SyntaxTreeNode<Tok, Result>>
			{
				MoveOrReturnFailMsg(stream, "input stream is empty after prefix operator({})", op);
				auto typeNode = parse("type-name", stream);
				// add fail log, add branch support of HtmlLogger for Or
				Log(Level::Here, "stream pos: {}, call external parser parsing type-name: {}", stream.CurrentPosition(), typeNode);
				return typeNode;
			};
			auto ExpOf = [&](int bindingPower)
			{
				return [bindingPower, this, &parse](auto& stream) -> ParserResult<SyntaxTreeNode<Tok, Result>>
				{
					MoveOrReturnFailMsg(stream, "input stream is empty before parsing expression");
					return ParseExpression<Result>(stream, bindingPower, parse);
				};
			};
			auto BindOpCons = [](Tok op)
			{
				return [op](auto node) { return Cons(move(op), array{ move(node) }); };
			};

			switch (op.Type)
			{
			case TokType::Keyword_Sizeof:
			case TokType::Keyword_Alignof:
			{
				auto typeNameOrExpression = Or(
					BindOpCons(op),
					From(leftParen).RightWith(Or(Id, typeName, ExpOf(Power::MinLevel)), SelectRight).RightWith(rightParen, SelectLeft),
					ExpOf(PrefixBindingPower(op.Type))
				);
				auto exp = typeNameOrExpression(stream);
				ReturnWithLog(exp, Level::Out, "sizeof/alignof");
			}
			case TokType::Punctuator_LeftParen:
			{
				auto typeCastExp = From(typeName)
					.RightWith(rightParen, SelectLeft)
					.RightWith(ExpOf(PrefixBindingPower(op.Type)), ConsTypeCastExp<Result, Tok>);
				auto parenExp = From(ExpOf(Power::MinLevel)).RightWith(rightParen, SelectLeft);
				auto typeCastOrParenExp = Or(Id, typeCastExp, parenExp);
				auto exp = typeCastOrParenExp(stream);
				ReturnWithLog(exp, Level::Out, "type-cast/paren-expression");
			}
			default:
			{
				auto rbp = PrefixBindingPower(op.Type);
				MoveOrReturnFailMsgWithLog(stream, Level::Out, "input stream is empty after prefix operator({})", op);
				auto rhs = ParseExpression<Result>(stream, rbp, parse);
				if (not rhs.has_value())
				{
					ReturnWithLog(rhs, Level::Out, "prefix-expression fail");
				}
				auto prefix = Cons(op, array{ move(rhs.value()) });
				ReturnWithLog(prefix, Level::Out, "prefix-expression");
			}
			}
		}

		switch (op.Type)
		{
		case TokType::Identifier:
			if (IsTypeName(op.Value))
			{
				auto atom = ConsAtom<Result>(op, "type-name");
				ReturnWithLog(atom, Level::Out, "identifier as type-name of prefix");
			}
			[[fallthrough]];
		case TokType::IntegerConstant:
		case TokType::FloatingConstant:
		case TokType::CharacterConstant:
		case TokType::StringLiteral:
		{
			auto atom = ConsAtom<Result>(op);
			ReturnWithLog(atom, Level::Out, "literal of prefix");
		}
		case TokType::Keyword_Struct:
			// sometimes, its form like "struct X" TODO
			// TODO
		default:
			if (IsBuiltinTypeName(op.Type))
			{
				auto atom = ConsAtom<Result>(op, "builtin-type-name");
				ReturnWithLog(atom, Level::Out, "builtin-type-name");
			}
			ReturnFailMsg(Level::Out, "unexpected token({}) when parse prefix expression", op);
		}
	}

	template <typename Result, template <typename> class ActualStream, typename Tok>
	auto ParseArgumentList(ActualStream<Tok>& stream, auto parse) const -> ParserResult<SyntaxTreeNode<Tok, Result>>
	{
		auto args = SyntaxTreeNode<Tok, Result>("args", {});
		// for empty args function call
		if (stream.Current().Type == TokType::Punctuator_RightParen)
		{
			stream.Rollback();
			return args;
		}
		for (auto i = 0;; ++i)
		{
			// don't call Parse method here which will pack the parse callback, it will compile time recursive
			auto arg = ParseExpression<Result>(stream, ExpressionParseConfigs.at("assignment-expression").MinInfixBindingPower, parse);
			if (not arg.has_value())
			{
				return arg;
			}
			args.ChildSymbols.push_back(String(format("arg-{}", i)));
			args.Children.push_back(move(arg.value()));
			MoveOrReturnFailMsg(stream, "input stream is empty after parsing argument expression");
			auto const& next = stream.Current();
			switch (next.Type)
			{
			case TokType::Punctuator_Comma:
				MoveOrReturnFailMsg(stream, "input stream is empty after parsing comma of argument list");
				continue;
			case TokType::Punctuator_RightParen:
				stream.Rollback();
				return args;
			default:
				return unexpected(ParseFailResult{ .Message = format("expect comma or right parenthesis after parsing argument expression, but got {}", next) });
			}
		}
	}

	static auto InfixBindingPower(TokType op) -> pair<int, int>
	{
		if (InfixOperators.contains(op))
		{
			return { InfixOperators.at(op).LeftBindingPower, InfixOperators.at(op).RightBindingPower };
		}

		throw std::out_of_range(format("unknown infix operator({}) when get the binding power", op));
	}

	static auto PrefixBindingPower(TokType op) -> int
	{
		if (PrefixOperators.contains(op))
		{
			return PrefixOperators.at(op).RightBindingPower;
		}
		
		throw std::out_of_range(format("unknown prefix operator({}) when get the binding power", op));
	}

	static auto PostfixBindingPower(TokType op) -> int
	{
		if (PostfixOperators.contains(op))
		{
			return PostfixOperators.at(op).LeftBindingPower;
		}

		throw std::out_of_range(format("unknown postfix operator({}) when get the binding power", op));
	}

	static auto IsBuiltinTypeName(TokType type) -> bool
	{
		switch (type)
		{
		case TokType::Keyword_Void:
		case TokType::Keyword_Char:
		case TokType::Keyword_Short:
		case TokType::Keyword_Int:
		case TokType::Keyword_Long:
		case TokType::Keyword_Float:
		case TokType::Keyword_Double:
		case TokType::Keyword_Signed:
		case TokType::Keyword_Unsigned:
			return true;
		default:
			return false;
		}
	}

	static auto IsTypeName(std::string const& typeStr) -> bool
	{
		// TODO
		return false;/*from context it's a type*/
	}

	template <typename Tok, typename Result>
	static auto IsTypeCastHeader(SyntaxTreeNode<Tok, Result> const& node)
	{
		return node.Name == "type-name";
	}

	template <typename Tok, typename Result, size_t Size>
	static auto Cons(Tok op, array<SyntaxTreeNode<Tok, Result>, Size> items) -> SyntaxTreeNode<Tok, Result>
	{
		auto n = SyntaxTreeNode<Tok, Result>(String(format("{}-expression", op.Type)), { String(format("{}", op.Type)) });
		n.Children.push_back(move(op));

		for (auto i = 0; auto& x : items)
		{
			n.ChildSymbols.push_back(String(format("{}-operand", i++)));
			if (x.Name == "atom" or x.Name == "builtin-type-name")
			{
				n.Children.push_back(move(x.GetChildAsToken(0)));
				continue;
			}
			n.Children.push_back(move(x));			
		}
		return n;
	}

	template <typename Result, typename Tok>
	static auto ConsAtom(Tok atom, String name = "atom") -> SyntaxTreeNode<Tok, Result>
	{
		auto n = SyntaxTreeNode<Tok, Result>(name, { "value" });
		n.Children.push_back(move(atom));
		return n;
	}

	template <typename Result, typename Tok>
	static auto ConsTypeCastExp(SyntaxTreeNode<Tok, Result> type, SyntaxTreeNode<Tok, Result> value) -> SyntaxTreeNode<Tok, Result>
	{
		auto n = SyntaxTreeNode<Tok, Result>("type-cast-expression", { "type", "value" });
		n.Children.push_back(move(type));
		n.Children.push_back(move(value));
		return n;
	}

	template <typename... Args>
	auto Log(Level indent, std::format_string<Args...> format, Args&&... args) const -> void
	{
		if (logger != nullptr)
		{
			logger->Log(indent, format, std::forward<Args>(args)...);
		}
	}
};

export
{
	class ExpressionParser;
}