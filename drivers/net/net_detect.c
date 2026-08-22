// SPDX-License-Identifier: GPL-2.0-only
#include "net_detect.h"
#include "net.h"
#include "e1000.h"
#include "../bus/pci.h"
#include "../../include/printk.h"

/*
 * Network discovery pass over the PCI enumeration results.
 *
 * Wired controllers with an in-tree driver are brought up directly.
 * Wireless cards are identified by PCI ID and reported honestly: real
 * WiFi needs firmware blobs and a full 802.11 stack, which is a later
 * milestone; until then the kernel says exactly what it found.
 */

struct net_id_entry {
	u16 vendor;
	u16 device;
	const char *name;
	bool wireless;
};

static const struct net_id_entry known_ids[] = {
	{ 0x8086, 0x100E, "Intel 82540EM",     false },
	{ 0x8086, 0x100F, "Intel 82545EM",     false },
	{ 0x8086, 0x153A, "Intel I217-LM",     false },
	{ 0x10EC, 0x8139, "Realtek RTL8139",   false },
	{ 0x10EC, 0x8168, "Realtek RTL8111",   false },
	{ 0x14E4, 0x1690, "Broadcom NetXtreme",false },

	{ 0x8086, 0x4222, "Intel PRO/Wireless 3945ABG", true },
	{ 0x8086, 0x08B1, "Intel Wireless 7260",        true },
	{ 0x168C, 0x002B, "Atheros AR9285",             true },
	{ 0x14E4, 0x4315, "Broadcom BCM4312",           true },
	{ 0x10EC, 0xC822, "Realtek RTL8822CE",          true },
	{ 0x14C3, 0x7961, "MediaTek MT7921",            true },
};

static const struct net_id_entry *lookup(u16 vendor, u16 device) {
	for (unsigned i = 0; i < sizeof(known_ids) / sizeof(known_ids[0]);
	     i++) {
		if (known_ids[i].vendor == vendor &&
		    known_ids[i].device == device)
			return &known_ids[i];
	}
	return NULL;
}

const char *net_pci_name(u16 vendor, u16 device) {
	const struct net_id_entry *e = lookup(vendor, device);

	return e ? e->name : "";
}

static void handle_device(const struct pci_dev *d,
			  const struct net_id_entry *known) {
	const char *name =
		known ? known->name : net_pci_name(d->vendor, d->device);

	if (!*name)
		name = "unidentified adapter";

	if (known && !known->wireless &&
	    d->vendor == 0x8086 &&
	    (d->device == 0x100E || d->device == 0x100F)) {
		if (e1000_init(d))
			return;
		kprintf("net %s present but init failed\n", name);
		return;
	}

	if ((known && known->wireless) ||
	    (!known && d->subclass == 0x80)) {
		kprintf("wifi %s at %02x:%02x.%d "
			"(detected; firmware stack pending)\n",
			name, d->bus, d->dev, d->fn);
	} else {
		kprintf("net %s at %02x:%02x.%d (no driver bound)\n",
			name, d->bus, d->dev, d->fn);
	}
}

void net_detect_devices(void) {
	int wired = 0, wireless = 0;

	for (int i = 0; i < pci_count(); i++) {
		const struct pci_dev *d = pci_get(i);

		if (d->class_code != 0x02)
			continue;

		handle_device(d, lookup(d->vendor, d->device));
		wired++;
	}

	(void)wireless;
	if (wired || netdev_count())
		kprintf("net discovery done: %d controller(s), "
			"%d driver-bound\n", wired, netdev_count());
}
