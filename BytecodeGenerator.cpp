#include "BytecodeGenerator.h"

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <variant>
#include <vector>

#include "AST.h"
#include "Program.h"
#include "Types.h"
#include "VMBytecode.h"
#include "VMFFI.h"

//
// default, included types
// check locals created
// references
// dereferences
// dereference+access
// change Program interface
// iterables
// for-each loops
// lists?
// if-let
// block expressions
// shadow variables
// global set values
//
// optimize jumps, short circuits, etc
//

namespace MattScript {
namespace Generator {

// TODO: get rid of these or move them to somewhere central
const std::string type_empty = "void";
const std::string type_bool = "bool";
const std::string type_s32 = "s32";
const std::string type_f32 = "f32";

std::unordered_map<Ast::BinaryOps, Bytecode> jump_opcode(std::string type) {
    // so these have to be the inverse to work right.
    if (type == type_s32) {
        return {
            {Ast::BinaryOps::Eq, Bytecode::s32JNE},
            {Ast::BinaryOps::NotEq, Bytecode::s32JEQ},
            {Ast::BinaryOps::Less, Bytecode::s32JGE},
            {Ast::BinaryOps::LessEqual, Bytecode::s32JGT},
            {Ast::BinaryOps::Greater, Bytecode::s32JLE},
            {Ast::BinaryOps::GreaterEqual, Bytecode::s32JLT},
        };
    } else if (type == type_f32) {
        return {
            {Ast::BinaryOps::Eq, Bytecode::f32JNE},
            {Ast::BinaryOps::NotEq, Bytecode::f32JEQ},
            {Ast::BinaryOps::Less, Bytecode::f32JGE},
            {Ast::BinaryOps::LessEqual, Bytecode::f32JGT},
            {Ast::BinaryOps::Greater, Bytecode::f32JLE},
            {Ast::BinaryOps::GreaterEqual, Bytecode::f32JLT},
        };
    }
    return {};
}

struct labellink {
    size_t labelid;
};
struct methodlink {
    std::string name;
    std::vector<std::string> scopes;
};

struct compiled_result {
    std::string type;
    bool is_mutable;
    bool assignable;
    // the current address information for the resulting data of this expression.
    std::variant<BytecodeParam, labellink, methodlink> address;
    // number of bytes of stack that are held onto (returned)
    size_t stack_bytes_returned;
    // the total stack used for reservations
    size_t stack_bytes_used;
};

struct labellinkable {
    // the param (0-2) that needs to be linked
    size_t param;
    // the label to look up
    labellink label;
};
struct methodlinkable {
    // the param (0-2) that needs to be linked
    size_t param;
    methodlink method;
};

struct variableinfo {
    std::string name;
    std::string type;
    bool is_mutable;
    size_t address;
};

struct methodinfo {
    std::string name;
    std::string type;
    bool return_mutable;
    bool defined;
    size_t index;
    size_t address;
    size_t size;
    size_t param_bytes;
    size_t stack_bytes;
};

struct operation {
    Opcode opcode;
    std::optional<labellinkable> label_link;
    std::optional<methodlinkable> method_link;
};

struct compilerscope {
    // TODO: how to have starting values for globals
    std::vector<std::unordered_map<std::string, variableinfo>> local_variables;
    std::unordered_map<std::string, methodinfo> methods;
    std::unordered_map<std::string, size_t> imported_method_names;
    std::unordered_map<std::string, compilerscope> subscopes;

public:
    compilerscope() {
        local_variables.push_back({});
    }
};

// holds all the necessary tables as we move through the compilation step
class compiler_wip {
public:
    compiler_wip(const Types::TypeTable& t, const ImportedMethods& m) : types(t), imported_methods(m), next_label(0), next_stack(0), next_method(0), next_const(0), rootscope(), current_scope(&rootscope) {
        for (size_t i = 0; i < m.size(); i++) {
            Ast::ImportedMethod method = m[i];
            auto s = get_scope(method.scopes);
            s->imported_method_names[method.name] = i;
        }
    }

    compilerscope* get_scope(std::vector<std::string> scopepath) {
        compilerscope* s = current_scope;
        for (auto& it : scopepath) {
            auto maybe = s->subscopes.find(it);
            if (maybe == s->subscopes.end()) {
                s->subscopes[it] = compilerscope();
            }
            s = &s->subscopes[it];
        }
        return s;
    }


    void add_bytecode(Opcode oc) {
        bytecodes.push_back(operation{oc, {}, {}});
    }

    void add_bytecode_linked_method(Opcode oc, methodlink method, size_t param_index) {
        bytecodes.push_back(operation{oc, {}, methodlinkable{param_index, method}});
    }

    void add_bytecode_linked_label(Opcode oc, labellink label, size_t param_index) {
        bytecodes.push_back(operation{oc, labellinkable{param_index, label}, {}});
    }

    const Types::TypeTable& types;
    const ImportedMethods& imported_methods;

    std::vector<operation> bytecodes;
    std::unordered_map<size_t, size_t> labels;
    size_t next_label;

    size_t next_stack;
    size_t next_method;

    std::vector<std::variant<
        float,
        int,
        size_t
    >> constant_values;
    std::unordered_map<std::string, size_t> constant_addresses;
    size_t next_const;

