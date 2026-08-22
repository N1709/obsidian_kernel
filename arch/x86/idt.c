// SPDX-License-Identifier: GPL-2.0-only
#include "idt.h"

struct idt_entry {
	u16 base_low;
	u16 selector;
	u8  zero;
	u8  flags;
	u16 base_high;
} __attribute__((packed));

struct idt_ptr {
	u16 limit;
	u32 base;
} __attribute__((packed));

#define IDT_ENTRIES 256

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idt_descriptor;

extern void idt_flush(u32 idt_ptr_addr);
extern u32  isr_stub_table[48];

static void idt_set_gate(int n, u32 handler) {
	idt[n].base_low  = (u16)(handler & 0xFFFF);
	idt[n].base_high = (u16)((handler >> 16) & 0xFFFF);
	idt[n].selector  = 0x08;	/* kernel code segment */
	idt[n].zero      = 0;
	idt[n].flags     = 0x8E;	/* present, ring0, 32-bit int gate */
}

void idt_init(void) {
	idt_descriptor.limit = sizeof(idt) - 1;
	idt_descriptor.base  = (u32) &idt;

	for (int i = 0;  i < 48;          i++) idt_set_gate(i, isr_stub_table[i]);
	for (int i = 48; i < IDT_ENTRIES; i++) idt_set_gate(i, 0);

	idt_flush((u32) &idt_descriptor);
}
