// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_GAMEPORT_H
#define OBSIDIAN_GAMEPORT_H

#include "../../include/types.h"

bool gameport_probe(void);
bool gameport_read(int *x, int *y, bool *a, bool *b,
		   bool *c, bool *d);

#endif
