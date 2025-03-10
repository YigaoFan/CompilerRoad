export module FiniteAutomataBuilder;

#include "Util.h"
import std;
import Base;
import Graph;
import FiniteAutomata;

using std::size_t;
using std::string_view;
using std::vector;
using std::pair;
using std::map;
using std::variant;
using std::set;
using std::queue;
using std::deque;
using std::optional;
using std::out_of_range;
using std::logic_error;
using std::back_inserter;
using std::move;
using std::format;
using std::any_of;
using std::println;
using std::ranges::to;
using std::ranges::views::transform;
using std::ranges::views::filter;

auto Convert2PostfixForm(string_view regExp) -> vector<char>
{
    auto Priority = [](char op)
    {
        switch (op)
        {
        case '*': return 3;
        case '?': return 3;
        case '+': return 2;
        case '-': return 2;
        case '|': return 1;
        case '^': return 0;
        default: throw std::out_of_range(format("out of operate range: {}", op));
        }
    };
    auto isOperatorOrEndScope = [](char c) { return string_view("?*+|-])").contains(c); };
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
        case '?':
            AddOperator(c);
            goto addPossibleRelationWithNextChar;
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
        case '^':
            operators.push_back('^');
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
                // if here operators is empty, that means parens in regExp not balanced
                auto op = operators.back();
                operators.pop_back();
                if (op == '(')
                {
                    goto addPossibleRelationWithNextChar;
                }
                output.push_back(op);
            }
            break;
        case '\\':
            output.push_back('\\');
            ++i;
            output.push_back(regExp[i]);
            goto addPossibleRelationWithNextChar;
        default:
            output.push_back(c);
        // some chars need to handle the relation with next item
        addPossibleRelationWithNextChar:
            if (i != len - 1 and not isOperatorOrEndScope(regExp[i + 1]))
            {
                if (any_of(operators.cbegin(), operators.cend(), [](char ch) { return '[' == ch; }))
                {
                    // [^ will reverse the relation here
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
template <typename Result>
auto ConstructNFAFrom(string_view regExp, Result acceptStateResult) -> FiniteAutomataDraft<char, Result>
{
    using Automata = FiniteAutomataDraft<char, Result>;
    //Log("Construct NFA for {}", regExp);

    switch (regExp.length())
    {
    case 1:
    {
        auto fa = Automata::From(regExp[0]);
        fa.SetAcceptStateResult(move(acceptStateResult));
        return fa;
    }
    case 2:
        if (regExp[0] == '\\')
        {
            auto fa = Automata::From(regExp[1]);
            fa.SetAcceptStateResult(move(acceptStateResult));
            return fa;
        }
        break;
    default:
        break;
    }

    auto postfixRegExp = Convert2PostfixForm(regExp);
    auto operandStack = vector<variant<char, Automata>>();
    struct
    {
        auto operator()(char c) -> Automata { return Automata::From(c); }
        auto operator()(Automata&& fa) -> Automata { return move(fa); }
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
            operandStack.push_back(Automata::Concat(std::visit(converter, move(b)), std::visit(converter, move(a))));
            break;
        }
        case '-':
        {
            auto a = move(operandStack.back());
            operandStack.pop_back();
            auto b = move(operandStack.back());
            operandStack.pop_back();
            char left = std::get<char>(b);
            char right = std::get<char>(a);
            operandStack.push_back(Automata::Range(left, right));
            break;
        }
        case '*':
        {
            auto a = move(operandStack.back());
            operandStack.pop_back();
            operandStack.push_back(Automata::ZeroOrMore(std::visit(converter, move(a))));
            break;
        }
        case '?':
        {
            auto a = move(operandStack.back());
            operandStack.pop_back();
            operandStack.push_back(Automata::Optional(std::visit(converter, move(a))));
            break;
        }
        case '|':
        {
            auto a = move(operandStack.back());
            operandStack.pop_back();
            auto b = move(operandStack.back());
            operandStack.pop_back();
            operandStack.push_back(Automata::Or(std::visit(converter, move(b)), std::visit(converter, move(a))));
            break;
        }
        case '^':
        {
            auto a = move(operandStack.back());
            operandStack.pop_back();
            operandStack.push_back(Automata::Not(std::visit(converter, move(a))));
            break;
        }
        case '\\':
            ++i;
            operandStack.push_back(postfixRegExp[i]);
            break;
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
    auto finalNfa = move(std::visit(converter, move(operandStack.front())));
    finalNfa.SetAcceptStateResult(move(acceptStateResult));
    //Log("result NFA: {}", finalNfa);
    return finalNfa;
}

template <template <typename...> class Container0, template <typename...> class Container1, typename Value>
auto SetIntersection(Container0<Value> const& set1, Container1<Value> const& set2) -> Container0<Value>
{
    using std::ranges::set_intersection;
    Container0<Value> set3;
    set_intersection(set1, set2, std::inserter(set3, set3.begin()));
    return set3;
}

template <template <typename...> class Container0, template <typename...> class Container1, typename Value>
auto SetDifference(Container0<Value> const& set1, Container1<Value> const& set2) -> Container0<Value>
{
    using std::ranges::set_difference;
    Container0<Value> set3;
    set_difference(set1, set2, std::inserter(set3, set3.begin()));
    return set3;
}

template <typename Input, typename Result>
auto NFA2DFA(FiniteAutomataDraft<Input, Result> nfa) -> FiniteAutomataDraft<Input, Result>
{
    auto FollowEpsilon = [&nfa](set<State> initTodos) -> set<State>
    {
        auto fullRecord = initTodos;
        auto todos = vector(initTodos.begin(), initTodos.end());
        auto nextTodos = vector<State>();
        for (;;)
        {
            for (auto s : todos)
            {
                auto nexts = nfa.Run(s, FiniteAutomataDraft<Input, Result>::epsilon);
                for (auto next : nexts)
                {
                    if (not fullRecord.contains(next))
                    {
                        fullRecord.insert(next);
                        nextTodos.push_back(next);
                    }
                }
            }
            if (nextTodos.empty())
            {
                return move(fullRecord);
            }
            std::sort(nextTodos.begin(), nextTodos.end());
            nextTodos.erase(std::unique(nextTodos.begin(), nextTodos.end()), nextTodos.end());
            todos = move(nextTodos);
        }
    };
    auto transitionTable = GraphDraft<Step<Input>>();
    auto subset2DFAState = map<set<State>, State>();
    auto AddSubset = [&transitionTable, &subset2DFAState](set<State> subset) -> State
    {
        auto s = transitionTable.AllocateState();
        subset2DFAState.insert({ move(subset), s });
        return s;
    };

    queue<set<State>> worklist;
    auto q0 = FollowEpsilon({ nfa.StartState });
    auto start = AddSubset(q0);
    vector<State> accepts;
    vector<pair<State, Result>> accept2Result;
    worklist.push(move(q0));
    auto chars = nfa.transitionTable.AllPossibleInputs() | filter([](Step<Input> const& c) { return c != FiniteAutomataDraft<Input, Result>::epsilon; });
    //println("all chars run {}", chars | to<vector<Step<char>>>());

    for (; not worklist.empty();)
    {
        auto q = move(worklist.front());
        //println("checking {}", q);
        worklist.pop();
        for (auto c : chars)
        {
            //println("run {}", c);
            auto nexts = set<State>();
            for (auto s : q)
            {
                nexts.insert_range(nfa.Run(s, c)); // performance point
            }
            auto temp = FollowEpsilon(move(nexts));
            //println("got {}", temp);
            if (not temp.empty())
            {
                State s;
                if (not subset2DFAState.contains(temp))
                {
                    //println("not in all subsets");
                    s = AddSubset(temp);

                    if (auto common = SetIntersection(nfa.AcceptingStates, temp); not common.empty())
                    {
                        if (common.size() == 1)
                        {
                            accept2Result.push_back({ s, nfa.AcceptStateResultOf(common.front()) });
                        }
                        else
                        {
                            accept2Result.push_back({ s, std::ranges::min(common | transform([&](State s) { return nfa.AcceptStateResultOf(s); })) });
                        }
                        accepts.push_back(s);
                    }
                    worklist.push(move(temp));
                }
                else
                {
                    s = subset2DFAState.at(temp);
                }
                // add transition q + c -> temp
                transitionTable.AddTransition(subset2DFAState[q], c, s); // will it have duplicate?
            }
        }
    }

    return FiniteAutomataDraft<Input, Result>(start, move(accepts), move(transitionTable), move(accept2Result));
}

// TODO refactor to construct set with move_iterator
template <bool DivideAccepts, typename Input, typename Result>
auto Minimize(FiniteAutomataDraft<Input, Result> dfa) -> FiniteAutomataDraft<Input, Result>
{
    using std::ranges::set_intersection;
    using std::ranges::set_difference;

    // no need to partion when each partion has only one item
    auto accepts = set<State>(dfa.AcceptingStates.begin(), dfa.AcceptingStates.end());
    auto nonaccepts = dfa.transitionTable.AllStates() | filter([&](auto s) { return not accepts.contains(s); }) | to<set<State>>();

    auto partition = set<set<State>>
    {
        nonaccepts,
    };
    if constexpr (DivideAccepts)
    {
        for (auto s : accepts)
        {
            partition.insert(set{ s });
        }
    }
    else
    {
        partition.insert(accepts);
    }
    auto worklist = deque<set<State>>
    {
        move(nonaccepts),
    };
    if constexpr (DivideAccepts)
    {
        for (auto s : accepts)
        {
            worklist.push_front(set{ s });
        }
    }
    else
    {
        worklist.push_front(move(accepts));
    }
    
    auto chars = dfa.transitionTable.AllPossibleInputs() | filter([](Step<Input> const& c) { return c != FiniteAutomataDraft<Input, Result>::epsilon; });

    for (; not worklist.empty();)
    {
        auto s = move(worklist.front());
        //println("checking {}", s);
        worklist.pop_front();
        // can we save pointer in worklist? because partition and worklist are sharing sets
        for (auto c : chars)
        {
            auto image = set<State>();
            for (auto state : s)
            {
                image.insert_range(dfa.transitionTable.ReverseRun(state, c));
            }
            //println("for {} found image: {}", c, image);

            vector<decltype(partition)::iterator> toRemoves;
            vector<set<State>> toAdds;
            auto count = image.size();
            for (auto i = partition.begin(); i != partition.end(); ++i) // not copy here
            {
                auto& q = *i;
                if (count == 0)
                {
                    break;
                }
                auto q1 = SetIntersection(q, image);
                if (not q1.empty())
                {
                    count = count - q1.size();
                    auto q2 = SetDifference(q, q1);
                    //println("related partition: {}, divide to {} and {}", q, q1, q2);
                    if (not q2.empty())
                    {
                        toRemoves.push_back(i);
                        toAdds.push_back(q1);
                        toAdds.push_back(q2);

                        if (auto i = std::find(worklist.begin(), worklist.end(), q); i != worklist.end())
                        {
                            worklist.erase(i);
                            worklist.push_back(move(q1));
                            worklist.push_back(move(q2));
                        }
                        else if (q1.size() <= q2.size())
                        {
                            worklist.push_back(move(q1));
                        }
                        else
                        {
                            worklist.push_back(move(q2));
                        }
                        if (s == q)
                        {
                            break;
                        }
                    }
                }
            }
            for (auto& i : toRemoves)
            {
                partition.erase(i);
            }
            for (auto& p : toAdds)
            {
                partition.insert(move(p));
            }
        }
    }

    optional<State> start_mdfa;
    vector<State> accepts_mdfa;
    vector<pair<State, Result>> accept2Result;
    GraphDraft<Step<Input>> transitionTable_mdfa;
    auto partitionState = partition | transform([&](auto const& p)
    {
        return transitionTable_mdfa.AllocateState();
    }) | to<vector<State>>();
    auto State2NewState = [&partition, &partitionState](State s) -> State
    {
        for (auto i = 0; auto & p : partition) // maybe convert into a set
        {
            if (p.contains(s))
            {
                return partitionState[i];
            }
            ++i;
        }
        throw std::out_of_range(format("not found {} in partition", s));
    };
    set<std::tuple<State, State, Step<Input>>> records;
    for (auto i = 0; auto const& p : partition)
    {
        auto from = partitionState[i];

        if (auto common = SetIntersection(dfa.AcceptingStates, p); not common.empty())
        {
            if (common.size() == 1)
            {
                accept2Result.push_back({ from, dfa.AcceptStateResultOf(common.front()) });
            }
            else
            {
                accept2Result.push_back({ from, std::ranges::min(common | transform([&](State s) { return dfa.AcceptStateResultOf(s); })) });
            }
            accepts_mdfa.push_back(from);
        }

        if (p.contains(dfa.StartState))
        {
            if (not start_mdfa.has_value())
            {
                start_mdfa = from;
            }
            else
            {
                throw std::logic_error(format("multiple start states({}, {}) in minimize DFA", from, start_mdfa.value()));
            }
        }
        for (auto s : p)
        {
            auto& t = dfa.transitionTable[s];
            for (auto& item : t)
            {
                auto to = State2NewState(item.second);
                auto input = item.first;
                if (records.contains({ from, to, input }))
                {
                    continue;
                }
                transitionTable_mdfa.AddTransition(from, input, to);
                records.insert({ from, to, input });
            }
        }
        ++i;
    }
    if (not start_mdfa.has_value())
    {
        throw std::logic_error("don't find start state in partition");
    }
    if (accepts_mdfa.empty())
    {
        throw std::logic_error("don't find accept states in partition");
    }
    return FiniteAutomataDraft<Input, Result>(start_mdfa.value(), move(accepts_mdfa), move(transitionTable_mdfa), move(accept2Result));
}

template <typename Input, typename Result>
auto OrWithoutMergeAcceptState(vector<FiniteAutomataDraft<Input, Result>> fas) -> FiniteAutomataDraft<Input, Result>
{
    if (fas.size() == 1)
    {
        return move(fas.back());
    }

    GraphDraft<Step<Input>> transitionTable;
    auto start = transitionTable.AllocateState();
    vector<State> accepts;
    vector<pair<State, Result>> acceptState2Result;
    for (auto& fa : fas)
    {
        auto offset = transitionTable.Merge(move(fa.transitionTable));
        transitionTable.AddTransition(start, FiniteAutomataDraft<Input, Result>::epsilon, fa.StartState + offset);

        for (auto& accept : fa.AcceptingStates)
        {
            accept += offset;
        }
        accepts.append_range(move(fa.AcceptingStates));
        for (auto& p : fa.AcceptState2Result)
        {
            p.first += offset;
        }
        acceptState2Result.append_range(move(fa.AcceptState2Result));
    }

    return FiniteAutomataDraft<Input, Result>(start, move(accepts), move(transitionTable), move(acceptState2Result));
}

export
{
    auto Convert2PostfixForm(string_view regExp) -> vector<char>;
    template <typename Result>
    auto ConstructNFAFrom(string_view regExp, Result acceptStateResult) -> FiniteAutomataDraft<char, Result>;
    template <typename Input, typename Result>
    auto NFA2DFA(FiniteAutomataDraft<Input, Result> nfa) -> FiniteAutomataDraft<Input, Result>;
    template <bool DivideAccepts, typename Input, typename Result>
    auto Minimize(FiniteAutomataDraft<Input, Result> dfa) -> FiniteAutomataDraft<Input, Result>;
    template <typename Input, typename Result>
    auto OrWithoutMergeAcceptState(vector<FiniteAutomataDraft<Input, Result>> fas) -> FiniteAutomataDraft<Input, Result>;
}