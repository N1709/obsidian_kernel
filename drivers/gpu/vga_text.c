// SPDX-License-Identifier: GPL-2.0-only
#include "vga_text.h"
#include "../../include/io.h"
#include "../../lib/string.h"

/*
 * Standard VGA text-mode backend: 80x25 characters at physical
 * 0xB8000, hardware cursor kept in sync on every write.
 */

#define VGA_COLS_ 80
#define VGA_ROWS_ 25

static volatile u16 *const vt = (volatile u16 *)0xB8000;

static void vt_clear(void) {
	for (int i = 0; i < VGA_COLS_ * VGA_ROWS_; i++)
		vt[i] = ((u16)CON_NORMAL << 8) | ' ';
}

static void vt_scroll(int lines) {
	for (int r = lines; r < VGA_ROWS_; r++)
		memcpy((void *)&vt[(r - lines) * VGA_COLS_],
		       (void *)&vt[r * VGA_COLS_], VGA_COLS_ * 2);
	for (int r = VGA_ROWS_ - lines; r < VGA_ROWS_; r++)
		for (int c = 0; c < VGA_COLS_; c++)
			vt[r * VGA_COLS_ + c] =
				((u16)CON_NORMAL << 8) | ' ';
}

static void vt_putc_at(int col, int row, char c, u8 attr) {
	if (col < 0 || col >= VGA_COLS_ || row < 0 || row >= VGA_ROWS_)
		return;

	vt[row * VGA_COLS_ + col] = (u16)(u8)c | ((u16)attr << 8);

	u16 pos = (u16)(row * VGA_COLS_ + col);
	outb(0x3D4, 0x0F); outb(0x3D5, (u8)(pos & 0xFF));
	outb(0x3D4, 0x0E); outb(0x3D5, (u8)((pos >> 8) & 0xFF));
}

const struct console_ops *vga_text_ops(void) {
	static const struct console_ops ops = {
		.cols    = VGA_COLS_,
		.rows    = VGA_ROWS_,
		.clear   = vt_clear,
		.putc_at = vt_putc_at,
		.scroll  = vt_scroll,
	};
	return &ops;
}
