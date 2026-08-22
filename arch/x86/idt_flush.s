; SPDX-License-Identifier: GPL-2.0-only
global idt_flush
idt_flush:
    mov eax, [esp + 4]
    lidt [eax]
    ret
