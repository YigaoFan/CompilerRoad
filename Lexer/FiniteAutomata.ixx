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
using std::deque;
using std::map;
using std::optional;
using std::back_inserter;
using std::isdigit;
using std::isalpha;
using std::move;
using std::format;
using std::any_of;
using std::print;
using std::println;
using std::ranges::to;
using std::ranges::views::transform;

// formatter should be in a file
template<> // before using format<set<size_t>>
struct std::formatter<std::set<std::size_t>, char>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        auto it = ctx.begin();
        if (it == ctx.end())
            return it;

        if (it != ctx.end() && *it != '}')
            throw std::format_error("Invalid format args for QuotableString.");

        return it;
    }

    template<class FormatContext>
    constexpr auto format(std::set<std::size_t>& t, FormatContext& fc) const
    {
        using std::back_inserter;
        using std::format_to;
        std::string out;
        format_to(back_inserter(out), "{{");

        for (auto first = true; auto x : t)
        {
            if (first)
            {
                first = false;
                format_to(back_inserter(out), "{}", x);
            }
            else
            {
                format_to(back_inserter(out), ", {}", x);
            }
        }
        format_to(back_inserter(out), "}}");

        return format_to(fc.out(), "{}", out);
    }
};
template<> // before using format<set<size_t>>
struct std::formatter<std::vector<std::size_t>, char>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        auto it = ctx.begin();
        if (it == ctx.end())
            return it;

        if (it != ctx.end() && *it != '}')
            throw std::format_error("Invalid format args for QuotableString.");

        return it;
    }

    template<class FormatContext>
    constexpr auto format(std::vector<std::size_t>& t, FormatContext& fc) const
    {
        using std::back_inserter;
        using std::format_to;
        std::string out;
        format_to(back_inserter(out), "[");

        for (auto first = true; auto x : t)
        {
            if (first)
            {
                first = false;
                format_to(back_inserter(out), "{}", x);
            }
            else
            {
                format_to(back_inserter(out), ", {}", x);
            }
        }
        format_to(back_inserter(out), "]");

        return format_to(fc.out(), "{}", out);
    }
};

class FiniteAutomata
{
private:
    template <typename T, typename Char>
    friend struct std::formatter;
    friend auto NFA2DFA(FiniteAutomata nfa) -> FiniteAutomata;
    template <bool DivideAccepts>
    friend auto Minimize(FiniteAutomata dfa) -> FiniteAutomata;
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

