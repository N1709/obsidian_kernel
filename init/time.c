// SPDX-License-Identifier: GPL-2.0-only
#include "time.h"
#include "../include/io.h"

/*
 * System timekeeping.
 *
 * The architecture timer driver calls time_tick() once per hardware
 * tick; everything else derives from that. sleep_ms() busy-waits, so
 * it is only honest before the scheduler exists.
 */

static volatile u64 g_ticks;
static u32 g_tick_hz = 100;

void time_init(u32 hz) {
	if (hz)
		g_tick_hz = hz;
	g_ticks = 0;
}

void time_tick(void) {
	g_ticks++;
}

u64 uptime_ms(void) {
	return (g_ticks * 1000ULL) / g_tick_hz;
}

void sleep_ms(u64 ms) {
	u64 deadline = g_ticks + (ms * (u64)g_tick_hz + 999) / 1000;

	while (g_ticks < deadline)
		cpu_halt();
}
