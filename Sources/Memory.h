#pragma once

#include <stdlib.h>

namespace Memory {
	void init();
	
	void* allocate(size_t size);
	
	template<class T> T* allocate(size_t count = 1) {
		return (T*)allocate(count * sizeof(T));
	}
	
	void* scratchPad(size_t size);
	
	template<class T> T* scratchPad(size_t count = 1) {
		return (T*)scratchPad(count * sizeof(T));
	}
}
