#include "Compiler.h"

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
#include "VMBytecode.h"
#include "VMFFI.h"

//
// default, included types
// if/elseif statements and comparisons
// loops (do-while)
// switch back to backwards params/rets? maybe?
// check locals created
// change Program interface
// objects
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
// optimize stack usage in compiled code
//   1. if cant re-use rhs, then try to reuse lhs
//   2. should we pass a "return hint" ?
//
// combine ops where possible
//   1. ADD x, y -> z; SET z -> q should be ADD x, y -> q
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

operinfo assignment_opcode(std::string type) {
    if (type == type_s32) {
        return {Bytecode::s32Set, type_bool};
    }
    else if (type == type_f32) {
        return {Bytecode::f32Set, type_bool};
    }
    else if (type == type_bool) {
        return {Bytecode::s32Set, type_bool};
    }
    return {};
}

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
            //{BinaryOps::BitShiftLeft, },
            //{BinaryOps::BitShiftRight, },
            //{BinaryOps::BitAnd, },
            //{BinaryOps::BitOr, },
            //{BinaryOps::BitXor, },
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
            //{BinaryOps::And, },
            //{BinaryOps::Or, },
            //{BinaryOps::Eq, },
            //{BinaryOps::NotEq, },
            //{BinaryOps::Less, },
            //{BinaryOps::LessEqual, },
            //{BinaryOps::Greater, },
            //{BinaryOps::GreaterEqual, },
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
    const ImportedTable& imported_methods;

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
        int
    >> constant_values;
    std::unordered_map<std::string, size_t> constant_addresses;
    size_t next_const;

    std::unordered_map<std::string, methodinfo> methods;

    public:
    compiler_wip(const TypeTable& t, const ImportedTable& m) : types(t), imported_methods(m), next_label(0), next_stack(0), next_const(0) {}
};

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
    wip.next_const += sizeof(T);
    wip.constant_values.push_back(v);
    wip.constant_addresses[key] = addr;
    return addr;
}

compiled_result compile_node(Node* n, compiler_wip& wip);

