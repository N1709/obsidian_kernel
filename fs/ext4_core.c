// SPDX-License-Identifier: GPL-2.0-only
#include "ext4_int.h"
#include "../init/time.h"

/*
 * ext4 driver core: mount, block IO, checksums, inodes, block maps,
 * allocation and directories. Read side handles extent trees of any
 * depth plus legacy indirect maps; write side produces extent trees
 * up to depth one and keeps every metadata checksum current so images
 * remain valid under Linux/e2fsck.
 *
 * Every architecture this kernel supports is little endian, matching
 * the on-disk byte order, so packed structures are used directly.
 */

struct ext4_fs e4;

static u8 *scratch1, *scratch2;
static bool mounted;

u64 e4_now(void) {
	return uptime_ms() / 1000u;
}

void *e4_block_buf(void) {
	return scratch1;
}

void *e4_block_buf2(void) {
	return scratch2;
}

int e4_init_scratch(u32 block_size) {
	scratch1 = kmalloc(block_size);
	scratch2 = kmalloc(block_size);

	if (!scratch1 || !scratch2)
		return -1;

	memset(scratch1, 0, block_size);
	memset(scratch2, 0, block_size);
	return 0;
}

/* ---------------- raw block IO ---------------- */

int e4_read_block(u64 block, void *buf) {
	if (!e4.dev || !buf)
		return -1;

	return blk_read(e4.dev,
			block * e4.sectors_per_block,
			e4.sectors_per_block,
			buf);
}

int e4_write_block(u64 block, const void *buf) {
	if (!e4.dev || !e4.writable || !buf)
		return -1;

	return blk_write(e4.dev,
			 block * e4.sectors_per_block,
			 e4.sectors_per_block,
			 buf);
}

/* ---------------- checksum plumbing ---------------- */

u32 e4_meta_seed(void) {
	if (e4.csum_seed_feat)
		return e4_get_le32(&e4.sb.checksum_seed);
	return e4.csum_seed;
}

void e4_sb_recompute_checksum(void) {
	e4.sb.checksum = ext4_crc32c(~0u, &e4.sb, 1020);
}

int e4_sync_superblock(void) {
	u8 buf[1024];

	memcpy(buf, &e4.sb, 1024);
	return blk_write(e4.dev, EXT4_SUPER_OFF / 512, 2, buf);
}

void e4_desc_checksum_set(u32 group, struct ext4_group_desc *gd) {
	u32 g_le = group;

	if (e4.meta_csum) {
		u32 crc;

		gd->checksum = 0;
		crc = ext4_crc32c(e4_meta_seed(), &g_le, 4);
		crc = ext4_crc32c(crc, gd, e4.desc_size);
		gd->checksum = (u16)(crc & 0xFFFFu);
	} else if (e4.gdt_csum) {
		u16 crc;
		size_t off = 30;

		crc = ext4_crc16((u16)~0u, e4.uuid, 16);
		crc = ext4_crc16(crc, &g_le, 4);
		crc = ext4_crc16(crc, gd, (u32)off);
		off += 2;
		crc = ext4_crc16(crc, ((const u8 *)gd) + off,
				 e4.desc_size - off);
		gd->checksum = crc;
	}
}

void e4_bitmap_checksum_set(u32 group, u8 *bitmap, bool inode_bitmap,
			    struct ext4_group_desc *gd) {
	if (!e4.meta_csum)
		return;

	u32 sz = inode_bitmap ?
		 e4.inodes_per_group / 8 :
		 e4.blocks_per_group / 8;
	u32 crc = ext4_crc32c(e4_meta_seed(), bitmap, sz);

	if (inode_bitmap) {
		gd->inode_bitmap_csum_lo = (u16)(crc & 0xFFFFu);
		if (e4.desc_size >= 60)
			gd->inode_bitmap_csum_hi = (u16)(crc >> 16);
	} else {
		gd->block_bitmap_csum_lo = (u16)(crc & 0xFFFFu);
		if (e4.desc_size >= 58)
			gd->block_bitmap_csum_hi = (u16)(crc >> 16);
	}
}

