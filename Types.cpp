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

TypeTable::TypeTable() {
    _types["void"] = TypeInfo{"void", {}, 0, PrimitiveType::empty, typeid(void)};
    _types["bool"] = TypeInfo{"bool", {}, runtimesizeof<bool>(), PrimitiveType::boolean, typeid(bool)};
    _types["s32"] = TypeInfo{"s32", {}, runtimesizeof<int>(), PrimitiveType::s32, typeid(int)};
    _types["f32"] = TypeInfo{"f32", {}, runtimesizeof<float>(), PrimitiveType::f32, typeid(float)};
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

std::string
TypeTable::add_method(std::string return_type, std::vector<MethodTypeParameter> params) {
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
