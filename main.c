#include "include/logging.h"
#include "include/errors.h"
#include "include/memarena.h"
#include "include/arrays.h"
#include "include/parser.h"
#include "include/interpereter.h"
#include "include/strings.h"

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#define ARGUMENTS_WORK_BUFFER_LENGTH 4096
#define CONTEXT_SIZE 65536

const char* ArgsHelp =
"Run a FernScript file. Must provide path to script file with '-i' or a command to evaluate with '-c'\n"\
"  -h --help: Display this help message\n"\
"  -i --input: Script file to run\n"\
"  -c --command: Command string to evaluate (instead of a file)\n"\
"  -d --debug: Display detailed information (tokens, ast, etc.). Used for debugging\n";

void SilenceErrors(ErrorData* e) {}

void HandleErrors(ErrorData* e) {
	LogErrorV("An %s error had occured on %s:%d: ", e->typeStr,  e->fileName, e->line);
	fprintf(stderr, "\t%s\n\n", e->message);
}

void HandleArgParserErrors(ErrorData* e) {
	if(e->type == ARRAY_OUT_OF_BOUNDS || e->type == STRING_OUT_OF_BOUNDS)
		return; // ignore

	HandleErrors(e);
}

#define ArgFlag(s, l) (CmpStrings(&ConstString(s), token) == 0 || CmpStrings(&ConstString(l), token) == 0)

typedef struct {
	MemArena _workBuffer;
	StringArray _tokens;
	String input;
	String command;
	bool debug;
} Arguments;

Arguments ParseArguments(int argc, char** argv) {
	PushErrorCallbackFunction(HandleArgParserErrors);

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
		if(CharAt(&args.input, 0) == '-')
			args.input = EmptyString();
	}

	if(ArgFlag("-d", "--debug"))
		args.debug = true;

	if(ArgFlag("-c", "--command"))
		StringArrayGetValue(&args._tokens, i + 1, &args.command);
loop:
	if(++i < args._tokens.length)
		goto argToken;

	if(StrIsEmpty(&args.input) && StrIsEmpty(&args.command)) {
		fprintf(stderr, "You must provide a script's file path (with '-i') or a command (with '-c')\n");
		exit(-1);
	}

	PopErrorCallbackFunction();

	return args;
}

void FreeArguments(Arguments* args) {
	free(args->_workBuffer.buffer);
}

String ReadFile(String* filePath, MemArena* context) {
	String contents = EmptyString();
	FILE* f = fopen(AsCString(filePath), "r");

	if(!f) {
		ThrowErrorF(FILE_NOT_FOUND, "Cannot read file '%s'", AsCString(filePath));
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
	PushErrorCallbackFunction(HandleErrors);
	Arguments args = ParseArguments(argc, argv);
	MemArena context = CreateMemArena(CONTEXT_SIZE, malloc(CONTEXT_SIZE));

	String code = {};

	if(!StrIsEmpty(&args.input)) {
		if(args.debug) printf("Opening file %s\n\n", AsCString(&args.input));
		ReadFile(&args.input, &context);
	} else {
		code = args.command;
		args.input = ConstString("<command>");
	}

	// Parse file into a list of tokens
	LexerTokenArray tokens = LexerTokenize(args.input, code, &context);
	if(args.debug) {
		printf("Parsed %d tokens:\n", tokens.length);
		for(int i = 0; i < tokens.length; i++) {
			LexerToken token = {};
			LexerTokenArrayGetValue(&tokens, i, &token);
			if(token.type == TK_NEWLINE)
				printf("\\n',");
			else
				printf("'%s',", AsCString((String*) &token.string));
		}
		printf("\n\n");
	}

	// Create an ast from tokens
	ProgramAST ast = ParseTokens(tokens, &context);
	if(args.debug) {
		printf("Allocated size for AST is %d bytes\n", ast._nodeDataCapacity);
		ShowAST(ast, stdout);
		printf("\n\n");
	}

	// Interperet the ast
	FernRuntime runtime = CreateFernRuntime(ast, &context, MemArenaRemainingCapacity(&context));
	if(args.debug) {
		printf("Allocated memory for runtime is %d bytes\n", runtime._memory.size);
		printf("Allocated stack for runtime is %d bytes\n", runtime._stack.size);
	}
	RuntimeValue* ret = EvaluateStatement(&runtime, (Statement*) &ast);

	if(args.debug){
		printf("Stack ptr after evaluation is %d\n", runtime._stack.ptr);
		printf("Stack capacity after evaluation is %d\n\n", runtime._stack.size - runtime._stack.ptr);
	}

	// Print the return value of the program
	switch(ret->type) {
	case RT_NUMBER_VALUE:
		printf("%f\n", ((RuntimeNumber*) ret)->value);
		break;
	case RT_NULL_VALUE:
		printf("null\n");
		break;
	case RT_BOOLEAN_VALUE:
		printf("%s\n", ((RuntimeBoolean*) ret)->value ? "true" : "false");
		break;
	}

	free(context.buffer);
	FreeArguments(&args);

	return 0;
}
