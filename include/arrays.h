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

ARRAY_TYPES(AS_ARRAY_STRUCT)
ARRAY_TYPES(AS_ARRAY_FUNC_DECLARATIONS)

#endif // ARRAY_H
