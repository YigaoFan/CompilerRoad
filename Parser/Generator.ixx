export module Generator;

import std;

using std::optional;
using std::suspend_always;
using std::coroutine_handle;
using std::move;

template <typename T>
class Generator
{
public:
    struct Promise
    {
        using CoroHandle = coroutine_handle<Promise>;
        T Value;

        auto initial_suspend() -> suspend_always
        {
            return {};
        }

        auto final_suspend() noexcept -> suspend_always
        {
            return {};
        }

        void unhandled_exception()
        {
            std::terminate();
        }

        auto yield_value(T t) -> suspend_always
        {
            Value = move(t);
            return {};
        }

        auto get_return_object()
        {
            return CoroHandle::from_promise(*this);
        }

        auto return_void() -> void
        {}
    };
private:
    Promise::CoroHandle handle;
public:
    using promise_type = Promise;

    Generator(Promise::CoroHandle handle) : handle(move(handle))
    { }

    Generator(Generator const& that) = delete;
    Generator(Generator&& that) noexcept
        : handle(move(that.handle))
    {
        that.handle = nullptr;
    }

    Generator& operator= (Generator const& that) = delete;
    Generator& operator= (Generator&& that) noexcept
    {
        this->handle = move(that.handle);
        that.handle = nullptr;
        return *this;
    }

    ~Generator()
    {
        if (handle)
        {
            handle.destroy();
        }
    }

    auto MoveNext() -> bool
    {
        if (handle.done())
        {
            return false;
        }

        handle.resume();
        return not handle.done();
    }

    auto Current() -> T&
    {
        return handle.promise().Value;
    }
};

template <typename Output, typename Input>
class Exchanger
{
public:
    struct Promise
    {
        using CoroHandle = coroutine_handle<Promise>;
        Output Out;
        optional<Input> In;

        auto initial_suspend() -> suspend_always
        {
            return {};
        }

        auto final_suspend() noexcept -> suspend_always
        {
            return {};
        }

        void unhandled_exception()
        {
            std::terminate();
        }

        auto yield_value(Output out) -> suspend_always
        {
            Out = move(out);
            return {};
        }

        auto get_return_object() -> Exchanger
        {
            return CoroHandle::from_promise(*this);
        }

        auto return_void() -> void
        {
        }

        auto await_transform(bool)
        {
            struct Awaiter
            {
                Promise* Mediator;
                bool await_ready() noexcept { return Mediator->In.has_value(); }
                void await_suspend(std::coroutine_handle<>) noexcept {}
                auto& await_resume() noexcept { return Mediator->In; }
            };
            return Awaiter{ .Mediator = this };
        }

        template <typename T>
        auto await_transform(T) = delete;
    };
private:
    Promise::CoroHandle handle;
public:
    using promise_type = Promise;

    Exchanger(Promise::CoroHandle handle) : handle(move(handle))
    {
    }

    Exchanger(Exchanger const& that) = delete;
    Exchanger(Exchanger&& that) noexcept
        : handle(move(that.handle))
    {
        that.handle = nullptr;
    }

    Exchanger& operator= (Exchanger const& that) = delete;
    Exchanger& operator= (Exchanger&& that) noexcept
    {
        this->handle = move(that.handle);
        that.handle = nullptr;
        return *this;
    }

    ~Exchanger()
    {
        if (handle)
        {
            handle.destroy();
        }
    }

    auto MoveNext() -> bool
    {
        if (handle.done())
        {
            return false;
        }

        handle.resume();
        return not handle.done();
    }

    auto Current() -> Output&
    {
        return handle.promise().Out;
    }

    auto Input(Input in) -> void
    {
        handle.promise().In = move(in);
    }
};

export
{
    template <typename T>
    class Generator;
    template <typename Output, typename Input>
    class Exchanger;
}