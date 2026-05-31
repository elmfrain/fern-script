#include "include/parser.h"

#include "include/lexer.h"
#include "include/logging.h"
#include "include/errors.h"
#include <stdlib.h>

#define MINIMUM_AST_NODE_DATA_CAPACITY 128

#define AS_NODE_TYPE_STR(_, NAME) #NAME,
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

#define AS_NODE_ALLOC_FUNCS(NAME, TYPE)\
	TYPE* Alloc##TYPE(ProgramAST* root) {\
		THROW_IF_NULL(root, NULL);\
		if(root->_nodeDataCapacity < root->_nextAlloc + sizeof(TYPE)) {\
			ThrowErrorF(\
				FAILED_MEMORY_ALLOCATION,\
				"Not enough space to allocate a '%s' node in AST tree (remaining capacity %d)",\
				#TYPE,\
				root->_nodeDataCapacity - root->_nextAlloc\
			);\
			return NULL;\
		}\
		int nextIndex = root->_nextAlloc;\
		root->_nextAlloc += sizeof(TYPE);\
		TYPE* newNode = (TYPE*) NodeGetFromIndex(root, nextIndex);\
		if(!newNode)\
			return NULL;\
		*newNode = (TYPE) {};\
		newNode->index = nextIndex;\
		newNode->type = NAME##_NODE;\
		return newNode;\
	}

PARSER_NODES(AS_NODE_ALLOC_FUNCS)

static const char* s_NodeTypesStrings[] = {
	PARSER_NODES(AS_NODE_TYPE_STR)
};

static void _ShowAST(ProgramAST* root, Statement* ast, FILE* file, int indent) {
	for(int i = 0; i < indent; i++)
		fprintf(file, "  ");

	fprintf(file, "%s", s_NodeTypesStrings[(int) ast->type]);
	// fprintf(file, " [%d]", ast->index);
	fprintf(file, ": ");

	switch(ast->type) {
		case NUMERIC_LITERAL_NODE:
			fprintf(file, "%f", ((NumericLiteral*) ast)->value);
			break;
		case IDENTIFIER_NODE:
			fprintf(file, "'%s'", AsCString(((String*) &((Identifier*) ast)->symbol)));
			break;
		case BINARY_EXPR_NODE:
			fprintf(file, "%s", AsCString((String*) &((BinaryExpr*) ast)->op));
			break;
		default:;
	}

	fprintf(file, "\n");

	NodeChildrenIterator i = NodeChildrenItr(root, ast);
	for(Statement* child; (child = NodeChildrenGet(&i)); NodeChildrenNext(&i))
		_ShowAST(root, child, file, indent + 1);
}

static void NodeAddChild(ProgramAST* root, Statement* parent, Statement* child) {
	THROW_IF_NULL(root,);
	THROW_IF_NULL(parent,);
	THROW_IF_NULL(child,);

	if(parent->numChildren == 0) {
		parent->firstChildIndex = parent->lastChildIndex = child->index;
	} else {
		Statement* lastChild = NodeGetFromIndex(root, parent->lastChildIndex);
		lastChild->nextSiblingIndex = child->index;
		parent->lastChildIndex = child->index;
	}

	parent->numChildren++;
}

#define ReturnIfNull(ptr, ret) if(!ptr) return ret;
#define TokenEquals(token, str) (StrEquals((String*) &token.string, &ConstString(str)))

// AST parser funcs
static Expression* ParsePrimaryExpression(ProgramAST* root, LexerTokenArrayStream* tokens);
static Expression* ParseMultiplicativeExpression(ProgramAST* root, LexerTokenArrayStream* tokens);
static Expression* ParseAdditiveExpression(ProgramAST* root, LexerTokenArrayStream* tokens);
static Expression* ParseExpression(ProgramAST* root, LexerTokenArrayStream* tokens);
static Statement* ParseStatement(ProgramAST* root, LexerTokenArrayStream* tokens);

static Expression* ParsePrimaryExpression(ProgramAST* root, LexerTokenArrayStream* tokens) {
	LexerToken token = {};
	if(!LexerTokenArrayStreamGet(tokens, &token)) return NULL;

	switch(token.type) {
	case TK_IDENTIFIER: {
		Identifier* id = AllocIdentifier(root);
		ReturnIfNull(id, NULL);
		id->symbol = token.string;
		return (Expression*) id;
	}
	case TK_NUMBER: {
		NumericLiteral* num = AllocNumericLiteral(root);
		ReturnIfNull(num, NULL);
		num->value = atof(AsCString((String*) &token.string));
		return (Expression*) num;
	}
	case TK_OPEN_PAREN: {
		Expression* expr = ParseExpression(root, tokens);
		if(!LexerTokenArrayStreamGet(tokens, &token) || token.type != TK_CLOSE_PAREN) {
			ThrowError(EXPECTED_TOKEN, "Expected a closing parenthesis ')'");
			return NULL;
		}
		return expr;
	}
	case TK_NULL_KEYWORD: {
		NullLiteral* null = AllocNullLiteral(root);
		return (Expression*) null;
	}
	default:;
		ThrowErrorF(UNEXPECTED_TOKEN, "Encountered unexpected token '%s' as primary expression", AsCString((String*) &token.string));
	}

	return NULL;
}

