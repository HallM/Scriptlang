#pragma once

#include "VMStack.h"

enum class Bytecode: unsigned int {
    Break, // _ _ _

    DataAddress, // [a] [out] _
    FunctionAddress, // [a] [out] _

    s32Set, // [a] [out] _
    f32Set,

    s32Add, // [a] [b] [out]
    s32Sub,
    s32Mul,
    s32Div,
    s32Mod,

    f32Add,
    f32Sub,
    f32Mul,
    f32Div,
    f32Mod,

    // sets the base plus handles reserving some stack space for a function
    // BeginFrame, // [size] _ _
    // calls assume that base-P is each param. for example: int f(int a,int b)
    // a is base-4
    // b is base-8
    // results are stored in the same manner as params.
    // base-(sizeof(result)) is the return.
    // base-4 in this case.
    CallRunnable, // [addr] [parambytes] _
    // The caller (Call) needs to allocate the stack for the callee.
    // It also adds the parameters in it's own stack space, but places
    // the params starting at base+0.
    Call, // [addr] [parambytes] [stackbytes]
    // ret also clears a stackframe. so there MUST be a stackframe set
    Ret, // _ _ _

    // note than jumping 0 is still advancing 1
    // Global: jump to exact location
    // Local: jump forward to relative location
    // Param: jump backward to relative location
    Jump, // [jumpto] _ _

    // conditional jumps
    // JLT [p1] [p2] [jumpto]
    f32JLT, // a b [jumpto]
    f32JLE,
    f32JGT,
    f32JGE,
    f32JEQ,
    f32JNE,

    s32JLT,
    s32JLE,
    s32JGT,
    s32JGE,
    s32JEQ,
    s32JNE,
};

// we need 4 bits
typedef unsigned int DataLoc;

// exact addresses in params work like:
// bit 17: is stack value (0 for const/global, 1 for stack/jumps). unused by jumps
// bit 16: 0:const / 1:global, or sign bit for stack/jumps
// bits 0-15: address
// for stack, the address is added/subtracted to base to get a final address
// for jumps, this is added to the IP, using same signbit of stack
const DataLoc LocMemoryDirect = 0x00;
// use the above rules to determine the location of what holds a size_t aka void*
// that points to a real-memory address.
// for jumps, the indirect value is added to the ip, using same signbit of stack
const DataLoc LocMemoryIndirect = 0x01;

// only used for stack/jumps
const size_t MemoryAddressSignBit = 1 << 15;

// the base+/base- addresses are always stack addresses.
// TODO: I might add an auto bitshift left 2 bits.

const size_t ParamAddressPageBit = 16;
const size_t ParamAddressPageMask = 0x03 << ParamAddressPageBit;
const size_t ParamAddressOffsetMask = (~ParamAddressPageMask) & 0x3FFFF;

size_t ConstantAddress(size_t offset);
size_t GlobalAddress(size_t offset);
size_t StackAddressForward(size_t offset);
size_t StackAddressBackward(size_t offset);
size_t JumpExact(size_t address);
size_t JumpOffsetForward(size_t offset);
size_t JumpOffsetBackward(size_t offset);

struct Opcode {
public:
    Bytecode op:7;
    DataLoc l1:1;
    DataLoc l2:1;
    DataLoc l3:1;
    size_t p1:18;
    size_t p2:18;
    size_t p3:18;

    Opcode(
        Bytecode o, DataLoc il1, DataLoc il2, DataLoc il3, size_t a, size_t b, size_t c
    ) : 
      op(o),
      l1(il1),
      l2(il2),
      l3(il3),
      p1(a),
      p2(b),
      p3(c) {}

    Opcode(
        Bytecode o, DataLoc il1, DataLoc il2, size_t a, size_t b
    ) : 
      op(o),
      l1(il1),
      l2(il2),
      l3(0),
      p1(a),
      p2(b),
      p3(0) {}

    Opcode(
        Bytecode o, DataLoc il1, size_t a
    ) : 
      op(o),
      l1(il1),
      l2(0),
      l3(0),
      p1(a),
      p2(0),
      p3(0) {}
};
