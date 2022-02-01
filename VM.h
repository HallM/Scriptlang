#pragma once

#include "VMBytecode.h"
#include "VMStack.h"
#include "VMFFI.h"
#include "Program.h"

#include <iostream>

class VM {
public:
    VM(size_t stack_size, const Program program);
    ~VM();

    void run_method(VMFixedStack& globals, size_t ip, size_t param_bytes, size_t stack_bytes);

private:
    void _precall(size_t param_bytes, size_t stack_bytes);
    // void _setup_stackframe(size_t stack_size);
    void _postcall(size_t stack_bytes);
    void _jump(DataLoc l, Opdata d);
    bool _run_next(VMFixedStack& globals);

    template<typename T>
    T _getv(VMFixedStack& globals, DataLoc l, Opdata d) {
        if (l == DataLoc::G) {
            //std::cout << "getglobal " << std::get<GlobalAddress>(d).addr << "\n";
            return *globals.at<T>(std::get<GlobalAddress>(d).addr);
        } else if (l == DataLoc::L) {
            //std::cout << "base " << _base << "\n";
            //std::cout << "getlocal " << _base+std::get<LocalAddress>(d).posoffset << "\n";
            return *data.at<T>(_base + std::get<LocalAddress>(d).posoffset);
        //} else if (l == DataLoc::P) {
        //    //std::cout << "getparam " << base-std::get<ParamAddress>(d).negoffset << "\n";
        //    return *data.at<T>(_base - std::get<ParamAddress>(d).negoffset);
        }
        return std::get<T>(d);
    }
    template<typename T>
    void _setv(VMFixedStack& globals, T v, DataLoc l, Opdata d) {
        if (l == DataLoc::G) {
            //std::cout << "setglobal " << std::get<GlobalAddress>(d).addr << " = " << v << "\n";
            *globals.at<T>(std::get<GlobalAddress>(d).addr) = v;
        } else if (l == DataLoc::L) {
            //std::cout << "base " << _base << "\n";
            //std::cout << "setlocal " << _base+std::get<LocalAddress>(d).posoffset << " = " << v << "\n";
            *data.at<T>(_base + std::get<LocalAddress>(d).posoffset) = v;
        //} else if (l == DataLoc::P) {
        //    //std::cout << "setparam " << base-std::get<ParamAddress>(d).negoffset << " = " << v << "\n";
        //    *data.at<T>(_base - std::get<ParamAddress>(d).negoffset) = v;
        }
    }

    template<typename NT>
    NT aluadd(VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(globals, oc.opcode.l1, oc.p1);
        NT b = _getv<NT>(globals, oc.opcode.l2, oc.p2);
        NT r = a + b;
        //std::cout << a << " + " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    NT alusub(VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(globals, oc.opcode.l1, oc.p1);
        NT b = _getv<NT>(globals, oc.opcode.l2, oc.p2);
        NT r = a - b;
        //std::cout << a << " - " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    NT alumul(VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(globals, oc.opcode.l1, oc.p1);
        NT b = _getv<NT>(globals, oc.opcode.l2, oc.p2);
        NT r = a * b;
        //std::cout << a << " * " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    NT aludiv(VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(globals, oc.opcode.l1, oc.p1);
        NT b = _getv<NT>(globals, oc.opcode.l2, oc.p2);
        NT r = a / b;
        //std::cout << a << " / " << b << " = " << r << "\n";
        return r;
    }

    template<typename NT>
    bool lt(VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(globals, oc.opcode.l1, oc.p1);
        NT b = _getv<NT>(globals, oc.opcode.l2, oc.p2);
        bool r = a < b;
        //std::cout << a << " < " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    bool le(VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(globals, oc.opcode.l1, oc.p1);
        NT b = _getv<NT>(globals, oc.opcode.l2, oc.p2);
        bool r = a <= b;
        //std::cout << a << " <= " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    bool gt(VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(globals, oc.opcode.l1, oc.p1);
        NT b = _getv<NT>(globals, oc.opcode.l2, oc.p2);
        bool r = a > b;
        //std::cout << a << " > " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    bool ge(VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(globals, oc.opcode.l1, oc.p1);
        NT b = _getv<NT>(globals, oc.opcode.l2, oc.p2);
        bool r = a >= b;
        //std::cout << a << " >= " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    bool eq(VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(globals, oc.opcode.l1, oc.p1);
        NT b = _getv<NT>(globals, oc.opcode.l2, oc.p2);
        bool r = a == b;
        //std::cout << a << " == " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    bool ne(VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(globals, oc.opcode.l1, oc.p1);
        NT b = _getv<NT>(globals, oc.opcode.l2, oc.p2);
        bool r = a != b;
        //std::cout << a << " != " << b << " = " << r << "\n";
        return r;
    }

    const Program _program;

    size_t _instruction_index;
    size_t _base;

    VMFixedStack _exec_stack;
    size_t _exec_stack_top;
    VMFixedStack data;
};
