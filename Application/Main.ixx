import std;
import Lexer;
import Base;
import Parser;

using std::pair;
using std::string;

enum class Token : int
{
    Id,
    Keyword,
};

int main()
{
    std::array rules = { pair<string, Token>{ "[a-zA-Z][a-zA-Z0-9_]*", Token::Id } };
    auto l = Lexer<Token>::New(rules);
    string code;
    auto tokens = l.Lex(code);
    auto p = TableDrivenParser::ConstructFrom("program", { }, { });
    //p.Parse();
}