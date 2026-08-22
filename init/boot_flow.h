// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_BOOT_FLOW_H
#define OBSIDIAN_BOOT_FLOW_H

#include "../include/obsidian.h"

/* Full driver bring-up sequence used by the standard kernel image. */
void standard_boot(const boot_info_t *boot);

#endif
