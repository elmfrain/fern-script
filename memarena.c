#include "include/memarena.h"

#include "include/errors.h"
#include "include/logging.h"

MemArena CreateMemArena(int capacity, void* mem) {
	MemArena arena = {
		.buffer = mem,
		.capacity = capacity & ~0xF,
	};

	if((size_t) mem % 16 != 0)
		LogWarningV("Creating memory arena from memory (void*=%p) that is not aligned in 16 byte blocks.", mem);

	return arena;
}

void* MemArenaAlloc(MemArena* arena, int capacity) {
	// Make the actual capacity memory aligned to 16 byte blocks, so add more memory if needed
	capacity = (capacity + 15) & ~0xF;
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

int MemArenaRemainingCapacity(MemArena* arena) {
	return arena->capacity - arena->nextAlloc;
}
