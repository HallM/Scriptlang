#pragma once

#include <variant>

namespace MattScript {
namespace Tokens {

struct Position {
    size_t line;
    size_t col;
};

// not a component
struct Span {
    Position start;
    Position end;
};

struct SpaceToken {
    std::string space;
};

struct NewlineToken {
    std::string space;
};

struct CommentToken {
    std::string text;
};

struct KeywordToken {
    std::string keyword;
};

struct IdentifierToken {
    std::string identifier;
};

struct OperatorToken {
    std::string oper;
};

struct StringToken {
    std::string text;
};
struct S32Token {
    int number;
};
struct F32Token {
    float number;
};
struct BoolToken {
    bool value;
};

typedef std::variant<
    SpaceToken,
    NewlineToken,
    CommentToken,
    KeywordToken,
    IdentifierToken,
    OperatorToken,
    StringToken,
    S32Token,
    F32Token,
    BoolToken
> TokenData;

class Token {
public:
    Token(Span pos, TokenData d) : span(pos), data(d) {}
    ~Token() {}
    Span span;
    TokenData data;
};

}
}
