#include "include/interpereter.h"

#include "include/logging.h"
#include "include/errors.h"
#include "include/parser.h"

#define AS_RUNTIME_VALUE_STR(NAME, _) #NAME,
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

#define AS_RUNTIME_VALUE_ALLOC_FUNCS(NAME, TYPE)\
	TYPE* Alloc##TYPE(FernRuntime* runtime) {\
		THROW_IF_NULL(runtime, NULL);\
		if(runtime->_memoryCapacity < runtime->_nextAlloc + sizeof(TYPE)) {\
			ThrowErrorF(\
				FAILED_MEMORY_ALLOCATION,\
				"Not enough space to allocate a '%s' runtime value in runtime (remaining capacity %d)",\
				#TYPE,\
				runtime->_memoryCapacity - runtime->_nextAlloc\
			);\
			return NULL;\
		}\
		int nextIndex = runtime->_nextAlloc;\
		runtime->_nextAlloc += sizeof(TYPE);\
		TYPE* newValue = (TYPE*) &((char*) runtime->_memory)[nextIndex];\
		newValue->type = _AS_RUNTIME_VALUE_TYPE_ENUM(NAME##_VALUE);\
		return newValue;\
	}

// static const char* s_RuntimeValueTypesStrings[] = {
// 	RUNTIME_VALUES(AS_RUNTIME_VALUE_STR)
// };

RUNTIME_VALUES(AS_RUNTIME_VALUE_ALLOC_FUNCS);

#define RuntimeNullValue(runtime) (RuntimeValue*) &runtime->nullValue;

static RuntimeValue* EvaluateProgramAST(FernRuntime* runtime, ProgramAST* program) {
	RuntimeValue* lastEvaluated = RuntimeNullValue(runtime);

	NodeChildrenIterator i = NodeChildrenItr(&runtime->root, (Statement*) program);
	for(Statement* stmt; (stmt = NodeChildrenGet(&i)); NodeChildrenNext(&i))
		lastEvaluated = EvaluateStatement(runtime, stmt);

	return lastEvaluated;
}

#define BinaryExprOpEquals(oper) (StrEquals((String*) &bExpr->op, &ConstString(oper)))

static RuntimeValue* EvaluateBinaryExpr(FernRuntime* runtime, BinaryExpr* bExpr) {
	NodeChildrenIterator i = NodeChildrenItr(&runtime->root, (Statement*) bExpr);
	RuntimeValue* leftValue = EvaluateStatement(runtime, NodeChildrenGet(&i));
	NodeChildrenNext(&i);
	RuntimeValue* rightValue = EvaluateStatement(runtime, NodeChildrenGet(&i));

	if(leftValue->type != RT_NUMBER_VALUE || rightValue->type != RT_NUMBER_VALUE)
		return RuntimeNullValue(runtime);

	RuntimeNumber* left = (RuntimeNumber*) leftValue;
	RuntimeNumber* right = (RuntimeNumber*) rightValue;
	RuntimeNumber* result = AllocRuntimeNumber(runtime);

	if(!result)
		return RuntimeNullValue(runtime);

	if(BinaryExprOpEquals("+"))
		result->value = left->value + right->value;
	else if(BinaryExprOpEquals("-"))
		result->value = left->value - right->value;
	else if(BinaryExprOpEquals("*"))
		result->value = left->value * right->value;
	else if(BinaryExprOpEquals("/"))
		result->value = left->value / right->value;
	else
		return RuntimeNullValue(runtime);

	return (RuntimeValue*) result;
}

FernRuntime CreateFernRuntime(ProgramAST root, MemArena* arena, int memoryCapacity) {
	FernRuntime runtime = { .nullValue = { .type = RT_NULL_VALUE } };

	runtime._memory = MemArenaAlloc(arena, memoryCapacity);
	if(!runtime._memory){
		ThrowErrorF(FAILED_MEMORY_ALLOCATION, "Cannot allocate enough space to runtime memory (requested=%d)", memoryCapacity);
		return runtime;
	}

	runtime._memoryCapacity = memoryCapacity;
	runtime.root = root;

	return runtime;
}

RuntimeValue* EvaluateStatement(FernRuntime* runtime, Statement* stmt) {
	THROW_IF_NULL(runtime, NULL);
	THROW_IF_NULL(stmt, NULL);

	if(!runtime->_memory)
		return RuntimeNullValue(runtime);

	switch(stmt->type) {
	case PROGRAM_AST_NODE:
		return EvaluateProgramAST(runtime, (ProgramAST*) stmt);
	case BINARY_EXPR_NODE:
		return EvaluateBinaryExpr(runtime, (BinaryExpr*) stmt);
	case NUMERIC_LITERAL_NODE: {
		NumericLiteral* literal = (NumericLiteral*) stmt;
		RuntimeNumber* num = AllocRuntimeNumber(runtime);
		if(!num) return RuntimeNullValue(runtime);
		num->value = literal->value;
		return (RuntimeValue*) num;
	}
	default:;
		LogWarningV("Interpreter does not have an implmentation for ast node of type %s", NodeTypeAsString(stmt->type));
	}

	return RuntimeNullValue(runtime);
}
