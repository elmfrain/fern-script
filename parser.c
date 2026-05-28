#include "include/parser.h"

#include "include/logging.h"
#include "include/errors.h"

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
	int Alloc##TYPE(ProgramAST* root) {\
		if(root->_nodeDataCapacity < root->_nextAlloc + sizeof(TYPE)) {\
			ThrowErrorF(\
				FAILED_MEMORY_ALLOCATION,\
				"Not enough space to allocate a '%s' node in AST tree (remaining capacity %d)"\
				#TYPE,\
				root->_nodeDataCapacity - root->_nextAlloc\
			);\
			return -1;\
		}\
		int nextIndex = root->_nextAlloc;\
		root->_nextAlloc += sizeof(TYPE);\
		Statement* newNode = NodeGet(root, nextIndex);\
		if(!newNode) return -1;\
		newNode->type = NAME##_NODE;\
		return nextIndex;\
	}

PARSER_NODES(AS_NODE_ALLOC_FUNCS)

static const char* s_NodeTypesStrings[] = {
	PARSER_NODES(AS_NODE_TYPE_STR)
};

static void _ShowAST(ProgramAST* root, Statement* ast, FILE* file, int indent) {
	for(int i = 0; i < indent; i++)
		fprintf(file, "  ");

	printf("%s", s_NodeTypesStrings[(int) ast->type]);
	fprintf(file, ":\n");

	NodeChildrenIterator i = NodeChildrenItr(root, ast);
	for(Statement* child; (child = NodeChildrenGet(&i)); NodeChildrenNext(&i))
		_ShowAST(root, child, file, indent + 1);
}

static void NodeAddChild(ProgramAST* root, Statement* parent, int newNodeIndex) {
	THROW_IF_NULL(parent,);

	if(parent->numChildren != 0) {
		Statement* node = NodeGet(root, newNodeIndex);
		node->nextSiblingIndex = parent->firstChildIndex;
	}

	parent->firstChildIndex = newNodeIndex;
	parent->numChildren++;
}

Statement* NodeGet(ProgramAST* root, int index) {
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

	if(itr->index == itr->size)
		return NULL;

	return NodeGet(itr->_root, itr->_currNodeIndex);
}

void NodeChildrenNext(NodeChildrenIterator* itr) {
	THROW_IF_NULL(itr,);

	if(itr->index == itr->size)
		return;

	itr->_currNodeIndex = NodeGet(itr->_root, itr->_currNodeIndex)->nextSiblingIndex;
	itr->index++;
}

ProgramAST ParseTokens(LexerTokenArray tokens, MemArena* context) {
	ProgramAST ast = {};

	ast.type = PROGRAM_AST_NODE;
	ast._nodeDataCapacity = tokens.length * sizeof(Statement) * 2;
	ast._nodeData = MemArenaAlloc(context, ast._nodeDataCapacity);

	if(!ast._nodeData) {
		ast._nodeDataCapacity = 0;
		return ast;
	}

	NodeAddChild(&ast, (Statement*) &ast, AllocNumericLiteral(&ast));
	NodeAddChild(&ast, (Statement*) &ast, AllocNumericLiteral(&ast));
	int test = AllocNumericLiteral(&ast);
	NodeAddChild(&ast, NodeGet(&ast, test), AllocNumericLiteral(&ast));
	NodeAddChild(&ast, (Statement*) &ast, test);

	LogDebugV("Root children count %d", ast.numChildren);

	return ast;
}

void ShowAST(ProgramAST ast, FILE* file) {
	_ShowAST(&ast, (Statement*) &ast, file, 0);
}
