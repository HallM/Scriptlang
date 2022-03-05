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

Compiler::Compiler() {}

std::shared_ptr<Program>
Compiler::compile(std::string filename, std::string contents) {
    auto tokens = tokenize_string(contents);
    auto ast = parse_to_ast("test.wut", tokens, _types);
    return generate_bytecode(ast->root, _types, _methods);
}
