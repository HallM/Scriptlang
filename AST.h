#pragma once

#include <memory>
#include <optional>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <variant>
#include <vector>

#include "VMFFI.h"

namespace MattScript {
namespace Ast {

// Tokenizer takes file contents (string) and produces list of tokens
// Parser takes tokens and produces a typed AST along with some metadata (what methods/consts exist)
// Compiler takes the AST
//    1. Verify types
//    2. Expand generics
//    3. optimize if necessary
//    4. generate unlinked bytecode
//    5. generate a to-link table
// Linker phase will link the addresses in bytecode to produce a Program

struct ImportedMethod {
    std::vector<std::string> scopes;
    std::string name;
    std::shared_ptr<IRunnable> runnable;
    std::string type;
    std::type_index ret_type;
    std::vector<std::type_index> param_types;
};

struct Node;

struct ConstS32 {
    int num;
};

struct ConstF32 {
    float num;
};

struct ConstBool {
    bool value;
};

enum class BinaryOps {
    // int/float types
    Add,
    Subtract,
    Multiply,
    Divide,
    Modulo,

    // int types only
    BitShiftLeft,
    BitShiftRight,
    BitAnd,
    BitOr,
    BitXor,

    // bool types
    And,
    Or,

    // int/float/bool
    Eq,
    NotEq,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
};

struct BinaryOperation {
    BinaryOps op;
    std::shared_ptr<Node> lhs;
    std::shared_ptr<Node> rhs;
};

enum class UnaryOps {
    // int/float types
    Negate,

    // int types only
    BitNot,

    // bool types
    Not,
};

struct UnaryOperation {
    UnaryOps op;
    std::shared_ptr<Node> value;
};

struct SetOperation {
    // no op set means a standard assignment.
    std::optional<BinaryOps> op;
    std::shared_ptr<Node> lhs;
    std::shared_ptr<Node> rhs;
};

struct Identifier {
    std::vector<std::string> scopes;
    std::string name;
};

struct AccessMember {
    std::shared_ptr<Node> container;
    std::shared_ptr<Node> member;
};

struct VariableDeclaration {
    std::string name;
    std::string type;
    Types::Mutable is_mutable;
};

struct Block {
    std::vector<std::shared_ptr<Node>> nodes;
};
struct GlobalBlock {
    std::vector<std::shared_ptr<Node>> nodes;
};

struct IfStmt {
    std::shared_ptr<Node> condition;
    std::shared_ptr<Node> then;
    std::optional<std::shared_ptr<Node>> otherwise;
};
struct DoWhile {
    std::shared_ptr<Node> block;
    std::shared_ptr<Node> condition;
};

struct MethodDeclaration {
    std::vector<std::string> scopes;
    std::string name;
    std::string type;
};
struct MethodDefinition {
    std::vector<std::string> scopes;
    std::string name;
    std::string type;
    std::vector<std::string> param_names;
    std::shared_ptr<Node> node;
};
struct ReturnValue {
    std::optional<std::shared_ptr<Node>> value;
};

struct CallParam {
    // TODO: named params?
    std::shared_ptr<Node> value;
};
struct MethodCall {
    std::shared_ptr<Node> callable;
    std::vector<std::shared_ptr<Node>> params;
};

struct CppTypeid {
    std::string type;
};

struct UnknownAST{};

struct Node {
    std::variant<
        UnknownAST,
        ConstS32,
        ConstF32,
        ConstBool,
        BinaryOperation,
        UnaryOperation,
        SetOperation,
        Identifier,
        AccessMember,
        VariableDeclaration,
        GlobalBlock,
        Block,
        IfStmt,
        DoWhile,
        MethodDeclaration,
        MethodDefinition,
        ReturnValue,
        CallParam,
        MethodCall,
        CppTypeid
    > data;
};

struct FileNode {
    std::string filename;
    std::shared_ptr<Node> root;
    std::vector<std::shared_ptr<Node>> nodes;
};

} // Ast
} // MattScript
