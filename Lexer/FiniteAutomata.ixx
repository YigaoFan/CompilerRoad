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
using std::out_of_range;
using std::runtime_error;
using std::move_iterator;
using std::logic_error;
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
using std::ranges::views::filter;

struct Step
{
public:
    enum class Strategy : int
    {
        PassOne,
        BlockOne,
        PassRange,
        BlockRange,
    };
    Strategy Signal = Strategy::PassOne;
    // TODO maybe merge below 3 field in the future
    size_t Data;
    size_t Left;
    size_t Right;

    /// <summary>
    /// for ordering in map
    /// </summary>
    auto operator< (Step const& that) const -> bool
    {
        if (Signal != that.Signal)
        {
            return Signal < that.Signal;
        }
        switch (Signal)
        {
        case Step::Strategy::PassOne:
        case Step::Strategy::BlockOne:
            return Data < that.Data;
        case Step::Strategy::PassRange:
        case Step::Strategy::BlockRange:
            auto sum0 = Left + Right;
            auto sum1 = that.Left + that.Right;
            if (sum0 < sum1)
            {
                return true;
            }
            else if (Left < that.Left)
            {
                return true;
            }
            else if (Right < that.Right)
            {
                return true;
            }
            // need more detail compare there
            return false;
        default:
            throw logic_error(format("not handled Signal: {}", static_cast<int>(Signal)));
        }
        return Data < that.Data;
    }

    /// <summary>
    /// for compare in FiniteAutomataCraft.Run
    /// </summary>
    auto operator== (Step const& that) const -> bool
    {
        if (Signal == that.Signal)
        {
            switch (Signal)
            {
            case Strategy::PassOne:
            case Strategy::BlockOne:
                return Data == that.Data;
            case Strategy::PassRange:
            case Strategy::BlockRange:
                return Left == that.Left and Right == that.Right;
            }
        }
        return false;
    }

    auto operator!= (Step const& that) const -> bool
    {
        return not operator==(that);
    }

    /// <summary>
    /// for support not same type map.contains
    /// </summary>
    auto operator< (size_t c) const -> bool
    {
        // by actual test, below compare act as expected TODO think
        // should match the order semantic defined by Step < Step
        switch (Signal)
        {
        case Strategy::PassOne:
            return Data < c;
        case Strategy::BlockOne:
            return Data == c;
        case Strategy::PassRange:
            if (c < Left)
            {
                return true;
            }
            else if (c > Right)
            {

            }
            if (c >= Left and c <= Right)
            {
                return true;
            }
            return false;
        }
        throw logic_error(std::format("not handled Signal: {}", static_cast<int>(Signal)));
    }
private:
    auto OrderValue() const -> float
    {
        float v = 0;
        switch (Signal)
        {
        case Step::Strategy::PassOne:
            v = Data;
            v += 0.5;
            break;
        case Step::Strategy::BlockOne:
            v = Data;
            break;
        case Step::Strategy::PassRange:
            v = (Left + Right) / 2;
            break;
        case Step::Strategy::BlockRange:
            v = (Left + Right) / 2;
            break;
        default:
            throw logic_error(std::format("not handled Signal: {}", static_cast<int>(Signal)));
        }
        return v;
    }
};

auto operator! (Step::Strategy const& a) -> Step::Strategy
{
    switch (a)
    {
    case Step::Strategy::BlockOne:
        return Step::Strategy::PassOne;
    case Step::Strategy::PassOne:
        return Step::Strategy::BlockOne;
    case Step::Strategy::PassRange:
        return Step::Strategy::BlockRange;
    case Step::Strategy::BlockRange:
        return Step::Strategy::PassRange;
    default:
        throw logic_error(std::format("not handled Signal: {}", static_cast<int>(a)));
    }
}

auto operator< (size_t b, Step const& a) -> bool
{
    return a < b;
}

template <typename Input, typename AcceptStateResult>
class FiniteAutomata
{
private:
    Graph<Input> transitionTable;
    map<State, AcceptStateResult> acceptState2Result;

public:
    State const StartState;
    FiniteAutomata(Graph<Input> transitionTable, State startState, map<State, AcceptStateResult> acceptState2Result)
        : transitionTable(move(transitionTable)), StartState(startState), acceptState2Result(move(acceptState2Result))
    {
    }

    template <typename InputeItem>
    auto Run(State from, InputeItem input) const -> optional<pair<State, optional<AcceptStateResult>>>
    {
        if (auto r = transitionTable.Run(from, input); r.has_value())
        {
            if (auto s = r.value(); acceptState2Result.contains(s))
            {
                return pair<State, optional<AcceptStateResult>>{ move(s), acceptState2Result.at(s) };
            }
            else
            {
                return pair<State, optional<AcceptStateResult>>{ move(s), {} };
            }
        }
        return {};
    }
};

