#include "Program.h"

#include <algorithm>
#include <iterator>

Program::Program() : _globals_size(0) {}
Program::~Program() {}

void
Program::add_builtin(std::string name, IRunnable* runnable) {
    size_t addr = _builtins.size();
    _builtins.push_back(runnable);
    _builtin_addresses[name] = addr;
}

void
Program::add_method(std::string name, std::vector<Opcode> method_code) {
    size_t addr = _code.size();
    std::copy(method_code.begin(), method_code.end(), std::back_inserter(_code));
    _function_addresses[name] = addr;
}

size_t
Program::get_global(std::string name) const {
    return _global_addresses.at(name);
}

size_t
Program::get_builtin(std::string name) const {
    return _builtin_addresses.at(name);
}

size_t
Program::get_method(std::string name) const {
    return _function_addresses.at(name);
}

size_t
Program::current_method() const {
    return _code.size();
}

size_t
Program::globals_size() const {
    return _globals_size;
}

const std::vector<IRunnable*>&
Program::get_builtins() const {
    return _builtins;
}

const std::vector<Opcode>&
Program::get_code() const {
    return _code;
}
