#include "include/interpereter.h"

#include "include/logging.h"
#include "include/errors.h"
#include "include/parser.h"

#include <stdint.h>
#include <math.h>
#include <strings.h>

#define GLOBAL_VARIABLE_SCOPE_CAPACITY 128
#define STACK_SIZE_FROM_TOTAL_MEM_SIZE_DIVISOR 4
#define STACK_SIZE_MAX 131072

/* --- Internal Runtime types --- */

typedef struct {
	RuntimeValue* value;
	StringView key;
} RuntimeVariable;

/*
 * A variable entry inside of dictionaries. These entires are essentially
 * nodes of a linked list of key-value pairs. Each linked list contains keys
 * with the same hash, but a quick linear search can find the correct key.
 * Deactivated entires are when the 'var.value' pointer is NULL.
 */
typedef struct {
	RuntimeVariable var;
	int next;
	int nextFree;
} VariableDictionaryEntry;

/*
 * Variable dictionaries stores the key value pairs of variables. It allows
 * retriveing a value based off of a key (e.g., the variable's name). This
 * struct is just the HEADER of the dictionary, the contents of it are
 * allocated after it.
 * Its contents will have:
 *  - The table itself; (capacity = tableCapacity)
 *  - The 'collided' buffer to store collided entires (capacity = totalCapacity - tableCapacity)
 */
typedef struct VariableDictionary {
	struct VariableDictionary* parent;
	int totalCapacity;
	int tableCapacity;
	int size;
	int firstFreeCollidedEntry;
} VariableDictionary;

#define AS_RUNTIME_VALUE_STR(NAME, _) #NAME,
#define AS_RUNTIME_VALUE_SIZE(_, NAME) sizeof(NAME),
#define THROW_IF_NULL(param, ret) {\
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

#define AS_RUNTIME_VALUE_FUNCS(NAME, TYPE)\
	TYPE Create##TYPE() {\
		TYPE value = {};\
		value.type = _AS_RUNTIME_VALUE_TYPE_ENUM(NAME##_VALUE);\
		return value;\
	}\
	TYPE* Push##TYPE(FernRuntime* runtime) {\
		TYPE* newValue = RuntimeStackAlloc(runtime, sizeof(TYPE));\
		if(!newValue) {\
			ThrowErrorF(\
				FAILED_MEMORY_ALLOCATION,\
				"Stack overflow when allocating '%s' runtime value onto the stack (remaining capacity %d)",\
				#TYPE,\
				runtime->_stack.ptr\
			);\
			return NULL;\
		}\
		newValue->type = _AS_RUNTIME_VALUE_TYPE_ENUM(NAME##_VALUE);\
		return newValue;\
	}\
	void Pop##TYPE(FernRuntime* runtime) {\
		RuntimeStackDealloc(runtime, sizeof(TYPE));\
	}

// static const char* s_RuntimeValueTypesStrings[] = {
// 	RUNTIME_VALUES(AS_RUNTIME_VALUE_STR)
// };

/*
 * This will run the EvaluateStatement function normally, but it will return from any
 * function if an error occured (signaled when it returns a NULL) like when runtime runs
 * out of memory. This is used to gracefully stop any internal evaluation functions.
 */
#define Eval(var, stmt)\
	RuntimeValue* var;\
	if(!(var = EvaluateStatement(runtime, (Statement*) stmt))) return NULL;

/*
 * This will run PushRuntimeValue functions normally, but similarly to the previous
 * macro, it will return from any function if an error occured. This is to stop
 * the functions gracefully.
 */
#define Push(type, var)\
	type* var = Push##type(runtime);\
	if(!var) return NULL;

/* Internal stack functions for the runtime such as allocating and deallocating bytes */

static void* RuntimeStackAlloc(FernRuntime* runtime, int size) {
	if(runtime->_stack.ptr - size < 0)
		return NULL;

	runtime->_stack.ptr -= size;
	return runtime->_stack.segment + runtime->_stack.ptr;
}

static void RuntimeStackDealloc(FernRuntime* runtime, int size) {
	if(runtime->_stack.size <= runtime->_stack.ptr + size)
		return;

	runtime->_stack.ptr += size;
}

static const int s_RuntimeValueTypeSizes[] = {
	RUNTIME_VALUES(AS_RUNTIME_VALUE_SIZE)
};

/*
 * Copy a runtime value onto the stack. This is usually used for copying runtime varables from
 * the heap onto the stack.
 */
