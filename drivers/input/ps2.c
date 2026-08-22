// SPDX-License-Identifier: GPL-2.0-only
#include "ps2.h"
#include "../../include/io.h"

/*
 * 8042 PS/2 controller: port1 = keyboard, port2 = mouse/TrackPoint.
 * Bring-up follows the standard init sequence with timeouts everywhere
 * so a missing device can never hang the boot.
 */

#define DATA_PORT   0x60
#define STATUS_PORT 0x64
#define CMD_PORT    0x64

#define STAT_OUT_FULL   0x01
#define STAT_IN_FULL    0x02

#define CMD_READ_CFG    0x20
#define CMD_WRITE_CFG   0x60
#define CMD_SELF_TEST   0xAA
#define CMD_PORT1_TEST  0xAB
#define CMD_PORT2_TEST  0xA9
#define CMD_ENABLE_P1   0xAE
#define CMD_ENABLE_P2   0xA8
#define CMD_DISABLE_P1  0xAD
#define CMD_DISABLE_P2  0xA7
#define CMD_WRITE_P2    0xD4

static bool port1_ok, port2_ok;

static bool wait_in_ready(void) {
	for (int i = 0; i < 100000; i++)
		if (!(inb(STATUS_PORT) & STAT_IN_FULL))
			return true;
	return false;
}

static bool wait_out_ready(void) {
	for (int i = 0; i < 100000; i++)
		if (inb(STATUS_PORT) & STAT_OUT_FULL)
			return true;
	return false;
}

static void flush_output(void) {
	while (inb(STATUS_PORT) & STAT_OUT_FULL)
		inb(DATA_PORT);
}

static bool controller_cmd(u8 cmd) {
	if (!wait_in_ready())
		return false;
	outb(CMD_PORT, cmd);
	return true;
}

static u8 data_read(void) {
	if (!wait_out_ready())
		return 0xFF;
	return inb(DATA_PORT);
}

static bool data_write(u8 val) {
	if (!wait_in_ready())
		return false;
	outb(DATA_PORT, val);
	return true;
}

/* Send a byte to the port2 (aux) device through the controller. */
static bool aux_write(u8 val) {
	if (!wait_in_ready())
		return false;
	outb(CMD_PORT, CMD_WRITE_P2);
	if (!wait_in_ready())
		return false;
	outb(DATA_PORT, val);
	return true;
}

bool ps2_controller_init(void) {
	port1_ok = false;
	port2_ok = false;

	/* Disable both ports and drain stale bytes. */
	controller_cmd(CMD_DISABLE_P1);
	controller_cmd(CMD_DISABLE_P2);
	flush_output();

	/* Controller self test must answer 0x55. */
	controller_cmd(CMD_SELF_TEST);
	if (data_read() != 0x55)
		return false;

	/* Read config, run it through the self-test reset path again so
	   IRQ lines are re-armed, then re-read. */
	controller_cmd(CMD_READ_CFG);
	u8 cfg = data_read();
	cfg &= ~(u8)(1 << 6);	/* translation for port1 only */

	controller_cmd(CMD_WRITE_CFG);
	data_write(cfg);

	/* Test both ports; a machine without a mouse simply fails p2. */
	controller_cmd(CMD_PORT1_TEST);
	port1_ok = (data_read() == 0x00);

	controller_cmd(CMD_PORT2_TEST);
	port2_ok = (data_read() == 0x00);

	if (port1_ok) {
		controller_cmd(CMD_ENABLE_P1);
		cfg |= 0x01;		/* IRQ1 keyboard */
	}
	if (port2_ok) {
		controller_cmd(CMD_ENABLE_P2);
		cfg |= 0x02;		/* IRQ12 aux */
	}
	controller_cmd(CMD_WRITE_CFG);
	data_write(cfg);

	return port1_ok || port2_ok;
}

bool ps2_port1_present(void) { return port1_ok; }
bool ps2_port2_present(void) { return port2_ok; }

bool ps2_aux_write(u8 val) {
	if (!wait_in_ready())
		return false;
	outb(CMD_PORT, CMD_WRITE_P2);
	return data_write(val);
}

int ps2_poll_scancode(void) {
	if (!(inb(STATUS_PORT) & STAT_OUT_FULL))
		return -1;
	return inb(DATA_PORT);
}

void ps2_flush_aux(void) {
	while (ps2_aux_read() >= 0)
		;
}

int ps2_aux_read(void) {
	for (int i = 0; i < 1000; i++) {
		u8 st = inb(0x64);

		if (!(st & 0x01))
			return -1;
		if (!(st & 0x20))
			continue;	/* byte came from port 1 */

		return inb(0x60);
	}
	return -1;
}
