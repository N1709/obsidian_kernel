// SPDX-License-Identifier: GPL-2.0-only
#include "blkdev.h"
#if defined(__x86_64__) || defined(__i386__)

/*
 * ATA PIO driver (LBA48) for the legacy IDE controller.
 *
 * Covers QEMU's default -hda disk on the 'pc' machine plus most real
 * SATA controllers running in IDE-compatibility mode. Entirely poll
 * based, no interrupts, no DMA: bring-up stays deterministic. Data
 * moves sector-at-a-time through a single staging buffer.
 */

#include "../../include/io.h"
#include "../../include/printk.h"
#include "../../lib/string.h"

#define ATA_SECTOR_SIZE 512

/* Task-file registers off io_base; ATA_CTL is off ctl_base. */
#define ATA_DATA     0
#define ATA_SECCOUNT 2
#define ATA_LBA_LO   3
#define ATA_LBA_MID  4
#define ATA_LBA_HI   5
#define ATA_DRIVE    6
#define ATA_STATUS   7
#define ATA_COMMAND  7

#define AST_BSY 0x80u
#define AST_DF  0x20u
#define AST_DRQ 0x08u
#define AST_ERR 0x01u

#define AC_IDENTIFY  0xEC
#define AC_READ_EXT  0x24
#define AC_WRITE_EXT 0x34
#define AC_FLUSH     0xE7

#define MAX_DRIVES 4		/* two buses x master/slave */

struct ata_drive {
	u16  io_base;
	u16  ctl_base;
	bool slave;
	bool present;
	u64  sectors;
	char model[41];
};

static struct ata_drive drives[MAX_DRIVES];
static struct blk_device blkdevs[MAX_DRIVES];
static u8 staging[MAX_DRIVES][ATA_SECTOR_SIZE] __attribute__((aligned(16)));

static u8 status_of(struct ata_drive *d) {
	return inb(d->io_base + ATA_STATUS);
}

static void wait400ns(struct ata_drive *d) {
	for (int i = 0; i < 4; i++)
		inb(d->ctl_base);
}

static int wait_not_busy(struct ata_drive *d, u32 spins) {
	while (spins--) {
		if (!(status_of(d) & AST_BSY))
			return 0;
		io_wait();
	}
	return -1;
}

static int wait_ready_for_data(struct ata_drive *d, u32 spins) {
	while (spins--) {
		u8 st = status_of(d);

		if (st & AST_ERR)
			return -1;
		if ((st & AST_BSY) == 0 && (st & AST_DRQ))
			return 0;
	}
	return -1;
}

static void select_drive(struct ata_drive *d) {
	outb(d->io_base + ATA_DRIVE,
	     d->slave ? 0xF0u : 0xE0u);
	wait400ns(d);
}

static int issue_lba48(struct ata_drive *d, u64 lba, u32 count, u8 cmd) {
	if (!count || count > 65535)
		return -1;

	if (wait_not_busy(d, 1000000))
		return -1;

	select_drive(d);

	outb(d->io_base + ATA_SECCOUNT, (u8)(count >> 8));
	outb(d->io_base + ATA_LBA_LO,   (u8)(lba >> 24));
	outb(d->io_base + ATA_LBA_MID,  (u8)(lba >> 32));
	outb(d->io_base + ATA_LBA_HI,   (u8)(lba >> 40));
	outb(d->io_base + ATA_SECCOUNT, (u8)count);
	outb(d->io_base + ATA_LBA_LO,   (u8)lba);
	outb(d->io_base + ATA_LBA_MID,  (u8)(lba >> 8));
	outb(d->io_base + ATA_LBA_HI,   (u8)(lba >> 16));

	outb(d->io_base + ATA_COMMAND, cmd);
	return 0;
}

static bool identify(struct ata_drive *d) {
	select_drive(d);

	outb(d->io_base + ATA_SECCOUNT, 0);
	outb(d->io_base + ATA_LBA_LO, 0);
	outb(d->io_base + ATA_LBA_MID, 0);
	outb(d->io_base + ATA_LBA_HI, 0);
	outb(d->io_base + ATA_COMMAND, AC_IDENTIFY);

	u8 st = status_of(d);

	if (!st || st == 0xFF)
		return false;

	/* An ATAPI device puts signatures in LBA mid/high instead of DRQ */
	if (inb(d->io_base + ATA_LBA_MID) || inb(d->io_base + ATA_LBA_HI))
		return false;

	if (wait_ready_for_data(d, 5000000))
		return false;

	u16 id[256];

	for (int i = 0; i < 256; i++)
		id[i] = inw(d->io_base + ATA_DATA);

	if (!(id[83] & (1u << 10)))
		return false;		/* LBA48 required, keeps one code path */

	u64 n28 = ((u64)id[61] << 16) | id[60];
	u64 n48 = ((u64)id[103] << 48) | ((u64)id[102] << 32) |
		  ((u64)id[101] << 16) | (u64)id[100];

	d->sectors = n48 ? n48 : n28;
	if (!d->sectors)
		return false;

	int o = 0;

	for (int w = 27; w <= 46 && o < 40; w++) {
		d->model[o++] = (char)(id[w] >> 8);
		d->model[o++] = (char)(id[w] & 0xFF);
	}
	d->model[40] = 0;
	o = 39;
	while (o >= 0 && d->model[o] == ' ')
		d->model[o--] = 0;

	d->present = true;
	return true;
}

