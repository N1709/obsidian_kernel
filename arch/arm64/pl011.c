// SPDX-License-Identifier: GPL-2.0-only
#include "pl011.h"
#include "../../include/types.h"

/*
 * Minimal polled PL011. Firmware (QEMU/UEFI/U-Boot) usually leaves the
 * UART clocked and enabled; we still program it defensively so the
 * kernel works from a cold reset.
 */

#define REG_DR     0x000
#define REG_FR     0x018
#define REG_IBRD   0x024
#define REG_FBRD   0x028
#define REG_LCRH   0x02c
#define REG_CR     0x030
#define REG_IMSC   0x038

#define FR_BUSY    (1u << 3)
#define FR_TXFF    (1u << 5)
#define FR_RXFE    (1u << 4)

static volatile u32 *uart;
static u64 uart_base;

static inline void wr(u32 off, u32 v) {
	uart[off / 4] = v;
}

static inline u32 rd(u32 off) {
	return uart[off / 4];
}

void pl011_init(u64 base) {
	uart_base = base;
	uart = (volatile u32 *)(uintptr_t)base;

	wr(REG_IMSC, 0);			/* no interrupts: polled */
	wr(REG_CR, 0);				/* disable while tuning */
	wr(REG_IBRD, 13);			/* 115200 @ 24 MHz */
	wr(REG_FBRD, 1);
	wr(REG_LCRH, (3u << 5) | (1u << 4));	/* 8N1, FIFO on */
	wr(REG_CR, (1u << 0) | (1u << 8) | (1u << 9));
}

void pl011_putc(char c) {
	if (!uart)
		return;

	while (rd(REG_FR) & FR_BUSY)
		;

	if (c == '\n')
		pl011_putc('\r');

	wr(REG_DR, (u8)c);
}

int pl011_getc(void) {
	if (!uart)
		return -1;

	if (rd(REG_FR) & FR_RXFE)
		return -1;

	return (int)(rd(REG_DR) & 0xFF);
}

void __console_fallback_putc(char ch) {
	pl011_putc(ch);
}

u64 pl011_base(void) {
	return uart_base ? uart_base : PL011_BASE_DEFAULT;
}
