export module Parser:Terminal;

import std;
import Base;
import :ParserBase;

using std::vector;
using std::set;
using std::map;
using std::pair;
using std::move;

struct Coordinate
{
    String Nontermin;
    int Rule;
    int Pos; // use -1 to represent the end position?

    Coordinate(String nontermin, int rule, int pos)
        : Nontermin(move(nontermin)), Rule(rule), Pos(pos)
    {
    }

    auto operator< (Coordinate const& that) const -> bool
    {
        if (Nontermin != that.Nontermin)
        {
            return Nontermin < that.Nontermin;
        }
        else if (Rule != that.Rule)
        {
            return Rule < that.Rule;
        }
        else if (Pos != that.Pos)
        {
            return Pos < that.Pos;
        }
        else
        {
            return false;
        }
    }

    auto operator!= (Coordinate const& that) const -> bool
    {
        return Nontermin != that.Nontermin or Rule != that.Rule or Pos != that.Rule;
    }

    auto operator== (Coordinate const& that) const -> bool
    {
        return Nontermin == that.Nontermin and Rule == that.Rule and Pos == that.Pos;
    }

    auto GetSymbol(SimpleGrammars const& grammars) const -> String
    {
        return grammars.at(Nontermin).at(Rule).at(Pos);
    }
};

struct Trace
{
    Coordinate Source;
    vector<Coordinate> Waypoints; // cannot mark it mutable and order by it

    auto operator< (Trace const& that) const -> bool
    {
        if (Source != that.Source)
        {
            return Source < that.Source;
        }

        return Waypoints < that.Waypoints;
    }
};

template <typename T>
concept IMergable = requires (T const& t)
{
    { t.Merge(t) } -> std::same_as<bool>;
};
template <IMergable T>
class MergeSet
{
private:
    set<T> set;
public:
    friend struct std::formatter<MergeSet, char>;

    MergeSet() : set()
    {
    }

    MergeSet(std::initializer_list<T> init) : set(move(init))
    {
    }

    MergeSet(MergeSet&& that) = default;
    MergeSet(MergeSet const& that) = default;

    auto operator= (MergeSet const& that) -> MergeSet&
    {
        set = that.set;
        return *this;
    }

    auto operator= (MergeSet&& that) -> MergeSet&
    {
        set = that.set;
        return *this;
    }

    template <typename Self>
    auto begin(this Self&& self)
    {
        return self.set.begin();
    }

    template <typename Self>
    auto end(this Self&& self)
    {
        return self.set.end();
    }

    template <typename Self, typename Key>
    auto Contains(this Self&& self, Key const& key)
    {
        return self.set.contains(key);
    }

    template <typename Self, typename Key>
    auto At(this Self&& self, Key const& key) -> T const&
    {
        return *self.set.find(key);
    }

    template <typename Key>
    auto Remove(Key const& key) -> void
    {
        set.erase(key);
    }

    /// <returns>bool means set changed</returns>
    template <typename Item>
    auto Add(Item&& t) -> bool
    {
        using std::forward;

        if (set.contains(t))
        {
            return set.find(t)->Merge(forward<Item>(t));
        }
        set.insert(forward<Item>(t));
        return true;
    }

    auto Empty() const
    {
        return set.empty();
    }

    auto Count() const
    {
        return set.size();
    }
};


class Terminal
{
private:
    mutable set<Trace> traces;
public:
    friend struct std::formatter<Terminal, char>;
        
    String const Value;

    Terminal(String value, Coordinate source)
        : Value(move(value))
    {
        traces.insert(Trace{ .Source = source, .Waypoints = {} });
    }

    Terminal(String value)
        : Value(move(value)), traces()
    {
    }

    Terminal(Terminal const& that)
        : traces(that.traces), Value(that.Value)
    {
    }

    Terminal(Terminal&& that)
        : traces(move(that.traces)), Value(move(that.Value))
    {
    }

    operator String const& () const
    {
        return Value;
    }

    auto Traces() const -> set<Trace> const&
    {
        return traces;
    }

    auto Merge(Terminal that) const -> bool
    {
        if (Value != that.Value)
        {
            throw std::logic_error("cannot merge two terminal records with different value");
        }
        auto n = traces.size();
        traces.merge(move(that.traces));
        return n != traces.size();
    }

    auto MarkWith(Coordinate currentCoordinate) const -> void
    {
        vector<Trace> ts;
        ts.reserve(traces.size());
        for (auto it = traces.begin(); it != traces.end(); )
        {
            ts.push_back(move(traces.extract(it++).value()));
            // why it++ should be here TODO put it above will cause exception
        }
        for (auto& x : ts)
        {
            x.Waypoints.push_back(currentCoordinate);
            traces.insert(move(x));
        }
    }
};

