// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_PS2_H
#define OBSIDIAN_PS2_H

#include "../../include/types.h"

/* Raw scancode polling that works with or without IRQs enabled; used
   by the Secure Kernel and any pre-interrupt prompt. Returns -1 when
   no key is waiting. */
int ps2_poll_scancode(void);

bool ps2_controller_init(void);
bool ps2_port1_present(void);
bool ps2_port2_present(void);

/* Send a byte to the aux (port2) device through the controller. */
bool ps2_aux_write(u8 val);
int  ps2_aux_read(void);

/* Discard any pending bytes from the auxiliary port. */
void ps2_flush_aux(void);

#endif
