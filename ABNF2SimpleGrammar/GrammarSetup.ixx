export module GrammarSetup;

import std;
import TokType;
import Lexer;
import Parser;

using std::string;
using std::pair;

auto BuildLexer() -> Lexer<TokType>
{

    std::array rules =
    {
        pair<TokType, string>{ TokType::StarArrow, "\\-\\*\\->" },
        pair<TokType, string>{ TokType::Arrow, "\\->" },
        pair<TokType, string>{ TokType::At, "@" },
        pair<TokType, string>{ TokType::LeftAngle, "<" },
        pair<TokType, string>{ TokType::RightAngle, ">" },
        pair<TokType, string>{ TokType::LeftSquare, "\\[" },
        pair<TokType, string>{ TokType::RightSquare, "\\]" },
        pair<TokType, string>{ TokType::LeftBracket, "{" },
        pair<TokType, string>{ TokType::RightBracket, "}" },
        pair<TokType, string>{ TokType::StarMark, "\\*" },
        pair<TokType, string>{ TokType::PipeMark, "\n?( |\t)*\\|" },
        pair<TokType, string>{ TokType::Whitespace, " |\t" },
        pair<TokType, string>{ TokType::Symbol, "[a-zA-Z][a-zA-Z0-9_\\-]*" },
        pair<TokType, string>{ TokType::Number, "[1-9][0-9]*" },
        pair<TokType, string>{ TokType::Comment, ";[^\n]*" },
        pair<TokType, string>{ TokType::Hyphen, "\\-" },
        pair<TokType, string>{ TokType::LeftParen, "\\(" },
        pair<TokType, string>{ TokType::RightParen, "\\)" },
        pair<TokType, string>{ TokType::Newline, "\n" },
        pair<TokType, string>{ TokType::Terminal, "\"((\\\\[^\n])|[^\\\\\"\n])*\"" },
        pair<TokType, string>{ TokType::QutotedDigitOrAlphabet, "'[a-zA-Z0-9]'" },
        pair<TokType, string>{ TokType::RegularExpression, "r\"((\\\\[^\n])|[^\\\\\"\n])*\"" },
        pair<TokType, string>{ TokType::LexRuleHeader, "\\- Lex \\-\n" },
        pair<TokType, string>{ TokType::ParseRuleHeader, "\\- Parse \\-\n" },
        pair<TokType, string>{ TokType::StartKeyword, "start" },
        pair<TokType, string>{ TokType::EofKeyword, "EOF" },
    };
    auto l = Lexer<TokType>::New(rules);
    return l;
}

auto BuildParser() -> LLParser
{
	auto p = LLParser::ConstructFrom("all-grammars",
		{// how to represent empty in current grammar, use ()
			{ "all-grammars", {
				{ "lex-header", "grammars", "parse-header", "start-info", "grammars", "EOF"},
			}},
			{ "start-info", {
				{ "@", "start", "sym", "eofKeyword"},
			}},
			{ "grammars", { // cannot place newlines if lex or parse part is empty
				{ "optional-newlines", "grammar", "more-grammars", },
				{ },
			}},
			{ "more-grammars", {
				{ "newlines", "grammar", "more-grammars"},
				{ "newlines" },
				{ },
			}},
			{ "grammar", {
				{ "sym", "arrow", "productions" },
			}},
			{ "arrow", {
				{ "->" },
				{ "-*->" },
			}},
			{ "optional-newlines", {
				{ "newlines", },
				{ },
			}},
			{ "newlines", {
				{ "newline", "more-newlines" },
			}},
			{ "more-newlines", {
				{ "newline", "more-newlines" },
				{ },
			}},
			{ "productions", {
				{ "production", "more-productions" },
				{ },
			}},
			{ "more-productions", {
				{ "|", "productions" },
				{ },
			}},
			{ "production", { // once production occurs, it means not empty, so production doesn't have {} right side
				{ "item", "more-items" }, // use () to represent empty
			}},
			{ "more-items", {
				{ "item", "more-items" },
				{ },
			}},
			{ "item", {
				{ "item_0" },
				{ "*", "item_0" },
				{ "num", "*", "num", "item_0" },
				{ "num", "*", "item_0" },
			}},
			{ "item_0", {
				{ "regExp" },
				{ "terminal", "@", "sym" },
				{ "terminal" },
				{ "sym" },
				{ "digitOrAlphabet", "-", "digitOrAlphabet" },
				{ "[", "productions", "]" },
				{ "(", "productions", ")" },
			}},
		},
	{
		{ "->", static_cast<int>(TokType::Arrow) },
		{ "-*->", static_cast<int>(TokType::StarArrow) },
		{ "@", static_cast<int>(TokType::At) },
		{ "|", static_cast<int>(TokType::PipeMark) },
		{ "<", static_cast<int>(TokType::LeftAngle) },
		{ ">", static_cast<int>(TokType::RightAngle) },
		{ "(", static_cast<int>(TokType::LeftParen) },
		{ ")", static_cast<int>(TokType::RightParen) },
		{ "[", static_cast<int>(TokType::LeftSquare) },
		{ "]", static_cast<int>(TokType::RightSquare) },
		{ "{", static_cast<int>(TokType::LeftBracket) },
		{ "}", static_cast<int>(TokType::RightBracket) },
		{ "*", static_cast<int>(TokType::StarMark) },
		{ "|", static_cast<int>(TokType::PipeMark) },
		{ "sym", static_cast<int>(TokType::Symbol) },
		{ "num", static_cast<int>(TokType::Number) },
		{ "-", static_cast<int>(TokType::Hyphen) },
		{ "comment", static_cast<int>(TokType::Comment) },
		{ "EOF", static_cast<int>(TokType::EOF)},
		{ "newline" , static_cast<int>(TokType::Newline) },
		{ "terminal" , static_cast<int>(TokType::Terminal) },
		{ "digitOrAlphabet" , static_cast<int>(TokType::QutotedDigitOrAlphabet) },
		{ "regExp" , static_cast<int>(TokType::RegularExpression) },
		{ "lex-header" , static_cast<int>(TokType::LexRuleHeader) },
		{ "parse-header" , static_cast<int>(TokType::ParseRuleHeader) },
		{ "start" , static_cast<int>(TokType::StartKeyword) },
		{ "eofKeyword" , static_cast<int>(TokType::EofKeyword) }, // represent the word EOF in abnf file
	});
    return p;
}

export
{
	auto BuildLexer() -> Lexer<TokType>;
	auto BuildParser() -> LLParser;
}