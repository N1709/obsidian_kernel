// SPDX-License-Identifier: GPL-2.0-only
#include "../../include/obsidian.h"
#include "../../include/printk.h"
#include "../../include/io.h"
#include "multiboot2.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "pmm.h"
#include "cpu/cpuid.h"
#include "../init/time.h"
#include "../../lib/string.h"

/*
 * C entry point for x86_64. boot.s lands here once long mode and the
 * 4 GiB identity map are live; everything below brings up core
 * services in dependency order, fills boot_info_t, then hands control
 * to the portable kernel_main().
 */

#define HEAP_BYTES (4u * 1024u * 1024u)

boot_info_t boot_state;

void cpu_report(void) {
	char vendor[13];
	char brand[49];
	u32 family, model, stepping;

	get_cpu_vendor(vendor);
	get_cpu_brand(brand);
	get_cpu_fms(&family, &model, &stepping);

	kprintf("cpu %s family %u model %u stepping %u\n",
		vendor, family, model, stepping);

	if (brand[0])
		kprintf("cpu %s\n", brand);
}

void kernel_reboot(void) {
	/* Pulse the reset line through the 8042 keyboard controller;
	   the classic method that works on nearly every x86 board. */
	u8 status;

	for (;;) {
		status = inb(0x64);
		if (!(status & 0x02))
			break;
	}
	outb(0x64, 0xFE);
	for (;;)
		cpu_halt();
}

void kernel_halt(void) {
	irq_disable();
	for (;;)
		cpu_halt();
}

static const char *extract_cmdline(u64 mbi_addr) {
	mb2_tag_t *tag = (mb2_tag_t *)(mbi_addr + 8);

	while (tag->type != MB2_TAG_END) {
		if (tag->type == MB2_TAG_CMDLINE)
			return ((mb2_cmdline_tag_t *)tag)->string;
		tag = (mb2_tag_t *)
			((((uintptr_t)(u8 *)tag + tag->size) + 7) &
			 ~(uintptr_t)7);
	}
	return "";
}

void arch_start(u32 magic, u64 mbi_addr) {
	if (magic != MB2_BOOTLOADER_MAGIC) {
		kputs("fatal: bad multiboot2 magic, refusing to boot\n");
		return;
	}

	gdt_init();
	idt_init();
	pic_init();
	pit_init(100);
	time_init(100);
	irq_enable();

	boot_state.mem_usable_kb = pmm_init((mb2_info_t *)mbi_addr);
	boot_state.cmdline = extract_cmdline(mbi_addr);
	boot_state.arch_name = "x86_64";

	u32 base = ((u32)(uintptr_t)_kernel_end + 4095u) & ~4095u;

	pmm_reserve(base, base + HEAP_BYTES);
	boot_state.heap_base = base;
	boot_state.heap_size = HEAP_BYTES;

	kernel_main(&boot_state);
	kernel_halt();
}
