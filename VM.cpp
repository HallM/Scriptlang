#include "VM.h"

#include <iostream>

VM::VM(size_t stack_size, const Program program)
    : data(stack_size), _exec_stack(1<<16), _program(program), _instruction_index(0), _base(0)
{
    _exec_stack_top = 0;
}

VM::~VM() {}

void
VM::run_method(VMFixedStack& globals, size_t ip, size_t param_bytes, size_t stack_bytes) {
    size_t s = _program.get_code().size();
    _instruction_index = s;
    _precall(param_bytes, stack_bytes);
    _instruction_index = ip;
    while (_instruction_index < s && _run_next(globals)) {}
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
        _instruction_index -= std::get<ParamAddress>(d).negoffset;
    }
    else if (l == DataLoc::L) {
        _instruction_index += std::get<LocalAddress>(d).posoffset;
    }
    else if (l == DataLoc::G) {
        _instruction_index = std::get<GlobalAddress>(d).addr;
    }
}

bool
VM::_run_next(VMFixedStack& globals) {
    const auto& oc = _program.get_code()[_instruction_index];
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
        size_t fn = std::get<GlobalAddress>(oc.p1).addr;
        IRunnable* r = _program.get_builtins()[fn];
        r->invoke(data, data.size());
        break;
    }
    //case Bytecode::BeginFrame: {
    //    auto size = std::get<GlobalAddress>(oc.p1).addr;
    //    _setup_stackframe(size);
    //    break;
    //}
    case Bytecode::Call: {
        size_t param_bytes = std::get<LocalAddress>(oc.p2).posoffset;
        size_t stack_bytes = std::get<LocalAddress>(oc.p3).posoffset;
        _precall(param_bytes, stack_bytes);
        _instruction_index = std::get<GlobalAddress>(oc.p1).addr;
        break;
    }
    case Bytecode::Ret: {
        size_t stack_bytes = std::get<LocalAddress>(oc.p3).posoffset;
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
