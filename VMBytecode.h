#pragma once

enum class Bytecode: unsigned int {
    Break, // _ _ _

    AddressOf, // [a] [out] _
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
    CallExtern, // [addr] _ _
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

const DataLoc LocTableBits = 0x03;
const DataLoc LocIndirectBits = 0x0C;

// which table are we using? 2 bits (bits 0-1)
// "constant" table.
const DataLoc LocTableConst = 0x00;
// "global" table.
const DataLoc LocTableGlobal = 0x01;
// "local" stack table, but base+value or ip+value for jumps
const DataLoc LocTableLocal = 0x02;
// similar to above, but base-value for stack or ip-value for jumps
const DataLoc LocTableBack = 0x03;

// Indirect values. 2 bits (bits 2-3). sign does not apply to the result.
// indirects means the value (from a table above) contains the address
// into a table that is specified by the name (Const, Stack, Global)Indirect.
const DataLoc LocIndirectConst = 0x04;
const DataLoc LocIndirectGlobal = 0x08;
// As named, this is not a local (base+addr). it is an exact address into Stack.
const DataLoc LocIndirectStack = 0x0C;

struct Opcode {
public:
    Bytecode op:7;
    DataLoc l1:4;
    DataLoc l2:4;
    DataLoc l3:4;
    size_t p1:15;
    size_t p2:15;
    size_t p3:15;

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
