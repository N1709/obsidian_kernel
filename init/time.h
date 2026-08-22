// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_TIME_H
#define OBSIDIAN_TIME_H

#include "../include/types.h"

void time_init(u32 hz);
void time_tick(void);
u64  uptime_ms(void);
void sleep_ms(u64 ms);

#endif
