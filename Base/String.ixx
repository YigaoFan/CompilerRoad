export module Base:String;

import std;

using std::size_t;
using std::string_view;
using std::string;
using std::copy;

class String
{
    // shared, cannot change, ref-count, substring also ref
private:
    struct Share
    {
        char const* const Str; // not null termination
        size_t RefCount;
        bool const Releasable = true;
    };
    Share* share;
    // [start, end)
    size_t start;
    size_t end;
    
    String(Share* share, size_t start, size_t end)
        : share(share), start(start), end(end)
    { }

public:
    static auto EmptyString() -> String
    {
        return String("");
    }

    template <size_t N>
    String(char const(&literal)[N])
        : share(new Share{ .Str = literal, .RefCount = 1, .Releasable = false }),
        start(0), end(N - 1)
    {
    }

    explicit String(string_view s)
        : share(new Share{ .Str = new char[s.size()], .RefCount = 1, .Releasable = true }), start(0), end(s.size())
    {
        s.copy(const_cast<char* const>(share->Str), s.size());
    }

    String(String const& that)
        : share(that.share), start(that.start), end(that.end)
    {
        ++that.share->RefCount;
    }

    String(String&& that)
        : share(that.share), start(that.start), end(that.end)
    {
        that.share = nullptr;
    }

    operator string_view() const
    {
        return string_view(share->Str + start, Length());
    }

    explicit operator string() const
    {
        return string(share->Str + start, Length());
    }

    auto operator= (String const& that) noexcept -> String&
    {
        ReduceCurrentShareRef();
        share = that.share;
        start = that.start;
        end = that.end;

        ++share->RefCount;
        return *this;
    }

    auto operator= (String&& that) noexcept -> String&
    {
        ReduceCurrentShareRef();
        share = that.share;
        start = that.start;
        end = that.end;

        that.share = nullptr;
        return *this;
    }

    auto operator== (String const& that) const -> bool
    {
        if (share == that.share and start == that.start and end == that.end)
        {
            return true;
        }
        if (Length() == that.Length())
        {
            for (auto i = start, j = that.start; i < end; ++i, ++j)
            {
                if (share->Str[i] != that.share->Str[j])
                {
                    return false;
                }
            }
            return true;
        }

        return false;
    }

    auto operator== (char const* that) const -> bool
    {
        if (share->Str == that)
        {
            if (start == 0)
            {
                return Length() == string_view(that).length();
            }
            else
            {
                return false;
            }
        }
        return string_view(*this) == string_view(that);
    }

    template <size_t N>
    auto operator== (char const(&that)[N]) const -> bool
    {
        if (Length() != N)
        {
            return false;
        }
        return operator== (static_cast<char const*>(that));
    }

    auto operator< (String const& that) const -> bool
    {
        return static_cast<string_view>(*this) < static_cast<string_view>(that);
    }

    auto operator+ (char c) const -> String
    {
        auto len = Length() + 1;
        auto newShare = new Share{ .Str = new char[len], .RefCount = 1, .Releasable = true };
        *copy(share->Str + start, share->Str + end, const_cast<char*>(newShare->Str)) = c;
        return String(newShare, 0, len);
    }

    auto operator+ (String const& that) const -> String
    {
        return operator+(static_cast<string_view>(that));
    }

    template <size_t N>
    auto operator+ (char const(&literal)[N]) const -> String
    {
        auto len = Length() + N - 1;
        auto newShare = new Share{ .Str = new char[len], .RefCount = 1, .Releasable = true};
        copy(literal, literal + N - 1,
            copy(share->Str + start, share->Str + end, const_cast<char*>(newShare->Str)));
        return String(newShare, 0, len);
    }

    auto operator+ (string_view s) const -> String
    {
        auto len = Length() + s.length();
        auto newShare = new Share{ .Str = new char[len], .RefCount = 1, .Releasable = true};
        copy(s.begin(), s.end(),
            copy(share->Str + start, share->Str + end, const_cast<char*>(newShare->Str)));
        return String(newShare, 0, len);
    }

    auto Substring(size_t start) const -> String
    {
        return Substring(start, Length());
    }

    auto Substring(size_t start, size_t length) const -> String
    {
        auto s{ *this };
        if (auto newStart = s.start + start; newStart < s.end)
        {
            s.start = newStart;
        }
        else
        {
            s.start = s.end;
        }

        if (auto newEnd = s.start + length; newEnd < s.end)
        {
            s.end = newEnd;
        }
        return s;
    }

    auto Contains(char c) const -> bool
    {
        for (size_t i = start; i < end; ++i)
        {
            if (share->Str[i] == c)
            {
                return true;
            }
        }
        return false;
    }

    auto Contains(String const& s) const -> bool
    {
        if (share == s.share)
        {
            if (s.start >= start and s.end <= end)
            {
                return true;
            }
            // maybe this is <a"aaa"> (repeat string), and s is the first "a" which is out of range, below compare function is same.
            // so we need recompare it
        }
        return Contains(static_cast<string_view>(s));
    }

    auto Contains(string_view s) const -> bool
    {
        return static_cast<string_view>(*this).contains(s);
    }

    auto StartWith(String const& s) const -> bool
    {
        if (share == s.share)
        {
            if (s.start == start and s.end <= end)
            {
                return true;
            }
        }
        return StartWith(static_cast<string_view>(s));
    }

    auto StartWith(string_view s) const -> bool
    {
        return static_cast<string_view>(*this).starts_with(s);
    }

    auto EndWith(String const& s) const -> bool
    {
        if (share == s.share)
        {
            if (s.end == end and s.start >= start)
            {
                return true;
            }
        }
        return EndWith(static_cast<string_view>(s));
    }

    auto EndWith(string_view s) const -> bool
    {
        return static_cast<string_view>(*this).ends_with(s);
    }

    auto Empty() const -> bool
    {
        return share == nullptr or start == end;
    }

    auto Length() const -> size_t
    {
        return end - start;
    }
    
    ~String()
    {
        if (share != nullptr and share->Releasable and share->RefCount == 0)
        {
            delete[] share->Str;
            delete share;
            share = nullptr;
        }
    }
private:
    auto ReduceCurrentShareRef() -> void
    {
        if (share == nullptr)
        {
            return;
        }
        if (share->RefCount > 0)
        {
            --share->RefCount;
        }
        if (share->RefCount == 0 and share->Releasable)
        {
            delete[] share->Str;
            delete share;
            share = nullptr;
        }
    }
};

std::ostream& operator<<(std::ostream& os, String const& s)
{
    os << static_cast<string_view>(s);
    return os;
}

export template<>
struct std::formatter<String, char>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        // not implement
        auto it = ctx.begin();
        if (it == ctx.end())
            return it;

        if (it != ctx.end() && *it != '}')
            throw std::format_error("Invalid format args for QuotableString.");

        return it;
    }

    template<class FormatContext>
    constexpr auto format(String const& t, FormatContext& fc) const
    {
        return std::format_to(fc.out(), "{}", static_cast<string_view>(t));
    }
};

export
{
    class String;
    std::ostream& operator<<(std::ostream& os, String const& s);
}