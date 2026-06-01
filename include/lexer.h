#ifndef LEXER_H
#define LEXER_H

#include "arrays.h"

#define LEXER_FIRST_KEYWORD(X) X(LET, let)

/* List of keywords that is used in the scripting language. Can be transformed as enums and strings */
#define LEXER_KEYWORDS(X)\
	LEXER_FIRST_KEYWORD(X)\
	X(NULL, null)

/* Intermediate macro; should not be used! */
#define _AS_KEYWORD_ENUM(NAME) NAME##_KEYWORD

/* Spell out the listed keywords in LEXER_KEYWORDS as enums */
#define AS_KEYWORD_ENUM(NAME, _) _AS_KEYWORD_ENUM(TK_##NAME),

/* Intermediate macro; should not be used! */
#define _LEXER_FIRST_KEYWORD_ENUM(NAME, _) _AS_KEYWORD_ENUM(TK_##NAME)

/* The first keyword enum */
#define LEXER_FIRST_KEYWORD_ENUM LEXER_FIRST_KEYWORD(_LEXER_FIRST_KEYWORD_ENUM)

typedef enum {
	TK_OPEN_PAREN,
	TK_CLOSE_PAREN,
	TK_OPERATOR,
	TK_ASSIGNMENT_OPERATOR,
	TK_NUMBER,
	TK_IDENTIFIER,
	LEXER_KEYWORDS(AS_KEYWORD_ENUM)
} LexerTokenType;

typedef struct {
	StringView string;
	LexerTokenType type;
} LexerToken;

#define LEXER_ARRAY_TYPES(X)\
	X(LexerToken, LexerToken)

LEXER_ARRAY_TYPES(AS_ARRAY_STRUCT)
LEXER_ARRAY_TYPES(AS_ARRAY_FUNC_DECLARATIONS)

LEXER_ARRAY_TYPES(AS_ARRAY_STREAM_STRUCT)
LEXER_ARRAY_TYPES(AS_ARRAY_STREAM_FUNC_DECLARATIONS)

LexerTokenArray LexerTokenize(String fileName, String script, MemArena* context);

#endif // LEXER_H
