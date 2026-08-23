// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_BLKDEV_H
#define OBSIDIAN_BLKDEV_H

#include "../../include/types.h"

/*
 * Tiny block-device layer.
 *
 * A block device hands the kernel fixed-size logical sectors and is
 * addressed by linear sector number (LBA). Drivers register themselves
 * here; filesystems consume whatever shows up.
 */

#define BLKDEV_MAX      4
#define BLKDEV_NAME_LEN 24

struct blk_device {
	char name[BLKDEV_NAME_LEN];
	u64  sectors;		/* total logical sectors */
	u32  sector_size;	/* bytes per logical sector, 512 or 4096 */
	bool present;
	bool writable;

	/* Driver entry points. All return 0 on success, -1 on error.
	   buf must hold count * sector_size bytes. */
	int (*read_sectors)(u64 lba, u32 count, void *buf);
	int (*write_sectors)(u64 lba, u32 count, const void *buf);
};

void blkdev_init(void);

bool blk_register(const struct blk_device *dev);
int  blk_count(void);
struct blk_device *blk_get(int idx);
struct blk_device *blk_first(void);

int  blk_read(struct blk_device *d, u64 lba, u32 count, void *buf);
int  blk_write(struct blk_device *d, u64 lba, u32 count, const void *buf);

#endif
