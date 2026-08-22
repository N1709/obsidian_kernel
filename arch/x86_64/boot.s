; Obsidian Kernel x86_64 boot entry.
; Multiboot2 header, CPU feature checks, long-mode switch with an
; identity map covering the full 4 GiB so PCI BARs and ACPI tables are
; directly addressable, then a call into the C entry point.

%define MB2_MAGIC     0xe85250d6
%define MB2_ARCH_I386 0
%define MB2_HDRLEN    24
%define MB2_CHECKSUM  -(MB2_MAGIC + MB2_ARCH_I386 + MB2_HDRLEN)

section .multiboot2
align 8
mb2_header:
    dd MB2_MAGIC
    dd MB2_ARCH_I386
    dd MB2_HDRLEN
    dd MB2_CHECKSUM
    dw 0
    dw 0
    dd 8

section .text

global _start
extern arch_start

[bits 32]
_start:
    cli
    cld

    ; CPUID must be toggleable before anything else can be trusted.
    pushfd
    pop  eax
    mov  ecx, eax
    xor  eax, (1 << 21)
    push eax
    popfd
    pushfd
    pop  eax
    xor  eax, ecx
    test eax, (1 << 21)
    jz   .unsupported

    ; Long mode requires extended leaf 0x80000001 with LM set in EDX.
    mov  eax, 0x80000000
    cpuid
    cmp  eax, 0x80000001
    jb   .unsupported
    mov  eax, 0x80000001
    cpuid
    test edx, (1 << 29)
    jz   .unsupported

    ; Page tables live below 1 MiB where nothing else is loaded.
    ; PML4@0x1000, PDPT@0x2000, four page directories @0x3000-0x6FFF
    ; give one giant 4 GiB identity map using 2 MiB pages.
    mov  edi, 0x1000
    xor  eax, eax
    mov  ecx, 0x1800          ; zero 24 KiB of tables
    rep  stosd

    mov  dword [0x1000], 0x2003        ; PML4[0] -> PDPT
    mov  dword [0x2000], 0x3003        ; PDPT[0] -> PD 0
    mov  dword [0x2004], 0x4003        ; PDPT[1] -> PD 1
    mov  dword [0x2008], 0x5003        ; PDPT[2] -> PD 2
    mov  dword [0x200C], 0x6003        ; PDPT[3] -> PD 3

    mov  edi, 0x3000
    mov  eax, 0x0083                   ; present | rw | 2 MiB page
    xor  edx, edx
    mov  ecx, 2048                     ; 4 * 512 entries = 4 GiB
.fill_tables:
    mov  [edi], eax
    mov  dword [edi + 4], 0
    add  eax, 0x00200000
    adc  edx, 0
    add  edi, 8
    loop .fill_tables

    ; PAE, root table, EFER.LME, then paging + protection on.
    mov  eax, cr4
    or   eax, (1 << 5)
    mov  cr4, eax

    mov  eax, 0x1000
    mov  cr3, eax

    mov  ecx, 0xC0000080               ; EFER MSR
    rdmsr
    or   eax, (1 << 8)
    wrmsr

    mov  eax, cr0
    or   eax, (1 << 31) | (1 << 0)
    mov  cr0, eax

    lgdt [gdt64_descriptor]
    jmp  0x08:.long_mode

.unsupported:
    cli
.unsupported_halt:
    hlt
    jmp  .unsupported_halt

[bits 64]
.long_mode:
    mov  ax, 0x10
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax

    mov  rsp, stack_top

    ; SysV ABI: rdi = magic, rsi = multiboot2 info pointer.
    mov  edi, eax
    mov  esi, ebx
    call arch_start

    cli
.hang:
    hlt
    jmp  .hang

section .data
align 8
gdt64:
    dq 0                               ; null descriptor
    dq 0x00AF9A000000FFFF              ; 0x08 kernel code
    dq 0x00AF92000000FFFF              ; 0x10 kernel data
gdt64_descriptor:
    dw $ - gdt64 - 1
    dq gdt64

section .bss
align 16
global stack_bottom
stack_bottom:
    resb 65536
global stack_top
stack_top:
