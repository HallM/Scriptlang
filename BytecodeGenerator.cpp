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

// TODO: get rid of these or move them to somewhere central
const std::string type_empty = "void";
const std::string type_bool = "bool";
const std::string type_s32 = "s32";
const std::string type_f32 = "f32";

struct operinfo {
    Bytecode bc;
    std::string type_returned;
};

std::unordered_map<BinaryOps, operinfo> binary_opcode(std::string type) {
    if (type == type_s32) {
        return {
            {BinaryOps::Add, {Bytecode::s32Add, type_s32}},
            {BinaryOps::Subtract, {Bytecode::s32Sub, type_s32}},
            {BinaryOps::Multiply, {Bytecode::s32Mul, type_s32}},
            {BinaryOps::Divide, {Bytecode::s32Div, type_s32}},
            {BinaryOps::Modulo, {Bytecode::s32Mod, type_s32}},
            {BinaryOps::Eq, {Bytecode::s32Equal, type_bool}},
            {BinaryOps::NotEq, {Bytecode::s32NotEqual, type_bool}},
            {BinaryOps::Less, {Bytecode::s32Less, type_bool}},
            {BinaryOps::LessEqual, {Bytecode::s32LessEqual, type_bool}},
            {BinaryOps::Greater, {Bytecode::s32Greater, type_bool}},
            {BinaryOps::GreaterEqual, {Bytecode::s32GreaterEqual, type_bool}},
            {BinaryOps::BitShiftLeft, {Bytecode::s32ShiftLeft, type_s32}},
            {BinaryOps::BitShiftRight, {Bytecode::s32ShiftRight, type_s32}},
            {BinaryOps::BitAnd, {Bytecode::s32BitAnd, type_s32}},
            {BinaryOps::BitOr, {Bytecode::s32BitOr, type_s32}},
            {BinaryOps::BitXor, {Bytecode::s32BitXor, type_s32}},
        };
    } else if (type == type_f32) {
        return {
            {BinaryOps::Add, {Bytecode::f32Add, type_f32}},
            {BinaryOps::Subtract, {Bytecode::f32Sub, type_f32}},
            {BinaryOps::Multiply, {Bytecode::f32Mul, type_f32}},
            {BinaryOps::Divide, {Bytecode::f32Div, type_f32}},
            {BinaryOps::Modulo, {Bytecode::f32Mod, type_f32}},
            {BinaryOps::Eq, {Bytecode::f32Equal, type_bool}},
            {BinaryOps::NotEq, {Bytecode::f32NotEqual, type_bool}},
            {BinaryOps::Less, {Bytecode::f32Less, type_bool}},
            {BinaryOps::LessEqual, {Bytecode::f32LessEqual, type_bool}},
            {BinaryOps::Greater, {Bytecode::f32Greater, type_bool}},
            {BinaryOps::GreaterEqual, {Bytecode::f32GreaterEqual, type_bool}},
        };
    } else if (type == type_bool) {
        return {
            {BinaryOps::And, {Bytecode::bAnd, type_bool}},
            {BinaryOps::Or, {Bytecode::bOr, type_bool}},
            {BinaryOps::Eq, {Bytecode::bEqual, type_bool}},
            {BinaryOps::NotEq, {Bytecode::bNotEqual, type_bool}},
        };
    }
    return {};
}

std::unordered_map<UnaryOps, operinfo> unary_opcode(std::string type) {
    if (type == type_s32) {
        return {
            {UnaryOps::Negate, {Bytecode::s32Negate, type_s32}},
            {UnaryOps::BitNot, {Bytecode::s32BitNot, type_s32}},
        };
    } else if (type == type_f32) {
        return {
            {UnaryOps::Negate, {Bytecode::f32Negate, type_f32}},
        };
    } else if (type == type_bool) {
        return {
            {UnaryOps::Not, {Bytecode::bNot, type_bool}},
        };
    }
    return {};
}

struct labellink {
    size_t labelid;
};
struct methodlink {
    std::string name;
};

struct compiled_result {
    std::string type;
    bool assignable;
    // the current address information for the resulting data of this expression.
    std::variant<BytecodeParam, labellink, methodlink> address;
    // number of bytes of stack that are held onto (returned)
    size_t stack_bytes_returned;
    // the total stack used for reservations
    size_t stack_bytes_used;
};

