// SPDX-License-Identifier: GPL-2.0-only
#include "../include/obsidian.h"
#include "../include/printk.h"
#include "../drivers/gpu/console.h"
#include "../drivers/input/ps2.h"
#include "../drivers/input/keyboard.h"
#include "../drivers/input/mouse.h"
#include "../drivers/input/touchpad_synaptics.h"
#include "../drivers/input/serial_mouse.h"
#include "../drivers/input/gameport.h"
#include "../drivers/gpu/backlight.h"
#include "../net/netstack.h"
#include "../fs/ramfs.h"
#include "../drivers/gpu/gpu.h"
#include "../drivers/bus/pci.h"
#include "../drivers/platform/x86/lenovo/thinkpad.h"
#include "../drivers/firmware/smbios.h"
#include "../drivers/firmware/cmos.h"
#include "../drivers/net/net.h"
#include "../drivers/acpi/acpi.h"
#include "../include/panic.h"
#include "../include/io.h"
#include "../mm/heap.h"
#include "../drivers/net/net_detect.h"
#include "../security/security.h"
#include "cmdline.h"

/* Provided by each architecture directory. */
extern void cpu_report(void);

/*
 * Full-feature boot sequence used by the standard kernel.
 * Order matters: security first, heap second, then discovery layers,
 * then drivers that depend on discovery.
 */

static void bringup_security_and_heap(const boot_info_t *boot) {
	security_early_init();

	if (!heap_init(boot->heap_base, boot->heap_size))
		panic("heap init failed");

	security_report();
}

static void bringup_discovery(void) {
	pci_init();

	gpu_init();

	if (backlight_probe()) {
		backlight_set(80);
		kputs("gpu display backlight control ready\n");
	}

	if (!acpi_find_rsdp())
		kputs("acpi no RSDP found");
	else
		acpi_walk_tables(acpi_rsdp_get());

	smbios_init();
	thinkpad_init(cmdline_flag("thinkpad_fanctl"));
}

static void bringup_drivers(void) {
	if (ps2_controller_init()) {
		if (!keyboard_init())
			kputs("input keyboard absent");
		mouse_init();
		synaptics_probe();
	} else {
		synaptics_probe();
	}

	if (!serial_mouse_probe())
		kputs("input no serial mouse");

	if (gameport_probe())
		kputs("input legacy gameport joystick found");
}

static void idle_loop(void) {
	kputs("\nready. type keys to test input, ESC reboots.");

	for (;;) {
		net_poll();

		int c = keyboard_try_getchar();

		while (c >= 0) {
			if (c == 0x1B)
				kernel_reboot();
			if (c >= ' ' || c == '\n' || c == '\r')
				console_putc((char)c);
			c = keyboard_try_getchar();
		}
		cpu_halt();
	}
}

void standard_boot(const boot_info_t *boot) {
	bringup_security_and_heap(boot);
	bringup_discovery();
	bringup_drivers();
	net_detect_devices();
	net_stack_init();

	ramfs_init();
	kputs("fs ramfs mounted");

	u8 code;

	if (cmos_last_boot_panicked(&code))
		kprintf("warn previous boot panicked (%u)\n", code);
	cmos_clear_panic_flag();

	idle_loop();
}
