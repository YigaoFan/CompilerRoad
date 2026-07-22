import std;
import Lexer;
import Parser;
import C11Spec;
import ConflictResolver;
import Context;
import ExpressionParser;

using std::ranges::views::filter;
using std::ranges::to;
using std::println;

int main()
{
    auto l = Lexer<TokType>::New(lexRules);
	auto toks = l.Lex("int main() { return 0; }") | filter([](auto& x) { return x.Type != TokType::Whitespace; }) | to<std::vector<Token<TokType>>>();

	println("Tokens count: {}", toks.size());

	String focus = "declaration";
	auto resolver = C11ConflictResolver();
	auto p = LLParser::ConstructFrom(focus, grammars, terminal2IntTokenType, resolver);
	ExpressionParser expParser;
	p.Parse<Context>(VectorStream{ .Tokens = move(toks) }, [](auto n) { /*std::println("encounter {}", n->Name);*/ }, {}, resolver, expParser);
	return 0;
}
