#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

#include <stdio.h>

/* List of node types that is used in a program's AST */
#define PARSER_NODES(X)\
	X(PROGRAM_AST, ProgramAST)\
	X(STATEMENT, Statement)\
	X(EXPRESSION, Expression)\
	X(NUMERIC_LITERAL, NumericLiteral)\
	X(IDENTIFIER, Identifier)\
	X(BINARY_EXPR, BinaryExpr)

/* Spell out the name of a node's type as enums */
#define AS_NODE_TYPE_ENUM(NAME, _) NAME##_NODE,

typedef enum {
	PARSER_NODES(AS_NODE_TYPE_ENUM)
} NodeType;

#define INHERIT_STATEMENT_NODE\
	NodeType type;\
	int numChildren;\
	int firstChildIndex;\
	int nextSiblingIndex;

/* Statements do not evaluate to a value */
typedef struct {
	INHERIT_STATEMENT_NODE;
} Statement;

/* Expressions do eventually evaluate to a value */
typedef struct {
	INHERIT_STATEMENT_NODE;
} Expression;

/* Root of AST */
typedef struct {
	INHERIT_STATEMENT_NODE;
	void* _nodeData;
	int _nodeDataCapacity;
	int _nextAlloc;
} ProgramAST;

/* Number as constants */
typedef struct {
	INHERIT_STATEMENT_NODE;
	double value;
} NumericLiteral;

/* Indentifiers likes variables and function names */
typedef struct {
	INHERIT_STATEMENT_NODE;
	StringView symbol;
} Identifier;

/* Two expressions separated by a binary operator */
typedef struct {
	INHERIT_STATEMENT_NODE;
	int leftExprIndex;
	int rightExprIndex;
	StringView oper;
} BinaryExpr;

typedef struct {
	int _currNodeIndex;
	ProgramAST* _root;
	int index;
	int size;
} NodeChildrenIterator;

/* Get pointer to a node in the AST */
Statement* NodeGet(ProgramAST* root, int index);

/* Start iterating through a node's children */
NodeChildrenIterator NodeChildrenItr(ProgramAST* root, Statement* parent);

/* Get pointer to a child that is currently selected. Returns null if at the end of list */
Statement* NodeChildrenGet(NodeChildrenIterator* itr);

/* Goto to the next child */
void NodeChildrenNext(NodeChildrenIterator* itr);

/* Parse tokens into an abstract syntax tree */
ProgramAST ParseTokens(LexerTokenArray tokens, MemArena* context);

/* Print out the structure of the AST to a file */
void ShowAST(ProgramAST ast, FILE* file);

#endif // PARSER_H
