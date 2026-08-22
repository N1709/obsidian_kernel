// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_UART8250_H
#define OBSIDIAN_UART8250_H

#include "../../include/types.h"

/*
 * Legacy 16550-family serial ports. Besides serving the classic
 * Microsoft serial mouse this driver doubles as the debug console on
 * boards without display hardware.
 */

bool uart_init(u16 port, u32 baud);
int  uart_read(u16 port);		/* -1 when empty */
void uart_write(u16 port, char c);

#endif
