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
    T _getv(const Program& program, VMFixedStack& globals, DataLoc l, size_t d) {
        if (l == DataLoc::G) {
            //std::cout << "getglobal " << d.global_address << "\n";
            return *globals.at<T>(d);
        } else if (l == DataLoc::O) {
            //std::cout << "base " << _base << "\n";
            //std::cout << "getlocal " << _base+d.local_address << "\n";
            return *data.at<T>(_base + d);
        //} else if (l == DataLoc::R) {
        //    //std::cout << "getparam " << base-std::get<ParamAddress>(d).negoffset << "\n";
        //    return *data.at<T>(_base - std::get<ParamAddress>(d).negoffset);
        }
        else if (l == DataLoc::C) {
            return program.get_constant<T>(d);
        }
        return (T)0;
    }
    template<typename T>
    void _setv(VMFixedStack& globals, T v, DataLoc l, size_t d) {
        if (l == DataLoc::G) {
            //std::cout << "setglobal " << d.global_address << " = " << v << "\n";
            *globals.at<T>(d) = v;
        } else if (l == DataLoc::O) {
            //std::cout << "base " << _base << "\n";
            //std::cout << "setlocal " << _base+d.local_address << " = " << v << "\n";
            *data.at<T>(_base + d) = v;
        //} else if (l == DataLoc::P) {
        //    //std::cout << "setparam " << base-std::get<ParamAddress>(d).negoffset << " = " << v << "\n";
        //    *data.at<T>(_base - std::get<ParamAddress>(d).negoffset) = v;
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
