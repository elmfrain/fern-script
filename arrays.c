#include "include/arrays.h"

#include "include/errors.h"

#define ARRAY_THROW_IF_NULL(x, ret) {\
	if(!x) {\
		ThrowErrorF(NULL_POINTER_ACCESS,\
			"%s: Tried to use a null array for param %s",\
			__FUNCTION__,\
			#x\
		);\
		return ret;\
	}\
}

#define AS_ARRAY_FUNCS(type, arrayName)\
	arrayName##Array arrayName##ArrayArenaAlloc(int capacity, MemArena* arena) {\
		arrayName##Array array = {};\
		array.items = MemArenaAlloc(arena, sizeof(type) * capacity);\
		if(!array.items) {\
			ThrowErrorF(\
				FAILED_MEMORY_ALLOCATION,\
				"There isn't enough space (remaining capacity %d) in the arena to allocate the array with capacity %d",\
				arena->capacity - arena->nextAlloc,\
				capacity\
			);\
			return array;\
		}\
		array.capacity = capacity;\
		return array;\
	}\
	type* arrayName##ArrayGet(arrayName##Array* array, int index) {\
		ARRAY_THROW_IF_NULL(array, NULL);\
		if(index < 0 || array->length <= index) {\
			ThrowErrorF(\
				ARRAY_OUT_OF_BOUNDS,\
				"Attempted to get item from array (capacity %d) that is out of bounds (index=%d)",\
				array->capacity,\
				index\
			);\
			return NULL;\
		}\
		return &array->items[index];\
	}\
	void arrayName##ArrayGetValue(arrayName##Array* array, int index, type* value) {\
		ARRAY_THROW_IF_NULL(array,);\
		if(index < 0 || array->length <= index) {\
			ThrowErrorF(\
				ARRAY_OUT_OF_BOUNDS,\
				"Attempted to get item from array (capacity %d) that is out of bounds (index=%d)",\
				array->capacity,\
				index\
			);\
			return;\
		}\
		if(value)\
			*value = array->items[index];\
	}\
	type arrayName##ArrayAdd(arrayName##Array* array, type value) {\
		ARRAY_THROW_IF_NULL(array, value);\
		if(array->length == array->capacity) {\
			ThrowError(ARRAY_TOO_FULL, "Attempted to add an item to a full array");\
			return value;\
		}\
		array->items[array->length] = value;\
		array->length++;\
		return value;\
	}\
	void arrayName##ArrayRemove(arrayName##Array* array, int index, type* removedValue) {\
	ARRAY_THROW_IF_NULL(array,);\
		if(index < 0 || array->length <= index) {\
			ThrowErrorF(\
				ARRAY_OUT_OF_BOUNDS,\
				"Attempted to remove item from array (capacity %d) that is out of bounds (index=%d)",\
				array->capacity,\
				index\
			);\
			return;\
		}\
		array->length--;\
		if(removedValue)\
			*removedValue = array->items[index];\
		array->items[index] = array->items[array->length];\
	}

ARRAY_TYPES(AS_ARRAY_FUNCS);

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
