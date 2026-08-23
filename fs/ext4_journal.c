// SPDX-License-Identifier: GPL-2.0-only
#include "ext4_int.h"

/*
 * Minimal JBD2 crash recovery.
 *
 * Two forward passes over the journal file:
 *   1. collect every revoked block plus the set of committed
 *      transaction sequences,
 *   2. replay the data blocks of committed transactions, skipping any
 *      target that was revoked anywhere in the journal.
 *
 * Afterwards the journal is emptied (s_start = 0) and the RECOVER
 * feature flag is dropped from the main superblock. Checksummed
 * journals (CSUM_V2/V3) are accepted but their block checksums are
 * not verified; the tail CRCs of the main filesystem are what this
 * driver actively maintains.
 */

#define J_MAGIC        0xC03B3998u
#define JT_DESC        1
#define JT_REVOKE      2
#define JT_COMMIT      3

#define TAG_ESCAPE     1u
#define TAG_LAST       8u

#define JSB_OFF_BLOCKSIZE   12	/* little endian from here on */
#define JSB_OFF_MAXLEN      16
#define JSB_OFF_FIRST       20
#define JSB_OFF_START       24
#define JSB_OFF_SEQUENCE    28
#define JSB_OFF_FEAT_INCOMPAT 36

#define JBD2_FEAT_64BIT     0x00000002u

#define MAX_TRACKED_SEQS 256

static struct ext4_inode jin;
static u32 committed[MAX_TRACKED_SEQS];
static u32 n_committed;
static u32 last_commit_seq;
static u8 *revbits;

static u32 be32(const u8 *p) {
	return ((u32)p[0] << 24) | ((u32)p[1] << 16) |
	       ((u32)p[2] << 8) | (u32)p[3];
}

static void note_commit(u32 seq) {
	for (u32 i = 0; i < n_committed; i++)
		if (committed[i] == seq)
			return;

	if (n_committed < MAX_TRACKED_SEQS)
		committed[n_committed++] = seq;

	if (seq > last_commit_seq || !n_committed)
		last_commit_seq = seq;
}

static bool was_committed(u32 seq) {
	for (u32 i = 0; i < n_committed; i++)
		if (committed[i] == seq)
			return true;
	return false;
}

static int jread(u64 jb, u8 *buf) {
	u64 phys;

	if (e4_bmap(&jin, jb, &phys))
		return -1;

	if (!phys)
		return -1;

	return e4_read_block(phys, buf);
}

bool e4_journal_pending(void) {
	return e4.dev != NULL &&
	       (e4.feat_incompat & FEAT_INCOMPAT_RECOVER) != 0;
}

