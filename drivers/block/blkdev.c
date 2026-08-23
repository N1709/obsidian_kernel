// SPDX-License-Identifier: GPL-2.0-only
#include "blkdev.h"
#include "../../lib/string.h"
#include "../../include/printk.h"

/*
 * Registry for block devices. Drivers fill in a blk_device and hand it
 * over; ordering is registration order, device 0 is "the disk".
 */

static struct blk_device devs[BLKDEV_MAX];
static int ndevs;

void blkdev_init(void) {
	ndevs = 0;
	memset(devs, 0, sizeof(devs));
}

bool blk_register(const struct blk_device *dev) {
	if (!dev || !dev->present || !dev->read_sectors)
		return false;

	if (ndevs >= BLKDEV_MAX || !dev->sector_size)
		return false;

	devs[ndevs] = *dev;
	k_strncpy(devs[ndevs].name, dev->name, BLKDEV_NAME_LEN - 1);
	ndevs++;
	return true;
}

int blk_count(void) {
	return ndevs;
}

struct blk_device *blk_get(int idx) {
	if (idx < 0 || idx >= ndevs)
		return NULL;
	return &devs[idx];
}

struct blk_device *blk_first(void) {
	return ndevs ? &devs[0] : NULL;
}

int blk_read(struct blk_device *d, u64 lba, u32 count, void *buf) {
	if (!d || !count || !buf)
		return -1;

	if (lba + count > d->sectors && d->sectors)
		return -1;

	return d->read_sectors(lba, count, buf);
}

int blk_write(struct blk_device *d, u64 lba, u32 count, const void *buf) {
	if (!d || !count || !buf || !d->write_sectors || !d->writable)
		return -1;

	if (lba + count > d->sectors && d->sectors)
		return -1;

	return d->write_sectors(lba, count, buf);
}
