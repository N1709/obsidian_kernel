// SPDX-License-Identifier: GPL-2.0-only
#include "thinkpad.h"
#include "../ec.h"
#include "../../../firmware/smbios.h"
#include "../../../../include/printk.h"
#include "../../../../include/io.h"
#include "../../../../lib/string.h"

/*
 * ThinkPad platform layer.
 *
 * ThinkPads expose fan level, temperature sensors and battery state
 * through Embedded Controller registers. Values are range-checked
 * before being reported so an unknown EC map can never produce garbage.
 * Writes are opt-in only via the thinkpad_fanctl=1 command line.
 */

static bool is_thinkpad;
static bool ec_ok;
static bool fanctl_enabled;

u32 tp_fan_rpm(void) {
	u8 lo, hi;

	if (!is_thinkpad || !ec_ok)
		return 0;
	if (!ec_read(0x84, &lo) || !ec_read(0x85, &hi))
		return 0;

	/* Formula and calibration constant taken from Linux
	   drivers/platform/x86/lenovo/thinkpad_acpi.c (FAN_RPM_CAL_CONST)
	   so Obsidian reads identical RPM values on the same ECs. */
	u32 rpm = lo ? 491520u / lo : 0;

	/* Plausible tachometer range only. */
	return (rpm > 500 && rpm < 12000) ? rpm : 0;
}

s32 tp_cpu_temp_c(void) {
	u8 raw;

	if (!is_thinkpad || !ec_ok)
		return -1;
	if (!ec_read(0x78, &raw))
		return -1;

	/* Sensors idle near room temperature and trip below ~110 C. */
	if (raw < 10 || raw > 110)
		return -1;
	return (s32)raw;
}

bool tp_battery_status(struct tp_battery *out, int index) {
	u8 v_lo, v_hi, p_lo, p_hi, status;

	if (!is_thinkpad || !ec_ok || !out)
		return false;

	/* Battery0 lives at EC 0xC0.., battery1 at 0xD0... */
	u8 base = index == 1 ? 0xD0 : 0xC0;

	if (!ec_read((u8)(base + 0x02), &status))
		return false;
	if (!ec_read((u8)(base + 0x08), &v_hi) ||
	    !ec_read((u8)(base + 0x09), &v_lo))
		return false;
	if (!ec_read((u8)(base + 0x0A), &p_hi) ||
	    !ec_read((u8)(base + 0x0B), &p_lo))
		return false;

	u32 mv = ((u32)v_hi << 8 | v_lo);
	u32 mw = (((u32)p_hi << 8 | p_lo)) / 100;

	/* Sanity gates keep unknown EC maps from printing nonsense:
	   Li-ion packs sit between 5 V and 20 V. */
	if (mv < 5000 || mv > 20000)
		return false;

	out->present = (status != 0x00);
	out->voltage_mv = mv;
	out->power_mw = (mw < 150000) ? mw : 0;
	out->discharging = (status & 0x01) != 0;
	out->charging = (status & 0x02) != 0;
	return true;
}

bool tp_fan_set_level(u8 level) {
	if (!is_thinkpad || !ec_ok)
		return false;
	if (!fanctl_enabled || level > 7)
		return false;

	return ec_write(0x2F, (u8)(level | 0x40));
}

const char *tp_model_string(void) {
	return smbios_product();
}

void thinkpad_init(bool allow_fan_control) {
	is_thinkpad = false;
	ec_ok = false;
	fanctl_enabled = allow_fan_control;

	const char *vendor = smbios_vendor();
	const char *product = smbios_product();

	if (!k_strstr(vendor, "LENOVO") &&
	    !k_strstr(vendor, "IBM") &&
	    !k_strstr(product, "ThinkPad"))
		return;

	is_thinkpad = true;
	ec_ok = ec_probe();

	kprintf("platform ThinkPad detected (%s), ec %s\n",
		product, ec_ok ? "online" : "absent");

	if (ec_ok) {
		s32 t = tp_cpu_temp_c();

		if (t >= 0)
			kprintf("platform cpu temp %d C\n", t);
	}
}
