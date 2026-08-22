// SPDX-License-Identifier: GPL-2.0-only
#include "console.h"
#include "vga_text.h"

/*
 * Console layer between kprintf and whatever display backend is live.
 * It boots on VGA text and can be re-bound to a GPU framebuffer by a
 * graphics driver at any time without callers noticing the switch.
 */

/* Weak default so architectures without early serial need no stub.
   ARM64 overrides this with its PL011 writer before any driver runs. */
__attribute__((weak)) void __console_fallback_putc(char ch) {
	(void)ch;
}

static const struct console_ops *con;

static int con_col;
static int con_row;
static u8  con_attr = CON_NORMAL;

void console_install(const struct console_ops *ops) {
	con = ops;
	con_col = 0;
	con_row = 0;
	if (con && con->clear)
		con->clear();
}

void console_init(void) {
	console_install(vga_text_ops());
}

void console_set_attr(u8 attr) { con_attr = attr; }
u8   console_get_attr(void)     { return con_attr; }

void console_clear(void) {
	con_col = 0;
	con_row = 0;
	if (con && con->clear)
		con->clear();
}

void console_putc(char c) {
	if (!con) {
		/* No backend installed yet (early serial boot on ARM):
		   hand characters straight to the arch fallback. */
		__console_fallback_putc(c);
		return;
	}

	switch (c) {
	case '\n':
		con_col = 0;
		con_row++;
		break;
	case '\r':
		con_col = 0;
		break;
	case '\b':
		if (con_col > 0) {
			con_col--;
			con->putc_at(con_col, con_row, ' ', con_attr);
		}
		break;
	default:
		con->putc_at(con_col, con_row, c, con_attr);
		con_col++;
		break;
	}

	if (con_row >= con->rows) {
		int lines = con_row - con->rows + 1;

		if (con->scroll)
			con->scroll(lines);
		else if (con->clear)
			con->clear();
		con_row = con->rows - 1;
	}
}

void console_print(const char *s) {
	while (*s)
		console_putc(*s++);
}

void console_print_c(const char *s, u8 attr) {
	u8 old = con_attr;

	con_attr = attr;
	console_print(s);
	con_attr = old;
}

void console_flush(void) { }
