// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_SMBIOS_H
#define OBSIDIAN_SMBIOS_H

#include "../../include/types.h"

bool smbios_init(void);
const char *smbios_vendor(void);
const char *smbios_product(void);

#endif
