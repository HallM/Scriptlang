#include "VMBytecode.h"

size_t
ConstantAddress(size_t offset) {
    return offset & ParamAddressOffsetMask;
}

size_t
GlobalAddress(size_t offset) {
    size_t page = 1 << ParamAddressPageBit;
    return page | (offset & ParamAddressOffsetMask);
}

size_t
StackAddressForward(size_t offset) {
    size_t page = 2 << ParamAddressPageBit;
    return page | (offset & ParamAddressOffsetMask);
}

size_t
StackAddressBackward(size_t offset) {
    size_t page = 3 << ParamAddressPageBit;
    return page | (offset & ParamAddressOffsetMask);
}

size_t
JumpExact(size_t address) {
    return (address& ParamAddressOffsetMask);
}

size_t
JumpOffsetForward(size_t offset) {
    size_t page = 2 << ParamAddressPageBit;
    return page | (offset & ParamAddressOffsetMask);
}

size_t
JumpOffsetBackward(size_t offset) {
    size_t page = 3 << ParamAddressPageBit;
    return page | (offset & ParamAddressOffsetMask);
}