static bool inode_has_csum_hi(const struct ext4_inode *in) {
	return e4.inode_size > EXT4_GOOD_OLD_INODE_SIZE &&
	       in->extra_isize >= 6;
}

void e4_inode_checksum_set(u32 ino, struct ext4_inode *inode) {
	if (!e4.meta_csum)
		return;

	bool has_hi = inode_has_csum_hi(inode);
	u16 saved_lo = inode->osd2.checksum_lo;
	u16 saved_hi = inode->checksum_hi;
	u32 crc;

	inode->osd2.checksum_lo = 0;
	inode->checksum_hi = 0;

	u32 ino_le = ino;

	crc = ext4_crc32c(e4_meta_seed(), &ino_le, 4);
	crc = ext4_crc32c(crc, &inode->generation, 4);
	crc = ext4_crc32c(crc, inode, e4.inode_size);

	inode->osd2.checksum_lo = (u16)(crc & 0xFFFFu);
	if (has_hi) {
		inode->checksum_hi = (u16)(crc >> 16);
	} else {
		inode->checksum_hi = saved_hi;
		(void)saved_lo;
	}
}

void e4_extent_block_checksum_set(u32 ino, struct extent_header *eh) {
	if (!e4.meta_csum)
		return;

	struct ext4_inode host;

	if (e4_read_inode(ino, &host))
		return;

	struct extent_tail *t = (struct extent_tail *)
		((char *)eh + 12 + (u32)eh->max * 12);
	u32 crc;
	u32 ino_le = ino;

	crc = ext4_crc32c(e4_meta_seed(), &ino_le, 4);
	crc = ext4_crc32c(crc, &host.generation, 4);
	crc = ext4_crc32c(crc, eh, 12 + (u32)eh->max * 12);
	t->checksum = crc;
}

void e4_dir_block_checksum_set(u32 dir_ino, void *blk) {
	if (!e4.meta_csum)
		return;

	struct ext4_inode host;

	if (e4_read_inode(dir_ino, &host))
		return;

	char *base = blk;
	char *top = base + e4.block_size;

	/* find the tail entry */
	struct ext4_dirent *d = (struct ext4_dirent *)base;

	while ((char *)d < top) {
		u16 rl = d->rec_len;

		if (rl < 8 || (rl & 3))
			return;
		d = (struct ext4_dirent *)((char *)d + rl);
	}
	if ((char *)d != top)
		return;

	struct ext4_dirent *t = (struct ext4_dirent *)(top - 12);

	if (t->inode != 0 || t->name_len != DIRENT_TAIL_MARKER ||
	    t->rec_len != 12)
		return;

	u32 crc;
	u32 ino_le = dir_ino;

	crc = ext4_crc32c(e4_meta_seed(), &ino_le, 4);
	crc = ext4_crc32c(crc, &host.generation, 4);
	crc = ext4_crc32c(crc, base, e4.block_size - 12);

	u32 *slot = (u32 *)(top - 4);

	*slot = crc;
}

/* ---------------- group descriptors ---------------- */

int e4_load_gdt(void) {
	u32 bytes = e4.desc_size * e4.group_count;
	u32 gdt_block = e4.first_data_block + 1;

	e4.gdt = kzalloc(bytes);
	if (!e4.gdt)
		return -1;

	u32 per_block = e4.block_size / e4.desc_size;
	u8 *tmp = scratch1;

	for (u32 g = 0; g < e4.group_count; g++) {
		u32 blk = gdt_block + g / per_block;
		u32 slot = g % per_block;

		if (e4_read_block(blk, tmp))
			return -1;
		memcpy(((u8 *)e4.gdt) + (u64)g * e4.desc_size,
		       tmp + (u64)slot * e4.desc_size, e4.desc_size);
	}

	/* verify checksums of group 0 as a sanity probe */
	struct ext4_group_desc *gd0 =
		(struct ext4_group_desc *)((u8 *)e4.gdt);
	struct ext4_group_desc probe;

	memcpy(&probe, gd0, sizeof(probe));
	u16 stored = probe.checksum;

	e4_desc_checksum_set(0, &probe);
	if (stored && probe.checksum != stored)
		kputs("ext4 warn group 0 checksum mismatch");

	return 0;
}

