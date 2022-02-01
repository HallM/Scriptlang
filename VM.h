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

    void run_method(size_t ip, size_t param_bytes, size_t stack_bytes);

    template<typename T>
    T get_value(size_t stack_address) {
        return _getv<T>(DataLoc::G, GlobalAddress{stack_address});
    }

    template<typename T>
    void set_value(T value, size_t stack_address) {
        _setv<T>(value, DataLoc::G, GlobalAddress{stack_address});
    }

private:
    void _reserve_globals(size_t stack_size);
    void _precall(size_t param_bytes, size_t stack_bytes);
    // void _setup_stackframe(size_t stack_size);
    void _postcall(size_t stack_bytes);
    void _jump(DataLoc l, Opdata d);
    bool _run_next();

    template<typename T>
    T _getv(DataLoc l, Opdata d) {
        if (l == DataLoc::G) {
            //std::cout << "getglobal " << std::get<GlobalAddress>(d).addr << "\n";
            return *data.at<T>(std::get<GlobalAddress>(d).addr);
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
    void _setv(T v, DataLoc l, Opdata d) {
        if (l == DataLoc::G) {
            //std::cout << "setglobal " << std::get<GlobalAddress>(d).addr << " = " << v << "\n";
            *data.at<T>(std::get<GlobalAddress>(d).addr) = v;
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
    NT aluadd(const Opcode& oc) {
        NT a = _getv<NT>(oc.opcode.l1, oc.p1);
        NT b = _getv<NT>(oc.opcode.l2, oc.p2);
        NT r = a + b;
        //std::cout << a << " + " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    NT alusub(const Opcode& oc) {
        NT a = _getv<NT>(oc.opcode.l1, oc.p1);
        NT b = _getv<NT>(oc.opcode.l2, oc.p2);
        NT r = a - b;
        //std::cout << a << " - " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    NT alumul(const Opcode& oc) {
        NT a = _getv<NT>(oc.opcode.l1, oc.p1);
        NT b = _getv<NT>(oc.opcode.l2, oc.p2);
        NT r = a * b;
        //std::cout << a << " * " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    NT aludiv(const Opcode& oc) {
        NT a = _getv<NT>(oc.opcode.l1, oc.p1);
        NT b = _getv<NT>(oc.opcode.l2, oc.p2);
        NT r = a / b;
        //std::cout << a << " / " << b << " = " << r << "\n";
        return r;
    }

    template<typename NT>
    bool lt(const Opcode& oc) {
        NT a = _getv<NT>(oc.opcode.l1, oc.p1);
        NT b = _getv<NT>(oc.opcode.l2, oc.p2);
        bool r = a < b;
        //std::cout << a << " < " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    bool le(const Opcode& oc) {
        NT a = _getv<NT>(oc.opcode.l1, oc.p1);
        NT b = _getv<NT>(oc.opcode.l2, oc.p2);
        bool r = a <= b;
        //std::cout << a << " <= " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    bool gt(const Opcode& oc) {
        NT a = _getv<NT>(oc.opcode.l1, oc.p1);
        NT b = _getv<NT>(oc.opcode.l2, oc.p2);
        bool r = a > b;
        //std::cout << a << " > " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    bool ge(const Opcode& oc) {
        NT a = _getv<NT>(oc.opcode.l1, oc.p1);
        NT b = _getv<NT>(oc.opcode.l2, oc.p2);
        bool r = a >= b;
        //std::cout << a << " >= " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    bool eq(const Opcode& oc) {
        NT a = _getv<NT>(oc.opcode.l1, oc.p1);
        NT b = _getv<NT>(oc.opcode.l2, oc.p2);
        bool r = a == b;
        //std::cout << a << " == " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    bool ne(const Opcode& oc) {
        NT a = _getv<NT>(oc.opcode.l1, oc.p1);
        NT b = _getv<NT>(oc.opcode.l2, oc.p2);
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
