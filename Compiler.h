#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "AST.h"
#include "VMFFI.h"
#include "Program.h"
#include "Types.h"

typedef std::vector<ImportedMethod> ImportedMethods;

std::shared_ptr<Program> compile_ast(std::shared_ptr<Node> ast_root, const TypeTable& types, const ImportedMethods& imported_methods);
