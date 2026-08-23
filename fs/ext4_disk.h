// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_EXT4_DISK_H
#define OBSIDIAN_EXT4_DISK_H

#include "../include/types.h"

/*
 * On-disk layout constants for ext2/3/4. All fields are little endian;
 * every architecture this kernel runs on is little endian, so packed
 * structs map the disk directly.
 */

#define EXT4_SUPER_OFF     1024u	/* superblock byte offset in partition */
#define EXT4_MAGIC         0xEF53u
#define EXT4_VALID_FS      0x0001u

/* feature bits */
#define FEAT_COMPAT_HAS_JOURNAL 0x00000004u
#define FEAT_COMPAT_DIR_INDEX   0x00000020u

#define FEAT_INCOMPAT_FILETYPE   0x00000002u
#define FEAT_INCOMPAT_RECOVER    0x00000004u
#define FEAT_INCOMPAT_JOURNAL_DEV 0x00000008u
#define FEAT_INCOMPAT_META_BG    0x00000010u
#define FEAT_INCOMPAT_EXTENTS    0x00000040u
#define FEAT_INCOMPAT_64BIT      0x00000080u
#define FEAT_INCOMPAT_MMP        0x00000100u
#define FEAT_INCOMPAT_FLEX_BG    0x00000200u
#define FEAT_INCOMPAT_EA_INODE   0x00000400u
#define FEAT_INCOMPAT_DIRDATA    0x00001000u
#define FEAT_INCOMPAT_CSUM_SEED  0x00002000u
#define FEAT_INCOMPAT_LARGEDIR   0x00004000u
#define FEAT_INCOMPAT_INLINE_DATA 0x00008000u
#define FEAT_INCOMPAT_ENCRYPT    0x00010000u

#define FEAT_RO_SPARSE_SUPER  0x00000001u
#define FEAT_RO_LARGE_FILE    0x00000002u
#define FEAT_RO_GDT_CSUM      0x00000010u
#define FEAT_RO_DIR_NLINK     0x00000020u
#define FEAT_RO_EXTRA_ISIZE   0x00000040u
#define FEAT_RO_BIGALLOC      0x00000200u
#define FEAT_RO_METADATA_CSUM 0x00000400u
#define FEAT_RO_QUOTA         0x00000100u
#define FEAT_RO_PROJECT       0x00002000u
#define FEAT_RO_VERITY        0x00008000u
#define FEAT_RO_ORPHAN_PRESENT 0x00010000u

/* inode flags */
#define INO_FLAG_EXTENTS  0x00080000u
#define INO_FLAG_INDEX_FL 0x00001000u

#define EXT4_ROOT_INO       2u
#define EXT4_GOOD_OLD_INODE_SIZE 128u
#define EXT4_GOOD_OLD_FIRST_INO  11u

/* file types in dirent (FILETYPE feature) */
#define DET_UNKNOWN 0u
#define DET_REG     1u
#define DET_DIR     2u
#define DET_CHR     3u
#define DET_BLK     4u
#define DET_FIFO    5u
#define DET_SOCK    6u
#define DET_LNK     7u

#pragma pack(push, 1)

