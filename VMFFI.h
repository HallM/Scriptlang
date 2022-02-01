#pragma once

#include <array>
#include <functional>

#include "VMStack.h"

class IRunnable {
public:
    virtual ~IRunnable() = default;
    virtual void invoke(VMFixedStack& s, size_t base) = 0;
};

template <typename R, typename... Args>
class BuiltinRunnable : public IRunnable {
public:
    BuiltinRunnable(std::function<R(Args...)> invokable) : _invokable(invokable) {
        _set_offset<Args...>(0, 0);
        _ret_offset = sizeof(R);
    }
    ~BuiltinRunnable() {}

    void invoke(VMFixedStack& s, size_t base) {
        _invoke(s, base, std::index_sequence_for<Args...>{});
    }
private:
    template <typename Af, typename As, typename... Ar>
    void _set_offset(size_t index, size_t offset) {
        size_t loc = offset + sizeof(Af);
        _poffsets[index] = loc;
        _set_offset<As, Ar...>(index + 1, loc);
    }
    template <typename Af>
    void _set_offset(size_t index, size_t offset) {
        _poffsets[index] = offset + sizeof(Af);
    }

    template <std::size_t... Is>
    void _invoke(VMFixedStack& s, size_t base, std::index_sequence<Is...>) {
        auto t = _invokable(
            (*s.at<Args>(base - _poffsets[Is]))...
        );
        *s.at<R>(base - _ret_offset) = t;
    }

    std::function<R(Args...)> _invokable;
    std::array<size_t, sizeof...(Args)> _poffsets;
    size_t _ret_offset;
};

template <typename... Args>
class BuiltinRunnable<void, Args...> : public IRunnable {
public:
    BuiltinRunnable(std::function<void(Args...)> invokable) : _invokable(invokable) {
        _set_offset<Args...>(0, 0);
    }
    ~BuiltinRunnable() {}

    void invoke(VMFixedStack& s, size_t base) {
        _invoke(s, base, std::index_sequence_for<Args...>{});
    }

private:
    template <typename Af, typename As, typename... Ar>
    void _set_offset(size_t index, size_t offset) {
        size_t loc = offset + sizeof(Af);
        _poffsets[index] = loc;
        _set_offset<As, Ar...>(index + 1, loc);
    }
    template <typename Af>
    void _set_offset(size_t index, size_t offset) {
        _poffsets[index] = offset + sizeof(Af);
    }

    template <std::size_t... Is>
    void _invoke(VMFixedStack& s, size_t base, std::index_sequence<Is...>) {
        _invokable(
            (*s.at<Args>(base - _poffsets[Is]))...
        );
    }

    std::function<void(Args...)> _invokable;
    std::array<size_t, sizeof...(Args)> _poffsets;
};
