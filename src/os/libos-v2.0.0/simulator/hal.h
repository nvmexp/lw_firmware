#pragma once
#include <functional>
#include <vector>

template <class T, class FRet, class... FArgs, class... Args>
auto bind_hal(FRet (T::*f)(FArgs...), T *pthis, Args &&... args) -> std::function<FRet(FArgs...)>
{
    return [=](FArgs &&... args) { return (pthis->*f)(args...); };
}

template <class T, class FRet>
auto bind_hal(FRet (T::*f)(), T *pthis) -> std::function<FRet()>
{
    return [=]() { return (pthis->*f)(); };
}

template <class T>
struct event;

template <class... FArgs>
struct event<void(FArgs...)>
{
    std::vector<std::function<void(FArgs...)>> handlers;

    void connect(std::function<void(FArgs...)> &&other) { handlers.push_back(other); }

    template <class T>
    void connect(void (T::*f)(FArgs...), T *pthis)
    {
        handlers.push_back([=](FArgs &&... args) { return (pthis->*f)(args...); });
    }

    void operator()(FArgs... args)
    {
        for (auto &handler : handlers)
            handler(args...);
    }
};

template <>
struct event<void()>
{
    std::vector<std::function<void()>> handlers;

    void connect(std::function<void()> &&other) { handlers.push_back(other); }

    template <class T>
    void connect(void (T::*f)(), T *pthis)
    {
        handlers.push_back([=]() { return (pthis->*f)(); });
    }

    void operator()()
    {
        for (auto &handler : handlers)
            handler();
    }
};