template <typename Input, typename AcceptStateResult>
class FiniteAutomataDraft
{
private:
    template <typename T, typename Char>
    friend struct std::formatter;
    template <typename Input, typename AcceptStateResult>
    friend auto NFA2DFA(FiniteAutomataDraft<Input, AcceptStateResult> nfa) -> FiniteAutomataDraft<Input, AcceptStateResult>;
    template <bool DivideAccepts, typename Input, typename AcceptStateResult>
    friend auto Minimize(FiniteAutomataDraft<Input, AcceptStateResult> dfa) -> FiniteAutomataDraft<Input, AcceptStateResult>;
    template <typename Input, typename AcceptStateResult>
    friend auto OrWithoutMergeAcceptState(vector<FiniteAutomataDraft<Input, AcceptStateResult>> fas) -> FiniteAutomataDraft<Input, AcceptStateResult>;

    static inline auto epsilon = Step{ .Data = 0 };
    GraphDraft<Input> transitionTable;

public:
    State StartState;
    vector<State> AcceptingStates;
    vector<pair<State, AcceptStateResult>> AcceptState2Result;

    static auto From(char c) -> FiniteAutomataDraft
    {
        auto transitionTable = GraphDraft<Input>();
        auto s0 = transitionTable.AllocateState();
        auto s1 = transitionTable.AllocateState();
        transitionTable.AddTransition(s0, Step{ .Data = static_cast<size_t>(c) }, s1);

        return FiniteAutomataDraft(s0, { s1 }, move(transitionTable), {});
    }

    static auto Not(FiniteAutomataDraft a) -> FiniteAutomataDraft
    {
        a.transitionTable.Traverse([](pair<State, vector<pair<Input, State>>>& item)
        {
            for (auto& e : item.second)
            {
                if (e.first.Data != epsilon.Data)
                {
                    e.first.Signal = not e.first.Signal;
                }
            }
        });

        return a;
    }

    // follow priority order to run FiniteAutomataDraft<char> combination, parentheses, closure, concatenation and alternation
    static auto Or(FiniteAutomataDraft a, FiniteAutomataDraft b) -> FiniteAutomataDraft
    {
        auto newStart = a.transitionTable.AllocateState();
        auto newAccept = a.transitionTable.AllocateState();

        auto offset = a.transitionTable.Merge(move(b.transitionTable));
        a.transitionTable.AddTransition(newStart, epsilon, a.StartState);
        a.transitionTable.AddTransition(newStart, epsilon, b.StartState + offset);

        for (auto accept : a.AcceptingStates)
        {
            a.transitionTable.AddTransition(accept, epsilon, newAccept);
        }
        for (auto accept : b.AcceptingStates)
        {
            a.transitionTable.AddTransition(accept + offset, epsilon, newAccept);
        }

        a.StartState = newStart;
        a.AcceptingStates = { newAccept };
        return a;
    }

    static auto ZeroOrMore(FiniteAutomataDraft a) -> FiniteAutomataDraft
    {
        auto newStart = a.transitionTable.AllocateState();
        auto newAccept = a.transitionTable.AllocateState();

        a.transitionTable.AddTransition(newStart, epsilon, a.StartState);
        a.transitionTable.AddTransition(newStart, epsilon, newAccept);

        for (auto accept : a.AcceptingStates)
        {
            a.transitionTable.AddTransition(accept, epsilon, a.StartState);
            a.transitionTable.AddTransition(accept, epsilon, newAccept);
        }

        a.StartState = newStart;
        a.AcceptingStates = { newAccept };
        return a;
    }

    static auto Concat(FiniteAutomataDraft a, FiniteAutomataDraft b) -> FiniteAutomataDraft
    {
        auto offset = a.transitionTable.Merge(move(b.transitionTable));
        for (auto accept : a.AcceptingStates)
        {
            a.transitionTable.AddTransition(accept, epsilon, b.StartState + offset);
        }

        for (auto& accept : b.AcceptingStates)
        {
            accept += offset;
        }
        a.AcceptingStates = move(b.AcceptingStates);
        return a;
    }

    static auto Range(char a, char b) -> FiniteAutomataDraft
    {
        if (not (std::isalnum(a) and std::isalnum(b) and a < b))
        {
            throw std::out_of_range(format("{} and {} should be number or alphabet", a, b));
        }
        auto transitionTable = GraphDraft<Input>();
        auto start = transitionTable.AllocateState();
        auto accept = transitionTable.AllocateState();

        auto n0 = transitionTable.AllocateState();
        auto n1 = transitionTable.AllocateState();

        transitionTable.AddTransition(start, epsilon, n0);
        transitionTable.AddTransition(n0, Step{ .Signal = Step::Strategy::PassRange, .Left = static_cast<size_t>(a), .Right = static_cast<size_t>(b) }, n1);
        transitionTable.AddTransition(n1, epsilon, accept);

        return FiniteAutomataDraft(start, { accept }, move(transitionTable), {});
    }

