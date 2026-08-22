// SPDX-License-Identifier: GPL-2.0-only
#include "mouse.h"
#include "../../include/irq.h"
#include "ps2.h"
#include "../../include/io.h"
#include "../../include/printk.h"

/*
 * PS/2 aux port driver. On a ThinkPad the TrackPoint sits on this port
 * and speaks exactly this protocol, so packets decode identically.
 * Movement accumulates into totals that callers can poll; the first
 * received packet proves the pointing device is alive.
 */

#define DATA_PORT 0x60

static volatile u32 pkt_count;
static volatile s32 acc_x, acc_y;
static volatile u8  buttons;
static volatile u8  state, bytes[3];
static bool seen_first;

void mouse_irq_handler(void) {
	u8 b = inb(DATA_PORT);

	bytes[state] = b;

	/* Bit 3 of byte0 is always set in valid packets; use it to
	   resynchronize after lost bytes. */
	if (state == 0 && !(b & 0x08)) {
		state = 0;
		return;
	}

	state = (u8)((state + 1) % 3);

	if (state != 0)
		return;

	buttons = (u8)(bytes[0] & 0x07);
	acc_x += (s32)(s8)bytes[1];
	acc_y += (s32)(s8)bytes[2];
	pkt_count++;

	if (!seen_first) {
		seen_first = true;
		kputs("input TrackPoint/mouse detected on aux port");
	}
}

u32 mouse_packets(void) { return pkt_count; }
s32 mouse_dx_total(void) { return acc_x; }
s32 mouse_dy_total(void) { return acc_y; }
u8  mouse_buttons(void)  { return buttons; }

bool mouse_init(void) {
	if (!ps2_port2_present())
		return false;

	/* Defaults: sampling rate 100 Hz, 4 counts/mm scaling. */
	ps2_aux_write(0xF3); ps2_aux_write(100);
	ps2_aux_write(0xE6);
	ps2_aux_write(0xF4);	/* enable data reporting */

	irq_install(12, mouse_irq_handler);
	pic_unmask_irq(12);
	return true;
}