struct ext4_group_desc *e4_gd(u32 group) {
	if (group >= e4.group_count)
		return NULL;
	return (struct ext4_group_desc *)
		(((u8 *)e4.gdt) + (u64)group * e4.desc_size);
}

int e4_sync_gd(u32 group) {
	struct ext4_group_desc *gd = e4_gd(group);

	if (!gd)
		return -1;

	e4_desc_checksum_set(group, gd);

	u32 per_block = e4.block_size / e4.desc_size;
	u32 gdt_block = e4.first_data_block + 1;
	u32 blk = gdt_block + group / per_block;
	u32 slot = group % per_block;

	if (e4_read_block(blk, scratch2))
		return -1;
	memcpy(scratch2 + (u64)slot * e4.desc_size, gd, e4.desc_size);
	return e4_write_block(blk, scratch2);
}

/* ---------------- inodes ---------------- */

#define INO_TABLE_BLOCK(in) \
	e4_get_le32(&e4_gd((in) / e4.inodes_per_group)->inode_table_lo)

static u64 inode_table_block(u32 ino) {
	u32 g = (ino - 1) / e4.inodes_per_group;
	struct ext4_group_desc *gd = e4_gd(g);

	if (!gd)
		return 0;

	u64 tbl = gd->inode_table_lo;

	if (e4.has_64bit)
		tbl |= (u64)gd->inode_table_hi << 32;
	return tbl;
}

int e4_read_inode(u32 ino, struct ext4_inode *out) {
	if (ino < 1 || ino > e4.inodes_count)
		return -1;

	u64 tbl = inode_table_block(ino);
	u32 idx = (ino - 1) % e4.inodes_per_group;
	u64 byte_off = (u64)idx * e4.inode_size;
	u64 blk = tbl + byte_off / e4.block_size;
	u32 off = (u32)(byte_off % e4.block_size);

	if (off + e4.inode_size <= e4.block_size) {
		if (e4_read_block(blk, scratch1))
			return -1;
		memcpy(out, scratch1 + off, e4.inode_size);
	} else {
		if (e4_read_block(blk, scratch1))
			return -1;
		if (e4_read_block(blk + 1, scratch2))
			return -1;
		u32 part = e4.block_size - off;

		memcpy(out, scratch1 + off, part);
		memcpy(((u8 *)out) + part, scratch2,
		       e4.inode_size - part);
	}
	return 0;
}

int e4_write_inode(u32 ino, const struct ext4_inode *in) {
	if (ino < 1 || ino > e4.inodes_count || !e4.writable)
		return -1;

	u64 tbl = inode_table_block(ino);
	u32 idx = (ino - 1) % e4.inodes_per_group;
	u64 byte_off = (u64)idx * e4.inode_size;
	u64 blk = tbl + byte_off / e4.block_size;
	u32 off = (u32)(byte_off % e4.block_size);

	if (off + e4.inode_size <= e4.block_size) {
		if (e4_read_block(blk, scratch1))
			return -1;
		memcpy(scratch1 + off, in, e4.inode_size);
		return e4_write_block(blk, scratch1);
	}

	u32 part = e4.block_size - off;

	if (e4_read_block(blk, scratch1))
		return -1;
	memcpy(scratch1 + off, in, part);
	if (e4_write_block(blk, scratch1))
		return -1;
	if (e4_read_block(blk + 1, scratch1))
		return -1;
	memcpy(scratch1, ((const u8 *)in) + part, e4.inode_size - part);
	return e4_write_block(blk + 1, scratch1);
}

