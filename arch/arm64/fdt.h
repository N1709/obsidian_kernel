// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_ARM64_FDT_H
#define OBSIDIAN_ARM64_FDT_H

#include "../../include/types.h"

/*
 * Flattened Device Tree reader.
 *
 * A tiny libfdt: walks the structure block directly so the kernel can
 * learn its memory, console UART and command line from whatever blob
 * the firmware passed, exactly like linux/drivers/of/fdt.c does for
 * the mainline kernel.
 */

bool fdt_init(u64 addr);

const char *fdt_chosen_bootargs(void);
u64         fdt_memory_base(u64 *size_out);
u64         fdt_uart_base(void);
int         fdt_cpu_count(void);

#endif
