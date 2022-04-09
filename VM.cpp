#include "VM.h"

#include <iostream>

#include "Program.h"

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

#define ALU_NUMERICALMETHODS(vmtype,realtype) \
        case Bytecode::##vmtype##Set: { \
            _setv<realtype>(constants, globals, _getv<realtype>(constants, globals, oc.l1, oc.p1), oc.l2, oc.p2); \
            break; \
        } \
        case Bytecode::##vmtype##SetFromIndexed: { \
            size_t offset = _getv<int>(constants, globals, oc.l2, oc.p2); \
            char* p = _getptr<char>(constants, globals, oc.l1, oc.p1); \
            p += offset; \
            realtype v = *((realtype*)p); \
            _setv<realtype>(constants, globals, v, oc.l3, oc.p3); \
            break; \
        } \
        case Bytecode::##vmtype##SetIntoIndexed: { \
            size_t offset = _getv<int>(constants, globals, oc.l2, oc.p2); \
            realtype v = _getv<realtype>(constants, globals, oc.l1, oc.p1); \
            char* p = _getptr<char>(constants, globals, oc.l3, oc.p3); \
            p += offset; \
            realtype* ptr = (realtype*)p; \
            *ptr = v; \
            break; \
        } \
        case Bytecode::##vmtype##Add: { \
            _setv<realtype>(constants, globals, aluadd<realtype>(constants, globals, oc) , oc.l3, oc.p3); \
            break; \
        } \
        case Bytecode::##vmtype##Sub: { \
            _setv<realtype>(constants, globals, alusub<realtype>(constants, globals, oc) , oc.l3, oc.p3); \
            break; \
        } \
        case Bytecode::##vmtype##Mul: { \
            _setv<realtype>(constants, globals, alumul<realtype>(constants, globals, oc) , oc.l3, oc.p3); \
            break; \
        } \
        case Bytecode::##vmtype##Div: { \
            _setv<realtype>(constants, globals, aludiv<realtype>(constants, globals, oc) , oc.l3, oc.p3); \
            break; \
        } \
        case Bytecode::##vmtype##Negate: { \
            _setv<realtype>(constants, globals, aluneg<realtype>(constants, globals, oc) , oc.l2, oc.p2); \
            break; \
        }

#define ALU_ORDINALMETHODS(vmtype,realtype) \
        case Bytecode::##vmtype##Less: { \
            _setv<bool>(constants, globals, lt<realtype>(constants, globals, oc) , oc.l3, oc.p3); \
            break; \
        } \
        case Bytecode::##vmtype##LessEqual: { \
            _setv<bool>(constants, globals, le<realtype>(constants, globals, oc) , oc.l3, oc.p3); \
            break; \
        } \
        case Bytecode::##vmtype##Greater: { \
            _setv<bool>(constants, globals, gt<realtype>(constants, globals, oc) , oc.l3, oc.p3); \
            break; \
        } \
        case Bytecode::##vmtype##GreaterEqual: { \
            _setv<bool>(constants, globals, ge<realtype>(constants, globals, oc) , oc.l3, oc.p3); \
            break; \
        }

#define ALU_EQUALMETHODS(vmtype,realtype) \
        case Bytecode::##vmtype##Equal: { \
            _setv<bool>(constants, globals, eq<realtype>(constants, globals, oc) , oc.l3, oc.p3); \
            break; \
        } \
        case Bytecode::##vmtype##NotEqual: { \
            _setv<bool>(constants, globals, ne<realtype>(constants, globals, oc) , oc.l3, oc.p3); \
            break; \
        }

#define ALU_BOOLLOGICMETHODS(vmtype,realtype) \
        case Bytecode::##vmtype##Not: { \
            _setv<bool>(constants, globals, alubitnot<realtype>(constants, globals, oc) , oc.l2, oc.p2); \
            break; \
        } \
        case Bytecode::##vmtype##And: { \
            _setv<bool>(constants, globals, eq<realtype>(constants, globals, oc) , oc.l3, oc.p3); \
            break; \
        } \
        case Bytecode::##vmtype##Or: { \
            _setv<bool>(constants, globals, ne<realtype>(constants, globals, oc) , oc.l3, oc.p3); \
            break; \
        }

#define ALU_JUMPMETHODS(vmtype,realtype) \
        case Bytecode::##vmtype##JLT: { \
            if (lt<realtype>(constants, globals, oc)) { \
                _jump(constants, globals, oc.l3, oc.p3); \
            } \
            break; \
        } \
        case Bytecode::##vmtype##JLE: { \
            if (le<realtype>(constants, globals, oc)) { \
                _jump(constants, globals, oc.l3, oc.p3); \
            } \
            break; \
        } \
        case Bytecode::##vmtype##JGT: { \
            if (gt<realtype>(constants, globals, oc)) { \
                _jump(constants, globals, oc.l3, oc.p3); \
            } \
            break; \
        } \
        case Bytecode::##vmtype##JGE: { \
            if (ge<realtype>(constants, globals, oc)) { \
                _jump(constants, globals, oc.l3, oc.p3); \
            } \
            break; \
        } \
        case Bytecode::##vmtype##JEQ: { \
            if (eq<realtype>(constants, globals, oc)) { \
                _jump(constants, globals, oc.l3, oc.p3); \
            } \
            break; \
        } \
        case Bytecode::##vmtype##JNE: { \
            if (ne<realtype>(constants, globals, oc)) { \
                _jump(constants, globals, oc.l3, oc.p3); \
            } \
            break; \
        }