template <>
struct std::less<Terminal> // sort by Terminal::Value, merging Terminal with different traces in Terminal::Merge
{
    using is_transparent = void; // support contains(String), contains(string_view)

    auto operator() (Terminal const& t0, Terminal const& t1) const -> bool
    {
        return t0.Value < t1.Value;
    }

    auto operator() (String const& s, Terminal const& t) const -> bool
    {
        return s < t.Value;
    }

    auto operator() (Terminal const& t, String const& s) const -> bool
    {
        return t.Value < s;
    }

    auto operator() (Terminal const& t, string_view s) const -> bool
    {
        return t.Value < s;
    }

    auto operator() (string_view s, Terminal const& t) const -> bool
    {
        return s < t.Value;
    }
};

struct DifferentPoint
{
    Coordinate Left;
    Coordinate Right;
};

auto DiffConflictTracesInteral() -> void
{

}

auto JumpOver(vector<Coordinate> const& track, int i, SimpleGrammars const& grammars) -> std::optional<Coordinate>
{
    if (i >= track.size())
    {
        return {};
    }

    auto c = track.at(i);
    if (c.Pos != -1)
    {
        if (c.Pos < grammars.at(c.Nontermin).at(c.Rule).size() - 1)
        {
            ++c.Pos;
            return move(c);
        }
    }

    return JumpOver(track, i + 1, grammars);
}

auto StartOf(Coordinate const& coordinate, SimpleGrammars const& grammars, map<String, MergeSet<Terminal>> const& firstSets, map<String, MergeSet<Terminal>> const& followSets) -> MergeSet<Terminal>
{
    auto const& rule = grammars.at(coordinate.Nontermin).at(coordinate.Rule);
    
    MergeSet<Terminal> start;

    for (auto const& symbol : rule | std::ranges::views::drop(coordinate.Pos))
    {
        if (not firstSets.contains(symbol))
        {
            start.Add(symbol);
            start.Remove(epsilon);
            return start;
        }
        else
        {
            auto const& f = firstSets.at(symbol); // access violation
            for (auto& x : f)
            {
                start.Add(x);
            }

            if (not f.Contains(epsilon))
            {
                start.Remove(epsilon);
                return start;
            }
        }
    }

    // arrive here means the current start set has epsilon
    start.Remove(epsilon);
    for (auto& x : followSets.at(rule.back()))
    {
        start.Add(x);
    }

    // add follow here
    return start;
}

auto SetIntersection(MergeSet<Terminal> const& set1, MergeSet<Terminal> set2) -> vector<pair<Terminal, Terminal>>
{
    vector<pair<Terminal, Terminal>> inter;
    //set_intersection(set1, set2, std::inserter(inter, inter.begin()));
    for (auto const& x : set1)
    {
        for (auto const& y : set2)
        {
            if (x.Value == y.Value)
            {
                inter.push_back({ x, y });
                set2.Remove(y);
                break;
            }
        }
    }
    return inter;
}

