// SPDX-License-Identifier: GPL-2.0-only
#include "pmm.h"
#include "../../include/obsidian.h"
#include "../../lib/string.h"

#define FRAME_SIZE   4096ULL
#define MAX_FRAMES   (512ULL * 1024)	/* 2TB ceiling */
#define BITMAP_WORDS (MAX_FRAMES / 64)

static u64 bitmap[BITMAP_WORDS];
static u64 highest_frame = 0;

static inline void bitmap_set(u64 f)   { bitmap[f / 64] |=  (1ULL << (f % 64)); }
static inline void bitmap_clear(u64 f) { bitmap[f / 64] &= ~(1ULL << (f % 64)); }
static inline int  bitmap_test(u64 f)  { return (bitmap[f / 64] >> (f % 64)) & 1; }

static void reserve_range(u64 start, u64 end) {
	u64 first = start / FRAME_SIZE;
	u64 last  = (end + FRAME_SIZE - 1) / FRAME_SIZE;
	if (last > MAX_FRAMES) last = MAX_FRAMES;
	for (u64 f = first; f < last; f++) bitmap_set(f);
}

static void free_range(u64 start, u64 end) {
	u64 first = start / FRAME_SIZE;
	u64 last  = end   / FRAME_SIZE;
	if (last > MAX_FRAMES) last = MAX_FRAMES;
	for (u64 f = first; f < last; f++) {
		bitmap_clear(f);
		if (f > highest_frame) highest_frame = f;
	}
}

u64 pmm_init(mb2_info_t *mbi) {
	for (u64 i = 0; i < BITMAP_WORDS; i++) bitmap[i] = ~0ULL;

	u64 usable_kb = 0;

	/* walk Multiboot2 tags */
	mb2_tag_t *tag = (mb2_tag_t *)((u8 *)mbi + 8);
	while (tag->type != MB2_TAG_END) {
		if (tag->type == MB2_TAG_MMAP) {
			mb2_mmap_tag_t *m = (mb2_mmap_tag_t *) tag;
			mb2_mmap_entry_t *e = m->entries;
			mb2_mmap_entry_t *end =
				(mb2_mmap_entry_t *)((u8 *)m + m->tag.size);
			while (e < end) {
				if (e->type == MB2_MMAP_AVAILABLE) {
					free_range(e->base_addr, e->base_addr + e->length);
					usable_kb += e->length / 1024;
				}
				e = (mb2_mmap_entry_t *)((u8 *)e + m->entry_size);
			}
		}
		/* tags are 8-byte aligned */
		tag = (mb2_tag_t *)(((uintptr_t)tag + tag->size + 7) & ~7ULL);
	}

	/* low 1MB + kernel image: never hand out */
	reserve_range(0, 1024 * 1024);
	reserve_range(1024 * 1024, (u64)(uintptr_t) _kernel_end);

	return usable_kb;
}

/* Mark an arbitrary physical range as permanently in use (kernel
   image extensions such as the heap live here). */
void pmm_reserve(u64 start, u64 end) {
	reserve_range(start, end);
}

u64 pmm_alloc_frame(void) {
	for (u64 f = 0; f <= highest_frame; f++) {
		if (!bitmap_test(f)) {
			bitmap_set(f);
			return f * FRAME_SIZE;
		}
	}
	return 0;
}

void pmm_free_frame(u64 phys_addr) {
	bitmap_clear(phys_addr / FRAME_SIZE);
}
