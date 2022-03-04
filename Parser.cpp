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

typedef std::shared_ptr<FileNode> FileNodePtr;
typedef VectorView<std::shared_ptr<Token>> Tokens;

struct node_return {
    std::shared_ptr<Node> node;
};

struct parser_wip {
    TypeTable& types;
};

template<typename T>
void
eat_token(Tokens& tokens) {
    for (size_t i = 0; i < tokens.size(); i++) {
        std::shared_ptr<Token> token = tokens.peak();
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
token_if(Tokens& tokens, std::function<bool(T)> cond = &always_true<T>) {
    TokenData d = tokens.peak()->data;
    if (std::holds_alternative<T>(d) && cond(std::get<T>(d))) {
        return std::get<T>(tokens.pull_front()->data);
    }
    return {};
}

template<typename T>
std::optional<T>
peek_if(Tokens& tokens, std::function<bool(T)> cond = &always_true<T>) {
    TokenData d = tokens.peak()->data;
    if (std::holds_alternative<T>(d) && cond(std::get<T>(d))) {
        return std::get<T>(d);
    }
    return {};
}

template<typename T>
bool
is_next(Tokens& tokens, std::function<bool(T)> cond = &always_true<T>) {
    return peek_if<T>(tokens, cond).has_value();
}

void
eat_whitespace(Tokens& tokens) {
    return eat_token<SpaceToken>(tokens);
}

void
eat_newlines(Tokens& tokens) {
    return eat_token<NewlineToken>(tokens);
}

void
eat_anyspace(Tokens& tokens) {
    for (size_t i = 0; i < tokens.size(); i++) {
        std::shared_ptr<Token> token = tokens.peak();
        if (!std::holds_alternative<SpaceToken>(token->data) && !std::holds_alternative<NewlineToken>(token->data)) {
            return;
        }
        tokens.pull_front();
    }
}

bool
is_operator(std::string want, OperatorToken t) {
    return want == t.oper;
}
bool
is_keyword(std::string want, KeywordToken t) {
    return want == t.keyword;
}

// some forward decls
node_return parse_block(FileNodePtr root, Tokens& tokens, bool is_global, parser_wip& wip);

/*
node_return
parse_expressionstmt(FileNodePtr root, Tokens& tokens) {
}
*/

std::pair<int,int>
binding_power(OperatorToken token) {
    if (token.oper == "+" || token.oper == "-") {
        return {3, 4};
    }
    else if (token.oper == "*" || token.oper == "/") {
        return {5, 6};
    }
    else if (token.oper == "=" || token.oper == "+=") {
        return {2, 1};
    }
    return {-1, -1};
}
BinaryOps
binary_operator(OperatorToken token) {
    std::unordered_map<std::string, BinaryOps> ops = {
        {"+", BinaryOps::Add},
        {"-", BinaryOps::Subtract},
        {"*", BinaryOps::Multiply},
        {"/", BinaryOps::Divide},
        {"+=", BinaryOps::Add},
    };

    return ops.at(token.oper);
}
bool
is_simple_assignment(OperatorToken token) {
    return token.oper == "=";
}
bool
is_compound_assignment(OperatorToken token) {
    return token.oper[token.oper.length() - 1] == '=';
}

node_return
parse_identifier(FileNodePtr root, Tokens& tokens, parser_wip& wip) {
    auto id = std::get<IdentifierToken>(tokens.pull_front()->data).identifier;

    std::shared_ptr<Node> node = std::make_shared<Node>();
    node->data = Identifier { id };
    return {node};
}

node_return
parse_const(FileNodePtr root, Tokens& tokens, parser_wip& wip) {
    if (auto v = token_if<S32Token>(tokens)) {
        std::shared_ptr<Node> node = std::make_shared<Node>();
        node->data = ConstS32 { v.value().number };
        return {node};
    }
    if (auto v = token_if<F32Token>(tokens)) {
        std::shared_ptr<Node> node = std::make_shared<Node>();
        node->data = ConstF32 { v.value().number };
        return {node};
    }
    if (auto v = token_if<BoolToken>(tokens)) {
        std::shared_ptr<Node> node = std::make_shared<Node>();
        node->data = ConstBool { v.value().value };
        return {node};
    }

    throw "unknown constant";
}

node_return
parse_expression(FileNodePtr root, Tokens& tokens, parser_wip& wip, int min_power) {
    eat_whitespace(tokens);

    std::cout << "Expr\n";
    std::shared_ptr<Node> lhs;
    if (is_next<IdentifierToken>(tokens)) {
        std::cout << "identifier\n";
        lhs = parse_identifier(root, tokens, wip).node;
    }
    else if (is_next<S32Token>(tokens) || is_next<F32Token>(tokens) || is_next<BoolToken>(tokens)) {
        std::cout << "const\n";
        lhs = parse_const(root, tokens, wip).node;
    }
    else if (is_next<OperatorToken>(tokens, std::bind_front(is_operator, "("))) {
        tokens.pull_front();
        std::cout << "subexpr\n";
        lhs = parse_expression(root, tokens, wip, 0).node;
        if (!token_if<OperatorToken>(tokens, std::bind_front(is_operator, ")"))) {
            throw "Missing ) to end the expression";
        }
    }
    else {
        std::cout << tokens.size() << " unknown\n";
        throw "invalid expression";
    }

    while (!tokens.empty()) {
        eat_whitespace(tokens);
        auto token = peek_if<OperatorToken>(tokens);
        if (!token) {
            break;
        }

        auto [lbp, rpb] = binding_power(token.value());
        if (lbp < min_power) {
            break;
        }
        tokens.pull_front();
        eat_whitespace(tokens);

        std::shared_ptr<Node> rhs = parse_expression(root, tokens, wip, rpb).node;
        eat_whitespace(tokens);

        std::shared_ptr<Node> node = std::make_shared<Node>();
        if (is_simple_assignment(token.value())) {
            node->data = SetOperation { {}, lhs, rhs };
        }
        else if (is_compound_assignment(token.value())) {
            node->data = SetOperation { binary_operator(token.value()), lhs, rhs };
        }
        else {
            node->data = BinaryOperation { binary_operator(token.value()), lhs, rhs };
        }
        lhs = node;
    }
    eat_whitespace(tokens);
    return {lhs};
}

node_return
parse_return(FileNodePtr root, Tokens& tokens, parser_wip& wip) {
    eat_whitespace(tokens);
    std::shared_ptr<Node> ret = parse_expression(root, tokens, wip, 0).node;
    std::shared_ptr<Node> node = std::make_shared<Node>();
    node->data = ReturnValue { ret };
    return {node};
}

node_return
parse_method_decl(FileNodePtr root, Tokens& tokens, parser_wip& wip) {
    //"fn", fn, ws, identifier, ws?, "(", param list ")" ws? ":" NL parse block
    eat_whitespace(tokens);

    auto ident = std::get<IdentifierToken>(tokens.pull_front()->data).identifier;
    eat_whitespace(tokens);

    if (!token_if<OperatorToken>(tokens, std::bind_front(is_operator, "("))) {
        throw "Syntax error, expected ( after identifier for function decl/def";
    }
    eat_anyspace(tokens);

    std::vector<std::string> param_names;
    std::vector<MethodTypeParameter> param_types;

    auto param_token = token_if<IdentifierToken>(tokens);
    while (param_token) {
        auto param_name = param_token.value().identifier;
        eat_whitespace(tokens);
        param_names.push_back(param_name);
        std::cout << "Read param " << param_name << "\n";

        if (!token_if<OperatorToken>(tokens, std::bind_front(is_operator, ":"))) {
            throw "Syntax error, expected : between parameter name and type";
        }
        eat_whitespace(tokens);

        auto param_type = std::get<IdentifierToken>(tokens.pull_front()->data).identifier;
        eat_whitespace(tokens);
        param_types.push_back(MethodTypeParameter{param_type});

        if (!token_if<OperatorToken>(tokens, std::bind_front(is_operator, ","))) {
            break;
        }
        eat_whitespace(tokens);

        param_token = token_if<IdentifierToken>(tokens);
    }
    eat_whitespace(tokens);

    if (!token_if<OperatorToken>(tokens, std::bind_front(is_operator, ")"))) {
        throw "Syntax error, expected ) after parameter list for function decl/def";
    }
    eat_whitespace(tokens);

    std::string ret_type = "void";
    if (token_if<OperatorToken>(tokens, std::bind_front(is_operator, ":"))) {
        eat_whitespace(tokens);
        ret_type = std::get<IdentifierToken>(tokens.pull_front()->data).identifier;

        eat_whitespace(tokens);
    }
    eat_whitespace(tokens);

    std::string method_type = wip.types.add_method(ret_type, param_types);

    if (!token_if<OperatorToken>(tokens, std::bind_front(is_operator, "{"))) {
        throw "Syntax error, expected block to start method";
    }
    eat_anyspace(tokens);

    auto block = parse_block(root, tokens, false, wip);

    std::shared_ptr<Node> node = std::make_shared<Node>();
    node->data = MethodDefinition{
        ident,
        method_type,
        param_names,
        block.node
    };

    return {node};
}

node_return
parse_statement(FileNodePtr root, Tokens& tokens, parser_wip& wip) {
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

    if (token_if<KeywordToken>(tokens, std::bind_front(is_keyword, "fn"))) {
        return parse_method_decl(root, tokens, wip);
    }
    else if (token_if<KeywordToken>(tokens, std::bind_front(is_keyword, "return"))) {
        return parse_return(root, tokens, wip);
    }
    else {
        return parse_expression(root, tokens, wip, 0);
    }

    throw "Unknown statement";
}

node_return
parse_block(FileNodePtr root, Tokens& tokens, bool is_global, parser_wip& wip) {
    std::vector<std::shared_ptr<Node>> vardefs;
    std::vector<std::shared_ptr<Node>> fndefs;
    std::vector<std::shared_ptr<Node>> statements;

    std::cout << "Start block " << tokens.size() << "\n";
    while (!tokens.empty()) {
        // TODO need to validate that blocks end
        if (peek_if<OperatorToken>(tokens, std::bind_front(is_operator, "}"))) {
            break;
        }

        auto statement = parse_statement(root, tokens, wip);

        if (std::holds_alternative<VariableDeclaration>(statement.node->data)) {
            vardefs.push_back(statement.node);
        }
        else if (std::holds_alternative<MethodDefinition>(statement.node->data)) {
            statements.push_back(statement.node);

            std::shared_ptr<Node> declnode = std::make_shared<Node>();
            std::cout << "Added method " << std::get<MethodDefinition>(statement.node->data).name << " : " << std::get<MethodDefinition>(statement.node->data).type << "\n";
            declnode->data = MethodDeclaration{
                std::get<MethodDefinition>(statement.node->data).name,
                std::get<MethodDefinition>(statement.node->data).type
            };
            fndefs.push_back(declnode);
        }
        else {
            std::cout << "Adding a statement";
            statements.push_back(statement.node);
        }
    }

    if (!is_global) {
        if (!token_if<OperatorToken>(tokens, std::bind_front(is_operator, "}"))) {
            throw "Block must end in a }";
        }
    }

    std::vector<std::shared_ptr<Node>> all;
    std::copy(vardefs.begin(), vardefs.end(), std::back_inserter(all));
    std::copy(fndefs.begin(), fndefs.end(), std::back_inserter(all));
    std::copy(statements.begin(), statements.end(), std::back_inserter(all));

    std::cout << "Block has " << all.size() << "\n";

    std::shared_ptr<Node> n;
    if (is_global) {
        n = std::make_shared<Node>();
        n->data = GlobalBlock{ all };
    }
    else {
        n = std::make_shared<Node>();
        n->data = Block{ all };
    }

    return {n};
}

std::shared_ptr<FileNode>
parse_to_ast(std::string file_name, std::vector<std::shared_ptr<Token>> tokens, TypeTable& types) {
    Tokens view = VectorView<std::shared_ptr<Token>>(tokens);
    parser_wip wip = {
        types
    };

    FileNodePtr file = std::make_shared<FileNode>();
    file->filename = file_name;

    auto ret = parse_block(file, view, true, wip);
    if (!view.empty()) {
        throw "Failed to parse entire document";
    }
    file->root = ret.node;

    return file;
}
