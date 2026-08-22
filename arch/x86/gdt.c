// SPDX-License-Identifier: GPL-2.0-only
#include "gdt.h"
#include "../../include/types.h"

struct gdt_entry {
	u16 limit_low;
	u16 base_low;
	u8  base_mid;
	u8  access;
	u8  granularity;
	u8  base_high;
} __attribute__((packed));

struct gdt_ptr {
	u16 limit;
	u32 base;
} __attribute__((packed));

static struct gdt_entry gdt[3];
static struct gdt_ptr   gdt_descriptor;

static void gdt_set_entry(int i, u32 base, u32 limit, u8 access, u8 gran) {
	gdt[i].base_low    = (u16)(base  & 0xFFFF);
	gdt[i].base_mid    = (u8)((base  >> 16) & 0xFF);
	gdt[i].base_high   = (u8)((base  >> 24) & 0xFF);
	gdt[i].limit_low   = (u16)(limit & 0xFFFF);
	gdt[i].granularity = (u8)((limit >> 16) & 0x0F) | (gran & 0xF0);
	gdt[i].access      = access;
}

extern void gdt_flush(u32 gdt_ptr_addr);

void gdt_init(void) {
	gdt_descriptor.limit = sizeof(gdt) - 1;
	gdt_descriptor.base  = (u32) &gdt;

	gdt_set_entry(0, 0, 0,       0,    0   );	/* null            */
	gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xC0);	/* 0x08: kcode     */
	gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xC0);	/* 0x10: kdata     */

	gdt_flush((u32) &gdt_descriptor);
}