static RuntimeValue* PushRuntimeValue(FernRuntime* runtime, RuntimeValue* value) {
	if(!value)
		return NULL;

	int valueSize =  s_RuntimeValueTypeSizes[(int) value->type];
	char* stackValueBuffer = RuntimeStackAlloc(runtime, valueSize);
	if(!stackValueBuffer)
		return NULL;

	for(int i = 0; i < valueSize; i++)
		stackValueBuffer[i] = ((char*) value)[i];

	return (RuntimeValue*) stackValueBuffer;
}

static void PopRuntimeValue(FernRuntime* runtime, RuntimeValue* value) {
	if(!value) return;

	RuntimeStackDealloc(runtime, s_RuntimeValueTypeSizes[(int) value->type]);
}

/*
 * --- Heap allocator functions. ---
 * We are not using malloc here because that is cheating!
 * Let's do simple linear allocation for now. A better allocation algorithm
 * like free lists can be used in the future.
 */

static void* RuntimeHeapAlloc(FernRuntime* runtime, int size) {
	if(runtime->_heap.size < runtime->_heap.nextAlloc + size)
		return NULL;

	int prevNextAlloc = runtime->_heap.nextAlloc;
	runtime->_heap.nextAlloc += size;
	return runtime->_heap.segment + prevNextAlloc;
}

static void RuntimeHeapDealloc(FernRuntime* runtime, void* mem) {

}

/*
 * Scope variable dictionary functions.
 * Scope and VariableDictionary terms are used interchangibly, but the
 * important distinction is that a scope has a variable dictionary, but
 * not the other way around.
 */

/*
 * Push a new scope onto the stack. This essentially allocates a new variable
 * dictionary to the stack and sets it as the current scope whenever the script
 * enters a code block or a function. It will allocate and initialize the
 * header of the variable dictionary, the table, and the collided entries buffer.
 */
static VariableDictionary* RuntimePushScope(FernRuntime* runtime, int capacityHint) {
	int totalCapacity = capacityHint * 2;
	int tableCapacity = totalCapacity * 3 / 4;

	VariableDictionaryEntry* dictContent = RuntimeStackAlloc(runtime, sizeof(VariableDictionaryEntry) * totalCapacity);
	VariableDictionary* varDict = RuntimeStackAlloc(runtime, sizeof(VariableDictionary));
	if(!varDict || !dictContent){
		ThrowErrorF(
			FAILED_MEMORY_ALLOCATION,
			"Stack overflow when allocating variable scope onto the stack (remaining capacity %d)",
			runtime->_stack.ptr
		);
		return NULL;
	}

	varDict->size = 0;
	varDict->firstFreeCollidedEntry = 0;
	varDict->totalCapacity = totalCapacity;
	varDict->tableCapacity = tableCapacity;

	for(int i = 0; i < varDict->totalCapacity; i++) {
		dictContent[i].var.value = NULL;
		dictContent[i].next = -1;
	}

	VariableDictionaryEntry* collidedEntries = dictContent + tableCapacity;
	int collidedEntriesCapacity = totalCapacity - tableCapacity;
	for(int i = 0; i < collidedEntriesCapacity; i++)
		collidedEntries[i].nextFree = i + 1;
	// Zero signifies that it is at the end of free list
	collidedEntries[collidedEntriesCapacity - 1].nextFree = 0;

	varDict->parent = runtime->_currentScope;
	runtime->_currentScope = varDict;

	return varDict;
}

/*
 * Pops the current scope from the stack; like when the scripts exits a funciton
 */
static void RuntimePopScope(FernRuntime* runtime) {
	if(!runtime->_currentScope)
		return;

	int dictTotalCapacity = runtime->_currentScope->totalCapacity;
	runtime->_currentScope = runtime->_currentScope->parent;

	RuntimeStackDealloc(runtime, sizeof(VariableDictionary));
	RuntimeStackDealloc(runtime, sizeof(VariableDictionaryEntry) * dictTotalCapacity);
}

static VariableDictionaryEntry* VariableDictionaryAllocCollidedEntry(VariableDictionary* dict, int* index) {
	VariableDictionaryEntry* entry = NULL;
	return entry;
}

