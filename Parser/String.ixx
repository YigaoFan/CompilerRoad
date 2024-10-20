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
    size_t start;
    size_t end; // not included

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
        : share(new Share{ .Str = new char[s.size()], .RefCount = 1, .Releasable = true}), start(0), end(s.size())
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
        return string_view(share->Str + start, end - start);
    }

    explicit operator string() const
    {
        // test if it will copy, make sure it will copy to make string use safely
        return string(share->Str + start, end - start);
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
        return Substring(start, end);
    }

    auto Substring(size_t start, size_t end) const -> String
    {
        auto s{ *this };
        s.start = start;
        s.end = end;
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
        if (share->Releasable and share->RefCount == 0)
        {
            delete[] share->Str;
            delete share;
            share = nullptr;
        }
    }
};

export
{
    class String;
}