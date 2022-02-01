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

    Program program;
    program.add_builtin("CAverage", &average);
    program.add_builtin("PrintF32", &printf32);
    program.add_global<float>("N");

    // Average(float,float)float, 4+8 stack size
    // base+0: a / result
    // base+4: b
    // base+8: tmp
    // 4 stack + 8 param
    program.add_method("Average", 8, 4, {
        //Opcode(Bytecode::BeginFrame,
        //       DataLoc::G, DataLoc::C, DataLoc::C,
        //       GlobalAddress(4), 0, 0),
        Opcode(Bytecode::f32Add,
               DataLoc::L, DataLoc::L, DataLoc::L,
               LocalAddress(0), LocalAddress(4), LocalAddress(8)),
        Opcode(Bytecode::f32Div,
               DataLoc::L, DataLoc::C, DataLoc::L,
               LocalAddress(8), ConstData(2.0f), LocalAddress(0)),
        Opcode(Bytecode::Ret,
               DataLoc::C, DataLoc::C, DataLoc::L,
               ConstData(0), ConstData(0), LocalAddress(4))
    });

    // func5(), 8+0 stack size:
    // base+0: param1 (also result)
    // base+4: param2
    program.add_method("func5", 0, 8, {
        //Opcode(Bytecode::BeginFrame,
        //       DataLoc::G, DataLoc::C, DataLoc::C,
        //       GlobalAddress(8), 0, 0),
        Opcode(Bytecode::f32Set,
               DataLoc::G, DataLoc::L, DataLoc::C,
               GlobalAddress(0), LocalAddress(0), ConstData(0.0f)),
        Opcode(Bytecode::f32Set,
               DataLoc::G, DataLoc::L, DataLoc::C,
               GlobalAddress(0), LocalAddress(4), ConstData(0.0f)),
        Opcode(Bytecode::Call,
               DataLoc::G, DataLoc::L, DataLoc::L,
               GlobalAddress(program.get_method_address("Average")), LocalAddress(8), LocalAddress(4)),
    //    Opcode(Bytecode::CallExtern,
    //           DataLoc::G, DataLoc::L, DataLoc::L,
    //           GlobalAddress(0), LocalAddress(0), LocalAddress(0)),
        Opcode(Bytecode::f32Add,
               DataLoc::G, DataLoc::L, DataLoc::G,
               GlobalAddress(0), LocalAddress(0), GlobalAddress(0)),
        Opcode(Bytecode::Ret,
               DataLoc::C, DataLoc::C, DataLoc::L,
               ConstData(0), ConstData(0), LocalAddress(8))
    });

    // func4(), stack 8+0
    // base+0 param2
    // base+4 param1/ret
    program.add_method("func4", 0, 8, {
        //Opcode(Bytecode::BeginFrame,
        //       DataLoc::G, DataLoc::C, DataLoc::C,
        //       GlobalAddress(8), 0, 0),
        Opcode(Bytecode::f32Add,
               DataLoc::G, DataLoc::C, DataLoc::L,
               GlobalAddress(0), ConstData(1.0f), LocalAddress(0)),
        Opcode(Bytecode::f32Add,
               DataLoc::G, DataLoc::C, DataLoc::L,
               GlobalAddress(0), ConstData(2.0f), LocalAddress(4)),
        Opcode(Bytecode::Call,
               DataLoc::G, DataLoc::L, DataLoc::L,
               GlobalAddress(program.get_method_address("Average")), LocalAddress(8), LocalAddress(4)),
    //    Opcode(Bytecode::CallExtern,
    //           DataLoc::G, DataLoc::L, DataLoc::L,
    //           GlobalAddress(0), LocalAddress(0), LocalAddress(0)),
        Opcode(Bytecode::f32Mul,
               DataLoc::L, DataLoc::C, DataLoc::L,
               LocalAddress(0), ConstData(2.0f), LocalAddress(4)),
        Opcode(Bytecode::f32Add,
               DataLoc::G, DataLoc::L, DataLoc::G,
               GlobalAddress(0), LocalAddress(4), GlobalAddress(0)),
        Opcode(Bytecode::Ret,
               DataLoc::C, DataLoc::C, DataLoc::L,
               ConstData(0), ConstData(0), LocalAddress(8))
    });

    // func3(), stack 0
    program.add_method("func3", 0, 0, {
        //Opcode(Bytecode::BeginFrame,
        //       DataLoc::G, DataLoc::C, DataLoc::C,
        //       GlobalAddress(0), 0, 0),
        Opcode(Bytecode::f32Mul,
               DataLoc::G, DataLoc::C, DataLoc::G,
               GlobalAddress(0), ConstData(2.1f), GlobalAddress(0)),
        Opcode(Bytecode::Ret,
               DataLoc::C, DataLoc::C, DataLoc::L,
               ConstData(0), ConstData(0), LocalAddress(0))
     });

    // func2(), stack 0
    program.add_method("func2", 0, 0, {
        //Opcode(Bytecode::BeginFrame,
        //       DataLoc::G, DataLoc::C, DataLoc::C,
        //       GlobalAddress(0), 0, 0),
        Opcode(Bytecode::f32Div,
               DataLoc::G, DataLoc::C, DataLoc::G,
               GlobalAddress(0), ConstData(3.5f), GlobalAddress(0)),
        Opcode(Bytecode::Ret,
               DataLoc::C, DataLoc::C, DataLoc::L,
               ConstData(0), ConstData(0), LocalAddress(0))
    });

    // Recursion(int), stack 4+4
    // base+0 is rec
    // base+4 is param/temp
    program.add_method("Recursion", 4, 4, {
        //Opcode(Bytecode::BeginFrame,
        //       DataLoc::G, DataLoc::C, DataLoc::C,
        //       GlobalAddress(4), 0, 0),
        Opcode(Bytecode::s32JLT,
               DataLoc::L, DataLoc::C, DataLoc::L,
               LocalAddress(0), ConstData(1), LocalAddress(2)),
        Opcode(Bytecode::s32Sub,
               DataLoc::L, DataLoc::C, DataLoc::L,
               LocalAddress(0), ConstData(1), LocalAddress(4)),
        Opcode(Bytecode::Call,
               DataLoc::G, DataLoc::L, DataLoc::L,
               GlobalAddress(program.current_method_address()), LocalAddress(4), LocalAddress(4)),
        Opcode(Bytecode::s32JNE,
               DataLoc::L, DataLoc::C, DataLoc::L,
               LocalAddress(0), ConstData(5), LocalAddress(2)),
        Opcode(Bytecode::Call,
               DataLoc::G, DataLoc::L, DataLoc::L,
               GlobalAddress(program.get_method_address("func5")), LocalAddress(0), LocalAddress(8)),
        Opcode(Bytecode::Jump,
               DataLoc::L, DataLoc::C, DataLoc::C,
               LocalAddress(10), ConstData(0), ConstData(0)),
        Opcode(Bytecode::s32JNE,
               DataLoc::L, DataLoc::C, DataLoc::L,
               LocalAddress(0), ConstData(4), LocalAddress(2)),
        Opcode(Bytecode::Call,
               DataLoc::G, DataLoc::L, DataLoc::L,
               GlobalAddress(program.get_method_address("func4")), LocalAddress(0), LocalAddress(8)),
        Opcode(Bytecode::Jump,
               DataLoc::L, DataLoc::C, DataLoc::C,
               LocalAddress(7), ConstData(0), ConstData(0)),
        Opcode(Bytecode::s32JNE,
               DataLoc::L, DataLoc::C, DataLoc::L,
               LocalAddress(0), ConstData(3), LocalAddress(2)),
        Opcode(Bytecode::Call,
               DataLoc::G, DataLoc::L, DataLoc::L,
               GlobalAddress(program.get_method_address("func3")), LocalAddress(0), LocalAddress(0)),
        Opcode(Bytecode::Jump,
               DataLoc::L, DataLoc::C, DataLoc::C,
               LocalAddress(4), ConstData(0), ConstData(0)),
        Opcode(Bytecode::s32JNE,
               DataLoc::L, DataLoc::C, DataLoc::L,
               LocalAddress(0), ConstData(2), LocalAddress(2)),
        Opcode(Bytecode::Call,
               DataLoc::G, DataLoc::L, DataLoc::L,
               GlobalAddress(program.get_method_address("func2")), LocalAddress(0), LocalAddress(0)),
        Opcode(Bytecode::Jump,
               DataLoc::L, DataLoc::C, DataLoc::C,
               LocalAddress(1), ConstData(0), ConstData(0)),
        Opcode(Bytecode::f32Mul,
               DataLoc::G, DataLoc::C, DataLoc::G,
               GlobalAddress(0), ConstData(1.5f), GlobalAddress(0)),
        Opcode(Bytecode::Ret,
               DataLoc::C, DataLoc::C, DataLoc::L,
               ConstData(0), ConstData(0), LocalAddress(4))
    });

    // main(), stack size 16+0
    // base+0 is i
    // base+4 is temp?
    // base+8 is p1
    // base+12 is p2
    program.add_method("main", 0, 16, {
        //Opcode(Bytecode::BeginFrame,
        //       DataLoc::G, DataLoc::C, DataLoc::C,
        //       GlobalAddress(16), 0, 0),
        Opcode(Bytecode::f32Set,
               DataLoc::C, DataLoc::G, DataLoc::C,
               ConstData(0.0f), GlobalAddress(0), ConstData(0.0f)),
        Opcode(Bytecode::f32Set,
               DataLoc::C, DataLoc::L, DataLoc::C,
               ConstData(0.0f), LocalAddress(0), ConstData(0.0f)),
        Opcode(Bytecode::f32JGT,
               DataLoc::L, DataLoc::C, DataLoc::L,
               LocalAddress(0), ConstData(iterate_to), LocalAddress(9)),
        Opcode(Bytecode::f32Set,
               DataLoc::L, DataLoc::L, DataLoc::C,
               LocalAddress(0), LocalAddress(8), ConstData(0.0f)),
    //    Opcode(Bytecode::CallExtern,
    //           DataLoc::G, DataLoc::L, DataLoc::C,
    //           GlobalAddress(1), LocalAddress(4), 0),
        Opcode(Bytecode::f32Set,
               DataLoc::L, DataLoc::L, DataLoc::C,
               LocalAddress(0), LocalAddress(12), ConstData(0.0f)),
        Opcode(Bytecode::Call,
               DataLoc::G, DataLoc::L, DataLoc::L,
               GlobalAddress(program.get_method_address("Average")), LocalAddress(8), LocalAddress(4)),
        // base+12 result isnt used
    //    Opcode(Bytecode::CallExtern,
    //           DataLoc::G, DataLoc::L, DataLoc::C,
    //           GlobalAddress(0), LocalAddress(0), 0),
        Opcode(Bytecode::s32Set,
               DataLoc::C, DataLoc::L, DataLoc::C,
               ConstData(5), LocalAddress(12), ConstData(0.0f)),
        Opcode(Bytecode::Call,
               DataLoc::G, DataLoc::L, DataLoc::L,
               GlobalAddress(program.get_method_address("Recursion")), LocalAddress(4), LocalAddress(4)),
        Opcode(Bytecode::f32JLE,
               DataLoc::G, DataLoc::C, DataLoc::L,
               GlobalAddress(0), ConstData(100.0f), LocalAddress(1)),
        Opcode(Bytecode::f32Set,
               DataLoc::C, DataLoc::G, DataLoc::C,
               ConstData(0.0f), GlobalAddress(0), ConstData(0.0f)),
        Opcode(Bytecode::f32Add,
               DataLoc::L, DataLoc::C, DataLoc::L,
               LocalAddress(0), ConstData(0.25f), LocalAddress(0)),
        Opcode(Bytecode::Jump,
               DataLoc::P, DataLoc::C, DataLoc::C,
               ParamAddress(10), ConstData(0), ConstData(0)),
        Opcode(Bytecode::Ret,
               DataLoc::C, DataLoc::C, DataLoc::L,
               ConstData(0), ConstData(0), LocalAddress(16))
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
