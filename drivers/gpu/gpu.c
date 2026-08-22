// SPDX-License-Identifier: GPL-2.0-only
#include "gpu.h"
#include "bochs.h"
#include "console.h"
#include "fb_console.h"
#include "../bus/pci.h"
#include "../../include/printk.h"
#include "../../lib/string.h"

/*
 * GPU subsystem. Matches the display controller found by PCI against a
 * small driver table. When a driver claims the card and successfully
 * sets a linear framebuffer mode, the console is re-bound to it so all
 * later kernel output is drawn through the detected GPU. Otherwise the
 * kernel keeps running happily on VGA text.
 */

static struct gpu_device g_gpu;

const struct gpu_device *gpu_info(void) {
	return &g_gpu;
}

static void report_text_only(const char *reason) {
	g_gpu.fb_active = false;
	kprintf("gpu %s -- staying on text console (%s)\n",
		g_gpu.name ? g_gpu.name : "none", reason);
}

static void try_bochs_family(void) {
	const struct pci_dev *d = pci_display_controller();

	if (!d)
		return;

	g_gpu.vendor = d->vendor;
	g_gpu.device = d->device;

	if (d->vendor == 0x1234 && d->device == 0x1111)
		g_gpu.name = "QEMU/Bochs SVGA";
	else if (d->vendor == 0x1b36 && d->device == 0x0100)
		g_gpu.name = "QXL";
	else if (d->vendor == 0x8086)
		g_gpu.name = "Intel HD graphics";
	else if (d->vendor == 0x1002)
		g_gpu.name = "AMD graphics";
	else if (d->vendor == 0x10de)
		g_gpu.name = "NVIDIA graphics";
	else if (d->vendor == 0x15ad)
		g_gpu.name = "VMware SVGA";
	else if (d->vendor == 0x80ee)
		g_gpu.name = "VirtualBox graphics";
	else
		g_gpu.name = "unknown display";

#ifdef OBSIDIAN_SECURE
	/* The Secure Kernel never touches display registers: safe text. */
	return;
#endif

	/* Bochs-compatible register interface: QEMU std-vga and QXL. */
	if ((d->vendor == 0x1234 && d->device == 0x1111) ||
	    (d->vendor == 0x1b36 && d->device == 0x0100)) {
		if (!bochs_set_mode(d, &g_gpu))
			report_text_only("mode set failed");
	}
}

void gpu_init(void) {
	memset(&g_gpu, 0, sizeof(g_gpu));

	try_bochs_family();

	if (g_gpu.fb_active) {
		fb_console_install(&g_gpu);
		kprintf("gpu %04x:%04x '%s' -> %ux%ux%u lfb @ %08x\n",
			g_gpu.vendor, g_gpu.device, g_gpu.name,
			g_gpu.width, g_gpu.height, g_gpu.bpp,
			g_gpu.lfb_phys);
	} else if (!g_gpu.name && pci_display_controller()) {
		report_text_only("unsupported");
	}
}
