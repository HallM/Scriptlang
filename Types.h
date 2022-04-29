#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <variant>
#include <vector>

#include "AST.h"
#include "VMBytecode.h"

namespace MattScript {
namespace Types {

enum class PrimitiveType: unsigned int {
    empty,
    boolean,
    s32,
    f32,
};

//struct TupleTypeValue {
//    size_t offset;
//    std::string type;
//};
//struct TupleType {
//    std::vector<TupleTypeValue> values;
//};
//
// Based on the C++ enum, but limited to signed 32-bit
struct EnumType {
    std::unordered_map<std::string, int> values;
};

struct StructTypeMember {
    std::string name;
    size_t offset;
    std::string type;
    bool is_mutable;
};
struct StructType {
    std::unordered_map<std::string, StructTypeMember> members;
};

struct MethodTypeParameter {
    std::string type;
    bool is_mutable;
};
struct MethodType {
    std::string return_type;
    bool return_mutable;
    std::vector<MethodTypeParameter> parameters;
};

struct TypeOperatorBytecode {
    Bytecode bytecode;
};
struct TypeOperatorCall {
    std::string method_name;
};
struct TypeBinaryOperator {
    std::string lhs_type;
    std::string rhs_type;
    std::string return_type;
    std::variant<TypeOperatorBytecode, TypeOperatorCall> method;
};
struct TypeUnaryOperator {
    std::string item_type;
    std::string return_type;
    std::variant<TypeOperatorBytecode, TypeOperatorCall> method;
};

struct TypeInfo {
    std::string name;
    // if this type is a reference type, this will be set to the type it refers to.
    std::optional<std::string> ref_type;
    size_t size;
    std::variant<PrimitiveType, EnumType, StructType, MethodType> type;
    std::optional<std::type_index> backing_type;
    std::unordered_map<std::string, std::string> methods;
    std::unordered_map<Ast::BinaryOps, TypeBinaryOperator> binary_operators;
    std::unordered_map<Ast::UnaryOps, TypeUnaryOperator> unary_operators;
};

class TypeTable {
public:
    TypeTable();
    ~TypeTable();

    bool type_exists(std::string name) const;
    const TypeInfo& get_type(std::string name) const;
    const std::vector<std::string> type_names() const;

    std::string add_method(std::string return_type, bool return_mutable, std::vector<MethodTypeParameter> params);
    std::string add_struct(std::string name, std::vector<StructTypeMember> members);
    std::string add_enum(std::string name, std::unordered_map<std::string, int> values);

    template <typename Ret, typename... Args>
    std::string imported_method_type() {
        std::type_index r = typeid(Ret);
        std::string r_name = _mapped.at(r);

        std::vector<MethodTypeParameter> params = {
            MethodTypeParameter{_mapped.at(typeid(Args)), !std::is_const<Args>::value}...
        };

        return add_method(r_name, !std::is_const<Ret>::value, params);
    }

    template <typename T>
    StructTypeMember imported_struct_member(std::string member_name, size_t offset) {
        std::type_index rawtype = typeid(T);
        return StructTypeMember{
            member_name, offset, _mapped.at(rawtype), !std::is_const<T>::value
        };
    }
    template <typename T>
    std::string imported_struct_type(std::string type_name, std::vector<StructTypeMember> members) {
        std::unordered_map<std::string, StructTypeMember> mapped_members;
        for (auto member : members) {
            mapped_members[member.name] = member;
        }

        _types[type_name] = TypeInfo{
            type_name, {},
            sizeof(T),
            StructType{mapped_members},
            typeid(T)
        };
        _mapped[typeid(T)] = type_name;
        _mapped[typeid(const T)] = type_name;

        std::string ref_type = "ref " + type_name;
        _types[ref_type] = TypeInfo{
            ref_type, type_name,
            sizeof(T*),
            StructType{mapped_members},
            typeid(T*)
        };
        _mapped[typeid(T*)] = ref_type;
        _mapped[typeid(const T*)] = ref_type;
        return type_name;
    }

private:
    std::unordered_map<std::string, TypeInfo> _types;
    std::unordered_map<std::type_index, std::string> _mapped;
};

} // Types
} // MattScript
