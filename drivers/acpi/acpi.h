// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_ACPI_H
#define OBSIDIAN_ACPI_H

#include "../../include/types.h"

/* ACPI table header shared by every SDT. */
struct acpi_sdt_header {
	u8  signature[4];
	u32 length;
	u8  revision;
	u8  checksum;
	u8  oem_id[6];
	u8  oem_table_id[8];
	u32 oem_revision;
	u32 creator_id;
	u32 creator_revision;
} __attribute__((packed));

struct acpi_rsdp {
	u8  signature[8];
	u8  checksum;
	u8  oem_id[6];
	u8  revision;
	u32 rsdt_address;
	/* revision >= 2 */
	u32 length;
	u64 xsdt_address;
	u8  extended_checksum;
} __attribute__((packed));

/* Fields of interest inside FACP, at their documented offsets. */
struct acpi_fadt {
	bool     valid;
	u16      sci_int;
	u32      smi_cmd_port;
	u8       acpi_enable_val;
	u8       acpi_disable_val;
	u32      pm1a_cnt_port;
	u32      dsdt_addr;
	char     dsdt_oem_table[9];
};

void acpi_init(void);
bool acpi_find_rsdp(void);
struct acpi_rsdp *acpi_rsdp_get(void);
void acpi_walk_tables(const struct acpi_rsdp *rsdp);
const struct acpi_fadt *acpi_fadt(void);
bool acpi_available(void);

#endif
