#include "VM.h"

#include <iostream>

//
// Items left:
// 0. returned values (solidified)
// 1. objects
// 2. refs
// 3. arrays
// 4. strings
// 5. FFI with the above items
// 6. bytecode finalization
// 7. AST -> bytecode + linker
// 8. text -> AST
// 
// Returns
// I'm making a different assumption. If you've defined a function to return,
// you want that return value. So callee always allocates return space.
// Re-using param stack?
// Pros: less stack usage. maybe.
// Cons: may require copying params.
//       more complex: have to worry about return-size > param size.
// 
// Objects
// An object on a stack is just values back to back.
// accessing a value of one object is no different than accessing any value
// Same could be said for arrays... sorta. cept we need refs/indexing
// Passing an object by value will just copy the ENTIRE object over.
// We may want a more efficient memcopy.
// 
// Object Methods
// It's just a normal method, but the first param is a ref to the object
// 
// Refs
// Need indirect registers.
// register stores the size_t address of the actual value
// Then we need a DataLoc for indirect. That's about it.
// 
// Arrays
// Stack allocated aka locals are similar to objects.
// Shoot, we can even preallocated fixed access.
// But indexed via variable access will require indirect access.
// Option 1: add the address(ref) of the array start + the offset as an u32Add
// Option 2: add a bytecode type specifically for doing this.
// Personally I am leaning option 1, simplest. 2 requires potentially LARGE bytecodes.
// 
// Strings
// They are objects with an array on the inside.
// Depending on how C++ u32string works, I may be ok having my own string type
// Not null terminated, but an object{len: u32, data: [u32; len]}
// Maybe different string types (ascii only u8string | unicode u32string)
// After all, a huge number of games could get by on ascii-only.
// But this should be mappable to, maybe built in, C++
// 
// 
// FFI
// Functions are figured out. Just need to collect metadata for compiler.
// 
// Objects
// The data members, types, sizes, and offsets are required.
// Adding an object type to compiler makes the object available under than name.
// No matching def required in language.
// Just going to avoid having to duplicate + marshalling
// 
// Should we ever have the ability to map a script object to a C++ object?
// I'm thinking no. The C++ type should exist and just generate the internal type.
// 
// Object methods
// The functions are registered similar to a method, but as an obj method.
// Nothing more is needed. C++ already can do invoke on a class method.
//

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
VM::_jump(const Program& program, VMFixedStack& globals, DataLoc l, size_t address) {
    if (l == LocMemoryDirect) {
        const size_t page = (address & ParamAddressPageMask) >> ParamAddressPageBit;
        const size_t offset = address & ParamAddressOffsetMask;
        switch (page) {
            case 0:
            case 1:
                _instruction_index = offset;
                break;
            case 2:
                _instruction_index += offset;
                break;
            case 3:
                _instruction_index -= offset;
                break;
        }
    }
    else {
        size_t addr = _table_value<size_t>(program, globals, address);
        _instruction_index = addr;
    }
}

