#ifndef INTERPERETER_H
#define INTERPERETER_H

#include "memarena.h"
#include "parser.h"

/* List of values the runtime keeps track of. Can be transformed as enums and typedefs */
#define RUNTIME_VALUES(X)\
	X(NUMBER, RuntimeNumber)\
	X(NULL, RuntimeNull)

/* Intermetidate macro, do not use! */
#define _AS_RUNTIME_VALUE_TYPE_ENUM(NAME) RT_##NAME

/* Spell out the name of the enum for each runtime value */
#define AS_RUNTIME_VALUE_TYPE_ENUM(NAME, _) _AS_RUNTIME_VALUE_TYPE_ENUM(NAME##_VALUE),

typedef enum {
	RUNTIME_VALUES(AS_RUNTIME_VALUE_TYPE_ENUM)
} RuntimeValueType;

#define INHERIT_RUNTIME_VALUE\
	RuntimeValueType type;

typedef struct {
	INHERIT_RUNTIME_VALUE;
} RuntimeValue;

typedef struct {
	INHERIT_RUNTIME_VALUE;
	double value;
} RuntimeNumber;

typedef struct {
	INHERIT_RUNTIME_VALUE
} RuntimeNull;

typedef struct {
	ProgramAST ast;
	struct {
		void* mem;
		int size;
	} _memory;
	struct {
		void* segment;
		int ptr;
		int size;
	} _stack;
	struct {
		void* segment;
		int nextAlloc;
		int size;
	} _heap;
	struct VariableDictionary* _currentScope;
} FernRuntime;

/* Allocate and setup an instance of a fern script runtime */
FernRuntime CreateFernRuntime(ProgramAST root, MemArena* arena, int memoryCapacity);

/* Evalutate an ast using a runtime instance */
RuntimeValue* EvaluateStatement(FernRuntime* runtime, Statement* stmt);

#endif // INTERPERETER_H