compiled_result compile_const_s32(Node* n, compiler_wip& wip) {
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

compiled_result compile_const_f32(Node* n, compiler_wip& wip) {
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

compiled_result compile_const_bool(Node* n, compiler_wip& wip) {
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

compiled_result compile_identifier(Node* n, compiler_wip& wip) {
    auto ident = std::get<Identifier>(n->data).name;

    for (size_t i = wip.local_variables.size(); i--;) {
        auto lv = wip.local_variables[i];
        auto it = lv.find(ident);
        if (it != lv.end()) {
            BytecodeParam ret;
            if (i == 0) {
                std::cout << "global: " << ident << " found in " << i << " at " << it->second.address << "\n";
                ret = GlobalAddress(LocMemoryDirect, it->second.address);
            }
            else {
                std::cout << "local: " << ident << " found in " << i << " at " << it->second.address << "\n";
                ret = StackAddressForward(LocMemoryDirect, it->second.address);
            }
            std::cout << "returned\n";
            return {
                it->second.type,
                true,
                ret,
                0,
                0
            };
        }
    }

    auto maybe_method = wip.methods.find(ident);
    if (maybe_method != wip.methods.end()) {
        if (!maybe_method->second.defined) {
            return {
                maybe_method->second.type,
                false,
                methodlink{ident},
                0,
                0
            };
        }
        return {
            maybe_method->second.type,
            false,
            ScriptCall(LocMemoryDirect, maybe_method->second.address),
            0,
            0
        };
    }

    //auto maybe_imported = wip.imported_methods.find(ident);
    //if (maybe_imported != wip.imported_methods.end()) {
    //    return {
    //        maybe_imported->second.type,
    //        true,
    //        ExternalCall(LocMemoryDirect, maybe_imported->second.address),
    //        0,
    //        0
    //    };
    //}

    throw "Identifier not found";
}

compiled_result reserve_local(std::string name, std::string type_name, compiler_wip& wip) {
    auto& loc = wip.local_variables[wip.local_variables.size() - 1];
    if (loc.find(name) != loc.end()) {
        // TODO: shadow probably would work fine without
        throw "Ident already declared";
    }
    auto type = wip.types.at(type_name);
    auto address = wip.next_stack;
    auto size = type.size;
    wip.next_stack += size;
    loc[name] = {
        name, type_name, address
    };
    std::cout << "reserving " << name << " in " << wip.local_variables.size() - 1 << " at " << address << "\n";
    return {
        type_empty,
        false,
        BytecodeParam(0, 0),
        size,
        size
    };
}

compiled_result compile_vardecl(Node* n, compiler_wip& wip) {
    auto decl = std::get<VariableDeclaration>(n->data);
    return reserve_local(decl.name, decl.type, wip);
}

compiled_result compile_shared_binop(Node* lhs, Node* rhs, BinaryOps operation, compiler_wip& wip) {
    // TODO: reduce temporary/stack usage
    // I think I should pass down a "please store here"
    size_t stack = 0;
    size_t stack_start = wip.next_stack;

    size_t total_used = 0;

    auto rhs_ret = compile_node(rhs, wip);
    std::cout << "binop rhs returned: " << rhs_ret.stack_bytes_returned << " used: " << rhs_ret.stack_bytes_used << "\n";
    total_used = rhs_ret.stack_bytes_used;
    auto optype = rhs_ret.type;

    auto lhs_ret = compile_node(lhs, wip);
    std::cout << "binop lhs returned: " << lhs_ret.stack_bytes_returned << " used: " << lhs_ret.stack_bytes_used << "\n";
    if (total_used < lhs_ret.stack_bytes_used + stack) {
        // the "stack" quantity is already reserved.
        // if the rhs still happened to use more than that, then we
        // could re-use some temporaries.
        std::cout << "binop lhs+rhs_ret > total_used: " << total_used << " < " << lhs_ret.stack_bytes_used << " + " << stack << " || " << rhs_ret.stack_bytes_returned << "\n";
        total_used = lhs_ret.stack_bytes_used + rhs_ret.stack_bytes_returned;
        std::cout << "binop expand total: " << total_used << "\n";
    }

    if (lhs_ret.type != optype) {
        throw "Incompatible types";
    }

    // TODO: there is one more re-use case I hadnt thought before:
    // we could reuse params / stack elements.
    // a lhs/rhs needs to know it is returning a stack element
    BytecodeParam ret;
    if (rhs_ret.stack_bytes_returned >= wip.types.at(optype).size) {
        // attempt to re-use any temporaries from the rhs
        std::cout << "reusing rhs temporary for return\n";
        ret = std::get<BytecodeParam>(rhs_ret.address);
        stack = rhs_ret.stack_bytes_returned;
    } else if (lhs_ret.stack_bytes_returned >= wip.types.at(optype).size) {
        std::cout << "reusing lhs temporary for return\n";
        ret = std::get<BytecodeParam>(lhs_ret.address);
        stack = lhs_ret.stack_bytes_returned;
    } else {
        // TODO: how to reduce...
        // need to create and return a new temporary to hold the returned value of the binop
        std::cout << "Need a new temporary for return " << rhs_ret.stack_bytes_returned << " < " << wip.types.at(optype).size << "\n";
        size_t addr = wip.next_stack;
        size_t size = wip.types.at(optype).size;
        wip.next_stack += size;
        ret = StackAddressForward(LocMemoryDirect, addr);
        stack = rhs_ret.stack_bytes_returned + size;
        total_used += size;
    }
    std::cout << "binop now at used: " << total_used << "\n";

    auto optable = binary_opcode(optype);
    auto maybeOp = optable.find(operation);
    if (maybeOp == optable.end()) {
        throw "Operator not supported by type";
    }
    auto op = maybeOp->second;

    wip.bytecodes.push_back(
        Opcode(op.bc, std::get<BytecodeParam>(lhs_ret.address), std::get<BytecodeParam>(rhs_ret.address), ret)
    );
    // can free all unused stack for other ops now
    std::cout << "before free: " << wip.next_stack << "\n";
    wip.next_stack = stack_start + stack;
    std::cout << "after free: " << wip.next_stack << "\n";

    std::cout << "returned: " << stack << " used: " << total_used << "\n";
    return {
        optype,
        false,
        ret,
        stack,
        total_used
    };
}

compiled_result compile_binop(Node* n, compiler_wip& wip) {
    auto opnode = std::get<BinaryOperation>(n->data);
    return compile_shared_binop(opnode.lhs, opnode.rhs, opnode.op, wip);
}

compiled_result compile_setop(Node* n, compiler_wip& wip) {
    auto opnode = std::get<SetOperation>(n->data);
    compiled_result assign_value;
    size_t stack_begin = wip.next_stack;

    if (opnode.op) {
        assign_value = compile_shared_binop(opnode.lhs, opnode.rhs, opnode.op.value(), wip);
    }
    else {
        std::cout << "Assigning without operator\n";
        assign_value = compile_node(opnode.rhs, wip);
    }
    size_t total_used = assign_value.stack_bytes_used;

    auto optype = assign_value.type;

    auto assign_to = compile_node(opnode.lhs, wip);
    if (!assign_to.assignable) {
        std::cout << "Not assignable sad face\n";
        throw "lhs is not assignable";
    }
    if (assign_to.type != optype) {
        std::cout << "`" << assign_to.type << "` no match `" << optype << "`\n";
        throw "Cannot set lhs to rhs as the types do not match";
    }
    if (total_used < assign_to.stack_bytes_used + assign_value.stack_bytes_returned) {
        total_used = assign_to.stack_bytes_used + assign_value.stack_bytes_returned;
    }

    auto opinfo = assignment_opcode(optype);
    wip.bytecodes.push_back(
        Opcode(opinfo.bc, std::get<BytecodeParam>(assign_value.address), std::get<BytecodeParam>(assign_to.address))
    );
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

compiled_result compile_nodelist(std::vector<Node*> nodes, compiler_wip& wip) {
    // TODO: support block-expressions
    size_t last_stack = wip.next_stack;
    size_t total_used = 0;
    for (auto& subnode : nodes) {
        compiled_result last = compile_node(subnode, wip);
        std::cout << "blocknode total used: " << last.stack_bytes_used << " vs " << total_used << "\n";
        if (last.stack_bytes_used > total_used) {
            total_used = last.stack_bytes_used;
        }
    }
    std::cout << "block total used: " << total_used << "\n";

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

compiled_result compile_block(Node* n, compiler_wip& wip) {
    auto block = std::get<Block>(n->data);

    std::cout << "add stack\n";
    wip.local_variables.push_back({});

    auto ret = compile_nodelist(block.nodes, wip);

    wip.local_variables.pop_back();
    std::cout << "pop stack\n";

    return ret;
}

compiled_result compile_globalblock(Node* n, compiler_wip& wip) {
    auto block = std::get<GlobalBlock>(n->data);
    return compile_nodelist(block.nodes, wip);
}

compiled_result compile_methoddecl(Node* n, compiler_wip& wip) {
    auto method = std::get<MethodDeclaration>(n->data);

    wip.methods[method.name] = methodinfo{
        method.name,
        method.type,
        false
    };

    return {
        type_empty,
        false,
        BytecodeParam(0, 0),
        0,
        0
    };
}

compiled_result compile_methoddef(Node* n, compiler_wip& wip) {
    auto method = std::get<MethodDefinition>(n->data);

    auto& methodinfo = wip.methods[method.name];

    size_t start_stack = wip.next_stack;
    wip.next_stack = 0;
    std::cout << "add mstack\n";
    wip.local_variables.push_back({});

    size_t param_size = 0;
    auto type = wip.types.at(methodinfo.type);
    auto typedata = std::get<TypeMethod>(type.type);

    if (typedata.parameters.size() != method.param_names.size()) {
        throw "Method definition parameters do not match declaration.";
    }

    for (size_t i = 0; i < typedata.parameters.size(); i++) {
        std::string ptype_name = typedata.parameters[i].type;
        auto& pt = wip.types.at(ptype_name);
        param_size += pt.size;
        reserve_local(method.param_names[i], ptype_name, wip);
    }
    std::cout << "check: " << wip.local_variables[wip.local_variables.size() - 1].size() << "\n";

    // always reserve at least the ret type
    auto rettype = wip.types.at(typedata.return_type);
    if (param_size < rettype.size) {
        param_size = rettype.size;
    }
    wip.next_stack = param_size;

    size_t address = wip.bytecodes.size();

    auto r = compile_node(method.node, wip);
    std::cout << "method total used: " << r.stack_bytes_used << "\n";

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
    std::cout << "pop mstack\n";

    return {
        type_empty,
        false,
        ScriptCall(LocMemoryDirect, address),
        0,
        0
    };
}

compiled_result compile_if(Node* n, compiler_wip& wip) {
    auto stmt = std::get<IfStmt>(n->data);

    size_t stack_start = wip.next_stack;
    size_t else_label = wip.next_label++;
    size_t end_label = wip.next_label++;
    size_t max_used = 0;

    auto condition = compile_node(stmt.condition, wip);
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

    auto then = compile_node(stmt.then, wip);
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
        auto otherwise = compile_node(stmt.otherwise.value(), wip);
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

compiled_result compile_return(Node* n, compiler_wip& wip) {
    auto ret = std::get<ReturnValue>(n->data);
    size_t max_used = 0;

    if (ret.value) {
        auto value = compile_node(ret.value.value(), wip);
        std::cout << "ret value total used: " << value.stack_bytes_used << "\n";

        auto opinfo = assignment_opcode(value.type);
        wip.bytecodes.push_back(
            Opcode(opinfo.bc, std::get<BytecodeParam>(value.address), StackAddressForward(LocMemoryDirect, 0))
        );
        max_used = value.stack_bytes_used;
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

compiled_result compile_methodparam(Node* n, MethodTypeParameter type, compiler_wip& wip) {
    auto param = std::get<CallParam>(n->data);
    // but how do I make sure I append to the END of the stack.
    // if some other step makes the stack even larger, then these values are wrong.
    // call passes the new base ptr which is right at the start
    // it doesnt have to be at the end of the stack. we just have to be assured
    // that there is nothing after the params.

    size_t last_stack = wip.next_stack;
    auto store_address = StackAddressForward(LocMemoryDirect, wip.next_stack);
    auto typeinfo = wip.types.at(type.type);

    // TODO: suggesting positions would REALLY help here
    auto value = compile_node(param.value, wip);
    if (value.type != type.type) {
        throw "Parameter is the incorrect type";
    }

    size_t total_used = value.stack_bytes_used;
    if (merge_page_offset(std::get<BytecodeParam>(value.address)) != merge_page_offset(store_address)) {
        auto opinfo = assignment_opcode(value.type);
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

compiled_result compile_methodcall(Node* n, compiler_wip& wip) {
    auto method = std::get<MethodCall>(n->data);

    size_t max_used = 0;

    auto callable = compile_node(method.callable, wip);
    max_used = callable.stack_bytes_used;

    auto methodtype = wip.types.at(callable.type);
    if (!std::holds_alternative<TypeMethod>(methodtype.type)) {
        throw "LHS is not a function";
    }

    auto typeinfo = std::get<TypeMethod>(methodtype.type);
    // TODO: default params
    if (typeinfo.parameters.size() != method.params.size()) {
        throw "Incorrect number of params";
    }
    auto returntype = wip.types.at(typeinfo.return_type);

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

//compiled_result compile_(Node* n, compiler_wip& wip) {
//}

compiled_result compile_node(Node* n, compiler_wip& wip) {
    if (std::holds_alternative<ConstS32>(n->data)) {
        std::cout << "compile cs32?!\n";
        return compile_const_s32(n, wip);
    }
    else if (std::holds_alternative<ConstF32>(n->data)) {
        std::cout << "compile cf32?!\n";
        return compile_const_f32(n, wip);
    }
    else if (std::holds_alternative<ConstBool>(n->data)) {
        std::cout << "compile cbool\n";
        return compile_const_bool(n, wip);
    }
    else if (std::holds_alternative<BinaryOperation>(n->data)) {
        std::cout << "compile bop\n";
        return compile_binop(n, wip);
    }
    else if (std::holds_alternative<SetOperation>(n->data)) {
        std::cout << "compile sop\n";
        return compile_setop(n, wip);
    }
    else if (std::holds_alternative<Identifier>(n->data)) {
        std::cout << "compile id\n";
        return compile_identifier(n, wip);
    }
    else if (std::holds_alternative<VariableDeclaration>(n->data)) {
        std::cout << "compile vdec\n";
        return compile_vardecl(n, wip);
    }
    else if (std::holds_alternative<Block>(n->data)) {
        std::cout << "compile block\n";
        return compile_block(n, wip);
    }
    else if (std::holds_alternative<GlobalBlock>(n->data)) {
        std::cout << "compile globalblock\n";
        return compile_globalblock(n, wip);
    }
    else if (std::holds_alternative<IfStmt>(n->data)) {
        std::cout << "compile if\n";
        return compile_if(n, wip);
    }
    else if (std::holds_alternative<MethodDeclaration>(n->data)) {
        std::cout << "compile mdec\n";
        return compile_methoddecl(n, wip);
    }
    else if (std::holds_alternative<MethodDefinition>(n->data)) {
        std::cout << "compile mdef\n";
        return compile_methoddef(n, wip);
    }
    else if (std::holds_alternative<ReturnValue>(n->data)) {
        std::cout << "compile ret\n";
        return compile_return(n, wip);
    }
    else if (std::holds_alternative<MethodCall>(n->data)) {
        std::cout << "compile call\n";
        return compile_methodcall(n, wip);
    }

    throw "Invalid node";
}

void
generate_bytecode(Node* ast_root, compiler_wip& wip) {
    std::cout << "add gstack\n";
    wip.local_variables.push_back({});
    compile_node(ast_root, wip);
}

void
link(compiler_wip& wip) {
    for (auto& ml : wip.methodlinks) {
        auto& bc = wip.bytecodes[ml.index];
        auto& method = wip.methods.at(ml.method_name);
        std::cout << "Linking " << ml.index << " to " << ml.method_name << " @ " << method.address << "\n";
        BytecodeParam linked = ScriptCall(LocMemoryDirect, method.address);
        switch (ml.param) {
        case 0:
            bc.l1 = linked.loc;
            bc.p1 = merge_page_offset(linked);
            break;
        case 1:
            bc.l2 = linked.loc;
            bc.p2 = merge_page_offset(linked);
            break;
        case 2:
            bc.l3 = linked.loc;
            bc.p3 = merge_page_offset(linked);
            break;
        }
    }

    for (auto& ll : wip.labellinks) {
        auto& bc = wip.bytecodes[ll.index];
        size_t link_address = wip.labels.at(ll.label);
        std::cout << "Linking " << ll.index << " to L" << ll.label << " @ " << link_address << "\n";

        BytecodeParam linked = JumpExact(LocMemoryDirect, link_address);
        switch (ll.param) {
        case 0:
            bc.l1 = linked.loc;
            bc.p1 = merge_page_offset(linked);
            break;
        case 1:
            bc.l2 = linked.loc;
            bc.p2 = merge_page_offset(linked);
            break;
        case 2:
            bc.l3 = linked.loc;
            bc.p3 = merge_page_offset(linked);
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
        std::cout << m.first << ": p=" << m.second.param_bytes << "; s=" << m.second.stack_bytes << "\n";
    }
    std::cout << "\nBytecodes:\n";

    for (auto& bc : wip.bytecodes) {
        std::cout << (unsigned int)bc.op << " ";
        std::cout << address_page(bc.p1) << ":" << address_offset(bc.p1) << " ";
        std::cout << address_page(bc.p2) << ":" << address_offset(bc.p2) << " ";
        std::cout << address_page(bc.p3) << ":" << address_offset(bc.p3) << "\n";
    }
}

std::type_index
findtype(const TypeInfo& type) {
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
    throw "Unable to convert type to C++";
}

std::shared_ptr<Program>
Compile(Node* ast_root, const TypeTable& types, const ImportedTable& imported_methods) {
    compiler_wip wip(types, imported_methods);
    generate_bytecode(ast_root, wip);
    link(wip);
    print_program(wip);

    auto p = std::make_shared<Program>(wip.next_const);

    for (auto& m : wip.local_variables[0]) {
        std::cout << "Add global " << m.first << ": addr=" << m.second.address << "\n";
        auto t = wip.types.at(m.second.type);
        p->add_global_index(m.first, t.size, m.second.address);
    }

    for (auto& it : imported_methods) {
        p->add_builtin(it.first, it.second);
    }

    for (auto& v : wip.constant_values) {
        if (std::holds_alternative<int>(v)) {
            std::cout << "Add s32 constant " << std::get<int>(v) << "\n";
            p->add_constant<int>("", std::get<int>(v));
        } else if (std::holds_alternative<float>(v)) {
            std::cout << "Add f32 constant " << std::get<float>(v) << "\n";
            p->add_constant<float>("", std::get<float>(v));
        }
    }
    p->add_code(wip.bytecodes);

    for (auto& m : wip.methods) {
        p->add_method_addr(m.first, m.second.param_bytes, m.second.stack_bytes, m.second.address);
        
        auto& t = std::get<TypeMethod>(wip.types.at(m.second.type).type);
        std::vector<std::type_index> types;
        types.push_back(findtype(wip.types.at(t.return_type)));
        for (auto& p : t.parameters) {
            types.push_back(findtype(wip.types.at(p.type)));
        }
        p->register_method(m.first, types);
    }

    return p;
}
