// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_PRINTK_H
#define OBSIDIAN_PRINTK_H

#include <stdarg.h>
#include "types.h"

int  kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void kputs(const char *s);
void kputc(char c);

/* Used by panic() to format into a fixed buffer. */
int kvsnprintf(char *buf, u32 size, const char *fmt, va_list ap)
	__attribute__((format(printf, 3, 0)));

#endif
