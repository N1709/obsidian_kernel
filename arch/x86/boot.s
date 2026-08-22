; SPDX-License-Identifier: GPL-2.0-only
; Multiboot1 entry point

MBALIGN  equ 1<<0
MEMINFO  equ 1<<1
FLAGS    equ MBALIGN | MEMINFO
MAGIC    equ 0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
	dd MAGIC
	dd FLAGS
	dd CHECKSUM

section .bss
align 16
stack_bottom:
	resb 65536
stack_top:

section .text
global _start
extern arch_start
_start:
	mov  esp, stack_top
	push ebx		; multiboot info pointer
	push eax		; multiboot magic
	call arch_start
	cli
.hang:
	hlt
	jmp .hang
