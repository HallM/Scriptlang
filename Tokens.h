#pragma once

#include <variant>

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

struct IdentifierToken {
    std::string identifier;
};

struct StringToken {
    std::string text;
};

struct S32Token {
    int number;
};

struct F32Token {
    double number;
};

struct ContinueToken {};
struct ReturnToken {};
struct BreakToken {};
struct ElseToken {};
struct ForToken {};
struct FnToken {};
struct IfToken {};
struct InToken {};

struct OpLShiftAssignToken {};
struct OpRShiftAssignToken {};
struct OpOrToken {};
struct OpAndToken {};
struct OpEqToken {};
struct OpNeqToken {};
struct OpLessEqToken {};
struct OpGreaterEqToken {};
struct OpLShiftToken {};
struct OpRShiftToken {};
struct OpAddAssignToken {};
struct OpSubAssignToken {};
struct OpMulAssignToken {};
struct OpDivAssignToken {};
struct OpModAssignToken {};
struct OpBitOrAssignToken {};
struct OpBitAndAssignToken {};
struct OpBitXorAssignToken {};
struct OpNotToken {};
struct OpLessToken {};
struct OpGreaterToken {};
struct OpBitOrToken {};
struct OpBitAndToken {};
struct OpBitXorToken {};
struct OpAddToken {};
struct OpSubToken {};
struct OpMulToken {};
struct OpDivToken {};
struct OpModToken {};
struct OpCommaToken {};
struct OpParenOpenToken {};
struct OpParenCloseToken {};
struct OpBraceOpenToken {};
struct OpBraceCloseToken {};
struct OpBracketOpenToken {};
struct OpBracketCloseToken {};
struct OpColonToken {};
struct OpAssignToken {};
struct OpDotToken {};

typedef std::variant<
    SpaceToken,
    NewlineToken,
    CommentToken,
    IdentifierToken,
    StringToken,
    S32Token,
    F32Token,

    ContinueToken,
    ReturnToken,
    BreakToken,
    ElseToken,
    ForToken,
    FnToken,
    IfToken,
    InToken,

    OpLShiftAssignToken,
    OpRShiftAssignToken,
    OpOrToken,
    OpAndToken,
    OpEqToken,
    OpNeqToken,
    OpLessEqToken,
    OpGreaterEqToken,
    OpLShiftToken,
    OpRShiftToken,
    OpAddAssignToken,
    OpSubAssignToken,
    OpMulAssignToken,
    OpDivAssignToken,
    OpModAssignToken,
    OpBitOrAssignToken,
    OpBitAndAssignToken,
    OpBitXorAssignToken,
    OpNotToken,
    OpLessToken,
    OpGreaterToken,
    OpBitOrToken,
    OpBitAndToken,
    OpBitXorToken,
    OpAddToken,
    OpSubToken,
    OpMulToken,
    OpDivToken,
    OpModToken,
    OpCommaToken,
    OpParenOpenToken,
    OpParenCloseToken,
    OpBraceOpenToken,
    OpBraceCloseToken,
    OpBracketOpenToken,
    OpBracketCloseToken,
    OpColonToken,
    OpAssignToken,
    OpDotToken
> TokenData;

class Token {
public:
    Token(Span pos, TokenData d) : span(pos), data(d) {}
    ~Token() {}
    Span span;
    TokenData data;
};
