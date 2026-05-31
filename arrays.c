#include "include/arrays.h"

#include "include/errors.h"

ARRAY_TYPES(AS_ARRAY_FUNCS);
ARRAY_TYPES(AS_ARRAY_STREAM_FUNCS);

// --- All the code below this line is for prototyping new array functions, but is is not used in code outside this translation unit --- ///

typedef struct {
	char* items;
	int length;
	int capacity;
} Array;

Array ArrayArenaAlloc(int capacity, MemArena* arena) {
	Array array = {};
	array.items = MemArenaAlloc(arena, sizeof(char) * capacity);

	if(!array.items) {
		ThrowError(FAILED_MEMORY_ALLOCATION, "There isn't enough space in the arena to allocate the array");
		return array;
	}

	array.capacity = capacity;
	return array;
}

char* ArrayGet(Array* array, int index) {
	if(index < 0 || array->length <= index) {
		ThrowError(ARRAY_OUT_OF_BOUNDS, "Attempted to get item from array that is out of bounds");
		return NULL;
	}

	return &array->items[index];
}

void ArrayGetValue(Array* array, int index, char* value) {
	if(index < 0 || array->length <= index) {
		ThrowError(ARRAY_OUT_OF_BOUNDS, "Attempted to get item from array that is out of bounds");
		return;
	}

	if(value)
		*value = array->items[index];
}

char ArrayAdd(Array* array, char value) {
	if(array->length == array->capacity) {
		ThrowError(ARRAY_TOO_FULL, "Attempted to add an item to a full array");
		return value;
	}

	array->items[array->length] = value;
	array->length++;

	return value;
}

void ArrayRemove(Array* array, int index, char* removedValue) {
	if(index < 0 || array->length <= index) {
		ThrowError(ARRAY_OUT_OF_BOUNDS, "Attempted to remove an items that is out of bounds");
		return;
	}

	array->length--;
	if(removedValue)
		*removedValue = array->items[index];
	array->items[index] = array->items[array->length];
}

// --- Array streams --- //

typedef struct {
	StringArray array;
	int position;
} ArrayStream;

ArrayStream CreateArrayStream(StringArray array) {
	ArrayStream stream = {};
	stream.array = array;
	return stream;
}

bool ArrayStreamPeek(ArrayStream* stream, String* value) {
	if(stream->position != stream->array.length)
		return false;

	*value = * StringArrayGet(&stream->array, stream->position);

	return true;
}

bool ArrayStreamGet(ArrayStream* stream, String* value) {
	bool ret = ArrayStreamPeek(stream, value);
	stream->position++;
	return ret;
}
