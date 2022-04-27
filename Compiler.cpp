#include "Compiler.h"

#include <functional>
#include <memory>

#include "AST.h"
#include "BytecodeGenerator.h"
#include "Parser.h"
#include "Program.h"
#include "Tokenizer.h"
#include "Tokens.h"
#include "Types.h"

namespace MattScript {

Compiler::Compiler() {}

std::shared_ptr<Program>
Compiler::compile(std::string filename, std::string contents) {
    auto tokens = Tokenizer::tokenize_string(contents);
    auto ast = Parser::parse_to_ast(filename, tokens, _types);
    return Generator::generate_bytecode(ast->root, _types, _methods);
}

}
