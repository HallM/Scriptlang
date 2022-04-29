#pragma once

#include <iostream>
#include <functional>
#include <memory>

#include "AST.h"
#include "Program.h"
#include "Types.h"
#include "VMFFI.h"

namespace MattScript {

template <typename T>
class StructImportBuilder {
public:
    StructImportBuilder(std::string name, Types::TypeTable& types) : _name(name), _types(types) {}

    template <typename M>
    StructImportBuilder<T>& add_member(std::string member_name, size_t offset) {
        _members.push_back(_types.imported_struct_member<M>(member_name, offset));
        return *this;
    }

    StructImportBuilder<T>& build() {
        _types.imported_struct_type<T>(_name, _members);
        return *this;
    }
private:
    Types::TypeTable& _types;
    std::string _name;
    std::vector<Types::StructTypeMember> _members;
    std::vector<Ast::ImportedMethod> _methods;
};

class EnumImportBuilder {
public:
    EnumImportBuilder(std::string name, Types::TypeTable& types) : _name(name), _types(types) {}

    EnumImportBuilder& add_value(std::string value_name, int value) {
        _values[value_name] = value;
        return *this;
    }

    EnumImportBuilder& build() {
        _types.add_enum(_name, _values);
        return *this;
    }
private:
    Types::TypeTable& _types;
    std::string _name;
    std::unordered_map<std::string, int> _values;
};

class Compiler {
public:
    Compiler();

    std::shared_ptr<Program> compile(std::string filename, std::string contents);

    template <typename T>
    StructImportBuilder<T> build_struct(std::string name) {
        return StructImportBuilder<T>(name, _types);
    }

    EnumImportBuilder build_enum(std::string name) {
        return EnumImportBuilder(name, _types);
    }

    template <typename Ret, typename... Args>
    void import_method(std::string name, std::function<Ret(Args...)> method) {
        std::string type_name = _types.imported_method_type<Ret, Args...>();
        std::shared_ptr<IRunnable> wrapped = std::make_shared<BuiltinRunnable<Ret, Args...>>(method);

        auto m = Ast::ImportedMethod{
            {},
            name,
            wrapped,
            type_name,
            typeid(Ret),
            { typeid(Args)... }
        };
        _methods.push_back(m);
    }

    template <typename Ret, typename... Args>
    void import_scoped_method(std::string scope, std::string name, std::function<Ret(Args...)> method) {
        std::string type_name = _types.imported_method_type<Ret, Args...>();
        std::shared_ptr<IRunnable> wrapped = std::make_shared<BuiltinRunnable<Ret, Args...>>(method);

        auto m = Ast::ImportedMethod{
            {scope},
            name,
            wrapped,
            type_name,
            typeid(Ret),
            { typeid(Args)... }
        };
        _methods.push_back(m);
    }

private:
    Types::TypeTable _types;
    std::vector<Ast::ImportedMethod> _methods;
};

} // MattScript
