// SPDX-License-Identifier: GPL-2.0-only
#include "ext4_int.h"
#include "ext4.h"
#include "../init/time.h"

/*
 * ext4 high level: extent-tree construction, truncation, directories,
 * mounting and the public file API. Writes go through a simple model:
 * replace = truncate-to-zero then append; append streams data through
 * freshly allocated blocks and serializes the resulting runs into an
 * extent tree (inline up to four entries, one leaf level beyond).
 */

struct run {
	u64 start;
	u32 first_block;
	u32 count;
};

static bool path_component_ok(const char *s, u32 len) {
	if (len == 0 || len > EXT4_NAME_MAX || s[0] == '/')
		return false;

	for (u32 i = 0; i < len; i++)
		if (s[i] == 0)
			return false;

	return true;
}

static void add_iblocks(struct ext4_inode *in, s64 delta_bytes) {
	s64 sectors = delta_bytes / 512;
	s64 cur = (s64)in->blocks_lo;

	if (e4.inode_size > 128)
		cur |= (s64)in->osd2.blocks_hi << 32;

	cur += sectors;
	if (cur < 0)
		cur = 0;

	in->blocks_lo = (u32)(u64)cur;
	if (e4.inode_size > 128)
		in->osd2.blocks_hi = (u16)((u64)cur >> 32);
}

/* ---------------- extent tree building ---------------- */

#define ROOT_MAX_ENTRIES 4

static u32 leaf_capacity(void) {
	u32 cap = (e4.block_size - 12 -
		   (e4.meta_csum ? 4 : 0)) / 12;

	return cap ? cap : 1;
}

static int write_leaf(u32 ino, const struct run *runs, u32 from, u32 n,
		      u64 *blk_out) {
	struct extent_entry {
		u32 first_block;
		u16 count;
		u16 start_hi;
		u32 start_lo;
	} __attribute__((packed));

	struct extent_header *eh;

	/* allocate first: allocation reuses the shared scratch buffer */
	if (e4_alloc_block_near(0, blk_out))
		return -1;

	eh = (struct extent_header *)e4_block_buf();

	memset(e4_block_buf(), 0, e4.block_size);
	eh->magic = 0xF30A;
	eh->depth = 0;
	eh->max = (u16)((e4.block_size - 12 -
			 (e4.meta_csum ? 4 : 0)) / 12);
	eh->entries = (u16)n;

	struct extent_entry *ex = (struct extent_entry *)
				  ((u8 *)eh + 12);

	for (u32 i = 0; i < n; i++) {
		ex[i].first_block = runs[from + i].first_block;
		ex[i].count = (u16)(runs[from + i].count == 32768 ?
				    32767 : runs[from + i].count);
		ex[i].start_lo = (u32)runs[from + i].start;
		ex[i].start_hi = (u16)(runs[from + i].start >> 32);
	}

	e4_extent_block_checksum_set(ino, eh);

	return e4_write_block(*blk_out, e4_block_buf());
}

static int build_tree(u32 ino, struct run *runs, u32 n,
		      struct ext4_inode *in) {
	struct extent_header *root =
		(struct extent_header *)&in->u.extent_hdr;

	if (n == 0) {
		root->magic = 0xF30A;
		root->entries = 0;
		root->max = ROOT_MAX_ENTRIES;
		root->depth = 0;
		root->generation = 0;
		return 0;
	}

	if (n <= ROOT_MAX_ENTRIES) {
		struct extent_entry {
			u32 first_block;
			u16 count;
			u16 start_hi;
			u32 start_lo;
		} __attribute__((packed));

		root->magic = 0xF30A;
		root->entries = (u16)n;
		root->max = ROOT_MAX_ENTRIES;
		root->depth = 0;
		root->generation = 0;

		struct extent_entry *ex = (struct extent_entry *)
					  ((u8 *)root + 12);

		for (u32 i = 0; i < n; i++) {
			ex[i].first_block = runs[i].first_block;
			ex[i].count = (u16)(runs[i].count == 32768 ?
					    32767 : runs[i].count);
			ex[i].start_lo = (u32)runs[i].start;
			ex[i].start_hi = (u16)(runs[i].start >> 32);
		}
		return 0;
	}

	u32 cap = leaf_capacity();
	u32 leaves = (n + cap - 1) / cap;

	if (leaves > ROOT_MAX_ENTRIES)
		return -1;	/* too fragmented for this driver */

	u64 leaf_blk[ROOT_MAX_ENTRIES];
	u32 written = 0;

	for (u32 l = 0; l < leaves; l++) {
		u32 here = n - written < cap ? n - written : cap;

		if (write_leaf(ino, runs, written, here, &leaf_blk[l]))
			return -1;
		written += here;
		add_iblocks(in, (s64)e4.block_size);
	}

	struct extent_idx *ix = (struct extent_idx *)((u8 *)root + 12);

	root->magic = 0xF30A;
	root->entries = (u16)leaves;
	root->max = ROOT_MAX_ENTRIES;
	root->depth = 1;
	root->generation = 0;

	written = 0;
	for (u32 l = 0; l < leaves; l++) {
		ix[l].first_block = runs[written].first_block;
		ix[l].leaf_lo = (u32)leaf_blk[l];
		ix[l].leaf_hi = (u16)(leaf_blk[l] >> 32);
		ix[l].unused = 0;
		written += (n - written < cap) ? n - written : cap;
	}
	return 0;
}

