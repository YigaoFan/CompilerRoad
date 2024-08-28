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
using std::map;
using std::isdigit;
using std::isalpha;
using std::move;
using std::format;

class FiniteAutomata
{
private:
    constexpr static int epsilon = 0;
    State startState;
    vector<State> acceptingStates;
    Graph<char> transitionTable;
    vector<pair<State, bool>> tokenType; // need it?
    //vector<pair<char, int>> classificationTable; // use it when refine

public:
    static auto From(char c) -> FiniteAutomata
    {
        auto transitionTable = Graph<char>();
        auto s0 = transitionTable.AllocateState();
        auto s1 = transitionTable.AllocateState();
        transitionTable.AddTransition(s0, c, s1);

        auto tokenType = vector<pair<State, bool>>();
        tokenType.push_back({ s0, false });
        tokenType.push_back({ s1, true });

        return FiniteAutomata(s0, { s1 }, move(transitionTable), move(tokenType));
    }

    // follow priority order to run FiniteAutomata combination, parentheses, closure, concatenation and alternation
    static auto Or(FiniteAutomata a, FiniteAutomata b) -> FiniteAutomata
    {
        auto newStart = a.transitionTable.AllocateState();
        auto newAccept = a.transitionTable.AllocateState();

        auto offset = a.transitionTable.Merge(move(b.transitionTable));
        a.transitionTable.AddTransition(newStart, epsilon, a.startState);
        a.transitionTable.AddTransition(newStart, epsilon, b.startState + offset);

        for (auto accept : a.acceptingStates)
        {
            a.transitionTable.AddTransition(accept, epsilon, newAccept);
        }
        for (auto accept : b.acceptingStates)
        {
            a.transitionTable.AddTransition(accept + offset, epsilon, newAccept);
        }

        a.startState = newStart;
        a.acceptingStates = { newAccept };
        return a;
    }

    static auto ZeroOrMore(FiniteAutomata a) -> FiniteAutomata
    {
        auto newStart = a.transitionTable.AllocateState();
        auto newAccept = a.transitionTable.AllocateState();

        a.transitionTable.AddTransition(newStart, epsilon, a.startState);

        for (auto accept : a.acceptingStates)
        {
            a.transitionTable.AddTransition(accept, epsilon, a.startState);
            a.transitionTable.AddTransition(accept, epsilon, newAccept);
        }

        a.startState = newStart;
        a.acceptingStates = { newAccept };
        return a;
    }

    static auto Concat(FiniteAutomata a, FiniteAutomata b) -> FiniteAutomata
    {
        auto offset = a.transitionTable.Merge(move(b.transitionTable));
        for (auto accept : a.acceptingStates)
        {
            a.transitionTable.AddTransition(accept, epsilon, b.startState + offset);
        }

        for (auto& accept : b.acceptingStates)
        {
            accept += offset;
        }
        a.acceptingStates = move(b.acceptingStates);
        return a;
    }

    static auto Range(char a, char b) -> FiniteAutomata
    {
        auto transitionTable = Graph<char>();
        auto tokenType = vector<pair<size_t, bool>>();
        auto start = transitionTable.AllocateState();
        auto accept = transitionTable.AllocateState();
        tokenType.push_back({ start, false });
        tokenType.push_back({ accept, true });

        // TODO check a and b if it's continuous
        for (auto i = a; i <= b; i++)
        {
            auto n0 = transitionTable.AllocateState();
            auto n1 = transitionTable.AllocateState();
            tokenType.push_back({ n0, false });
            tokenType.push_back({ n1, false });

            transitionTable.AddTransition(start, epsilon, n0);
            transitionTable.AddTransition(n0, i, n1);
            transitionTable.AddTransition(n1, epsilon, accept);
        }

        return FiniteAutomata(start, { accept }, move(transitionTable), move(tokenType));
    }

    // the below constexpr is must? TODO compile check
    constexpr FiniteAutomata(State startState, vector<State> acceptingStates, Graph<char> transitionTable, vector<pair<State, bool>> tokenType)
        : startState(startState), acceptingStates(move(acceptingStates)), transitionTable(move(transitionTable)), tokenType(move(tokenType))
    { }
private:
    //auto GetColumnIndex(char c) const -> int
    //{
    //    for (auto const& x : classificationTable)
    //    {
    //        if (c == x.first)
    //        {
    //            return x.second;
    //        }
    //    }
    //    throw std::out_of_range(format("not found {} in" nameof(classificationTable), c));
    //}
};

auto Convert2PostfixForm(string_view regExp) -> vector<char>
{
    auto priority = [](char op)
    {
        switch (op)
        {
        case '*': return 3;
        case '+': return 2;
        case '-': return 2;
        case '|': return 1;
        }
    };
    auto isOperatorOrEndScope = [](char c) { return string_view("*+|-])").contains(c); };
    auto output = vector<char>();
    auto operators = vector<char>();
    auto addOperator = [&](char op) -> void
    {
        for (;;)
        {
            if (not operators.empty())
            {
                if (auto lastOp = operators.back(); lastOp != '(' and lastOp != '[')
                {
                    if (priority(op) <= priority(lastOp))
                    {
                        output.push_back(lastOp);
                        operators.pop_back();
                        continue;
                    }
                }
            }

            operators.push_back(op);
            break;
        }
    };
    
    for (size_t i = 0, len = regExp.length(); i < len; i++)
    {
        auto c = regExp[i];
        switch (c)
        {
        // expand the operators
        case '*':
            addOperator(c);
            goto addPossibleRelationWithNextChar;
        case '+':
        case '|':
        case '-':
            addOperator(c);
            break;
        case '[':
            operators.push_back('[');
            break;
        case ']':
            for (;;)
            {
                auto op = operators.back();
                operators.pop_back();
                if (op == '[')
                {
                    goto addPossibleRelationWithNextChar;
                }
                output.push_back(op);
            }
            break;
        case '(':
            operators.push_back('(');
            break;
        case ')':
            for (;;)
            {
                auto op = operators.back();
                operators.pop_back();
                if (op == '(')
                {
                    goto addPossibleRelationWithNextChar;
                }
                output.push_back(op);
            }
            break;
        default:
            output.push_back(c);
        addPossibleRelationWithNextChar:
            if (i != len - 1 && not isOperatorOrEndScope(regExp[i + 1]))
            {
                if (std::any_of(operators.cbegin(), operators.cend(), [](char ch) { return '[' == ch; }))
                {
                    addOperator('|');
                }
                else
                {
                    addOperator('+');
                }
            }
        }
    }

    for (auto i = operators.rbegin(); i != operators.rend(); i++)
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
    // support [0-9a-z] (specific times closure)
    if (regExp.starts_with("(") && regExp.ends_with(")"))
    {
        regExp = regExp.substr(1, regExp.length() - 2);
    }

    if (regExp.length() == 1)
    {
        return FiniteAutomata::From(regExp[0]);
    }
    auto postfixRegExp = Convert2PostfixForm(regExp);
    auto faStack = vector<FiniteAutomata>();
    auto charStack = vector<char>();
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
            auto a = move(charStack.back());
            charStack.pop_back();
            auto b = move(charStack.back());
            charStack.pop_back();
            faStack.push_back(FiniteAutomata::Range(a, b));
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
            faStack.push_back(FiniteAutomata::From(c));
            break;
        }
    }
    if (faStack.size() != 1)
    {
        throw std::logic_error(nameof(faStack)" doesn't have one finite automata as final result");
    }
    return move(faStack.front());
}

export
{
    auto Convert2PostfixForm(string_view regExp) -> vector<char>;
}