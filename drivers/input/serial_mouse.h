// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_SERIAL_MOUSE_H
#define OBSIDIAN_SERIAL_MOUSE_H

#include "../../include/types.h"

bool serial_mouse_probe(void);
void serial_mouse_poll(int *dx, int *dy, bool *left, bool *right);

#endif
