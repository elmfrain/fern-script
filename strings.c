#include "include/strings.h"

#include "include/memarena.h"
#include "include/errors.h"

#define C_STR_BUFFER_LENGTH 2048

#define STRING_THROW_IF_NULL(param, ret) {\
	if(!param) {\
		ThrowErrorF(\
			NULL_POINTER_ACCESS,\
			"%s: Tried to use a null string for param %s",\
			__FUNCTION__,\
			#param\
		);\
		return ret;\
	}\
}

static char s_CStringBuffer[C_STR_BUFFER_LENGTH];
static int s_CStringBufferOffset = 0;

String CopyCStrArenaAlloc(const char* cstr, MemArena* arena) {
	int cstrLength = strlen(cstr);
	String string = {};
	string.chars = MemArenaAlloc(arena, cstrLength);

	if(!string.chars) {
		ThrowErrorF(
			FAILED_MEMORY_ALLOCATION,
			"There isn't enough space (remaining capacity %d) in the arena to allocate a string (length %d)",
			arena->capacity - arena->nextAlloc,
			cstrLength
		);
		return string;
	}

	string.capacity = cstrLength;
	string.length = cstrLength;
	string.isStaticallyAllocated = false;

	for(int i = 0; i < string.length; i++)
		string.chars[i] = cstr[i];

	return string;
}

String StringArenaAlloc(int capacity, MemArena* arena) {
	String string = {};
	string.chars = MemArenaAlloc(arena, capacity);

	if(!string.chars) {
		ThrowErrorF(
			FAILED_MEMORY_ALLOCATION,
			"There isn't enough space (remaining capacity %d) in the arena to allocate a string with capacity of %d",
			arena->capacity - arena->nextAlloc,
			capacity
		);
		return string;
	}

	string.capacity = capacity;
	string.length = 0;
	string.isStaticallyAllocated = false;

	return string;
}

StringView SubString(String* str, int offset, int length) {
	StringView view = {};

	STRING_THROW_IF_NULL(str, view);

	if(offset + length > str->length) {
		ThrowErrorF(
			INVALID_SUBSTR,
			"Substring goes out of bounds of original string (offset=%d, length=%d)",
			offset,
			length
		);
		return view;
	}

	view.chars = str->chars + offset;
	view.length = length;
	view.baseChars = str->chars;

	return view;
}

char CharAt(String* str, int index) {
	STRING_THROW_IF_NULL(str, 0);

	if(index < 0 || str->length <= index) {
		ThrowErrorF(
			STRING_OUT_OF_BOUNDS,
			"Tried to access char (at index=%d) that's out of bounds (length=%d)",
			index,
			str->length
		);
		return 0;
	}

	return str->chars[index];
}

const char* AsCString(String* str) {
	STRING_THROW_IF_NULL(str, "");

	int length = str->length;
	if(C_STR_BUFFER_LENGTH <= str->length) {
		length = C_STR_BUFFER_LENGTH - 1;
		s_CStringBufferOffset = 0;
	} else if(C_STR_BUFFER_LENGTH <= (s_CStringBufferOffset + length))
		s_CStringBufferOffset = 0;

	int i = s_CStringBufferOffset;
	for(int j = 0; j < length; i++, j++)
		s_CStringBuffer[i] = CharAt(str, j);
	s_CStringBuffer[i] = '\x0';

	const char* cstr = s_CStringBuffer + s_CStringBufferOffset;
	s_CStringBufferOffset += length + 1;

	return cstr;
}

const char* ToCString(String* str, void* (*allocator)(size_t)) {
	STRING_THROW_IF_NULL(str, NULL);

	char* cstr = allocator(str->length + 1);

	if(!cstr) {
		ThrowError(FAILED_MEMORY_ALLOCATION, "The cstring allocator returned null");
		return NULL;
	}

	int i = 0;
	for(; i < str->length; i++)
		cstr[i] = CharAt(str, i);

	cstr[i] = '\x0';

	return cstr;
}

int CmpStrings(String* str1, String* str2) {
	STRING_THROW_IF_NULL(str1, -1);
	STRING_THROW_IF_NULL(str2, -1);

	if(str1->length > str2->length)
		return CharAt(str1, str2->length);
	else if(str1->length < str2->length)
		return -CharAt(str2, str1->length);

	for(int i = 0; i < str1->length; i++) {
		int difference = CharAt(str1, i) - CharAt(str2, i);
		if(difference != 0)
			return difference;
	}

	return 0;
}

bool StrStartsWith(String* base, String* prefix) {
	STRING_THROW_IF_NULL(base, false);
	STRING_THROW_IF_NULL(prefix, false);

	if(base->length < prefix->length)
		return false;

	for(int i = 0; i < prefix->length; i++) {
		if(CharAt(base, i) != CharAt(prefix, i))
			return false;
	}

	return true;
}

bool StrIsEmpty(String* str) {
	STRING_THROW_IF_NULL(str, true);

	return str->length == 0;
}

bool StrEquals(String* str1, String* str2) {
	STRING_THROW_IF_NULL(str1, false);
	STRING_THROW_IF_NULL(str2, false);

	return CmpStrings(str1, str2) == 0;
}

void ConcatCStr(String* str, const char* cstr) {
	STRING_THROW_IF_NULL(str,);

	int i, j = 0;
	for(i = str->length; i < str->capacity && cstr[j]; i++, j++)
		str->chars[i] = cstr[j];

	str->length = i;
}

void ConcatCStrN(String* str, int length, const char* cstr) {
	STRING_THROW_IF_NULL(str,);

	int i, j = 0;
	for(i = str->length; i < str->capacity && j < length; i++, j++)
		str->chars[i] = cstr[j];

	str->length = i;
}