struct ext4_superblock {
	u32 inodes_count;		/* 0x00 */
	u32 blocks_count_lo;		/* 0x04 */
	u32 r_blocks_count_lo;		/* 0x08 */
	u32 free_blocks_count_lo;	/* 0x0C */
	u32 free_inodes_count;		/* 0x10 */
	u32 first_data_block;		/* 0x14 */
	u32 log_block_size;		/* 0x18: bs = 1024 << n */
	u32 log_cluster_size;		/* 0x1C */
	u32 blocks_per_group;		/* 0x20 */
	u32 clusters_per_group;		/* 0x24 */
	u32 inodes_per_group;		/* 0x28 */
	u32 mtime;			/* 0x2C */
	u32 wtime;			/* 0x30 */
	u16 mnt_count;			/* 0x34 */
	u16 max_mnt_count;		/* 0x36 */
	u16 magic;			/* 0x38 */
	u16 state;			/* 0x3A */
	u16 errors;			/* 0x3C */
	u16 minor_rev_level;		/* 0x3E */
	u32 lastcheck;			/* 0x40 */
	u32 checkinterval;		/* 0x44 */
	u32 creator_os;			/* 0x48 */
	u32 rev_level;			/* 0x4C */
	u16 def_resuid;			/* 0x50 */
	u16 def_resgid;			/* 0x52 */
	u32 first_ino;			/* 0x54 */
	u16 inode_size;			/* 0x58 */
	u16 block_group_nr;		/* 0x5A */
	u32 feature_compat;		/* 0x5C */
	u32 feature_incompat;		/* 0x60 */
	u32 feature_ro_compat;		/* 0x64 */
	u8  uuid[16];			/* 0x68 */
	char volume_name[16];		/* 0x78 */
	char last_mounted[64];		/* 0x88 */
	u32 algorithm_usage_bitmap;	/* 0xC8 */
	u8  prealloc_blocks;		/* 0xCC */
	u8  prealloc_dir_blocks;	/* 0xCD */
	u16 reserved_gdt_blocks;	/* 0xCE */
	u8  journal_uuid[16];		/* 0xD0 */
	u32 journal_inum;		/* 0xE0 */
	u32 journal_dev;		/* 0xE4 */
	u32 last_orphan;		/* 0xE8 */
	u32 hash_seed[4];		/* 0xEC */
	u8  def_hash_version;		/* 0xFC */
	u8  jnl_backup_type;		/* 0xFD */
	u16 desc_size;			/* 0xFE */
	u32 default_mount_opts;		/* 0x100 */
	u32 first_meta_bg;		/* 0x104 */
	u32 mkfs_time;			/* 0x108 */
	u32 jnl_blocks[17];		/* 0x10C */
	u32 blocks_count_hi;		/* 0x150 */
	u32 r_blocks_count_hi;		/* 0x154 */
	u32 free_blocks_count_hi;	/* 0x158 */
	u16 min_extra_isize;		/* 0x15C */
	u16 want_extra_isize;		/* 0x15E */
	u32 flags;			/* 0x160 */
	u16 raid_stride;		/* 0x164 */
	u16 mmp_interval;		/* 0x166 */
	u64 mmp_block;			/* 0x168 */
	u32 raid_stripe_width;		/* 0x170 */
	u8  log_groups_per_flex;	/* 0x174 */
	u8  checksum_type;		/* 0x175 */
	u16 reserved_pad;		/* 0x176 */
	u64 kbytes_written;		/* 0x178 */
	u32 snapshot_inum;		/* 0x180 */
	u32 snapshot_id;		/* 0x184 */
	u64 snapshot_list;		/* 0x188 */
	u32 error_count;		/* 0x190 */
	u32 first_error_time;		/* 0x194 */
	u32 first_error_ino;		/* 0x198 */
	u64 first_error_block;		/* 0x19C */
	u8  first_error_func[32];	/* 0x1A4 */
	u32 first_error_line;		/* 0x1C4 */
	u32 last_error_time;		/* 0x1C8 */
	u32 last_error_ino;		/* 0x1CC */
	u32 last_error_line;		/* 0x1D0 */
	u64 last_error_block;		/* 0x1D4 */
	u8  last_error_func[32];	/* 0x1DC */
	u8  mount_opts[64];		/* 0x1FC */
	u32 usr_quota_inum;		/* 0x23C */
	u32 grp_quota_inum;		/* 0x240 */
	u32 overhead_blocks;		/* 0x244 */
	u32 backup_bgs[2];		/* 0x248 sparse_super2 */
	u8  encrypt_algos[4];		/* 0x250 */
	u8  encrypt_pw_salt[16];	/* 0x254 */
	u32 lpf_ino;			/* 0x264 */
	u32 prj_quota_inum;		/* 0x268 */
	u32 checksum_seed;		/* 0x26C */
	u32 wtime_hi;			/* 0x270 */
	u32 mtime_hi;			/* 0x274 */
	u32 mkfs_time_hi;		/* 0x278 */
	u32 lastcheck_hi;		/* 0x27C */
	u32 first_error_time_hi;	/* 0x280 */
	u32 last_error_time_hi;		/* 0x284 */
	u8  encrypt_algos_dsc[4];	/* 0x288 */
	/* pad to 1020, then s_checksum at 0x3FC */
	u8  _pad[1020 - 0x28C];
	u32 checksum;			/* 0x3FC */
};

