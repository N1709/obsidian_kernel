// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_KBD_LIGHT_H
#define OBSIDIAN_KBD_LIGHT_H

#include "../../include/types.h"

/*
 * Keyboard backlight subsystem.
 *
 * Vendor machines hide this behind different interfaces, so the core
 * keeps a tiny backend registry: a platform module registers handlers
 * at boot and the generic API fans out to whoever claimed the machine.
 * Machines without any known interface simply report absent and every
 * call degrades gracefully.
 */

struct kbd_light_backend {
	const char *vendor;	/* SMBIOS vendor string prefix */
	bool (*probe)(void);
	int  (*set_level)(u8 level);	/* 0..max, returns 0 ok */
	int  (*get_level)(u8 *out);
	u8   max_level;
};

bool kbd_light_register(const struct kbd_light_backend *be);

bool kbd_light_present(void);
u8   kbd_light_max(void);
int  kbd_light_set(u8 level);
int  kbd_light_get(void);

#endif