struct labellinkable {
    // the index in the bytecode table to be linked
    size_t index;
    // the param (0-2) that needs to be linked
    size_t param;
    // the label to look up
    size_t label;
};
struct methodlinkable {
    // the index in the bytecode table to be linked
    size_t index;
    // the param (0-2) that needs to be linked
    size_t param;
    // the label to look up
    std::string method_name;
};

struct variableinfo {
    std::string name;
    std::string type;
    size_t address;
};

struct methodinfo {
    std::string name;
    std::string type;
    bool defined;
    size_t address;
    size_t param_bytes;
    size_t stack_bytes;
};

// holds all the necessary tables as we move through the compilation step
struct compiler_wip {
    const TypeTable& types;
    const ImportedMethods& imported_methods;
    std::unordered_map<std::string, size_t> imported_method_names;

    std::vector<Opcode> bytecodes;
    std::vector<labellinkable> labellinks;
    std::vector<methodlinkable> methodlinks;
    std::unordered_map<size_t, size_t> labels;
    size_t next_label;

    size_t next_stack;

    // TODO: how to have starting values for globals
    std::vector<std::unordered_map<std::string, variableinfo>> local_variables;

    std::vector<std::variant<
        float,
        int,
        size_t
    >> constant_values;
    std::unordered_map<std::string, size_t> constant_addresses;
    size_t next_const;

    std::vector<methodinfo> methods;
    std::unordered_map<std::string, size_t> method_indices;

    public:
    compiler_wip(const TypeTable& t, const ImportedMethods& m) : types(t), imported_methods(m), next_label(0), next_stack(0), next_const(0) {
        for (size_t i = 0; i < m.size(); i++) {
            ImportedMethod method = m[i];
            std::cout << method.name << " imported\n";
            imported_method_names[method.name] = i;
        }
    }
};

bool compatible_types(const TypeInfo& a, const TypeInfo& b) {
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

const TypeInfo& get_type(std::string type_name, compiler_wip& wip) {
    return wip.types.get_type(type_name);
}

bool compatible_types_by_name(std::string a, std::string b, compiler_wip& wip) {
    return compatible_types(get_type(a, wip), get_type(b, wip));
}

operinfo assignment_opcode(std::string type_name, compiler_wip& wip) {
    auto type = get_type(type_name, wip);
    if (type.ref_type) {
        return {Bytecode::refSet, type_bool};
    }
    if (std::holds_alternative<PrimitiveType>(type.type)) {
        auto prim = std::get<PrimitiveType>(type.type);
        if (prim == PrimitiveType::s32) {
            return {Bytecode::s32Set, type_bool};
        }
        else if (prim == PrimitiveType::f32) {
            return {Bytecode::f32Set, type_bool};
        }
        else if (prim == PrimitiveType::boolean) {
            return {Bytecode::s32Set, type_bool};
        }
    }

    throw "unknown assignment";
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

methodinfo&
get_method_named(std::string name, compiler_wip& wip) {
    size_t index = wip.method_indices[name];
    return wip.methods[index];
}

compiled_result compile_node(std::shared_ptr<Node> n, compiler_wip& wip, std::optional<BytecodeParam> suggested_return);

compiled_result compile_const_s32(std::shared_ptr<Node> n, compiler_wip& wip) {
    auto s32node = std::get<ConstS32>(n->data);
    size_t address = constant<int>(s32node.num, wip);
    return {
        type_s32,
        false,
        ConstantAddress(LocMemoryDirect, address),
        0,
        0
    };
}

compiled_result compile_const_f32(std::shared_ptr<Node> n, compiler_wip& wip) {
    auto f32node = std::get<ConstF32>(n->data);
    size_t address = constant<float>(f32node.num, wip);
    return {
        type_f32,
        false,
        ConstantAddress(LocMemoryDirect, address),
        0,
        0
    };
}

compiled_result compile_const_bool(std::shared_ptr<Node> n, compiler_wip& wip) {
    auto bnode = std::get<ConstBool>(n->data);
    size_t address = constant<bool>(bnode.value, wip);
    return {
        type_bool,
        false,
        ConstantAddress(LocMemoryDirect, address),
        0,
        0
    };
}

compiled_result compile_identifier(std::shared_ptr<Node> n, compiler_wip& wip) {
    auto ident = std::get<Identifier>(n->data).name;

    for (size_t i = wip.local_variables.size(); i--;) {
        auto lv = wip.local_variables[i];
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
                true,
                ret,
                0,
                0
            };
        }
    }

    auto maybe_method = wip.method_indices.find(ident);
    if (maybe_method != wip.method_indices.end()) {
        // Note that method references would be handled above.
        return {
            wip.methods.at(maybe_method->second).type,
            false,
            methodlink{ident},
            0,
            0
        };
    }

    auto maybe_imported = wip.imported_method_names.find(ident);
    if (maybe_imported != wip.imported_method_names.end()) {
        auto method = wip.imported_methods[maybe_imported->second];
        return {
            method.type,
            true,
            ExternalCall(LocMemoryDirect, maybe_imported->second),
            0,
            0
        };
    }

    std::cout << ident << " not found\n";
    throw "Identifier not found";
}

