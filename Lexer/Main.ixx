import std;
import Lexer;
import FiniteAutomata;

using std::string;
using std::pair;
using std::vector;
using std::string_view;
using std::print;
using std::println;

enum class Token
{
    Id,
    Keyword,
};

int main()
{
    //ab-|+c|
    //auto postfixExp = Convert2PostfixForm("[a-b]*d|c");
    //auto postfixExp = Convert2PostfixForm("e[ab]*d|c"); // support this
    //println("{}", string_view(postfixExp.begin(), postfixExp.end()));
    //auto nfa = ConstructNFAFrom("e[ab]*d|c");
    ////auto nfa = ConstructNFAFrom("e[a-z]*d|c");// are you ok?
    //print("{}", nfa);
    //auto dfa = NFA2DFA(nfa);
    //print("{}", dfa);
    //auto mdfa = Minimize<false>(dfa);
    //print("{}", mdfa);
    //std::cout << "OK";
    //std::array rules = { pair<string, Token>{ "[a-zA-Z][a-zA-Z0-9_]*", Token::Id } };
    std::array rules = { pair<string, Token>{ "[a-zA-Z][0-9_]*", Token::Id } };
    auto l = Lexer<Token>::New(rules);
    //print("{}", l.dfa);
}