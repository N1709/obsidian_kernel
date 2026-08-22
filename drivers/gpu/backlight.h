// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_BACKLIGHT_H
#define OBSIDIAN_BACKLIGHT_H

#include "../../include/types.h"

/*
 * Display backlight control.
 *
 * Primary backend drives the Intel display PWM (BLC_PWM_CTL) through
 * the graphics BAR, which covers every Intel-generation ThinkPad and
 * most consumer laptops. QEMU std-vga reports no backlight and every
 * call degrades gracefully.
 */

bool backlight_probe(void);
void backlight_set(u8 percent);		/* 0..100 */
u8   backlight_get(void);
bool backlight_present(void);

#endif
