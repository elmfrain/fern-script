#include "include/errors.h"
#include "include/logging.h"

#include <stdio.h>
#include <stdarg.h>

#define CALLBACK_FUNC_STACK_SIZE 32

#define AS_ERROR_STR(NAME) #NAME,

static const char* s_ErrorTypeAsStringMap[] = {
	ERROR_ENUMS(AS_ERROR_STR)
};

static int s_CallbackStackIndex = -1;
static ErrorCallbackFunction s_CallbackStack[CALLBACK_FUNC_STACK_SIZE] = {};

void PushErrorCallbackFunction(ErrorCallbackFunction callback) {
	if(s_CallbackStackIndex == CALLBACK_FUNC_STACK_SIZE) {
		LogErrorV("Cannot push more callbacks; stack full (maxsize=%d)", CALLBACK_FUNC_STACK_SIZE);
		return;
	}

	s_CallbackStack[++s_CallbackStackIndex] = callback;
}

void PopErrorCallbackFunction() {
	if(s_CallbackStackIndex == -1) {
		LogError("Cannot pop more callbacks; already at empty stack");
		return;
	}

	s_CallbackStackIndex--;
}

void _ThrowError(ErrorData error, const char* fmt, ...) {
	if(s_CallbackStackIndex == -1) {
		LogError("An error had occured, but no error callbacks had been set yet.");
		return;
	}

	ErrorCallbackFunction callback = s_CallbackStack[s_CallbackStackIndex];

	if(!callback) {
		LogError("An error had occured, but callback function is null");
		return;
	}

	va_list args;
	va_start(args, fmt);
	vsnprintf(error.message, ERROR_MSG_CAPACITY, fmt, args);
	va_end(args);

	error.typeStr = s_ErrorTypeAsStringMap[(int) error.type];
	callback(&error);
}
