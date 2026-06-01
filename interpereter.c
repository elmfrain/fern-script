#include "include/interpereter.h"

#include "include/logging.h"
#include "include/errors.h"
#include "include/parser.h"

#include <stdint.h>
#include <math.h>

#define STACK_SIZE_FROM_TOTAL_MEM_SIZE_DIVISOR 4
#define STACK_SIZE_MAX 131072

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

static void PopRuntimeValue(FernRuntime* runtime, RuntimeValue* value) {
	static const int s_RuntimeValueTypeSizes[] = {
		RUNTIME_VALUES(AS_RUNTIME_VALUE_SIZE)
	};

	if(!value) return;

	RuntimeStackDealloc(runtime, s_RuntimeValueTypeSizes[(int) value->type]);
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
	else if(BinaryExprOpEquals("/")){
		if(right->value == 0.0) value = INFINITY;
		else value = left->value / right->value;
	} else if(BinaryExprOpEquals("%")) {
		if(right->value == 0.0) value = NAN;
		else {
			value = (int64_t) left->value % (int64_t) right->value;
			value += left->value - floor(left->value);
			value -= value < 0 ? 1 : 0;
		}
	}
	else
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
	case NULL_LITERAL_NODE:
		return (RuntimeValue*) PushRuntimeNull(runtime);
	default:;
		LogWarningV("Interpreter does not have an implmentation for ast node of type %s", NodeTypeAsString(stmt->type));
	}

	return NULL;
}