static Expression* ParseMultiplicativeExpression(ProgramAST* root, LexerTokenArrayStream* tokens) {
	Expression* left = ParsePrimaryExpression(root, tokens);
	ReturnIfNull(left, NULL);

	LexerToken token;
	if(!LexerTokenArrayStreamPeek(tokens, &token))
		return left;

	while(TokenEquals(token, "*") || TokenEquals(token, "/")) {
		LexerTokenArrayStreamGet(tokens, &token);

		Expression* right = ParsePrimaryExpression(root, tokens);
		if(!right) {
			ThrowError(EXPECTED_EXPRESSION, "Expected an expression on the right hand side of an multiplicative expression");
			return NULL;
		}
		BinaryExpr* binExpr = AllocBinaryExpr(root);
		NodeAddChild(root, (Statement*) binExpr, (Statement*) left);
		NodeAddChild(root, (Statement*) binExpr, (Statement*) right);
		binExpr->op = token.string;

		left = (Expression*) binExpr;

		if(!LexerTokenArrayStreamPeek(tokens, &token)) break;
	}

	return left;
}

static Expression* ParseAdditiveExpression(ProgramAST* root, LexerTokenArrayStream* tokens) {
	Expression* left = ParseMultiplicativeExpression(root, tokens);
	ReturnIfNull(left, NULL);

	LexerToken token;
	if(!LexerTokenArrayStreamPeek(tokens, &token))
		return left;

	while(TokenEquals(token, "+") || TokenEquals(token, "-")) {
		LexerTokenArrayStreamGet(tokens, &token);

		Expression* right = ParseMultiplicativeExpression(root, tokens);
		if(!right) {
			ThrowError(EXPECTED_EXPRESSION, "Expected an expression on the right hand side of an additive expression");
			return NULL;
		}
		BinaryExpr* binExpr = AllocBinaryExpr(root);
		NodeAddChild(root, (Statement*) binExpr, (Statement*) left);
		NodeAddChild(root, (Statement*) binExpr, (Statement*) right);
		binExpr->op = token.string;

		left = (Expression*) binExpr;

		if(!LexerTokenArrayStreamPeek(tokens, &token)) break;
	}

	return left;
}

static Expression* ParseExpression(ProgramAST* root, LexerTokenArrayStream* tokens) {
	return ParseAdditiveExpression(root, tokens);
}

static Statement* ParseStatement(ProgramAST* root, LexerTokenArrayStream* tokens) {
	return (Statement*) ParseExpression(root, tokens);
}

Statement* NodeGetFromIndex(ProgramAST* root, int index) {
	THROW_IF_NULL(root, NULL);

	if(index < 0 || root->_nextAlloc <= index) {
		ThrowErrorF(
			INVALID_NODE_INDEX,
			"Trying to access node at index=%d that's out of bounds of AST's node data (limit=%d)",
			index,
			root->_nextAlloc
		);
		return NULL;
	}

	return (Statement*) &((char*) root->_nodeData)[index];
}

NodeChildrenIterator NodeChildrenItr(ProgramAST* root, Statement* parent) {
	NodeChildrenIterator itr = {};

	THROW_IF_NULL(root, itr);
	THROW_IF_NULL(parent, itr);

	if(parent->numChildren == 0)
		return itr;

	itr._currNodeIndex = parent->firstChildIndex;
	itr._root = root;
	itr.size = parent->numChildren;

	return itr;
}

Statement* NodeChildrenGet(NodeChildrenIterator* itr) {
	THROW_IF_NULL(itr, NULL);

	if(itr->position == itr->size)
		return NULL;

	return NodeGetFromIndex(itr->_root, itr->_currNodeIndex);
}

void NodeChildrenNext(NodeChildrenIterator* itr) {
	THROW_IF_NULL(itr,);

	if(itr->position == itr->size)
		return;

	itr->_currNodeIndex = NodeGetFromIndex(itr->_root, itr->_currNodeIndex)->nextSiblingIndex;
	itr->position++;
}

ProgramAST ParseTokens(LexerTokenArray tokens, MemArena* context) {
	ProgramAST ast = {};

	ast.type = PROGRAM_AST_NODE;
	ast._nodeDataCapacity = tokens.length * sizeof(Statement) * 2 + MINIMUM_AST_NODE_DATA_CAPACITY;
	ast._nodeData = MemArenaAlloc(context, ast._nodeDataCapacity);

	if(!ast._nodeData) {
		ast._nodeDataCapacity = 0;
		return ast;
	}

	LexerTokenArrayStream tokensStream = LexerTokenArrayStreamCreate(tokens);
	while(LexerTokenArrayStreamHasNext(&tokensStream)) {
		Statement* stmt = ParseStatement(&ast, &tokensStream);
		if(!stmt) return (ProgramAST) {};
		NodeAddChild(&ast, (Statement*) &ast, stmt);
	}

	LogDebugV("Root children count %d, memory used %d", ast.numChildren, ast._nextAlloc);

	return ast;
}

void ShowAST(ProgramAST ast, FILE* file) {
	_ShowAST(&ast, (Statement*) &ast, file, 0);
}
