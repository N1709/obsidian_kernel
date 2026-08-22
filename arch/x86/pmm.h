// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_PMM_H
#define OBSIDIAN_PMM_H

#include "../include/types.h"
#include "multiboot.h"

/* Builds the free-frame bitmap from the multiboot memory map (or from
   mem_lower/mem_upper if no map was supplied), then reserves the frames
   the kernel image itself occupies. Returns total usable RAM in KB. */
u32 pmm_init(multiboot_info_t *mbi);

/* Returns the physical address of a free 4K frame, or 0 if out of
   memory. The frame is marked used before it's returned. */
void pmm_reserve(u32 phys_start, u32 phys_end);
u32 pmm_alloc_frame(void);

void pmm_free_frame(u32 phys_addr);

#endif
