// SPDX-License-Identifier: GPL-2.0-only
#include "heap.h"
#include "../security/memguard.h"
#include "../lib/string.h"

/*
 * C reference allocator.
 *
 * First-fit free list over a single region with address-ordered free
 * blocks so adjacent frees coalesce. Every block carries a 16-byte
 * header whose magic detects double frees and wild pointers. This file
 * implements exactly the same ABI as the Rust backend in mm/alloc, so
 * the build links one or the other.
 */

struct block_hdr {
	u32 magic;
	u32 size;
	u32 next_off;
	u32 used;
};

#define HDR_SIZE      sizeof(struct block_hdr)
#define ALIGN         16u
#define MAGIC_USED    0x0B51D1A0u
#define MAGIC_FREE    0xF4EEB10Cu
#define END_OF_LIST   0xFFFFFFFFu

static u8 *hbase;
static u32 hsize;
static u32 free_head = END_OF_LIST;
static struct heap_stats st;

static u32 align_up(u32 v) {
	return (v + ALIGN - 1) & ~(ALIGN - 1);
}

bool heap_init(u32 base, u32 size) {
	hbase = (u8 *)(uintptr_t)((base + ALIGN - 1) & ~(ALIGN - 1));
	hsize = align_up(size);
	free_head = 0;

	struct block_hdr *first = (struct block_hdr *)hbase;

	first->magic = MAGIC_FREE;
	first->size = hsize - HDR_SIZE;
	first->next_off = END_OF_LIST;
	first->used = 0;

	st.total_bytes = first->size;
	st.free_bytes = first->size;
	return true;
}

static struct block_hdr *hdr_at(u32 off) {
	return (struct block_hdr *)(hbase + off);
}

void *kmalloc(u32 size) {
	if (!size || !hbase)
		return NULL;

	u32 need = align_up(size);

	u32 prev_off = END_OF_LIST;
	u32 cur_off = free_head;

	while (cur_off != END_OF_LIST) {
		struct block_hdr *cur = hdr_at(cur_off);

		if (cur->size >= need + HDR_SIZE) {
			/* Split when the remainder can hold header+payload. */
			if (cur->size >= need + HDR_SIZE + ALIGN) {
				u32 new_off = cur_off + HDR_SIZE + need;
				struct block_hdr *rest = hdr_at(new_off);

				rest->magic = MAGIC_FREE;
				rest->size = cur->size - need - HDR_SIZE;
				rest->next_off = cur->next_off;
				rest->used = 0;

				cur->size = need;
				cur->next_off = new_off;
			}

			if (prev_off == END_OF_LIST)
				free_head = cur->next_off;
			else
				hdr_at(prev_off)->next_off = cur->next_off;

			cur->used = 1;
			cur->magic = MAGIC_USED;

			void *ptr = hbase + cur_off + HDR_SIZE;

			st.used_bytes += cur->size;
			st.free_bytes -= cur->size;
			st.alloc_count++;
			if (st.used_bytes > st.high_water)
				st.high_water = st.used_bytes;

			memguard_check_allocation(ptr, cur->size, ALIGN);
			return ptr;
		}
		prev_off = cur_off;
		cur_off = cur->next_off;
	}
	st.errors++;
	return NULL;
}

void kfree(void *ptr) {
	if (!ptr)
		return;

	u32 off = (u32)((u8 *)ptr - hbase - HDR_SIZE);

	if ((s32)off < 0 || off >= hsize) {
		st.errors++;
		return;
	}

	struct block_hdr *cur = hdr_at(off);

	if (cur->magic != MAGIC_USED || !cur->used) {
		st.errors++;		/* double free or wild pointer */
		return;
	}

	memguard_poison_free(ptr, cur->size);

	st.used_bytes -= cur->size;
	st.free_bytes += cur->size;
	st.free_count++;

	cur->magic = MAGIC_FREE;
	cur->used = 0;

	/* Insert keeping the free list ordered by offset, merging with
	   the previous and following blocks when physically adjacent. */
	u32 prev_off = END_OF_LIST;
	u32 walk = free_head;

	while (walk != END_OF_LIST && walk < off) {
		prev_off = walk;
		walk = hdr_at(walk)->next_off;
	}

	cur->next_off = walk;
	if (prev_off == END_OF_LIST)
		free_head = off;
	else {
		hdr_at(prev_off)->next_off = off;
		struct block_hdr *prev = hdr_at(prev_off);

		if (prev_off + HDR_SIZE + prev->size == off) {
			prev->size += HDR_SIZE + cur->size;
			prev->next_off = cur->next_off;
			cur = prev;
			off = prev_off;
		}
	}

	if (off + HDR_SIZE + cur->size == cur->next_off &&
	    cur->next_off != END_OF_LIST) {
		struct block_hdr *nxt = hdr_at(cur->next_off);

		if (nxt->magic == MAGIC_FREE && !nxt->used) {
			cur->size += HDR_SIZE + nxt->size;
			cur->next_off = nxt->next_off;
		}
	}
}

void *krealloc(void *ptr, u32 size) {
	if (!ptr)
		return kmalloc(size);
	if (!size) {
		kfree(ptr);
		return NULL;
	}

	struct block_hdr *cur =
		(struct block_hdr *)((u8 *)ptr - HDR_SIZE);
	void *fresh = kmalloc(size);

	if (!fresh)
		return NULL;

	memcpy(fresh, ptr,
	       cur->size < size ? cur->size : size);
	kfree(ptr);
	return fresh;
}

const char *heap_backend_name(void) {
	return "C freelist";
}
