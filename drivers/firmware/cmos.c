// SPDX-License-Identifier: GPL-2.0-only
#include "cmos.h"
#include "../../include/io.h"
#include <stdarg.h>

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

#define PANIC_MAGIC_ADDR 0x3A
#define PANIC_CODE_ADDR  0x3B
#define PANIC_MAGIC      0x4F

/* Bit 7 of the address port masks NMI; it is kept set during access so a
   stray SMI/NMI cannot interrupt the read-modify cycle mid-way. */
u8 cmos_read(u8 reg) {
	outb(CMOS_ADDR, (u8)(reg | 0x80));
	io_wait();
	return inb(CMOS_DATA);
}

void cmos_write(u8 reg, u8 val) {
	outb(CMOS_ADDR, (u8)((reg & 0x7F) | 0x80));
	io_wait();
	outb(CMOS_DATA, val);
	io_wait();
}

bool cmos_last_boot_panicked(u8 *code_out) {
	u8 magic = cmos_read(PANIC_MAGIC_ADDR);

	if (magic != PANIC_MAGIC)
		return false;
	if (code_out)
		*code_out = cmos_read(PANIC_CODE_ADDR);
	return true;
}

void cmos_clear_panic_flag(void) {
	cmos_write(PANIC_MAGIC_ADDR, 0x00);
	cmos_write(PANIC_CODE_ADDR, 0x00);
}
