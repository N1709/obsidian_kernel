// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_E1000_H
#define OBSIDIAN_E1000_H

#include "../../include/types.h"
#include "../bus/pci.h"

bool e1000_init(const struct pci_dev *dev);

bool e1000_present(void);

int  e1000_send(const void *frame, u16 len);
int  e1000_poll(u8 *buf, u16 max_len);

#endif