int e4_journal_replay(void) {
	u32 jsb_incompat, tagsz, applied = 0;
	u32 maxlen, first, start;
	u64 jblocks, cap;
	u8 *buf, *content;
	int rc = 0;

	n_committed = 0;
	last_commit_seq = 0;
	revbits = NULL;

	if (!e4.dev || !e4.sb.journal_inum)
		return 0;

	if (!(e4.feat_incompat & FEAT_INCOMPAT_RECOVER))
		return 0;	/* clean shutdown */

	kputs("jbd2: journal needs recovery");

	if (e4_read_inode(e4.sb.journal_inum, &jin)) {
		kputs("jbd2: cannot read journal inode");
		return -1;
	}

	if (!(jin.flags & INO_FLAG_EXTENTS)) {
		kputs("jbd2: legacy journal inode unsupported");
		return -1;
	}

	jblocks = e4_inode_size(&jin) / e4.block_size;

	buf = kmalloc(e4.block_size);
	content = kmalloc(e4.block_size);

	if (!buf || !content) {
		kfree(buf);
		kfree(content);
		return -1;
	}

	if (jread(0, buf)) {
		kputs("jbd2: unreadable journal superblock");
		rc = -1;
		goto out;
	}

	{
		u32 bs = e4_get_le32(buf + JSB_OFF_BLOCKSIZE);

		maxlen = e4_get_le32(buf + JSB_OFF_MAXLEN);
		first = e4_get_le32(buf + JSB_OFF_FIRST);
		start = e4_get_le32(buf + JSB_OFF_START);
		jsb_incompat =
			e4_get_le32(buf + JSB_OFF_FEAT_INCOMPAT);

		if (bs != e4.block_size || first < 2 ||
		    first >= jblocks || start >= jblocks) {
			kputs("jbd2: bogus journal superblock");
			rc = -1;
			goto out;
		}
	}

	tagsz = (jsb_incompat & JBD2_FEAT_64BIT) ? 16 : 12;

	cap = (maxlen && maxlen <= jblocks) ? maxlen : jblocks;

	revbits = kzalloc((e4.blocks_count + 7) / 8);

	if (!revbits) {
		rc = -1;
		goto out;
	}

	/* pass 1: commits + revokes */
	for (u64 i = first; i < cap; i++) {
		if (jread(i, buf))
			continue;

		if (be32(buf) != J_MAGIC)
			continue;

		u32 type = be32(buf + 4);

		if (type == JT_COMMIT) {
			note_commit(be32(buf + 8));
		} else if (type == JT_REVOKE) {
			u32 used = e4_get_le32(buf + 12);

			if (used > e4.block_size)
				continue;

			for (u32 off = 16; off + 4 <= used; off += 4) {
				u64 b = be32(buf + off);

				if (b < e4.blocks_count)
					revbits[b >> 3] |=
						(u8)(1u << (b & 7));
			}
		}
	}

	if (!n_committed) {
		kputs("jbd2: no complete transactions");
		goto done;
	}

	/* pass 2: apply */
	for (u64 i = first; i < cap; i++) {
		u32 type, seq, off;
		u64 c;

		if (jread(i, buf))
			continue;

		if (be32(buf) != J_MAGIC)
			continue;

		type = be32(buf + 4);
		seq = be32(buf + 8);

		if (type != JT_DESC || !was_committed(seq))
			continue;

		c = i + 1;
		off = 12;

		while (off + tagsz <= e4.block_size && c < cap) {
			u64 target = be32(buf + off);
			u32 flags = be32(buf + off + 4);

			if (flags & TAG_LAST)
				break;

			if (!target)
				break;

			if (jread(c, content))
				break;

			if (flags & TAG_ESCAPE) {
				content[0] = (u8)(J_MAGIC >> 24);
				content[1] = (u8)(J_MAGIC >> 16);
				content[2] = (u8)(J_MAGIC >> 8);
				content[3] = (u8)J_MAGIC;
			}

			if (!(revbits[target >> 3] &
			      (1u << (target & 7)))) {
				if (e4_write_block(target, content))
					break;
				applied++;
			}

			c++;
			off += tagsz;
		}

		i = c - 1;	/* contents were consumed */
	}

	kprintf("jbd2: replayed %u blocks\n", applied);

done:
	e4_journal_reset();

out:
	kfree(revbits);
	kfree(buf);
	kfree(content);
	revbits = NULL;
	return rc;
}

void e4_journal_reset(void) {
	u8 *buf;
	u64 phys;

	if (!e4.dev || !e4.sb.journal_inum)
		return;

	buf = kmalloc(e4.block_size);

	if (!buf)
		return;

	if (!e4_read_inode(e4.sb.journal_inum, &jin) &&
	    !e4_bmap(&jin, 0, &phys) && phys &&
	    !e4_read_block(phys, buf)) {
		e4_put_le32(buf + JSB_OFF_START, 0);
		e4_put_le32(buf + JSB_OFF_SEQUENCE,
			    last_commit_seq + 1);
		e4_write_block(phys, buf);
	}

	kfree(buf);

	e4.feat_incompat &= ~FEAT_INCOMPAT_RECOVER;
	e4.sb.feature_incompat &= ~FEAT_INCOMPAT_RECOVER;
	e4_sb_recompute_checksum();
	e4_sync_superblock();

	kputs("jbd2: journal emptied");
}
