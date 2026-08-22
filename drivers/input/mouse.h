// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_MOUSE_H
#define OBSIDIAN_MOUSE_H

#include "../../include/types.h"

bool mouse_init(void);
u32  mouse_packets(void);
s32  mouse_dx_total(void);
s32  mouse_dy_total(void);
u8   mouse_buttons(void);

#endif
