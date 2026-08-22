// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_ARM64_PL011_H
#define OBSIDIAN_ARM64_PL011_H

#include "../../include/types.h"

/*
 * PrimeCell PL011 UART driver, the console device of every ARM
 * dev board that matters including QEMU virt at 0x09000000.
 * Register layout follows the PL011 technical reference manual,
 * matching linux/drivers/tty/serial/amba-pl011.c behaviour.
 */

#define PL011_BASE_DEFAULT 0x09000000ull

void pl011_init(u64 base);
void pl011_putc(char c);
int  pl011_getc(void);

/* Console glue: overrides the weak fallback so printk reaches the
   serial line before any display backend exists. */
void __console_fallback_putc(char ch);

#endif