/* Insert a variable into a variable scope */
static void VariableDictionaryInsert(VariableDictionary* dict, RuntimeVariable variable) {
	if(!dict)
		return;

	if(dict->totalCapacity <= dict->size)
		goto notEnoughSpace;

	VariableDictionaryEntry* table = (VariableDictionaryEntry*) ((void*) dict + sizeof(VariableDictionary));
	int tableIndex = HashString((String*) &variable.key) % dict->tableCapacity;
	VariableDictionaryEntry* current = &table[tableIndex];
	// No collisions, insert into table directly
	if(current->var.value == NULL) {
		current->var = variable;
		dict->size++;
		return;
	}

	// Collision occured, store it in collided entries
	int collidedEntryIndex;
	VariableDictionaryEntry* collidedEntry = VariableDictionaryAllocCollidedEntry(dict, &collidedEntryIndex);
	if(!collidedEntry)
		goto notEnoughSpace;

	collidedEntry->var = variable;
	collidedEntry->next = -1;

	VariableDictionaryEntry* collidedEntries = table + dict->tableCapacity;
	for(;current->next != -1; current = collidedEntries + current->next); // Reach the end of collided list

	current->next = collidedEntryIndex; // Insert at end of collided list
	return;

notEnoughSpace:
	ThrowErrorF(\
		FAILED_MEMORY_ALLOCATION,
		"Not enough capacity in variable scope (capacity = %d) to store var '%s'",
		dict->totalCapacity,
		AsCString((String*) &variable.key)
	);
	return;
}

static RuntimeValue* VariableDictionaryGet(VariableDictionary* dict, StringView key) {
	if(!dict)
		return NULL;

	VariableDictionaryEntry* table = (VariableDictionaryEntry*) ((void*) dict + sizeof(VariableDictionary));
	int tableIndex = HashString((String*) &key) % dict->tableCapacity;
	VariableDictionaryEntry* current = &table[tableIndex];
	// Not found
	if(current->var.value == NULL)
		goto tryParent;

	VariableDictionaryEntry* collidedEntries = table + dict->tableCapacity;
	for(;; current = collidedEntries + current->next) {
		if(StrEquals((String*) &current->var.key, (String*) &key))
			return current->var.value;
		if(current->next == -1) break;
	}

tryParent:
	if(!dict->parent)
		return NULL;

	return VariableDictionaryGet(dict->parent, key);
}

RUNTIME_VALUES(AS_RUNTIME_VALUE_FUNCS);

static RuntimeValue* EvaluateProgramAST(FernRuntime* runtime, ProgramAST* program) {
	RuntimeValue* lastEvaluated = NULL;

	NodeChildrenIterator i = NodeChildrenItr(&runtime->ast, (Statement*) program);
	for(Statement* stmt; (stmt = NodeChildrenGet(&i)); NodeChildrenNext(&i)) {
		lastEvaluated = EvaluateStatement(runtime, stmt);
		if(i.size == i.position + 1 || !lastEvaluated) break;
		PopRuntimeValue(runtime, lastEvaluated);
	}

	if(!lastEvaluated)
		return (RuntimeValue*) PushRuntimeNull(runtime);

	return lastEvaluated;
}

#define BinaryExprOpEquals(oper) (StrEquals((String*) &bExpr->op, &ConstString(oper)))

static RuntimeValue* EvaluateBinaryExpr(FernRuntime* runtime, BinaryExpr* bExpr) {
	NodeChildrenIterator i = NodeChildrenItr(&runtime->ast, (Statement*) bExpr);

	Eval(leftSide, NodeChildrenGet(&i));
	NodeChildrenNext(&i);
	Eval(rightSide, NodeChildrenGet(&i));

	if(leftSide->type != RT_NUMBER_VALUE || rightSide->type != RT_NUMBER_VALUE)
		goto retNull;

	RuntimeNumber* left = (RuntimeNumber*) leftSide;
	RuntimeNumber* right = (RuntimeNumber*) rightSide;
	double value;

	if(BinaryExprOpEquals("+"))
		value = left->value + right->value;
	else if(BinaryExprOpEquals("-"))
		value = left->value - right->value;
	else if(BinaryExprOpEquals("*"))
		value = left->value * right->value;
	else if(BinaryExprOpEquals("/")) {
		if(right->value == 0.0) value = INFINITY;
		else value = left->value / right->value;
	} else if(BinaryExprOpEquals("^")) {
		value = pow(left->value, right->value);
	} else if(BinaryExprOpEquals("%")) {
		if(right->value == 0.0) value = NAN;
		else {
			value = (int64_t) left->value % (int64_t) right->value;
			value += left->value - floor(left->value);
			value -= value < 0 ? 1 : 0;
		}
	} else
		goto retNull;

	PopRuntimeValue(runtime, rightSide);
	PopRuntimeValue(runtime, leftSide);
	Push(RuntimeNumber, result);
	result->value = value;
	return (RuntimeValue*) result;

retNull:
	PopRuntimeValue(runtime, rightSide);
	PopRuntimeValue(runtime, leftSide);
	return (RuntimeValue*) PushRuntimeNull(runtime);
}