compiled_result compile_access(std::shared_ptr<Node> n, compiler_wip& wip) {
    AccessMember access = std::get<AccessMember>(n->data);

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
    if (std::holds_alternative<StructType>(lhstype.type)) {
        auto structinfo = std::get<StructType>(lhstype.type);
        if (std::holds_alternative<Identifier>(access.member->data)) {
            std::string name = std::get<Identifier>(access.member->data).name;
            auto value = structinfo.members.at(name);
            auto value_type = get_type(value.type, wip);

            if (lhstype.ref_type) {
                // TODO: could we save stack by re-using space here?
                // we create a new ptr to the value by doing a sizet add.
                size_t temp = wip.next_stack;
                wip.next_stack += sizeof(size_t);
                auto offsetaddr = ConstantAddress(LocMemoryDirect, constant<size_t>(value.offset, wip));
                auto tempaddr = StackAddressForward(LocMemoryIndirect, temp);
                wip.bytecodes.push_back(Opcode(Bytecode::refAdd, address, offsetaddr, tempaddr));
                return {
                    value.type,
                    true,
                    tempaddr,
                    sizeof(size_t),
                    lhs.stack_bytes_used + value_type.size
                };
            }
            else {
                address.offset += value.offset;

                return {
                    value.type,
                    true,
                    address,
                    lhs.stack_bytes_returned,
                    lhs.stack_bytes_used
                };
            }
        }
    }

    throw "Unknown member to access";
}

compiled_result reserve_local(std::string name, std::string type_name, compiler_wip& wip) {
    auto& loc = wip.local_variables[wip.local_variables.size() - 1];
    if (loc.find(name) != loc.end()) {
        // TODO: shadow probably would work fine without
        throw "Ident already declared";
    }
    auto type = wip.types.get_type(type_name);
    auto address = wip.next_stack;
    auto size = type.size;
    wip.next_stack += size;
    loc[name] = {
        name, type_name, address
    };
    return {
        type_empty,
        false,
        BytecodeParam(0, 0),
        size,
        size
    };
}

compiled_result compile_vardecl(std::shared_ptr<Node> n, compiler_wip& wip) {
    auto decl = std::get<VariableDeclaration>(n->data);
    return reserve_local(decl.name, decl.type, wip);
}

compiled_result compile_unaryop(std::shared_ptr<Node> n, compiler_wip& wip, std::optional<BytecodeParam> suggested_return) {
    auto opnode = std::get<UnaryOperation>(n->data);
    size_t stack = 0;
    size_t stack_start = wip.next_stack;

    size_t total_used = 0;

    auto value_ret = compile_node(opnode.value, wip, {});
    total_used = value_ret.stack_bytes_used;

    auto optable = unary_opcode(value_ret.type);
    auto maybeOp = optable.find(opnode.op);
    if (maybeOp == optable.end()) {
        throw "Unary Operator not supported by type";
    }
    auto op = maybeOp->second;
    auto optype = maybeOp->second.type_returned;

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

    wip.bytecodes.push_back(
        Opcode(op.bc, std::get<BytecodeParam>(value_ret.address), ret)
    );
    // can free all unused stack for other ops now
    wip.next_stack = stack_start + stack;

    return {
        optype,
        false,
        ret,
        stack,
        total_used
    };
}

