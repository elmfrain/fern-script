#include "include/errors.h"
#include "include/logging.h"

#include <stdio.h>
#include <stdarg.h>

static const char* s_ErrorTypeAsStringMap[] = {
	ERROR_ENUMS(AS_ERROR_STR)
};

static void (*s_ErrorCallbackFunction)(ErrorData* data) = NULL;

void SetErrorCallbackFunction(void(*callback)(ErrorData*)) {
	s_ErrorCallbackFunction = callback;
}

void _ThrowError(ErrorData error, const char* fmt, ...) {
	if(!s_ErrorCallbackFunction) {
		LogError("An error had occured, but no callback function set");
		return;
	}

	va_list args;
	va_start(args, fmt);
	vsnprintf(error.message, ERROR_MSG_CAPACITY, fmt, args);
	va_end(args);

	error.typeStr = s_ErrorTypeAsStringMap[(int) error.type];
	s_ErrorCallbackFunction(&error);
}
