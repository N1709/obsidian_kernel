// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_THINKPAD_H
#define OBSIDIAN_THINKPAD_H

#include "../../../../include/types.h"

/*
 * Lenovo ThinkPad platform support.
 *
 * Talks to the embedded controller (drivers/platform/ec.c) to expose
 * model information, fan control and battery telemetry. All of it is
 * best effort: on non-ThinkPad hardware every call degrades to a safe
 * no-op so the same kernel image stays universal.
 */

struct tp_battery {
	u8  percent;		/* 0-100 */
	u16 voltage_mv;
	u32 power_mw;
	bool charging;
	bool discharging;
	bool present;
};

void thinkpad_init(bool allow_fan_control);

const char *tp_model_string(void);
u32         tp_fan_rpm(void);
bool        tp_fan_set_level(u8 level);
bool        tp_battery_status(struct tp_battery *out, int index);

#endif
