// SPDX-License-Identifier: GPL-2.0-only
#include "../../include/obsidian.h"
#include "../../include/printk.h"
#include "pl011.h"
#include "fdt.h"
#include "../init/time.h"
#include "../lib/string.h"

/*
 * C entry for ARM64. start.S hands us the DTB pointer; from there we
 * bring up the serial console, read the firmware's device tree,
 * prepare the boot_info_t contract shared with x86 and call the
 * portable kernel_main().
 */

boot_info_t boot_state;

void kernel_reboot(void) {
	/* PSCI SYSTEM_RESET (function id 0x84000008) via SMC; present on
	   QEMU virt and virtually every modern ARM server/board. */
	register u64 x0 __asm__("x0") = 0x84000008ull;

	__asm__ volatile ("smc #0" : : "r"(x0) : "memory");

	for (;;)
		__asm__ volatile ("wfi");
}

void kernel_halt(void) {
	for (;;)
		__asm__ volatile ("wfi");
}

static void cpu_report(void) {
	u64 midr;

	__asm__ volatile ("mrs %0, midr_el1" : "=r"(midr));

	const char *impl = "unknown";

	switch ((midr >> 24) & 0xFF) {
	case 0x41: impl = "ARM";     break;
	case 0x51: impl = "Qualcomm";break;
	case 0x4E: impl = "Nvidia";  break;
	case 0x53: impl = "Samsung"; break;
	}

	kprintf("cpu %s implementer %02llx part %03llx rev %llu\n",
		impl, (midr >> 24) & 0xFF,
		(midr >> 4) & 0xFFF, midr & 0xF);
	kprintf("cpu %d core(s) reported by firmware\n",
		fdt_cpu_count());
}

/* Heap: carve a fixed slice out of discovered RAM, above the image. */
#define HEAP_BYTES (8u * 1024u * 1024u)

extern char __bss_end[];

void arch_start(u64 dtb) {
	if (!fdt_init(dtb)) {
		kputs("fatal: no usable device tree, halting\n");
		kernel_halt();
	}

	pl011_init(fdt_uart_base());

	boot_state.cmdline = fdt_chosen_bootargs();
	boot_state.arch_name = "arm64";

	u64 mem_base, mem_size;

	fdt_memory_base(&mem_size);
	mem_base = fdt_memory_base(&mem_size);

	if (!mem_size) {	/* firmware gave no memory node */
		mem_base = 0x40000000ull;
		mem_size = 128ull * 1024 * 1024;
	}

	boot_state.mem_usable_kb = (u32)(mem_size / 1024);

	u64 heap = ((uintptr_t)__bss_end + 0xFFFFFull) & ~0xFFFFFull;

	boot_state.heap_base = (u32)heap;
	boot_state.heap_size = HEAP_BYTES;

	time_init(100);

	cpu_report();
	kprintf("mem %llu MB at 0x%llx\n",
		(unsigned long long)(mem_size >> 20),
		(unsigned long long)mem_base);

	kernel_main(&boot_state);
	kernel_halt();
}
