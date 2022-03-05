// main.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <chrono>

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>

#include "VM.h"
#include "VMFFI.h"
#include "Program.h"
#include "AST.h"
#include "BytecodeGenerator.h"
#include "Parser.h"
#include "Tokenizer.h"
#include "Types.h"

const float iterate_to = 1.0f;

struct Point2f {
    float x;
    float y;
};

void print_point2f(Point2f* p) {
    std::cout << "Point is: ";
    std::cout << p->x << ", " << p->y << "\n";
}
void print_s32(int x) {
    std::cout << "s32: ";
    std::cout << x << "\n";
}
void print_f32(float x) {
    std::cout << "f32: ";
    std::cout << x << "\n";
}

// the lua application including compilation was 1 second 249ms (0.0031ms per loop)
// result is 54.6 for 5x iterations.

void compileast_demo() {
    auto m_beg = std::chrono::steady_clock::now();

    Node avgdecl = { MethodDeclaration{"average", "f32(f32,f32)"} };
    Node func5decl = { MethodDeclaration{"func5", "void()"} };
    Node func4decl = { MethodDeclaration{"func4", "void()"} };
    Node func3decl = { MethodDeclaration{"func3", "void()"} };
    Node func2decl = { MethodDeclaration{"func2", "void()"} };
    Node recursiondecl = { MethodDeclaration{"recursion", "void(s32)"} };
    Node maindecl = { MethodDeclaration{"main", "void()"} };
    Node testdecl = { MethodDeclaration{"test", "void(ref Point2f)"} };

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
    Node sumxy = { BinaryOperation{BinaryOps::Add, std::make_shared<Node>(identx), std::make_shared<Node>(identy)} };

    Node avgxy = { BinaryOperation{BinaryOps::Divide, std::make_shared<Node>(sumxy), std::make_shared<Node>(c_2f)} };
    Node avgret = { ReturnValue{std::make_shared<Node>(avgxy)} };

    Node avgblock = { Block{ {std::make_shared<Node>(avgret)} } };
    Node avgm = { MethodDefinition{"average", "f32(f32,f32)", {"x", "y"}, std::make_shared<Node>(avgblock)} };

    // func5() = N += average(n, n)
    Node f5avgp = { CallParam{std::make_shared<Node>(identN)} };
    Node f5avg = { MethodCall{std::make_shared<Node>(identaverage), {std::make_shared<Node>(f5avgp), std::make_shared<Node>(f5avgp)}} };
    Node f5addavgtoN = { SetOperation{BinaryOps::Add, std::make_shared<Node>(identN), std::make_shared<Node>(f5avg)} };
    Node func5block = { Block{ {std::make_shared<Node>(f5addavgtoN)} } };
    Node func5def = { MethodDefinition{"func5", "void()", {}, std::make_shared<Node>(func5block)} };

    // func4() = N += 2*average(n+1, n+2)
    Node nplus1 = { BinaryOperation{BinaryOps::Add, std::make_shared<Node>(identN), std::make_shared<Node>(c_1f)} };
    Node nplus2 = { BinaryOperation{BinaryOps::Add, std::make_shared<Node>(identN), std::make_shared<Node>(c_2f)} };
    Node f4avgp1 = { CallParam{std::make_shared<Node>(nplus1)} };
    Node f4avgp2 = { CallParam{std::make_shared<Node>(nplus2)} };
    Node f4avg = { MethodCall{std::make_shared<Node>(identaverage), {std::make_shared<Node>(f4avgp1), std::make_shared<Node>(f4avgp2)}} };
    Node f4avgtimes2 = { BinaryOperation{BinaryOps::Multiply, std::make_shared<Node>(f4avg), std::make_shared<Node>(c_2f)} };
    Node f4addavgtoN = { SetOperation{BinaryOps::Add, std::make_shared<Node>(identN), std::make_shared<Node>(f4avgtimes2)} };
    Node func4block = { Block{ {std::make_shared<Node>(f4addavgtoN)} } };
    Node func4def = { MethodDefinition{"func4", "void()", {}, std::make_shared<Node>(func4block)} };

    // func3() = n *= 2.1
    Node c_2_1f = { ConstF32{2.1f} };
    Node f3multoN = { SetOperation{BinaryOps::Multiply, std::make_shared<Node>(identN), std::make_shared<Node>(c_2_1f)} };
    Node func3block = { Block{ {std::make_shared<Node>(f3multoN)} } };
    Node func3def = { MethodDefinition{"func3", "void()", {}, std::make_shared<Node>(func3block)} };

    // func2() = n /= 3.5
    Node c_3_5f = { ConstF32{3.5f} };
    Node f2divtoN = { SetOperation{BinaryOps::Divide, std::make_shared<Node>(identN), std::make_shared<Node>(c_3_5f)} };
    Node func2block = { Block{ {std::make_shared<Node>(f2divtoN)} } };
    Node func2def = { MethodDefinition{"func2", "void()", {}, std::make_shared<Node>(func2block)} };

    // recursion(rec)
    Node identrec = { Identifier{"rec"} };
    Node c_1s = { ConstS32{1} };
    Node c_2s = { ConstS32{2} };
    Node c_3s = { ConstS32{3} };
    Node c_4s = { ConstS32{4} };
    Node c_5s = { ConstS32{5} };
    Node c_1_5f = { ConstF32{1.5f} };
    // if rec >= 1
    Node recgt1 = { BinaryOperation{BinaryOps::GreaterEqual, std::make_shared<Node>(identrec), std::make_shared<Node>(c_1s)} };
    // then recursion(rec-1)
    Node recminus1 = { BinaryOperation{BinaryOps::Subtract, std::make_shared<Node>(identrec), std::make_shared<Node>(c_1s)} };
    Node recrm1p = { CallParam{std::make_shared<Node>(recminus1)} };
    Node recrm1call = { MethodCall{std::make_shared<Node>(identRecursion), {std::make_shared<Node>(recrm1p)}} };
    Node ifrecgt1block = { Block { {std::make_shared<Node>(recrm1call)} } };
    Node ifrecgt1 = { IfStmt{std::make_shared<Node>(recgt1), std::make_shared<Node>(ifrecgt1block), {}} };
    // else n *= 1.5
    Node recf0nmul15 = { SetOperation{BinaryOps::Multiply, std::make_shared<Node>(identN), std::make_shared<Node>(c_1_5f)} };
    Node recf0block = { Block { {std::make_shared<Node>(recf0nmul15)} } };
    // elseif rec == 2, call func2
    Node identfunc2 = { Identifier{"func2"} };
    Node receq2 = { BinaryOperation{BinaryOps::Eq, std::make_shared<Node>(identrec), std::make_shared<Node>(c_2s)} };
    Node recf2call = { MethodCall{std::make_shared<Node>(identfunc2), {}} };
    Node recf2block = { Block { {std::make_shared<Node>(recf2call)} } };
    Node ifrec2 = { IfStmt{std::make_shared<Node>(receq2), std::make_shared<Node>(recf2block), std::make_shared<Node>(recf0block)} };
    // elseif rec == 3, call func3
    Node identfunc3 = { Identifier{"func3"} };
    Node receq3 = { BinaryOperation{BinaryOps::Eq, std::make_shared<Node>(identrec), std::make_shared<Node>(c_3s)} };
    Node recf3call = { MethodCall{std::make_shared<Node>(identfunc3), {}} };
    Node recf3block = { Block { {std::make_shared<Node>(recf3call)} } };
    Node ifrec3 = { IfStmt{std::make_shared<Node>(receq3), std::make_shared<Node>(recf3block), std::make_shared<Node>(ifrec2)} };
    // elseif rec == 4, call func4
    Node identfunc4 = { Identifier{"func4"} };
    Node receq4 = { BinaryOperation{BinaryOps::Eq, std::make_shared<Node>(identrec), std::make_shared<Node>(c_4s)} };
    Node recf4call = { MethodCall{std::make_shared<Node>(identfunc4), {}} };
    Node recf4block = { Block { {std::make_shared<Node>(recf4call)} } };
    Node ifrec4 = { IfStmt{std::make_shared<Node>(receq4), std::make_shared<Node>(recf4block), std::make_shared<Node>(ifrec3)} };
    // elseif rec == 5, call func5
    Node identfunc5 = { Identifier{"func5"} };
    Node receq5 = { BinaryOperation{BinaryOps::Eq, std::make_shared<Node>(identrec), std::make_shared<Node>(c_5s)} };
    Node recf5call = { MethodCall{std::make_shared<Node>(identfunc5), {}} };
    Node recf5block = { Block { {std::make_shared<Node>(recf5call)} } };
    Node ifrec5 = { IfStmt{std::make_shared<Node>(receq5), std::make_shared<Node>(recf5block), std::make_shared<Node>(ifrec4)} };
    // end
    Node recblock = { Block{ {std::make_shared<Node>(ifrecgt1), std::make_shared<Node>(ifrec5)} } };
    Node recdef = { MethodDefinition{"recursion", "void(s32)", {"rec"}, std::make_shared<Node>(recblock)} };

    // main:
    // N = 0.0f
    Node setNto0 = { SetOperation{{}, std::make_shared<Node>(identN), std::make_shared<Node>(c_0f)} };

    Node varidec = { VariableDeclaration{"i", "f32"} };
    // for (i = 0; ...
    Node identi = { Identifier{"i"} };
    Node setito0 = { SetOperation{{}, std::make_shared<Node>(identi), std::make_shared<Node>(c_0f)} };

    // ... i <= 1000000.0f; ...
    Node c_endgoal = { ConstF32{iterate_to} };
    Node forcomparison = { BinaryOperation{BinaryOps::LessEqual, std::make_shared<Node>(identi), std::make_shared<Node>(c_endgoal)} };

    // i += 0.25)
    Node c_025f = { ConstF32{0.25f} };
    Node foriter = { SetOperation{BinaryOps::Add, std::make_shared<Node>(identi), std::make_shared<Node>(c_025f)} };

    // average(i, i+1)
    // i
    Node avgp1 = { CallParam{std::make_shared<Node>(identi)} };
    // i+1
    Node iplus1 = { BinaryOperation{BinaryOps::Add, std::make_shared<Node>(identi), std::make_shared<Node>(c_1f)} };
    Node avgp2 = { CallParam{std::make_shared<Node>(iplus1)} };
    // average()
    Node callavg = { MethodCall{std::make_shared<Node>(identaverage), {std::make_shared<Node>(avgp1), std::make_shared<Node>(avgp2)}} };

    // recursion(5)
    Node recp1 = { CallParam{std::make_shared<Node>(c_5s)} };
    Node callrec = { MethodCall{std::make_shared<Node>(identRecursion), {std::make_shared<Node>(recp1)}} };

    // if N > 100: N = 0
    Node c_100f = { ConstF32{100.0f} };
    Node nlt100 = { BinaryOperation{BinaryOps::Greater, std::make_shared<Node>(identN), std::make_shared<Node>(c_100f)} };
    Node ifnlt100 = { IfStmt{std::make_shared<Node>(nlt100), std::make_shared<Node>(setNto0), {}} };

    Node forblock = { Block { {std::make_shared<Node>(callavg), std::make_shared<Node>(callrec), std::make_shared<Node>(ifnlt100), std::make_shared<Node>(foriter)} } };
    Node forloop = { DoWhile{std::make_shared<Node>(forblock), std::make_shared<Node>(forcomparison)} };
    Node forstartif = { IfStmt{std::make_shared<Node>(forcomparison), std::make_shared<Node>(forloop), {}} };

    Node mainblock = { Block{ {std::make_shared<Node>(setNto0), std::make_shared<Node>(varidec), std::make_shared<Node>(setito0), std::make_shared<Node>(forstartif),} } };
    Node mainm = { MethodDefinition{"main", "void()", {}, std::make_shared<Node>(mainblock)} };

    // test(pv: ref point2f)
    Node pvident = { Identifier{"pv"} };
    Node pvxident = { Identifier{"x"} };
    Node pvxaccess = { AccessMember{std::make_shared<Node>(pvident), std::make_shared<Node>(pvxident)} };
    Node pvxset = { SetOperation{BinaryOps::Add, std::make_shared<Node>(pvxaccess), std::make_shared<Node>(c_1f)} };

    Node printident = { Identifier{"print_point2f"} };
    Node printparam = { CallParam{std::make_shared<Node>(pvident)} };
    Node callprint = { MethodCall{std::make_shared<Node>(printident), {std::make_shared<Node>(printparam)}} };

    Node testblock = { Block{ {std::make_shared<Node>(pvxset), std::make_shared<Node>(callprint)} } };
    Node testm = { MethodDefinition{"test", "void(ref Point2f)", {"pv"}, std::make_shared<Node>(testblock)} };

    Node root = { GlobalBlock{ {
        std::make_shared<Node>(avgdecl), std::make_shared<Node>(func5decl), std::make_shared<Node>(func4decl), std::make_shared<Node>(func3decl), std::make_shared<Node>(func2decl), std::make_shared<Node>(recursiondecl), std::make_shared<Node>(maindecl), std::make_shared<Node>(testdecl),
        std::make_shared<Node>(global),
        std::make_shared<Node>(avgm), std::make_shared<Node>(func5def), std::make_shared<Node>(func4def), std::make_shared<Node>(func3def), std::make_shared<Node>(func2def), std::make_shared<Node>(recdef), std::make_shared<Node>(mainm), std::make_shared<Node>(testm)
        } } };

    TypeTable types;
    types.imported_struct_type<Point2f>("Point2f", {
        types.imported_struct_member<float>("x", offsetof(Point2f, x)),
        types.imported_struct_member<float>("y", offsetof(Point2f, y)),
    });
    types.add_method("f32", { {"f32"}, {"f32"} });
    types.add_method("void", {});
    types.add_method("void", { {"s32"} });
    types.add_method("void", { {"ref Point2f"} });

    BuiltinRunnable<void, Point2f*> printpointrunnable(print_point2f);
    std::vector<ImportedMethod> imported_methods = {
        ImportedMethod{
            "print_point2f",
            &printpointrunnable,
            "void(ref Point2f)",
            typeid(void),
            { typeid(Point2f*) }
        }
    };

    auto program = generate_bytecode(std::make_shared<Node>(root), types, imported_methods);
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

    std::cout << "-----\ntry test:\n";
    auto scripttest = program->method<void, Point2f*>("test");
    Point2f p = {1.23f, 98.76f};
    scripttest(*vm, *globals, &p);
}

