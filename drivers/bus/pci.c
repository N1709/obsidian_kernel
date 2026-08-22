// SPDX-License-Identifier: GPL-2.0-only
#include "pci.h"
#include "../../include/io.h"
#include "../gpu/console.h"
#include "../../include/printk.h"

/*
 * PCI configuration mechanism 1 (ports 0xCF8/0xCFC). We enumerate bus 0
 * and follow PCI-to-PCI bridges recursively, which covers every machine
 * this kernel targets including QEMU and real ThinkPads.
 */

void pci_scan_bus(u8 bus);

#define CONF_ADDR 0xCF8
#define CONF_DATA 0xCFC

static struct pci_dev devices[PCI_MAX_DEVICES];
static int ndevices;

static u32 pci_conf_read(u8 bus, u8 dev, u8 fn, u8 reg) {
	u32 addr = 0x80000000u |
		   ((u32)bus << 16) | ((u32)dev << 11) |
		   ((u32)fn << 8) | (reg & 0xFC);

	outl(CONF_ADDR, addr);
	return inl(CONF_DATA);
}

static void pci_conf_write(u8 bus, u8 dev, u8 fn, u8 reg, u32 val) {
	u32 addr = 0x80000000u |
		   ((u32)bus << 16) | ((u32)dev << 11) |
		   ((u32)fn << 8) | (reg & 0xFC);

	outl(CONF_ADDR, addr);
	outl(CONF_DATA, val);
}

static u32 bar_size_of(struct pci_dev *d, int i) {
	u32 orig = d->bar_base[i];
	u32 size;

	if (d->bar_is_io[i]) {
		pci_conf_write(d->bus, d->dev, d->fn, 0x10 + i * 4,
			       0xFFFFFFFFu);
		size = pci_conf_read(d->bus, d->dev, d->fn, 0x10 + i * 4);
		size = (~size + 1) & 0xFFFFu;
	} else {
		pci_conf_write(d->bus, d->dev, d->fn, 0x10 + i * 4,
			       0xFFFFFFFFu);
		size = pci_conf_read(d->bus, d->dev, d->fn, 0x10 + i * 4);
		size = (~size + 1);
	}
	pci_conf_write(d->bus, d->dev, d->fn, 0x10 + i * 4, orig);
	return size;
}

static void add_device(u8 bus, u8 dev, u8 fn) {
	struct pci_dev *d;
	u32 id, cls;

	if (ndevices >= PCI_MAX_DEVICES)
		return;

	id = pci_conf_read(bus, dev, fn, 0x00);
	if ((id & 0xFFFF) == 0xFFFF || (id & 0xFFFF) == 0)
		return;

	d = &devices[ndevices];
	d->vendor = (u16)(id & 0xFFFF);
	d->device = (u16)(id >> 16);
	d->bus = bus;
	d->dev = dev;
	d->fn = fn;

	cls = pci_conf_read(bus, dev, fn, 0x08);
	d->class_code = (u8)(cls >> 24);
	d->subclass = (u8)(cls >> 16);
	d->prog_if = (u8)(cls >> 8);
	d->header_type =
		(u8)(pci_conf_read(bus, dev, fn, 0x0C) >> 16);
	d->valid = true;

	for (int i = 0; i < 6; i++) {
		u32 bar = pci_conf_read(bus, dev, fn, 0x10 + i * 4);

		if (bar == 0)
			continue;
		if ((bar & 1) && i < 4) {
			d->bar_is_io[i] = true;
			d->bar_base[i] = bar & ~3u;
			d->bar_size[i] = bar_size_of(d, i);
		} else if (!(bar & 1)) {
			static const u32 mask_lo[] =
				{ ~0xFu, ~0xFu, ~0xFu, ~0xFu };
			d->bar_is_io[i] = false;
			d->bar_base[i] = bar & mask_lo[(u32)(bar >> 1) & 3];
			d->bar_size[i] = bar_size_of(d, i);
		}
	}

	kprintf("pci %02x:%02x.%d [%s] %04x:%04x\n",
		bus, dev, fn, pci_class_name(d->class_code),
		d->vendor, d->device);

	ndevices++;
}

static void scan_function(u8 bus, u8 dev, u8 fn);

static void scan_slot(u8 bus, u8 dev) {
	add_device(bus, dev, 0);

	if (!devices[0].valid && ndevices == 0)
		return;

	u32 ht = pci_conf_read(bus, dev, 0, 0x0C);

	if (((ht >> 16) & 0x80) == 0)
		return;

	for (u8 fn = 1; fn < 8; fn++)
		scan_function(bus, dev, fn);
}

static void scan_function(u8 bus, u8 dev, u8 fn) {
	u32 id = pci_conf_read(bus, dev, fn, 0x00);

	if ((id & 0xFFFF) == 0xFFFF)
		return;

	u32 before = ndevices;
	add_device(bus, dev, fn);

	if (ndevices > before) {
		struct pci_dev *d = &devices[ndevices - 1];

		if (d->class_code == 0x06 && d->subclass == 0x04) {
			u32 sec = pci_conf_read(bus, dev, fn, 0x18);

			pci_scan_bus((u8)(sec >> 8));
		}
	}
}

void pci_scan_bus(u8 bus) {
	for (u8 dev = 0; dev < 32; dev++)
		scan_slot(bus, dev);
}

void pci_init(void) {
	ndevices = 0;
	pci_scan_bus(0);
}

int pci_count(void) {
	return ndevices;
}

const struct pci_dev *pci_get(int idx) {
	if (idx < 0 || idx >= ndevices)
		return NULL;
	return &devices[idx];
}

int pci_find_class(u8 class_code, u8 subclass) {
	for (int i = 0; i < ndevices; i++) {
		if (devices[i].class_code == class_code &&
		    (subclass == 0xFF || devices[i].subclass == subclass))
			return i;
	}
	return -1;
}

int pci_find_vendor_device(u16 vendor, u16 device) {
	for (int i = 0; i < ndevices; i++) {
		if (devices[i].vendor == vendor &&
		    devices[i].device == device)
			return i;
	}
	return -1;
}

const struct pci_dev *pci_display_controller(void) {
	int i = pci_find_class(0x03, 0xFF);

	return (i >= 0) ? &devices[i] : NULL;
}

const char *pci_class_name(u8 class_code) {
	switch (class_code) {
	case 0x00: return "legacy";
	case 0x01: return "storage";
	case 0x02: return "network";
	case 0x03: return "display";
	case 0x04: return "multimedia";
	case 0x05: return "memory";
	case 0x06: return "bridge";
	case 0x07: return "comm";
	case 0x08: return "system";
	case 0x09: return "input";
	case 0x0C: return "serial-bus";
	default:   return "other";
	}
}
