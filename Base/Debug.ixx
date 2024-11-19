export module Base:Debug;

import std;

using std::string_view;

auto Assert(bool value, char const* message) -> void
{
    if (!value)
    {
        throw std::runtime_error(message);
    }
}