// main.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <chrono>

#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>

#include "VM.h"
#include "Program.h"
#include "AST.h"
#include "Compiler.h"

void compile_demo() {
    Node avgdecl;
    avgdecl.data = MethodDeclaration{"average", "f32(f32,f32)"};
    Node maindecl;
    maindecl.data = MethodDeclaration{"main", "void(bool)"};

    Node global;
    global.data = VariableDeclaration{"N", "f32"};

    Node lhs;
    lhs.data = Identifier{"x"};
    Node rhs;
    rhs.data = Identifier{"y"};

    Node denom;
    denom.data = ConstF32{2.0f};
    Node sum;
    sum.data = BinaryOperation{BinaryOps::Add, false, &lhs, &rhs};
    Node div;
    div.data = BinaryOperation{BinaryOps::Divide, false, &sum, &denom};
    Node avgret;
    avgret.data = ReturnValue{&div};

    Node avgblock;
    avgblock.data = Block{ {&avgret} };

    Node avgm;
    avgm.data = MethodDefinition{"average", {"x", "y"}, &avgblock};

    Node varxdec;
    varxdec.data = VariableDeclaration{"x", "f32"};

    Node callp1;
    callp1.data = ConstF32{2.0f};
    Node callp1pn;
    callp1pn.data = CallParam{&callp1};
    Node callp2;
    callp2.data = ConstF32{3.0f};
    Node callp2pn;
    callp2pn.data = CallParam{&callp2};
    Node avgid;
    avgid.data = Identifier{"average"};
    Node callavg;
    callavg.data = MethodCall{&avgid, {&callp1pn, &callp2pn}};

    Node thenvarx;
    thenvarx.data = Identifier{"x"};
    Node setx;
    setx.data = SetOperation{{}, &thenvarx, &callavg};
    Node thenblock;
    thenblock.data = Block{ {&setx} };

    Node elsevarx;
    elsevarx.data = Identifier{"x"};
    Node elseval;
    elseval.data = ConstF32{123.0f};
    Node elsesetx;
    elsesetx.data = SetOperation{{}, &elsevarx, &elseval};
    Node elseblock;
    elseblock.data = Block{ {&elsesetx} };

    Node doextern;
    doextern.data = Identifier{"doextern"};
    Node ifset;
    ifset.data = IfStmt{&doextern, &thenblock, &elseblock};

    Node varNset;
    varNset.data = Identifier{"N"};
    Node varNsetx;
    varNsetx.data = Identifier{"x"};
    Node setvarn;
    setvarn.data = SetOperation{{}, &varNset, &varNsetx};

    Node mainret;
    mainret.data = ReturnValue{};

    Node mainblock;
    mainblock.data = Block{ {&varxdec, &ifset, &setvarn, &mainret} };
    Node mainm;
    mainm.data = MethodDefinition{"main", {"doextern"}, &mainblock};

    Node root;
    root.data = GlobalBlock{ {&avgdecl, &maindecl, &global, &mainm, &avgm} };

    TypeTable types;

    types["void"] = TypeInfo{"void", 0, PrimitiveType::empty};
    types["bool"] = TypeInfo{"bool", 4, PrimitiveType::boolean};
    types["s32"] = TypeInfo{"s32", sizeof(int), PrimitiveType::s32};
    types["f32"] = TypeInfo{"f32", sizeof(float), PrimitiveType::f32};
    types["f32(f32,f32)"] = TypeInfo{
        "f32(f32,f32)",
        0,
        TypeMethod{
            "f32",
            {
                MethodTypeParameter{"f32"},
                MethodTypeParameter{"f32"},
            }
        }
    };
    types["void(bool)"] = TypeInfo{
        "void(bool)",
        0,
        TypeMethod{
            "void",
            {
                MethodTypeParameter{"bool"},
            }
        }
    };

    auto program = Compile(&root, types, {});
    auto scriptaverage = program->method<float,float,float>("average");
    auto scriptmain = program->method<void,bool>("main");

    std::shared_ptr<VMFixedStack> globals = program->generate_state();

    VM* vm = new VM(VMSTACK_PAGE_SIZE);

    float avg = scriptaverage(*vm, *globals, 3, 2);
    std::cout << "checking avg: " << avg << "\n-----\n";

    scriptmain(*vm, *globals, true);
    std::cout << "checking N: " << globals->cvalue<float>(program->get_global_address("N")) << "\n";
}

void vm_demo() {
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
               average_location,
               StackAddressForward(LocMemoryDirect, 0)),
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
               StackAddressForward(LocMemoryDirect, 0)),
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
               StackAddressForward(LocMemoryDirect, 0)),
        Opcode(Bytecode::s32JNE,
               StackAddressForward(LocMemoryDirect, 0),
               ConstantAddress(LocMemoryDirect, program.get_constant_address("5")),
               JumpOffsetForward(LocMemoryDirect, 2)),
        Opcode(Bytecode::Call,
               ScriptCall(LocMemoryDirect, program.get_method_address("func5")),
               StackAddressForward(LocMemoryDirect, 0)),
        Opcode(Bytecode::Jump,
               JumpOffsetForward(LocMemoryDirect, 10)),
        Opcode(Bytecode::s32JNE,
               StackAddressForward(LocMemoryDirect, 0),
               ConstantAddress(LocMemoryDirect, program.get_constant_address("4")),
               JumpOffsetForward(LocMemoryDirect, 2)),
        Opcode(Bytecode::Call,
               ScriptCall(LocMemoryDirect, program.get_method_address("func4")),
               StackAddressForward(LocMemoryDirect, 0)),
        Opcode(Bytecode::Jump,
               JumpOffsetForward(LocMemoryDirect, 7)),
        Opcode(Bytecode::s32JNE,
               StackAddressForward(LocMemoryDirect, 0),
               ConstantAddress(LocMemoryDirect, program.get_constant_address("3")),
               JumpOffsetForward(LocMemoryDirect, 2)),
        Opcode(Bytecode::Call,
               ScriptCall(LocMemoryDirect, program.get_method_address("func3")),
               StackAddressForward(LocMemoryDirect, 0)),
        Opcode(Bytecode::Jump,
               JumpOffsetForward(LocMemoryDirect, 4)),
        Opcode(Bytecode::s32JNE,
               StackAddressForward(LocMemoryDirect, 0),
               ConstantAddress(LocMemoryDirect, program.get_constant_address("2")),
               JumpOffsetForward(LocMemoryDirect, 2)),
        Opcode(Bytecode::Call,
               ScriptCall(LocMemoryDirect, program.get_method_address("func2")),
               StackAddressForward(LocMemoryDirect, 0)),
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
               StackAddressForward(LocMemoryDirect, 0)),
        Opcode(Bytecode::s32Set,
               ConstantAddress(LocMemoryDirect, program.get_constant_address("5")),
               StackAddressForward(LocMemoryDirect, 12)),
        Opcode(Bytecode::Call,
               ScriptCall(LocMemoryDirect, program.get_method_address("Recursion")),
               StackAddressForward(LocMemoryDirect, 0)),
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
    //program.register_method<void>("main");
    //program.register_method<float,float,float>("LAverage");

    VM* vm = new VM(VMSTACK_PAGE_SIZE);

    std::shared_ptr<VMFixedStack> globals = program.generate_state();
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

int main() {
    compile_demo();
    return 0;
}
