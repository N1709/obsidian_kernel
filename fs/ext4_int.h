// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_EXT4_INT_H
#define OBSIDIAN_EXT4_INT_H

#include "ext4_disk.h"
#include "ext4_csum.h"
#include "../drivers/block/blkdev.h"
#include "../mm/heap.h"
#include "../lib/string.h"
#include "../include/printk.h"

/*
 * Mount state shared by the ext4 implementation files.
 */

struct ext4_fs {
	struct blk_device *dev;

	u32 block_size;
	u32 sectors_per_block;
	u64 blocks_count;
	u32 inodes_count;
	u32 blocks_per_group;
	u32 inodes_per_group;
	u32 group_count;
	u32 first_data_block;
	u32 inode_size;
	u32 desc_size;
	u32 first_ino;

	u32 feat_compat;
	u32 feat_incompat;
	u32 feat_ro_compat;

	bool has_extents;
	bool has_64bit;
	bool meta_csum;
	bool gdt_csum;
	bool csum_seed_feat;
	bool writable;

	u32 csum_seed;
	u8  uuid[16];

	struct ext4_superblock sb;
	struct ext4_group_desc *gdt;	/* cached group descriptors */
};

extern struct ext4_fs e4;

/* block IO in filesystem-block units */
int  e4_read_block(u64 block, void *buf);
int  e4_write_block(u64 block, const void *buf);
void *e4_block_buf(void);		/* shared scratch, block_size bytes */
void *e4_block_buf2(void);		/* second scratch */
int  e4_init_scratch(u32 block_size);

/* descriptors */
int  e4_load_gdt(void);
struct ext4_group_desc *e4_gd(u32 group);
int  e4_sync_gd(u32 group);

/* checksum plumbing */
u32  e4_meta_seed(void);
void e4_sb_recompute_checksum(void);
int  e4_sync_superblock(void);
void e4_desc_checksum_set(u32 group, struct ext4_group_desc *gd);
void e4_bitmap_checksum_set(u32 group, u8 *bitmap, bool inode_bitmap,
			    struct ext4_group_desc *gd);
void e4_inode_checksum_set(u32 ino, struct ext4_inode *inode);
void e4_dir_block_checksum_set(u32 dir_ino, void *blk);
void e4_extent_block_checksum_set(u32 ino, struct extent_header *eh);

/* inode layer */
int  e4_read_inode(u32 ino, struct ext4_inode *out);
int  e4_write_inode(u32 ino, const struct ext4_inode *in);
bool e4_is_dir(const struct ext4_inode *i);
u64  e4_inode_size(const struct ext4_inode *i);

/* mapping */
int  e4_bmap(const struct ext4_inode *in, u64 file_block, u64 *disk_block);
int  e4_append_blocks(struct ext4_inode *in, u32 ino, u64 start_file_block,
		      u32 count, const u8 *data);
int  e4_truncate(struct ext4_inode *in, u32 ino, u64 new_size);

/* allocation */
int  e4_alloc_block_near(u32 hint_group, u64 *out);
int  e4_free_block(u64 block);
int  e4_alloc_inode(u32 prefer_group, u32 mode, u32 *out);
int  e4_free_inode(u32 ino);
void e4_count_free(u32 *blocks, u32 *inodes);

/* directory layer */
int  e4_dir_lookup(u32 dir_ino, const char *name, u32 *ino_out);
int  e4_dir_add_entry(u32 dir_ino, const char *name, u32 child_ino,
		      u8 file_type);
int  e4_dir_remove_entry(u32 dir_ino, const char *name);
int  e4_iterate_dir(u32 dir_ino,
		    int (*cb)(const char *name, u32 ino, u8 type, void *ctx),
		    void *ctx);
bool e4_dir_empty(u32 dir_ino);

/* journal */
int  e4_journal_replay(void);
void e4_journal_reset(void);
bool e4_journal_pending(void);

/* misc */
u64  e4_now(void);
#define E4_LE16(x) ((u16)(x))
#define E4_LE32(x) ((u32)(x))

static inline u16 e4_get_le16(const void *p) {
	const u8 *b = p;

	return (u16)((u16)b[0] | ((u16)b[1] << 8));
}

static inline u32 e4_get_le32(const void *p) {
	const u8 *b = p;

	return (u32)b[0] | ((u32)b[1] << 8) |
	       ((u32)b[2] << 16) | ((u32)b[3] << 24);
}

static inline void e4_put_le32(void *p, u32 v) {
	u8 *b = p;

	b[0] = (u8)v;
	b[1] = (u8)(v >> 8);
	b[2] = (u8)(v >> 16);
	b[3] = (u8)(v >> 24);
}

#endif