int e4_append_blocks(struct ext4_inode *in, u32 ino, u64 start_file_block,
		     u32 count, const u8 *data) {
	struct run *runs = kzalloc(sizeof(struct run) * (count + 1));
	u32 nruns = 0;
	u32 done = 0;
	int rc = -1;

	if (!runs)
		return -1;

	while (done < count) {
		u64 blk;

		if (e4_alloc_block_near((u32)((start_file_block + done) /
					      e4.blocks_per_group),
					&blk))
			goto out;

		if (data)
			memcpy(e4_block_buf2(), data + (u64)done *
			       e4.block_size, e4.block_size);
		else
			memset(e4_block_buf2(), 0, e4.block_size);

		if (e4_write_block(blk, e4_block_buf2()))
			goto out;

		if (nruns && runs[nruns - 1].start +
		    runs[nruns - 1].count == blk &&
		    runs[nruns - 1].first_block +
		    runs[nruns - 1].count ==
		    start_file_block + done) {
			runs[nruns - 1].count++;
		} else {
			runs[nruns].start = blk;
			runs[nruns].first_block =
				start_file_block + done;
			runs[nruns].count = 1;
			nruns++;
		}
		done++;
	}

	rc = build_tree(ino, runs, nruns, in);
	if (!rc)
		add_iblocks(in, (s64)count * e4.block_size);

out:
	kfree(runs);
	return rc;
}

/* ---------------- truncation ---------------- */

static void free_extent_tree(struct ext4_inode *in) {
	struct extent_header *root =
		(struct extent_header *)&in->u.extent_hdr;

	if (root->magic != 0xF30A || !root->entries)
		return;

	struct extent_entry {
		u32 first_block;
		u16 count;
		u16 start_hi;
		u32 start_lo;
	} __attribute__((packed));

	struct extent_entry *ex = (struct extent_entry *)
				  ((u8 *)root + 12);
	u64 freed_data = 0;

	if (root->depth == 0) {
		for (u16 i = 0; i < root->entries && i < 4; i++) {
			bool unwritten = ex[i].count >= 32768;
			u32 cnt = unwritten ?
				  ex[i].count - 32768 : ex[i].count;

			if (unwritten)
				continue;   /* no blocks reserved */

			u64 start = ex[i].start_lo |
				    ((u64)ex[i].start_hi << 32);

			for (u32 j = 0; j < cnt; j++)
				e4_free_block(start + j);
			freed_data += cnt;
		}
	}

	if (root->depth > 0) {
		struct extent_idx *ix = (struct extent_idx *)
					((u8 *)root + 12);

		for (u16 i = 0; i < root->entries && i < root->max;
		     i++) {
			u64 lb = ix[i].leaf_lo |
				 ((u64)ix[i].leaf_hi << 32);

			if (lb)
				e4_free_block(lb);
		}
		freed_data += root->entries;
	}

	add_iblocks(in, -(s64)freed_data * (s64)e4.block_size);
	root->entries = 0;
	root->depth = 0;
}

