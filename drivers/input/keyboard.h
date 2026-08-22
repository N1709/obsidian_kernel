// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_KEYBOARD_H
#define OBSIDIAN_KEYBOARD_H

#include "../../include/types.h"

bool keyboard_init(void);
int  keyboard_getchar(void);		/* blocking */
int  keyboard_try_getchar(void);	/* -1 when empty */

#endif