    FiniteAutomataDraft(State StartState, vector<State> acceptingStates, GraphDraft<Input> transitionTable, vector<pair<State, AcceptStateResult>> acceptState2Result)
        : StartState(StartState), AcceptingStates(move(acceptingStates)), transitionTable(move(transitionTable)), AcceptState2Result(move(acceptState2Result))
    { }

    auto SetAcceptStateResult(AcceptStateResult acceptStateResult) -> void
    {
        for (auto s : AcceptingStates)
        {
            AcceptState2Result.push_back({ s, acceptStateResult });
        }
    }

    auto AcceptStateResultOf(State state) const -> AcceptStateResult
    {
        for (auto& p : AcceptState2Result)
        {
            if (p.first == state)
            {
                return p.second;
            }
        }
        throw runtime_error(format("cannot find accept state result for {}", state));
    }

    auto Run(State from, Input input) const -> set<State>
    {
        return transitionTable.Run(from, input);
    }

    auto Freeze() const -> FiniteAutomata<Input, AcceptStateResult>
    {
        // maybe some states don't have result? allow or not allow
        return FiniteAutomata<Input, AcceptStateResult>(transitionTable.Freeze(), StartState, map(move_iterator(AcceptState2Result.begin()), move_iterator(AcceptState2Result.end())));
    }
};

template <typename Result>
class RefineFiniteAutomata
{
private:
    FiniteAutomata<Step, Result> fa;
    vector<pair<pair<char, char>, size_t>> classification;
public:
    RefineFiniteAutomata(FiniteAutomata<Step, Result> fa, map<pair<char, char>, size_t> classification)
        : fa(move(fa)), classification(move_iterator(classification.begin()), move_iterator(classification.end())) // already sorted
    { }

    /// <summary>
    /// 
    /// </summary>
    /// <param name="transitionTable">dfa </param>
    /// <returns></returns>
    template <typename Input>
    static auto CompressInput(GraphDraft<Input> const& transitionTable) -> void//pair<map<char, int>, Graph<int>>
    {
        auto states = transitionTable.AllStates() | to<vector<State>>(); // ensure order from 0 to n TODO
        for (auto s : states)
        {
            auto& ts = transitionTable[s]; // first level compress, second level compress
            map<State, vector<Input>> counter;
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

    auto RunFromStart(char input) const -> optional<pair<State, optional<Result>>>
    {
        return Run(fa.StartState, input);
    }

    auto Run(State from, char input) const -> optional<pair<State, optional<Result>>>
    {
        if (auto r = fa.Run(from, static_cast<size_t>(input)); r.has_value())
        {
            return r;
        }
        else if (classification.empty())
        {
            return r;
        }
        else
        {
            // divide into 2 parts to search
            for (size_t i = classification.size() / 2; ;)
            {
                auto const& x = classification[i];
                if (input >= x.first.first)
                {
                    if (input <= x.first.second)
                    {
                        return fa.Run(from, x.second); // this classification[i] do wrong job when in block direction. I should make range Step to limit range match at specific node
                    }
                    else // move right
                    {
                        if (auto next = (classification.size() + i) / 2; next != i)
                        {
                            i = next;
                        }
                        else
                        {
                            break;
                        }
                    }
                }
                else // move left
                {
                    if (auto next = i / 2; next != i)
                    {
                        i = next;
                    }
                    else
                    {
                        break;
                    }
                }
            }            
        }

        return {};
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
        case '^': return 0;
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
        addPossibleRelationWithNextChar:
            if (i != len - 1 and not isOperatorOrEndScope(regExp[i + 1]))
            {
                // [^ will affect relation here
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
template <typename Result>
auto ConstructNFAFrom(string_view regExp, Result acceptStateResult, map<pair<char, char>, size_t>&) -> FiniteAutomataDraft<Step, Result>
{
    using Automata = FiniteAutomataDraft<Step, Result>;
    if (regExp.starts_with("(") && regExp.ends_with(")"))
    {
        regExp = regExp.substr(1, regExp.length() - 2);
    }

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
            // should all content in [] be a item in classification?
            // when item bigger than 128, it will use classification
            // should template FiniteAutomataDraft<char>?
            //auto p = pair{ left, right };
            //if (not classification.contains(p))
            //{
            //    size_t c = classification.size() + 128;
            //    classification.insert({ p, c });
            //}
            //operandStack.push_back(Automata::From(Step{ .Data = classification[p] }));
            break;
        }
        case '*':
        {
            auto a = move(operandStack.back());
            operandStack.pop_back();
            operandStack.push_back(Automata::ZeroOrMore(std::visit(converter, move(a))));
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
    auto finalNfa = move(std::get<Automata>(operandStack.front()));
    finalNfa.SetAcceptStateResult(move(acceptStateResult));
    return finalNfa;
}

template <template <typename...> class Container0, template <typename...> class Container1, typename Value>
auto SetIntersection(Container0<Value> const& set1, Container1<Value> const& set2) -> Container0<Value>
{
    using std::ranges::set_intersection;
    Container0<Value> un;
    set_intersection(set1, set2, std::inserter(un, un.begin()));
    return un;
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
    auto transitionTable = GraphDraft<Input>();
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
    auto chars = nfa.transitionTable.AllPossibleInputs() | filter([](Input c) { return c != FiniteAutomataDraft<Input, Result>::epsilon; });

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
                    if (any_of(nfa.AcceptingStates.cbegin(), nfa.AcceptingStates.cend(), [&temp](State accept) { return temp.contains(accept); }))
                    {
                        accepts.push_back(s);
                    }
                    worklist.push(temp);
                }
                // add transition q + c -> temp
                transitionTable.AddTransition(subset2DFAState[q], c, subset2DFAState[temp]); // will it have duplicate?
            }
        }
    }

    return FiniteAutomataDraft<Input, Result>(start, move(accepts), move(transitionTable), move(accept2Result));
}

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
        move(accepts),
        move(nonaccepts),
    };
    auto chars = dfa.transitionTable.AllPossibleInputs() | filter([](Input c) { return c != FiniteAutomataDraft<Input, Result>::epsilon; });

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

    optional<State> start_mdfa;
    vector<State> accepts_mdfa;
    vector<pair<State, Result>> accept2Result;
    GraphDraft<Input> transitionTable_mdfa;
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
    set<std::tuple<State, State, Input>> records;
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
    //RefineFiniteAutomata::CompressInput(transitionTable_mdfa);
    return FiniteAutomataDraft<Input, Result>(start_mdfa.value(), move(accepts_mdfa), move(transitionTable_mdfa), move(accept2Result));
}

