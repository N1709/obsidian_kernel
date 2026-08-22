; SPDX-License-Identifier: GPL-2.0-only
[bits 64]
section .text

global idt_flush
idt_flush:
    lidt [rdi]
    ret
