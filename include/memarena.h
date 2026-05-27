#ifndef MEM_ARENA_H
#define MEM_ARENA_H

typedef struct {
	char* buffer;
	int capacity;
	int nextAlloc;
} MemArena;

/* Create a memory arena from a given block of memory */
MemArena CreateMemArena(int capacity, void* mem);

/* Allocate memory from the arena's buffer using linear allocation */
void* MemArenaAlloc(MemArena* arena, int capacity);

#endif // MEM_ARENA_H
