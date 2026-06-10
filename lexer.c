#include "include/lexer.h"

#include "include/logging.h"
#include "include/errors.h"
#include "include/memarena.h"
#include "include/strings.h"

#define AS_KEYWORD_STR(_, NAME) ConstString(#NAME),

static String s_LexerKeywords[] = {
	LEXER_KEYWORDS(AS_KEYWORD_STR)
};

static const int NUM_KEYWORDS = sizeof(s_LexerKeywords) / sizeof(String);

LEXER_ARRAY_TYPES(AS_ARRAY_FUNCS);
LEXER_ARRAY_TYPES(AS_ARRAY_STREAM_FUNCS);

static bool IsAlphabetical(char c) {
	return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

static bool IsWhitespace(char c) {
	return c == ' ' || c == '\t' || c == '\n';
}

static bool IsDecimal(char c) {
	return '0' <= c && c <= '9';
}

static bool IsOctal(char c) {
	return '0' <= c && c <= '7';
}

static bool IsNonZeroDecimal(char c) {
	return '1' <= c && c <= '9';
}

static bool IsHexidecimal(char c) {
	return IsDecimal(c) || ('A' <= c && c <= 'F') || ('a' <= c && c <= 'f');
}

static bool IsBinaryOperator(char c) {
	return c == '+' || c == '-' || c == '/' || c == '*' || c == '%' || c == '^';
}

static bool IsUnaryOperator(char c) {
	return c == '+' || c == '-';
}

static bool IsAnOperator(char c) {
	return IsBinaryOperator(c) || IsUnaryOperator(c);
}

static bool IsPlusOrMinus(char c) {
	return c == '+' || c == '-';
}

static bool IsIdentifier(char c) {
	return IsAlphabetical(c) || c == '_';
}

#define CurrentCharEq(chr) (*length < view.length && CharAt((String*) &view, *length) == chr)
#define CurrentChar(checker) (*length < view.length && checker(CharAt((String*) &view, *length)))
#define RemainingStr() SubString((String*) &view, *length, view.length - *length)

static bool IsHexidecimalNumber(StringView view, int* length) {
	// Hexidecimal number regex: 0[xX][0-9a-fA-F]+
	*length = 0;
	if(!CurrentCharEq('0'))
		return false;
	(*length)++;

	if(!CurrentCharEq('x') && !CurrentCharEq('X'))
		return false;
	(*length)++;

	int prevLen = *length;
	for(;; (*length)++) if(!CurrentChar(IsHexidecimal)) break;
	if(prevLen == *length)
		return false;

	return true;
}

static bool IsOctalNumber(StringView view, int* length) {
	// Octal number regex: 0[0-7]*
	*length = 0;
	if(!CurrentCharEq('0'))
		return false;
	(*length)++;

	for(;; (*length)++) if(!CurrentChar(IsOctal))

	return true;
}

static bool IsIntegerNumber(StringView view, int* length) {
	// Integer number regex: [1-9][0-9]*
	*length = 0;
	if(!CurrentChar(IsNonZeroDecimal))
		return false;
	(*length)++;

	for(;; (*length)++) if(!CurrentChar(IsDecimal)) break;

	return true;
}

static bool IsSciNotation(StringView view, int* length) {
	// Sci Notation number regex: ([Ee][+-]?{D}+)
	*length = 0;
	if(!CurrentCharEq('e') && !CurrentCharEq('E'))
		return false;
	(*length)++;

	int prevLen = *length;
	for(;; (*length)++) if(!CurrentChar(IsPlusOrMinus)) break;
	if(1 < (*length - prevLen))
		return false;

	prevLen = *length;
	for(;; (*length)++) if(!CurrentChar(IsDecimal)) break;
	if(prevLen == *length)
		return false;

	return true;
}

static bool IsFloatingPointNumber(StringView view, int* length) {
	// Float Sci Notation number regex: [0-9]+{IsSciNotation}
	*length = 0;
	for(;; (*length)++) if(!CurrentChar(IsDecimal)) break;
	if(*length == 0)
		goto frontPoint;

	int sciNotLen = 0;
	if(!IsSciNotation(RemainingStr(), &sciNotLen))
		goto frontPoint;
	*length += sciNotLen;

	return true;

	// Float number (where the decimal point can be at the beninging like .25) regex:
	// [0-9]*\.[0-9]+{IsSciNotation}?
frontPoint:
	*length = 0;
	for(;; (*length)++) if(!CurrentChar(IsDecimal)) break;

	if(!CurrentCharEq('.'))
		goto backPoint;
	(*length)++;

	int prevLen = *length;
	for(;; (*length)++) if(!CurrentChar(IsDecimal)) break;
	if(prevLen == *length)
		goto backPoint;

	sciNotLen = 0;
	if(IsSciNotation(RemainingStr(), &sciNotLen))
		*length += sciNotLen;

	return true;

	// Float number (where the decimal point can be at the end like 25.) regex:
	// [0-9]+\.[0-9]*{IsSciNotation}?
backPoint:
	*length = 0;
	for(;; (*length)++) if(!CurrentChar(IsDecimal)) break;
	if(*length == 0)
		return false;

	if(!CurrentCharEq('.'))
		return false;
	(*length)++;

	for(;; (*length)++) if(!CurrentChar(IsDecimal)) break;

	sciNotLen = 0;
	if(IsSciNotation(RemainingStr(), &sciNotLen))
		*length += sciNotLen;

	return true;
}

static bool IsNumberToken(StringView view, int* length) {
	if(IsHexidecimalNumber(view, length))
		return true;
	if(IsOctalNumber(view, length))
		return true;
	if(IsFloatingPointNumber(view, length))
		return true;
	if(IsIntegerNumber(view, length))
		return true;

	return false;
}

static bool IsIdentifierToken(StringView view, int* length) {
	// Indentifier token regex: [a-zA-Z_]([a-zA-Z_]|[0-9])*
	*length = 0;
	if(!CurrentChar(IsIdentifier))
		return false;
	(*length)++;

	for(;;(*length)++) if(!CurrentChar(IsIdentifier) & !CurrentChar(IsDecimal)) break;

	return true;
}

static bool IsKeywordToken(StringView view, int* length, LexerTokenType* keyword) {
	// Keyword token regex: {keyword}((?=[a-zA-Z_])|$)
	bool startsWithKeyword = false;
	for(int i = 0; i < NUM_KEYWORDS; i++)
		if(StrStartsWith((String*) &view, &s_LexerKeywords[i])) {
			*keyword = LEXER_FIRST_KEYWORD_ENUM + i;
			*length = s_LexerKeywords[i].length;
			startsWithKeyword = true;
			break;
		}

	if(!startsWithKeyword) return false;
	if(CurrentChar(IsIdentifier) && *length < view.length) return false;

	return true;
}

#define AddToken(tokenLen, tokenType) {\
	LexerTokenArrayAdd(&tokens, (LexerToken){\
		.string = SubString((String*) &scriptView, 0, tokenLen),\
		.type = tokenType\
	});\
	scriptView = SubString((String*) &scriptView, tokenLen, scriptView.length - tokenLen);\
	column += tokenLen;\
}

