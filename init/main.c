// SPDX-License-Identifier: GPL-2.0-only
#include "../include/obsidian.h"
#include "../include/printk.h"
#include "../drivers/gpu/console.h"
#include "../drivers/input/keyboard.h"
#include "cmdline.h"
#include "boot_flow.h"
#include "../include/io.h"

/*
 * The 'version' command line entry boots into an information screen
 * instead of the normal boot flow. This gives users a GRUB menu entry
 * that answers "which kernel am I running" without touching hardware.
 */

static void version_screen(const boot_info_t *boot) {
	kprintf("\n");
	kprintf("%s %s (%s kernel)\n", KERNEL_NAME, KERNEL_VERSION,
		KERNEL_VARIANT_NAME);
	kprintf("built for %-6s on %s\n", KERNEL_BUILD_ARCH,
		KERNEL_BUILD_DATE);
#ifdef __VERSION__
	kprintf("compiler %s\n", __VERSION__);
#endif
#ifdef OB_HAVE_RUST_ALLOC
	kprintf("allocator rust (obsidian_alloc)\n");
#else
	kprintf("allocator C fallback\n");
#endif
	kprintf("running on %s\n", boot->arch_name);
	kprintf("usable memory %llu KB\n",
		(unsigned long long)boot->mem_usable_kb);
	if (boot->cmdline[0])
		kprintf("cmdline '%s'\n", boot->cmdline);

	kprintf("\npress any key to reboot\n");

	while (keyboard_try_getchar() < 0)
		cpu_halt();

	kernel_reboot();
}

void kernel_main(const boot_info_t *boot);

/* Defined in secure_boot.c for OBSIDIAN_SECURE builds. */
#ifdef OBSIDIAN_SECURE
extern void secure_main(const boot_info_t *boot);
#endif

void kernel_main(const boot_info_t *boot) {
	console_init();
	cmdline_parse(boot->cmdline);

	kprintf("\n%s %s -- %s kernel -- %s\n", KERNEL_NAME,
		KERNEL_VERSION, KERNEL_VARIANT_NAME, boot->arch_name);
	kprintf("built %s\n\n", KERNEL_BUILD_DATE);

	if (cmdline_action() == CMDLINE_ACTION_VERSION)
		version_screen(boot);

#ifdef OBSIDIAN_SECURE
	secure_main(boot);
#else
	standard_boot(boot);
#endif
}
