// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_IDT_H
#define OBSIDIAN_IDT_H

#include "../include/types.h"

/* Register snapshot built by isr_common (kernel/isr_stubs.s) on every
   interrupt/exception. Field order must match the push sequence there. */
typedef struct {
    u32 gs, fs, es, ds;
    u32 edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    u32 int_no, err_code;
    u32 eip, cs, eflags;
} regs_t;

void idt_init(void);

#endif
