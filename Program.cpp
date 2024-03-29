#include "Program.h"

#include <algorithm>
#include <iterator>

Program::Program(size_t const_bytes) : _globals_size(0), _constants(const_bytes) {}
Program::~Program() {}

std::shared_ptr<VMFixedStack>
Program::generate_state() {
    auto size = globals_size();
    return std::make_shared<VMFixedStack>(size);
}

void
Program::add_builtin(std::string name, std::shared_ptr<IRunnable> runnable) {
    size_t addr = _builtins.size();
    _builtins.push_back(runnable);
    _builtin_addresses[name] = addr;
}

void
Program::add_global_index(std::string name, size_t size, size_t addr) {
    _global_addresses[name] = addr;
    _globals_size = addr + size;
}

void
Program::add_code(std::vector<Opcode> code) {
    std::copy(code.begin(), code.end(), std::back_inserter(_code));
}

void
Program::register_method(std::string name, size_t address, std::vector<std::type_index> types) {
    _function_addresses[name] = address;
    _function_ret_params[name] = types;
}

void
Program::add_method_addr(size_t index, size_t param_size, size_t stack_size, size_t address) {
    _function_metadata[address] = MethodMetadata{param_size, stack_size};
    std::shared_ptr<IRunnable> runnable = std::make_shared<BytecodeRunnable>(address, param_size, stack_size);
    if (index >= _methods.size()) {
        _methods.resize(index + 1);
    }
    _methods[index] = runnable;
}

size_t
Program::get_global_address(std::string name) const {
    return _global_addresses.at(name);
}

size_t
Program::get_constant_address(std::string name) const {
    return _constant_addresses.at(name);
}

size_t
Program::get_builtin_address(std::string name) const {
    return _builtin_addresses.at(name);
}

size_t
Program::get_method_address(std::string name) const {
    return _function_addresses.at(name);
}

size_t
Program::current_method_address() const {
    return _code.size();
}

const MethodMetadata&
Program::get_method_metadata(std::string name) const {
    size_t address = _function_addresses.at(name);
    return _function_metadata.at(address);
}

const MethodMetadata&
Program::get_method_metadata(size_t address) const {
    return _function_metadata.at(address);
}

size_t
Program::globals_size() const {
    return _globals_size;
}

//const std::vector<IRunnable*>&
//Program::get_builtins() const {
//    return _builtins;
//}

const std::vector<Opcode>&
Program::get_code() const {
    return _code;
}

const Opcode&
Program::get_opcode(size_t index) const {
    return _code.at(index);
}

const VMFixedStack&
Program::constants_table() const {
    return _constants;
}

const std::shared_ptr<IRunnable>
Program::get_builtin_runnable(size_t addr) const {
    return _builtins.at(addr);
}

const std::shared_ptr<IRunnable>
Program::get_method_runnable(size_t addr) const {
    return _methods.at(addr);
}