auto DiffOn(vector<Coordinate> t0, vector<Coordinate> t1, SimpleGrammars const& grammars, map<String, MergeSet<Terminal>> const& firstSets, map<String, MergeSet<Terminal>> const& followSets) -> DifferentPoint
{
    auto FindFromTail = [](vector<Coordinate> const& t, int tailPosition, Coordinate const& dest) -> std::optional<int>
    {
        for (int i = tailPosition; i >= 0; i--)
        {
            if (t.at(i) == dest)
            {
                return i;
            }
        }
        return {};
    };
    auto FindFromHead = [](vector<Coordinate> const& t, int tail, Coordinate const& dest) -> std::optional<int>
    {
        for (int i = 0; i <= tail; i++)
        {
            if (t.at(i) == dest)
            {
                return i;
            }
        }
        return {};
    };
    auto DoWhenHaveSamePoint = [&grammars, &firstSets, &followSets, &t0, &t1](int i0, int i1) -> DifferentPoint
    {
        // add log here to show which coordinate to jump over what nontermin
        auto nextC0 = JumpOver(t0, i0 + 1, grammars);
        auto nextC1 = JumpOver(t1, i1 + 1, grammars);
        // check the start from this. if has intersection, recur on it
        if (nextC0.has_value() and nextC1.has_value())
        {
            // judge if it's same symbol TODO
            if (auto i = SetIntersection(StartOf(nextC0.value(), grammars, firstSets, followSets), 
                StartOf(nextC1.value(), grammars, firstSets, followSets));
                i.empty())
            {
                return { .Left = move(nextC0.value()), .Right = move(nextC1.value()) };
            }
            else
            {
                throw std::runtime_error("not implement DiffOn the next symbol when they have conflict");
                //for (auto const& t : i)
                //{
                    // maybe mutltiple items in intersection
                //}
                //return DiffOn();
            }
        }
        else if (nextC0.has_value())
        {
            return { .Left = move(nextC0.value()), .Right = move(t1.back()) };
        }
        else if (nextC1.has_value())
        {
            return { .Left = move(t0.back()), .Right = move(nextC1.value())};
        }
        throw std::exception("jump over failed, cannot find different point");
    };
    int i0 = 0;
    int i1 = 0;

    for (;;)
    {
        auto const& c0 = t0.at(i0);
        auto const& c1 = t1.at(i1);
        if (auto p0 = FindFromHead(t0, i0, c1); p0.has_value())
        {
            return DoWhenHaveSamePoint(p0.value(), i1);

        }
        else if (auto p1 = FindFromHead(t1, i1, c0); p1.has_value())
        {
            return DoWhenHaveSamePoint(i0, p1.value());
        }
        else
        {
            auto tail0 = t0.size() - 1;
            auto tail1 = t1.size() - 1;
            if (i0 == tail0 and i1 == tail1)
            {
                // now means we cannot find same point in two waypoints, 
                // only Sources are similar, so we use the first of traces to diff and jump over
                return DoWhenHaveSamePoint(-1, -1);
            }

            i0 = i0 == tail0 ? i0 : ++i0;
            i1 = i1 == tail1 ? i1 : ++i1;
        }
    }
}

auto DiffConflictTraces(set<Trace> traces0, set<Trace> traces1, SimpleGrammars const& grammars, map<String, MergeSet<Terminal>> const& firstSets, map<String, MergeSet<Terminal>> const& followSets) -> DifferentPoint
{
    for (auto const& t0 : traces0)
    {
        for (auto const& t1 : traces1)
        {
            // TODO if have multiple conflict traces, and maybe it also has conflict in Terminal internal
            // assume both of Terminals have non-empty Track. No handle of empty Track situation
            if (t0.Waypoints.back().Nontermin == t1.Waypoints.back().Nontermin)
            {
                if (t0.Source == t1.Source)
                {
					return DiffOn(t0.Waypoints, t1.Waypoints, grammars, firstSets, followSets);
                }
                else
                {
                    auto track0 = t0.Waypoints;
                    //track0.pop_back();
                    auto track1 = t1.Waypoints;
                    //track1.pop_back();
                    track0.insert(track0.begin(), t0.Source);
                    track1.insert(track1.begin(), t1.Source);
                    return DiffOn(move(track0), move(track1), grammars, firstSets, followSets);
                }
            }
        }
    }
    throw std::invalid_argument("no conflict traces need to be processed");
}

auto DiffConflictTerminal(Terminal const& terminal0, Terminal const& terminal1, SimpleGrammars const& grammars, map<String, MergeSet<Terminal>> const& firstSets, map<String, MergeSet<Terminal>> const& followSets) -> DifferentPoint
{
    return DiffConflictTraces(terminal0.Traces(), terminal1.Traces(), grammars, firstSets, followSets);
}

template <>
struct std::formatter<Coordinate, char> : NoSpecialProcessParse
{
    template<class FormatContext>
    constexpr auto format(Coordinate const& t, FormatContext& fc) const
    {
        return std::format_to(fc.out(), "({}:{}:{})", t.Nontermin, t.Rule, t.Pos);
    }
};

template <>
struct std::formatter<Trace, char> : NoSpecialProcessParse
{
    template<class FormatContext>
    constexpr auto format(Trace const& t, FormatContext& fc) const
    {
        std::format_to(fc.out(), "[{}", t.Source);
        for (auto const& x : t.Waypoints)
        {
            std::format_to(fc.out(), "->{}", x);
        }
        return std::format_to(fc.out(), "]");
    }
};

template <>
struct std::formatter<Terminal, char> : NoSpecialProcessParse
{
    template<class FormatContext>
    constexpr auto format(Terminal const& t, FormatContext& fc) const
    {
        return std::format_to(fc.out(), "{}:{}", t.Value, t.traces);
    }
};

template<typename T>
struct std::formatter<MergeSet<T>, char> : NoSpecialProcessParse
{
    template<class FormatContext>
    constexpr auto format(MergeSet<T>& t, FormatContext& fc) const
    {
        return std::format_to(fc.out(), "{}", t.set);
    }
};