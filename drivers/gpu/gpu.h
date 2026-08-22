// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_GPU_H
#define OBSIDIAN_GPU_H

#include "../../include/types.h"

/* Result of GPU bring-up: what was detected and which mode is live. */
struct gpu_device {
	const char *name;	/* driver-provided device name */
	u16 vendor;
	u16 device;
	bool fb_active;		/* true once a linear framebuffer mode is up */
	u32 lfb_phys;
	u32 width;
	u32 height;
	u32 bpp;
	u32 pitch;
};

void gpu_init(void);
const struct gpu_device *gpu_info(void);

#endif
