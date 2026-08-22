// SPDX-License-Identifier: GPL-2.0-only
#include "gdt.h"
#include "../../include/types.h"

/*
 * In long mode the CPU largely ignores base/limit in code and data
 * descriptors - only the L (64-bit) and D/B bits matter.
 * We keep three entries: null, kernel code (L=1), kernel data.
 */

struct gdt_entry {
	u16 limit_low;
	u16 base_low;
	u8  base_mid;
	u8  access;
	u8  granularity;	/* [7:4] flags, [3:0] limit_high */
	u8  base_high;
} __attribute__((packed));

struct gdt_ptr {
	u16 limit;
	u64 base;
} __attribute__((packed));

static struct gdt_entry gdt[3];
static struct gdt_ptr   gdt_descriptor;

static void gdt_set_entry(int i, u8 access, u8 gran) {
	gdt[i].limit_low  = 0xFFFF;
	gdt[i].base_low   = 0;
	gdt[i].base_mid   = 0;
	gdt[i].access     = access;
	gdt[i].granularity = gran;
	gdt[i].base_high  = 0;
}

extern void gdt_flush(u64 gdt_ptr_addr);

void gdt_init(void) {
	gdt_descriptor.limit = sizeof(gdt) - 1;
	gdt_descriptor.base  = (u64)(uintptr_t) &gdt;

	gdt_set_entry(0, 0x00, 0x00);		/* null            */
	gdt_set_entry(1, 0x9A, 0xAF);		/* 0x08: kcode 64  */
	gdt_set_entry(2, 0x92, 0xAF);		/* 0x10: kdata 64  */

	gdt_flush((u64)(uintptr_t) &gdt_descriptor);
}
