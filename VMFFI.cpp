#include "VMFFI.h"
#include "VM.h"
#include "VMStack.h"

BytecodeRunnable::BytecodeRunnable(size_t address, size_t param_size, size_t stack_reserve) :
    _address(address), _param_size(param_size), _stack_reserve(stack_reserve) {}

BytecodeRunnable::~BytecodeRunnable() {}

void
BytecodeRunnable::invoke(VM& vm, VMFixedStack& s, size_t base) const {
    vm._precall(_param_size, _stack_reserve);
    vm._instruction_index = _address;
}
