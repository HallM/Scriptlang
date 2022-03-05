#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "AST.h"
#include "VMFFI.h"
#include "Program.h"
#include "Types.h"

namespace MattScript {
namespace Generator {

typedef std::vector<Ast::ImportedMethod> ImportedMethods;

std::shared_ptr<Program> generate_bytecode(std::shared_ptr<Ast::Node> ast_root, const Types::TypeTable& types, const ImportedMethods& imported_methods);

} // bgen
} // MattScript
