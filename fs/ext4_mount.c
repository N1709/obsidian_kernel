// SPDX-License-Identifier: GPL-2.0-only
#include "ext4_int.h"
#include "ext4.h"

/*
 * Mounting and the public file surface. Paths are flat and absolute,
 * rooted at inode 2. Writes replace files wholesale: truncate to zero,
 * stream fresh blocks through the extent allocator, rewrite metadata.
 */

#define SUPPORTED_INCOMPAT ( \
	FEAT_INCOMPAT_FILETYPE   | \
	FEAT_INCOMPAT_RECOVER    | \
	FEAT_INCOMPAT_EXTENTS    | \
	FEAT_INCOMPAT_64BIT      | \
	FEAT_INCOMPAT_FLEX_BG    | \
	FEAT_INCOMPAT_CSUM_SEED)

/* ---------------- path resolution ---------------- */

static bool comp_ok(const char *s, u32 len) {
	return len > 0 && len <= EXT4_NAME_MAX && s[0] != '/';
}

static int resolve_full(const char *path, u32 *ino_out) {
	if (!path || path[0] != '/' ||
	    k_strlen(path) >= EXT4_MAX_PATH)
		return -1;

	u32 cur = EXT4_ROOT_INO;
	const char *p = path + 1;

	while (*p) {
		while (*p == '/')
			p++;
		if (!*p)
			break;

		const char *q = p;

		while (*q && *q != '/')
			q++;

		u32 len = (u32)(q - p);

		if (!comp_ok(p, len))
			return -1;

		char name[EXT4_NAME_MAX + 1];

		memcpy(name, p, len);
		name[len] = 0;

		struct ext4_inode ci;

		if (e4_read_inode(cur, &ci) || !e4_is_dir(&ci))
			return -1;

		if (e4_dir_lookup(cur, name, &cur))
			return -1;

		p = *q ? q + 1 : q;
	}

	*ino_out = cur;
	return 0;
}

static int resolve_parent(const char *path, char *name_out,
			  u32 *parent_out) {
	char buf[EXT4_MAX_PATH];

	if (!path || path[0] != '/')
		return -1;

	u32 plen = k_strlen(path);

	if (plen >= EXT4_MAX_PATH)
		return -1;

	memcpy(buf, path, plen);
	buf[plen] = 0;

	while (plen > 1 && buf[plen - 1] == '/') {
		buf[--plen] = 0;
	}

	s32 cut = -1;

	for (s32 i = (s32)plen - 1; i >= 0; i--) {
		if (buf[i] == '/') {
			cut = i;
			break;
		}
	}

	const char *dirp;
	u32 dirlen;
	const char *nm;
	u32 nmlen;

	if (cut < 0) {
		dirp = "/";
		dirlen = 1;
		nm = buf;
		nmlen = plen;
	} else {
		dirp = buf;
		dirlen = cut == 0 ? 1 : (u32)cut;
		nm = buf + cut + 1;
		nmlen = plen - (u32)cut - 1;
	}

	if (!comp_ok(nm, nmlen))
		return -1;

	char dpath[EXT4_MAX_PATH];

	if (dirlen >= EXT4_MAX_PATH)
		return -1;

	memcpy(dpath, dirp, dirlen);
	dpath[dirlen] = 0;

	struct ext4_inode pi;

	if (resolve_full(dpath, parent_out))
		return -1;
	if (e4_read_inode(*parent_out, &pi) || !e4_is_dir(&pi))
		return -1;

	memcpy(name_out, nm, nmlen);
	name_out[nmlen] = 0;
	return 0;
}

/* ---------------- mount ---------------- */

