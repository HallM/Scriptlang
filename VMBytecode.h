#pragma once

enum class Bytecode: unsigned int {
    Break, // _ _ _

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

// we have 5 bits, so 32 possible values
enum class DataLoc: unsigned int {
    // c for "constant", just a value direct. the opcode determines type.
    C,
    // g for "global". accesses globals or could be an exact jump
    G,
    // o for "offset". this is like a local var (to the base) or foward jump (to the IP)
    O,
    // r for reverse offset. backwards jumps only. no reverse local vars.
    R,
    // indirect: a local stores the exact (stack) address
    // I,

    // global indirection
    // GI,
    // local indirection
    // LI,
    // param indirection
    // PI,
};

union Opdata {
    size_t global_address;
    size_t constant_address;
    size_t local_address;
    size_t indirect_address;
    size_t backward_jump;
    size_t forward_jump;
    size_t exact_jump;
};
Opdata GlobalAddress(size_t addr);
Opdata LocalAddress(size_t addr);
Opdata BackwardJump(size_t addr);
Opdata ForwardJump(size_t addr);
Opdata ExactJump(size_t addr);
Opdata ConstAddress(size_t addr);

struct CombinedOperation {
    // 32 bits almost fully allocated.
    // We need ~250 instructions (8 int types, 18 insts; 2 float types, 12 insts; some extras)
    Bytecode op:16;
    DataLoc l1:5;
    DataLoc l2:5;
    DataLoc l3:5;

    CombinedOperation(Bytecode o, DataLoc a, DataLoc b, DataLoc c) : op(o), l1(a), l2(b), l3(c) {}
};

struct Opcode {
public:
    Bytecode op:10;
    DataLoc l1:5;
    DataLoc l2:5;
    DataLoc l3:5;
    size_t p1:13;
    size_t p2:13;
    size_t p3:13;

    Opcode(
        Bytecode o, DataLoc il1, DataLoc il2, DataLoc il3, size_t a, size_t b, size_t c
    )
    : 
      //opcode(o, l1, l2, l3),
      op(o),
      l1(il1),
      l2(il2),
      l3(il3),
      p1(a),
      p2(b),
      p3(c) {}
};
