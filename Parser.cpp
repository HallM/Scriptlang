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
std::optional<T>
token_if(Tokens& tokens) {
    if (std::holds_alternative<T>(tokens.peak()->data)) {
        return std::get<T>(tokens.pull_front()->data);
    }
    return {};
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

// some forward decls
node_return parse_block(FileNodePtr root, Tokens& tokens, bool is_global, parser_wip& wip);

/*
node_return
parse_expressionstmt(FileNodePtr root, Tokens& tokens) {
}
*/

std::pair<int,int>
binding_power(TokenData token) {
    if (auto t = std::get_if<OpAddToken>(&token)) {
        return {13, 14};
    }
    else if (auto t = std::get_if<OpMulToken>(&token)) {
        return {11, 12};
    }
    else if (auto t = std::get_if<OpAssignToken>(&token)) {
        return {34, 33};
    }
}

node_return
parse_expression(FileNodePtr root, Tokens& tokens, parser_wip& wip) {
}

node_return
parse_identifier(FileNodePtr root, Tokens& tokens, parser_wip& wip) {
    if (!std::holds_alternative<IdentifierToken>(tokens.peak()->data)) {
        throw "Expected identifier";
    }
    auto id = std::get<IdentifierToken>(tokens.pull_front()->data).identifier;

    std::shared_ptr<Node> node = std::make_shared<Node>();
    node->data = Identifier { id };
    return {node};
}

node_return
parse_method_decl(FileNodePtr root, Tokens& tokens, parser_wip& wip) {
    //"fn", fn, ws, identifier, ws?, "(", param list ")" ws? ":" NL parse block
    eat_whitespace(tokens);

    auto ident = std::get<IdentifierToken>(tokens.pull_front()->data).identifier;
    eat_whitespace(tokens);

    if (!token_if<OpParenOpenToken>(tokens)) {
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

        if (!token_if<OpColonToken>(tokens)) {
            throw "Syntax error, expected : between parameter name and type";
        }
        eat_whitespace(tokens);

        auto param_type = std::get<IdentifierToken>(tokens.pull_front()->data).identifier;
        eat_whitespace(tokens);
        param_types.push_back(MethodTypeParameter{param_type});

        if (!token_if<OpCommaToken>(tokens)) {
            break;
        }
        eat_whitespace(tokens);

        param_token = token_if<IdentifierToken>(tokens);
    }
    eat_whitespace(tokens);

    if (!token_if<OpParenCloseToken>(tokens)) {
        throw "Syntax error, expected ) after parameter list for function decl/def";
    }
    eat_whitespace(tokens);

    std::string ret_type = "void";
    if (token_if<OpColonToken>(tokens)) {
        eat_whitespace(tokens);
        ret_type = std::get<IdentifierToken>(tokens.pull_front()->data).identifier;

        eat_whitespace(tokens);
    }
    eat_whitespace(tokens);

    std::string method_type = wip.types.add_method(ret_type, param_types);

    if (!token_if<OpBraceOpenToken>(tokens)) {
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
    std::shared_ptr<Token> token = tokens.pull_front();

    if (std::holds_alternative<FnToken>(token->data)) {
        return parse_method_decl(root, tokens, wip);
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
        if (token_if<OpBraceCloseToken>(tokens)) {
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
        }
        else {
            statements.push_back(statement.node);
        }
    }

    std::vector<std::shared_ptr<Node>> all;
    std::copy(vardefs.begin(), vardefs.end(), std::back_inserter(all));
    std::copy(fndefs.begin(), fndefs.end(), std::back_inserter(all));
    std::copy(statements.begin(), statements.end(), std::back_inserter(all));

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
