// SPDX-License-Identifier: GPL-2.0-only
#include "touchpad_synaptics.h"
#include "ps2.h"
#include "../../include/printk.h"

/*
 * Synaptics absolute-mode driver, polled variant.
 *
 * The pad shares the second PS/2 port with plain mice; detection
 * sends the documented unlock sequence then reads identity bytes.
 * Packets are six bytes: two header bytes with button bits and four
 * data bytes holding a 12-bit X and Y plus pressure.
 */

#define AUX_CMD_ACK 0xFE

static bool present;
static u16 max_x = 6143, max_y = 2047;

static int aux_write_ack(u8 cmd) {
	if (!ps2_aux_write(cmd))
		return -1;

	for (int i = 0; i < 1000; i++) {
		int r = ps2_aux_read();

		if (r == 0xFA)	/* ACK */
			return 0;
		if (r >= 0)
			return -1;
	}
	return -1;
}

static int aux_read_byte(u8 *out) {
	for (int i = 0; i < 1000; i++) {
		int r = ps2_aux_read();

		if (r >= 0) {
			*out = (u8)r;
			return 0;
		}
	}
	return -1;
}

bool synaptics_probe(void) {
	u8 id_hi = 0, id_lo = 0;

	/* Magic knock per Synaptics appendix B. */
	if (aux_write_ack(0xF3) || aux_write_ack(0x64) ||
	    aux_write_ack(0xEA) ||
	    aux_write_ack(0xF3) || aux_write_ack(0x28) ||
	    aux_write_ack(0xEA) ||
	    aux_write_ack(0xF3) || aux_write_ack(0x14))
		return false;

	if (aux_write_ack(0xF2))	/* identify */
		return false;

	if (aux_read_byte(&id_lo) || aux_read_byte(&id_hi))
		return false;

	u16 model = ((u16)(id_hi & 0x0F) << 8) | id_lo;

	if (id_hi == 0x47 || (model >> 8) == 0x4B) {
		present = true;
		kprintf("input synaptics touchpad detected "
			"(model %04x)\n", model);
	}

	return present;
}

static int read_packet_byte(u8 *out) {
	for (int i = 0; i < 200; i++) {
		int r = ps2_aux_read();

		if (r >= 0) {
			*out = (u8)r;
			return 0;
		}
	}
	return -1;
}

void synaptics_poll(int *x_out, int *y_out,
		    bool *left, bool *right) {
	if (!present)
		return;

	u8 b[6];

	if (read_packet_byte(&b[0]))
		return;

	if (!(b[0] & 0x40)) {	/* not an absolute packet */
		ps2_flush_aux();
		return;
	}

	for (int i = 1; i < 6; i++)
		if (read_packet_byte(&b[i]))
			return;

	bool w = (b[0] & 0x04) != 0;	/* extended: 4th byte valid */

	int x = ((b[3] & 0x10) << 8) | (b[1] << 4) | (b[3] & 0x0F);
	int y = ((b[3] & 0x20) << 7) | (b[2] << 4) | (b[4] & 0x0F);
	bool l = (b[0] & 0x01) != 0;
	bool r = (b[0] & 0x02) != 0;

	if (!w && x_out)
		*x_out = x;
	if (!w && y_out)
		*y_out = y;
	if (left)
		*left = l;
	if (right)
		*right = r;
}
