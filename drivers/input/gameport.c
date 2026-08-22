// SPDX-License-Identifier: GPL-2.0-only
#include "gameport.h"
#include "../../include/io.h"
#include "../../include/types.h"

/*
 * Analog axes work as RC timers: writing any bit discharges all four
 * capacitors, then each axis line rises when its stick position says
 * so; software times the rise. Timeout guards keep a missing stick
 * from hanging the kernel.
 */

#define GAMEPORT 0x201
#define TIMEOUT  20000

static bool present;

static int measure_axis(u8 mask) {
	u32 ticks = 0;

	outb(GAMEPORT, 0xFF);

	while (ticks < TIMEOUT) {
		if (!(inb(GAMEPORT) & mask))
			break;
		ticks++;
	}
	return ticks >= TIMEOUT ? -1 : (int)ticks;
}

bool gameport_probe(void) {
	int x = measure_axis(0x01);

	present = x > 10 && x < TIMEOUT;
	return present;
}

bool gameport_read(int *x, int *y, bool *a, bool *b,
		   bool *c, bool *d) {
	if (!present)
		return false;

	u8 bits = inb(GAMEPORT);
	int vx = measure_axis(0x01);
	int vy = measure_axis(0x02);

	if (vx < 0 || vy < 0)
		return false;

	if (x) *x = vx;
	if (y) *y = vy;
	if (a) *a = !(bits & 0x10);
	if (b) *b = !(bits & 0x20);
	if (c) *c = !(bits & 0x40);
	if (d) *d = !(bits & 0x80);
	return true;
}
