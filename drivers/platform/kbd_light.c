// SPDX-License-Identifier: GPL-2.0-only
#include "kbd_light.h"
#include "../firmware/smbios.h"
#include "../../include/printk.h"
#include "../../lib/string.h"

/*
 * Generic keyboard backlight core. Exactly one backend wins the
 * machine; everything else stays a no-op so the same image runs on
 * any vendor.
 */

static const struct kbd_light_backend *active;

bool kbd_light_register(const struct kbd_light_backend *be) {
	if (!be || !be->probe || !be->set_level)
		return false;

	if (active)
		return false;

	if (!be->probe())
		return false;

	active = be;
	kprintf("input keyboard backlight via %s (max %u)\n",
		be->vendor ? be->vendor : "vendor", be->max_level);
	return true;
}

bool kbd_light_present(void) {
	return active != NULL;
}

u8 kbd_light_max(void) {
	return active ? active->max_level : 0;
}

int kbd_light_set(u8 level) {
	if (!active || level > active->max_level)
		return -1;
	return active->set_level(level);
}

int kbd_light_get(void) {
	u8 lvl = 0;

	if (!active || !active->get_level ||
	    active->get_level(&lvl))
		return -1;
	return lvl;
}
