#include "Types.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <sstream>
#include <typeindex>
#include <typeinfo>
#include <variant>
#include <vector>

#include "VMFFI.h"

#define NUMERICAL_OPERATORS(vmtype) \
    {Ast::BinaryOps::Add, TypeBinaryOperator{ #vmtype, #vmtype, #vmtype, TypeOperatorBytecode{Bytecode::##vmtype##Add} }}, \
    {Ast::BinaryOps::Subtract, TypeBinaryOperator{ #vmtype, #vmtype, #vmtype, TypeOperatorBytecode{Bytecode::##vmtype##Sub} }}, \
    {Ast::BinaryOps::Multiply, TypeBinaryOperator{ #vmtype, #vmtype, #vmtype, TypeOperatorBytecode{Bytecode::##vmtype##Mul} }}, \
    {Ast::BinaryOps::Divide, TypeBinaryOperator{ #vmtype, #vmtype, #vmtype, TypeOperatorBytecode{Bytecode::##vmtype##Div} }}, \
    {Ast::BinaryOps::Modulo, TypeBinaryOperator{ #vmtype, #vmtype, #vmtype, TypeOperatorBytecode{Bytecode::##vmtype##Mod} }},

#define COMPARE_OPERATORS(vmtype) \
    {Ast::BinaryOps::Eq, TypeBinaryOperator{ #vmtype, #vmtype, "bool", TypeOperatorBytecode{Bytecode::##vmtype##Equal} }}, \
    {Ast::BinaryOps::NotEq, TypeBinaryOperator{ #vmtype, #vmtype, "bool", TypeOperatorBytecode{Bytecode::##vmtype##NotEqual} }}, \
    {Ast::BinaryOps::Less, TypeBinaryOperator{ #vmtype, #vmtype, "bool", TypeOperatorBytecode{Bytecode::##vmtype##Less} }}, \
    {Ast::BinaryOps::LessEqual, TypeBinaryOperator{ #vmtype, #vmtype, "bool", TypeOperatorBytecode{Bytecode::##vmtype##LessEqual} }}, \
    {Ast::BinaryOps::Greater, TypeBinaryOperator{ #vmtype, #vmtype, "bool", TypeOperatorBytecode{Bytecode::##vmtype##Greater} }}, \
    {Ast::BinaryOps::GreaterEqual, TypeBinaryOperator{ #vmtype, #vmtype, "bool", TypeOperatorBytecode{Bytecode::##vmtype##GreaterEqual} }},

#define BITWISE_OPERATORS(vmtype) \
    {Ast::BinaryOps::BitShiftLeft, TypeBinaryOperator{ #vmtype, #vmtype, #vmtype, TypeOperatorBytecode{Bytecode::##vmtype##ShiftLeft} }}, \
    {Ast::BinaryOps::BitShiftRight, TypeBinaryOperator{ #vmtype, #vmtype, #vmtype, TypeOperatorBytecode{Bytecode::##vmtype##ShiftRight} }}, \
    {Ast::BinaryOps::BitAnd, TypeBinaryOperator{ #vmtype, #vmtype, #vmtype, TypeOperatorBytecode{Bytecode::##vmtype##BitAnd} }}, \
    {Ast::BinaryOps::BitOr, TypeBinaryOperator{ #vmtype, #vmtype, #vmtype, TypeOperatorBytecode{Bytecode::##vmtype##BitOr} }}, \
    {Ast::BinaryOps::BitXor, TypeBinaryOperator{ #vmtype, #vmtype, #vmtype, TypeOperatorBytecode{Bytecode::##vmtype##BitXor} }},

namespace MattScript {
namespace Types {

TypeTable::TypeTable() {
    _types["void"] = TypeInfo{"void", {}, 0, PrimitiveType::empty, typeid(void)};
    _types["bool"] = TypeInfo{
        "bool", {}, runtimesizeof<bool>(), PrimitiveType::boolean, typeid(bool),
        {},
        {
            {Ast::BinaryOps::And, TypeBinaryOperator{ "bool", "bool", "bool", TypeOperatorBytecode{Bytecode::bAnd} }},
            {Ast::BinaryOps::Or, TypeBinaryOperator{ "bool", "bool", "bool", TypeOperatorBytecode{Bytecode::bOr} }},
            {Ast::BinaryOps::Eq, TypeBinaryOperator{ "bool", "bool", "bool", TypeOperatorBytecode{Bytecode::bEqual} }},
            {Ast::BinaryOps::NotEq, TypeBinaryOperator{ "bool", "bool", "bool", TypeOperatorBytecode{Bytecode::bNotEqual} }},
        }
    };
    _types["s32"] = TypeInfo{
        "s32", {}, runtimesizeof<int>(), PrimitiveType::s32, typeid(int),
        {},
        {
            NUMERICAL_OPERATORS(s32)
            COMPARE_OPERATORS(s32)
            BITWISE_OPERATORS(s32)
        }
    };
    _types["f32"] = TypeInfo{
        "f32", {}, runtimesizeof<float>(), PrimitiveType::f32, typeid(float),
        {},
        {
            NUMERICAL_OPERATORS(f32)
            COMPARE_OPERATORS(f32)
        }
    };
    _types["ref bool"] = TypeInfo{"ref bool", "bool", runtimesizeof<bool*>(), PrimitiveType::boolean, typeid(bool*)};
    _types["ref s32"] = TypeInfo{"ref s32", "s32", runtimesizeof<int*>(), PrimitiveType::s32, typeid(int*)};
    _types["ref f32"] = TypeInfo{"ref f32", "f32", runtimesizeof<float*>(), PrimitiveType::f32, typeid(float*)};
    _mapped[typeid(void)] = "void";
    _mapped[typeid(bool)] = "bool";
    _mapped[typeid(int)] = "s32";
    _mapped[typeid(float)] = "f32";
    _mapped[typeid(bool*)] = "ref bool";
    _mapped[typeid(int*)] = "ref s32";
    _mapped[typeid(float*)] = "ref f32";
}

TypeTable::~TypeTable() {}

const TypeInfo&
TypeTable::get_type(std::string name) const {
    return _types.at(name);
}

const std::vector<std::string>
TypeTable::type_names() const {
    std::vector<std::string> v;
    for (const auto it : _types) {
        v.push_back(it.first);
    }
    return v;
}

std::string
TypeTable::add_method(std::string return_type, bool return_mutable, std::vector<MethodTypeParameter> params) {
    std::ostringstream stream;
    stream << return_type << "(";
    bool add_comma = false;
    for (auto& p : params) {
        if (add_comma) {
        stream << ",";
        }
        else {
            add_comma = true;
        }
        stream << p.type;
    }
    stream << ")";

    std::string type_name = stream.str();
    std::string ref_type = "ref " + type_name;

    _types[type_name] = TypeInfo{
        type_name, {},
        0,
        MethodType{
            return_type,
            return_mutable,
            params
        },
        // There's no backing type at this time.
        {}
    };
    _types[ref_type] = TypeInfo{
        ref_type, type_name,
        sizeof(size_t),
        MethodType{
            return_type,
            return_mutable,
            params
        },
        {}
    };
    return type_name;
}

std::string
TypeTable::add_struct(std::string type_name, std::vector<StructTypeMember> members) {
    size_t size = 0;
    for (auto& m : members) {
        size += get_type(m.type).size;
    }

    std::unordered_map<std::string, StructTypeMember> mapped_members;
    for (auto member : members) {
        mapped_members[member.name] = member;
    }

    _types[type_name] = TypeInfo{
        type_name, {},
        size,
        StructType{mapped_members},
        {}
    };
    std::string ref_type = "ref " + type_name;
    _types[ref_type] = TypeInfo{
        ref_type, type_name,
        sizeof(size_t),
        StructType{mapped_members},
        {}
    };
    return type_name;
}

}
}
