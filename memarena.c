#include "include/memarena.h"

#include "include/errors.h"

MemArena CreateMemArena(int capacity, void* mem) {
	MemArena arena = {
		.buffer = mem,
		.capacity = capacity,
	};

	return arena;
}

void* MemArenaAlloc(MemArena* arena, int capacity) {
	if(arena->nextAlloc + capacity > arena->capacity) {
		ThrowErrorF(
			FAILED_MEMORY_ALLOCATION,
			"Memory arena (remaining capacity %d) does not have enough space for requested allocation (%d bytes)",
			arena->capacity - arena->nextAlloc,
			capacity
		);
		return NULL;
	}

	void* mem = arena->buffer + arena->nextAlloc;
	arena->nextAlloc += capacity;
	return mem;
}
