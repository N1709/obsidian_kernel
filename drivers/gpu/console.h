// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_CONSOLE_H
#define OBSIDIAN_CONSOLE_H

#include "../../include/types.h"

/* Text attribute byte: VGA color pair used by both backends. */
#define CON_NORMAL 0x0F
#define CON_DIM    0x07
#define CON_OK     0x0A
#define CON_WARN   0x0E
#define CON_ERR    0x0C
#define CON_PANIC  0x4F

struct console_ops {
	int cols;
	int rows;
	void (*clear)(void);
	void (*putc_at)(int col, int row, char c, u8 attr);
	void (*scroll)(int lines);
};

/* A display backend registers here once; kprintf always goes through
   these wrappers so the rest of the kernel never touches hardware. */
void console_install(const struct console_ops *ops);
void console_init(void);

void console_putc(char c);
void console_print(const char *s);
void console_print_c(const char *s, u8 attr);
void console_set_attr(u8 attr);
u8   console_get_attr(void);
void console_clear(void);
void console_flush(void);

#endif
