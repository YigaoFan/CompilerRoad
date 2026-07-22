export module ExpressionParser;

import std;
import Base;
import Parser;
import C11Spec;

using std::map;
using std::unexpected;
using std::array;
using std::pair;
using std::map;
using std::set;
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

class ExpressionParser
{
private:
	// TODO Assoc not use
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
		{ TokType::Punctuator_Assign,           {.LeftBindingPower = Power::Level2, .RightBindingPower = Power::Level3 - 1, }},
		{ TokType::Punctuator_MulAssign,        {.LeftBindingPower = Power::Level2, .RightBindingPower = Power::Level3 - 1, }},
		{ TokType::Punctuator_DivAssign,        {.LeftBindingPower = Power::Level2, .RightBindingPower = Power::Level3 - 1, }},
		{ TokType::Punctuator_ModAssign,        {.LeftBindingPower = Power::Level2, .RightBindingPower = Power::Level3 - 1, }},
		{ TokType::Punctuator_AddAssign,        {.LeftBindingPower = Power::Level2, .RightBindingPower = Power::Level3 - 1, }},
		{ TokType::Punctuator_SubAssign,        {.LeftBindingPower = Power::Level2, .RightBindingPower = Power::Level3 - 1, }},
		{ TokType::Punctuator_LeftShiftAssign,  {.LeftBindingPower = Power::Level2, .RightBindingPower = Power::Level3 - 1, }},
		{ TokType::Punctuator_RightShiftAssign, {.LeftBindingPower = Power::Level2, .RightBindingPower = Power::Level3 - 1, }},
		{ TokType::Punctuator_AndAssign,        {.LeftBindingPower = Power::Level2, .RightBindingPower = Power::Level3 - 1, }},
		{ TokType::Punctuator_OrAssign,         {.LeftBindingPower = Power::Level2, .RightBindingPower = Power::Level3 - 1, }},
		{ TokType::Punctuator_XorAssign,        {.LeftBindingPower = Power::Level2, .RightBindingPower = Power::Level3 - 1, }},
		
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
		//{ TokType::Punctuator_LeftParen,   {.LeftBindingPower = Power::Level15, }}, // function call
		{ TokType::Punctuator_LeftBracket, {.LeftBindingPower = Power::Level15, }},
		{ TokType::Punctuator_Dot,         {.LeftBindingPower = Power::Level15, }},
		{ TokType::Punctuator_Arrow,       {.LeftBindingPower = Power::Level15, }},
	};
public:
	auto FirstSet() const -> map<String, set<String>>
	{
		map<String, set<String>> firsts
		{
			{ "logical-or-expression", {} }, 
			{ "unary-expression", {} }, 
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
					firsts.at("logical-or-expression").insert(String(term));
					firsts.at("unary-expression").insert(String(term));
					firsts.at("assignment-expression").insert(String(term));
					break;
				}
			}
		}
		// atoms: primary-expression first
		firsts.at("logical-or-expression").insert("Identifier");
		firsts.at("logical-or-expression").insert("Integer-constant");
		firsts.at("logical-or-expression").insert("Floating-constant");
		firsts.at("logical-or-expression").insert("Character-constant");
		firsts.at("logical-or-expression").insert("StringLiteral");

		firsts.at("unary-expression").insert("Identifier");
		firsts.at("unary-expression").insert("Integer-constant");
		firsts.at("unary-expression").insert("Floating-constant");
		firsts.at("unary-expression").insert("Character-constant");
		firsts.at("unary-expression").insert("StringLiteral");

		firsts.at("assignment-expression").insert("Identifier");
		firsts.at("assignment-expression").insert("Integer-constant");
		firsts.at("assignment-expression").insert("Floating-constant");
		firsts.at("assignment-expression").insert("Character-constant");
		firsts.at("assignment-expression").insert("StringLiteral");

		return firsts;
	}

	auto Parsable(String nontermin) const -> bool
	{
		return nontermin == "logical-or-expression"
			or nontermin == "unary-expression"
			or nontermin == "assignment-expression";
		// all expression nonterminals handled by the Pratt parser
			/*nontermin == "primary-expression"
			or nontermin == "postfix-expression"
			
			or nontermin == "cast-expression"
			or nontermin == "multiplicative-expression"
			or nontermin == "additive-expression"
			or nontermin == "shift-expression"
			or nontermin == "relational-expression"
			or nontermin == "equality-expression"
			or nontermin == "AND-expression"
			or nontermin == "exclusive-OR-expression"
			or nontermin == "inclusive-OR-expression"
			or nontermin == "logical-AND-expression"
			or nontermin == "logical-OR-expression";*/
	}

	template <typename Result, template <typename> class ActualStream, IToken Tok>
		requires Stream<ActualStream, Tok>
	auto Parse(String nontermin, ActualStream<Tok>& stream) const -> ParserResult<SyntaxTreeNode<Tok, Result>>
	{
		if (not Parsable(nontermin))
		{
			return unexpected(ParseFailResult{ .Message = format("nonterminal({}) isn't parsable by ExpressionParser", nontermin) });
		}
		return ParseExpression<Result>(stream, Power::MinLevel);
	}