#define UnaryExprOpEquals(oper) (StrEquals((String*) &uExpr->op, &ConstString(oper)))

static RuntimeValue* EvaluateUnaryExpr(FernRuntime* runtime, UnaryExpr* uExpr) {
	Eval(rightSide, NodeGetFromIndex(&runtime->ast, uExpr->firstChildIndex));

	if(rightSide->type != RT_NUMBER_VALUE)
		goto retNull;

	RuntimeNumber* right = (RuntimeNumber*) rightSide;

	double value = right->value;

	if(UnaryExprOpEquals("-"))
		value *= -1;
	else if(UnaryExprOpEquals("+"))
		value = value;
	else
		goto retNull;

	PopRuntimeValue(runtime, rightSide);
	Push(RuntimeNumber, result);
	result->value = value;
	return (RuntimeValue*) result;

retNull:
	PopRuntimeValue(runtime, rightSide);
	return (RuntimeValue*) PushRuntimeNull(runtime);
}

static RuntimeValue* EvaluateNumericLiteral(FernRuntime* runtime, NumericLiteral* literal) {
	Push(RuntimeNumber, num);

	num->value = literal->value;
	return (RuntimeValue*) num;
}

static RuntimeValue* EvaluateIdentifier(FernRuntime* runtime, Identifier* identifier) {
	RuntimeValue* result = VariableDictionaryGet(runtime->_currentScope, identifier->symbol);

	if(!result)
		return (RuntimeValue*) PushRuntimeNull(runtime);

	return PushRuntimeValue(runtime, result);
}

static int DetermineStackSize(int memoryCapacity) {
	int stackSize = memoryCapacity / STACK_SIZE_FROM_TOTAL_MEM_SIZE_DIVISOR;

	if(stackSize > STACK_SIZE_MAX)
		return STACK_SIZE_MAX;

	// Make stack segment size a multiple of 16 bytes for memory alignment resulting in better performance
	return (stackSize + 15) & ~0xF;
}

FernRuntime CreateFernRuntime(ProgramAST root, MemArena* arena, int memoryCapacity) {
	FernRuntime runtime = {};

	runtime._memory.mem = MemArenaAlloc(arena, memoryCapacity);
	if(!runtime._memory.mem){
		ThrowErrorF(FAILED_MEMORY_ALLOCATION, "Cannot allocate enough space to runtime memory (requested=%d)", memoryCapacity);
		return runtime;
	}

	runtime._memory.size = memoryCapacity;
	runtime.ast = root;

	// Allocate the stack near the end of the memory space
	runtime._stack.size = DetermineStackSize(memoryCapacity);
	runtime._stack.ptr = runtime._stack.size;
	runtime._stack.segment = runtime._memory.mem + runtime._memory.size - runtime._stack.size;

	// Allocate the rest of the memory to the heap
	runtime._heap.size = runtime._memory.size - runtime._stack.size;
	runtime._heap.nextAlloc = 0;
	runtime._heap.segment = runtime._memory.mem;

	RuntimePushScope(&runtime, GLOBAL_VARIABLE_SCOPE_CAPACITY);
	RuntimeNumber* num = PushRuntimeNumber(&runtime);
	num->value = 3.14;
	VariableDictionaryInsert(runtime._currentScope, (RuntimeVariable) { .value = (RuntimeValue*) num, .key = ConstStringView("x") });

	return runtime;
}

RuntimeValue* EvaluateStatement(FernRuntime* runtime, Statement* stmt) {
	THROW_IF_NULL(runtime, NULL);
	THROW_IF_NULL(stmt, NULL);

	if(!runtime->_memory.mem)
		return NULL;

	switch(stmt->type) {
	case PROGRAM_AST_NODE:
		return EvaluateProgramAST(runtime, (ProgramAST*) stmt);
	case BINARY_EXPR_NODE:
		return EvaluateBinaryExpr(runtime, (BinaryExpr*) stmt);
	case UNARY_EXPR_NODE:
		return EvaluateUnaryExpr(runtime, (UnaryExpr*) stmt);
	case NUMERIC_LITERAL_NODE:
		return EvaluateNumericLiteral(runtime, (NumericLiteral*) stmt);
	case IDENTIFIER_NODE:
		return EvaluateIdentifier(runtime, (Identifier*) stmt);
	case NULL_LITERAL_NODE:
		return (RuntimeValue*) PushRuntimeNull(runtime);
	default:;
		LogWarningV("Interpreter does not have an implmentation for ast node of type %s", NodeTypeAsString(stmt->type));
	}

	return NULL;
}
