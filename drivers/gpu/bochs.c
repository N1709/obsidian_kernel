// SPDX-License-Identifier: GPL-2.0-only
#include "bochs.h"
#include "../../include/io.h"

/*
 * Driver for the Bochs "std" VBE dispi interface, emulated by QEMU
 * (-vga std), Bochs itself and QXL in compatibility mode. Registers are
 * reached through legacy ports 0x01CE/0x01CF; the linear framebuffer is
 * the largest memory BAR of the card (BAR0 on QEMU std-vga).
 */

#define DISPI_INDEX 0x01CE
#define DISPI_DATA  0x01CF

#define DISPI_ID      0x00
#define DISPI_XRES    0x01
#define DISPI_YRES    0x02
#define DISPI_BPP     0x03
#define DISPI_ENABLE  0x04
#define DISPI_BANK    0x05
#define DISPI_VIRT_W  0x06
#define DISPI_VIRT_H  0x07
#define DISPI_X_OFF   0x08
#define DISPI_Y_OFF   0x09

#define ENABLE_ON       0x01
#define ENABLE_LFB      0x40
#define KEEP_MEMORY     0x80

static u16 dispi_read(u16 index) {
	outw(DISPI_INDEX, index);
	return inw(DISPI_DATA);
}

static void dispi_write(u16 index, u16 val) {
	outw(DISPI_INDEX, index);
	outw(DISPI_DATA, val);
}

/* Largest memory BAR = video RAM holding the framebuffer. */
static const struct pci_dev_bar {
	u32 base;
	u32 size;
} lfb_bar_of(const struct pci_dev *d) {
	struct pci_dev_bar best = { 0, 0 };

	for (int i = 0; i < 6; i++) {
		if (d->bar_is_io[i] || !d->bar_base[i])
			continue;
		if (d->bar_size[i] > best.size) {
			best.base = d->bar_base[i];
			best.size = d->bar_size[i];
		}
	}
	return best;
}

bool bochs_set_mode(const struct pci_dev *d, struct gpu_device *gpu) {
	const u32 want_w = 1024, want_h = 768, bpp = 32;

	if ((dispi_read(DISPI_ID) & 0xFFF0) != 0xB0C0)
		return false;

	struct pci_dev_bar bar = lfb_bar_of(d);
	u32 needed = want_w * want_h * (bpp / 8);

	if (!bar.base || bar.size < needed)
		return false;

	/* Program the mode with memory contents preserved so the switch
	   does not flash the screen black mid-boot. */
	dispi_write(DISPI_ENABLE, 0);
	dispi_write(DISPI_XRES, (u16)want_w);
	dispi_write(DISPI_YRES, (u16)want_h);
	dispi_write(DISPI_BPP, (u16)bpp);
	dispi_write(DISPI_VIRT_W, (u16)want_w);
	dispi_write(DISPI_VIRT_H, (u16)(want_h));
	dispi_write(DISPI_BANK, 0);
	dispi_write(DISPI_X_OFF, 0);
	dispi_write(DISPI_Y_OFF, 0);
	dispi_write(DISPI_ENABLE,
		    (u16)(ENABLE_ON | ENABLE_LFB | KEEP_MEMORY));

	volatile u32 *lfb = (volatile u32 *)(uintptr_t)bar.base;

	/* Verify the aperture really is writable video memory before we
	   commit to it; some hypervisors hand back unusable BARs. */
	u32 probe_off = ((want_h / 2) * want_w + (want_w / 2));
	u32 old = lfb[probe_off];

	lfb[probe_off] = 0xA55A1357u;
	if (lfb[probe_off] != 0xA55A1357u) {
		lfb[probe_off] = old;
		return false;
	}
	lfb[probe_off] = old;

	gpu->fb_active = true;
	gpu->lfb_phys = bar.base;
	gpu->width = want_w;
	gpu->height = want_h;
	gpu->bpp = bpp;
	gpu->pitch = want_w * (bpp / 8);
	return true;
}
