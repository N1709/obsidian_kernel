// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_PCI_H
#define OBSIDIAN_PCI_H

#include "../../include/types.h"

#define PCI_MAX_DEVICES 32

struct pci_dev {
	u8  bus, dev, fn;
	u16 vendor, device;
	u8  class_code, subclass, prog_if;
	u8  header_type;
	u32 bar_base[6];
	u32 bar_size[6];
	bool bar_is_io[6];
	bool valid;
};

void pci_init(void);
int  pci_count(void);
const struct pci_dev *pci_get(int idx);

int pci_find_class(u8 class_code, u8 subclass);
int pci_find_vendor_device(u16 vendor, u16 device);
const struct pci_dev *pci_display_controller(void);
const char *pci_class_name(u8 class_code);

#endif
