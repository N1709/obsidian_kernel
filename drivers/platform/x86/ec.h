// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_EC_H
#define OBSIDIAN_EC_H

#include "../../../include/types.h"

bool ec_probe(void);
bool ec_read(u8 reg, u8 *val);
bool ec_write(u8 reg, u8 val);

#endif
