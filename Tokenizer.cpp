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

typedef std::function<std::optional<std::shared_ptr<Token>>(std::string, Position)> TokenScanner;

std::optional<std::shared_ptr<Token>>
try_space(std::string line, Position start) {
    int i = 0;
    while (i < line.length() && std::isspace(line[i])) {
        i++;
    }
    if (i == 0) {
        return {};
    }
    Position end = Position{start.line, start.col + i};
    return std::make_shared<Token>(Span{start, end}, SpaceToken{line.substr(0, i)});
}

std::optional<std::shared_ptr<Token>>
try_comment(std::string line, Position start) {
    if (line[0] != '#') {
        return {};
    }
    Position end = {start.line, start.col + line.length()};
    return std::make_shared<Token>(Span{start, end}, CommentToken{line.substr(1)});
}

std::optional<std::shared_ptr<Token>>
try_keyword(std::string line, Position start) {
    // TODO: optimize
    std::vector<std::pair<std::string, TokenData>> keywords = {
        {"continue", ContinueToken{}},
        {"return", ReturnToken{}},
        {"break", BreakToken{}},
        {"else", ElseToken{}},
        {"for", ForToken{}},
        {"fn", FnToken{}},
        {"if", IfToken{}},
        {"in", InToken{}},
    };

    for (auto& p : keywords) {
        auto& k = p.first;
        if (k.length() > line.length()) {
            continue;
        }
        if (line.substr(0, k.length()) == k) {
            Position end = {start.line, start.col + k.length()};
            return std::make_shared<Token>(Span{start, end}, p.second);
        }
    }
    return {};
}

std::optional<std::shared_ptr<Token>>
try_identifier(std::string line, Position start) {
    if (!std::isalpha(line[0]) && line[0] != '_') {
        return {};
    }
    int i = 1;
    while (i < line.length() && (std::isalnum(line[i]) || line[i] == '_')) {
        i++;
    }
    Position end = {start.line, start.col + i};
    return std::make_shared<Token>(Span{start, end}, IdentifierToken{line.substr(0, i)});
}

std::optional<std::shared_ptr<Token>>
try_operator(std::string line, Position start) {
    std::vector<std::pair<std::string, TokenData>> ops = {
        {"<<=", OpLShiftAssignToken{}},
        {">>=", OpRShiftAssignToken{}},
        {"||", OpOrToken{}},
        {"&&", OpAndToken{}},
        {"==", OpEqToken{}},
        {"!=", OpNeqToken{}},
        {"<=", OpLessEqToken{}},
        {">=", OpGreaterEqToken{}},
        {"<<", OpLShiftToken{}},
        {">>", OpRShiftToken{}},
        {"+=", OpAddAssignToken{}},
        {"-=", OpSubAssignToken{}},
        {"*=", OpMulAssignToken{}},
        {"/=", OpDivAssignToken{}},
        {"%=", OpModAssignToken{}},
        {"|=", OpBitOrAssignToken{}},
        {"&=", OpBitAndAssignToken{}},
        {"^=", OpBitXorAssignToken{}},
        {"!", OpNotToken{}},
        {"<", OpLessToken{}},
        {">", OpGreaterToken{}},
        {"|", OpBitOrToken{}},
        {"&", OpBitAndToken{}},
        {"^", OpBitXorToken{}},
        {"+", OpAddToken{}},
        {"-", OpSubToken{}},
        {"*", OpMulToken{}},
        {"/", OpDivToken{}},
        {"%", OpModToken{}},
        {",", OpCommaToken{}},
        {"(", OpParenOpenToken{}},
        {")", OpParenCloseToken{}},
        {"{", OpBraceOpenToken{}},
        {"}", OpBraceCloseToken{}},
        {"[", OpBracketOpenToken{}},
        {"]", OpBracketCloseToken{}},
        {":", OpColonToken{}},
        {"=", OpAssignToken{}},
        {".", OpDotToken{}}
    };

    for (auto& p : ops) {
        auto& o = p.first;
        if (o.length() > line.length()) {
            continue;
        }
        if (line.substr(0, o.length()) == o) {
            Position end = {start.line, start.col + o.length()};
            return std::make_shared<Token>(Span{start, end}, p.second);
        }
    }
    return {};
}

std::optional<std::shared_ptr<Token>>
try_string(std::string line, Position start) {
    auto quote_mark = line[0];
    if (quote_mark != '"' && quote_mark != '\'' && quote_mark != '`') {
        return {};
    }
    int i = 1;
    while (i < line.length()) {
        if (line[i] == quote_mark) {
            Position end = Position{start.line, start.col + i + 1};
            return std::make_shared<Token>(Span{start, end}, StringToken{line.substr(1, i)});
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

std::optional<std::shared_ptr<Token>>
try_int(std::string line, Position start) {
    bool enable_hex = false;
    int i = 0;
    while (i < line.length() && (std::isdigit(line[i]) || (enable_hex && std::isxdigit(line[i])) || (i == 1 && line[0] == '0' && line[1] == 'x') ) ) {
        i++;
    }
    if (i == 0) {
        return {};
    }
    Position end = Position{start.line, start.col + i};
    return std::make_shared<Token>(Span{start, end}, S32Token{std::stoi(line.substr(0, i))});
}

std::optional<std::shared_ptr<Token>>
try_float(std::string line, Position start) {
    // TODO: more floats (expo et al)
    int i = 0;
    while (i < line.length() && (std::isdigit(line[i]) || (i > 0 && line[i] == '.')) ) {
        i++;
    }
    if (i == 0) {
        return {};
    }
    Position end = Position{start.line, start.col + i};
    return std::make_shared<Token>(Span{start, end}, F32Token{std::stod(line.substr(0, i))});
}

std::vector<std::shared_ptr<Token>>
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
        try_float
    };

    std::vector<std::shared_ptr<Token>> tokens;
    std::string line;
    size_t line_num = 0;
    while (std::getline(stream, line)) {
        line_num++;

        for (size_t col = 0; col < line.length();) {
            Position start = Position{line_num, col};
            std::string text = line.substr(col);

            std::optional<std::shared_ptr<Token>> longest = {};
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
            if (std::holds_alternative<SpaceToken>(longest.value()->data) && longest.value()->span.start.col == 0 && longest.value()->span.end.col == line.length()) {
                // a space that takes up the entire new line is just eaten up.
                break;
            }
            if (std::holds_alternative<CommentToken>(longest.value()->data)) {
                // just eat the entire comment...
                break;
            }

            tokens.push_back(longest.value());
            col = longest.value()->span.end.col;
        }
        while (tokens.size() > 0 && std::holds_alternative<SpaceToken>(tokens[tokens.size() - 1]->data)) {
            // we don't need empty lines...
            tokens.pop_back();
        }
        if (tokens.size() > 0 && std::holds_alternative<NewlineToken>(tokens[tokens.size() - 1]->data)) {
            // only add a new line if we added something else.
            // this way we only have one NL 
            // TODO: update that token to be larger?
            tokens.push_back(std::make_shared<Token>(Span{Position{line_num, line.length()}, Position{line_num + 1, 0}}, NewlineToken{}));
        }
    }
    return tokens;
}
