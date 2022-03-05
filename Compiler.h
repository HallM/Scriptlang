#pragma once

#include <functional>
#include <memory>

#include "AST.h"
#include "Program.h"
#include "Types.h"
#include "VMFFI.h"

template <typename T>
class StructImportBuilder {
public:
    StructImportBuilder(std::string name, TypeTable& types) : _name(name), _types(types) {}

    template <typename M>
    StructImportBuilder<T>& add_member(std::string member_name, size_t offset) {
        _members.push_back(_types.imported_struct_member<M>(member_name, offset));
        return *this;
    }

    void build() {
        _types.imported_struct_type<T>(_name, _members);
    }
private:
    TypeTable& _types;
    std::string _name;
    std::vector<StructTypeMember> _members;
};

class Compiler {
public:
    Compiler();

    std::shared_ptr<Program> compile(std::string filename, std::string contents);

    template <typename T>
    StructImportBuilder<T> build_struct(std::string name) {
        return StructImportBuilder<T>(name, _types);
    }

    template <typename Ret, typename... Args>
    void import_method(std::string name, std::function<Ret(Args...)> method) {
        std::string type_name = _types.imported_method_type<Ret, Args...>();
        std::shared_ptr<IRunnable> wrapped = std::make_shared<BuiltinRunnable<Ret, Args...>>(method);

        auto m = ImportedMethod{
            name,
            wrapped,
            type_name,
            typeid(Ret),
            { typeid(Args)... }
        };
        _methods.push_back(m);
    }

private:
    TypeTable _types;
    std::vector<ImportedMethod> _methods;
};
