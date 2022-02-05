#pragma once

#include "VMBytecode.h"
#include "VMStack.h"
#include "VMFFI.h"
#include "Program.h"

#include <iostream>
#include <type_traits>

class VM {
public:
    VM(size_t stack_size);
    ~VM();

    void run_method(const Program& program, std::string method_name, VMFixedStack& globals);

private:
    void _precall(size_t param_bytes, size_t stack_bytes);
    // void _setup_stackframe(size_t stack_size);
    void _postcall(size_t stack_bytes);
    void _jump(DataLoc l, size_t d);
    bool _run_next(const Program& program, VMFixedStack& globals);

    template<typename T>
    T _table_value(const Program& program, VMFixedStack& globals, DataLoc l, size_t address) {
        switch (l & LocTableBits) {
            case LocTableConst:
                return program.get_constant<T>(address);
            case LocTableGlobal:
                return *globals.at<T>(address);
            case LocTableLocal:
                return *data.at<T>(_base + address);
            case LocTableBack:
                return *data.at<T>(_base - address);
        }
        return (T)address;
    }

    template<typename T>
    T _getv(const Program& program, VMFixedStack& globals, DataLoc l, size_t d) {
        size_t addr = d;
        DataLoc table = l;
        if (l & LocIndirectBits) {
            addr = _table_value<size_t>(program, globals, l, d);
            table = ((l & LocIndirectBits) >> 2) - 1;
        }
        return _table_value<T>(program, globals, table, addr);
    }
    template<typename T>
    void _setv(const Program& program, VMFixedStack& globals, T v, DataLoc l, size_t d) {
        size_t addr = d;
        DataLoc table = l;
        if (l & LocIndirectBits) {
            addr = _table_value<size_t>(program, globals, l, d);
            table = ((l & LocIndirectBits) >> 2) - 1;
        }
        switch (table & LocTableBits) {
            case LocTableGlobal:
                *globals.at<T>(addr) = v;
                break;
            case LocTableLocal:
                *data.at<T>(_base + addr) = v;
                break;
            case LocTableBack:
                *data.at<T>(_base - addr) = v;
                break;
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