#undef CurrentChar
#define CurrentChar() (CharAt((String*) &scriptView, 0))

LexerTokenArray LexerTokenize(String fileName, String script, MemArena* context) {
	LexerTokenArray tokens = LexerTokenArrayArenaAlloc(script.length, context);

	if(tokens.capacity == 0)
		return tokens;

	StringView scriptView = SubString(&script, 0, script.length);
	LexerTokenType keyword = LEXER_FIRST_KEYWORD_ENUM;
	int tokenLength = 0;
	int line = 1;
	int column = 1;
token:
	if(scriptView.length == 0)
		return tokens;
	if(tokens.capacity == tokens.length) {
		LogWarningV("Ran out of space to define more tokens in Lexer (capacity=%d)", tokens.capacity);
		return tokens;
	}

	if(CurrentChar() == '\n') {
		AddToken(1, TK_NEWLINE);
		line++;
		column = 1;
	} else if(IsWhitespace(CurrentChar())) {
		scriptView = SubString((String*) &scriptView, 1, scriptView.length - 1);
	} else if(CurrentChar() == '(') {
		AddToken(1, TK_OPEN_PAREN);
	} else if(CurrentChar() == ')') {
		AddToken(1, TK_CLOSE_PAREN);
	} else if(CurrentChar() == ';') {
		AddToken(1, TK_SEMICOLON);
	} else if(IsAnOperator(CurrentChar())) {
		AddToken(1, TK_OPERATOR);
	} else if(CurrentChar() == '=') {
		AddToken(1, TK_ASSIGNMENT_OPERATOR);
	} else if(IsNumberToken(scriptView, &tokenLength)) {
		AddToken(tokenLength, TK_NUMBER);
	} else if(IsKeywordToken(scriptView, &tokenLength, &keyword)) {
		AddToken(tokenLength, keyword);
	} else if(IsIdentifierToken(scriptView, &tokenLength)) {
		AddToken(tokenLength, TK_IDENTIFIER);
	} else { // Unidentified token found
		int len = 0;
		for(;len < scriptView.length; len++)
			if(IsWhitespace(CharAt((String*) &scriptView, len))) break;

		StringView unknownTokenView = scriptView;
		unknownTokenView.length = len;

		LogErrorV(
			"Encountered an unknown token '%s' at %s:%d:%d",
			AsCString((String*) &unknownTokenView),
			AsCString(&fileName),
			line,
			column
		);
		scriptView = SubString((String*) &scriptView, len, scriptView.length - len);
	}

	goto token;
}
