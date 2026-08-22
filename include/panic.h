// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_PANIC_H
#define OBSIDIAN_PANIC_H

/* Halts the machine. Writes the panic flag into CMOS NVRAM so the Secure
   kernel can report "previous boot panicked" on the next boot. */
void panic(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));

#endif