static int transfer_sector(struct ata_drive *d, int idx, u64 lba, bool write) {
	if (issue_lba48(d, lba, 1, write ? AC_WRITE_EXT : AC_READ_EXT))
		return -1;

	u8 *buf = staging[idx];

	for (int i = 0; i < ATA_SECTOR_SIZE; i += 2) {
		if (write) {
			outw(d->io_base + ATA_DATA,
			     (u16)buf[i] | ((u16)buf[i + 1] << 8));
		} else {
			u16 v = inw(d->io_base + ATA_DATA);

			buf[i] = (u8)v;
			buf[i + 1] = (u8)(v >> 8);
		}
	}

	if (wait_not_busy(d, 20000000))
		return -1;

	if (status_of(d) & (AST_ERR | AST_DF))
		return -1;

	if (write)
		outb(d->io_base + ATA_COMMAND, AC_FLUSH);

	return 0;
}

static int drv_read(int idx, u64 lba, u32 count, void *buf) {
	struct ata_drive *d = &drives[idx];

	if (!d->present)
		return -1;

	u8 *out = buf;

	for (u32 s = 0; s < count; s++) {
		if (transfer_sector(d, idx, lba + s, false))
			return -1;
		memcpy(out + s * ATA_SECTOR_SIZE, staging[idx],
		       ATA_SECTOR_SIZE);
	}
	return 0;
}

static int drv_write(int idx, u64 lba, u32 count, const void *buf) {
	struct ata_drive *d = &drives[idx];

	if (!d->present)
		return -1;

	const u8 *in = buf;

	for (u32 s = 0; s < count; s++) {
		memcpy(staging[idx], in + s * ATA_SECTOR_SIZE,
		       ATA_SECTOR_SIZE);
		if (transfer_sector(d, idx, lba + s, true))
			return -1;
	}
	return 0;
}

#define WRAP(n, idx) \
	static int wrap_read_##n(u64 lba, u32 c, void *b) { \
		return drv_read(idx, lba, c, b); \
	} \
	static int wrap_write_##n(u64 lba, u32 c, const void *b) { \
		return drv_write(idx, lba, c, b); \
	}

WRAP(0, 0)
WRAP(1, 1)
WRAP(2, 2)
WRAP(3, 3)

static int (*const readers[MAX_DRIVES])(u64, u32, void *) = {
	wrap_read_0, wrap_read_1, wrap_read_2, wrap_read_3
};
static int (*const writers[MAX_DRIVES])(u64, u32, const void *) = {
	wrap_write_0, wrap_write_1, wrap_write_2, wrap_write_3
};

static const struct { u16 io, ctl; const char *prefix; } buses[] = {
	{ 0x1F0, 0x3F6, "ata0" },
	{ 0x170, 0x376, "ata1" },
};

void ata_probe(void) {
	int idx = 0;

	for (size_t b = 0; b < sizeof(buses) / sizeof(buses[0]); b++) {
		for (int sl = 0; sl < 2 && idx < MAX_DRIVES; sl++, idx++) {
			struct ata_drive *d = &drives[idx];

			memset(d, 0, sizeof(*d));
			d->io_base = buses[b].io;
			d->ctl_base = buses[b].ctl;
			d->slave = sl != 0;

			/* Soft reset, then reselect; a floating bus reads 0xFF */
			outb(d->ctl_base, 0x06);
			wait400ns(d);
			outb(d->ctl_base, 0x02);
			wait400ns(d);
			select_drive(d);

			if (status_of(d) == 0xFF)
				continue;

			if (!identify(d))
				continue;

			char name[BLKDEV_NAME_LEN];
			int plen = k_strlen(buses[b].prefix);

			k_strcpy(name, buses[b].prefix);
			name[plen] = sl ? 's' : 'm';
			name[plen + 1] = 0;

			struct blk_device *bd = &blkdevs[idx];

			memset(bd, 0, sizeof(*bd));
			k_strcpy(bd->name, name);
			bd->present = true;
			bd->sector_size = ATA_SECTOR_SIZE;
			bd->sectors = d->sectors;
			bd->writable = true;
			bd->read_sectors = readers[idx];
			bd->write_sectors = writers[idx];

			if (!blk_register(bd)) {
				idx++;
				break;
			}

			kprintf("blk %s %s %llu MB\n",
				name,
				d->model[0] ? d->model : "disk",
				(unsigned long long)((d->sectors >> 11)));
		}
	}
}

#else /* !x86: keep the symbol stable across architectures */

void ata_probe(void) { }

#endif