struct ext4_group_desc {
	u32 block_bitmap_lo;		/* 0 */
	u32 inode_bitmap_lo;		/* 4 */
	u32 inode_table_lo;		/* 8 */
	u16 free_blocks_count_lo;	/* 12 */
	u16 free_inodes_count_lo;	/* 14 */
	u16 used_dirs_count_lo;		/* 16 */
	u16 flags;			/* 18 */
	u32 exclude_bitmap_lo;		/* 20 */
	u16 block_bitmap_csum_lo;	/* 24 */
	u16 inode_bitmap_csum_lo;	/* 26 */
	u16 itable_unused_lo;		/* 28 */
	u16 checksum;			/* 30 */
	u32 block_bitmap_hi;		/* 32 */
	u32 inode_bitmap_hi;		/* 36 */
	u32 inode_table_hi;		/* 40 */
	u16 free_blocks_count_hi;	/* 44 */
	u16 free_inodes_count_hi;	/* 46 */
	u16 used_dirs_count_hi;		/* 48 */
	u16 itable_unused_hi;		/* 50 */
	u32 exclude_bitmap_hi;		/* 52 */
	u16 block_bitmap_csum_hi;	/* 56 */
	u16 inode_bitmap_csum_hi;	/* 58 */
	u32 reserved;			/* 60 */
};

struct ext4_inode {
	u16 mode;			/* 0x00 */
	u16 uid;			/* 0x02 */
	u32 size_lo;			/* 0x04 */
	u32 atime;			/* 0x08 */
	u32 ctime;			/* 0x0C */
	u32 mtime;			/* 0x10 */
	u32 dtime;			/* 0x14 */
	u16 gid;			/* 0x18 */
	u16 links_count;		/* 0x1A */
	u32 blocks_lo;			/* 0x1C, in 512-byte sectors */
	u32 flags;			/* 0x20 */
	union {				/* 0x24 */
		u32 version;
		struct {
			u16 l_isize;	/* unused */
			u16 l_nblk_high;
		} linux_old;
	} osd1;
	union {				/* 0x28: 15 x u32 */
		u32 block[15];
		struct {
			u16 magic;	/* 0xF30A when extents */
			u16 entries;
			u16 max;
			u16 depth;
			u32 generation;
			struct {
				u32 first_block;
				u16 count;
				u16 start_hi;
				u32 start_lo;
			} __attribute__((packed)) ext[4];
		} __attribute__((packed)) extent_hdr;
	} u;
	u32 generation;			/* 0x64 */
	u32 file_acl;			/* 0x68 */
	u32 size_or_dir_acl;		/* 0x6C */
	u32 faddr;			/* 0x70 */
	struct {			/* 0x74 osd2 linux */
		u16 blocks_hi;
		u16 file_acl_hi;
		u16 uid_high;
		u16 gid_high;
		u16 checksum_lo;
		u16 reserved2;
	} __attribute__((packed)) osd2;
	u16 extra_isize;		/* 0x80 */
	u16 checksum_hi;		/* 0x82 */
	u32 ctime_extra;		/* 0x84 */
	u32 mtime_extra;		/* 0x88 */
	u32 atime_extra;		/* 0x8C */
	u32 crtime;			/* 0x90 */
	u32 crtime_extra;		/* 0x94 */
	u32 version_hi;			/* 0x98 */
	u32 projid;			/* 0x9C */
	u8  rest[96];			/* 0xA0..0xFF */
};

struct ext4_dirent {
	u32 inode;
	u16 rec_len;
	u8  name_len;
	u8  file_type;
	char name[];
};

struct extent_header {
	u16 magic;	/* 0xF30A */
	u16 entries;
	u16 max;
	u16 depth;
	u32 generation;
};

struct extent_idx {
	u32 first_block;
	u32 leaf_lo;
	u16 leaf_hi;
	u16 unused;
};

struct extent_tail {
	u32 checksum;
};

#pragma pack(pop)

/* dirent tail marker lives in name_len of the fake entry */
#define DIRENT_TAIL_MARKER 0xDEu

#endif
