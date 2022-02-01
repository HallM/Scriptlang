#pragma once

#include <unordered_map>
#include <memory>
#include <vector>

#include "VMFFI.h"
#include "VMBytecode.h"
#include "VMStack.h"

struct MethodMetadata {
    size_t param_size;
    size_t stack_size;
};

// Program is a set of compiled instructions and information to find addresses.
// This can be used to generate a fixed stack frame that is just for globals.
class Program {
public:
    Program();
    ~Program();

    template <typename T>
    T get_global(std::string name, VMFixedStack& globals) {
        auto address = get_global_address(name);
        return globals.at<T>(address);
    }

    // Generates a fixed stack containing all globals.
    std::shared_ptr<VMFixedStack> generate_state();

    template <typename T>
    void add_global(std::string name) {
        size_t s = sizeof(T);
        _global_addresses[name] = _globals_size;
        _globals_size += s;
    }
    void add_builtin(std::string name, IRunnable* runnable);
    void add_method(std::string name, size_t param_size, size_t stack_size, std::vector<Opcode> method_code);

    size_t get_global_address(std::string name) const;
    size_t get_builtin_address(std::string name) const;
    size_t get_method_address(std::string name) const;
    size_t current_method_address() const;

    const MethodMetadata& get_method_metadata(std::string name) const;

    size_t globals_size() const;
    const std::vector<IRunnable*>& get_builtins() const;
    const std::vector<Opcode>& get_code() const;

    const Opcode& get_opcode(size_t index) const;

private:
    size_t _globals_size;
    std::vector<IRunnable*> _builtins;
    std::vector<Opcode> _code;

    std::unordered_map<std::string, size_t> _builtin_addresses;
    std::unordered_map<std::string, size_t> _function_addresses;
    std::unordered_map<std::string, MethodMetadata> _function_metadata;
    std::unordered_map<std::string, size_t> _global_addresses;
};