compiled_result compile_shared_binop(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs, BinaryOps operation, compiler_wip& wip, std::optional<BytecodeParam> suggested_return) {
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

    auto optable = binary_opcode(lhs_ret.type);
    auto maybeOp = optable.find(operation);
    if (maybeOp == optable.end()) {
        throw "Operator not supported by type";
    }
    auto op = maybeOp->second;
    auto optype = maybeOp->second.type_returned;

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

    wip.bytecodes.push_back(
        Opcode(op.bc, std::get<BytecodeParam>(lhs_ret.address), std::get<BytecodeParam>(rhs_ret.address), ret)
    );
    // can free all unused stack for other ops now
    wip.next_stack = stack_start + stack;

    return {
        optype,
        false,
        ret,
        stack,
        total_used
    };
}

compiled_result compile_binop(std::shared_ptr<Node> n, compiler_wip& wip, std::optional<BytecodeParam> suggested_return) {
    auto opnode = std::get<BinaryOperation>(n->data);
    return compile_shared_binop(opnode.lhs, opnode.rhs, opnode.op, wip, suggested_return);
}

compiled_result compile_setop(std::shared_ptr<Node> n, compiler_wip& wip) {
    auto opnode = std::get<SetOperation>(n->data);
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
    BytecodeParam from_address = std::get<BytecodeParam>(assign_value.address);
    size_t total_used = assign_value.stack_bytes_used;

    auto optype = assign_value.type;

    if (!compatible_types_by_name(assign_to.type, optype, wip)) {
        throw "Cannot set lhs to rhs as the types do not match";
    }
    if (total_used < assign_to.stack_bytes_used + assign_value.stack_bytes_returned) {
        total_used = assign_to.stack_bytes_used + assign_value.stack_bytes_returned;
    }

    if (from_address != assign_address) {
        auto opinfo = assignment_opcode(optype, wip);
        wip.bytecodes.push_back(
            Opcode(opinfo.bc, from_address, assign_address)
        );
    }
    // can free all stack since the result is stored
    wip.next_stack = stack_begin;

    return {
        optype,
        false,
        assign_to.address,
        0, // this assigns to some variable, so nothing should be returned.
        total_used
    };
}

compiled_result compile_nodelist(std::vector<std::shared_ptr<Node>> nodes, compiler_wip& wip) {
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
        BytecodeParam(0, 0),
        0,
        total_used
    };
}

compiled_result compile_block(std::shared_ptr<Node> n, compiler_wip& wip) {
    auto block = std::get<Block>(n->data);

    wip.local_variables.push_back({});
    auto ret = compile_nodelist(block.nodes, wip);
    wip.local_variables.pop_back();

    return ret;
}

