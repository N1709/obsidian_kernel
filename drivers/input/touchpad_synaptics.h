// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_SYNAPTICS_H
#define OBSIDIAN_SYNAPTICS_H

#include "../../include/types.h"

/*
 * Synaptics touchpad over the PS/2 auxiliary port, the pointing
 * device shipped in most ThinkPads and countless other laptops.
 * Protocol follows the vendor whitepaper also implemented by
 * linux/drivers/input/mouse/synaptics.c: a magic knock switches the
 * pad to absolute mode where every packet carries full coordinates.
 */

bool synaptics_probe(void);
void synaptics_poll(int *x_out, int *y_out,
		    bool *left, bool *right);

#endif