template <typename Input, typename Result>
auto OrWithoutMergeAcceptState(vector<FiniteAutomataDraft<Input, Result>> fas) -> FiniteAutomataDraft<Input, Result>
{
    if (fas.size() == 1)
    {
        return move(fas.back());
    }

    GraphDraft<Input> transitionTable;
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

template<typename Input, typename Result>
struct std::formatter<FiniteAutomataDraft<Input, Result>, char>
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
    constexpr auto format(FiniteAutomataDraft<Input, Result>& t, FormatContext& fc) const
    {
        using std::format_to;
        std::string out;
        format_to(back_inserter(out), "startState: {}\n", t.StartState);
        format_to(back_inserter(out), "acceptingStates: ");
        for (auto s : t.AcceptingStates)
        {
            format_to(back_inserter(out), "{} ", s);
        }

        format_to(back_inserter(out), "\ntransitions: \n{}", t.transitionTable);
        return format_to(fc.out(), "{}", out);
    }
};

export
{
    struct Step;
    auto operator< (size_t b, Step const& a) -> bool;
    auto operator! (Step::Strategy const& a)->Step::Strategy;
    auto Convert2PostfixForm(string_view regExp) -> vector<char>;
    //auto ConstructNFAFrom(string_view regExp) -> FiniteAutomataDraft<char>;
    template <typename Result>
    auto ConstructNFAFrom(string_view regExp, Result acceptStateResult, map<pair<char, char>, size_t>& classification) -> FiniteAutomataDraft<Step, Result>;
    template <typename Input, typename Result>
    auto NFA2DFA(FiniteAutomataDraft<Input, Result> nfa) -> FiniteAutomataDraft<Input, Result>;
    template <bool DivideAccepts, typename Input, typename Result>
    auto Minimize(FiniteAutomataDraft<Input, Result> dfa) -> FiniteAutomataDraft<Input, Result>;
    template <typename Input, typename Result>
    auto OrWithoutMergeAcceptState(vector<FiniteAutomataDraft<Input, Result>> fas) -> FiniteAutomataDraft<Input, Result>;
    template<typename Input, typename Result>
    struct std::formatter<FiniteAutomataDraft<Input, Result>, char>;
    template <typename Input, typename Result>
    class FiniteAutomataDraft;
    template <typename Result>
    class RefineFiniteAutomata;
}