static void free_indirect_tree(u64 blk, int levels, u32 *freed) {
	u32 bpp = e4.block_size / 4;
	u32 *snapshot;

	if (!blk || levels < 0)
		return;

	if (levels == 0) {
		e4_free_block(blk);
		(*freed)++;
		return;
	}

	/* snapshot child pointers: recursion reuses scratch buffers */
	snapshot = kmalloc(e4.block_size);

	if (!snapshot)
		return;

	if (e4_read_block(blk, snapshot)) {
		kfree(snapshot);
		return;
	}

	for (u32 i = 0; i < bpp; i++) {
		u64 child = snapshot[i];

		free_indirect_tree(child, levels - 1, freed);
	}
	kfree(snapshot);
	e4_free_block(blk);
	(*freed)++;
}

int e4_truncate(struct ext4_inode *in, u32 ino, u64 new_size) {
	(void)ino;

	if (new_size != 0)
		return -1;

	if (in->flags & INO_FLAG_EXTENTS) {
		free_extent_tree(in);
	} else {
		u32 bpp = e4.block_size / 4;
		u32 freed = 0;

		for (int i = 11; i >= 0; i--) {
			if (in->u.block[i]) {
				e4_free_block(in->u.block[i]);
				freed++;
				in->u.block[i] = 0;
			}
		}
		for (int which = 12; which <= 14; which++) {
			int levels = which - 11;

			free_indirect_tree(in->u.block[which], levels,
					   &freed);
			in->u.block[which] = 0;
		}
		(void)bpp;
		add_iblocks(in, -(s64)freed * (s64)e4.block_size);
	}

	in->size_lo = 0;
	in->size_or_dir_acl = 0;
	return 0;
}

/* ---------------- directories ---------------- */

struct dir_scan_ctx {
	int (*cb)(const char *, u32, u8, void *);
	void *ctx;
	bool indexed;
};

static int scan_dirent_block(u32 dir_ino, u8 *blk, u32 block_index,
			     struct dir_scan_ctx *sc) {
	struct ext4_dirent *d = (struct ext4_dirent *)blk;
	char *top = (char *)blk + e4.block_size;

	while ((char *)d + 8 <= top) {
		u16 rl = d->rec_len;

		if (!rl || (rl & 3) || (char *)d + rl > top)
			break;		/* corrupt or dx node junk */

		if (rl == e4.block_size && d->name_len == 0)
			break;		/* htree fake entry */

		if (d->inode == 0) {
			d = (struct ext4_dirent *)((char *)d + rl);
			continue;
		}

		if (d->name_len == 0 ||
		    d->name_len > rl - 8 ||
		    d->inode > e4.inodes_count)
			break;

		char name[EXT4_NAME_MAX + 1];

		memcpy(name, d->name, d->name_len);
		name[d->name_len] = 0;

		int rc = sc->cb(name, d->inode, d->file_type, sc->ctx);

		if (rc)
			return rc;

		d = (struct ext4_dirent *)((char *)d + rl);
	}
	(void)dir_ino;
	(void)block_index;
	return 0;
}

struct lookup_ctx {
	const char *want;
	u32 found;
};

static int lookup_cb(const char *name, u32 ino, u8 type, void *ctx) {
	struct lookup_ctx *lc = ctx;

	(void)type;
	if (!k_strcmp(name, lc->want)) {
		lc->found = ino;
		return 1;
	}
	return 0;
}

int e4_iterate_dir(u32 dir_ino,
		   int (*cb)(const char *, u32, u8, void *), void *ctx) {
	struct ext4_inode din;

	if (e4_read_inode(dir_ino, &din))
		return -1;
	if (!e4_is_dir(&din))
		return -1;

	struct dir_scan_ctx sc = { cb, ctx,
				   !!(din.flags & INO_FLAG_INDEX_FL) };

	u64 size = e4_inode_size(&din);
	u64 nblocks = (size + e4.block_size - 1) / e4.block_size;

	for (u64 bi = 0; bi < nblocks; bi++) {
		u64 db;

		if (e4_bmap(&din, bi, &db))
			return -1;
		if (!db)
			continue;	/* hole */

		if (e4_read_block(db, e4_block_buf()))
			return -1;

		int rc = scan_dirent_block(dir_ino, e4_block_buf(),
					   (u32)bi, &sc);

		if (rc)
			return rc;
	}
	return 0;
}

