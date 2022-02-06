#pragma once

#include "VMBytecode.h"
#include "VMStack.h"
#include "VMFFI.h"
#include "Program.h"

#include <iostream>
#include <type_traits>

class BytecodeRunnable;

class VM {
public:
    VM(size_t stack_size);
    ~VM();

    void run_method(const Program& program, std::string method_name, VMFixedStack& globals);

private:
    friend BytecodeRunnable;

    void _precall(size_t param_bytes, size_t stack_bytes);
    // void _setup_stackframe(size_t stack_size);
    void _postcall(size_t stack_bytes);
    void _jump(const Program& program, VMFixedStack& globals, DataLoc l, size_t d);
    bool _run_next(const Program& program, VMFixedStack& globals);

    template<typename T>
    T _table_value(const Program& program, VMFixedStack& globals, size_t address) {
        const size_t page = (address & ParamAddressPageMask) >> ParamAddressPageBit;
        const size_t offset = address & ParamAddressOffsetMask;
        switch (page) {
            case 0: {
                //std::cout << "get constant " << offset << "\n";
                return program.get_constant<T>(offset);
            }
            case 1: {
                //std::cout << "get global " << offset << "\n";
                return *globals.at<T>(offset);
            }
            case 2: {
                //std::cout << "get +offset " << offset << ": " << _base+offset << "\n";
                return *data.at<T>(_base + offset);
            }
            default: {
                //std::cout << "get -offset " << offset << ": " << _base-offset << "\n";
                return *data.at<T>(_base - offset);
            }
        }
    }
    template<typename T>
    T* _table_ptr(VMFixedStack& globals, size_t address) {
        const size_t page = (address & ParamAddressPageMask) >> ParamAddressPageBit;
        const size_t offset = address & ParamAddressOffsetMask;

        switch (page) {
            case 0: {
                //std::cout << "try set constant (bad!) " << offset << "\n";
                return nullptr;
            }
            case 1: {
                //std::cout << "set global " << offset << "\n";
                return globals.at<T>(offset);
            }
            case 2: {
                //std::cout << "set +offset " << offset << ": " << _base+offset << "\n";
                return data.at<T>(_base + offset);
            }
            default: {
                //std::cout << "set -offset " << offset << ": " << _base-offset << "\n";
                return data.at<T>(_base - offset);
            }
        }
    }

    template<typename T>
    T _getv(const Program& program, VMFixedStack& globals, DataLoc l, size_t address) {
        switch (l) {
            case LocMemoryIndirect: {
                T* ptr = _table_value<T*>(program, globals, address);
                return *ptr;
            }
            default: {
                return _table_value<T>(program, globals, address);
            }
        }
    }
    template<typename T>
    void _setv(const Program& program, VMFixedStack& globals, T v, DataLoc l, size_t address) {
        switch (l) {
            case LocMemoryIndirect: {
                T* ptr = _table_value<T*>(program, globals, address);
                *ptr = v;
                break;
            }
            default: {
                *_table_ptr<T>(globals, address) = v;
                break;
            }
        }
    }

    template<typename NT>
    NT aluadd(const Program& program, VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(program, globals, oc.l1, oc.p1);
        NT b = _getv<NT>(program, globals, oc.l2, oc.p2);
        NT r = a + b;
        //std::cout << a << " + " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    NT alusub(const Program& program, VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(program, globals, oc.l1, oc.p1);
        NT b = _getv<NT>(program, globals, oc.l2, oc.p2);
        NT r = a - b;
        //std::cout << a << " - " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    NT alumul(const Program& program, VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(program, globals, oc.l1, oc.p1);
        NT b = _getv<NT>(program, globals, oc.l2, oc.p2);
        NT r = a * b;
        //std::cout << a << " * " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    NT aludiv(const Program& program, VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(program, globals, oc.l1, oc.p1);
        NT b = _getv<NT>(program, globals, oc.l2, oc.p2);
        NT r = a / b;
        //std::cout << a << " / " << b << " = " << r << "\n";
        return r;
    }

    template<typename NT>
    bool lt(const Program& program, VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(program, globals, oc.l1, oc.p1);
        NT b = _getv<NT>(program, globals, oc.l2, oc.p2);
        bool r = a < b;
        //std::cout << a << " < " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    bool le(const Program& program, VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(program, globals, oc.l1, oc.p1);
        NT b = _getv<NT>(program, globals, oc.l2, oc.p2);
        bool r = a <= b;
        //std::cout << a << " <= " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    bool gt(const Program& program, VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(program, globals, oc.l1, oc.p1);
        NT b = _getv<NT>(program, globals, oc.l2, oc.p2);
        bool r = a > b;
        //std::cout << a << " > " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    bool ge(const Program& program, VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(program, globals, oc.l1, oc.p1);
        NT b = _getv<NT>(program, globals, oc.l2, oc.p2);
        bool r = a >= b;
        //std::cout << a << " >= " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    bool eq(const Program& program, VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(program, globals, oc.l1, oc.p1);
        NT b = _getv<NT>(program, globals, oc.l2, oc.p2);
        bool r = a == b;
        //std::cout << a << " == " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    bool ne(const Program& program, VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(program, globals, oc.l1, oc.p1);
        NT b = _getv<NT>(program, globals, oc.l2, oc.p2);
        bool r = a != b;
        //std::cout << a << " != " << b << " = " << r << "\n";
        return r;
    }

    size_t _instruction_index;
    size_t _base;

    VMFixedStack _exec_stack;
    size_t _exec_stack_top;
    VMFixedStack data;
};
