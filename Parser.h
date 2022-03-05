#pragma once

#include <memory>
#include <string>
#include <vector>

#include "AST.h"
#include "Tokens.h"
#include "Types.h"

namespace MattScript {
namespace Parser {

std::shared_ptr<Ast::FileNode> parse_to_ast(std::string file_name, std::vector<std::shared_ptr<Tokens::Token>> tokens, Types::TypeTable& types);

}
}
