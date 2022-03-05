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
#include "Compiler.h"
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

void
compile_code_test() {
    std::cout << "\n++++++++\n";

    auto m_beg = std::chrono::steady_clock::now();

    std::ifstream infile("test.wut");
    std::string contents(
        (std::istreambuf_iterator<char>(infile)),
        (std::istreambuf_iterator<char>())
    );

    MattScript::Compiler compiler;

    auto pointbuilder = compiler.build_struct<Point2f>("Point2f");
    pointbuilder.add_member<float>("x", offsetof(Point2f, x));
    pointbuilder.add_member<float>("y", offsetof(Point2f, y));
    pointbuilder.build();

    compiler.import_method<void,Point2f*>("print_point2f", print_point2f);
    compiler.import_method<void,int>("print_s32", print_s32);
    compiler.import_method<void,float>("print_f32", print_f32);

    auto program = compiler.compile("test.wut", contents);

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
    compile_code_test();
    return 0;
}
