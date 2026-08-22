// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_IRQ_H
#define OBSIDIAN_IRQ_H

#include "types.h"

/*
 * Architecture-neutral interrupt registration API.
 * Drivers include this instead of touching per-CPU headers directly;
 * each architecture directory supplies the real implementation and
 * unknown architectures get safe no-op stubs so portable code still
 * links.
 */

#ifdef __x86_64__
#include "../arch/x86_64/isr.h"
#elif defined(__i386__)
#include "../arch/x86/isr.h"
#else
typedef void (*irq_handler_t)(void);

static inline void irq_install(u8 irq, irq_handler_t handler) {
	(void)irq;
	(void)handler;
}

static inline void pic_unmask_irq(u8 irq) { (void)irq; }
static inline void pic_mask_irq(u8 irq)   { (void)irq; }
#endif

#endif
