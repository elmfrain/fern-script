#ifndef ERRORS_H
#define ERRORS_H

#include <stdbool.h>

#define ERROR_MSG_CAPACITY 256

#ifndef NULL
#define NULL 0
#endif

#define ERROR_ENUMS(X) \
	X(ARRAY_OUT_OF_BOUNDS)\
	X(ARRAY_TOO_FULL)\
	X(STRING_OUT_OF_BOUNDS)\
	X(INVALID_SUBSTR)\
	X(FAILED_MEMORY_ALLOCATION)\
	X(UNKOWN_ARGUMENT)\
	X(NULL_POINTER_ACCESS)\
	X(FILE_NOT_FOUND)

#define AS_ERROR_ENUM(NAME) NAME,

typedef enum {
	ERROR_ENUMS(AS_ERROR_ENUM)
} ErrorType;

typedef struct {
	int line;
	const char* fileName;
	char message[ERROR_MSG_CAPACITY];
	ErrorType type;
	const char* typeStr;
	void* userData;
} ErrorData;

typedef void(*ErrorCallbackFunction)(ErrorData* data);

#define ThrowError(errType, messageFormat) _ThrowError((ErrorData) {\
	.line = __LINE__,\
	.fileName = __FILE__,\
	.type = errType,\
	.userData = NULL\
}, messageFormat)

#define ThrowErrorF(errType, messageFormat, ...) _ThrowError((ErrorData) {\
	.line = __LINE__,\
	.fileName = __FILE__,\
	.type = errType,\
	.userData = NULL\
}, messageFormat, __VA_ARGS__)

/* Set the error callback function that is called everytime an error is thrown */
void PushErrorCallbackFunction(ErrorCallbackFunction callback);
void PopErrorCallbackFunction();

/* Private error throwing trigger function */
void _ThrowError(ErrorData error, const char* fmt, ...);

#endif // ERRORS_H
