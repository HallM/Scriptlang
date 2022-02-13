#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "AST.h"
#include "VMFFI.h"
#include "Program.h"

typedef std::unordered_map<std::string, TypeInfo> TypeTable;
typedef std::unordered_map<std::string, IRunnable*> ImportedTable;

std::shared_ptr<Program> Compile(Node* ast_root, const TypeTable& types, const ImportedTable& imported_methods);
