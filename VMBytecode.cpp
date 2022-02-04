#include "VMBytecode.h"

Opdata GlobalAddress(size_t addr) {
    Opdata ret;
    ret.global_address = addr;
    return ret;
}

Opdata LocalAddress(size_t addr) {
    Opdata ret;
    ret.local_address = addr;
    return ret;
}

Opdata BackwardJump(size_t addr) {
    Opdata ret;
    ret.backward_jump = addr;
    return ret;
}

Opdata ForwardJump(size_t addr) {
    Opdata ret;
    ret.forward_jump = addr;
    return ret;
}

Opdata ExactJump(size_t addr) {
    Opdata ret;
    ret.exact_jump = addr;
    return ret;
}

Opdata ConstAddress(size_t addr) {
    Opdata ret;
    ret.constant_address = addr;
    return ret;
}
