#include "pch.h"

#include "Memory.h"

#include <assert.h>

using namespace Kore;

namespace {
	const size_t memorySize = 10 * 1024 * 1024;
	const size_t scratchPadSize = 4 * 1024 * 1024;
	u8* memory;
	size_t index;
}

void Memory::init() {
	memory = new u8[memorySize];
	index = scratchPadSize;
}

void* Memory::scratchPad(size_t size) {
	assert(size < scratchPadSize);
	return memory;
}

void* Memory::allocate(size_t size) {
	assert(index + size < memorySize);
	void* data = &memory[index];
	index += size;
	return data;
}
