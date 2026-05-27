#ifndef ARRAYS_H
#define ARRAYS_H

#include "strings.h"

#define ARRAY_TYPES(X)\
	X(String, String)

#define AS_ARRAY_STRUCT(type, arrayName)\
	typedef struct {\
		type* items;\
		int length;\
		int capacity;\
	} arrayName##Array;

#define AS_ARRAY_FUNC_DECLARATIONS(type, arrayName)\
	/* Create an array with its storage allocated from a memory arena */\
	arrayName##Array arrayName##ArrayArenaAlloc(int capacity, MemArena* arena);\
	/* Get the pointer to the element from an index with bounds checking */\
	type* arrayName##ArrayGet(arrayName##Array* array, int index);\
	/* Get the element by value from an index with bounds checking */\
	void arrayName##ArrayGetValue(arrayName##Array* array, int index, type* value);\
	/* Add an element to the end of the array if it has enough capacity */\
	type arrayName##ArrayAdd(arrayName##Array* array, type value);\
	/* Remove an element from the array, and swap it with the last element to fill the empty spot */\
	void arrayName##ArrayRemove(arrayName##Array* array, int index, type* removedValue);

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

ARRAY_TYPES(AS_ARRAY_STRUCT)
ARRAY_TYPES(AS_ARRAY_FUNC_DECLARATIONS)

#endif // ARRAY_H