bool e4_is_dir(const struct ext4_inode *i) {
	return (i->mode & 0xF000u) == 0x4000u;
}

bool e4_is_regular(const struct ext4_inode *i) {
	return (i->mode & 0xF000u) == 0x8000u;
}

u64 e4_inode_size(const struct ext4_inode *i) {
	u64 sz = i->size_lo;

	if (e4_is_dir(i))
		return sz;
	if (e4.feat_ro_compat & FEAT_RO_LARGE_FILE)
		sz |= (u64)i->size_or_dir_acl << 32;
	return sz;
}

/* ---------------- extent / indirect mapping (read) ---------------- */

static int extent_lookup(const struct ext4_inode *in, u64 logical,
			 u64 *phys_out) {
	struct extent_entry {
		u32 first_block;
		u16 count;
		u16 start_hi;
		u32 start_lo;
	} __attribute__((packed));

	const struct extent_header *root =
		(const struct extent_header *)&in->u.extent_hdr;

	if (root->magic != 0xF30A)
		return -1;

	int depth = root->depth;
	u64 node_block = 0;

	for (;;) {
		u8 *node;

		if (depth == 0) {
			node = (u8 *)&in->u.extent_hdr;
		} else {
			if (e4_read_block(node_block, scratch1))
				return -1;
			node = scratch1;
		}

		struct extent_header *h = (struct extent_header *)node;
		struct extent_idx *ix = (struct extent_idx *)(node + 12);
		struct extent_entry *ex = (struct extent_entry *)(node + 12);

		if (depth == 0) {
			for (u16 i = 0; i < h->entries; i++) {
				bool unwritten = ex[i].count >= 32768;
				u32 cnt = unwritten ?
					  ex[i].count - 32768 : ex[i].count;

				if (logical >= ex[i].first_block &&
				    logical < ex[i].first_block + cnt) {
					if (unwritten) {
						*phys_out = 0;
						return 0;
					}
					u64 start = ex[i].start_lo |
						    ((u64)ex[i].start_hi << 32);

					*phys_out = start + logical -
						    ex[i].first_block;
					return 0;
				}
			}
			*phys_out = 0;	/* hole */
			return 0;
		}

		u16 chosen = 0;
		bool found = false;

		for (u16 i = 0; i < h->entries; i++) {
			if (logical >= ix[i].first_block) {
				chosen = i;
				found = true;
			}
		}
		if (!found)
			return -1;

		node_block = ix[chosen].leaf_lo |
			     ((u64)ix[chosen].leaf_hi << 32);
		depth--;
	}
}

static int indirect_lookup(const struct ext4_inode *in, u64 logical,
			   u64 *phys_out) {
	u32 bpp = e4.block_size / 4;
	u32 direct_max = 12;
	u32 single_max = direct_max + bpp;
	u32 double_max = single_max + bpp * bpp;

	if (logical < direct_max) {
		*phys_out = in->u.block[logical];
		return 0;
	}

	u32 which;
	u64 remaining = logical;

	if (logical < single_max) {
		which = 12;
		remaining -= direct_max;
	} else if (logical < double_max) {
		which = 13;
		remaining -= single_max;
	} else {
		which = 14;
		remaining -= double_max;
	}

	u64 ind = in->u.block[which];

	for (int level = which - 11; level >= 0; level--) {
		if (!ind) {
			*phys_out = 0;
			return 0;
		}
		if (e4_read_block(ind, scratch1))
			return -1;

		u32 stride = 1;

		for (int k = 0; k < level; k++)
			stride *= bpp;

		u32 slot = (u32)((remaining / stride) % bpp);

		ind = e4_get_le32(scratch1 + slot * 4);
	}
	*phys_out = ind;
	return 0;
}

int e4_bmap(const struct ext4_inode *in, u64 file_block, u64 *disk_block) {
	if (in->flags & INO_FLAG_EXTENTS)
		return extent_lookup(in, file_block, disk_block);
	return indirect_lookup(in, file_block, disk_block);
}

