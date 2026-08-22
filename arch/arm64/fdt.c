// SPDX-License-Identifier: GPL-2.0-only
#include "fdt.h"
#include "../../include/types.h"
#include "../../lib/string.h"

/*
 * Minimal flattened device tree walker. All fields are big endian;
 * we only need enough structure to pull memory, the chosen command
 * line, the PL011 console and a CPU count out of QEMU's blob.
 */

#define FDT_MAGIC      0xd00dfeedu
#define FDT_BEGIN_NODE 0x1u
#define FDT_END_NODE   0x2u
#define FDT_PROP       0x3u
#define FDT_NOP        0x4u
#define FDT_END        0x9u

struct fdt_header {
	u32 magic;
	u32 totalsize;
	u32 off_dt_struct;
	u32 off_dt_strings;
	u32 off_mem_rsvmap;
	u32 version;
	u32 last_comp_version;
	u32 boot_cpuid_phys;
	u32 size_dt_strings;
	u32 size_dt_struct;
};

static const struct fdt_header *hdr;
static const char *strings_base;

static u32 be32(const void *p) {
	const u8 *b = p;

	return ((u32)b[0] << 24) | ((u32)b[1] << 16) |
	       ((u32)b[2] << 8) | (u32)b[3];
}

static u64 be64(const void *p) {
	return ((u64)be32(p) << 32) | be32((const u8 *)p + 4);
}

static const char *prop_name(u32 offset) {
	return strings_base + offset;
}

static int cpu_cells, addr_cells, size_cells;

/* Results harvested during the single walk. */
static const char *bootargs;
static u64 mem_base, mem_size;
static u64 uart_base_val;
static int cpus_found;

bool fdt_init(u64 addr) {
	hdr = (const struct fdt_header *)(uintptr_t)addr;

	if (!hdr || be32(&hdr->magic) != FDT_MAGIC)
		return false;

	strings_base = (const char *)hdr +
		       be32(&hdr->off_dt_strings);

	cpu_cells = 1;
	addr_cells = 2;
	size_cells = 2;
	bootargs = NULL;
	mem_base = 0;
	mem_size = 0;
	uart_base_val = 0;
	cpus_found = 0;

	const u8 *p = (const u8 *)hdr + be32(&hdr->off_dt_struct);
	int depth = 0;
	char path[8][48];

	for (;;) {
		u32 token = be32(p);
		p += 4;

		if (token == FDT_END)
			break;

		if (token == FDT_NOP)
			continue;

		if (token == FDT_BEGIN_NODE) {
			const char *name = (const char *)p;
			size_t len = k_strlen(name) + 1;

			if (depth < 8) {
				k_strncpy(path[depth], name,
					  sizeof(path[depth]) - 1);
				path[depth][sizeof(path[depth]) - 1] = 0;
			}
			p += (len + 3) & ~3u;
			depth++;
			continue;
		}

		if (token == FDT_END_NODE) {
			depth--;
			continue;
		}

		if (token != FDT_PROP)
			continue;	/* unknown token: bail out */

		u32 plen = be32(p);
		u32 noff = be32(p + 4);
		const char *pname = prop_name(noff);
		const u8 *val = p + 8;

		p += 8 + ((plen + 3) & ~3u);

		const char *parent = depth >= 2 ?
			path[depth - 2] : "";

		/* /chosen bootargs */
		if (depth >= 2 && !k_strcmp(parent, "chosen") &&
		    !k_strcmp(pname, "bootargs"))
			bootargs = (const char *)val;

		/* #address-cells style overrides */
		if (!k_strcmp(pname, "#address-cells") && plen == 4)
			addr_cells = (int)be32(val);
		if (!k_strcmp(pname, "#size-cells") && plen == 4)
			size_cells = (int)be32(val);

		/* memory node: device_type = "memory", reg = base/size */
		if (depth >= 2 &&
		    (k_strncmp(parent, "memory@", 8) == 0 ||
		     !k_strcmp(parent, "memory"))) {
			if (!k_strcmp(pname, "device_type") &&
			    val[0] == 'm')
				;
			if (!k_strcmp(pname, "reg") &&
			    plen >= (u32)((addr_cells + size_cells) * 4)) {
				mem_base = be64(val);
				mem_size = be64(val + addr_cells * 4);
			}
			continue;
		}

		/* serial node: remember first reg of any uart */
		if (depth >= 2 &&
		    (k_strncmp(parent, "uart@", 5) == 0 ||
		     k_strncmp(parent, "serial@", 7) == 0)) {
			if (!k_strcmp(pname, "reg") && !uart_base_val)
				uart_base_val = be64(val);
			continue;
		}

		/* count cpus */
		if (depth >= 2 && !k_strcmp(parent, "cpus") &&
		    !k_strcmp(pname, "device_type") &&
		    !k_strncmp((const char *)val, "cpu", 3))
			cpus_found++;
	}

	return true;
}

const char *fdt_chosen_bootargs(void) {
	return bootargs ? bootargs : "";
}

u64 fdt_memory_base(u64 *size_out) {
	if (size_out)
		*size_out = mem_size;
	return mem_base;
}

u64 fdt_uart_base(void) {
	return uart_base_val ? uart_base_val : 0x09000000ull;
}

int fdt_cpu_count(void) {
	return cpus_found > 0 ? cpus_found : 1;
}
