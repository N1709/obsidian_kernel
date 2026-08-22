// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_RAMFS_H
#define OBSIDIAN_RAMFS_H

#include "../include/types.h"

#define RAMFS_NAME_LEN   32
#define RAMFS_MAX_FILES  32

bool ramfs_init(void);
int  ramfs_create(const char *name);
int  ramfs_write(const char *name, const void *data, u32 len);
int  ramfs_read(const char *name, void *out, u32 max_len);
u32  ramfs_size(const char *name);
int  ramfs_list(char names[][RAMFS_NAME_LEN], int max);

#endif