bool ext4_mount(void) {
	if (e4.dev)
		return true;

	memset(&e4, 0, sizeof e4);

	struct blk_device *dev = blk_first();

	if (!dev) {
		kputs("ext4 no block device present");
		return false;
	}

	u8 *img = kzalloc(1024);

	if (!img)
		return false;

	int rc = blk_read(dev, 2, 2, img);

	if (rc) {
		kfree(img);
		kputs("ext4 cannot read superblock");
		return false;
	}

	memcpy(&e4.sb, img, sizeof e4.sb);
	kfree(img);

	if (e4.sb.magic != EXT4_MAGIC)
		return false;

	u32 log = e4.sb.log_block_size;

	if (log > 3) {
		kprintf("ext4 block size %u not supported\n",
			1024u << log);
		e4.dev = NULL;
		return false;
	}

	u32 inc = e4.sb.feature_incompat;
	u32 bad = inc & ~SUPPORTED_INCOMPAT;

	if (bad) {
		kprintf("ext4 unsupported features 0x%x\n", bad);
		e4.dev = NULL;
		return false;
	}

	if (e4.sb.feature_ro_compat & FEAT_RO_BIGALLOC) {
		kputs("ext4 bigalloc unsupported");
		e4.dev = NULL;
		return false;
	}

	e4.block_size = 1024u << log;
	e4.sectors_per_block = e4.block_size / 512;
	e4.blocks_count = e4.sb.blocks_count_lo;

	if (inc & FEAT_INCOMPAT_64BIT)
		e4.blocks_count |=
			(u64)e4.sb.blocks_count_hi << 32;

	e4.inodes_count = e4.sb.inodes_count;
	e4.blocks_per_group = e4.sb.blocks_per_group;
	e4.inodes_per_group = e4.sb.inodes_per_group;
	e4.first_data_block = e4.sb.first_data_block;
	e4.inode_size = e4.sb.inode_size;
	e4.first_ino = e4.sb.first_ino ?
		       e4.sb.first_ino : EXT4_GOOD_OLD_FIRST_INO;
	e4.feat_compat = e4.sb.feature_compat;
	e4.feat_incompat = inc;
	e4.feat_ro_compat = e4.sb.feature_ro_compat;
	e4.has_extents = (inc & FEAT_INCOMPAT_EXTENTS) != 0;
	e4.has_64bit = (inc & FEAT_INCOMPAT_64BIT) != 0;
	e4.meta_csum =
		(e4.sb.feature_ro_compat & FEAT_RO_METADATA_CSUM) != 0;
	e4.gdt_csum =
		(e4.sb.feature_ro_compat & FEAT_RO_GDT_CSUM) != 0;
	e4.csum_seed_feat = (inc & FEAT_INCOMPAT_CSUM_SEED) != 0;
	memcpy(e4.uuid, e4.sb.uuid, 16);

	e4.desc_size = 32;

	if (e4.has_64bit && e4.sb.desc_size)
		e4.desc_size = e4.sb.desc_size;

	if (e4.desc_size < 32 || e4.desc_size > 128 ||
	    (e4.desc_size & 3)) {
		kputs("ext4 bogus group descriptor size");
		e4.dev = NULL;
		return false;
	}

	u64 rem = e4.blocks_count - e4.first_data_block;

	e4.group_count =
		(u32)((rem + e4.blocks_per_group - 1) /
		      e4.blocks_per_group);

	if (!e4.group_count || e4.group_count > 65536) {
		kputs("ext4 bogus group count");
		e4.dev = NULL;
		return false;
	}

	if (e4_init_scratch(e4.block_size)) {
		kputs("ext4 out of memory for buffers");
		e4.dev = NULL;
		return false;
	}

	e4.gdt = kzalloc(e4.group_count * e4.desc_size);

	if (!e4.gdt || e4_load_gdt()) {
		kputs("ext4 cannot load group descriptors");
		e4.dev = NULL;
		return false;
	}

	if ((inc & FEAT_INCOMPAT_RECOVER) && e4_journal_pending())
		e4_journal_replay();

	e4.sb.state |= EXT4_VALID_FS;
	e4.sb.mnt_count++;
	e4_sb_recompute_checksum();

	if (e4_sync_superblock()) {
		kputs("ext4 superblock write failed");
		e4.dev = NULL;
		return false;
	}

	kprintf("ext4 rw mounted on %s: %u MiB, %u groups, bs %u\n",
		dev->name,
		(u32)(e4.blocks_count / ((1u << 20) / e4.block_size)),
		e4.group_count, e4.block_size);

	return true;
}

bool ext4_ready(void) {
	return e4.dev != NULL;
}

const char *ext4_devname(void) {
	return e4.dev ? e4.dev->name : "none";
}

/* ---------------- read ---------------- */

int ext4_read(const char *path, void *user_buf, u32 max_len) {
	if (!e4.dev || !user_buf)
		return -1;

	u32 ino;

	if (resolve_full(path, &ino))
		return -1;

	struct ext4_inode in;

	if (e4_read_inode(ino, &in) || e4_is_dir(&in))
		return -1;

	u64 size = e4_inode_size(&in);
	u32 want = size < (u64)max_len ? (u32)size : max_len;
	u8 *out = user_buf;
	u32 done = 0;

	while (done < want) {
		u64 lb = (u64)done / e4.block_size;
		u32 boff = (u32)((u64)done % e4.block_size);
		u32 chunk = e4.block_size - boff;

		if (chunk > want - done)
			chunk = want - done;

		u64 db;

		if (e4_bmap(&in, lb, &db))
			return -1;

		if (!db) {
			memset(out + done, 0, chunk);
		} else {
			/* bmap has finished with its scratch use */
			if (e4_read_block(db, e4_block_buf()))
				return -1;
			memcpy(out + done,
			       (u8 *)e4_block_buf() + boff, chunk);
		}
		done += chunk;
	}

	return (int)done;
}

/* ---------------- write ---------------- */

static int do_replace(u32 ino, struct ext4_inode *in,
		      const u8 *data, u32 len) {
	if (!(in->flags & INO_FLAG_EXTENTS)) {
		kputs("ext4 legacy inode layout unwritable");
		return -1;
	}

	if (e4_truncate(in, ino, 0))
		return -1;

	u32 nblocks = (len + e4.block_size - 1) / e4.block_size;

	if (nblocks &&
	    e4_append_blocks(in, ino, 0, nblocks, len ? data : NULL))
		return -1;

	u64 now = e4_now();

	in->size_lo = len;
	in->size_or_dir_acl = 0;
	in->mtime = (u32)now;
	in->ctime = (u32)now;
	in->atime = (u32)now;

	return e4_write_inode(ino, in);
}

