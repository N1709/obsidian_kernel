// SPDX-License-Identifier: GPL-2.0-only
#include "pmm.h"
#include "../../include/obsidian.h"
#include "../../lib/string.h"

#define FRAME_SIZE   4096
#define MAX_FRAMES   131072
#define BITMAP_WORDS (MAX_FRAMES / 32)

static u32 bitmap[BITMAP_WORDS];
static u32 highest_frame = 0;

static inline void bitmap_set(u32 f)   { bitmap[f / 32] |=  (1u << (f % 32)); }
static inline void bitmap_clear(u32 f) { bitmap[f / 32] &= ~(1u << (f % 32)); }
static inline int  bitmap_test(u32 f)  { return (bitmap[f / 32] >> (f % 32)) & 1; }

static void reserve_range(u32 phys_start, u32 phys_end) {
	u32 first = phys_start / FRAME_SIZE;
	u32 last  = (phys_end + FRAME_SIZE - 1) / FRAME_SIZE;
	if (last > MAX_FRAMES) last = MAX_FRAMES;
	for (u32 f = first; f < last; f++) bitmap_set(f);
}

static void free_range(u32 phys_start, u32 phys_end) {
	u32 first = phys_start / FRAME_SIZE;
	u32 last  = phys_end   / FRAME_SIZE;
	if (last > MAX_FRAMES) last = MAX_FRAMES;
	for (u32 f = first; f < last; f++) {
		bitmap_clear(f);
		if (f > highest_frame) highest_frame = f;
	}
}

u32 pmm_init(multiboot_info_t *mbi) {
	for (u32 i = 0; i < BITMAP_WORDS; i++) bitmap[i] = 0xFFFFFFFF;

	u32 usable_kb = 0;

	if (mbi->flags & MB_FLAG_MMAP) {
		u32 off = mbi->mmap_addr;
		u32 end = mbi->mmap_addr + mbi->mmap_length;
		while (off < end) {
			multiboot_mmap_entry_t *e = (multiboot_mmap_entry_t *) off;
			if (e->type == MB_MMAP_AVAILABLE && e->addr_high == 0) {
				u32 start = e->addr_low;
				u32 len   = e->len_low;
				free_range(start, start + len);
				usable_kb += len / 1024;
			}
			off += e->size + 4;
		}
	} else if (mbi->flags & MB_FLAG_MEM) {
		free_range(0, mbi->mem_lower * 1024);
		free_range(1024 * 1024, 1024 * 1024 + mbi->mem_upper * 1024);
		usable_kb = mbi->mem_lower + mbi->mem_upper;
	}

	/* low 1MB + kernel image: never hand out */
	reserve_range(0, 1024 * 1024);
	reserve_range(1024 * 1024, (u32) _kernel_end);

	return usable_kb;
}

/* Mark an arbitrary physical range as permanently in use. */
void pmm_reserve(u32 phys_start, u32 phys_end) {
	reserve_range(phys_start, phys_end);
}

u32 pmm_alloc_frame(void) {
	for (u32 f = 0; f <= highest_frame; f++) {
		if (!bitmap_test(f)) {
			bitmap_set(f);
			return f * FRAME_SIZE;
		}
	}
	return 0;
}

void pmm_free_frame(u32 phys_addr) {
	bitmap_clear(phys_addr / FRAME_SIZE);
}
