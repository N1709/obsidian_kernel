; SPDX-License-Identifier: GPL-2.0-only
global gdt_flush
gdt_flush:
    mov eax, [esp + 4]
    lgdt [eax]

    mov ax, 0x10        ; kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    jmp 0x08:.flush      ; kernel code selector, forces CS reload
.flush:
    ret
