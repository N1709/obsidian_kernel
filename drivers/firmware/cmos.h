// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_CMOS_H
#define OBSIDIAN_CMOS_H

#include "../../include/types.h"

u8  cmos_read(u8 reg);
void cmos_write(u8 reg, u8 val);
bool cmos_last_boot_panicked(u8 *code_out);
void cmos_clear_panic_flag(void);

#endif