/* ---------------- allocation ---------------- */

static u64 blk_of(u32 group, u32 index) {
	return e4.first_data_block +
	       (u64)group * e4.blocks_per_group + index;
}

static int load_bitmap(bool inode_map, u32 group, u8 **bitmap_out,
		       struct ext4_group_desc **gd_out, u64 *blk_out) {
	struct ext4_group_desc *gd = e4_gd(group);

	if (!gd)
		return -1;

	u64 blk = inode_map ? gd->inode_bitmap_lo : gd->block_bitmap_lo;

	if (e4.has_64bit) {
		if (inode_map)
			blk |= (u64)gd->inode_bitmap_hi << 32;
		else
			blk |= (u64)gd->block_bitmap_hi << 32;
	}

	if (e4_read_block(blk, scratch1))
		return -1;

	*bitmap_out = scratch1;
	*gd_out = gd;
	*blk_out = blk;
	return 0;
}

static void adjust_free_blocks(struct ext4_group_desc *gd, s32 delta) {
	s32 v = (s32)gd->free_blocks_count_lo + delta;

	if (v < 0)
		v = 0;
	gd->free_blocks_count_lo = (u16)v;
	if (e4.has_64bit && e4.desc_size >= 44)
		gd->free_blocks_count_hi = (u16)(((u32)v >> 16) & 0xFFFFu);

	s64 total = (s64)e4.sb.free_blocks_count_lo +
		    (((s64)e4.sb.free_blocks_count_hi) << 32) + delta;

	if (total < 0)
		total = 0;
	e4.sb.free_blocks_count_lo = (u32)total;
	e4.sb.free_blocks_count_hi = (u32)((u64)total >> 32);
}

static void adjust_free_inodes(struct ext4_group_desc *gd, s32 delta) {
	s32 v = (s32)gd->free_inodes_count_lo + delta;

	if (v < 0)
		v = 0;
	gd->free_inodes_count_lo = (u16)v;
	if (e4.has_64bit && e4.desc_size >= 46)
		gd->free_inodes_count_hi = (u16)(((u32)v >> 16) & 0xFFFFu);

	s32 total = (s32)e4.sb.free_inodes_count + delta;

	if (total < 0)
		total = 0;
	e4.sb.free_inodes_count = (u32)total;
}

int e4_alloc_block_near(u32 hint_group, u64 *out) {
	if (!e4.writable)
		return -1;

	for (u32 gi = 0; gi < e4.group_count; gi++) {
		u32 g = (hint_group + gi) % e4.group_count;
		struct ext4_group_desc *gd;
		u8 *bm;
		u64 blk;

		if (load_bitmap(false, g, &bm, &gd, &blk))
			continue;

		u32 bits = e4.blocks_per_group;
		u32 last_bits = bits;

		if (g == e4.group_count - 1) {
			u64 rem = e4.blocks_count -
				  e4.first_data_block -
				  (u64)g * e4.blocks_per_group;

			last_bits = (u32)rem;
		}

		for (u32 byte_i = 0; byte_i < (last_bits + 7) / 8; byte_i++) {
			u8 v = bm[byte_i];

			if (v == 0xFF)
				continue;
			for (int bit = 0; bit < 8; bit++) {
				u32 bidx = byte_i * 8 + (u32)bit;

				if (bidx >= last_bits)
					break;
				if (!(v & (1u << bit))) {
					bm[byte_i] |= (u8)(1u << bit);
					e4_bitmap_checksum_set(g, bm, false, gd);
					adjust_free_blocks(gd, -1);
					e4_sync_gd(g);
					e4_write_block(blk, bm);
					memset(scratch2, 0, e4.block_size);
					e4_write_block(blk_of(g, bidx),
						       scratch2);
					*out = blk_of(g, bidx);
					return 0;
				}
			}
		}
	}
	return -1;
}

