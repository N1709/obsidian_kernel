// SPDX-License-Identifier: GPL-2.0-only
#include "smbios.h"
#include "../../include/io.h"
#include "../../include/printk.h"
#include "../../lib/string.h"

/*
 * SMBIOS/DMI reader. Scans the BIOS ROM area for the entry-point
 * anchor ("_SM_" for SMBIOS 2.x, "_SM3_" for 3.x), then walks the
 * structure table pulling out the System Information (type 1) strings
 * used for platform detection.
 */

#define SCAN_BASE  0xF0000u
#define SCAN_LIMIT 0x100000u

static char dmi_vendor[64];
static char dmi_product[96];

const char *smbios_vendor(void)  { return dmi_vendor; }
const char *smbios_product(void) { return dmi_product; }

/* Copy the NUL-terminated strings that follow a structure's fixed
   fields. String n is the nth string (1-based); empty means missing. */
static void copy_string(const u8 *p, int n, char *dst, u32 dst_size) {
	dst[0] = 0;
	for (int i = 1; *p && i <= n; i++) {
		if (i == n) {
			u32 j = 0;

			while (*p && j + 1 < dst_size)
				dst[j++] = (char)*p++;
			dst[j] = 0;
			return;
		}
		while (*p) p++;
		p++;
		i--;
		i++;
	}
}

static bool parse_type1(const u8 *base, u32 len) {
	if (len < 27 || base[0x04] != 1)
		return false;

	const u8 *strings = base + base[0x02];

	copy_string(strings, base[0x08], dmi_vendor, sizeof(dmi_vendor));
	copy_string(strings, base[0x09], dmi_product, sizeof(dmi_product));
	return true;
}

static bool parse_smbios2(const u8 *anchor) {
	u32 table_addr = *(const u32 *)(anchor + 0x18);
	u16 nstructs   = *(const u16 *)(anchor + 0x16);

	if (!table_addr || !nstructs)
		return false;

	const u8 *p = (const u8 *)(uintptr_t)table_addr;

	for (u16 i = 0; i < nstructs; i++) {
		if (p[0x01] == 5 || p[0x01] == 127)
			break;

		u8 type = p[0];
		u8 len  = p[1];
		u16 hlen = len;

		if (type == 1 && parse_type1(p, len))
			return true;

		/* Skip fixed part, then the string area (double NUL). */
		p += hlen;
		while (!(p[0] == 0 && p[1] == 0))
			p++;
		p += 2;
	}
	return false;
}

static bool parse_smbios3(const u8 *anchor) {
	u64 addr = *(const u64 *)(anchor + 0x30);
	u32 maxlen = *(const u32 *)(anchor + 0x28);

	if (!addr || addr >= 0x100000000ull || !maxlen)
		return false;

	/* Structure tables live under 4 GB; walk as for 2.x. */
	const u8 *p = (const u8 *)(uintptr_t)(u32)addr;
	u32 left = maxlen;

	while (left > 4 && p[0x01] != 127) {
		u8 type = p[0];
		u8 len  = p[1];

		if (type == 1 && parse_type1(p, len))
			return true;

		p += len;
		left -= len;
		while (p[0] || p[1])
			p++;
		p += 2;
		left -= 2;
	}
	return false;
}

bool smbios_init(void) {
	dmi_vendor[0] = 0;
	dmi_product[0] = 0;

	for (uintptr_t p = SCAN_BASE; p < SCAN_LIMIT; p += 16) {
		const volatile u8 *mem = (const volatile u8 *)p;

		if (mem[0] != '_' )
			continue;
		if (mem[1] != 'S' || mem[2] != 'M')
			continue;

		if (k_strncmp((const char *)mem, "_SM_", 4) == 0 &&
		    parse_smbios2((const u8 *)p))
			return true;
		if (k_strncmp((const char *)mem, "_SM3_", 5) == 0 &&
		    parse_smbios3((const u8 *)p))
			return true;
	}
	return false;
}
