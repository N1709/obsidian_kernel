// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_BOCHS_H
#define OBSIDIAN_BOCHS_H

#include "../../include/types.h"
#include "../bus/pci.h"
#include "gpu.h"

bool bochs_set_mode(const struct pci_dev *dev, struct gpu_device *out);

#endif
