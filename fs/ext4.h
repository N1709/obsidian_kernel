// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_EXT4_H
#define OBSIDIAN_EXT4_H

#include "../include/types.h"

/*
 * ext4 filesystem driver - public surface.
 *
 * Mount the first block device that carries a valid ext2/3/4
 * superblock, then read and write files through flat paths rooted at
 * "/". Writes are journaled-free but metadata-consistent: bitmaps,
 * group descriptors, inode checksums and directory tails are all kept
 * correct, and any journal left over from a Linux crash is replayed
 * once at mount time.
 */

#define EXT4_MAX_PATH   128
#define EXT4_NAME_MAX   255

bool ext4_mount(void);
bool ext4_ready(void);
const char *ext4_devname(void);

/* Read up to max_len bytes; returns bytes read or negative error. */
int  ext4_read(const char *path, void *buf, u32 max_len);

/* Create or replace a file with len bytes; returns 0 or negative. */
int  ext4_write(const char *path, const void *data, u32 len);

int  ext4_unlink(const char *path);
u64  ext4_size(const char *path);

struct ext4_dirent_info {
	char name[EXT4_NAME_MAX + 1];
	u8   type;		/* DET_* from ext4_disk.h */
	u32  inode;
};

int  ext4_listdir(const char *path, struct ext4_dirent_info *out, int max);
u32  ext4_free_blocks(void);
u32  ext4_free_inodes(void);

#endif