compiled_result compile_if(std::shared_ptr<Node> n, compiler_wip& wip) {
    auto stmt = std::get<IfStmt>(n->data);

    size_t stack_start = wip.next_stack;
    size_t else_label = wip.next_label++;
    size_t end_label = wip.next_label++;
    size_t max_used = 0;

    auto condition = compile_node(stmt.condition, wip, {});
    if (condition.type != type_bool) {
        throw "If condition must be a boolean";
    }

    max_used = condition.stack_bytes_used;

    wip.labellinks.push_back(labellinkable{
        wip.bytecodes.size(),
        1,
        stmt.otherwise ? else_label : end_label
    });
    wip.bytecodes.push_back(
        Opcode(Bytecode::bJFalse, std::get<BytecodeParam>(condition.address), BytecodeParam(0, 0))
    );

    // at this point, the condition is no longer needed
    wip.next_stack = stack_start;

    auto then = compile_node(stmt.then, wip, {});
    if (then.stack_bytes_used > max_used) {
        max_used = then.stack_bytes_used;
    }

    if (stmt.otherwise) {
        // need to jump Then to the end to skip Else
        wip.labellinks.push_back(labellinkable{
            wip.bytecodes.size(),
            0,
            end_label
        });
        wip.bytecodes.push_back(
            Opcode(Bytecode::Jump, BytecodeParam(0, 0))
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
        BytecodeParam(0, 0),
        0,
        max_used
    };
}

compiled_result compile_dowhile(std::shared_ptr<Node> n, compiler_wip& wip) {
    auto dowhile = std::get<DoWhile>(n->data);

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

    wip.labellinks.push_back(labellinkable{
        wip.bytecodes.size(),
        1,
        start_label
    });
    wip.bytecodes.push_back(
        Opcode(Bytecode::bJTrue, std::get<BytecodeParam>(condition.address), BytecodeParam(0, 0))
    );

    // free all stack used above
    wip.next_stack = stack_start;

    // TODO: figure out a return one day

    return {
        type_empty,
        false,
        BytecodeParam(0, 0),
        0,
        max_used
    };
}

compiled_result compile_globalblock(std::shared_ptr<Node> n, compiler_wip& wip) {
    auto block = std::get<GlobalBlock>(n->data);
    return compile_nodelist(block.nodes, wip);
}

compiled_result compile_methoddecl(std::shared_ptr<Node> n, compiler_wip& wip) {
    auto method = std::get<MethodDeclaration>(n->data);
    std::cout << "Added method " << method.name << "\n";

    wip.method_indices[method.name] = wip.methods.size();
    wip.methods.push_back(methodinfo{
        method.name,
        method.type,
        false
    });

    return {
        type_empty,
        false,
        BytecodeParam(0, 0),
        0,
        0
    };
}

compiled_result compile_methoddef(std::shared_ptr<Node> n, compiler_wip& wip) {
    auto method = std::get<MethodDefinition>(n->data);

    auto& methodinfo = get_method_named(method.name, wip);

    size_t start_stack = wip.next_stack;
    wip.next_stack = 0;
    wip.local_variables.push_back({});

    size_t param_size = 0;
    auto type = wip.types.get_type(methodinfo.type);
    auto typedata = std::get<MethodType>(type.type);

    if (typedata.parameters.size() != method.param_names.size()) {
        throw "Method definition parameters do not match declaration.";
    }

    for (size_t i = 0; i < typedata.parameters.size(); i++) {
        std::string ptype_name = typedata.parameters[i].type;
        auto& pt = wip.types.get_type(ptype_name);
        param_size += pt.size;
        reserve_local(method.param_names[i], ptype_name, wip);
    }

    // always reserve at least the ret type
    auto rettype = wip.types.get_type(typedata.return_type);
    if (param_size < rettype.size) {
        param_size = rettype.size;
    }
    wip.next_stack = param_size;

    size_t address = wip.bytecodes.size();

    auto r = compile_node(method.node, wip, {});

    if (wip.bytecodes[wip.bytecodes.size() - 1].op != Bytecode::Ret) {
        if (rettype.size == 0) {
            wip.bytecodes.push_back(Opcode(Bytecode::Ret));
        }
        else {
            // TODO: need a better "exit" detector to make sure all paths exit
            throw "Must have a return from a method";
        }
    }

    methodinfo.defined = true;
    methodinfo.address = address;
    methodinfo.param_bytes = param_size;
    methodinfo.stack_bytes = r.stack_bytes_used;

    wip.next_stack = start_stack;
    wip.local_variables.pop_back();

    return {
        type_empty,
        false,
        ScriptCall(LocMemoryDirect, address),
        0,
        0
    };
}

compiled_result compile_return(std::shared_ptr<Node> n, compiler_wip& wip) {
    auto ret = std::get<ReturnValue>(n->data);
    size_t max_used = 0;

    if (ret.value) {
        BytecodeParam ret_address = StackAddressForward(LocMemoryDirect, 0);
        auto value = compile_node(ret.value.value(), wip, ret_address);
        max_used = value.stack_bytes_used;

        if (ret_address != std::get<BytecodeParam>(value.address)) {
            auto opinfo = assignment_opcode(value.type, wip);
            wip.bytecodes.push_back(
                Opcode(opinfo.bc, std::get<BytecodeParam>(value.address), StackAddressForward(LocMemoryDirect, 0))
            );
        }
    }
    wip.bytecodes.push_back(
        Opcode(Bytecode::Ret)
    );
    return {
        type_empty,
        false,
        BytecodeParam(0, 0),
        0, // we assume this is reserved by the method.
        max_used
    };
}

compiled_result compile_methodparam(std::shared_ptr<Node> n, MethodTypeParameter type, compiler_wip& wip) {
    auto param = std::get<CallParam>(n->data);
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

    size_t total_used = value.stack_bytes_used;
    if (param_type.ref_type && !value_type.ref_type) {
        wip.bytecodes.push_back(
            Opcode(Bytecode::DataAddress, std::get<BytecodeParam>(value.address), store_address)
        );
    }
    else if (!param_type.ref_type && value_type.ref_type) {
        wip.bytecodes.push_back(
            Opcode(Bytecode::Dereference, std::get<BytecodeParam>(value.address), store_address)
        );
    }
    else if (std::get<BytecodeParam>(value.address) != store_address) {
        auto opinfo = assignment_opcode(value.type, wip);
        wip.bytecodes.push_back(
            Opcode(opinfo.bc, std::get<BytecodeParam>(value.address), store_address)
        );
        total_used += typeinfo.size;
    }

    // free any temporaries
    wip.next_stack = last_stack + typeinfo.size;

    return {
        type.type,
        false,
        store_address,
        typeinfo.size,
        total_used
    };
}

compiled_result compile_methodcall(std::shared_ptr<Node> n, compiler_wip& wip) {
    auto method = std::get<MethodCall>(n->data);

    size_t max_used = 0;

    auto callable = compile_node(method.callable, wip, {});
    max_used = callable.stack_bytes_used;

    auto methodtype = wip.types.get_type(callable.type);
    if (!std::holds_alternative<MethodType>(methodtype.type)) {
        throw "LHS is not a function";
    }

    auto typeinfo = std::get<MethodType>(methodtype.type);
    // TODO: default params
    if (typeinfo.parameters.size() != method.params.size()) {
        throw "Incorrect number of params";
    }
    auto returntype = wip.types.get_type(typeinfo.return_type);

    size_t base = wip.next_stack;
    size_t total_requied = callable.stack_bytes_used;

    // TODO: named params and ordering
    for (size_t i = 0; i < method.params.size(); i++) {
        auto param = compile_methodparam(method.params[i], typeinfo.parameters[i], wip);
        if (total_requied + param.stack_bytes_used > max_used) {
            max_used = total_requied + param.stack_bytes_used;
        }
        total_requied += param.stack_bytes_returned;
    }

    BytecodeParam p1;
    if (std::holds_alternative<methodlink>(callable.address)) {
        auto l = std::get<methodlink>(callable.address);
        p1 = BytecodeParam(0, 0);
        wip.methodlinks.push_back({
            wip.bytecodes.size(),
            0,
            l.name
        });
    }
    else {
        p1 = std::get<BytecodeParam>(callable.address);
    }

    std::cout << "add call " << wip.bytecodes.size() << "\n";
    wip.bytecodes.push_back(
        Opcode(Bytecode::Call, p1, StackAddressForward(LocMemoryDirect, base))
    );

    // free all the params
    wip.next_stack = base + returntype.size;
    // TODO: returning assignables? I think refs make it work anyway.
    return {
        typeinfo.return_type,
        false,
        StackAddressForward(LocMemoryDirect, base),
        returntype.size,
        max_used
    };
}

//compiled_result compile_(std::shared_ptr<Node> n, compiler_wip& wip) {
//}

compiled_result compile_node(std::shared_ptr<Node> n, compiler_wip& wip, std::optional<BytecodeParam> suggested_return) {
    if (std::holds_alternative<ConstS32>(n->data)) {
        return compile_const_s32(n, wip);
    }
    else if (std::holds_alternative<ConstF32>(n->data)) {
        return compile_const_f32(n, wip);
    }
    else if (std::holds_alternative<ConstBool>(n->data)) {
        return compile_const_bool(n, wip);
    }
    else if (std::holds_alternative<UnaryOperation>(n->data)) {
        return compile_unaryop(n, wip, suggested_return);
    }
    else if (std::holds_alternative<BinaryOperation>(n->data)) {
        return compile_binop(n, wip, suggested_return);
    }
    else if (std::holds_alternative<SetOperation>(n->data)) {
        return compile_setop(n, wip);
    }
    else if (std::holds_alternative<Identifier>(n->data)) {
        return compile_identifier(n, wip);
    }
    else if (std::holds_alternative<AccessMember>(n->data)) {
        return compile_access(n, wip);
    }
    else if (std::holds_alternative<VariableDeclaration>(n->data)) {
        return compile_vardecl(n, wip);
    }
    else if (std::holds_alternative<Block>(n->data)) {
        return compile_block(n, wip);
    }
    else if (std::holds_alternative<GlobalBlock>(n->data)) {
        return compile_globalblock(n, wip);
    }
    else if (std::holds_alternative<IfStmt>(n->data)) {
        return compile_if(n, wip);
    }
    else if (std::holds_alternative<DoWhile>(n->data)) {
        return compile_dowhile(n, wip);
    }
    else if (std::holds_alternative<MethodDeclaration>(n->data)) {
        return compile_methoddecl(n, wip);
    }
    else if (std::holds_alternative<MethodDefinition>(n->data)) {
        return compile_methoddef(n, wip);
    }
    else if (std::holds_alternative<ReturnValue>(n->data)) {
        return compile_return(n, wip);
    }
    else if (std::holds_alternative<MethodCall>(n->data)) {
        return compile_methodcall(n, wip);
    }

    throw "Invalid node";
}

void
generate_bytecode(std::shared_ptr<Node> ast_root, compiler_wip& wip) {
    wip.local_variables.push_back({});
    compile_node(ast_root, wip, {});
}

void
link(compiler_wip& wip) {
    for (auto& ml : wip.methodlinks) {
        auto& bc = wip.bytecodes[ml.index];
        auto& method_index = wip.method_indices.at(ml.method_name);
        if (!wip.methods[method_index].defined) {
            throw "Method never defined";
        }
        size_t diff = 0;
        if (ml.index >= wip.methods[method_index].address) {
            diff = ml.index - wip.methods[method_index].address;
        }
        else {
            diff = wip.methods[method_index].address - ml.index;
        }

        BytecodeParam linked;
        if (diff < 0xFFFF) {
            if (ml.index >= wip.methods[method_index].address) {
                linked = FastCallBackward(LocMemoryDirect, diff + 1);
            }
            else {
                linked = FastCallForward(LocMemoryDirect, diff - 1);
            }
        }
        else {
            linked = ScriptCall(LocMemoryDirect, method_index);
        }

        bc.set_parameter1(linked);
        // We don't touch the base (p2)
        bc.set_parameter3(StackSize(LocMemoryDirect, wip.methods[method_index].stack_bytes));
    }

    for (auto& ll : wip.labellinks) {
        auto& bc = wip.bytecodes[ll.index];
        size_t link_address = wip.labels.at(ll.label);

        BytecodeParam linked = JumpExact(LocMemoryDirect, link_address);
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

void
print_program(compiler_wip& wip) {
    std::cout << "\nGlobals:\n";
    for (auto& m : wip.local_variables[0]) {
        std::cout << m.first << ": addr=" << m.second.address << "\n";
    }
    std::cout << "\nMethods:\n";
    for (auto& m : wip.methods) {
        std::cout << m.name<< ": p=" << m.param_bytes << "; s=" << m.stack_bytes << "\n";
    }
    std::cout << "\nBytecodes:\n";

    size_t i = 0;
    for (auto& bc : wip.bytecodes) {
        std::cout << i << ": " << (unsigned int)bc.op << " ";
        std::cout << address_page(bc.p1) << ":" << address_offset(bc.p1) << " ";
        std::cout << address_page(bc.p2) << ":" << address_offset(bc.p2) << " ";
        std::cout << address_page(bc.p3) << ":" << address_offset(bc.p3) << "\n";
        i++;
    }
}

std::optional<std::type_index>
get_cpp_type(const TypeInfo& type) {
    if (std::holds_alternative<PrimitiveType>(type.type)) {
        auto prim = std::get<PrimitiveType>(type.type);
        switch (prim) {
        case PrimitiveType::boolean:
            return typeid(bool);
        case PrimitiveType::empty:
            return typeid(void);
        case PrimitiveType::s32:
            return typeid(int);
        case PrimitiveType::f32:
            return typeid(float);
        }
    }
    if (type.backing_type) {
        return type.backing_type.value();
    }
    return {};
}

std::shared_ptr<Program>
generate_bytecode(std::shared_ptr<Node> ast_root, const TypeTable& types, const ImportedMethods& imported_methods) {
    compiler_wip wip(types, imported_methods);
    generate_bytecode(ast_root, wip);
    link(wip);
    print_program(wip);
    wip.bytecodes.push_back(Opcode(Bytecode::Break));

    auto p = std::make_shared<Program>(wip.next_const);

    for (auto& m : wip.local_variables[0]) {
        auto t = wip.types.get_type(m.second.type);
        p->add_global_index(m.first, t.size, m.second.address);
    }

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
    p->add_code(wip.bytecodes);

    for (auto& m : wip.methods) {
        p->add_method_addr(m.name, m.param_bytes, m.stack_bytes, m.address);
        
        auto& t = std::get<MethodType>(wip.types.get_type(m.type).type);
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
            p->register_method(m.name, types);
        }
    }

    return p;
}
