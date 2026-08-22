// SPDX-License-Identifier: GPL-2.0-only
#include "acpi.h"
#include "../../include/io.h"
#include "../../include/printk.h"
#include "../../lib/string.h"

/*
 * Root System Description Pointer discovery.
 * Per the ACPI spec the RSDP sits either in the first kilobyte of the
 * EBDA or in the BIOS ROM area 0xE00000-0xFFFFF, 16-byte aligned.
 */

static struct acpi_rsdp found_rsdp;
static bool have_rsdp;

static bool checksum_ok(const u8 *p, u32 len) {
	u8 sum = 0;

	for (u32 i = 0; i < len; i++)
		sum = (u8)(sum + p[i]);
	return sum == 0;
}

static bool validate_rsdp(const struct acpi_rsdp *r) {
	if (k_strncmp((const char *)r->signature, "RSD PTR ", 8) != 0)
		return false;
	if (!checksum_ok((const u8 *)r, 20))
		return false;

	/* Version 2 adds an extension block with its own checksum. */
	if (r->revision >= 2 && !checksum_ok((const u8 *)r, r->length))
		return false;
	return true;
}

bool acpi_find_rsdp(void) {
	/* Candidate 1: EBDA segment pointer in the BDA. */
	u16 ebda_seg = *(volatile const u16 *)0x40E;
	uintptr_t start = (uintptr_t)ebda_seg << 4;
	uintptr_t end   = start + 1024;

	for (uintptr_t p = start; p + sizeof(found_rsdp) <= end; p += 16) {
		const volatile u8 *mem = (const volatile u8 *)p;

		found_rsdp = *(const struct acpi_rsdp *)(void *)mem;
		have_rsdp = validate_rsdp(&found_rsdp);
		if (have_rsdp)
			return true;
	}

	/* Candidate 2: BIOS ROM area. */
	for (uintptr_t p = 0xE0000; p + sizeof(found_rsdp) <= 0xFFFFF;
	     p += 16) {
		const volatile u8 *mem = (const volatile u8 *)p;

		found_rsdp = *(const struct acpi_rsdp *)(void *)mem;
		have_rsdp = validate_rsdp(&found_rsdp);
		if (have_rsdp)
			return true;
	}
	return false;
}

struct acpi_rsdp *acpi_rsdp_get(void) {
	return have_rsdp ? &found_rsdp : NULL;
}
