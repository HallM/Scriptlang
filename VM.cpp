#include "VM.h"

#include <iostream>

VM::VM(size_t stack_size)
    : data(stack_size), _exec_stack(1<<16), _instruction_index(0), _base(0)
{
    _exec_stack_top = 0;
}

VM::~VM() {}

void
VM::run_method(const Program& program, std::string method_name, VMFixedStack& globals) {
    auto& metadata = program.get_method_metadata(method_name);
    size_t s = program.get_code().size();
    _instruction_index = s;
    _precall(metadata.param_size, metadata.stack_size);
    _instruction_index = program.get_method_address(method_name);
    while (_instruction_index < s && _run_next(program, globals)) {}
}

void
VM::_precall(size_t param_bytes, size_t stack_bytes) {
    //std::cout << "precall " << param_bytes << "  " << stack_bytes << "\n";
    //std::cout << "precall before " << _exec_stack_top  << "\n";
    _exec_stack.reserve(2 * sizeof(size_t));
    *_exec_stack.at<size_t>(_exec_stack_top) = _instruction_index;
    //std::cout << "oldb " << _base << " -> " << data.size() - param_bytes << "\n";
    *_exec_stack.at<size_t>(_exec_stack_top + sizeof(size_t)) = _base;
    _exec_stack_top += 2 * sizeof(size_t);
    //std::cout << "precall after " << _exec_stack_top  << "\n";

    _base = data.size() - param_bytes;
    data.reserve(stack_bytes);
}

//void
//VM::_setup_stackframe(size_t stack_size) {
//    _base = data.size();
//    data.reserve(stack_size);
//}

void
VM::_postcall(size_t stack_bytes) {
    //std::cout << "postcall before " << _exec_stack_top  << "\n";
    _exec_stack_top -= 2 * sizeof(size_t);
    data.unreserve(stack_bytes);
    _instruction_index = *_exec_stack.at<size_t>(_exec_stack_top);
    //std::cout << "popb " << _base << " -> ";
    _base = *_exec_stack.at<size_t>(_exec_stack_top + sizeof(size_t));
    //std::cout << _base << "\n";
    _exec_stack.unreserve(2 * sizeof(size_t));
    //std::cout << "postcall after " << _exec_stack_top  << "\n";
}

void
VM::_jump(DataLoc l, Opdata d) {
    if (l == DataLoc::P) {
        _instruction_index -= d.param_address;
    }
    else if (l == DataLoc::L) {
        _instruction_index += d.local_address;
    }
    else if (l == DataLoc::G) {
        _instruction_index = d.global_address;
    }
}

bool
VM::_run_next(const Program& program, VMFixedStack& globals) {
    const auto& oc = program.get_code()[_instruction_index];
    //std::cout << _instruction_index << " << " << (int)oc.opcode.op << "\n";
    _instruction_index++;

    switch (oc.opcode.op) {
    case Bytecode::Break: {
        return false;
        break;
    }
    case Bytecode::f32Set: {
        _setv<float>(globals, _getv<float>(globals, oc.opcode.l1, oc.p1), oc.opcode.l2, oc.p2);
        break;
    }
    case Bytecode::f32Add: {
        _setv<float>(globals, aluadd<float>(globals, oc) , oc.opcode.l3, oc.p3);
        break;
    }
    case Bytecode::f32Sub: {
        _setv<float>(globals, alusub<float>(globals, oc) , oc.opcode.l3, oc.p3);
        break;
    }
    case Bytecode::f32Mul: {
        _setv<float>(globals, alumul<float>(globals, oc) , oc.opcode.l3, oc.p3);
        break;
    }
    case Bytecode::f32Div: {
        _setv<float>(globals, aludiv<float>(globals, oc) , oc.opcode.l3, oc.p3);
        break;
    }
    case Bytecode::s32Set: {
        _setv<int>(globals, _getv<int>(globals, oc.opcode.l1, oc.p1), oc.opcode.l2, oc.p2);
        break;
    }
    case Bytecode::s32Add: {
        _setv<int>(globals, aluadd<int>(globals, oc) , oc.opcode.l3, oc.p3);
        break;
    }
    case Bytecode::s32Sub: {
        _setv<int>(globals, alusub<int>(globals, oc) , oc.opcode.l3, oc.p3);
        break;
    }
    case Bytecode::s32Mul: {
        _setv<int>(globals, alumul<int>(globals, oc) , oc.opcode.l3, oc.p3);
        break;
    }
    case Bytecode::s32Div: {
        _setv<int>(globals, aludiv<int>(globals, oc) , oc.opcode.l3, oc.p3);
        break;
    }

    case Bytecode::CallExtern: {
        size_t fn = oc.p1.global_address;
        IRunnable* r = program.get_builtins()[fn];
        r->invoke(data, data.size());
        break;
    }
    //case Bytecode::BeginFrame: {
    //    auto size = std::get<GlobalAddress>(oc.p1).addr;
    //    _setup_stackframe(size);
    //    break;
    //}
    case Bytecode::Call: {
        size_t param_bytes = oc.p2.local_address;
        size_t stack_bytes = oc.p3.local_address;
        _precall(param_bytes, stack_bytes);
        _instruction_index = oc.p1.global_address;
        break;
    }
    case Bytecode::Ret: {
        size_t stack_bytes = oc.p3.local_address;
        _postcall(stack_bytes);
        break;
    }

    case Bytecode::Jump: {
        _jump(oc.opcode.l1, oc.p1);
        break;
    }

    case Bytecode::f32JLT: {
        if (lt<float>(globals, oc)) {
            _jump(oc.opcode.l3, oc.p3);
        }
        break;
    }
    case Bytecode::f32JLE: {
        if (le<float>(globals, oc)) {
            _jump(oc.opcode.l3, oc.p3);
        }
        break;
    }
    case Bytecode::f32JGT: {
        if (gt<float>(globals, oc)) {
            _jump(oc.opcode.l3, oc.p3);
        }
        break;
    }
    case Bytecode::f32JGE: {
        if (ge<float>(globals, oc)) {
            _jump(oc.opcode.l3, oc.p3);
        }
        break;
    }
    case Bytecode::f32JEQ: {
        if (eq<float>(globals, oc)) {
            _jump(oc.opcode.l3, oc.p3);
        }
        break;
    }
    case Bytecode::f32JNE: {
        if (ne<float>(globals, oc)) {
            _jump(oc.opcode.l3, oc.p3);
        }
        break;
    }


    case Bytecode::s32JLT: {
        if (lt<int>(globals, oc)) {
            _jump(oc.opcode.l3, oc.p3);
        }
        break;
    }
    case Bytecode::s32JLE: {
        if (le<int>(globals, oc)) {
            _jump(oc.opcode.l3, oc.p3);
        }
        break;
    }
    case Bytecode::s32JGT: {
        if (gt<int>(globals, oc)) {
            _jump(oc.opcode.l3, oc.p3);
        }
        break;
    }
    case Bytecode::s32JGE: {
        if (ge<int>(globals, oc)) {
            _jump(oc.opcode.l3, oc.p3);
        }
        break;
    }
    case Bytecode::s32JEQ: {
        if (eq<int>(globals, oc)) {
            _jump(oc.opcode.l3, oc.p3);
        }
        break;
    }
    case Bytecode::s32JNE: {
        if (ne<int>(globals, oc)) {
            _jump(oc.opcode.l3, oc.p3);
        }
        break;
    }

    default:
        return false;
    }
    return true;
}
