export module FiniteAutomata;

#include "Util.h"
import std;
import Graph;

using std::array;
using std::size_t;
using std::string;
using std::string_view;
using std::vector;
using std::pair;
using std::isdigit;
using std::isalpha;
using std::move;
using std::format;

class FiniteAutomata
{
private:
    Graph transitionTable;
    vector<bool> tokenType;
    vector<pair<char, int>> classificationTable;

public:
    static auto NewFrom(char c) -> FiniteAutomata
    {
        auto tokenType = vector<bool>();
        tokenType.push_back(false);
        tokenType.push_back(true);

        auto classificationTable = vector<pair<char, int>>();
        constexpr auto charKind = 1;
        classificationTable.push_back({ c, charKind });

        auto transitionTable = Graph();
        auto s0 = transitionTable.AllocateState();
        auto s1 = transitionTable.AllocateState();
        transitionTable.AddTransition(s0, charKind, s1);

        return FiniteAutomata(move(transitionTable), move(tokenType), move(classificationTable));
    }


    // follow priority order to run FiniteAutomata combination, parentheses, closure, concatenation and alternation
    static auto Or(FiniteAutomata a, FiniteAutomata const& b) -> FiniteAutomata
    {
        auto c = a.classificationTable + b.classificationTable;
    }

    static auto ZeroOrMore(FiniteAutomata a) -> FiniteAutomata
    {

    }

    static auto Concat(FiniteAutomata a, FiniteAutomata b) -> FiniteAutomata
    {

    }

    static auto Range(FiniteAutomata a, FiniteAutomata b) -> FiniteAutomata
    {

    }

    // the below constexpr is must? TODO compile check
    constexpr FiniteAutomata(Graph transitionTable, vector<bool> tokenType, vector<pair<char, int>> classificationTable)
        : transitionTable(move(transitionTable)), tokenType(move(tokenType)), classificationTable(move(classificationTable))
    { }




private:
    auto GetColumnIndex(char c) const -> int
    {
        for (auto const& x : classificationTable)
        {
            if (c == x.first)
            {
                return x.second;
            }
        }
        throw std::out_of_range(format("not found {} in" nameof(classificationTable), c));
    }
};

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
consteval auto ConstructFAFrom(string_view regExp) -> FiniteAutomata
{
    // support [0-9] [a-z] (specific times closure)
    if (regExp.starts_with("(") && regExp.ends_with(")"))
    {
        regExp = regExp.substr(1, regExp.length() - 2);
    }

    if (regExp.length() == 1)
    {
        return FiniteAutomata::NewFrom(regExp[0]);
    }
    auto postfixRegExp = Convert2PostfixForm(regExp);
    auto faStack = vector<FiniteAutomata>();
    for (size_t l = postfixRegExp.size(), i = 0; i < l; i++)
    {
        auto c = postfixRegExp[i];
        switch (c)
        {
        case '+':
        {
            auto a = move(faStack.back());
            faStack.pop_back();
            auto b = move(faStack.back());
            faStack.pop_back();
            faStack.push_back(FiniteAutomata::Concat(move(a), move(b)));
        }
        case '-':
        {
            auto a = move(faStack.back());
            faStack.pop_back();
            auto b = move(faStack.back());
            faStack.pop_back();
            faStack.push_back(FiniteAutomata::Range(move(a), move(b)));
        }
        case '*':
        {
            auto a = move(faStack.back());
            faStack.pop_back();
            faStack.push_back(FiniteAutomata::ZeroOrMore(move(a)));
        }
        case '|':
        {
            auto a = move(faStack.back());
            faStack.pop_back();
            auto b = move(faStack.back());
            faStack.pop_back();
            faStack.push_back(FiniteAutomata::Or(move(a), move(b)));
        }
        default:
            // number or alphabet
            faStack.push_back(FiniteAutomata::NewFrom(c));
            break;
        }
    }
    if (faStack.size() != 1)
    {
        throw std::logic_error(nameof(faStack)" doesn't have one finite automata as final result");
    }
    return move(faStack.front());
}