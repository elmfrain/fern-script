#include "include/logging.h"
#include "include/errors.h"
#include "include/memarena.h"
#include "include/arrays.h"

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#define ARGUMENTS_WORK_BUFFER_LENGTH 4096
#define CONTEXT_SIZE 65536

const char* ArgsHelp =
"Tokenize and parse EasyScript into an AST\n"\
"  -h --help: Display this help message\n"\
"  -i --input: Input code file to parse\n";

// void SilenceErrors(ErrorData* e) {}

void HandleErrors(ErrorData* e) {
	LogErrorV("An %s error had occured on %s:%d: ", e->typeStr,  e->fileName, e->line);
	fprintf(stderr, "\t%s\n\n", e->message);
}

#define ArgFlag(s, l) (CmpStrings(&ConstString(s), token) == 0 || CmpStrings(&ConstString(l), token) == 0)

typedef struct {
	MemArena _workBuffer;
	StringArray _tokens;
	String input;
} Arguments;

Arguments ParseArguments(int argc, char** argv) {
	Arguments args = {
		._workBuffer = CreateMemArena(ARGUMENTS_WORK_BUFFER_LENGTH, malloc(ARGUMENTS_WORK_BUFFER_LENGTH))
	};

	args._tokens = StringArrayArenaAlloc(argc, &args._workBuffer);

	for(int i = 0; i < argc; i++) {
		String argToken = CopyCStrArenaAlloc(argv[i], &args._workBuffer);
		StringArrayAdd(&args._tokens, argToken);
	}

	int i = -0;
	String* token;
argToken:
	token = StringArrayGet(&args._tokens, i);
	if(CharAt(token, 0) == '-')
		goto argFlag;

	goto loop;
argFlag:
	if(ArgFlag("-h", "--help")) {
		printf("%s", ArgsHelp);
		exit(-1);
	}

	if(ArgFlag("-i", "--input")) {
		StringArrayGetValue(&args._tokens, i + 1, &args.input);
	}
loop:
	if(++i < args._tokens.length)
		goto argToken;

	if(StrIsEmpty(&args.input)) {
		fprintf(stderr, "You must provide the -i or --input flag\n");
		exit(-1);
	}

	return args;
}

void FreeArguments(Arguments* args) {
	free(args->_workBuffer.buffer);
}

String ReadFile(String* filePath, MemArena* context) {
	String contents;
	FILE* f = fopen(AsCString(filePath), "r");

	if(!f) {
		ThrowError(FILE_NOT_FOUND, "Cannot read file");
		return contents;
	}

	fseek(f, 0, SEEK_END);
	long fileSize = ftell(f);

	contents = StringArenaAlloc(fileSize, context);
	fseek(f, 0, SEEK_SET);

	char readBuffer[1024];
	size_t charsRead;
	while((charsRead = fread(readBuffer, 1, sizeof(readBuffer), f)))
		ConcatCStrN(&contents, charsRead, readBuffer);

	fclose(f);

	return contents;
}

int main(int argc, char** argv) {
	SetErrorCallbackFunction(HandleErrors);
	Arguments args = ParseArguments(argc, argv);
	MemArena context = CreateMemArena(CONTEXT_SIZE, malloc(CONTEXT_SIZE));

	printf("Opening file %s\n", AsCString(&args.input));
	String contents = ReadFile(&args.input, &context);

	printf("%s\n", AsCString(&contents));

	free(context.buffer);
	FreeArguments(&args);

	return 0;
}
