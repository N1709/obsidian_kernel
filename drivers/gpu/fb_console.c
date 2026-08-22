// SPDX-License-Identifier: GPL-2.0-only
#include "fb_console.h"
#include "console.h"
#include "../../include/io.h"
#include "../../lib/string.h"

/*
 * Framebuffer text console. Renders glyphs from the classic 8x8 bitmap
 * font stored in the VGA BIOS ROM at F000:FA6E, present on every PC
 * BIOS including SeaBIOS. Glyphs are drawn 2x for readability. The ROM
 * copy is validated before use; on failure the caller keeps the VGA
 * text backend.
 */

#define FONT_ROM_ADDR 0xFFA6Eu	/* physical address of F000:FA6E */
#define CELL          16	/* glyph cell after 2x scaling */
#define FB_BG         0xFF101418u

static u8 font[128][8];
static volatile u32 *fb;
static u32 fb_w, fb_h, fb_pitch_px;
static int cols_, rows_;

static u32 attr_fg_rgb(u8 attr) {
	static const u32 pal[16] = {
		0x00000000, 0x0000AAAAu, 0x00AA0000u, 0x00AAAA55u,
		0x00550000u, 0x005500AAu, 0x0055AA00u, 0x00AAAAAAu,
		0x00555555u, 0x005555FFu, 0x0055FF55u, 0x0055FFFFu,
		0x00FF5555u, 0x00FF55FFu, 0x00FFFF55u, 0x00FFFFFFu,
	};
	return pal[attr & 0x0F];
}

static bool copy_bios_font(void) {
	const volatile u8 *rom = (const volatile u8 *)(uintptr_t)FONT_ROM_ADDR;
	u8 first;

	first = rom[0];
	for (int i = 1; i < 128 * 8; i++)
		if (rom[i] != first)
			goto looks_real;
	return false;		/* whole page identical => not a font */

looks_real:
	for (int g = 0; g < 128; g++)
		for (int b = 0; b < 8; b++)
			font[g][b] = rom[g * 8 + b];

	/* 'A' must have ink, space must not. */
	if (!font['A'][1] && !font['A'][3])
		return false;
	if (font[' '][3])
		return false;
	return true;
}

static void draw_cell(int col, int row, char c, u8 attr) {
	u32 fg = attr_fg_rgb(attr);
	u8 idx = (u8)c & 0x7F;
	u32 px = (u32)(col * CELL);
	u32 py = (u32)(row * CELL);

	for (int gy = 0; gy < 8; gy++) {
		u8 bits = font[idx][gy];
		volatile u32 *line_a = &fb[(py + gy * 2) * fb_pitch_px + px];
		volatile u32 *line_b = line_a + fb_pitch_px;

		for (int gx = 0; gx < 8; gx++) {
			u32 color = (bits & (0x80 >> gx)) ? fg : FB_BG;
			line_a[gx * 2]     = color;
			line_a[gx * 2 + 1] = color;
			line_b[gx * 2]     = color;
			line_b[gx * 2 + 1] = color;
		}
	}
}

static void fbc_clear(void) {
	for (u32 y = 0; y < fb_h; y++) {
		volatile u32 *line = &fb[y * fb_pitch_px];
		for (u32 x = 0; x < fb_w; x++)
			line[x] = FB_BG;
	}
}

static void fbc_scroll(int lines) {
	u32 row_bytes = (u32)CELL * fb_pitch_px * sizeof(u32);

	for (int r = lines; r < rows_; r++)
		memcpy((void *)&fb[(u32)(r - lines) * CELL * fb_pitch_px],
		       (void *)&fb[(u32)r * CELL * fb_pitch_px],
		       row_bytes);

	for (int r = rows_ - lines; r < rows_; r++)
		for (int c = 0; c < cols_; c++)
			draw_cell(c, r, ' ', CON_NORMAL);
}

static void fbc_putc_at(int col, int row, char c, u8 attr) {
	draw_cell(col, row, c, attr);
}

static struct console_ops fbc_ops = {
	.cols    = 0,
	.rows    = 0,
	.clear   = fbc_clear,
	.putc_at = fbc_putc_at,
	.scroll  = fbc_scroll,
};

void fb_console_install(struct gpu_device *gpu) {
	if (!gpu->fb_active || !copy_bios_font())
		return;

	fb = (volatile u32 *)(uintptr_t)gpu->lfb_phys;
	fb_w = gpu->width;
	fb_h = gpu->height;
	fb_pitch_px = gpu->pitch / 4;
	cols_ = (int)(fb_w / CELL);
	rows_ = (int)(fb_h / CELL);

	fbc_ops.cols = cols_;
	fbc_ops.rows = rows_;
	console_install(&fbc_ops);
}
