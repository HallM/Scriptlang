#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

// Tokenizer takes file contents (string) and produces list of tokens
// Parser takes tokens and produces a typed AST along with some metadata (what methods/consts exist)
// Compiler takes the AST
//    1. Verify types
//    2. Expand generics
//    3. optimize if necessary
//    4. generate unlinked bytecode
//    5. generate a to-link table
// Linker phase will link the addresses in bytecode to produce a Program

enum class PrimitiveType: unsigned int {
    empty,
    boolean,
    s32,
    f32,
};
struct TupleTypeValue {
    size_t offset;
    std::string type;
};
struct TupleType {
    std::vector<TupleTypeValue> values;
};

struct EnumTypeNamedValue {
    std::string name;
    std::string type;
};
struct EnumType {
    std::vector<EnumTypeNamedValue> values;
};

struct StructTypeMember {
    std::string name;
    size_t offset;
    std::string type;
};
struct StructType {
    std::unordered_map<std::string, StructTypeMember> members;
};

struct MethodTypeParameter {
    std::string type;
};
struct MethodType {
    std::string return_type;
    std::vector<MethodTypeParameter> parameters;
};

struct TypeInfo {
    std::string name;
    size_t size;
    std::variant<PrimitiveType, StructType, TupleType, EnumType, MethodType> type;
    std::unordered_map<std::string, std::string> methods;
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
    Node* lhs;
    Node* rhs;
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
    Node* value;
};

struct SetOperation {
    // no op set means a standard assignment.
    std::optional<BinaryOps> op;
    Node* lhs;
    Node* rhs;
};

struct Identifier {
    std::string name;
};

struct AccessMember {
    Node* container;
    Node* member;
};

struct VariableDeclaration {
    std::string name;
    std::string type;
};

struct Block {
    std::vector<Node*> nodes;
};
struct GlobalBlock {
    std::vector<Node*> nodes;
};

struct IfStmt {
    Node* condition;
    Node* then;
    std::optional<Node*> otherwise;
};
struct DoWhile {
    Node* block;
    Node* condition;
};

struct MethodDeclaration {
    std::string name;
    std::string type;
};
struct MethodDefinition {
    std::string name;
    std::vector<std::string> param_names;
    Node* node;
};
struct ReturnValue {
    std::optional<Node*> value;
};

struct CallParam {
    // TODO: named params?
    Node* value;
};
struct MethodCall {
    Node* callable;
    std::vector<Node*> params;
};

struct UnknownAST{};

struct Node {
    std::variant<
        UnknownAST,
        ConstS32,
        ConstF32,
        ConstBool,
        BinaryOperation,
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
        MethodCall
    > data;
};
