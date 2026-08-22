// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_X86_64_PMM_H
#define OBSIDIAN_X86_64_PMM_H

#include "../../include/types.h"
#include "multiboot2.h"

u64 pmm_init(mb2_info_t *mbi);
void pmm_reserve(u64 start, u64 end);
u64 pmm_alloc_frame(void);
void pmm_free_frame(u64 phys_addr);

#endif
