// SPDX-License-Identifier: GPL-2.0-only
#include "uart8250.h"
#include "../../include/io.h"

/* Standard 8250 register set relative to the port base. */
#define REG_DATA 0
#define REG_IER  1
#define REG_FCR  2
#define REG_LCR  3
#define REG_MCR  4
#define REG_LSR  5

#define LSR_RX_READY 0x01
#define LSR_TX_EMPTY 0x20

bool uart_init(u16 port, u32 baud) {
	if (baud == 0 || baud > 115200)
		return false;

	u16 div = (u16)(115200 / baud);

	outb(port + REG_IER, 0x00);		/* no interrupts */
	outb(port + REG_LCR, 0x80);		/* DLAB on */
	outb(port + 0, (u8)(div & 0xFF));
	outb(port + 1, (u8)(div >> 8));
	outb(port + REG_LCR, 0x03);		/* 8N1 */
	outb(port + REG_FCR, 0xC7);		/* FIFO enable+clear */
	outb(port + REG_MCR, 0x0B);		/* DTR|RTS|OUT2 */

	/* Line must settle idle-high for the device to exist. */
	return (inb(port + REG_LSR) & 0x60) != 0;
}

int uart_read(u16 port) {
	if (!(inb(port + REG_LSR) & LSR_RX_READY))
		return -1;
	return inb(port + REG_DATA);
}

void uart_write(u16 port, char c) {
	while (!(inb(port + REG_LSR) & LSR_TX_EMPTY))
		;
	outb(port + REG_DATA, (u8)c);
}