#define ALU_BITWISEMETHODS(vmtype,realtype) \
        case Bytecode::##vmtype##BitNot: { \
            _setv<realtype>(constants, globals, alubitnot<realtype>(constants, globals, oc) , oc.l2, oc.p2); \
            break; \
        } \
        case Bytecode::##vmtype##BitAnd: { \
            _setv<realtype>(constants, globals, alubitand<realtype>(constants, globals, oc) , oc.l3, oc.p3); \
            break; \
        } \
        case Bytecode::##vmtype##BitOr: { \
            _setv<realtype>(constants, globals, alubitor<realtype>(constants, globals, oc) , oc.l3, oc.p3); \
            break; \
        } \
        case Bytecode::##vmtype##BitXor: { \
            _setv<realtype>(constants, globals, alubitxor<realtype>(constants, globals, oc) , oc.l3, oc.p3); \
            break; \
        } \
        case Bytecode::##vmtype##ShiftLeft: { \
            _setv<realtype>(constants, globals, alubitshl<realtype>(constants, globals, oc) , oc.l3, oc.p3); \
            break; \
        } \
        case Bytecode::##vmtype##ShiftRight: { \
            _setv<realtype>(constants, globals, alubitshr<realtype>(constants, globals, oc) , oc.l3, oc.p3); \
            break; \
        }

VM::VM(size_t stack_size)
    : data(stack_size), _exec_stack(1<<16), _instruction_index(0), _base(0)
{
    _exec_stack_top = 0;
}

VM::~VM() {}

void
VM::clear_state() {
    _exec_stack_top = 0;
    _exec_stack.unreserve_to(0);
    _base = 0;
    data.unreserve_to(0);
    _instruction_index = 0;
}

void
VM::run_method(const Program& program, VMFixedStack& globals, size_t address, size_t stack_size) {
    // set IP to the end, so that returning will jump to the end/break
    size_t program_size = program.get_code().size();
    _instruction_index = program_size;
    _precall(_base, stack_size);
    _instruction_index = address;
    _run_next(program, globals);
}

void
VM::_precall(size_t base, size_t stack_bytes) {
    //std::cout << "precall before " << _exec_stack_top << " " << _base << " " << _instruction_index << "\n";
    //std::cout << "precall " << param_bytes << "  " << stack_bytes << "\n";
    _exec_stack.reserve(3 * sizeof(size_t));
    *_exec_stack.at<size_t>(_exec_stack_top) = _instruction_index;
    *_exec_stack.at<size_t>(_exec_stack_top + sizeof(size_t)) = _base;
    *_exec_stack.at<size_t>(_exec_stack_top + 2*sizeof(size_t)) = data.size();
    _exec_stack_top += 3 * sizeof(size_t);

    _base = base;
    size_t reserve = (base + stack_bytes) - data.size();
    if (reserve > 0) {
        data.reserve(stack_bytes);
    }
}

//void
//VM::_setup_stackframe(size_t stack_size) {
//    _base = data.size();
//    data.reserve(stack_size);
//}

void
VM::_postcall() {
    _exec_stack_top -= 3 * sizeof(size_t);
    _instruction_index = *_exec_stack.at<size_t>(_exec_stack_top);
    _base = *_exec_stack.at<size_t>(_exec_stack_top + sizeof(size_t));
    size_t previous_top = *_exec_stack.at<size_t>(_exec_stack_top + 2*sizeof(size_t));
    size_t unreserve = data.size() - previous_top;
    if (unreserve > 0) {
        data.unreserve(unreserve);
    }
    _exec_stack.unreserve(3 * sizeof(size_t));
    //std::cout << "postcall after " << _exec_stack_top << " " << _base << " " << _instruction_index << "\n";
}

void
VM::_jump(const VMFixedStack& constants, VMFixedStack& globals, DataLoc l, size_t address) {
    if (l == LocMemoryDirect) {
        const size_t page = address_page(address);
        const size_t offset = address_offset(address);
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
        size_t addr = _table_value<size_t>(constants, globals, address);
        _instruction_index = addr;
    }
}

