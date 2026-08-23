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
#include "../drivers/block/blkdev.h"
#include "../fs/ext4.h"
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
#include "../lib/string.h"
#include "../drivers/net/net_detect.h"
#include "../security/security.h"
#include "cmdline.h"

/* Provided by each architecture directory. */
extern void cpu_report(void);
extern void ata_probe(void);
extern void virtio_blk_probe(void);

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

/*
 * Persistence demo: keep a boot counter in /boots on the ext4 volume.
 * A number that grows across reboots proves the block device, the
 * ext4 write path and the journal all survive a power cycle.
 */
static void ext4_persistence_demo(void) {
	if (!ext4_mount())
		return;

	char buf[64] = { 0 };
	u32 boots = 0;
	int n = ext4_read("/boots", buf, sizeof buf - 1);

	if (n > 0)
		for (int i = 0; i < n && buf[i] >= '0' &&
		     buf[i] <= '9'; i++)
			boots = boots * 10 + (u32)(buf[i] - '0');

	boots++;

	char out[32];
	u32 len = 0;
	u32 v = boots;
	char tmp[12];

	do {
		tmp[len++] = (char)('0' + v % 10);
		v /= 10;
	} while (v);

	for (u32 i = 0; i < len; i++)
		out[i] = tmp[len - 1 - i];
	out[len] = '\n';

	if (ext4_write("/boots", out, len + 1)) {
		kputs("ext4 demo write failed");
		return;
	}

	kprintf("ext4 persistence: boot #%u recorded\n", boots);

	struct ext4_dirent_info ents[16];
	int cnt = ext4_listdir("/", ents, 16);

	kputs("ext4 root:");
	for (int i = 0; i < cnt; i++) {
		char p[EXT4_MAX_PATH];
		u32 l = k_strlen(ents[i].name);

		memcpy(p + 1, ents[i].name, l);
		p[0] = '/';
		p[l + 1] = 0;
		kprintf("  %s%s %lu bytes\n", ents[i].name,
			ents[i].type == 2 ? "/" : " ",
			ext4_size(p));
	}
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

	blkdev_init();
	ata_probe();
	virtio_blk_probe();
	ext4_persistence_demo();

	u8 code;

	if (cmos_last_boot_panicked(&code))
		kprintf("warn previous boot panicked (%u)\n", code);
	cmos_clear_panic_flag();

	idle_loop();
}
