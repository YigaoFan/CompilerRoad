export module String;

import std;

using std::size_t;
using std::string_view;
using std::string;

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

    explicit String(string const& s)
        : share(new Share{ .Str = new char[s.size()], .RefCount = 1, .Releasable = true }), start(0), end(s.size())
    {
        s.copy(const_cast<char *const>(share->Str), s.size());
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
        --share->RefCount;
        share = that.share;
        start = that.start;
        end = that.end;

        ++share->RefCount;
        return *this;
    }

    auto operator= (String&& that) noexcept -> String&
    {
        --share->RefCount;
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
            if (start >= s.start and end >= s.end)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        return Contains(static_cast<string_view>(s));
    }

    auto Contains(string_view s) const -> bool
    {
        return static_cast<string_view>(*this).contains(s);
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
};

std::ostream& operator<<(std::ostream& os, String const& s)
{
    os << static_cast<string_view>(s);
    return os;
}

template<>
struct std::formatter<String, char> : private std::formatter<string_view, char>
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
        return std::formatter<string_view, char>::format(static_cast<string_view>(t), fc);
    }
};
export
{
    class String;
    std::ostream& operator<<(std::ostream& os, String const& s);
}