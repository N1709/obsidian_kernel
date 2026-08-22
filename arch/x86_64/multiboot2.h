// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_MULTIBOOT2_H
#define OBSIDIAN_MULTIBOOT2_H

#include "../../include/types.h"

/* Multiboot2 specification constants */
#define MB2_BOOTLOADER_MAGIC 0x36d76289

#define MB2_TAG_END     0
#define MB2_TAG_CMDLINE 1
#define MB2_TAG_MMAP    6

#define MB2_MMAP_AVAILABLE 1

typedef struct {
	u32 total_size;
	u32 reserved;
} __attribute__((packed)) mb2_info_t;

typedef struct {
	u32 type;
	u32 size;
} __attribute__((packed)) mb2_tag_t;

typedef struct {
	u64 base_addr;
	u64 length;
	u32 type;
	u32 reserved;
} __attribute__((packed)) mb2_mmap_entry_t;

typedef struct {
	mb2_tag_t tag;
	u32       entry_size;
	u32       entry_version;
	mb2_mmap_entry_t entries[];
} __attribute__((packed)) mb2_mmap_tag_t;

typedef struct {
	mb2_tag_t tag;
	char      string[];
} __attribute__((packed)) mb2_cmdline_tag_t;

#endif
