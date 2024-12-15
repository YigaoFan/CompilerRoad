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

template <typename... Ts>
auto Log(std::format_string<Ts...> const fmt, Ts&&... ts) -> void
{
    std::cout << std::vformat(fmt.get(), std::make_format_args(ts...)) << std::endl;
}

export
{
    template <typename... Ts>
    auto Log(const std::format_string<Ts...> fmt, Ts&&... ts) -> void;
}