int e4_dir_lookup(u32 dir_ino, const char *name, u32 *ino_out) {
	struct lookup_ctx lc = { name, 0 };

	if (e4_iterate_dir(dir_ino, lookup_cb, &lc))
		return -1;
	if (!lc.found)
		return -1;
	*ino_out = lc.found;
	return 0;
}

bool e4_dir_empty(u32 dir_ino) {
	struct ext4_inode din;

	if (e4_read_inode(dir_ino, &din))
		return false;

	u64 size = e4_inode_size(&din);
	u64 nblocks = (size + e4.block_size - 1) / e4.block_size;

	for (u64 bi = 0; bi < nblocks; bi++) {
		u64 db;

		if (e4_bmap(&din, bi, &db) || !db)
			continue;
		if (e4_read_block(db, e4_block_buf()))
			continue;

		struct ext4_dirent *d = (struct ext4_dirent *)
					e4_block_buf();
		char *top = (char *)d + e4.block_size;

		while ((char *)d + 8 <= top) {
			u16 rl = d->rec_len;

			if (!rl || (rl & 3) || (char *)d + rl > top)
				break;
			if (rl == e4.block_size && d->name_len == 0)
				break;
			if (d->inode &&
			    !(d->name_len == 1 && d->name[0] == '.') &&
			    !(d->name_len == 2 && d->name[0] == '.' &&
			      d->name[1] == '.'))
				return false;
			d = (struct ext4_dirent *)((char *)d + rl);
		}
	}
	return true;
}

