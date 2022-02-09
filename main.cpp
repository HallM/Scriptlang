// main.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <chrono>

#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>

#include "VM.h"
#include "Program.h"

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
    program.add_method("LAverage", 8, 4, {
        Opcode(Bytecode::f32Add,
               StackAddressForward(LocMemoryDirect, 0),
               StackAddressForward(LocMemoryDirect, 4),
               StackAddressForward(LocMemoryDirect, 8)),
        Opcode(Bytecode::f32Div,
               StackAddressForward(LocMemoryDirect, 8),
               ConstantAddress(LocMemoryDirect, program.get_constant_address("2.0f")),
               StackAddressForward(LocMemoryDirect, 0)),
        Opcode(Bytecode::Ret, StackSize(LocMemoryDirect, 4))
    });

    // auto average_location = ExternalCall(LocMemoryDirect, program.get_builtin_address("CAverage"));
    auto average_location = ScriptCall(LocMemoryDirect, program.get_method_address("LAverage"));

    // func5(), 8+0 stack size:
    // base+0: param1 (also result)
    // base+4: param2
    program.add_method("func5", 0, 8, {
        Opcode(Bytecode::f32Set,
               GlobalAddress(LocMemoryDirect, 0),
               StackAddressForward(LocMemoryDirect, 0)),
        Opcode(Bytecode::f32Set,
               GlobalAddress(LocMemoryDirect, 0),
               StackAddressForward(LocMemoryDirect, 4)),
        Opcode(Bytecode::Call,
               average_location, StackSize(LocMemoryDirect, 8)),
        Opcode(Bytecode::f32Add,
               GlobalAddress(LocMemoryDirect, 0),
               StackAddressForward(LocMemoryDirect, 0),
               GlobalAddress(LocMemoryDirect, 0)),
        Opcode(Bytecode::Ret, StackSize(LocMemoryDirect, 8))
    });

    // func4(), stack 8+0
    // base+0 param2
    // base+4 param1/ret
    program.add_method("func4", 0, 8, {
        Opcode(Bytecode::f32Add,
               GlobalAddress(LocMemoryDirect, 0),
               ConstantAddress(LocMemoryDirect, program.get_constant_address("1.0f")),
               StackAddressForward(LocMemoryDirect, 0)),
        Opcode(Bytecode::f32Add,
               GlobalAddress(LocMemoryDirect, 0),
               ConstantAddress(LocMemoryDirect, program.get_constant_address("2.0f")),
               StackAddressForward(LocMemoryDirect, 4)),
        Opcode(Bytecode::Call,
               average_location,
               StackSize(LocMemoryDirect, 8)),
        Opcode(Bytecode::f32Mul,
               StackAddressForward(LocMemoryDirect, 0),
               ConstantAddress(LocMemoryDirect, program.get_constant_address("2.0f")),
               StackAddressForward(LocMemoryDirect, 4)),
        Opcode(Bytecode::f32Add,
               GlobalAddress(LocMemoryDirect, 0),
               StackAddressForward(LocMemoryDirect, 4),
               GlobalAddress(LocMemoryDirect, 0)),
        Opcode(Bytecode::Ret, StackSize(LocMemoryDirect, 8))
    });

    // func3(), stack 0
    program.add_method("func3", 0, 0, {
        Opcode(Bytecode::f32Mul,
               GlobalAddress(LocMemoryDirect, 0),
               ConstantAddress(LocMemoryDirect, program.get_constant_address("2.1f")),
               GlobalAddress(LocMemoryDirect, 0)),
        Opcode(Bytecode::Ret, StackSize(LocMemoryDirect, 0))
     });

    // func2(), stack 0
    program.add_method("func2", 0, 0, {
        Opcode(Bytecode::f32Div,
               GlobalAddress(LocMemoryDirect, 0),
               ConstantAddress(LocMemoryDirect, program.get_constant_address("3.5f")),
               GlobalAddress(LocMemoryDirect, 0)),
        Opcode(Bytecode::Ret, StackSize(LocMemoryDirect, 0))
    });

    // Recursion(int), stack 4+4
    // base+0 is rec
    // base+4 is param/temp
    program.add_method("Recursion", 4, 4, {
        Opcode(Bytecode::s32JLT,
               StackAddressForward(LocMemoryDirect, 0),
               ConstantAddress(LocMemoryDirect, program.get_constant_address("1")),
               JumpOffsetForward(LocMemoryDirect, 2)),
        Opcode(Bytecode::s32Sub,
               StackAddressForward(LocMemoryDirect, 0),
               ConstantAddress(LocMemoryDirect, program.get_constant_address("1")),
               StackAddressForward(LocMemoryDirect, 4)),
        Opcode(Bytecode::Call,
               ScriptCall(LocMemoryDirect, program.current_method_address()),
               StackSize(LocMemoryDirect, 4)),
        Opcode(Bytecode::s32JNE,
               StackAddressForward(LocMemoryDirect, 0),
               ConstantAddress(LocMemoryDirect, program.get_constant_address("5")),
               JumpOffsetForward(LocMemoryDirect, 2)),
        Opcode(Bytecode::Call,
               ScriptCall(LocMemoryDirect, program.get_method_address("func5")),
               StackSize(LocMemoryDirect, 0)),
        Opcode(Bytecode::Jump,
               JumpOffsetForward(LocMemoryDirect, 10)),
        Opcode(Bytecode::s32JNE,
               StackAddressForward(LocMemoryDirect, 0),
               ConstantAddress(LocMemoryDirect, program.get_constant_address("4")),
               JumpOffsetForward(LocMemoryDirect, 2)),
        Opcode(Bytecode::Call,
               ScriptCall(LocMemoryDirect, program.get_method_address("func4")),
               StackSize(LocMemoryDirect, 0)),
        Opcode(Bytecode::Jump,
               JumpOffsetForward(LocMemoryDirect, 7)),
        Opcode(Bytecode::s32JNE,
               StackAddressForward(LocMemoryDirect, 0),
               ConstantAddress(LocMemoryDirect, program.get_constant_address("3")),
               JumpOffsetForward(LocMemoryDirect, 2)),
        Opcode(Bytecode::Call,
               ScriptCall(LocMemoryDirect, program.get_method_address("func3")),
               StackSize(LocMemoryDirect, 0)),
        Opcode(Bytecode::Jump,
               JumpOffsetForward(LocMemoryDirect, 4)),
        Opcode(Bytecode::s32JNE,
               StackAddressForward(LocMemoryDirect, 0),
               ConstantAddress(LocMemoryDirect, program.get_constant_address("2")),
               JumpOffsetForward(LocMemoryDirect, 2)),
        Opcode(Bytecode::Call,
               ScriptCall(LocMemoryDirect, program.get_method_address("func2")),
               StackSize(LocMemoryDirect, 0)),
        Opcode(Bytecode::Jump,
               JumpOffsetForward(LocMemoryDirect, 1)),
        Opcode(Bytecode::f32Mul,
               GlobalAddress(LocMemoryDirect, 0),
               ConstantAddress(LocMemoryDirect, program.get_constant_address("1.5f")),
               GlobalAddress(LocMemoryDirect, 0)),
        Opcode(Bytecode::Ret, StackSize(LocMemoryDirect, 4))
    });

    // main(), stack size 16+0
    // base+0 is i
    // base+4 is temp?
    // base+8 is p1
    // base+12 is p2
    program.add_method("main", 0, 16, {
        Opcode(Bytecode::f32Set,
               ConstantAddress(LocMemoryDirect, program.get_constant_address("0.0f")),
               GlobalAddress(LocMemoryDirect, 0)),
        Opcode(Bytecode::f32Set,
               ConstantAddress(LocMemoryDirect, program.get_constant_address("0.0f")),
               StackAddressForward(LocMemoryDirect, 0)),
        Opcode(Bytecode::f32JGT,
               StackAddressForward(LocMemoryDirect, 0),
               ConstantAddress(LocMemoryDirect, program.get_constant_address("end")),
               JumpOffsetForward(LocMemoryDirect, 9)),
        Opcode(Bytecode::f32Set,
               StackAddressForward(LocMemoryDirect, 0),
               StackAddressForward(LocMemoryDirect, 8)),
        Opcode(Bytecode::f32Set,
               StackAddressForward(LocMemoryDirect, 0),
               StackAddressForward(LocMemoryDirect, 12)),
        Opcode(Bytecode::Call,
               average_location,
               StackSize(LocMemoryDirect, 8)), // base+12 result isnt used fyi
        Opcode(Bytecode::s32Set,
               ConstantAddress(LocMemoryDirect, program.get_constant_address("5")),
               StackAddressForward(LocMemoryDirect, 12)),
        Opcode(Bytecode::Call,
               ScriptCall(LocMemoryDirect, program.get_method_address("Recursion")),
               StackSize(LocMemoryDirect, 4)),
        Opcode(Bytecode::f32JLE,
               GlobalAddress(LocMemoryDirect, 0),
               ConstantAddress(LocMemoryDirect, program.get_constant_address("100.0f")),
               JumpOffsetForward(LocMemoryDirect, 1)),
        Opcode(Bytecode::f32Set,
               ConstantAddress(LocMemoryDirect, program.get_constant_address("0.0f")),
               GlobalAddress(LocMemoryDirect, 0)),
        Opcode(Bytecode::f32Add,
               StackAddressForward(LocMemoryDirect, 0),
               ConstantAddress(LocMemoryDirect, program.get_constant_address("0.25f")),
               StackAddressForward(LocMemoryDirect, 0)),
        Opcode(Bytecode::f32JLE,
               StackAddressForward(LocMemoryDirect, 0),
               ConstantAddress(LocMemoryDirect, program.get_constant_address("end")),
               JumpOffsetBackward(LocMemoryDirect, 9)),
        Opcode(Bytecode::Ret, StackSize(LocMemoryDirect, 16))
    });
    program.register_method<void>("main");
    program.register_method<float,float,float>("LAverage");

    VM* vm = new VM(VMSTACK_PAGE_SIZE);

    std::shared_ptr<VMFixedStack> globals = program.generate_state();
    // vm->run_method(program, "main", *globals);

    auto scriptmain = program.method<void>("main");
    scriptmain(*vm, *globals);

    float N = *globals->at<float>(program.get_global_address("N"));
    std::cout << "N = " << N << "\n";

    auto dur = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1> >>(std::chrono::steady_clock::now() - m_beg).count();

    std::cout << "time: " << dur << "s\n";

    auto scriptaverage = program.method<float,float,float>("LAverage");
    float avg = scriptaverage(*vm, *globals, 1, 2);
    std::cout << "checking: " << avg << "\n";

    delete vm;
}