bool
VM::_run_next(const Program& program, VMFixedStack& globals) {
    const auto& oc = program.get_opcode(_instruction_index);
    // std::cout << _instruction_index << " << " << (int)oc.op << "\n";
    _instruction_index++;

    switch (oc.op) {
    case Bytecode::Break: {
        return false;
        break;
    }
    case Bytecode::AddressOf: {
        size_t address = size_t(_table_ptr<int*>(globals, oc.p1));
        _setv<size_t>(program, globals, address, oc.l2, oc.p2);
        break;
    }
    case Bytecode::f32Set: {
        _setv<float>(program, globals, _getv<float>(program, globals, oc.l1, oc.p1), oc.l2, oc.p2);
        break;
    }
    case Bytecode::f32Add: {
        _setv<float>(program, globals, aluadd<float>(program, globals, oc) , oc.l3, oc.p3);
        break;
    }
    case Bytecode::f32Sub: {
        _setv<float>(program, globals, alusub<float>(program, globals, oc) , oc.l3, oc.p3);
        break;
    }
    case Bytecode::f32Mul: {
        _setv<float>(program, globals, alumul<float>(program, globals, oc) , oc.l3, oc.p3);
        break;
    }
    case Bytecode::f32Div: {
        _setv<float>(program, globals, aludiv<float>(program, globals, oc) , oc.l3, oc.p3);
        break;
    }
    case Bytecode::s32Set: {
        _setv<int>(program, globals, _getv<int>(program, globals, oc.l1, oc.p1), oc.l2, oc.p2);
        break;
    }
    case Bytecode::s32Add: {
        _setv<int>(program, globals, aluadd<int>(program, globals, oc) , oc.l3, oc.p3);
        break;
    }
    case Bytecode::s32Sub: {
        _setv<int>(program, globals, alusub<int>(program, globals, oc) , oc.l3, oc.p3);
        break;
    }
    case Bytecode::s32Mul: {
        _setv<int>(program, globals, alumul<int>(program, globals, oc) , oc.l3, oc.p3);
        break;
    }
    case Bytecode::s32Div: {
        _setv<int>(program, globals, aludiv<int>(program, globals, oc) , oc.l3, oc.p3);
        break;
    }

    case Bytecode::CallExtern: {
        size_t fn = oc.p1;
        IRunnable* r = program.get_builtins()[fn];
        r->invoke(data, data.size());
        break;
    }
    case Bytecode::Call: {
        // TODO: indirect calls
        size_t param_bytes = oc.p2;
        size_t stack_bytes = oc.p3;
        _precall(param_bytes, stack_bytes);
        _instruction_index = oc.p1;
        break;
    }
    case Bytecode::Ret: {
        size_t stack_bytes = oc.p1;
        _postcall(stack_bytes);
        break;
    }

    case Bytecode::Jump: {
        _jump(program, globals, oc.l1, oc.p1);
        break;
    }

    case Bytecode::f32JLT: {
        if (lt<float>(program, globals, oc)) {
            _jump(program, globals, oc.l3, oc.p3);
        }
        break;
    }
    case Bytecode::f32JLE: {
        if (le<float>(program, globals, oc)) {
            _jump(program, globals, oc.l3, oc.p3);
        }
        break;
    }
    case Bytecode::f32JGT: {
        if (gt<float>(program, globals, oc)) {
            _jump(program, globals, oc.l3, oc.p3);
        }
        break;
    }
    case Bytecode::f32JGE: {
        if (ge<float>(program, globals, oc)) {
            _jump(program, globals, oc.l3, oc.p3);
        }
        break;
    }
    case Bytecode::f32JEQ: {
        if (eq<float>(program, globals, oc)) {
            _jump(program, globals, oc.l3, oc.p3);
        }
        break;
    }
    case Bytecode::f32JNE: {
        if (ne<float>(program, globals, oc)) {
            _jump(program, globals, oc.l3, oc.p3);
        }
        break;
    }


    case Bytecode::s32JLT: {
        if (lt<int>(program, globals, oc)) {
            _jump(program, globals, oc.l3, oc.p3);
        }
        break;
    }
    case Bytecode::s32JLE: {
        if (le<int>(program, globals, oc)) {
            _jump(program, globals, oc.l3, oc.p3);
        }
        break;
    }
    case Bytecode::s32JGT: {
        if (gt<int>(program, globals, oc)) {
            _jump(program, globals, oc.l3, oc.p3);
        }
        break;
    }
    case Bytecode::s32JGE: {
        if (ge<int>(program, globals, oc)) {
            _jump(program, globals, oc.l3, oc.p3);
        }
        break;
    }
    case Bytecode::s32JEQ: {
        if (eq<int>(program, globals, oc)) {
            _jump(program, globals, oc.l3, oc.p3);
        }
        break;
    }
    case Bytecode::s32JNE: {
        if (ne<int>(program, globals, oc)) {
            _jump(program, globals, oc.l3, oc.p3);
        }
        break;
    }

    default:
        return false;
    }
    return true;
}
