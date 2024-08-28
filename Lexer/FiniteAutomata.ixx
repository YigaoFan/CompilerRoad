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
using std::variant;
using std::set;
using std::queue;
using std::optional;
using std::isdigit;
using std::isalpha;
using std::move;
using std::format;

class FiniteAutomata
{
private:
    friend auto NFA2DFA(FiniteAutomata nfa) -> FiniteAutomata;
    constexpr static int epsilon = 0;
    State startState;
    vector<State> acceptingStates;
    Graph<char> transitionTable;
    //vector<pair<State, bool>> tokenType; // need it?
    //vector<pair<char, int>> classificationTable; // use it when refine

public:
    static auto From(char c) -> FiniteAutomata
    {
        auto transitionTable = Graph<char>();
        auto s0 = transitionTable.AllocateState();
        auto s1 = transitionTable.AllocateState();
        transitionTable.AddTransition(s0, c, s1);

        return FiniteAutomata(s0, { s1 }, move(transitionTable));
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
        auto start = transitionTable.AllocateState();
        auto accept = transitionTable.AllocateState();

        // TODO check a and b if it's continuous
        for (auto i = a; i <= b; i++)
        {
            auto n0 = transitionTable.AllocateState();
            auto n1 = transitionTable.AllocateState();

            transitionTable.AddTransition(start, epsilon, n0);
            transitionTable.AddTransition(n0, i, n1);
            transitionTable.AddTransition(n1, epsilon, accept);
        }

        return FiniteAutomata(start, { accept }, move(transitionTable));
    }

    // the below constexpr is must? TODO compile check
    constexpr FiniteAutomata(State startState, vector<State> acceptingStates, Graph<char> transitionTable)
        : startState(startState), acceptingStates(move(acceptingStates)), transitionTable(move(transitionTable))
    { }

    auto Run(State from, char input) const -> optional<State>
    {
        
    }
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
    auto Priority = [](char op)
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
    auto AddOperator = [&](char op) -> void
    {
        for (;;)
        {
            if (not operators.empty())
            {
                if (auto lastOp = operators.back(); lastOp != '(' and lastOp != '[')
                {
                    if (Priority(op) <= Priority(lastOp))
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
        case '*':
            AddOperator(c);
            goto addPossibleRelationWithNextChar;
        case '+':
        case '|':
        case '-':
            AddOperator(c);
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
                    AddOperator('|');
                }
                else
                {
                    AddOperator('+');
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

/// <summary>
/// assume regExp is valid regular expression
/// </summary>
auto ConstructNFAFrom(string_view regExp) -> FiniteAutomata
{
    // support [0-9a-z]
    if (regExp.starts_with("(") && regExp.ends_with(")"))
    {
        regExp = regExp.substr(1, regExp.length() - 2);
    }

    if (regExp.length() == 1)
    {
        return FiniteAutomata::From(regExp[0]);
    }
    auto postfixRegExp = Convert2PostfixForm(regExp);
    auto operandStack = vector<variant<char, FiniteAutomata>>();
    struct
    {
        auto operator()(char c) -> FiniteAutomata { return FiniteAutomata::From(c); }
        auto operator()(FiniteAutomata&& fa) -> FiniteAutomata { return move(fa); }
    } converter;
    for (size_t l = postfixRegExp.size(), i = 0; i < l; i++)
    {
        auto c = postfixRegExp[i];
        switch (c)
        {
        case '+':
        {
            auto a = move(operandStack.back());
            operandStack.pop_back();
            auto b = move(operandStack.back());
            operandStack.pop_back();
            operandStack.push_back(FiniteAutomata::Concat(std::visit(converter, move(a)), std::visit(converter, move(b))));
            break;
        }
        case '-':
        {
            auto a = move(operandStack.back());
            operandStack.pop_back();
            auto b = move(operandStack.back());
            operandStack.pop_back();
            operandStack.push_back(FiniteAutomata::Range(std::get<char>(a), std::get<char>(b)));
            break;
        }
        case '*':
        {
            auto a = move(operandStack.back());
            operandStack.pop_back();
            operandStack.push_back(FiniteAutomata::ZeroOrMore(std::visit(converter, move(a))));
            break;
        }
        case '|':
        {
            auto a = move(operandStack.back());
            operandStack.pop_back();
            auto b = move(operandStack.back());
            operandStack.pop_back();
            operandStack.push_back(FiniteAutomata::Or(std::visit(converter, move(a)), std::visit(converter, move(b))));
            break;
        }
        default:
            // number or alphabet
            operandStack.push_back(c);
            break;
        }
    }
    if (operandStack.size() != 1)
    {
        throw std::logic_error(nameof(operandStack)" doesn't have one finite automata as final result");
    }
    return move(std::get<FiniteAutomata>(operandStack.front()));
}

auto NFA2DFA(FiniteAutomata nfa) -> FiniteAutomata
{
    auto FollowEpsilon = [&nfa](set<State> todos) -> set<State>
    {
        set<State> fullRecord = todos;
        auto Iter = [&nfa, &fullRecord](this auto&& self, set<State> todos) -> set<State>
        {
            auto nextTodos = set<State>();
            for (auto s : todos)
            {
                auto next = nfa.Run(s, FiniteAutomata::epsilon);
                if (not next.has_value())
                {
                    continue;
                }
                else if (not fullRecord.contains(next.value()))
                {
                    nextTodos.insert(next);
                }
            }
            if (nextTodos.empty())
            {
                return move(fullRecord);
            }
            fullRecord.insert_range(nextTodos);
            return self(move(nextTodos));
        };
        return Iter(move(todos));
    };
    auto transitionTable = Graph<char>();
    auto subset2DFAState = map<set<State>, State>();
    auto worklist = queue<set<State>>();
    auto q0 = FollowEpsilon({ nfa.startState });
    subset2DFAState.insert({ q0, transitionTable.AllocateState() });
    worklist.push(move(q0));
    auto chars = nfa.transitionTable.AllPossibleInputs();

    for (; not worklist.empty();)
    {
        auto q = move(worklist.front());
        worklist.pop();
        for (auto c : chars)
        {
            auto nexts = set<State>();
            for (auto s : q)
            {
                if (auto next = nfa.Run(s, c); next.has_value())
                {
                    nexts.insert(next.value());
                }
            }
            auto temp = FollowEpsilon(move(nexts));
            if (not temp.empty())
            {
                if (not subset2DFAState.contains(temp))
                {
                    auto s = transitionTable.AllocateState();
                    subset2DFAState.insert({ temp, s });
                    worklist.push(temp);
                }
                // add transition q + c -> temp
                transitionTable.AddTransition(subset2DFAState[q], c, subset2DFAState[temp]); // will it have duplicate?
            }
        }
    }
    // construct DFA
}

auto Minimize(FiniteAutomata dfa) -> FiniteAutomata
{
    throw;
}

export
{
    auto Convert2PostfixForm(string_view regExp) -> vector<char>;
    auto ConstructNFAFrom(string_view regExp) -> FiniteAutomata;
    auto NFA2DFA(FiniteAutomata nfa) -> FiniteAutomata;
    auto Minimize(FiniteAutomata dfa) -> FiniteAutomata;
}