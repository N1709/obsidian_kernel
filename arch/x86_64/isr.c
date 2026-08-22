// SPDX-License-Identifier: GPL-2.0-only
#include "isr.h"
#include "idt.h"
#include "../init/time.h"
#include "../../include/io.h"
#include "../../include/printk.h"
#include "../../include/panic.h"

/*
 * Legacy 8259 PIC plus 8253 PIT management and the shared trap
 * dispatcher installed in the IDT. Exceptions escalate to panic(),
 * hardware IRQs fan out to handlers registered by drivers.
 */

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI   0x20

#define PIT_CH0   0x40
#define PIT_CMD   0x43

static irq_handler_t irq_handlers[16];

void irq_install(u8 irq, irq_handler_t handler) {
	if (irq < 16)
		irq_handlers[irq] = handler;
}

static void pic_send_eoi(u8 irq) {
	if (irq >= 8)
		outb(PIC2_CMD, PIC_EOI);
	outb(PIC1_CMD, PIC_EOI);
}

void pic_unmask_irq(u8 irq) {
	u16 port = irq < 8 ? PIC1_DATA : PIC2_DATA;
	u8  bit  = irq < 8 ? (u8)irq : (u8)(irq - 8);

	outb(port, inb(port) & ~(u8)(1u << bit));
}

void pic_mask_irq(u8 irq) {
	u16 port = irq < 8 ? PIC1_DATA : PIC2_DATA;
	u8  bit  = irq < 8 ? (u8)irq : (u8)(irq - 8);

	outb(port, inb(port) | (u8)(1u << bit));
}

void pic_init(void) {
	outb(PIC1_CMD, 0x11); io_wait();
	outb(PIC2_CMD, 0x11); io_wait();
	outb(PIC1_DATA, 0x20); io_wait();	/* IRQ0-7  -> vectors 32-39 */
	outb(PIC2_DATA, 0x28); io_wait();	/* IRQ8-15 -> vectors 40-47 */
	outb(PIC1_DATA, 0x04); io_wait();	/* slave on IRQ2 */
	outb(PIC2_DATA, 0x02); io_wait();
	outb(PIC1_DATA, 0x01); io_wait();	/* 8086 mode */
	outb(PIC2_DATA, 0x01); io_wait();

	/* Everything masked until a driver claims its line. */
	outb(PIC1_DATA, 0xFF);
	outb(PIC2_DATA, 0xFF);
}

void pit_init(u32 hz) {
	u32 divisor = 1193182 / hz;

	outb(PIT_CMD, 0x36);
	outb(PIT_CH0, (u8)(divisor & 0xFF));
	outb(PIT_CH0, (u8)((divisor >> 8) & 0xFF));
}

static const char *exception_name(u64 vector) {
	static const char *names[] = {
		"divide error", "debug", "NMI", "breakpoint",
		"overflow", "bound range exceeded",
		"invalid opcode", "device not available",
		"double fault", "coprocessor segment overrun",
		"invalid TSS", "segment not present",
		"stack-segment fault", "general protection fault",
		"page fault", "reserved",
		"x87 floating-point exception", "alignment check",
		"machine check", "SIMD floating-point exception"
	};

	if (vector < sizeof(names) / sizeof(names[0]))
		return names[vector];
	return "unknown exception";
}

void isr_handler(regs_t *regs) {
	if (regs->int_no < 32) {
		panic("cpu exception %llu (%s) err=%llu",
		      (unsigned long long)regs->int_no,
		      exception_name(regs->int_no),
		      (unsigned long long)regs->err_code);
		return;
	}

	u8 irq = (u8)(regs->int_no - 32);

	if (irq == 0)
		time_tick();

	if (irq < 16 && irq_handlers[irq])
		irq_handlers[irq]();

	pic_send_eoi(irq);
}
