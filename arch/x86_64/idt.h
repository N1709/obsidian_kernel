// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_X86_64_IDT_H
#define OBSIDIAN_X86_64_IDT_H

#include "../../include/types.h"

/*
 * Register snapshot pushed by isr_common (isr_stubs.s).
 * Field order must match the push sequence in that file.
 */
typedef struct {
	u64 r15, r14, r13, r12, r11, r10, r9,  r8;
	u64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
	u64 int_no, err_code;
	/* pushed by CPU */
	u64 rip, cs, rflags, rsp, ss;
} __attribute__((packed)) regs_t;

void idt_init(void);

#endif
