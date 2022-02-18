#include "VMBytecode.h"

bool
BytecodeParam::operator==(const BytecodeParam& r) const {
    return (this->loc == r.loc) && (this->page == r.page) && (this->offset == r.offset);
}

BytecodeParam
ConstantAddress(DataLoc loc, size_t offset) {
    return {loc, 0, offset};
}

BytecodeParam
GlobalAddress(DataLoc loc, size_t offset) {
    return {loc, 1, offset};
}

BytecodeParam
StackAddressForward(DataLoc loc, size_t offset) {
    return {loc, 2, offset};
}

BytecodeParam
StackAddressBackward(DataLoc loc, size_t offset) {
    return {loc, 3, offset};
}

BytecodeParam
JumpExact(DataLoc loc, size_t address) {
    return {loc, 0, address};
}

BytecodeParam
JumpOffsetForward(DataLoc loc, size_t offset) {
    return {loc, 2, offset};
}

BytecodeParam
JumpOffsetBackward(DataLoc loc, size_t offset) {
    return {loc, 3, offset};
}

BytecodeParam
ScriptCall(DataLoc loc, size_t offset) {
    return {loc, 0, offset};
}

BytecodeParam
ExternalCall(DataLoc loc, size_t offset) {
    return {loc, 1, offset};
}

BytecodeParam
FastCallForward(DataLoc loc, size_t offset) {
    return {loc, 2, offset};
}

BytecodeParam
FastCallBackward(DataLoc loc, size_t offset) {
    return {loc, 3, offset};
}

BytecodeParam
StackSize(DataLoc loc, size_t bytes) {
    return {loc, 0, bytes};
}

size_t
merge_page_offset(BytecodeParam param) {
    return (param.page << ParamAddressPageBit) | (param.offset & ParamAddressOffsetMask);
}

size_t
address_page(size_t address) {
    return (address & ParamAddressPageMask) >> ParamAddressPageBit;
}

size_t
address_offset(size_t address) {
    return address & ParamAddressOffsetMask;
}