void
compile_code_test() {
    std::cout << "\n++++++++\n";

    auto m_beg = std::chrono::steady_clock::now();

    std::ifstream infile("test.wut");
    std::string contents(
        (std::istreambuf_iterator<char>(infile)),
        (std::istreambuf_iterator<char>())
    );
    std::cout << contents << "\n";

    TypeTable types;
    types.imported_struct_type<Point2f>("Point2f", {
        types.imported_struct_member<float>("x", offsetof(Point2f, x)),
        types.imported_struct_member<float>("y", offsetof(Point2f, y)),
    });
    types.add_method("void", { {"ref Point2f"} });
    types.add_method("void", { {"s32"} });
    types.add_method("void", { {"f32"} });

    BuiltinRunnable<void, Point2f*> printpoint_runnable(print_point2f);
    BuiltinRunnable<void, int> prints32_runnable(print_s32);
    BuiltinRunnable<void, float> printf32_runnable(print_f32);
    std::vector<ImportedMethod> imported_methods = {
        ImportedMethod{
            "print_point2f",
            &printpoint_runnable,
            "void(ref Point2f)",
            typeid(void),
            { typeid(Point2f*) }
        },
        ImportedMethod{
            "print_s32",
            &prints32_runnable,
            "void(s32)",
            typeid(void),
            { typeid(int) }
        },
        ImportedMethod{
            "print_f32",
            &printf32_runnable,
            "void(f32)",
            typeid(void),
            { typeid(float) }
        }
    };

    auto tokens = tokenize_string(contents);
    auto ast = parse_to_ast("test.wut", tokens, types);
    auto program = generate_bytecode(ast->root, types, imported_methods);

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

    std::cout << "-----\ntry test:\n";
    auto scripttest = program->method<void, Point2f*>("test");
    Point2f p = {1.23f, 98.76f};
    scripttest(*vm, *globals, &p);
}

int main() {
    compileast_demo();
    compile_code_test();
    return 0;
}