int e4_free_block(u64 block) {
	if (!e4.writable || block < e4.first_data_block ||
	    block >= e4.blocks_count)
		return -1;

	u32 g = (u32)((block - e4.first_data_block) /
		      e4.blocks_per_group);
	u32 idx = (u32)((block - e4.first_data_block) %
			e4.blocks_per_group);
	struct ext4_group_desc *gd;
	u8 *bm;
	u64 blk;

	if (load_bitmap(false, g, &bm, &gd, &blk))
		return -1;

	if (!(bm[idx >> 3] & (1u << (idx & 7)))) {
		kputs("ext4 freeing free block");
		return -1;
	}

	bm[idx >> 3] &= (u8)~(1u << (idx & 7));
	e4_bitmap_checksum_set(g, bm, false, gd);
	adjust_free_blocks(gd, 1);
	e4_sync_gd(g);
	return e4_write_block(blk, bm);
}

int e4_alloc_inode(u32 prefer_group, u32 mode, u32 *out) {
	if (!e4.writable)
		return -1;

	for (u32 gi = 0; gi < e4.group_count; gi++) {
		u32 g = (prefer_group + gi) % e4.group_count;
		struct ext4_group_desc *gd;
		u8 *bm;
		u64 blk;

		if (load_bitmap(true, g, &bm, &gd, &blk))
			continue;

		bool uninit = (gd->flags & 0x1) != 0;

		if (uninit) {
			/* bitmap never initialized: everything below the
			   reserved range is free; materialize it now */
			memset(bm, 0, e4.block_size);
			if (g == 0) {
				for (u32 i = 0; i < e4.first_ino - 1 &&
				     i < e4.inodes_per_group; i++)
					bm[i >> 3] |=
						(u8)(1u << (i & 7));
			}
			gd->flags &= (u16)~0x1u;
			gd->itable_unused_lo = 0;
			if (e4.has_64bit && e4.desc_size >= 50)
				gd->itable_unused_hi = 0;
		}

		for (u32 i = e4.first_ino - 1; i < e4.inodes_per_group; i++) {
			if (!(bm[i >> 3] & (1u << (i & 7)))) {
				u32 ino = g * e4.inodes_per_group + i + 1;

				if (ino < e4.first_ino ||
				    ino > e4.inodes_count)
					continue;

				bm[i >> 3] |= (u8)(1u << (i & 7));
				e4_bitmap_checksum_set(g, bm, true, gd);
				adjust_free_inodes(gd, -1);
				if (mode & 0x4000u) {
					u16 d = gd->used_dirs_count_lo + 1;

					gd->used_dirs_count_lo = d;
					if (e4.has_64bit && e4.desc_size >= 48)
						gd->used_dirs_count_hi =
							(u16)(((u32)d >> 16));
				}
				e4_sync_gd(g);
				e4_write_block(blk, bm);
				*out = ino;
				return 0;
			}
		}
	}
	return -1;
}

int e4_free_inode(u32 ino) {
	if (!e4.writable || ino < e4.first_ino ||
	    ino > e4.inodes_count)
		return -1;

	u32 g = (ino - 1) / e4.inodes_per_group;
	u32 idx = (ino - 1) % e4.inodes_per_group;
	struct ext4_group_desc *gd;
	u8 *bm;
	u64 blk;

	if (load_bitmap(true, g, &bm, &gd, &blk))
		return -1;

	if (!(bm[idx >> 3] & (1u << (idx & 7))))
		return -1;

	bm[idx >> 3] &= (u8)~(1u << (idx & 7));
	e4_bitmap_checksum_set(g, bm, true, gd);
	adjust_free_inodes(gd, 1);
	e4_sync_gd(g);
	return e4_write_block(blk, bm);
}

void e4_count_free(u32 *blocks, u32 *inodes) {
	if (blocks)
		*blocks = e4.sb.free_blocks_count_lo;
	if (inodes)
		*inodes = e4.sb.free_inodes_count;
}
