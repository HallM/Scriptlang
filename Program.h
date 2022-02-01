#pragma once

#include <unordered_map>
#include <vector>

#include "VMFFI.h"
#include "VMBytecode.h"

class Program {
public:
    Program();
    ~Program();

    template <typename T>
    void add_global(std::string name) {
        size_t s = sizeof(T);
        _global_addresses[name] = _globals_size;
        _globals_size += s;
    }
    void add_builtin(std::string name, IRunnable* runnable);
    void add_method(std::string name, std::vector<Opcode> method_code);

    size_t get_global(std::string name) const;
    size_t get_builtin(std::string name) const;
    size_t get_method(std::string name) const;
    size_t current_method() const;

    size_t globals_size() const;
    const std::vector<IRunnable*>& get_builtins() const;
    const std::vector<Opcode>& get_code() const;

private:
    size_t _globals_size;
    std::vector<IRunnable*> _builtins;
    std::vector<Opcode> _code;

    std::unordered_map<std::string, size_t> _builtin_addresses;
    std::unordered_map<std::string, size_t> _function_addresses;
    std::unordered_map<std::string, size_t> _global_addresses;
};
