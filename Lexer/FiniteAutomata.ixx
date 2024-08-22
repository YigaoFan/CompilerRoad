export module FiniteAutomata;

import std;
import Graph;

using std::array;
using std::size_t;
using std::map;
using std::string;
using std::string_view;
using std::vector;
using std::isdigit;
using std::isalpha;

template <size_t StatesCount>
class FiniteAutomata
{
private:
    Graph transitionTable;
    array<bool, StatesCount> tokenType;
    //map<char, int> classificationTable;

public:
    constexpr FiniteAutomata()
    { }
};

// follow priority order to run FiniteAutomata combination, parentheses, closure, concatenation and alternation
template <size_t SizeA, size_t SizeB>
auto Or(FiniteAutomata<SizeA> a, FiniteAutomata<SizeB> b) -> FiniteAutomata<SizeA + SizeB + 2>
{

}

template <size_t Size>
auto ZeroOrMore(FiniteAutomata<Size> a) -> FiniteAutomata<Size + 2>
{

}

template <size_t SizeA, size_t SizeB>
auto Concat(FiniteAutomata<SizeA> a, FiniteAutomata<SizeB> b) -> FiniteAutomata<SizeA + SizeB>
{

}

consteval auto Convert2PostfixForm(string_view regExp) -> vector<char>
{
    // TODO process ()
    auto priority = [](char op)
    {
        switch (op)
        {
        case '*': return 3;
        case '+': return 2;
        case '|': return 1;
        }
    };
    auto isOperator = [](char c) { return string_view("*+|").contains(c); };
    auto output = vector<char>();
    auto operators = vector<char>();
    auto addOperator = [&](char op) -> void
    {
        for (;;)
        {
            if (!operators.empty())
            {
                if (auto lastOp = operators[operators.size() - 1]; priority(op) <= priority(lastOp))
                {
                    output.push_back(lastOp);
                    operators.pop_back();
                    continue;
                }
            }

            operators.push_back(op);
            break;
        }
    };
    
    for (size_t i = 0, len = regExp.length(); i < len; i++)
    {
        auto c = regExp[i];
        if (isOperator(c))
        {
            addOperator(c);
        }
        else
        {
            output.push_back(c);
            if (i != len - 1 && not isOperator(regExp[i + 1]))
            {
                addOperator('+');
            }
        }
    }

    for (auto i = operators.crbegin(); i != operators.crend(); i++)
    {
        output.push_back(*i);
    }

    return output;
}


// how to run it at compile time
/*
  assume regExp is valid regular expression
*/
template <size_t Size>
consteval auto ConsFAFrom(string_view regExp) -> FiniteAutomata<Size>
{
    if (regExp.starts_with("(") && regExp.ends_with(")"))
    {
        regExp = regExp.substr(1, regExp.length() - 2);
    }

    if (regExp.length() == 1)
    {
        return FiniteAutomata<2>(); // todo complete
    }
    auto postfixRegExp = Convert2PostfixForm(regExp);
    auto faStack = vector<FiniteAutomata<2>>();
    for (size_t l = postfixRegExp.length(), i = 0; i < l; i++)
    {
        auto c = regExp[i];
        // how to get out the highest operator from regExp
        switch (c)
        {
        case '*':
            // maybe exist parens in previous part, so not only length 1
            return ZeroOrMore(ConsFAFrom(regExp.substr(i - 1, 1));
        case '|':
            return Or(ConsFAFrom(regExp.substr(0, i)), ConsFAFrom(regExp.substr(i + 1)));
        default:
            // concat
            break;
        }
    }
}