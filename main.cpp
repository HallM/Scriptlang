// main.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <chrono>

#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>

#include "VM.h"
#include "VMFFI.h"
#include "Program.h"
#include "AST.h"
#include "Compiler.h"

const float iterate_to = 100000.0f;

void compile_demo() {
    auto m_beg = std::chrono::steady_clock::now();

    Node avgdecl = { MethodDeclaration{"average", "f32(f32,f32)"} };
    Node func5decl = { MethodDeclaration{"func5", "void()"} };
    Node func4decl = { MethodDeclaration{"func4", "void()"} };
    Node func3decl = { MethodDeclaration{"func3", "void()"} };
    Node func2decl = { MethodDeclaration{"func2", "void()"} };
    Node recursiondecl = { MethodDeclaration{"recursion", "void(s32)"} };
    Node maindecl = { MethodDeclaration{"main", "void()"} };

    Node global = { VariableDeclaration{"N", "f32"} };

    // special re-usables
    Node identN = { Identifier{"N"} };
    Node identaverage = { Identifier{"average"} };
    Node identRecursion = { Identifier{"recursion"} };

    // const reusables
    Node c_0f = { ConstF32{0.0f} };
    Node c_1f = { ConstF32{1.0f} };
    Node c_2f = { ConstF32{2.0f} };


    // average(x,y) = (x + y) / 2
    Node identx = { Identifier{"x"} };
    Node identy = { Identifier{"y"} };
    Node sumxy = { BinaryOperation{BinaryOps::Add, &identx, &identy} };

    Node avgxy = { BinaryOperation{BinaryOps::Divide, &sumxy, &c_2f} };
    Node avgret = { ReturnValue{&avgxy} };

    Node avgblock = { Block{ {&avgret} } };
    Node avgm = { MethodDefinition{"average", {"x", "y"}, &avgblock} };

    // func5() = N += average(n, n)
    Node f5avgp = { CallParam{&identN} };
    Node f5avg = { MethodCall{&identaverage, {&f5avgp, &f5avgp}} };
    Node f5addavgtoN = { SetOperation{BinaryOps::Add, &identN, &f5avg} };
    Node func5block = { Block{ {&f5addavgtoN} } };
    Node func5def = { MethodDefinition{"func5", {}, &func5block} };

    // func4() = N += 2*average(n+1, n+2)
    Node nplus1 = { BinaryOperation{BinaryOps::Add, &identN, &c_1f} };
    Node nplus2 = { BinaryOperation{BinaryOps::Add, &identN, &c_2f} };
    Node f4avgp1 = { CallParam{&nplus1} };
    Node f4avgp2 = { CallParam{&nplus2} };
    Node f4avg = { MethodCall{&identaverage, {&f4avgp1, &f4avgp2}} };
    Node f4avgtimes2 = { BinaryOperation{BinaryOps::Multiply, &f4avg, &c_2f} };
    Node f4addavgtoN = { SetOperation{BinaryOps::Add, &identN, &f4avgtimes2} };
    Node func4block = { Block{ {&f4addavgtoN} } };
    Node func4def = { MethodDefinition{"func4", {}, &func4block} };

    // func3() = n *= 2.1
    Node c_2_1f = { ConstF32{2.1f} };
    Node f3multoN = { SetOperation{BinaryOps::Multiply, &identN, &c_2_1f} };
    Node func3block = { Block{ {&f3multoN} } };
    Node func3def = { MethodDefinition{"func3", {}, &func3block} };

    // func2() = n /= 3.5
    Node c_3_5f = { ConstF32{3.5f} };
    Node f2divtoN = { SetOperation{BinaryOps::Divide, &identN, &c_3_5f} };
    Node func2block = { Block{ {&f2divtoN} } };
    Node func2def = { MethodDefinition{"func2", {}, &func2block} };

    // recursion(rec)
    Node identrec = { Identifier{"rec"} };
    Node c_1s = { ConstS32{1} };
    Node c_2s = { ConstS32{2} };
    Node c_3s = { ConstS32{3} };
    Node c_4s = { ConstS32{4} };
    Node c_5s = { ConstS32{5} };
    Node c_1_5f = { ConstF32{1.5f} };
    // if rec >= 1
    Node recgt1 = { BinaryOperation{BinaryOps::GreaterEqual, &identrec, &c_1s} };
    // then recursion(rec-1)
    Node recminus1 = { BinaryOperation{BinaryOps::Subtract, &identrec, &c_1s} };
    Node recrm1p = { CallParam{&recminus1} };
    Node recrm1call = { MethodCall{&identRecursion, {&recrm1p}} };
    Node ifrecgt1block = { Block { {&recrm1call} } };
    Node ifrecgt1 = { IfStmt{&recgt1, &ifrecgt1block, {}} };
    // else n *= 1.5
    Node recf0nmul15 = { SetOperation{BinaryOps::Multiply, &identN, &c_1_5f} };
    Node recf0block = { Block { {&recf0nmul15} } };
    // elseif rec == 2, call func2
    Node identfunc2 = { Identifier{"func2"} };
    Node receq2 = { BinaryOperation{BinaryOps::Eq, &identrec, &c_2s} };
    Node recf2call = { MethodCall{&identfunc2, {}} };
    Node recf2block = { Block { {&recf2call} } };
    Node ifrec2 = { IfStmt{&receq2, &recf2block, &recf0block} };
    // elseif rec == 3, call func3
    Node identfunc3 = { Identifier{"func3"} };
    Node receq3 = { BinaryOperation{BinaryOps::Eq, &identrec, &c_3s} };
    Node recf3call = { MethodCall{&identfunc3, {}} };
    Node recf3block = { Block { {&recf3call} } };
    Node ifrec3 = { IfStmt{&receq3, &recf3block, &ifrec2} };
    // elseif rec == 4, call func4
    Node identfunc4 = { Identifier{"func4"} };
    Node receq4 = { BinaryOperation{BinaryOps::Eq, &identrec, &c_4s} };
    Node recf4call = { MethodCall{&identfunc4, {}} };
    Node recf4block = { Block { {&recf4call} } };
    Node ifrec4 = { IfStmt{&receq4, &recf4block, &ifrec3} };
    // elseif rec == 5, call func5
    Node identfunc5 = { Identifier{"func5"} };
    Node receq5 = { BinaryOperation{BinaryOps::Eq, &identrec, &c_5s} };
    Node recf5call = { MethodCall{&identfunc5, {}} };
    Node recf5block = { Block { {&recf5call} } };
    Node ifrec5 = { IfStmt{&receq5, &recf5block, &ifrec4} };
    // end
    Node recblock = { Block{ {&ifrecgt1, &ifrec5} } };
    Node recdef = { MethodDefinition{"recursion", {"rec"}, &recblock} };

    // main:
    // N = 0.0f
    Node setNto0 = { SetOperation{{}, &identN, &c_0f} };

    Node varidec = { VariableDeclaration{"i", "f32"} };
    // for (i = 0; ...
    Node identi = { Identifier{"i"} };
    Node setito0 = { SetOperation{{}, &identi, &c_0f} };

    // ... i <= 1000000.0f; ...
    Node c_endgoal = { ConstF32{iterate_to} };
    Node forcomparison = { BinaryOperation{BinaryOps::LessEqual, &identi, &c_endgoal} };

    // i += 0.25)
    Node c_025f = { ConstF32{0.25f} };
    Node foriter = { SetOperation{BinaryOps::Add, &identi, &c_025f} };

    // average(i, i+1)
    // i
    Node avgp1 = { CallParam{&identi} };
    // i+1
    Node iplus1 = { BinaryOperation{BinaryOps::Add, &identi, &c_1f} };
    Node avgp2 = { CallParam{&iplus1} };
    // average()
    Node callavg = { MethodCall{&identaverage, {&avgp1, &avgp2}} };

    // recursion(5)
    Node recp1 = { CallParam{&c_5s} };
    Node callrec = { MethodCall{&identRecursion, {&recp1}} };

    // if N > 100: N = 0
    Node c_100f = { ConstF32{100.0f} };
    Node nlt100 = { BinaryOperation{BinaryOps::Greater, &identN, &c_100f} };
    Node ifnlt100 = { IfStmt{&nlt100, &setNto0, {}} };

    Node forblock = { Block { {&callavg, &callrec, &ifnlt100, &foriter} } };
    Node forloop = { DoWhile{&forblock, &forcomparison} };
    Node forstartif = { IfStmt{&forcomparison, &forloop, {}} };

    Node mainblock = { Block{ {&setNto0, &varidec, &setito0, &forstartif,} } };
    Node mainm = { MethodDefinition{"main", {}, &mainblock} };

    Node root = { GlobalBlock{ {
        &avgdecl, &func5decl, &func4decl, &func3decl, &func2decl, &recursiondecl, &maindecl,
        &global,
        &avgm, &func5def, &func4def, &func3def, &func2def, &recdef, &mainm
        } } };

    TypeTable types;
    types["void"] = TypeInfo{"void", {}, 0, PrimitiveType::empty};
    types["bool"] = TypeInfo{"bool", {}, runtimesizeof<bool>(), PrimitiveType::boolean};
    types["s32"] = TypeInfo{"s32", {}, runtimesizeof<int>(), PrimitiveType::s32};
    types["f32"] = TypeInfo{"f32", {}, runtimesizeof<float>(), PrimitiveType::f32};
    types["f32(f32,f32)"] = TypeInfo{
        "f32(f32,f32)", {},
        0,
        MethodType{
            "f32",
            {
                MethodTypeParameter{"f32"},
                MethodTypeParameter{"f32"},
            }
        }
    };
    types["void()"] = TypeInfo{
        "void()", {},
        0,
        MethodType{
            "void",
            {}
        }
    };
    types["void(s32)"] = TypeInfo{
        "void(s32)", {},
        0,
        MethodType{
            "void",
            {
                MethodTypeParameter{"s32"},
            }
        }
    };

    auto program = Compile(&root, types, {});
    auto dur = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1> >>(std::chrono::steady_clock::now() - m_beg).count();
    std::cout << "compile time: " << dur << "s\n";

    std::cout << "compiled size: " << program->get_code().size() << "\n";

    m_beg = std::chrono::steady_clock::now();

    auto scriptaverage = program->method<float,float,float>("average");
    auto scriptmain = program->method<void>("main");

    std::shared_ptr<VMFixedStack> globals = program->generate_state();

    VM* vm = new VM(VMSTACK_PAGE_SIZE);

    scriptmain(*vm, *globals);
    std::cout << "checking N: " << globals->cvalue<float>(program->get_global_address("N")) << "\n";

    dur = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1> >>(std::chrono::steady_clock::now() - m_beg).count();
    std::cout << "run time: " << dur << "s\n";

    float avg = scriptaverage(*vm, *globals, 3, 2);
    std::cout << "-----\nchecking avg: " << avg << "\n";
}