int e4_dir_add_entry(u32 dir_ino, const char *name, u32 child_ino,
		     u8 file_type) {
	struct ext4_inode din;

	if (e4_read_inode(dir_ino, &din))
		return -1;

	if (din.flags & INO_FLAG_INDEX_FL) {
		kputs("ext4 refusing to modify indexed directory");
		return -1;
	}

	u32 nl = k_strlen(name);
	u32 need = (8 + nl + 3) & ~3u;

	if (need > e4.block_size - (e4.meta_csum ? 12 : 0))
		return -1;

	u64 size = e4_inode_size(&din);
	u64 nblocks = (size + e4.block_size - 1) / e4.block_size;

	/* pass 1: squeeze into an existing block */
	for (u64 bi = 0; bi < nblocks; bi++) {
		u64 db;

		if (e4_bmap(&din, bi, &db) || !db)
			continue;
		if (e4_read_block(db, e4_block_buf()))
			return -1;

		struct ext4_dirent *d = (struct ext4_dirent *)
					e4_block_buf();
		char *base = (char *)d;
		char *top = base + e4.block_size;
		u32 tail_room = e4.meta_csum ? 12 : 0;

		while ((char *)d + 8 <= top) {
			u16 rl = d->rec_len;

			if (!rl || (rl & 3) || (char *)d + rl > top)
				break;

			bool real = d->inode != 0 &&
				    !(rl == e4.block_size &&
				      d->name_len == 0);
			u32 minimal = real ?
				      ((8 + d->name_len + 3) & ~3u) : 0;

			if (rl - minimal >= need) {
				u16 keep = (u16)minimal;
				u16 rest = (u16)(rl - keep);

				d->rec_len = keep;

				/* new entry lives purely in the old
				   slack; nothing else moves */
				struct ext4_dirent *nd =
					(struct ext4_dirent *)
					((char *)d + keep);

				nd->rec_len = rest;
				nd->inode = child_ino;
				nd->name_len = (u8)nl;
				nd->file_type = file_type;
				memcpy(nd->name, name, nl);

				e4_dir_block_checksum_set(dir_ino,
							  base);
				return e4_write_block(db, base);
			}

			d = (struct ext4_dirent *)((char *)d + rl);
		}
	}

	/* pass 2: append a fresh directory block */
	if (!(din.flags & INO_FLAG_EXTENTS)) {
		kputs("ext4 legacy dir too small");
		return -1;
	}

	u64 newblk;
	u32 g = (dir_ino - 1) / e4.inodes_per_group;

	if (e4_alloc_block_near(g, &newblk))
		return -1;

	struct ext4_dirent *nd = (struct ext4_dirent *)e4_block_buf();

	memset(e4_block_buf(), 0, e4.block_size);

	u32 span = e4.block_size - (e4.meta_csum ? 12 : 0);

	nd->inode = child_ino;
	nd->rec_len = (u16)span;
	nd->name_len = (u8)nl;
	nd->file_type = file_type;
	memcpy(nd->name, name, nl);

	if (e4.meta_csum) {
		struct ext4_dirent *tl = (struct ext4_dirent *)
					 (e4_block_buf() + span);

		tl->inode = 0;
		tl->rec_len = 12;
		tl->name_len = DIRENT_TAIL_MARKER;
		tl->file_type = 0;
	}

	e4_dir_block_checksum_set(dir_ino, e4_block_buf());

	if (e4_write_block(newblk, e4_block_buf()))
		return -1;

	/* attach the block through the extent tree */
	struct extent_header *root =
		(struct extent_header *)&din.u.extent_hdr;
	struct extent_entry {
		u32 first_block;
		u16 count;
		u16 start_hi;
		u32 start_lo;
	} __attribute__((packed));

	if (root->magic != 0xF30A)
		return -1;

	if (root->depth == 0) {
		if (root->entries >= root->max) {
			kputs("ext4 dir extent table full");
			return -1;
		}
		struct extent_entry *ex = (struct extent_entry *)
					  ((u8 *)root + 12);

		ex[root->entries].first_block = (u32)nblocks;
		ex[root->entries].count = 1;
		ex[root->entries].start_lo = (u32)newblk;
		ex[root->entries].start_hi = (u16)(newblk >> 32);
		root->entries++;
	} else if (root->depth == 1) {
		struct extent_idx *ix = (struct extent_idx *)
					((u8 *)root + 12);

		u16 last = (u16)(root->entries - 1);
		u64 lb = ix[last].leaf_lo |
			 ((u64)ix[last].leaf_hi << 32);

		if (e4_read_block(lb, e4_block_buf2()))
			return -1;

		struct extent_header *lh =
			(struct extent_header *)e4_block_buf2();

		if (lh->entries >= lh->max) {
			kputs("ext4 dir leaf full");
			return -1;
		}
		struct extent_entry *ex = (struct extent_entry *)
					  ((u8 *)lh + 12);

		ex[lh->entries].first_block = (u32)nblocks;
		ex[lh->entries].count = 1;
		ex[lh->entries].start_lo = (u32)newblk;
		ex[lh->entries].start_hi = (u16)(newblk >> 32);
		lh->entries++;

		e4_extent_block_checksum_set(dir_ino, lh);
		if (e4_write_block(lb, e4_block_buf2()))
			return -1;
		(void)ix;
	} else {
		return -1;
	}

	din.size_lo += e4.block_size;
	add_iblocks(&din, e4.block_size);

	u64 now = e4_now();

	din.mtime = (u32)now;
	din.ctime = (u32)now;

	if (e4_write_inode(dir_ino, &din))
		return -1;

	return e4_sync_gd((newblk - e4.first_data_block) /
			  e4.blocks_per_group);
}

int e4_dir_remove_entry(u32 dir_ino, const char *name) {
	struct ext4_inode din;

	if (e4_read_inode(dir_ino, &din))
		return -1;

	u64 size = e4_inode_size(&din);
	u64 nblocks = (size + e4.block_size - 1) / e4.block_size;

	for (u64 bi = 0; bi < nblocks; bi++) {
		u64 db;

		if (e4_bmap(&din, bi, &db) || !db)
			continue;
		if (e4_read_block(db, e4_block_buf()))
			return -1;

		struct ext4_dirent *d = (struct ext4_dirent *)
					e4_block_buf();
		char *top = (char *)d + e4.block_size;

		while ((char *)d + 8 <= top) {
			u16 rl = d->rec_len;

			if (!rl || (rl & 3) || (char *)d + rl > top)
				break;

			if (d->inode && d->name_len ==
			    k_strlen(name) &&
			    !memcmp(d->name, name, d->name_len)) {
				d->inode = 0;	/* tombstone */

				e4_dir_block_checksum_set(dir_ino,
							  e4_block_buf());
				return e4_write_block(db,
						      e4_block_buf());
			}
			d = (struct ext4_dirent *)((char *)d + rl);
		}
	}
	return -1;
}