    static auto OrWithoutMergeAcceptState(vector<FiniteAutomata> fas) -> FiniteAutomata
    {
        auto transitionTable = Graph<char>();
        auto start = transitionTable.AllocateState();
        auto accepts = vector<State>();
        for (auto& fa : fas)
        {
            auto offset = transitionTable.Merge(move(fa.transitionTable));
            transitionTable.AddTransition(start, epsilon, fa.startState + offset);
            for (auto& accept : fa.acceptingStates)
            {
                accepts.push_back(accept + offset);
            }
        }

        return FiniteAutomata(start, move(accepts), move(transitionTable));
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

    FiniteAutomata(State startState, vector<State> acceptingStates, Graph<char> transitionTable)
        : startState(startState), acceptingStates(move(acceptingStates)), transitionTable(move(transitionTable))
    { }

    auto Run(State from, char input) const -> set<State>
    {
        return transitionTable.Run(from, input);
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

template<> // before using format<set<size_t>>
struct std::formatter<std::map<std::size_t, std::size_t>, char>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        auto it = ctx.begin();
        if (it == ctx.end())
            return it;

        if (it != ctx.end() && *it != '}')
            throw std::format_error("Invalid format args for QuotableString.");

        return it;
    }

    template<class FormatContext>
    constexpr auto format(std::map<std::size_t, std::size_t>& t, FormatContext& fc) const
    {
        using std::back_inserter;
        using std::format_to;
        std::string out;
        format_to(back_inserter(out), "{{");

        for (auto first = true; auto x : t)
        {
            if (first)
            {
                first = false;
                format_to(back_inserter(out), "{}", x);
            }
            else
            {
                format_to(back_inserter(out), ", {}", x);
            }
        }
        format_to(back_inserter(out), "}}");

        return format_to(fc.out(), "{}", out);
    }
};

class RefineFiniteAutomata
{
public:
    /// <summary>
    /// 
    /// </summary>
    /// <param name="transitionTable">dfa </param>
    /// <returns></returns>
    static auto CompressInput(Graph<char> const& transitionTable) -> void//pair<map<char, int>, Graph<int>>
    {
        //auto chars = transitionTable.AllPossibleInputs() | to<vector<char>>();
        auto states = transitionTable.AllStates() | to<vector<State>>(); // ensure order from 0 to n TODO
        for (auto s : states)
        {
            auto& ts = transitionTable[s]; // first level compress, second level compress
            map<State, vector<char>> counter;
            for (auto& t : ts)
            {
                counter[t.second].push_back(t.first);
            }
            for (auto& i : counter)
            {
                if (i.second.size() > 1)
                {
                    println("{} to {} can be compress, {} to 1", s, i.first, i.second.size());
                    for (auto c : i.second)
                    {
                        print("{} ", c);
                    }
                }
            }
        }
        //print("Compress from {} to {}", chars.size(), newCharIndexes.size());
    }
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
        default: throw std::out_of_range(format("out of operate range: {}", op));
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
                if (any_of(operators.cbegin(), operators.cend(), [](char ch) { return '[' == ch; }))
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
            operandStack.push_back(FiniteAutomata::Concat(std::visit(converter, move(b)), std::visit(converter, move(a))));
            break;
        }
        case '-':
        {
            auto a = move(operandStack.back());
            operandStack.pop_back();
            auto b = move(operandStack.back());
            operandStack.pop_back();
            operandStack.push_back(FiniteAutomata::Range(std::get<char>(b), std::get<char>(a)));
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
            operandStack.push_back(FiniteAutomata::Or(std::visit(converter, move(b)), std::visit(converter, move(a))));
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
    auto FollowEpsilon = [&nfa](set<State> initTodos) -> set<State>
    {
        auto fullRecord = initTodos;
        auto todos = vector(initTodos.begin(), initTodos.end());
        auto nextTodos = vector<State>();
        for (;;)
        {
            for (auto s : todos)
            {
                auto nexts = nfa.Run(s, FiniteAutomata::epsilon);
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
    auto transitionTable = Graph<char>();
    auto subset2DFAState = map<set<State>, State>();
    auto AddSubset = [&transitionTable, &subset2DFAState](set<State> subset) -> State
    {
        auto s = transitionTable.AllocateState();
        subset2DFAState.insert({ move(subset), s });
        return s;
    };

    auto worklist = queue<set<State>>();
    auto q0 = FollowEpsilon({ nfa.startState });
    auto start = AddSubset(q0);
    auto accepts = vector<State>();
    worklist.push(move(q0));
    auto chars = nfa.transitionTable.AllPossibleInputs() | std::ranges::views::filter([](char c) { return c != FiniteAutomata::epsilon; });

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
                nexts.insert_range(nfa.Run(s, c));
            }
            auto temp = FollowEpsilon(move(nexts)); // performance point
            //println("got {}", temp);
            if (not temp.empty())
            {
                if (not subset2DFAState.contains(temp))
                {
                    //println("not in all subsets");
                    auto s = AddSubset(temp);
                    if (any_of(nfa.acceptingStates.cbegin(), nfa.acceptingStates.cend(), [&temp](State accept) { return temp.contains(accept); }))
                    {
                        accepts.push_back(s); // possible duplicate?
                    }
                    worklist.push(temp);
                }
                // add transition q + c -> temp
                transitionTable.AddTransition(subset2DFAState[q], c, subset2DFAState[temp]); // will it have duplicate?
            }
        }
    }
    return FiniteAutomata(start, move(accepts), move(transitionTable));
}

template <bool DivideAccepts>
auto Minimize(FiniteAutomata dfa) -> FiniteAutomata
{
    using std::ranges::views::filter;
    using std::ranges::set_intersection;
    using std::ranges::set_difference;

    // no need to partion when each partion has only one item
    auto accepts = set<State>(dfa.acceptingStates.begin(), dfa.acceptingStates.end());
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
    //auto transitionRecord = map<set<State>, vector<pair<set<State>, char>>>();
    auto worklist = deque<set<State>>
    {
        move(accepts),
        move(nonaccepts),
    };
    auto chars = dfa.transitionTable.AllPossibleInputs() | filter([](char c) { return c != FiniteAutomata::epsilon; });

    for (; not worklist.empty();)
    {
        auto s = move(worklist.front());
        //println("checking {}", s);
        worklist.pop_front();
        for (auto c : chars)
        {
            auto image = set<State>();
            for (auto state : s)
            {
                image.insert_range(dfa.transitionTable.ReverseRun(state, c));
            }
            //println("for {} found image: {}", c, image);

            for (auto& q : partition | to<vector<set<State>>>())
            {
                auto q1 = vector<State>();
                set_intersection(q, image, back_inserter(q1));
                if (not q1.empty())
                {
                    auto q2 = vector<State>();
                    set_difference(q, q1, back_inserter(q2));
                    //println("related partition: {}, divide to {} and {}", q, q1, q2);
                    if (not q2.empty())
                    {
                        partition.erase(q);
                        auto q1Set = set(q1.begin(), q1.end());
                        partition.insert(q1Set);
                        partition.insert(set(q2.begin(), q2.end()));
                        //transitionRecord[move(q1Set)].push_back({ s, c });
                        
                        if (auto i = std::find(worklist.begin(), worklist.end(), q); i != worklist.end())
                        {
                            worklist.erase(i);
                            worklist.push_back(set(q1.begin(), q1.end()));
                            worklist.push_back(set(q2.begin(), q2.end()));
                        }
                        else if (q1.size() <= q2.size())
                        {
                            worklist.push_back(set(q1.begin(), q1.end()));
                        }
                        else
                        {
                            worklist.push_back(set(q2.begin(), q2.end()));
                        }
                        if (s == q)
                        {
                            break;
                        }
                    }
                }
            }
        }
    }

    auto start_mdfa = optional<State>();
    auto accepts_mdfa = vector<State>();
    auto transitionTable_mdfa = Graph<char>();
    //auto partition2State = map<set<State>, State>();
    auto partitionState = partition | transform([&](auto const& p)
    {
        return transitionTable_mdfa.AllocateState();
    }) | to<vector<State>>();
    auto State2NewState = [&partition, &partitionState](State s) -> State
    {
        for (auto i = 0; auto& p : partition) // maybe convert into a set
        {
            if (p.contains(s))
            {
                return partitionState[i];
            }
            ++i;
        }
        throw std::out_of_range(format("not found {} in partition", s));
    };
    set<std::tuple<State, State, char>> records;
    for (auto i = 0; auto const& p : partition)
    {
        auto from = partitionState[i];

        if (any_of(dfa.acceptingStates.begin(), dfa.acceptingStates.end(), [&p](auto s) { return p.contains(s); }))
        {
            accepts_mdfa.push_back(from);
        }
        if (p.contains(dfa.startState))
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
    //auto GetStateOfPartition = [&partition2State, &transitionTable_mdfa](auto const& p) -> State
    //{
    //    if (partition2State.contains(p))
    //    {
    //        return partition2State[p];
    //    }
    //    else
    //    {
    //        auto s = transitionTable_mdfa.AllocateState();
    //        partition2State.insert({ p, s });
    //        return s;
    //    }
    //};

    //for (auto& p : partition)
    //{
    //    auto from = GetStateOfPartition(p);
    //    if (any_of(dfa.acceptingStates.begin(), dfa.acceptingStates.end(), [&p](auto s) { return p.contains(s); }))
    //    {
    //        accepts_mdfa.push_back(from);
    //    }
    //    if (p.contains(dfa.startState))
    //    {
    //        if (not start_mdfa.has_value())
    //        {
    //            start_mdfa = from;
    //        }
    //        else
    //        {
    //            throw std::logic_error(format("multiple start states({}, {}) in minimize DFA", from, start_mdfa.value()));
    //        }
    //    }
    //    
    //    if (transitionRecord.contains(p))
    //    {
    //        for (auto& t : transitionRecord[p])
    //        {
    //            auto to = GetStateOfPartition(t.first);
    //            transitionTable_mdfa.AddTransition(from, t.second, to);
    //        }
    //    }
    //    else // why need else?
    //    {

    //    }
    //}
    if (not start_mdfa.has_value())
    {
        throw std::logic_error("don't find start state in partition");
    }
    if (accepts_mdfa.empty())
    {
        throw std::logic_error("don't find accept states in partition");
    }
    //RefineFiniteAutomata::CompressInput(transitionTable_mdfa);
    return FiniteAutomata(start_mdfa.value(), move(accepts_mdfa), move(transitionTable_mdfa));
}

template<>
struct std::formatter<FiniteAutomata, char>// : std::formatter<Graph<char>, char> // change Char to char, and inherit to use base format
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        auto it = ctx.begin();
        if (it == ctx.end())
            return it;

        if (it != ctx.end() && *it != '}')
            throw std::format_error("Invalid format args for QuotableString.");

        return it;
    }

    template<class FormatContext>
    constexpr auto format(FiniteAutomata& t, FormatContext& fc) const
    {
        using std::format_to;
        std::string out;
        format_to(back_inserter(out), "startState: {}\n", t.startState);
        format_to(back_inserter(out), "acceptingStates: ");
        for (auto s : t.acceptingStates)
        {
            format_to(back_inserter(out), "{} ", s);
        }

        format_to(back_inserter(out), "\ntransitions: \n{}", t.transitionTable);
        return format_to(fc.out(), "{}", out);
    }
};

export
{
    auto Convert2PostfixForm(string_view regExp) -> vector<char>;
    auto ConstructNFAFrom(string_view regExp) -> FiniteAutomata;
    auto NFA2DFA(FiniteAutomata nfa) -> FiniteAutomata;
    template <bool DivideAccepts>
    auto Minimize(FiniteAutomata dfa) -> FiniteAutomata;
    template<>
    struct std::formatter<FiniteAutomata, char>;
    class FiniteAutomata;
}