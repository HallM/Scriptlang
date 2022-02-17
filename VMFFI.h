#pragma once

#include <array>
#include <functional>

#include "VMStack.h"

class VM;

template<typename T>
size_t
runtimesizeof() {
    size_t s = sizeof(T);
    if (s == 0) {
        return 0;
    }
    // adjust
    size_t adjusted = (((s - 1) / 4) + 1) * 4;
    return adjusted;
}

class IRunnable {
public:
    virtual ~IRunnable() = default;
    virtual void invoke(VM& vm, VMFixedStack& s, size_t base) const = 0;
};

class BytecodeRunnable : public IRunnable {
public:
    BytecodeRunnable(size_t address, size_t param_size, size_t stack_reserve);
    ~BytecodeRunnable();
    void invoke(VM& vm, VMFixedStack& s, size_t base) const;
private:
    size_t _address;
    size_t _param_size;
    size_t _stack_reserve;
};

template <typename R, typename... Args>
class BuiltinRunnable : public IRunnable {
public:
    BuiltinRunnable(std::function<R(Args...)> invokable) : _invokable(invokable) {
        _set_offset<Args...>(0, 0);
        _ret_offset = 0;
    }
    ~BuiltinRunnable() {}

    void invoke(VM& vm, VMFixedStack& s, size_t base) const{
        _invoke(s, base, std::index_sequence_for<Args...>{});
    }
private:
    template <typename Af, typename As, typename... Ar>
    void _set_offset(size_t index, size_t offset) {
        _poffsets[index] = offset;
        size_t loc = offset + runtimesizeof<Af>();
        _set_offset<As, Ar...>(index + 1, loc);
    }
    template <typename Af>
    void _set_offset(size_t index, size_t offset) {
        _poffsets[index] = offset;
    }

    template <std::size_t... Is>
    void _invoke(VMFixedStack& s, size_t base, std::index_sequence<Is...>) const {
        auto t = _invokable(
            (*s.at<Args>(base + _poffsets[Is]))...
        );
        *s.at<R>(base + _ret_offset) = t;
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

    void invoke(VM& vm, VMFixedStack& s, size_t base) const {
        _invoke(s, base, std::index_sequence_for<Args...>{});
    }

private:
    template <typename Af, typename As, typename... Ar>
    void _set_offset(size_t index, size_t offset) {
        _poffsets[index] = 0;
        size_t loc = offset + runtimesizeof<Af>();
        _set_offset<As, Ar...>(index + 1, loc);
    }
    template <typename Af>
    void _set_offset(size_t index, size_t offset) {
        _poffsets[index] = offset;
    }

    template <std::size_t... Is>
    void _invoke(VMFixedStack& s, size_t base, std::index_sequence<Is...>) const {
        _invokable(
            (*s.at<Args>(base + _poffsets[Is]))...
        );
    }

    std::function<void(Args...)> _invokable;
    std::array<size_t, sizeof...(Args)> _poffsets;
};
