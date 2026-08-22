// SPDX-License-Identifier: GPL-2.0-only
#include "../../include/obsidian.h"
#include "../../include/printk.h"
#include "../../include/io.h"
#include "multiboot.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "pmm.h"
#include "cpu/cpuid.h"
#include "../init/time.h"
#include "../lib/string.h"

/*
 * C entry point for i686. boot.s lands here in protected mode with a
 * flat 4 GiB mapping installed by the bootloader; this file brings up
 * core services and hands control to portable kernel_main().
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

static const char *extract_cmdline(const multiboot_info_t *mbi) {
	if (!(mbi->flags & (1u << 2)) || !mbi->cmdline)
		return "";
	return (const char *)(uintptr_t)mbi->cmdline;
}

void arch_start(u32 magic, const multiboot_info_t *mbi) {
	if (magic != 0x2BADB002u) {
		kputs("fatal: bad multiboot magic, refusing to boot\n");
		return;
	}

	gdt_init();
	idt_init();
	pic_init();
	pit_init(100);
	time_init(100);
	irq_enable();

	boot_state.mem_usable_kb = pmm_init((multiboot_info_t *)mbi);
	boot_state.cmdline = extract_cmdline(mbi);
	boot_state.arch_name = "x86";

	u32 base = (((u32)(uintptr_t)_kernel_end) + 4095u) & ~4095u;

	pmm_reserve(base, base + HEAP_BYTES);
	boot_state.heap_base = base;
	boot_state.heap_size = HEAP_BYTES;

	kernel_main(&boot_state);
	kernel_halt();
}