void vm_demo() {
    BuiltinRunnable<float, float, float> average([](float a, float b) -> float {
        //std::cout << a << " " << b << " " << (a + b)/2.0f << "\n";
        return (a + b) / 2.0f;
    });
    BuiltinRunnable<void, float> printf32([](float a) {
        std::cout << a << "\n";
    });

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
               StackAddressForward(LocMemoryDirect, 4)),
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
               StackAddressForward(LocMemoryDirect, 8)),
        Opcode(Bytecode::s32Set,
               ConstantAddress(LocMemoryDirect, program.get_constant_address("5")),
               StackAddressForward(LocMemoryDirect, 12)),
        Opcode(Bytecode::Call,
               ScriptCall(LocMemoryDirect, program.get_method_address("Recursion")),
               StackAddressForward(LocMemoryDirect, 12)),
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
    program.late_register_method<void>("main");
    program.late_register_method<float,float,float>("LAverage");
    std::cout << "hand written size: " << program.get_code().size() << "\n";

    auto m_beg = std::chrono::steady_clock::now();

    VM* vm = new VM(VMSTACK_PAGE_SIZE);

    std::shared_ptr<VMFixedStack> globals = program.generate_state();
    auto scriptmain = program.method<void>("main");
    scriptmain(*vm, *globals);

    float N = *globals->at<float>(program.get_global_address("N"));
    std::cout << "N = " << N << "\n";

    auto dur = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1> >>(std::chrono::steady_clock::now() - m_beg).count();
    std::cout << "run time: " << dur << "s\n";

    auto scriptaverage = program.method<float,float,float>("LAverage");
    float avg = scriptaverage(*vm, *globals, 1, 2);
    std::cout << "checking: " << avg << "\n";

    delete vm;
}

int main() {
    compile_demo();
    //std::cout << "\n+++++\n";
    //vm_demo();
    return 0;
}
