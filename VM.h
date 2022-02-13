#pragma once

#include "VMBytecode.h"
#include "VMStack.h"
#include "VMFFI.h"

#include <iostream>
#include <type_traits>

class Program;
class BytecodeRunnable;

class VM {
public:
    VM(size_t stack_size);
    ~VM();

    // Only needed when calling an exported method with a return and no params.
    template <typename T>
    size_t reserve_return() {
        size_t addr = data.size();
        data.reserve(sizeof(T));
        return addr;
    }
    template <typename T>
    size_t push_parameters(T v) {
        size_t addr = data.size();
        data.reserve(sizeof(T));
        *data.at<T>(addr) = v;
        return addr;
    }
    template <typename Tf, typename Ts, typename... Tr>
    size_t push_parameters(Tf f, Ts s, Tr... r) {
        size_t ret_address = push_parameters<Tf>(f);
        push_parameters<Ts, Tr...>(s, r...);
        return ret_address;
    }
    template <typename T>
    T get_return(size_t addr) {
        return *data.at<T>(addr);
    }

    void run_method(const Program& program, std::string method_name, VMFixedStack& globals);

private:
    friend BytecodeRunnable;

    void _precall(size_t param_bytes, size_t stack_bytes);
    // void _setup_stackframe(size_t stack_size);
    void _postcall();
    void _jump(const VMFixedStack& constants, VMFixedStack& globals, DataLoc l, size_t d);
    bool _run_next(const Program& program, VMFixedStack& globals);

    template<typename T>
    T _table_value(const VMFixedStack& constants, VMFixedStack& globals, size_t address) {
        const size_t page = address_page(address);
        const size_t offset = address_offset(address);
        switch (page) {
            case 0: {
                //std::cout << "get constant " << offset << "\n";
                return constants.cvalue<T>(offset);
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
        const size_t page = address_page(address);
        const size_t offset = address_offset(address);

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
    T _getv(const VMFixedStack& constants, VMFixedStack& globals, DataLoc l, size_t address) {
        switch (l) {
            case LocMemoryIndirect: {
                T* ptr = _table_value<T*>(constants, globals, address);
                return *ptr;
            }
            default: {
                return _table_value<T>(constants, globals, address);
            }
        }
    }
    template<typename T>
    void _setv(const VMFixedStack& constants, VMFixedStack& globals, T v, DataLoc l, size_t address) {
        switch (l) {
            case LocMemoryIndirect: {
                T* ptr = _table_value<T*>(constants, globals, address);
                *ptr = v;
                break;
            }
            default: {
                *_table_ptr<T>(globals, address) = v;
                //std::cout << "set something to " << v << "\n";
                break;
            }
        }
    }

    template<typename NT>
    NT aluadd(const VMFixedStack& constants, VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(constants, globals, oc.l1, oc.p1);
        NT b = _getv<NT>(constants, globals, oc.l2, oc.p2);
        NT r = a + b;
        //std::cout << a << " + " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    NT alusub(const VMFixedStack& constants, VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(constants, globals, oc.l1, oc.p1);
        NT b = _getv<NT>(constants, globals, oc.l2, oc.p2);
        NT r = a - b;
        //std::cout << a << " - " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    NT alumul(const VMFixedStack& constants, VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(constants, globals, oc.l1, oc.p1);
        NT b = _getv<NT>(constants, globals, oc.l2, oc.p2);
        NT r = a * b;
        //std::cout << a << " * " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    NT aludiv(const VMFixedStack& constants, VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(constants, globals, oc.l1, oc.p1);
        NT b = _getv<NT>(constants, globals, oc.l2, oc.p2);
        NT r = a / b;
        //std::cout << a << " / " << b << " = " << r << "\n";
        return r;
    }

    template<typename NT>
    bool lt(const VMFixedStack& constants, VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(constants, globals, oc.l1, oc.p1);
        NT b = _getv<NT>(constants, globals, oc.l2, oc.p2);
        bool r = a < b;
        //std::cout << a << " < " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    bool le(const VMFixedStack& constants, VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(constants, globals, oc.l1, oc.p1);
        NT b = _getv<NT>(constants, globals, oc.l2, oc.p2);
        bool r = a <= b;
        //std::cout << a << " <= " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    bool gt(const VMFixedStack& constants, VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(constants, globals, oc.l1, oc.p1);
        NT b = _getv<NT>(constants, globals, oc.l2, oc.p2);
        bool r = a > b;
        //std::cout << a << " > " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    bool ge(const VMFixedStack& constants, VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(constants, globals, oc.l1, oc.p1);
        NT b = _getv<NT>(constants, globals, oc.l2, oc.p2);
        bool r = a >= b;
        //std::cout << a << " >= " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    bool eq(const VMFixedStack& constants, VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(constants, globals, oc.l1, oc.p1);
        NT b = _getv<NT>(constants, globals, oc.l2, oc.p2);
        bool r = a == b;
        //std::cout << a << " == " << b << " = " << r << "\n";
        return r;
    }
    template<typename NT>
    bool ne(const VMFixedStack& constants, VMFixedStack& globals, const Opcode& oc) {
        NT a = _getv<NT>(constants, globals, oc.l1, oc.p1);
        NT b = _getv<NT>(constants, globals, oc.l2, oc.p2);
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
