import std;
import Lexer;
import Base;
import Parser;

using std::pair;
using std::string;

enum class Token : int
{
    Keyword,
    Id,
    Space,
    ClassName,
};

int main()
{
    std::array rules = 
    {
        pair<string, Token>{ "if|for", Token::Keyword },
        pair<string, Token>{ "[a-zA-Z][a-zA-Z0-9_]*", Token::Id },
        pair<string, Token>{ "[A-Z][a-zA-Z0-9]*", Token::ClassName },
        pair<string, Token>{ " ", Token::Space },
    };
    auto l = Lexer<Token>::New(rules);
    string code = "if ab0 for Hello";
    auto tokens = l.Lex(code);
    auto p = TableDrivenParser::ConstructFrom("program", { }, { });
    //p.Parse();
}