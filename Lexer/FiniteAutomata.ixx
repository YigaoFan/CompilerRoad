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
using std::move;

class FiniteAutomata
{
private:
    Graph transitionTable;
    vector<bool> tokenType;
    //map<char, int> classificationTable;

public:
    constexpr FiniteAutomata()
    { }
};

// follow priority order to run FiniteAutomata combination, parentheses, closure, concatenation and alternation
auto Or(FiniteAutomata a, FiniteAutomata b) -> FiniteAutomata
{

}

auto ZeroOrMore(FiniteAutomata a) -> FiniteAutomata
{

}

auto Concat(FiniteAutomata a, FiniteAutomata b) -> FiniteAutomata
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
                if (auto lastOp = operators.back(); priority(op) <= priority(lastOp))
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
consteval auto ConsFAFrom(string_view regExp) -> FiniteAutomata
{
    // support [0-9] [a-z] (specific times closure)
    if (regExp.starts_with("(") && regExp.ends_with(")"))
    {
        regExp = regExp.substr(1, regExp.length() - 2);
    }

    if (regExp.length() == 1)
    {
        return FiniteAutomata(); // todo complete
    }
    auto postfixRegExp = Convert2PostfixForm(regExp);
    auto faStack = vector<FiniteAutomata>();
    for (size_t l = postfixRegExp.size(), i = 0; i < l; i++)
    {
        auto c = postfixRegExp[i];
        // how to get out the highest operator from regExp
        switch (c)
        {
        case '+':
        {
            auto a = move(faStack.back());
            faStack.pop_back();
            auto b = move(faStack.back());
            faStack.pop_back();
            faStack.push_back(Concat(move(a), move(b)));
        }
        case '*':
        {
            auto a = move(faStack.back());
            faStack.pop_back();
            // maybe exist parens in previous part, so not only length 1
            faStack.push_back(ZeroOrMore(move(a)));
        }
        case '|':
        {
            auto a = move(faStack.back());
            faStack.pop_back();
            auto b = move(faStack.back());
            faStack.pop_back();
            faStack.push_back(Or(move(a), move(b)));
        }
        default:
            // number or alphabet
            break;
        }
    }
    if (faStack.size() != 1)
    {
        throw std::logic_error("Not have one finite automata as final result");
    }
    return move(faStack.front());
}