// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_GDT_H
#define OBSIDIAN_GDT_H

/* Installs a flat GDT: null descriptor, ring-0 code, ring-0 data.
   Protected mode already gives us a GDT from the bootloader, but we
   don't know its layout, so we replace it with one of our own. */
void gdt_init(void);

#endif
