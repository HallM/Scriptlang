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

Opdata ParamAddress(size_t addr) {
    Opdata ret;
    ret.param_address = addr;
    return ret;
}

Opdata ConstData(int v) {
    Opdata ret;
    ret.const_s32 = v;
    return ret;
}

Opdata ConstData(float v) {
    Opdata ret;
    ret.const_f32 = v;
    return ret;
}