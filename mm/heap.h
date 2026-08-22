// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_HEAP_H
#define OBSIDIAN_HEAP_H

#include "../include/types.h"

/* Heap statistics shared by both allocator backends. */
struct heap_stats {
	u32 total_bytes;
	u32 used_bytes;
	u32 free_bytes;
	u32 high_water;
	u32 alloc_count;
	u32 free_count;
	u32 errors;
};

bool        heap_init(u32 base, u32 size);
void       *kmalloc(u32 size);
void       *kzalloc(u32 size);
void       *krealloc(void *ptr, u32 size);
void        kfree(void *ptr);
const char *heap_backend_name(void);
bool        heap_selftest(void);

#endif
