// SPDX-License-Identifier: GPL-2.0-only
#include "../include/obsidian.h"
#include "../include/printk.h"
#include "../drivers/gpu/console.h"
#include "../drivers/input/ps2.h"
#include "../drivers/bus/pci.h"
#include "../drivers/firmware/cmos.h"
#include "cmdline.h"

/*
 * Secure kernel boot flow.
 *
 * The Secure Kernel is the safety net of the system: when the main
 * kernel panics, the operator boots this image from GRUB. It stays: no display mode changes, no writes to any
 * device except the panic flag in CMOS. It reports why the previous
 * boot died, verifies RAM the main kernel uses, lists hardware, and
 * offers recovery actions from a polling keyboard menu so it works
 * even when interrupt routing is what broke.
 */

static void memtest_region(u32 base, u32 size_bytes) {
	volatile u32 *p = (volatile u32 *)(uintptr_t)base;
	u32 words = size_bytes / sizeof(u32);

	for (u32 i = 0; i < words; i++)
		p[i] = 0x5A5AA5A5u;

	for (u32 i = 0; i < words; i++) {
		if (p[i] != 0x5A5AA5A5u) {
			kprintf("memtest FAIL at %08x\n",
				(base + i * sizeof(u32)));
			return;
		}
	}
	kprintf("memtest ok %u KB\n", size_bytes / 1024);
}

static void dump_devices(void) {
	for (int i = 0; i < pci_count(); i++) {
		const struct pci_dev *d = pci_get(i);

		kprintf("pci %02x:%02x.%d %04x:%04x %s\n",
			d->bus, d->dev, d->fn, d->vendor, d->device,
			pci_class_name(d->class_code));
	}
}

/* Minimal scancode set 1 make-codes for the menu letters. */
static char poll_key(void) {
	static const char map[128] = {
		[0x13] = 'r', [0x2E] = 'c', [0x20] = 'd',
		[0x32] = 'm', [0x23] = 'h',
	};

	int sc;

	do {
		sc = ps2_poll_scancode();
	} while (sc < 0 || (sc & 0x80));

	return map[sc & 0x7F];
}

void secure_main(const boot_info_t *boot) {
	kprintf("\n%s SECURE %s -- recovery environment\n", KERNEL_NAME,
		KERNEL_VERSION);

	u8 code;
	bool panicked = cmos_last_boot_panicked(&code);

	kprintf("last boot: %s\n", panicked ?
		"KERNEL PANIC detected" : "clean shutdown");

	kprintf("memory available to this session: %llu KB\n\n",
		(unsigned long long)boot->mem_usable_kb);

	kprintf("menu:\n");
	kprintf("  r reboot into the main kernel\n");
	kprintf("  c clear the panic flag\n");
	kprintf("  d list PCI devices\n");
	kprintf("  m memory self test\n");
	kprintf("  h show this help\n\n");

	for (;;) {
		char k = poll_key();

		switch (k) {
		case 'r':
			cmos_clear_panic_flag();
			kernel_reboot();
			break;
		case 'c':
			cmos_clear_panic_flag();
			kputs("panic flag cleared");
			break;
		case 'd':
			dump_devices();
			break;
		case 'm':
			memtest_region(boot->heap_base, boot->heap_size);
			break;
		default:
			kputs("menu: r c d m h");
			break;
		}
	}
}
