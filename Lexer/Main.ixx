import std;
import Lexer;
import FiniteAutomata;

using std::string;
using std::pair;
using std::vector;
using std::string_view;

enum class Token
{
    Id,
    Keyword,
};

int main()
{
    //ab-|+c|
    //std::array rules = { pair<string, Token>{ "[a-zA-Z][a-zA-Z0-9_]*", Token::Id } };
    //auto l = Lexer{ rules };
    //auto postfixExp = Convert2PostfixForm("[a-b]*d|c");
    auto postfixExp = Convert2PostfixForm("e[ab]*d|c"); // support this
    std::print("{}", string_view(postfixExp.begin(), postfixExp.end()));
    auto nfa = ConstructNFAFrom("e[ab]*d|c");
    std::print("{}", nfa);
    auto dfa = NFA2DFA(nfa);
    std::print("{}", dfa);
    std::cout << "OK";
}