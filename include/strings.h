#ifndef STRINGS_H
#define STRINGS_H

#include "memarena.h"

#include <stdbool.h>
#include <string.h>

#define ENSURE_STRING_LITERAL(x) ("" x "")
#define STRING_LENGTH(x) ((sizeof(x) / sizeof(x[0])) - sizeof(x[0]))
#define ConstString(x) (String) { ENSURE_STRING_LITERAL(x), STRING_LENGTH(x), STRING_LENGTH(x), true }

typedef struct {
	char* chars;
	int length;
	int capacity;
	bool isStaticallyAllocated;
} String;

typedef struct {
	const char* chars;
	int length;
	const char* baseChars;
} StringView;

/* Create a string (with its contents allocated in a memory arena) from a c string */
String CopyCStrArenaAlloc(const char* cstr, MemArena* arena);

/* Create an empty string with its capacity allocated in a memory arena */
String StringArenaAlloc(int capacity, MemArena* arena);

/* Create a string view of a substring from an original string */
StringView SubString(String* str, int offset, int length);

/* Get a character from a string with bounds checking */
char CharAt(String* str, int index);

/* Get a temporary representation of a String as a c string (useful when using printf) */
const char* AsCString(String* str);

/* Create a copy of a String as a c string. The memory is allocated using the given allocator (usually malloc) */
const char* ToCString(String* str, void* (*allocator)(size_t));

/* Compare two strings. It does the same behavior as in strcmp, but for Strings */
int CmpStrings(String* str1, String* str2);

/* Check if a string starts with a prefix */
bool StrStartWith(String* base, String* prefix);

/* Check if a string is empty. */
bool StrIsEmpty(String* str);

/* Concatentate a c string to a String. It quitely fails if the String is at full capacity */
void ConcatCStr(String* str, const char* cstr);

/* Concatentate a length bounded c string to a String. It quietly fails if the String is at full capacity */
void ConcatCStrN(String* str, int length, const char* cstr);

#endif // STRINGS_H
