// SPDX-License-Identifier: GPL-2.0-only
#include "serial_mouse.h"
#include "uart8250.h"
#include "../../include/printk.h"

/*
 * Microsoft two-button serial mouse on COM1 (1200 baud). The device
 * streams three-byte relative packets at its own pace; we simply drain
 * whatever arrived since the last poll.
 */

#define COM1 0x3F8

static bool present;

bool serial_mouse_probe(void) {
	if (!uart_init(COM1, 1200))
		return false;

	/* The mouse identifies itself by streaming a stray 'M' after
	   reset; tolerate its absence so hot-unplug stays quiet. */
	present = true;
	kputs("input microsoft serial mouse on com1\n");
	return true;
}

void serial_mouse_poll(int *dx, int *dy,
		       bool *left, bool *right) {
	if (!present)
		return;

	int b0;

	while ((b0 = uart_read(COM1)) >= 0) {
		if ((b0 & 0xE0) != 0x40)
			continue;	/* resync: wait for header */

		int b1 = uart_read(COM1);
		int b2 = uart_read(COM1);

		if (b1 < 0 || b2 < 0)
			break;

		int sx = ((b0 & 0x03) << 6) | (b1 & 0x3F);
		int sy = ((b0 & 0x0C) << 4) | (b2 & 0x3F);

		if (b1 & 0x40)
			sx -= 64;
		if (b2 & 0x40)
			sy -= 64;

		if (dx)
			*dx += sx;
		if (dy)
			*dy += sy;
		if (left)
			*left |= (b0 & 0x20) != 0;
		if (right)
			*right |= (b0 & 0x10) != 0;
	}
}
