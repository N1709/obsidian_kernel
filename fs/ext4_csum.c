// SPDX-License-Identifier: GPL-2.0-only
#include "ext4_csum.h"

/*
 * CRC32C (Castagnoli, reflected polynomial 0x82F63B78) and the CRC16
 * used by legacy group descriptor checksums.
 *
 * Both follow the exact calling convention of e2fsprogs and Linux:
 * the incoming value is the raw running register state, callers start
 * from ~0 (or a stored seed) and chain previous outputs back in. No
 * extra complements happen here, so results are bit-for-bit identical
 * to ext2fs_crc32c_le()/ext2fs_crc16().
 */

static u32 crc32c_table[256];
static u16 crc16_table[256];
static bool tables_ready;

static void build_tables(void) {
	for (u32 i = 0; i < 256; i++) {
		u32 c = i;

		for (int k = 0; k < 8; k++)
			c = (c & 1) ? (c >> 1) ^ 0x82F63B78u : c >> 1;
		crc32c_table[i] = c;

		u16 r = (u16)(i << 8);

		for (int k = 0; k < 8; k++)
			r = (r & 0x8000) ?
			    (u16)((r << 1) ^ 0x1021) : (u16)(r << 1);
		crc16_table[i] = r;
	}
	tables_ready = true;
}

u32 ext4_crc32c(u32 crc, const void *buf, u32 len) {
	const u8 *p = buf;

	if (!tables_ready)
		build_tables();

	while (len--) {
		u32 b = *p++;

		crc = (crc >> 8) ^ crc32c_table[(crc ^ b) & 0xFFu];
	}
	return crc;
}

u16 ext4_crc16(u16 crc, const void *buf, u32 len) {
	const u8 *p = buf;

	if (!tables_ready)
		build_tables();

	while (len--)
		crc = (u16)(crc16_table[((crc >> 8) ^ *p++) & 0xFFu] ^
			    (crc << 8));
	return crc;
}
