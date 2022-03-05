#include "Tokenizer.h"

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <sstream>
#include <typeinfo>
#include <unordered_map>
#include <variant>
#include <vector>

#include "Tokens.h"

namespace MattScript {
namespace Tokenizer {

typedef std::function<std::optional<std::shared_ptr<Tokens::Token>>(std::string, Tokens::Position)> TokenScanner;

std::optional<std::shared_ptr<Tokens::Token>>
try_space(std::string line, Tokens::Position start) {
    int i = 0;
    while (i < line.length() && std::isspace(line[i])) {
        i++;
    }
    if (i == 0) {
        return {};
    }
    Tokens::Position end = Tokens::Position{start.line, start.col + i};
    return std::make_shared<Tokens::Token>(Tokens::Span{start, end}, Tokens::SpaceToken{line.substr(0, i)});
}

std::optional<std::shared_ptr<Tokens::Token>>
try_comment(std::string line, Tokens::Position start) {
    if (line[0] != '#') {
        return {};
    }
    Tokens::Position end = {start.line, start.col + line.length()};
    return std::make_shared<Tokens::Token>(Tokens::Span{start, end}, Tokens::CommentToken{line.substr(1)});
}

std::optional<std::shared_ptr<Tokens::Token>>
try_keyword(std::string line, Tokens::Position start) {
    // TODO: optimize
    std::vector<std::string> keywords = {
        "continue",
        "return",
        "break",
        "else",
        "let",
        "for",
        "fn",
        "if",
        "in"
    };

    for (auto& k : keywords) {
        if (k.length() > line.length()) {
            continue;
        }
        if (line.substr(0, k.length()) == k) {
            Tokens::Position end = {start.line, start.col + k.length()};
            return std::make_shared<Tokens::Token>(Tokens::Span{start, end}, Tokens::KeywordToken{k});
        }
    }
    return {};
}

std::optional<std::shared_ptr<Tokens::Token>>
try_identifier(std::string line, Tokens::Position start) {
    if (!std::isalpha(line[0]) && line[0] != '_') {
        return {};
    }
    int i = 1;
    while (i < line.length() && (std::isalnum(line[i]) || line[i] == '_')) {
        i++;
    }
    Tokens::Position end = {start.line, start.col + i};
    return std::make_shared<Tokens::Token>(Tokens::Span{start, end}, Tokens::IdentifierToken{line.substr(0, i)});
}

std::optional<std::shared_ptr<Tokens::Token>>
try_operator(std::string line, Tokens::Position start) {
    std::vector<std::string> ops = {
        "<<=",
        ">>=",
        "||",
        "&&",
        "**",
        "==",
        "!=",
        "<=",
        ">=",
        "<<",
        ">>",
        "+=",
        "-=",
        "*=",
        "/=",
        "%=",
        "|=",
        "&=",
        "^=",
        "::",
        "!",
        "<",
        ">",
        "|",
        "&",
        "^",
        "+",
        "-",
        "*",
        "/",
        "%",
        ",",
        "(",
        ")",
        "{",
        "}",
        "[",
        "]",
        ":",
        ";",
        "=",
        "."
    };

    for (auto& o : ops) {
        if (o.length() > line.length()) {
            continue;
        }
        if (line.substr(0, o.length()) == o) {
            Tokens::Position end = {start.line, start.col + o.length()};
            return std::make_shared<Tokens::Token>(Tokens::Span{start, end}, Tokens::OperatorToken{o});
        }
    }
    return {};
}

std::optional<std::shared_ptr<Tokens::Token>>
try_string(std::string line, Tokens::Position start) {
    auto quote_mark = line[0];
    if (quote_mark != '"' && quote_mark != '\'' && quote_mark != '`') {
        return {};
    }
    int i = 1;
    while (i < line.length()) {
        if (line[i] == quote_mark) {
            Tokens::Position end = Tokens::Position{start.line, start.col + i + 1};
            return std::make_shared<Tokens::Token>(Tokens::Span{start, end}, Tokens::StringToken{line.substr(1, i)});
        }
        // TODO: escape support
        if (line[i] == '\\') {
            int next = i + 1;
            if (next >= line.length()) {
                break;
            }
            i++;
        }
        i++;
    }
    // an incomplete string
    return {};
}

std::optional<std::shared_ptr<Tokens::Token>>
try_int(std::string line, Tokens::Position start) {
    bool enable_hex = false;
    int i = 0;
    while (i < line.length() && (std::isdigit(line[i]) || (enable_hex && std::isxdigit(line[i])) || (i == 1 && line[0] == '0' && line[1] == 'x') ) ) {
        i++;
    }
    if (i == 0) {
        return {};
    }
    Tokens::Position end = Tokens::Position{start.line, start.col + i};
    return std::make_shared<Tokens::Token>(Tokens::Span{start, end}, Tokens::S32Token{std::stoi(line.substr(0, i))});
}

std::optional<std::shared_ptr<Tokens::Token>>
try_float(std::string line, Tokens::Position start) {
    // TODO: more floats (expo et al)
    int i = 0;
    while (i < line.length() && (std::isdigit(line[i]) || (i > 0 && line[i] == '.')) ) {
        i++;
    }
    if (i == 0) {
        return {};
    }
    Tokens::Position end = Tokens::Position{start.line, start.col + i};
    return std::make_shared<Tokens::Token>(Tokens::Span{start, end}, Tokens::F32Token{std::stof(line.substr(0, i))});
}

std::optional<std::shared_ptr<Tokens::Token>>
try_bool(std::string line, Tokens::Position start) {
    std::vector<std::pair<std::string, bool>> bools = {
        {"true", true},
        {"false", false}
    };

    for (auto& p : bools) {
        auto str = p.first;
        if (str.length() > line.length()) {
            continue;
        }
        if (line.substr(0, str.length()) == str) {
            Tokens::Position end = {start.line, start.col + str.length()};
            return std::make_shared<Tokens::Token>(Tokens::Span{start, end}, Tokens::BoolToken{p.second});
        }
    }
    return {};
}

std::vector<std::shared_ptr<Tokens::Token>>
tokenize_string(std::string contents) {
    std::istringstream stream(contents);

    std::vector<TokenScanner> scanners = {
        try_space,
        try_comment,
        try_keyword,
        try_identifier,
        try_operator,
        try_string,
        try_int,
        try_float,
        try_bool
    };

    std::vector<std::shared_ptr<Tokens::Token>> tokens;
    std::string line;
    size_t line_num = 0;
    while (std::getline(stream, line)) {
        line_num++;

        for (size_t col = 0; col < line.length();) {
            Tokens::Position start = Tokens::Position{line_num, col};
            std::string text = line.substr(col);

            std::optional<std::shared_ptr<Tokens::Token>> longest = {};
            for (auto& scanner : scanners) {
                auto maybe = scanner(text, start);
                if (maybe && (!longest || maybe.value()->span.end.col > longest.value()->span.end.col)) {
                    longest = maybe;
                }
            }

            if (!longest || !longest.value()) {
                std::cerr << line_num << " " << col << " " << line << "\n";
                throw "Syntax error, no token";
            }
            //if (std::holds_alternative<SpaceToken>(longest.value()->data) && longest.value()->span.start.col == 0 && longest.value()->span.end.col == line.length()) {
            //    // a space that takes up the entire new line is just eaten up.
            //    break;
            //}
            if (std::holds_alternative<Tokens::CommentToken>(longest.value()->data)) {
                // just eat the entire comment...
                break;
            }

            if (!std::holds_alternative<Tokens::SpaceToken>(longest.value()->data)) {
                tokens.push_back(longest.value());
            }
            col = longest.value()->span.end.col;
        }
        while (tokens.size() > 0 && std::holds_alternative<Tokens::SpaceToken>(tokens[tokens.size() - 1]->data)) {
            // we don't need empty lines...
            tokens.pop_back();
        }
        if (tokens.size() > 0 && std::holds_alternative<Tokens::NewlineToken>(tokens[tokens.size() - 1]->data)) {
            // only add a new line if we added something else.
            // this way we only have one NL 
            // TODO: update that token to be larger?
            tokens.push_back(std::make_shared<Tokens::Token>(Tokens::Span{Tokens::Position{line_num, line.length()}, Tokens::Position{line_num + 1, 0}}, Tokens::NewlineToken{}));
        }
    }
    return tokens;
}

}
}