bool
VM::_run_next(const Program& program, VMFixedStack& globals) {
    const auto& constants = program.constants_table();
    size_t program_size = program.get_code().size();

    while (_instruction_index < program_size) {
        const auto& oc = program.get_opcode(_instruction_index);
        //std::cout << _instruction_index << " << " << (int)oc.op << "\n";
        _instruction_index++;

        switch (oc.op) {
        case Bytecode::Break: {
            return false;
            break;
        }
        case Bytecode::DataAddress: {
            size_t address = size_t(_table_ptr<int>(globals, oc.p1));
            _setv<size_t>(constants, globals, address, oc.l2, oc.p2);
            break;
        }
        case Bytecode::FunctionAddress: {
            const size_t page = address_page(oc.p1);
            const size_t offset = address_offset(oc.p1);
            if (page == 0) {
                size_t address = size_t(program.get_method_runnable(offset).get());
                _setv<size_t>(constants, globals, address, oc.l2, oc.p2);
            }
            else {
                size_t address = size_t(program.get_builtin_runnable(offset).get());
                _setv<size_t>(constants, globals, address, oc.l2, oc.p2);
            }
            break;
        }
        case Bytecode::Dereference: {
            size_t size = oc.p2;
            char* src = _getv<char*>(constants, globals, LocMemoryDirect, oc.p1);
            char *dest = _table_ptr<char>(globals, oc.p3);
            memcpy(dest, src, size);
            break;
        }
        case Bytecode::refSet: {
            // Always fetch the ptr as direct to get the ptr itself.
            size_t v = _getv<size_t>(constants, globals, LocMemoryDirect, oc.p1);
            _setv<size_t>(constants, globals, v, LocMemoryDirect, oc.p2);
            break;
        }
        case Bytecode::refAdd: {
            if (oc.l3 == LocMemoryIndirect) {
                char* v = _getv<char*>(constants, globals, LocMemoryDirect, oc.p1);
                size_t offset = _getv<size_t>(constants, globals, oc.l2, oc.p2);
                size_t* r = reinterpret_cast<size_t*>(v + offset);
                _setv<size_t>(constants, globals, *r, LocMemoryDirect, oc.p3);
            }
            else {
                size_t v = _getv<size_t>(constants, globals, LocMemoryDirect, oc.p1);
                size_t offset = _getv<size_t>(constants, globals, oc.l2, oc.p2);
                _setv<size_t>(constants, globals, v + offset, LocMemoryDirect, oc.p3);
            }
            break;
        }
    
        ALU_NUMERICALMETHODS(f32,float)
        ALU_ORDINALMETHODS(f32,float)
        ALU_EQUALMETHODS(f32,float)
        ALU_JUMPMETHODS(f32,float)

        ALU_NUMERICALMETHODS(s32,int)
        ALU_ORDINALMETHODS(s32,int)
        ALU_EQUALMETHODS(s32,int)
        ALU_JUMPMETHODS(s32,int)
        ALU_BITWISEMETHODS(s32,int)

        ALU_BOOLLOGICMETHODS(bool,bool)
        ALU_EQUALMETHODS(bool,bool)

        case Bytecode::FCall: {
            size_t fn_base = _base + address_offset(oc.p2);
            const size_t addr = address_offset(oc.p1);
            size_t stack = oc.p3;
            _precall(fn_base, stack);
            _instruction_index = addr;
            break;
        }

        case Bytecode::Call: {
            size_t fn_base = _base + address_offset(oc.p2);
            if (oc.l1 == LocMemoryDirect) {
                std::shared_ptr<IRunnable> r;
                const size_t page = address_page(oc.p1);
                const size_t offset = address_offset(oc.p1);
                size_t stack = oc.p3;
                switch (page) {
                case 0:
                    r = program.get_method_runnable(offset);
                    r->invoke(*this, data, fn_base);
                    break;
                case 1: {
                    r = program.get_builtin_runnable(offset);
                    r->invoke(*this, data, fn_base);
                    break;
                }
                case 2:
                    _precall(fn_base, stack);
                    _instruction_index += offset;
                    break;
                default:
                case 3:
                    _precall(fn_base, stack);
                    _instruction_index -= offset;
                    break;
                }
            }
            else {
                auto runnable = _table_value<IRunnable*>(constants, globals, oc.p1);
                runnable->invoke(*this, data, fn_base);
            }
            break;
        }
        case Bytecode::Ret: {
            _postcall();
            break;
        }

        case Bytecode::Jump: {
            _jump(constants, globals, oc.l1, oc.p1);
            break;
        }

        case Bytecode::boolJTrue: {
            if (_getv<bool>(constants, globals, oc.l1, oc.p1)) {
                _jump(constants, globals, oc.l2, oc.p2);
            }
            break;
        }
        case Bytecode::boolJFalse: {
            if (!_getv<bool>(constants, globals, oc.l1, oc.p1)) {
                _jump(constants, globals, oc.l2, oc.p2);
            }
            break;
        }

        default:
            return false;
        }
    }
    return true;
}