    compilerscope rootscope;
    compilerscope* current_scope;
};

bool compatible_types(const Types::TypeInfo& a, const Types::TypeInfo& b) {
    if (a.name == b.name) {
        return true;
    }
    if (a.ref_type && a.ref_type.value() == b.name) {
        return true;
    }
    if (b.ref_type && b.ref_type.value() == a.name) {
        return true;
    }
    return false;
}

const Types::TypeInfo& get_type(std::string type_name, compiler_wip& wip) {
    return wip.types.get_type(type_name);
}

bool compatible_types_by_name(std::string a, std::string b, compiler_wip& wip) {
    return compatible_types(get_type(a, wip), get_type(b, wip));
}

Bytecode assignment_opcode(std::string type_name, compiler_wip& wip) {
    auto type = get_type(type_name, wip);
    if (type.ref_type) {
        return Bytecode::refSet;
    }
    if (std::holds_alternative<Types::PrimitiveType>(type.type)) {
        auto prim = std::get<Types::PrimitiveType>(type.type);
        if (prim == Types::PrimitiveType::s32) {
            return Bytecode::s32Set;
        }
        else if (prim == Types::PrimitiveType::f32) {
            return Bytecode::f32Set;
        }
        else if (prim == Types::PrimitiveType::boolean) {
            return Bytecode::s32Set;
        }
    }
    return Bytecode::memSet;
}

template<typename T>
size_t constant(T v, compiler_wip& wip) {
    std::ostringstream iss;
    iss << typeid(v).name() << v;
    std::string key = iss.str();

    auto it = wip.constant_addresses.find(key);
    if (it != wip.constant_addresses.end()) {
        return it->second;
    }
    size_t addr = wip.next_const;
    wip.next_const += runtimesizeof<T>();
    wip.constant_values.push_back(v);
    wip.constant_addresses[key] = addr;
    return addr;
}

std::optional<methodinfo*>
get_method_named(std::vector<std::string> scopepath, std::string name, compiler_wip& wip) {
    auto s = wip.get_scope(scopepath);
    auto maybe = s->methods.find(name);
    if (maybe == s->methods.end()) {
        return {};
    }
    return &s->methods[name];
}

compiled_result compile_node(std::shared_ptr<Ast::Node> n, compiler_wip& wip, std::optional<BytecodeParam> suggested_return);

compiled_result compile_const_s32(Ast::ConstS32& s32node, compiler_wip& wip) {
    size_t address = constant<int>(s32node.num, wip);
    return {
        type_s32,
        true,
        false,
        ConstantAddress(LocMemoryDirect, address),
        0,
        0
    };
}

compiled_result compile_const_f32(Ast::ConstF32& f32node, compiler_wip& wip) {
    size_t address = constant<float>(f32node.num, wip);
    return {
        type_f32,
        true,
        false,
        ConstantAddress(LocMemoryDirect, address),
        0,
        0
    };
}

compiled_result compile_const_bool(Ast::ConstBool& bnode, compiler_wip& wip) {
    size_t address = constant<bool>(bnode.value, wip);
    return {
        type_bool,
        true,
        false,
        ConstantAddress(LocMemoryDirect, address),
        0,
        0
    };
}

std::optional<compiled_result>
find_method(std::vector<std::string> scope_names, std::string ident, compiler_wip& wip) {
    auto maybe_method = get_method_named(scope_names, ident, wip);
    if (maybe_method) {
        // Note that method references would be handled above.
        return compiled_result{
            maybe_method.value()->type,
            false,
            false,
            methodlink{ident},
            0,
            0
        };
    }

    auto scope = wip.get_scope(scope_names);
    auto maybe_imported = scope->imported_method_names.find(ident);
    if (maybe_imported != scope->imported_method_names.end()) {
        auto method = wip.imported_methods[maybe_imported->second];
        return compiled_result{
            method.type,
            false,
            false,
            ExternalCall(LocMemoryDirect, maybe_imported->second),
            0,
            0
        };
    }
    return {};
}

compiled_result compile_identifier(Ast::Identifier n, compiler_wip& wip) {
    auto ident = n.name;

    auto scope = wip.get_scope(n.scopes);

    for (size_t i = scope->local_variables.size(); i--;) {
        auto lv = scope->local_variables[i];
        auto it = lv.find(ident);
        if (it != lv.end()) {
            auto type = wip.types.get_type(it->second.type);
            DataLoc ptr = LocMemoryDirect;
            if (type.ref_type) {
                ptr = LocMemoryIndirect;
            }
            BytecodeParam ret;
            if (i == 0) {
                ret = GlobalAddress(ptr, it->second.address);
            }
            else {
                ret = StackAddressForward(ptr, it->second.address);
            }
            return {
                type.name,
                it->second.is_mutable,
                it->second.is_mutable,
                ret,
                0,
                0
            };
        }
    }

    auto maybe_method = find_method(n.scopes, ident, wip);
    if (maybe_method) {
        return maybe_method.value();
    }

    // TODO: any way to have enum in a scope?
    if (n.scopes.size() == 1 && wip.types.type_exists(n.scopes[0])) {
        auto t = wip.types.get_type(n.scopes[0]);
        if (std::holds_alternative<Types::EnumType>(t.type)) {
            auto enum_values = std::get<Types::EnumType>(t.type).values;

            if (enum_values.contains(ident)) {
                int value = enum_values[ident];

                size_t address = constant<int>(value, wip);
                return {
                    type_s32,
                    true,
                    false,
                    ConstantAddress(LocMemoryDirect, address),
                    0,
                    0
                };
            }
        }
    }

    throw "Identifier not found";
}

compiled_result compile_memberaccess(Ast::AccessMember& access, compiler_wip& wip) {
    auto lhs = compile_node(access.container, wip, {});
    auto lhstype = wip.types.get_type(lhs.type);
    auto address = std::get<BytecodeParam>(lhs.address);

    // Tuples use consts32 (ex mytup.0) for accesses.
    //if (std::holds_alternative<TupleType>(lhstype.type)) {
    //    auto tupleinfo = std::get<TupleType>(lhstype.type);
    //    if (std::holds_alternative<ConstS32>(access.member->data)) {
    //        int index = std::get<ConstS32>(access.member->data).num;
    //        int total = (int)tupleinfo.values.size();
    //        if (index < 0) {
    //            index = total + index;
    //            if (index < 0) {
    //                throw "Tuple access out of range";
    //            }
    //        }
    //        else if (index >= total) {
    //            throw "Tuple access out of range";
    //        }
    //        auto value = tupleinfo.values.at(index);
    //        address.offset += value.offset;

    //        return {
    //            value.type,
    //            true,
    //            address,
    //            lhs.stack_bytes_returned,
    //            lhs.stack_bytes_used
    //        };
    //    }
    //}
    if (std::holds_alternative<Types::StructType>(lhstype.type)) {
        auto structinfo = std::get<Types::StructType>(lhstype.type);
        if (std::holds_alternative<Ast::Identifier>(access.member->data)) {
            std::string name = std::get<Ast::Identifier>(access.member->data).name;
            auto maybe_value = structinfo.members.find(name);
            if (maybe_value != structinfo.members.end()) {
                auto value = maybe_value->second;
                auto value_type = get_type(value.type, wip);

                if (lhstype.ref_type) {
                    // TODO: could we save stack by re-using space here?
                    // we create a new ptr to the value by doing a sizet add.
                    size_t temp = wip.next_stack;
                    wip.next_stack += sizeof(size_t);
                    auto offsetaddr = ConstantAddress(LocMemoryDirect, constant<size_t>(value.offset, wip));

                    // the result of an access to a ref is always a ref.
                    // We set the direct/indirect loc of dest/P2 of the bytecode to signify the VM needs to deref.
                    auto resultaddr = StackAddressForward(LocMemoryDirect, temp);
                    std::string value_type_name = value.type;
                    if (value_type.ref_type) {
                        // if the member is a ref, we need to deref the first to not return a double-ptr.
                        resultaddr = StackAddressForward(LocMemoryIndirect, temp);
                    }
                    else {
                        // even if the member is not a ref, we still return a ptr.
                        value_type_name = "ref " + value_type_name;
                    }
                    wip.add_bytecode(Opcode(Bytecode::refAdd, address, offsetaddr, resultaddr));

                    return {
                        value_type_name,
                        lhs.is_mutable && value.is_mutable,
                        lhs.assignable && value.is_mutable,
                        // always return an indirect for another to use
                        StackAddressForward(LocMemoryIndirect, temp),
                        sizeof(size_t),
                        lhs.stack_bytes_used + value_type.size
                    };
                }
                else {
                    address.offset += value.offset;

                    // For non-refs, we just return the value as-is.
                    // We don't wrap into a ref.
                    return {
                        value.type,
                        lhs.is_mutable && value.is_mutable,
                        lhs.assignable && value.is_mutable,
                        address,
                        lhs.stack_bytes_returned,
                        lhs.stack_bytes_used
                    };
                }
            }
        }
    }

    if (std::holds_alternative<Ast::Identifier>(access.member->data)) {
        std::string name = std::get<Ast::Identifier>(access.member->data).name;

        auto maybe_method = find_method({lhs.type}, name, wip);
        if (maybe_method) {
            return maybe_method.value();
        }
        if (lhstype.ref_type) {
            auto maybe_method = find_method({lhstype.ref_type.value()}, name, wip);
            if (maybe_method) {
                return maybe_method.value();
            }
        }
        std::cout << "Did not find " << name << "\n";
    }

    throw "Unknown member to access";
}

compiled_result reserve_local(std::string name, std::string type_name, bool is_mutable, compiler_wip& wip) {
    auto& loc = wip.current_scope->local_variables[wip.current_scope->local_variables.size() - 1];
    if (loc.find(name) != loc.end()) {
        // TODO: shadow probably would work fine without
        throw "Ident already declared";
    }
    auto type = wip.types.get_type(type_name);
    auto address = wip.next_stack;
    auto size = type.size;
    wip.next_stack += size;
    loc[name] = {
        name, type_name, is_mutable, address
    };
    DataLoc dloc = LocMemoryDirect;
    if (type.ref_type) {
        dloc = LocMemoryIndirect;
    }
    return {
        type_name,
        is_mutable,
        true,
        StackAddressForward(dloc, address),
        size,
        size
    };
}

compiled_result compile_vardecl(Ast::VariableDeclaration& decl, compiler_wip& wip) {
    return reserve_local(decl.name, decl.type, decl.is_mutable, wip);
}

compiled_result compile_unaryop(Ast::UnaryOperation& opnode, compiler_wip& wip, std::optional<BytecodeParam> suggested_return) {
    size_t stack = 0;
    size_t stack_start = wip.next_stack;

    size_t total_used = 0;

    auto value_ret = compile_node(opnode.value, wip, {});
    total_used = value_ret.stack_bytes_used;

    auto lhstypeinfo = get_type(value_ret.type, wip);
    if (lhstypeinfo.ref_type) {
        lhstypeinfo = get_type(lhstypeinfo.ref_type.value(), wip);
    }
    auto maybeOp = lhstypeinfo.unary_operators.find(opnode.op);
    if (maybeOp == lhstypeinfo.unary_operators.end()) {
        throw "Operator not supported by type";
    }
    auto op = maybeOp->second.method;
    auto optype = maybeOp->second.return_type;

    BytecodeParam ret;
    if (suggested_return) {
        ret = suggested_return.value();
    }
    else if (value_ret.stack_bytes_returned >= wip.types.get_type(optype).size) {
        // attempt to re-use any temporaries from the rhs
        ret = std::get<BytecodeParam>(value_ret.address);
        stack = value_ret.stack_bytes_returned;
    }
    else {
        size_t addr = wip.next_stack;
        size_t size = wip.types.get_type(optype).size;
        wip.next_stack += size;
        ret = StackAddressForward(LocMemoryDirect, addr);
        stack = value_ret.stack_bytes_returned + size;
        total_used += size;
    }

    if (std::holds_alternative<Types::TypeOperatorBytecode>(op)) {
        auto bc = std::get<Types::TypeOperatorBytecode>(op).bytecode;
        wip.add_bytecode(
            Opcode(bc, std::get<BytecodeParam>(value_ret.address), ret)
        );
    }
    else {
        throw "Calling operators is not supported yet";
    }
    // can free all unused stack for other ops now
    wip.next_stack = stack_start + stack;

    return {
        optype,
        // we generate a new value, so it could safely be mutable
        true,
        false,
        ret,
        stack,
        total_used
    };
}

compiled_result compile_shared_binop(std::shared_ptr<Ast::Node> lhs, std::shared_ptr<Ast::Node> rhs, Ast::BinaryOps operation, compiler_wip& wip, std::optional<BytecodeParam> suggested_return) {
    // TODO: reduce temporary/stack usage
    // I think I should pass down a "please store here"
    size_t stack = 0;
    size_t stack_start = wip.next_stack;

    size_t total_used = 0;

    auto rhs_ret = compile_node(rhs, wip, {});
    total_used = rhs_ret.stack_bytes_used;

    auto lhs_ret = compile_node(lhs, wip, {});
    if (total_used < lhs_ret.stack_bytes_used + stack) {
        // the "stack" quantity is already reserved.
        // if the rhs still happened to use more than that, then we
        // could re-use some temporaries.
        total_used = lhs_ret.stack_bytes_used + rhs_ret.stack_bytes_returned;
    }

    if (!compatible_types_by_name(lhs_ret.type, rhs_ret.type, wip)) {
        throw "Incompatible types";
    }

    auto lhstypeinfo = get_type(lhs_ret.type, wip);
    if (lhstypeinfo.ref_type) {
        lhstypeinfo = get_type(lhstypeinfo.ref_type.value(), wip);
    }
    auto maybeOp = lhstypeinfo.binary_operators.find(operation);
    if (maybeOp == lhstypeinfo.binary_operators.end()) {
        throw "Operator not supported by type";
    }
    auto op = maybeOp->second.method;
    auto optype = maybeOp->second.return_type;

    // TODO: there is one more re-use case I hadnt thought before:
    // we could reuse params / stack elements.
    // a lhs/rhs needs to know it is returning a stack element
    BytecodeParam ret;
    if (suggested_return) {
        ret = suggested_return.value();
    }
    else if (rhs_ret.stack_bytes_returned >= wip.types.get_type(optype).size) {
        // attempt to re-use any temporaries from the rhs
        ret = std::get<BytecodeParam>(rhs_ret.address);
        stack = rhs_ret.stack_bytes_returned;
    }
    else if (lhs_ret.stack_bytes_returned >= wip.types.get_type(optype).size) {
        ret = std::get<BytecodeParam>(lhs_ret.address);
        stack = lhs_ret.stack_bytes_returned;
    }
    else {
        // TODO: how to reduce...
        // need to create and return a new temporary to hold the returned value of the binop
        size_t addr = wip.next_stack;
        size_t size = wip.types.get_type(optype).size;
        wip.next_stack += size;
        ret = StackAddressForward(LocMemoryDirect, addr);
        stack = rhs_ret.stack_bytes_returned + size;
        total_used += size;
    }

    if (std::holds_alternative<Types::TypeOperatorBytecode>(op)) {
        auto bc = std::get<Types::TypeOperatorBytecode>(op).bytecode;
        wip.add_bytecode(
            Opcode(bc, std::get<BytecodeParam>(lhs_ret.address), std::get<BytecodeParam>(rhs_ret.address), ret)
        );
    }
    else {
        throw "Calling operators is not supported yet";
    }
    // can free all unused stack for other ops now
    wip.next_stack = stack_start + stack;

    return {
        optype,
        // we generate a new value, so it could safely be mutable
        true,
        false,
        ret,
        stack,
        total_used
    };
}

compiled_result compile_binop(Ast::BinaryOperation& opnode, compiler_wip& wip, std::optional<BytecodeParam> suggested_return) {
    return compile_shared_binop(opnode.lhs, opnode.rhs, opnode.op, wip, suggested_return);
}

compiled_result compile_setop(Ast::SetOperation& opnode, compiler_wip& wip) {
    compiled_result assign_value;
    size_t stack_begin = wip.next_stack;

    auto assign_to = compile_node(opnode.lhs, wip, {});
    if (!assign_to.assignable) {
        throw "lhs is not assignable";
    }
    BytecodeParam assign_address = std::get<BytecodeParam>(assign_to.address);

    if (opnode.op) {
        assign_value = compile_shared_binop(opnode.lhs, opnode.rhs, opnode.op.value(), wip, assign_address);
    }
    else {
        assign_value = compile_node(opnode.rhs, wip, assign_address);
    }
    if (assign_to.is_mutable && !assign_value.is_mutable) {
        throw "Unable to assign immutable value to a mutable variable";
    }

    BytecodeParam from_address = std::get<BytecodeParam>(assign_value.address);
    size_t total_used = assign_value.stack_bytes_used;

    auto optype = assign_value.type;
    auto optypeinfo = get_type(optype, wip);

    if (!compatible_types_by_name(assign_to.type, optype, wip)) {
        throw "Cannot set lhs to rhs as the types do not match";
    }
    if (total_used < assign_to.stack_bytes_used + assign_value.stack_bytes_returned) {
        total_used = assign_to.stack_bytes_used + assign_value.stack_bytes_returned;
    }

    if (from_address != assign_address) {
        auto opcode = assignment_opcode(optype, wip);
        wip.add_bytecode(
            Opcode(opcode, from_address, StackSize(LocMemoryDirect, optypeinfo.size), assign_address)
        );
    }
    // can free all stack since the result is stored
    wip.next_stack = stack_begin;

    return {
        optype,
        assign_to.is_mutable,
        false,
        assign_to.address,
        0, // this assigns to some variable, so nothing should be returned.
        total_used
    };
}

// TODO: finish with && and || shortcuts
compiled_result compile_testbinop(Ast::BinaryOperation& opnode, compiler_wip& wip, size_t else_label) {
    size_t stack = 0;
    size_t stack_start = wip.next_stack;

    size_t total_used = 0;

    auto rhs_ret = compile_node(opnode.rhs, wip, {});
    total_used = rhs_ret.stack_bytes_used;

    auto lhs_ret = compile_node(opnode.lhs, wip, {});
    if (total_used < lhs_ret.stack_bytes_used + stack) {
        // the "stack" quantity is already reserved.
        // if the rhs still happened to use more than that, then we
        // could re-use some temporaries.
        total_used = lhs_ret.stack_bytes_used + rhs_ret.stack_bytes_returned;
    }

    if (!compatible_types_by_name(lhs_ret.type, rhs_ret.type, wip)) {
        throw "Incompatible types";
    }

    auto lhstypeinfo = get_type(lhs_ret.type, wip);
    if (lhstypeinfo.ref_type) {
        lhstypeinfo = get_type(lhstypeinfo.ref_type.value(), wip);
    }
    auto optable = jump_opcode(lhstypeinfo.name);
    auto maybeOp = optable.find(opnode.op);
    if (maybeOp == optable.end()) {
        throw "Operator not supported by type";
    }

    wip.add_bytecode_linked_label(
        Opcode(maybeOp->second, std::get<BytecodeParam>(lhs_ret.address), std::get<BytecodeParam>(rhs_ret.address), BytecodeParam(0, 0)),
        labellink{else_label},
        2
    );

    // can free all unused stack for other ops now
    wip.next_stack = stack_start + stack;

    return {
        type_empty,
        false,
        false,
        BytecodeParam(0, 0),
        stack,
        total_used
    };
}


compiled_result compile_nodelist(std::vector<std::shared_ptr<Ast::Node>> nodes, compiler_wip& wip) {
    // TODO: support block-expressions
    size_t last_stack = wip.next_stack;
    size_t total_used = 0;
    for (auto& subnode : nodes) {
        compiled_result last = compile_node(subnode, wip, {});
        if (last.stack_bytes_used > total_used) {
            total_used = last.stack_bytes_used;
        }
    }

    // free the entire stack
    wip.next_stack = last_stack;
    return {
        type_empty,
        false,
        false,
        BytecodeParam(0, 0),
        0,
        total_used
    };
}

compiled_result compile_block(Ast::Block& block, compiler_wip& wip) {
    wip.current_scope->local_variables.push_back({});
    auto ret = compile_nodelist(block.nodes, wip);
    wip.current_scope->local_variables.pop_back();

    return ret;
}

compiled_result compile_if(Ast::IfStmt& stmt, compiler_wip& wip) {
    size_t stack_start = wip.next_stack;
    size_t else_label = wip.next_label++;
    size_t end_label = wip.next_label++;
    size_t max_used = 0;

    if (auto* cond = std::get_if<Ast::BinaryOperation>(&stmt.condition->data)) {
        compile_testbinop(*cond, wip, stmt.otherwise ? else_label : end_label);
    }
    else {
        auto condition = compile_node(stmt.condition, wip, {});
        if (condition.type != type_bool) {
            throw "If condition must be a boolean";
        }

        max_used = condition.stack_bytes_used;

        wip.add_bytecode_linked_label(
            Opcode(Bytecode::boolJFalse, std::get<BytecodeParam>(condition.address), BytecodeParam(0, 0)),
            stmt.otherwise ? labellink{else_label} : labellink{end_label},
            1
        );
    }

    // at this point, the condition is no longer needed
    wip.next_stack = stack_start;

    auto then = compile_node(stmt.then, wip, {});
    if (then.stack_bytes_used > max_used) {
        max_used = then.stack_bytes_used;
    }

    if (stmt.otherwise) {
        // need to jump Then to the end to skip Else
        wip.add_bytecode_linked_label(
            Opcode(Bytecode::Jump, BytecodeParam(0, 0)),
            labellink{end_label},
            0
        );

        // nothing from Then is needed.
        wip.next_stack = stack_start;

        wip.labels[else_label] = wip.bytecodes.size();
        auto otherwise = compile_node(stmt.otherwise.value(), wip, {});
        if (otherwise.stack_bytes_used > max_used) {
            max_used = otherwise.stack_bytes_used;
        }
    }
    wip.labels[end_label] = wip.bytecodes.size();

    // TODO: if expressions
    return {
        type_empty,
        false,
        false,
        BytecodeParam(0, 0),
        0,
        max_used
    };
}

compiled_result compile_dowhile(Ast::DoWhile& dowhile, compiler_wip& wip) {
    size_t stack_start = wip.next_stack;
    size_t start_label = wip.next_label++;
    wip.labels[start_label] = wip.bytecodes.size();
    size_t max_used = 0;

    auto value = compile_node(dowhile.block, wip, {});
    max_used = value.stack_bytes_used;

    auto condition = compile_node(dowhile.condition, wip, {});
    if (condition.type != type_bool) {
        throw "Dowhile condition must be a boolean";
    }
    if (max_used < (condition.stack_bytes_used + value.stack_bytes_returned)) {
        max_used = condition.stack_bytes_used + value.stack_bytes_returned;
    }

    wip.add_bytecode_linked_label(
        Opcode(Bytecode::boolJTrue, std::get<BytecodeParam>(condition.address), BytecodeParam(0, 0)),
        labellink{start_label},
        1
    );

    // free all stack used above
    wip.next_stack = stack_start;

    // TODO: figure out a return one day

    return {
        type_empty,
        false,
        false,
        BytecodeParam(0, 0),
        0,
        max_used
    };
}

compiled_result compile_globalblock(Ast::GlobalBlock& block, compiler_wip& wip) {
    return compile_nodelist(block.nodes, wip);
}

compiled_result compile_methoddecl(Ast::MethodDeclaration& method, compiler_wip& wip) {
    auto type = wip.types.get_type(method.type);
    auto typedata = std::get<Types::MethodType>(type.type);

    wip.current_scope->methods[method.name] = methodinfo{
        method.name,
        method.type,
        typedata.return_mutable,
        false,
        wip.next_method,
        0, 0, 0, 0
    };
    wip.next_method++;

    return {
        type_empty,
        false,
        false,
        BytecodeParam(0, 0),
        0,
        0
    };
}

compiled_result compile_methoddef(Ast::MethodDefinition& method, compiler_wip& wip) {
    auto maybemethod = get_method_named(method.scopes, method.name, wip);
    if (!maybemethod) {
        throw "Method was not declared.";
    }
    auto minfo = maybemethod.value();

    size_t start_stack = wip.next_stack;
    wip.next_stack = 0;
    wip.current_scope->local_variables.push_back({});

    size_t param_size = 0;
    auto type = wip.types.get_type(minfo->type);
    auto typedata = std::get<Types::MethodType>(type.type);

    if (typedata.parameters.size() != method.param_names.size()) {
        throw "Method definition parameters do not match declaration.";
    }

    for (size_t i = 0; i < typedata.parameters.size(); i++) {
        std::string ptype_name = typedata.parameters[i].type;
        auto& pt = wip.types.get_type(ptype_name);
        param_size += pt.size;
        reserve_local(method.param_names[i], ptype_name, typedata.parameters[i].is_mutable, wip);
    }

    // always reserve at least the ret type
    auto rettype = wip.types.get_type(typedata.return_type);
    if (param_size < rettype.size) {
        param_size = rettype.size;
    }
    wip.next_stack = param_size;

    size_t address = wip.bytecodes.size();

    auto r = compile_node(method.node, wip, {});

    if (wip.bytecodes[wip.bytecodes.size() - 1].opcode.op != Bytecode::Ret) {
        if (rettype.size == 0) {
            wip.add_bytecode(Opcode(Bytecode::Ret));
        }
        else {
            // TODO: need a better "exit" detector to make sure all paths exit
            throw "Must have a return from a method";
        }
    }

    minfo->defined = true;
    minfo->address = address;
    minfo->size = wip.bytecodes.size() - address;
    minfo->param_bytes = param_size;
    minfo->stack_bytes = r.stack_bytes_used;

    wip.next_stack = start_stack;
    wip.current_scope->local_variables.pop_back();

    return {
        type_empty,
        false,
        false,
        ScriptCall(LocMemoryDirect, address),
        0,
        0
    };
}

compiled_result compile_return(Ast::ReturnValue& ret, compiler_wip& wip) {
    size_t max_used = 0;

    if (ret.value) {
        BytecodeParam ret_address = StackAddressForward(LocMemoryDirect, 0);
        auto value = compile_node(ret.value.value(), wip, ret_address);
        max_used = value.stack_bytes_used;

        if (ret_address != std::get<BytecodeParam>(value.address)) {
            auto optypeinfo = get_type(value.type, wip);
            auto opcode = assignment_opcode(value.type, wip);
            wip.add_bytecode(
                Opcode(opcode, std::get<BytecodeParam>(value.address), StackSize(LocMemoryDirect, optypeinfo.size), StackAddressForward(LocMemoryDirect, 0))
            );
        }
    }
    wip.add_bytecode(
        Opcode(Bytecode::Ret)
    );
    return {
        type_empty,
        false,
        false,
        BytecodeParam(0, 0),
        0, // we assume this is reserved by the method.
        max_used
    };
}

compiled_result compile_methodparam(Ast::CallParam& param, Types::MethodTypeParameter type, compiler_wip& wip) {
    // but how do I make sure I append to the END of the stack.
    // if some other step makes the stack even larger, then these values are wrong.
    // call passes the new base ptr which is right at the start
    // it doesnt have to be at the end of the stack. we just have to be assured
    // that there is nothing after the params.

    size_t last_stack = wip.next_stack;
    auto store_address = StackAddressForward(LocMemoryDirect, wip.next_stack);
    auto typeinfo = wip.types.get_type(type.type);

    // TODO: suggesting positions would REALLY help here
    auto value = compile_node(param.value, wip, {});

    const auto& value_type = get_type(value.type, wip);
    const auto& param_type = get_type(type.type, wip);

    if (!compatible_types(value_type, param_type)) {
        throw "Parameter is the incorrect type";
    }
    if (!value.is_mutable && type.is_mutable) {
        throw "Unable to assign immutable value to a mutable parameter";
    }

    size_t total_used = value.stack_bytes_used;
    if (param_type.ref_type && !value_type.ref_type) {
        wip.add_bytecode(
            Opcode(Bytecode::DataAddress, std::get<BytecodeParam>(value.address), store_address)
        );
    }
    else if (!param_type.ref_type && value_type.ref_type) {
        auto ref = get_type(value_type.ref_type.value(), wip);
        wip.add_bytecode(
            Opcode(Bytecode::Dereference, std::get<BytecodeParam>(value.address), StackSize(LocMemoryDirect, ref.size), store_address)
        );
    }
    else if (std::get<BytecodeParam>(value.address) != store_address) {
        auto optypeinfo = get_type(value.type, wip);
        auto opcode = assignment_opcode(value.type, wip);
        wip.add_bytecode(
            Opcode(opcode, std::get<BytecodeParam>(value.address), StackSize(LocMemoryDirect, optypeinfo.size), store_address)
        );
        total_used += typeinfo.size;
    }

    // free any temporaries
    wip.next_stack = last_stack + typeinfo.size;

    return {
        type.type,
        type.is_mutable,
        false,
        store_address,
        typeinfo.size,
        total_used
    };
}

compiled_result compile_methodcall(Ast::MethodCall& method, compiler_wip& wip) {
    size_t max_used = 0;

    auto callable = compile_node(method.callable, wip, {});
    max_used = callable.stack_bytes_used;
    bool has_implicit = std::holds_alternative<Ast::AccessMember>(method.callable->data);
    size_t implicit_qty = has_implicit ? 1 : 0;

    auto methodtype = wip.types.get_type(callable.type);
    if (!std::holds_alternative<Types::MethodType>(methodtype.type)) {
        throw "LHS is not a function";
    }

    auto typeinfo = std::get<Types::MethodType>(methodtype.type);
    // TODO: default params
    if (typeinfo.parameters.size() != (method.params.size() + implicit_qty)) {
        std::cerr << "Supplied " << (method.params.size() + implicit_qty) << " for a call requiring " << typeinfo.parameters.size() << "\n";
        throw "Incorrect number of params";
    }
    auto returntype = wip.types.get_type(typeinfo.return_type);

    size_t base = wip.next_stack;
    size_t total_requied = callable.stack_bytes_used;
    if (has_implicit) {
        auto access = std::get<Ast::AccessMember>(method.callable->data);
        Ast::CallParam p0 = Ast::CallParam{ access.container };
        auto param = compile_methodparam(p0, typeinfo.parameters[0], wip);
        if (total_requied + param.stack_bytes_used > max_used) {
            max_used = total_requied + param.stack_bytes_used;
        }
        total_requied += param.stack_bytes_returned;
    }

    // TODO: named params and ordering
    for (size_t i = 0; i < method.params.size(); i++) {
        auto param = compile_methodparam(std::get<Ast::CallParam>(method.params[i]->data), typeinfo.parameters[i + implicit_qty], wip);
        if (total_requied + param.stack_bytes_used > max_used) {
            max_used = total_requied + param.stack_bytes_used;
        }
        total_requied += param.stack_bytes_returned;
    }

    BytecodeParam p1;
    if (std::holds_alternative<methodlink>(callable.address)) {
        auto l = std::get<methodlink>(callable.address);
        p1 = BytecodeParam(0, 0);
        wip.add_bytecode_linked_method(
            Opcode(Bytecode::Call, p1, StackAddressForward(LocMemoryDirect, base)),
            l,
            0
        );
    }
    else {
        p1 = std::get<BytecodeParam>(callable.address);
        wip.add_bytecode(
            Opcode(Bytecode::Call, p1, StackAddressForward(LocMemoryDirect, base))
        );
    }

    // free all the params
    wip.next_stack = base + returntype.size;
    // TODO: returning assignables? I think refs make it work anyway.
    return {
        typeinfo.return_type,
        typeinfo.return_mutable,
        false,
        StackAddressForward(LocMemoryDirect, base),
        returntype.size,
        max_used
    };
}

//compiled_result compile_(Ast::blah& node, compiler_wip& wip) {
//}

compiled_result compile_node(std::shared_ptr<Ast::Node> n, compiler_wip& wip, std::optional<BytecodeParam> suggested_return) {
    if (auto* v = std::get_if<Ast::ConstS32>(&n->data)) {
        return compile_const_s32(*v, wip);
    }
    else if (auto* v = std::get_if<Ast::ConstF32>(&n->data)) {
        return compile_const_f32(*v, wip);
    }
    else if (auto* v = std::get_if<Ast::ConstBool>(&n->data)) {
        return compile_const_bool(*v, wip);
    }
    else if (auto* v = std::get_if<Ast::UnaryOperation>(&n->data)) {
        return compile_unaryop(*v, wip, suggested_return);
    }
    else if (auto* v = std::get_if<Ast::BinaryOperation>(&n->data)) {
        return compile_binop(*v, wip, suggested_return);
    }
    else if (auto* v = std::get_if<Ast::SetOperation>(&n->data)) {
        return compile_setop(*v, wip);
    }
    else if (auto* v = std::get_if<Ast::Identifier>(&n->data)) {
        return compile_identifier(*v, wip);
    }
    else if (auto* v = std::get_if<Ast::AccessMember>(&n->data)) {
        return compile_memberaccess(*v, wip);
    }
    else if (auto* v = std::get_if<Ast::VariableDeclaration>(&n->data)) {
        return compile_vardecl(*v, wip);
    }
    else if (auto* v = std::get_if<Ast::Block>(&n->data)) {
        return compile_block(*v, wip);
    }
    else if (auto* v = std::get_if<Ast::GlobalBlock>(&n->data)) {
        return compile_globalblock(*v, wip);
    }
    else if (auto* v = std::get_if<Ast::IfStmt>(&n->data)) {
        return compile_if(*v, wip);
    }
    else if (auto* v = std::get_if<Ast::DoWhile>(&n->data)) {
        return compile_dowhile(*v, wip);
    }
    else if (auto* v = std::get_if<Ast::MethodDeclaration>(&n->data)) {
        return compile_methoddecl(*v, wip);
    }
    else if (auto* v = std::get_if<Ast::MethodDefinition>(&n->data)) {
        return compile_methoddef(*v, wip);
    }
    else if (auto* v = std::get_if<Ast::ReturnValue>(&n->data)) {
        return compile_return(*v, wip);
    }
    else if (auto* v = std::get_if<Ast::MethodCall>(&n->data)) {
        return compile_methodcall(*v, wip);
    }

    throw "Invalid node";
}

void
generate_bytecode(std::shared_ptr<Ast::Node> ast_root, compiler_wip& wip) {
    compile_node(ast_root, wip, {});
}

BytecodeParam
_jump_address(size_t jump_to, size_t jump_from) {
    if (jump_from > jump_to) {
        size_t diff = jump_from - jump_to;
        return JumpOffsetBackward(LocMemoryDirect, diff + 1);
    }
    else {
        size_t diff = jump_to - jump_from;
        return JumpOffsetForward(LocMemoryDirect, diff - 1);
    }
}

BytecodeParam
_call_address(size_t jump_to, size_t jump_from) {
    return JumpExact(LocMemoryDirect, jump_to);
}

bool
_farcall_required(size_t jump_to, size_t jump_from) {
    if (jump_to > 0xFFFF) {
        return true;
    }

    size_t diff = 0;
    if (jump_from > jump_to) {
        diff = jump_from - jump_to;
    }
    else {
        diff = jump_to - jump_from;
    }
    return diff > 0xFFFF;
}

void
link(compiler_wip& wip) {
    for (size_t i = 0; i < wip.bytecodes.size(); i++) {
        auto& op = wip.bytecodes[i];

        if (op.method_link) {
            auto ml = op.method_link.value();
            auto& bc = op.opcode;
            auto maybe_method = get_method_named(ml.method.scopes, ml.method.name, wip);
            if (!maybe_method || !maybe_method.value()->defined) {
                throw "Method never defined";
            }
            auto method = maybe_method.value();

            BytecodeParam linked;
            if (_farcall_required(method->address, i)) {
                linked = ScriptCall(LocMemoryDirect, method->index);
            }
            else {
                bc.op = Bytecode::FCall;
                linked = _call_address(method->address, i);
            }

            bc.set_parameter1(linked);
            // We don't touch the base (p2)
            bc.set_parameter3(StackSize(LocMemoryDirect, method->stack_bytes));
        }
        if (op.label_link) {
            auto ll = op.label_link.value();
            auto& bc = op.opcode;
            size_t link_address = wip.labels.at(ll.label.labelid);

            BytecodeParam linked = _jump_address(link_address, i);
            switch (ll.param) {
            case 0:
                bc.set_parameter1(linked);
                break;
            case 1:
                bc.set_parameter2(linked);
                break;
            case 2:
                bc.set_parameter3(linked);
                break;
            }
        }
    }
}

void
print_program(compiler_wip& wip) {
    std::cout << "Bytecodes:\n";

    size_t i = 0;
    for (auto& bc : wip.bytecodes) {
        std::cout << i << ": " << (unsigned int)bc.opcode.op << " ";
        std::cout << address_page(bc.opcode.p1) << ":" << address_offset(bc.opcode.p1) << " ";
        std::cout << address_page(bc.opcode.p2) << ":" << address_offset(bc.opcode.p2) << " ";
        std::cout << address_page(bc.opcode.p3) << ":" << address_offset(bc.opcode.p3) << "\n";
        i++;
    }
}

std::optional<std::type_index>
get_cpp_type(const Types::TypeInfo& type) {
    if (std::holds_alternative<Types::PrimitiveType>(type.type)) {
        auto prim = std::get<Types::PrimitiveType>(type.type);
        switch (prim) {
        case Types::PrimitiveType::boolean:
            return typeid(bool);
        case Types::PrimitiveType::empty:
            return typeid(void);
        case Types::PrimitiveType::s32:
            return typeid(int);
        case Types::PrimitiveType::f32:
            return typeid(float);
        }
    }
    if (type.backing_type) {
        return type.backing_type.value();
    }
    return {};
}

void
add_scope_to_program(std::shared_ptr<Program> p, compilerscope& scope, compiler_wip& wip) {
    for (auto& m : scope.local_variables[0]) {
        auto t = wip.types.get_type(m.second.type);
        p->add_global_index(m.first, t.size, m.second.address);
    }

    for (auto& it : scope.methods) {
        auto& m = it.second;
        p->add_method_addr(m.index, m.param_bytes, m.stack_bytes, m.address);
        
        auto& t = std::get<Types::MethodType>(wip.types.get_type(m.type).type);
        std::vector<std::type_index> types;
        bool has_all = true;
        auto cpp_type = get_cpp_type(wip.types.get_type(t.return_type));
        // methods are not registered for C++ if any type is not backed by a C++ type.
        if (!cpp_type) {
            continue;
        }
        types.push_back(cpp_type.value());
        for (auto& p : t.parameters) {
            cpp_type = get_cpp_type(wip.types.get_type(p.type));
            if (!cpp_type) {
                has_all = false;
                break;
            }
            types.push_back(cpp_type.value());
        }
        if (has_all) {
            p->register_method(m.name, m.address, types);
        }
    }

    for (auto& it : scope.subscopes) {
        add_scope_to_program(p, it.second, wip);
    }
}

std::shared_ptr<Program>
generate_bytecode(std::shared_ptr<Ast::Node> ast_root, const Types::TypeTable& types, const ImportedMethods& imported_methods) {
    compiler_wip wip(types, imported_methods);
    generate_bytecode(ast_root, wip);
    link(wip);
    print_program(wip);

    auto p = std::make_shared<Program>(wip.next_const);

    for (auto& it : imported_methods) {
        p->add_builtin(it.name, it.runnable);
    }

    for (auto& v : wip.constant_values) {
        if (std::holds_alternative<int>(v)) {
            p->add_constant<int>("", std::get<int>(v));
        }
        else if (std::holds_alternative<float>(v)) {
            p->add_constant<float>("", std::get<float>(v));
        }
        else if (std::holds_alternative<size_t>(v)) {
            p->add_constant<size_t>("", std::get<size_t>(v));
        }
        else {
            throw "Unknown constant type";
        }
    }
    std::vector<Opcode> bytecodes;
    bytecodes.reserve(wip.bytecodes.size());
    for (auto& b : wip.bytecodes) {
        bytecodes.push_back(b.opcode);
    }
    p->add_code(bytecodes);

    add_scope_to_program(p, wip.rootscope, wip);

    return p;
}

}
}
