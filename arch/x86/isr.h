// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_X86_ISR_H
#define OBSIDIAN_X86_ISR_H

#include "../include/types.h"

/*
 * Interrupt plumbing for i686: legacy 8259 PIC + PIT control plus
 * the driver-facing IRQ registration API.
 */

typedef void (*irq_handler_t)(void);

void pic_init(void);
void pit_init(u32 hz);
void irq_install(u8 irq, irq_handler_t handler);
void pic_unmask_irq(u8 irq);
void pic_mask_irq(u8 irq);

#endif