#define EXT4_WRITE_CAP ((u32)16 << 20)

int ext4_write(const char *path, const void *data, u32 len) {
	if (!e4.dev)
		return -1;

	if (len > EXT4_WRITE_CAP) {
		kputs("ext4 write too large");
		return -1;
	}

	char name[EXT4_NAME_MAX + 1];
	u32 parent;

	if (resolve_parent(path, name, &parent))
		return -1;

	u32 existing = 0;

	if (!e4_dir_lookup(parent, name, &existing)) {
		struct ext4_inode ex;

		if (e4_read_inode(existing, &ex))
			return -1;

		if (e4_is_dir(&ex)) {
			kputs("ext4 target is a directory");
			return -1;
		}

		if (do_replace(existing, &ex, data, len))
			return -1;

		e4_sb_recompute_checksum();
		e4_sync_superblock();
		return 0;
	}

	/* create a fresh regular file */
	struct ext4_inode pin;

	if (e4_read_inode(parent, &pin))
		return -1;

	u32 pref = (parent - 1) / e4.inodes_per_group;
	u32 newino;

	if (e4_alloc_inode(pref, 0x81A4, &newino))
		return -1;

	struct ext4_inode nin;

	memset(&nin, 0, sizeof nin);

	u64 now = e4_now();

	nin.mode = 0x81A4;
	nin.uid = 0;
	nin.gid = 0;
	nin.links_count = 1;
	nin.flags = INO_FLAG_EXTENTS;
	nin.atime = (u32)now;
	nin.ctime = (u32)now;
	nin.mtime = (u32)now;
	nin.crtime = (u32)now;
	nin.generation = (u32)(now ^ (newino * 2654435761u));

	if (e4.inode_size > 128) {
		u32 room = e4.inode_size - 128;

		nin.extra_isize = (u16)(room > 32 ? 32 : room);
	}

	if (do_replace(newino, &nin, data, len)) {
		e4_free_inode(newino);
		e4_sync_gd(pref);
		return -1;
	}

	if (e4_dir_add_entry(parent, name, newino, DET_REG)) {
		e4_truncate(&nin, newino, 0);
		e4_free_inode(newino);
		e4_sync_gd(pref);
		return -1;
	}

	e4_sb_recompute_checksum();
	e4_sync_superblock();
	return 0;
}

/* ---------------- unlink / misc ---------------- */

int ext4_unlink(const char *path) {
	if (!e4.dev)
		return -1;

	char name[EXT4_NAME_MAX + 1];
	u32 parent;

	if (resolve_parent(path, name, &parent))
		return -1;

	u32 victim;

	if (e4_dir_lookup(parent, name, &victim))
		return -1;

	if (victim == EXT4_ROOT_INO)
		return -1;

	struct ext4_inode v;

	if (e4_read_inode(victim, &v))
		return -1;

	if (e4_is_dir(&v) && !e4_dir_empty(victim)) {
		kputs("ext4 directory not empty");
		return -1;
	}

	if (e4_dir_remove_entry(parent, name))
		return -1;

	e4_truncate(&v, victim, 0);
	e4_free_inode(victim);
	e4_sync_gd((victim - 1) / e4.inodes_per_group);
	e4_sb_recompute_checksum();
	e4_sync_superblock();
	return 0;
}

u64 ext4_size(const char *path) {
	if (!e4.dev)
		return 0;

	u32 ino;
	struct ext4_inode in;

	if (resolve_full(path, &ino) || e4_read_inode(ino, &in))
		return 0;

	return e4_inode_size(&in);
}

struct list_ctx {
	struct ext4_dirent_info *out;
	int max;
	int n;
};

static int list_cb(const char *nm, u32 ino, u8 type, void *ctx) {
	struct list_ctx *c = ctx;

	if (c->n >= c->max)
		return 1;

	struct ext4_dirent_info *e = &c->out[c->n++];
	u32 l = k_strlen(nm);

	memcpy(e->name, nm, l + 1);
	e->type = type;
	e->inode = ino;
	return 0;
}

int ext4_listdir(const char *path, struct ext4_dirent_info *out,
		 int max) {
	if (!e4.dev || !out || max <= 0)
		return -1;

	u32 d;

	if (resolve_full(path, &d))
		return -1;

	struct list_ctx c = { out, max, 0 };

	if (e4_iterate_dir(d, list_cb, &c))
		return -1;

	return c.n;
}

u32 ext4_free_blocks(void) {
	u32 b = 0, i = 0;

	e4_count_free(&b, &i);
	return b;
}

u32 ext4_free_inodes(void) {
	u32 b = 0, i = 0;

	e4_count_free(&b, &i);
	return i;
}
