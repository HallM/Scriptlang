#include "Parser.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <variant>
#include <vector>

#include "AST.h"
#include "Tokens.h"
#include "VectorView.h"

namespace MattScript {
namespace Parser {

typedef std::shared_ptr<Ast::FileNode> FileNodePtr;
typedef VectorView<std::shared_ptr<Tokens::Token>> TokenStream;

struct node_return {
    std::shared_ptr<Ast::Node> node;
};

struct parser_wip {
    Types::TypeTable& types;
};

template<typename T>
void
eat_token(TokenStream& tokens) {
    for (size_t i = 0; i < tokens.size(); i++) {
        std::shared_ptr<Tokens::Token> token = tokens.peak();
        if (!std::holds_alternative<T>(token->data)) {
            return;
        }
        tokens.pull_front();
    }
}

template<typename T>
bool always_true(T v) { return true; }

template<typename T>
std::optional<T>
token_if(TokenStream& tokens, std::function<bool(T)> cond = &always_true<T>) {
    Tokens::TokenData d = tokens.peak()->data;
    if (std::holds_alternative<T>(d) && cond(std::get<T>(d))) {
        return std::get<T>(tokens.pull_front()->data);
    }
    return {};
}

template<typename T>
std::optional<T>
peek_if(TokenStream& tokens, std::function<bool(T)> cond = &always_true<T>) {
    Tokens::TokenData d = tokens.peak()->data;
    if (std::holds_alternative<T>(d) && cond(std::get<T>(d))) {
        return std::get<T>(d);
    }
    return {};
}

template<typename T>
bool
is_next(TokenStream& tokens, std::function<bool(T)> cond = &always_true<T>) {
    return peek_if<T>(tokens, cond).has_value();
}

void
eat_whitespace(TokenStream& tokens) {
    return eat_token<Tokens::SpaceToken>(tokens);
}

void
eat_newlines(TokenStream& tokens) {
    return eat_token<Tokens::NewlineToken>(tokens);
}

void
eat_anyspace(TokenStream& tokens) {
    for (size_t i = 0; i < tokens.size(); i++) {
        std::shared_ptr<Tokens::Token> token = tokens.peak();
        if (!std::holds_alternative<Tokens::SpaceToken>(token->data) && !std::holds_alternative<Tokens::NewlineToken>(token->data)) {
            return;
        }
        tokens.pull_front();
    }
}

bool
is_operator(std::string want, Tokens::OperatorToken t) {
    return want == t.oper;
}
bool
is_keyword(std::string want, Tokens::KeywordToken t) {
    return want == t.keyword;
}

// some forward decls
node_return parse_block(FileNodePtr root, TokenStream& tokens, bool is_global, parser_wip& wip);

/*
node_return
parse_expressionstmt(FileNodePtr root, TokenStream& tokens) {
}
*/

bool
is_simple_assignment(Tokens::OperatorToken token) {
    return token.oper == "=";
}
bool
is_compound_assignment(Tokens::OperatorToken token) {
    if (token.oper == "+=") { return true; }
    if (token.oper == "-=") { return true; }
    if (token.oper == "*=") { return true; }
    if (token.oper == "/=") { return true; }
    if (token.oper == "%=") { return true; }

    return false;
}

std::pair<int,int>
binding_power(Tokens::OperatorToken token) {
    if (token.oper == "+" || token.oper == "-") {
        return {23, 24};
    }
    else if (token.oper == "*" || token.oper == "/" || token.oper == "%") {
        return {25, 26};
    }

    else if (token.oper == "<" || token.oper == "<=" || token.oper == ">" || token.oper == ">=") {
        return {17, 18};
    }
    else if (token.oper == "==" || token.oper == "!=") {
        return {15, 16};
    }

    else if (is_simple_assignment(token) || is_compound_assignment(token)) {
        return {4, 3};
    }

    else if (token.oper == ".") {
        return {31, 32};
    }

    return {-1, -1};
}
Ast::BinaryOps
binary_operator(Tokens::OperatorToken token) {
    std::unordered_map<std::string, Ast::BinaryOps> ops = {
        {"+", Ast::BinaryOps::Add},
        {"-", Ast::BinaryOps::Subtract},
        {"*", Ast::BinaryOps::Multiply},
        {"/", Ast::BinaryOps::Divide},
        {"%", Ast::BinaryOps::Modulo},

        {"<", Ast::BinaryOps::Less},
        {"<=", Ast::BinaryOps::LessEqual},
        {">", Ast::BinaryOps::Greater},
        {">=", Ast::BinaryOps::GreaterEqual},
        {"==", Ast::BinaryOps::Eq},
        {"!=", Ast::BinaryOps::NotEq},

        {"+=", Ast::BinaryOps::Add},
        {"-=", Ast::BinaryOps::Subtract},
        {"*=", Ast::BinaryOps::Multiply},
        {"/=", Ast::BinaryOps::Divide},
        {"%=", Ast::BinaryOps::Modulo},
    };

    return ops.at(token.oper);
}

struct typeret {
    std::string type_name;
    bool is_mutable;
};

typeret
parse_type(FileNodePtr root, TokenStream& tokens, parser_wip& wip) {
    bool is_ref = false;
    bool is_mut = false;

    if (token_if<Tokens::KeywordToken>(tokens, std::bind_front(is_keyword, "mut"))) {
        is_mut = true;
    }
    if (token_if<Tokens::KeywordToken>(tokens, std::bind_front(is_keyword, "ref"))) {
        is_ref = true;
    }

    auto id = std::get<Tokens::IdentifierToken>(tokens.pull_front()->data).identifier;
    if (is_ref) {
        id = "ref " + id;
    }
    return typeret{ id, is_mut };
}

node_return
parse_identifier(FileNodePtr root, TokenStream& tokens, parser_wip& wip) {
    auto id = std::get<Tokens::IdentifierToken>(tokens.pull_front()->data).identifier;

    std::shared_ptr<Ast::Node> node = std::make_shared<Ast::Node>();
    node->data = Ast::Identifier { id };
    return {node};
}

node_return
parse_const(FileNodePtr root, TokenStream& tokens, parser_wip& wip) {
    if (auto v = token_if<Tokens::S32Token>(tokens)) {
        std::shared_ptr<Ast::Node> node = std::make_shared<Ast::Node>();
        node->data = Ast::ConstS32 { v.value().number };
        return {node};
    }
    if (auto v = token_if<Tokens::F32Token>(tokens)) {
        std::shared_ptr<Ast::Node> node = std::make_shared<Ast::Node>();
        node->data = Ast::ConstF32 { v.value().number };
        return {node};
    }
    if (auto v = token_if<Tokens::BoolToken>(tokens)) {
        std::shared_ptr<Ast::Node> node = std::make_shared<Ast::Node>();
        node->data = Ast::ConstBool { v.value().value };
        return {node};
    }

    throw "unknown constant";
}

node_return
parse_expression(FileNodePtr root, TokenStream& tokens, parser_wip& wip, int min_power) {
    eat_whitespace(tokens);

    std::cout << "Expr\n";
    std::shared_ptr<Ast::Node> lhs;
    if (is_next<Tokens::IdentifierToken>(tokens)) {
        std::cout << "identifier\n";
        lhs = parse_identifier(root, tokens, wip).node;
    }
    else if (is_next<Tokens::S32Token>(tokens) || is_next<Tokens::F32Token>(tokens) || is_next<Tokens::BoolToken>(tokens)) {
        std::cout << "const\n";
        lhs = parse_const(root, tokens, wip).node;
    }
    else if (is_next<Tokens::OperatorToken>(tokens, std::bind_front(is_operator, "("))) {
        tokens.pull_front();
        std::cout << "subexpr\n";
        lhs = parse_expression(root, tokens, wip, 0).node;
        if (!token_if<Tokens::OperatorToken>(tokens, std::bind_front(is_operator, ")"))) {
            throw "Missing ) to end the expression";
        }
    }
    else {
        std::cout << tokens.size() << " unknown\n";
        throw "invalid expression";
    }

    while (!tokens.empty()) {
        eat_whitespace(tokens);

        if (token_if<Tokens::OperatorToken>(tokens, std::bind_front(is_operator, "("))) {
            std::cout << "Call method\n";
            std::vector<std::shared_ptr<Ast::Node>> params;

            if (!token_if<Tokens::OperatorToken>(tokens, std::bind_front(is_operator, ")"))) {
                while (!tokens.empty()) {
                    std::cout << "get param\n";
                    auto paramexpr = parse_expression(root, tokens, wip, 0).node;
                    std::shared_ptr<Ast::Node> pnode = std::make_shared<Ast::Node>();
                    pnode->data = Ast::CallParam { paramexpr };
                    params.push_back(pnode);
                
                    if (!token_if<Tokens::OperatorToken>(tokens, std::bind_front(is_operator, ","))) {
                        break;
                    }
                }
                if (!token_if<Tokens::OperatorToken>(tokens, std::bind_front(is_operator, ")"))) {
                    throw "function call did not end in )";
                }
            }

            std::shared_ptr<Ast::Node> callnode = std::make_shared<Ast::Node>();
            callnode->data = Ast::MethodCall { lhs, params };
            lhs = callnode;
        }

        auto maybe = peek_if<Tokens::OperatorToken>(tokens);
        if (!maybe) {
            break;
        }
        auto token = maybe.value();

        auto [lbp, rpb] = binding_power(token);
        if (lbp < min_power) {
            break;
        }
        tokens.pull_front();
        eat_whitespace(tokens);

        std::shared_ptr<Ast::Node> rhs = parse_expression(root, tokens, wip, rpb).node;
        eat_whitespace(tokens);

        std::shared_ptr<Ast::Node> node = std::make_shared<Ast::Node>();
        if (token.oper == ".") {
            node->data = Ast::AccessMember { lhs, rhs };
        }
        else if (is_simple_assignment(token)) {
            node->data = Ast::SetOperation { {}, lhs, rhs };
        }
        else if (is_compound_assignment(token)) {
            node->data = Ast::SetOperation { binary_operator(token), lhs, rhs };
        }
        else {
            node->data = Ast::BinaryOperation { binary_operator(token), lhs, rhs };
        }
        lhs = node;
    }
    eat_whitespace(tokens);
    return {lhs};
}

node_return
parse_if(FileNodePtr root, TokenStream& tokens, parser_wip& wip) {
    // if condexpr block (else? block|if)
    auto condition = parse_expression(root, tokens, wip, 0).node;
    if (!token_if<Tokens::OperatorToken>(tokens, std::bind_front(is_operator, "{"))) {
        throw "Syntax error, expected block for if statement";
    }
    auto then = parse_block(root, tokens, false, wip).node;

    std::optional<std::shared_ptr<Ast::Node>> elsevalue = {};
    if (token_if<Tokens::KeywordToken>(tokens, std::bind_front(is_keyword, "else"))) {
        if (token_if<Tokens::KeywordToken>(tokens, std::bind_front(is_keyword, "if"))) {
            elsevalue = parse_if(root, tokens, wip).node;
        }
        else if (token_if<Tokens::OperatorToken>(tokens, std::bind_front(is_operator, "{"))) {
            elsevalue = parse_block(root, tokens, false, wip).node;
        }
        else {
            throw "Invalid else expression";
        }
    }

    std::shared_ptr<Ast::Node> node = std::make_shared<Ast::Node>();
    node->data = Ast::IfStmt { condition, then, elsevalue };
    return {node};
}

node_return
parse_for(FileNodePtr root, TokenStream& tokens, parser_wip& wip) {
    // for expr; expr; expr {}
    // for expr { maybe later

    auto start = parse_expression(root, tokens, wip, 0).node;

    if (token_if<Tokens::OperatorToken>(tokens, std::bind_front(is_operator, ";"))) {
        auto condition = parse_expression(root, tokens, wip, 0).node;

        if (!token_if<Tokens::OperatorToken>(tokens, std::bind_front(is_operator, ";"))) {
            throw "Syntax error, expected ; between init, condition, and iteration of a for loop";
        }

        auto iteration = parse_expression(root, tokens, wip, 0).node;

        if (!token_if<Tokens::OperatorToken>(tokens, std::bind_front(is_operator, "{"))) {
            throw "Syntax error, expected block for for statement";
        }
        auto dothething = parse_block(root, tokens, false, wip).node;
        std::get<Ast::Block>(dothething->data).nodes.push_back(iteration);

        std::shared_ptr<Ast::Node> dowhile = std::make_shared<Ast::Node>();
        dowhile->data = Ast::DoWhile { dothething, condition };
        
        std::shared_ptr<Ast::Node> startcond = std::make_shared<Ast::Node>();
        startcond->data = Ast::IfStmt { condition, dowhile, {} };

        std::shared_ptr<Ast::Node> surround = std::make_shared<Ast::Node>();
        surround->data = Ast::Block { { start, startcond } };

        return {surround};
    }

    throw "for init; condition; iteration is the only supported loop.";
}

node_return
parse_let(FileNodePtr root, TokenStream& tokens, parser_wip& wip) {
    auto id = std::get<Tokens::IdentifierToken>(tokens.pull_front()->data).identifier;

    if (!token_if<Tokens::OperatorToken>(tokens, std::bind_front(is_operator, ":"))) {
        throw "Type of a variable must be specified";
    }
    auto type = parse_type(root, tokens, wip);

    std::shared_ptr<Ast::Node> node = std::make_shared<Ast::Node>();
    node->data = Ast::VariableDeclaration { id, type.type_name, type.is_mutable };

    if (token_if<Tokens::OperatorToken>(tokens, std::bind_front(is_operator, "="))) {
        auto expr = parse_expression(root, tokens, wip, 0).node;

        std::shared_ptr<Ast::Node> setnode = std::make_shared<Ast::Node>();
        setnode->data = Ast::SetOperation { {}, node, expr };
        node = setnode;
    }

    return {node};
}

node_return
parse_return(FileNodePtr root, TokenStream& tokens, parser_wip& wip) {
    eat_whitespace(tokens);
    std::shared_ptr<Ast::Node> node = std::make_shared<Ast::Node>();
    if (token_if<Tokens::KeywordToken>(tokens, std::bind_front(is_keyword, "void"))) {
        node->data = Ast::ReturnValue { {} };
    }
    else {
        std::shared_ptr<Ast::Node> ret = parse_expression(root, tokens, wip, 0).node;
        node->data = Ast::ReturnValue { ret };
    }
    return {node};
}

node_return
parse_method_decl(FileNodePtr root, TokenStream& tokens, parser_wip& wip) {
    //"fn", fn, ws, identifier, ws?, "(", param list ")" ws? ":" NL parse block
    eat_whitespace(tokens);

    auto ident = std::get<Tokens::IdentifierToken>(tokens.pull_front()->data).identifier;
    eat_whitespace(tokens);

    if (!token_if<Tokens::OperatorToken>(tokens, std::bind_front(is_operator, "("))) {
        throw "Syntax error, expected ( after identifier for function decl/def";
    }
    eat_anyspace(tokens);

    std::vector<std::string> param_names;
    std::vector<Types::MethodTypeParameter> param_types;

    auto param_token = token_if<Tokens::IdentifierToken>(tokens);
    while (param_token) {
        auto param_name = param_token.value().identifier;
        eat_whitespace(tokens);
        param_names.push_back(param_name);
        std::cout << "Read param " << param_name << "\n";

        if (!token_if<Tokens::OperatorToken>(tokens, std::bind_front(is_operator, ":"))) {
            throw "Syntax error, expected : between parameter name and type";
        }
        eat_whitespace(tokens);

        auto param_type = parse_type(root, tokens, wip);
        eat_whitespace(tokens);
        param_types.push_back(Types::MethodTypeParameter{param_type.type_name, param_type.is_mutable});

        if (!token_if<Tokens::OperatorToken>(tokens, std::bind_front(is_operator, ","))) {
            break;
        }
        eat_whitespace(tokens);

        param_token = token_if<Tokens::IdentifierToken>(tokens);
    }
    eat_whitespace(tokens);

    if (!token_if<Tokens::OperatorToken>(tokens, std::bind_front(is_operator, ")"))) {
        throw "Syntax error, expected ) after parameter list for function decl/def";
    }
    eat_whitespace(tokens);

    typeret ret_type = {"void", false};
    if (token_if<Tokens::OperatorToken>(tokens, std::bind_front(is_operator, ":"))) {
        eat_whitespace(tokens);
        ret_type = parse_type(root, tokens, wip);

        eat_whitespace(tokens);
    }
    eat_whitespace(tokens);

    std::string method_type = wip.types.add_method(ret_type.type_name, ret_type.is_mutable, param_types);

    if (!token_if<Tokens::OperatorToken>(tokens, std::bind_front(is_operator, "{"))) {
        throw "Syntax error, expected block to start method";
    }
    eat_anyspace(tokens);

    auto block = parse_block(root, tokens, false, wip);

    std::shared_ptr<Ast::Node> node = std::make_shared<Ast::Node>();
    node->data = Ast::MethodDefinition{
        ident,
        method_type,
        param_names,
        block.node
    };

    return {node};
}

node_return
parse_statement(FileNodePtr root, TokenStream& tokens, parser_wip& wip) {
    std::cout << "Start statement" << tokens.size() << "\n";

    // keyword:
    //"if", = if, WS, expr, WS?, "{", NL parse block.
    //"else", else, WS?, ":", NL parse block. verify above to append to if
    //"fn", fn, ws, identifier, ws?, "(", param list ")" ws? ":" NL parse block
    //"for", for, ws, identifier or destructor, ws, "in", ws, expr, ws? ":" NL parse block
    //"in" -> invalid
    // "(" could allow multiline statements

    // identifier -> could be assignment, could be a lot of things

    eat_newlines(tokens);

    if (token_if<Tokens::KeywordToken>(tokens, std::bind_front(is_keyword, "fn"))) {
        return parse_method_decl(root, tokens, wip);
    }
    else if (token_if<Tokens::KeywordToken>(tokens, std::bind_front(is_keyword, "return"))) {
        return parse_return(root, tokens, wip);
    }
    else if (token_if<Tokens::KeywordToken>(tokens, std::bind_front(is_keyword, "if"))) {
        return parse_if(root, tokens, wip);
    }
    else if (token_if<Tokens::KeywordToken>(tokens, std::bind_front(is_keyword, "for"))) {
        return parse_for(root, tokens, wip);
    }
    else if (token_if<Tokens::KeywordToken>(tokens, std::bind_front(is_keyword, "let"))) {
        return parse_let(root, tokens, wip);
    }
    else {
        return parse_expression(root, tokens, wip, 0);
    }

    throw "Unknown statement";
}

node_return
parse_block(FileNodePtr root, TokenStream& tokens, bool is_global, parser_wip& wip) {
    std::vector<std::shared_ptr<Ast::Node>> fndefs;
    std::vector<std::shared_ptr<Ast::Node>> statements;

    std::cout << "Start block " << tokens.size() << "\n";
    while (!tokens.empty()) {
        // TODO need to validate that blocks end
        if (peek_if<Tokens::OperatorToken>(tokens, std::bind_front(is_operator, "}"))) {
            break;
        }

        auto statement = parse_statement(root, tokens, wip);

        if (std::holds_alternative<Ast::MethodDefinition>(statement.node->data)) {
            statements.push_back(statement.node);

            std::shared_ptr<Ast::Node> declnode = std::make_shared<Ast::Node>();
            declnode->data = Ast::MethodDeclaration{
                std::get<Ast::MethodDefinition>(statement.node->data).name,
                std::get<Ast::MethodDefinition>(statement.node->data).type
            };
            fndefs.push_back(declnode);
        }
        else {
            std::cout << "Adding a statement";
            statements.push_back(statement.node);
        }
    }

    if (!is_global) {
        if (!token_if<Tokens::OperatorToken>(tokens, std::bind_front(is_operator, "}"))) {
            throw "Block must end in a }";
        }
    }

    std::vector<std::shared_ptr<Ast::Node>> all;
    std::copy(fndefs.begin(), fndefs.end(), std::back_inserter(all));
    std::copy(statements.begin(), statements.end(), std::back_inserter(all));

    std::cout << "Block has " << all.size() << "\n";

    std::shared_ptr<Ast::Node> n;
    if (is_global) {
        n = std::make_shared<Ast::Node>();
        n->data = Ast::GlobalBlock{ all };
    }
    else {
        n = std::make_shared<Ast::Node>();
        n->data = Ast::Block{ all };
    }

    return {n};
}

std::shared_ptr<Ast::FileNode>
parse_to_ast(std::string file_name, std::vector<std::shared_ptr<Tokens::Token>> tokens, Types::TypeTable& types) {
    TokenStream view = VectorView<std::shared_ptr<Tokens::Token>>(tokens);
    parser_wip wip = {
        types
    };

    FileNodePtr file = std::make_shared<Ast::FileNode>();
    file->filename = file_name;

    auto ret = parse_block(file, view, true, wip);
    if (!view.empty()) {
        throw "Failed to parse entire document";
    }
    file->root = ret.node;

    return file;
}

}
}