private:
	template <typename Result, template <typename> class ActualStream, typename Tok>
	auto ParseExpression(ActualStream<Tok>& stream, int minBindingPower) const -> ParserResult<SyntaxTreeNode<Tok, Result>>
	{
		auto lhs = ParsePrefix<Result>(stream);

		if (not lhs.has_value())
		{
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
				if (lbp < minBindingPower)
				{
					// rollback to let the remain expression parser can read this operator
					stream.Rollback();
					break;
				}
				lhs = Cons(op, array{ move(lhs.value()) });
				continue;
			}
			if (InfixOperators.contains(op.Type))
			{
				auto [lbp, rbp] = InfixBindingPower(op.Type);
				if (lbp < minBindingPower)
				{
					// same as above
					stream.Rollback();
					break;
				}
				if (not stream.MoveNext())
				{
					return unexpected(ParseFailResult{ .Message = format("input stream is empty after infix operator({})", op) });
				}
				auto rhs = ParseExpression<Result>(stream, rbp);
				if (not rhs.has_value())
				{
					return rhs;
				}
				lhs = Cons(op, array{ move(lhs.value()), move(rhs.value()) });
				continue;
			}
			stream.Rollback();
			break;
		}
		return lhs;
	}

	template <typename Result, template <typename> class ActualStream, typename Tok>
	auto ParsePrefix(ActualStream<Tok>& stream) const -> ParserResult<SyntaxTreeNode<Tok, Result>>
	{
		auto const& op = stream.Current();
		if (PrefixOperators.contains(op.Type))
		{
			if (op.Type == TokType::Punctuator_LeftParen)
			{
				if (not stream.MoveNext())
				{
					return unexpected(ParseFailResult{ .Message = format("input stream is empty after prefix operator({})", op) });
				}
				auto subExp = ParseExpression<Result>(stream, Power::MinLevel);
				if (not stream.MoveNext())
				{
					return unexpected(ParseFailResult{ .Message = format("input stream is empty after parsing '(expression'", op) });
				}
				auto const& rightParen = stream.Current();
				if (rightParen.Type != TokType::Punctuator_RightParen)
				{
					return unexpected(ParseFailResult{ .Message = format("expect right parenthesis after parsing '(expression', but got {}", rightParen) });
				}
				return subExp;
			}
			else
			{
				auto rbp = PrefixBindingPower(op.Type);
				if (not stream.MoveNext())
				{
					return unexpected(ParseFailResult{ .Message = format("input stream is empty after prefix operator({})", op) });
				}
				auto rhs = ParseExpression<Result>(stream, rbp);
				if (not rhs.has_value())
				{
					return rhs;
				}
				return Cons(op, array{ move(rhs.value()) });
			}
		}

		switch (op.Type)
		{
		case TokType::Identifier:
		case TokType::IntegerConstant:
		case TokType::FloatingConstant:
		case TokType::CharacterConstant:
		case TokType::StringLiteral:
		{
			auto atom = SyntaxTreeNode<Tok, Result>("atom", { "value" });
			atom.Children.push_back(op);
			return atom;
		}
		default:
			return unexpected(ParseFailResult{ .Message = format("unexpected token({}) when parse prefix expression", op) });
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

	template <typename Tok, typename Result, size_t Size>
	static auto Cons(Tok op, array<SyntaxTreeNode<Tok, Result>, Size> items) -> SyntaxTreeNode<Tok, Result>
	{
		auto n = SyntaxTreeNode<Tok, Result>(String(format("{}-expression", op.Type)), { String(format("{}", op.Type)) });
		n.Children.push_back(move(op));

		for (auto i = 0; auto& x : items)
		{
			n.ChildSymbols.push_back(String(format("{}-operand", i++)));
			if (x.Name == "atom")
			{
				n.Children.push_back(move(x.GetChildAsToken(0)));
				continue;
			}
			n.Children.push_back(move(x));			
		}
		return n;
	}
};

export
{
	class ExpressionParser;
}