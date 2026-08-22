; SPDX-License-Identifier: GPL-2.0-only
[bits 64]
section .text

global gdt_flush
gdt_flush:
    lgdt [rdi]
    mov  ax, 0x10
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax
    pop  rdi            ; return address
    mov  rax, 0x08
    push rax
    push rdi
    retfq               ; far return to reload CS
