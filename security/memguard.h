// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_MEMGUARD_H
#define OBSIDIAN_MEMGUARD_H

#include "../include/types.h"

void memguard_poison_free(void *ptr, u32 size);
bool memguard_check_allocation(const void *ptr, u32 size, u32 align);
u32  memguard_violations(void);

#endif
