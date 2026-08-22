// SPDX-License-Identifier: GPL-2.0-only
#include "ec.h"
#include "../../../include/io.h"

/*
 * Embedded Controller protocol driver.
 *
 * Command port 0x66, data port 0x62. Every transfer polls the status
 * register with a bounded spin so an absent or wedged EC times out and
 * reports failure instead of hanging the boot.
 */

#define EC_DATA 0x62
#define EC_CMD  0x66

#define ST_OBF 0x01		/* output buffer full: host may read */
#define ST_IBF 0x02		/* input buffer full: still busy */

#define CMD_READ  0x80
#define CMD_WRITE 0x81

static bool wait_ibf_clear(void) {
	for (int i = 0; i < 200000; i++)
		if (!(inb(EC_CMD) & ST_IBF))
			return true;
	return false;
}

static bool wait_obf_set(void) {
	for (int i = 0; i < 200000; i++)
		if (inb(EC_CMD) & ST_OBF)
			return true;
	return false;
}

bool ec_read(u8 reg, u8 *val) {
	if (!wait_ibf_clear())
		return false;

	outb(EC_CMD, CMD_READ);
	io_wait();

	if (!wait_ibf_clear())
		return false;

	outb(EC_DATA, reg);
	io_wait();

	if (!wait_obf_set())
		return false;

	*val = inb(EC_DATA);
	return true;
}

bool ec_write(u8 reg, u8 val) {
	if (!wait_ibf_clear())
		return false;

	outb(EC_CMD, CMD_WRITE);
	io_wait();

	if (!wait_ibf_clear())
		return false;

	outb(EC_DATA, reg);
	io_wait();

	if (!wait_ibf_clear())
		return false;

	outb(EC_DATA, val);
	io_wait();

	/* The EC acknowledges by raising OBF; drain it. */
	wait_obf_set();
	inb(EC_DATA);
	return true;
}

bool ec_probe(void) {
	u8 v;

	/* Register 0x00 always exists on any spec-compliant EC. */
	return ec_read(0x00, &v);
}
