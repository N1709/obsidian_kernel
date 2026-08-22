// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_MULTIBOOT_H
#define OBSIDIAN_MULTIBOOT_H

#include "../include/types.h"

/* Flag bits inside multiboot_info_t.flags, indicating which fields the
   bootloader actually filled in. Only the ones this kernel reads. */
#define MB_FLAG_MEM     (1 << 0)
#define MB_FLAG_MODS    (1 << 3)
#define MB_FLAG_MMAP    (1 << 6)

typedef struct {
    u32 flags;
    u32 mem_lower;
    u32 mem_upper;
    u32 boot_device;
    u32 cmdline;
    u32 mods_count;
    u32 mods_addr;
    u32 syms[4];
    u32 mmap_length;
    u32 mmap_addr;
    /* Remaining fields (drives, config table, boot loader name, APM,
       VBE) are not used by this kernel and are left out. */
} __attribute__((packed)) multiboot_info_t;

typedef struct {
    u32 mod_start;
    u32 mod_end;
    u32 string;   /* physical address of a NUL-terminated command line */
    u32 reserved;
} __attribute__((packed)) multiboot_module_t;

typedef struct {
    u32 size;
    u32 addr_low, addr_high;
    u32 len_low, len_high;
    u32 type; /* 1 = available RAM, everything else = reserved/unusable */
} __attribute__((packed)) multiboot_mmap_entry_t;

#define MB_MMAP_AVAILABLE 1

#endif
