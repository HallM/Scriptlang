#pragma once

#include <unordered_map>
#include <memory>
#include <typeindex>
#include <typeinfo>
#include <vector>

#include "VMFFI.h"
#include "VMBytecode.h"
#include "VMStack.h"
#include "VM.h"

struct MethodMetadata {
    size_t param_size;
    size_t stack_size;
};

class Program;

template <typename Ret, typename... Args>
class Callable {
public:
    Callable(const Program& p, size_t address, size_t stack_size) :
        _p(p),
        _address(address),
        _stack_size(stack_size) {}

    Ret operator()(VM& vm, VMFixedStack& globals, Args... args) {
        size_t ret = 0;
        vm.clear_state();
        if constexpr (sizeof...(Args) > 0) {
            vm.push_parameters<Args...>(args...);
        } else if constexpr (!std::is_void<Ret>::value) {
            vm.reserve_return<Ret>();
        }
        vm.run_method(_p, globals, _address, _stack_size);
        if constexpr (!std::is_void<Ret>::value) {
            return vm.get_return<Ret>(0);
        }
    }
private:
    const Program& _p;
    size_t _address;
    size_t _stack_size;
};

// Program is a set of compiled instructions and information to find addresses.
// This can be used to generate a fixed stack frame that is just for globals.
class Program {
public:
    Program(size_t const_bytes);
    ~Program();

    template <typename T>
    T get_global(std::string name, VMFixedStack& globals) {
        auto address = get_global_address(name);
        return globals.at<T>(address);
    }

    template <typename Ret, typename... Args>
    std::function<Ret(VM&, VMFixedStack&, Args...)> method(std::string name) {
        std::vector<std::type_index>& check = _function_ret_params.at(name);
        std::vector<std::type_index> input = {
            typeid(Ret),
            typeid(Args)...
        };
        size_t size = check.size();
        if (input.size() != size) {
            throw "Incorrect number of parameters";
        }
        for (size_t i = 0; i < size; i++) {
            if (check[i] != input[i]) {
                throw "Incorrect parameter";
            }
        }

        size_t address = get_method_address(name);
        size_t stack_size = get_method_metadata(address).stack_size;
        return Callable<Ret, Args...>(*this, address, stack_size);
    }

    // Register a method that is possible to be called by C++.
    void register_method(std::string name, std::vector<std::type_index> types);

    // Generates a fixed stack containing all globals.
    std::shared_ptr<VMFixedStack> generate_state();

    template <typename T>
    void add_global(std::string name) {
        size_t s = runtimesizeof<T>();
        _global_addresses[name] = _globals_size;
        _globals_size += s;
    }
    template <typename T>
    void add_constant(std::string name, T value) {
        size_t s = runtimesizeof<T>();
        size_t loc = _constants.size();
        _constants.reserve(s);
        *_constants.at<T>(loc) = value;
        _constant_addresses[name] = loc;
    }
    void add_builtin(std::string name, std::shared_ptr<IRunnable> runnable);
    void add_method(std::string name, size_t param_size, size_t stack_size, std::vector<Opcode> method_code);

    void add_code(std::vector<Opcode> code);
    void add_global_index(std::string name, size_t size, size_t addr);
    void add_method_addr(std::string name, size_t param_size, size_t stack_size, size_t address);

    size_t get_global_address(std::string name) const;
    size_t get_constant_address(std::string name) const;
    size_t get_builtin_address(std::string name) const;
    size_t get_method_address(std::string name) const;
    size_t current_method_address() const;

    const MethodMetadata& get_method_metadata(std::string name) const;
    const MethodMetadata& get_method_metadata(size_t address) const;

    size_t globals_size() const;
    // const std::vector<std::shared_ptr<IRunnable>>& get_builtins() const;
    const std::vector<Opcode>& get_code() const;

    const Opcode& get_opcode(size_t index) const;

    template <typename T>
    const T get_constant(size_t address) const {
        return _constants.cvalue<T>(address);
    }
    const VMFixedStack& constants_table() const;

    const std::shared_ptr<IRunnable> get_builtin_runnable(size_t addr) const;
    const std::shared_ptr<IRunnable> get_method_runnable(size_t addr) const;

private:
    size_t _globals_size;
    std::vector<std::shared_ptr<IRunnable>> _builtins;
    std::vector<Opcode> _code;
    VMFixedStack _constants;
    //std::unordered_map<size_t, IRunnable*> _methods;
    std::vector<std::shared_ptr<IRunnable>> _methods;

    std::unordered_map<std::string, size_t> _builtin_addresses;
    std::unordered_map<std::string, size_t> _function_addresses;
    std::unordered_map<std::string, std::vector<std::type_index>> _function_ret_params;
    std::unordered_map<size_t, MethodMetadata> _function_metadata;
    std::unordered_map<std::string, size_t> _constant_addresses;
    std::unordered_map<std::string, size_t> _global_addresses;
};
