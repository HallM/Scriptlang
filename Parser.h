#pragma once

#include <memory>
#include <string>
#include <vector>

#include "AST.h"
#include "Tokens.h"
#include "Types.h"

std::shared_ptr<FileNode> parse_to_ast(std::string file_name, std::vector<std::shared_ptr<Token>> tokens, TypeTable& types);
