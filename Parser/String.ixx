export module String;

import std;

using std::size_t;

class String
{
    // shared, cannot change, ref-count, substring also ref
private:
    struct Share
    {
        char const* const Str;
        size_t RefCount;
        bool const Releasable = true;
    };
    Share* share;
    size_t start;
    size_t end; // not included

public:
    template <size_t N>
    String(char const(&literal)[N])
        : share(new Share{ .Str = literal, .RefCount = 1, .Releasable = false }),
        start(0), end(N - 1)
    {
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

    String& operator= (String const& that) noexcept
    {
        --share->RefCount;
        share = that.share;
        start = that.start;
        end = that.end;

        ++share->RefCount;
        return *this;
    }

    String& operator= (String&& that) noexcept
    {
        --share->RefCount;
        share = that.share;
        start = that.start;
        end = that.end;

        that.share = nullptr;
        return *this;
    }

    String Substring(size_t start) const
    {
        return Substring(start, end);
    }

    String Substring(size_t start, size_t end) const
    {
        auto s{ *this };
        s.start = start;
        s.end = end;
        return s;
    }

    bool Contains(char c) const
    {
        for (size_t i = start; i < end; ++i)
        {
            if (share->Str[i] == c)
            {
                return true;
            }
        }
    }

    size_t Length() const
    {
        return end - start;
    }
    
    ~String()
    {
        if (share->Releasable and share->RefCount == 0)
        {
            delete share->Str;
            delete share;
            share = nullptr;
        }
    }
};

export
{
    class String;
}