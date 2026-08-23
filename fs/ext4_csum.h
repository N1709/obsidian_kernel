// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_EXT4_CSUM_H
#define OBSIDIAN_EXT4_CSUM_H

#include "../include/types.h"

u32 ext4_crc32c(u32 crc, const void *buf, u32 len);
u16 ext4_crc16(u16 crc, const void *buf, u32 len);

#endif
