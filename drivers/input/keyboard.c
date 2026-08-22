// SPDX-License-Identifier: GPL-2.0-only
#include "keyboard.h"
#include "ps2.h"
#include "../../include/irq.h"
#include "../../include/io.h"
#include "../gpu/console.h"

/*
 * PS/2 keyboard driver on IRQ1. With controller translation left
 * enabled the device reports scancode set 1; extended keys arrive
 * prefixed with 0xE0. Decoded characters go into a small ring buffer
 * that readers drain through keyboard_getchar().
 */

#define DATA_PORT 0x60

static volatile u32 put_idx, get_idx;
static volatile char keybuf[64];

/* US layout, unshifted / shifted columns. */
static const char keymap_normal[128] = {
	[0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
	[0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
	[0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
	[0x0E] = '\b', [0x0F] = '\t',
	[0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
	[0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
	[0x18] = 'o', [0x19] = 'p', [0x1A] = '[', [0x1B] = ']',
	[0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
	[0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
	[0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = '`',
	[0x2B] = '\\', [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c',
	[0x2F] = 'v', [0x30] = 'b', [0x31] = 'n', [0x32] = 'm',
	[0x33] = ',', [0x34] = '.', [0x35] = '/', [0x39] = ' ',
};

static const char keymap_shift[128] = {
	[0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$',
	[0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*',
	[0x0A] = '(', [0x0B] = ')', [0x0C] = '_', [0x0D] = '+',
	[0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R',
	[0x14] = 'T', [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I',
	[0x18] = 'O', [0x19] = 'P', [0x1A] = '{', [0x1B] = '}',
	[0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F',
	[0x22] = 'G', [0x23] = 'H', [0x24] = 'J', [0x25] = 'K',
	[0x26] = 'L', [0x27] = ':', [0x28] = '"', [0x29] = '~',
	[0x2B] = '|', [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C',
	[0x2F] = 'V', [0x30] = 'B', [0x31] = 'N', [0x32] = 'M',
	[0x33] = '<', [0x34] = '>', [0x35] = '?',
};

static bool shift_down;

static void push_char(char c) {
	u32 next = (put_idx + 1) % sizeof(keybuf);

	if (next != get_idx)
		return;			/* buffer full, drop */
	keybuf[put_idx] = c;
	put_idx = next;
}

static void handle_scancode(u8 sc) {
	if (sc == 0xE0) {
		/* Extended prefix: arrow keys land here. */
		while (!(inb(0x64) & 0x01)) { }
		sc = inb(DATA_PORT);
		switch (sc) {
		case 0x48: push_char('A' - 0x40); return; /* up    */
		case 0x50: push_char('B' - 0x40); return; /* down  */
		case 0x4B: push_char('C' - 0x40); return; /* left  */
		case 0x4D: push_char('D' - 0x40); return; /* right */
		default: return;
		}
	}

	if (sc & 0x80) {			/* release */
		sc &= 0x7F;
		if (sc == 0x2A || sc == 0x36)
			shift_down = false;
		return;
	}

	if (sc == 0x2A || sc == 0x36) {
		shift_down = true;
		return;
	}

	char c = shift_down ? keymap_shift[sc] : keymap_normal[sc];
	if (c)
		push_char(c);
}

int keyboard_try_getchar(void) {
	if (get_idx == put_idx)
		return -1;

	int c = keybuf[get_idx];
	get_idx = (get_idx + 1) % sizeof(keybuf);
	return c;
}

int keyboard_getchar(void) {
	int c;

	while ((c = keyboard_try_getchar()) < 0)
		cpu_halt();
	return c;
}

/* Called from the IRQ1 dispatch in the interrupt layer. */
void keyboard_irq_handler(void) {
	handle_scancode(inb(DATA_PORT));
}

bool keyboard_init(void) {
	if (!ps2_port1_present())
		return false;

	put_idx = 0;
	get_idx = 0;
	irq_install(1, keyboard_irq_handler);
	pic_unmask_irq(1);
	return true;
}
