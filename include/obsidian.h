// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_KERNEL_H
#define OBSIDIAN_KERNEL_H

#include "types.h"

#define KERNEL_NAME "Obsidian"

#ifndef KERNEL_VERSION
#define KERNEL_VERSION "0.0.0-unknown"
#endif
#ifndef KERNEL_CODENAME
#define KERNEL_CODENAME ""
#endif
#ifndef KERNEL_BUILD_ARCH
#define KERNEL_BUILD_ARCH "unknown"
#endif
#ifndef KERNEL_BUILD_DATE
#define KERNEL_BUILD_DATE "unknown"
#endif

/* Build variants produced from the same source tree:
   - standard: full feature set for daily use of your OS
   - secure  : reduced safe-mode kernel used when the main one panics */
#ifdef OBSIDIAN_SECURE
#define KERNEL_VARIANT_NAME "secure"
#else
#define KERNEL_VARIANT_NAME "standard"
#endif

/* Boot parameters handed over by the architecture entry code. */
typedef struct {
	const char *arch_name;
	const char *cmdline;	/* "" when the bootloader gave none */
	u64         mem_usable_kb;
	u32         heap_base;	/* physical == linear in this kernel */
	u32         heap_size;
} boot_info_t;

extern char _kernel_end[];

void kernel_main(const boot_info_t *boot);
void kernel_reboot(void);
void kernel_halt(void);

#endif
