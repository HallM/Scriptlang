// main.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <chrono>

#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>

#include "VM.h"

int main() {
    auto m_beg = std::chrono::steady_clock::now();

    BuiltinRunnable<float, float, float> average([](float a, float b) -> float {
        //std::cout << a << " " << b << " " << (a + b)/2.0f << "\n";
        return (a + b) / 2.0f;
    });
    BuiltinRunnable<void, float> printf32([](float a) {
        std::cout << a << "\n";
    });

    float iterate_to = 1.0f; // 1000000.0f

    // the lua application including compilation was 1 second 249ms (1.25s)
    // this app was 1.32s. Lua 0.0031ms per loop. THis 0.0033ms per loop.
    // result is 54.6

    Program program(15 * 4);
    program.add_constant<float>("0.0f", 0.0f);
    program.add_constant<float>("0.25f", 0.25f);
    program.add_constant<float>("1.0f", 1.0f);
    program.add_constant<float>("1.5f", 1.5f);
    program.add_constant<float>("2.0f", 2.0f);
    program.add_constant<float>("2.1f", 2.1f);
    program.add_constant<float>("3.5f", 3.5f);
    program.add_constant<float>("100.0f", 100.0f);
    program.add_constant<float>("end", iterate_to);
    program.add_constant<int>("0", 0);
    program.add_constant<int>("1", 1);
    program.add_constant<int>("2", 2);
    program.add_constant<int>("3", 3);
    program.add_constant<int>("4", 4);
    program.add_constant<int>("5", 5);
    program.add_builtin("CAverage", &average);
    program.add_builtin("PrintF32", &printf32);
    program.add_global<float>("N");

    // Average(float,float)float, 4+8 stack size
    // base+0: a / result
    // base+4: b
    // base+8: tmp
    // 4 stack + 8 param
    program.add_method("Average", 8, 4, {
        Opcode(Bytecode::f32Add,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               StackAddressForward(0), StackAddressForward(4), StackAddressForward(8)),
        Opcode(Bytecode::f32Div,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               StackAddressForward(8), ConstantAddress(program.get_constant_address("2.0f")), StackAddressForward(0)),
        Opcode(Bytecode::Ret, LocMemoryDirect, 4)
    });

    // func5(), 8+0 stack size:
    // base+0: param1 (also result)
    // base+4: param2
    program.add_method("func5", 0, 8, {
        Opcode(Bytecode::f32Set,
               LocMemoryDirect, LocMemoryDirect,
               GlobalAddress(0), StackAddressForward(0)),
        Opcode(Bytecode::f32Set,
               LocMemoryDirect, LocMemoryDirect,
               GlobalAddress(0), StackAddressForward(4)),
        Opcode(Bytecode::Call,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               (program.get_method_address("Average")), (8), (4)),
    //    Opcode(Bytecode::CallExtern,
    //           LocMemoryDirect,
    //           GlobalAddress(0)),
        Opcode(Bytecode::f32Add,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               GlobalAddress(0), StackAddressForward(0), GlobalAddress(0)),
        Opcode(Bytecode::Ret, LocMemoryDirect, 8)
    });

    // func4(), stack 8+0
    // base+0 param2
    // base+4 param1/ret
    program.add_method("func4", 0, 8, {
        Opcode(Bytecode::f32Add,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               GlobalAddress(0), ConstantAddress(program.get_constant_address("1.0f")), StackAddressForward(0)),
        Opcode(Bytecode::f32Add,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               GlobalAddress(0), ConstantAddress(program.get_constant_address("2.0f")), StackAddressForward(4)),
        Opcode(Bytecode::Call,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               (program.get_method_address("Average")), (8), (4)),
    //    Opcode(Bytecode::CallExtern,
    //           LocMemoryDirect,
    //           GlobalAddress(0)),
        Opcode(Bytecode::f32Mul,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               StackAddressForward(0), ConstantAddress(program.get_constant_address("2.0f")), StackAddressForward(4)),
        Opcode(Bytecode::f32Add,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               GlobalAddress(0), StackAddressForward(4), GlobalAddress(0)),
        Opcode(Bytecode::Ret, LocMemoryDirect, 8)
    });

    // func3(), stack 0
    program.add_method("func3", 0, 0, {
        Opcode(Bytecode::f32Mul,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               GlobalAddress(0), ConstantAddress(program.get_constant_address("2.1f")), GlobalAddress(0)),
        Opcode(Bytecode::Ret, LocMemoryDirect, 0)
     });

    // func2(), stack 0
    program.add_method("func2", 0, 0, {
        Opcode(Bytecode::f32Div,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               GlobalAddress(0), ConstantAddress(program.get_constant_address("3.5f")), GlobalAddress(0)),
        Opcode(Bytecode::Ret, LocMemoryDirect, 0)
    });

    // Recursion(int), stack 4+4
    // base+0 is rec
    // base+4 is param/temp
    program.add_method("Recursion", 4, 4, {
        Opcode(Bytecode::s32JLT,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               StackAddressForward(0), ConstantAddress(program.get_constant_address("1")), JumpOffsetForward(2)),
        Opcode(Bytecode::s32Sub,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               StackAddressForward(0), ConstantAddress(program.get_constant_address("1")), StackAddressForward(4)),
        Opcode(Bytecode::Call,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               (program.current_method_address()), (4), (4)),
        Opcode(Bytecode::s32JNE,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               StackAddressForward(0), ConstantAddress(program.get_constant_address("5")), JumpOffsetForward(2)),
        Opcode(Bytecode::Call,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               (program.get_method_address("func5")), (0), (8)),
        Opcode(Bytecode::Jump,
               LocMemoryDirect,
               JumpOffsetForward(10)),
        Opcode(Bytecode::s32JNE,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               StackAddressForward(0), ConstantAddress(program.get_constant_address("4")), JumpOffsetForward(2)),
        Opcode(Bytecode::Call,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               (program.get_method_address("func4")), (0), (8)),
        Opcode(Bytecode::Jump,
               LocMemoryDirect,
               JumpOffsetForward(7)),
        Opcode(Bytecode::s32JNE,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               StackAddressForward(0), ConstantAddress(program.get_constant_address("3")), JumpOffsetForward(2)),
        Opcode(Bytecode::Call,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               (program.get_method_address("func3")), (0), (0)),
        Opcode(Bytecode::Jump,
               LocMemoryDirect,
               JumpOffsetForward(4)),
        Opcode(Bytecode::s32JNE,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               StackAddressForward(0), ConstantAddress(program.get_constant_address("2")), JumpOffsetForward(2)),
        Opcode(Bytecode::Call,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               (program.get_method_address("func2")), (0), (0)),
        Opcode(Bytecode::Jump,
               LocMemoryDirect,
               JumpOffsetForward(1)),
        Opcode(Bytecode::f32Mul,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               GlobalAddress(0), ConstantAddress(program.get_constant_address("1.5f")), GlobalAddress(0)),
        Opcode(Bytecode::Ret, LocMemoryDirect, 4)
    });

    // main(), stack size 16+0
    // base+0 is i
    // base+4 is temp?
    // base+8 is p1
    // base+12 is p2
    program.add_method("main", 0, 16, {
        Opcode(Bytecode::f32Set,
               LocMemoryDirect, LocMemoryDirect,
               ConstantAddress(program.get_constant_address("0.0f")), GlobalAddress(0)),
        Opcode(Bytecode::f32Set,
               LocMemoryDirect, LocMemoryDirect,
               ConstantAddress(program.get_constant_address("0.0f")), StackAddressForward(0)),
        Opcode(Bytecode::f32JGT,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               StackAddressForward(0), ConstantAddress(program.get_constant_address("end")), JumpOffsetForward(9)),
        Opcode(Bytecode::f32Set,
               LocMemoryDirect, LocMemoryDirect,
               StackAddressForward(0), StackAddressForward(8)),
    //    Opcode(Bytecode::CallExtern,
    //           LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
    //           GlobalAddress(1), (4), 0),
        Opcode(Bytecode::f32Set,
               LocMemoryDirect, LocMemoryDirect,
               StackAddressForward(0), StackAddressForward(12)),
        Opcode(Bytecode::Call,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               (program.get_method_address("Average")), (8), (4)),
        // base+12 result isnt used
    //    Opcode(Bytecode::CallExtern,
    //           LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
    //           GlobalAddress(0), (0), 0),
        Opcode(Bytecode::s32Set,
               LocMemoryDirect, LocMemoryDirect,
               ConstantAddress(program.get_constant_address("5")), StackAddressForward(12)),
        Opcode(Bytecode::Call,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               (program.get_method_address("Recursion")), (4), (4)),
        Opcode(Bytecode::f32JLE,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               GlobalAddress(0), ConstantAddress(program.get_constant_address("100.0f")), JumpOffsetForward(1)),
        Opcode(Bytecode::f32Set,
               LocMemoryDirect, LocMemoryDirect,
               ConstantAddress(program.get_constant_address("0.0f")), GlobalAddress(0)),
        Opcode(Bytecode::f32Add,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               StackAddressForward(0), ConstantAddress(program.get_constant_address("0.25f")), StackAddressForward(0)),
        Opcode(Bytecode::f32JLE,
               LocMemoryDirect, LocMemoryDirect, LocMemoryDirect,
               StackAddressForward(0), ConstantAddress(program.get_constant_address("end")), JumpOffsetBackward(9)),
        Opcode(Bytecode::Ret, LocMemoryDirect, 16)
    });
    VM* m = new VM(VMSTACK_PAGE_SIZE);

    std::shared_ptr<VMFixedStack> globals = program.generate_state();
    m->run_method(program, "main", *globals);

    float N = *globals->at<float>(program.get_global_address("N"));
    std::cout << "N = " << N << "\n";

    auto dur = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1> >>(std::chrono::steady_clock::now() - m_beg).count();

    std::cout << "time: " << dur << "s\n";

    delete m;
}
