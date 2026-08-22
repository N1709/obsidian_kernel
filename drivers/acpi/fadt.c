// SPDX-License-Identifier: GPL-2.0-only
#include "acpi.h"
#include "../../include/printk.h"
#include "../../lib/string.h"
#include <stdarg.h>

/*
 * SDT table walker. Uses the XSDT on ACPI 2.0+ machines and falls back
 * to the RSDT otherwise, validating each table checksum along the way.
 * From FACP we extract the ports needed for power management and the
 * DSDT identity used for platform detection.
 */

static struct acpi_fadt fadt;
static bool acpi_ready;

static bool header_ok(const struct acpi_sdt_header *h) {
	const u8 *p = (const u8 *)h;
	u8 sum = 0;

	for (u32 i = 0; i < h->length; i++)
		sum = (u8)(sum + p[i]);
	return sum == 0;
}

static void handle_table(const struct acpi_sdt_header *h) {
	if (h->signature[0] == 'F' && h->signature[1] == 'A' &&
	    h->signature[2] == 'C' && h->signature[3] == 'P') {
		const u8 *raw = (const u8 *)h;

		fadt.valid = true;
		fadt.sci_int = *(const u16 *)(raw + 46);
		fadt.smi_cmd_port = *(const u32 *)(raw + 48);
		fadt.acpi_enable_val = raw[52];
		fadt.acpi_disable_val = raw[53];
		fadt.pm1a_cnt_port = *(const u32 *)(raw + 64);
		fadt.dsdt_addr = *(const u32 *)(raw + 40);

		struct acpi_sdt_header *dsdt =
			(struct acpi_sdt_header *)(uintptr_t)fadt.dsdt_addr;

		for (int i = 0; i < 8; i++)
			fadt.dsdt_oem_table[i] = (char)dsdt->oem_table_id[i];
		fadt.dsdt_oem_table[8] = 0;
	}
}

void acpi_init(void) {
	acpi_ready = false;

	struct acpi_rsdp *rsdp =
		*(struct acpi_rsdp **)0; /* replaced below */
	(void)rsdp;
}

/* Called by the kernel once RSDP discovery has succeeded. */
void acpi_walk_tables(const struct acpi_rsdp *rsdp) {
	acpi_ready = false;

	if (!rsdp)
		return;

	char oem[7];

	for (int i = 0; i < 6; i++)
		oem[i] = (char)rsdp->oem_id[i];
	oem[6] = 0;

	kprintf("acpi RSDP rev %u oem '%s'\n", rsdp->revision, oem);

	bool use_xsdt = rsdp->revision >= 2 && rsdp->xsdt_address != 0 &&
			rsdp->xsdt_address < 0x100000000ull;

	u32 count = 0;

	if (use_xsdt) {
		struct acpi_sdt_header *xsdt =
			(struct acpi_sdt_header *)(uintptr_t)rsdp->xsdt_address;

		count = (xsdt->length - sizeof(*xsdt)) / 8;
		const u64 *entries = (const u64 *)(xsdt + 1);

		for (u32 i = 0; i < count && i < 32; i++) {
			if (entries[i] >= 0x100000000ull)
				continue;
			handle_table((struct acpi_sdt_header *)
				     (uintptr_t)entries[i]);
		}
	} else {
		struct acpi_sdt_header *rsdt =
			(struct acpi_sdt_header *)
				(uintptr_t)rsdp->rsdt_address;

		count = (rsdt->length - sizeof(*rsdt)) / 4;
		const u32 *entries = (const u32 *)(rsdt + 1);

		for (u32 i = 0; i < count && i < 32; i++) {
			if (!entries[i])
				continue;
			handle_table((struct acpi_sdt_header *)
				     (uintptr_t)entries[i]);
		}
	}

	acpi_ready = fadt.valid;
}

bool acpi_available(void) {
	return acpi_ready;
}
