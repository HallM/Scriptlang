#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <variant>
#include <vector>

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
//struct EnumTypeNamedValue {
//    std::string name;
//    std::string type;
//};
//struct EnumType {
//    std::vector<EnumTypeNamedValue> values;
//};

struct StructTypeMember {
    std::string name;
    size_t offset;
    std::string type;
};
struct StructType {
    std::unordered_map<std::string, StructTypeMember> members;
};

struct MethodTypeParameter {
    std::string type;
};
struct MethodType {
    std::string return_type;
    std::vector<MethodTypeParameter> parameters;
};

struct TypeInfo {
    std::string name;
    // if this type is a reference type, this will be set to the type it refers to.
    std::optional<std::string> ref_type;
    size_t size;
    std::variant<PrimitiveType, StructType, MethodType> type;
    std::optional<std::type_index> backing_type;
    std::unordered_map<std::string, std::string> methods;
};

class TypeTable {
public:
    TypeTable();
    ~TypeTable();

    const TypeInfo& get_type(std::string name) const;

    std::string add_method(std::string return_type, std::vector<MethodTypeParameter> params);
    std::string add_struct(std::string name, std::vector<StructTypeMember> members);

    template <typename Ret, typename... Args>
    std::string imported_method_type() {
        std::type_index r = typeid(Ret);
        std::string r_name = _mapped.at(r);

        std::vector<MethodTypeParameter> params = {
            MethodTypeParameter{_mapped.at(typeid(Args))}...
        };

        return add_method(r_name, params);
    }

    template <typename T>
    StructTypeMember imported_struct_member(std::string member_name, size_t offset) {
        std::type_index rawtype = typeid(T);
        return StructTypeMember{
            member_name, offset, _mapped.at(rawtype)
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

        std::string ref_type = "ref " + type_name;
        _types[ref_type] = TypeInfo{
            ref_type, type_name,
            sizeof(T*),
            StructType{mapped_members},
            typeid(T*)
        };
        _mapped[typeid(T*)] = ref_type;
        return type_name;
    }

private:
    std::unordered_map<std::string, TypeInfo> _types;
    std::unordered_map<std::type_index, std::string> _mapped;
};

} // Types
} // MattScript
