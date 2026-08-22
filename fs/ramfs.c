// SPDX-License-Identifier: GPL-2.0-only
#include "ramfs.h"
#include "../mm/heap.h"
#include "../lib/string.h"

/*
 * Flat in-memory filesystem. Files live in heap blocks sized on
 * first write; reads copy out so callers never hold interior
 * pointers.
 */

struct ramfs_file {
	char name[RAMFS_NAME_LEN];
	u8  *data;
	u32  size;
	bool used;
};

static struct ramfs_file files[RAMFS_MAX_FILES];
static bool ready;

bool ramfs_init(void) {
	memset(files, 0, sizeof(files));
	ready = true;
	return true;
}

static struct ramfs_file *lookup(const char *name) {
	if (!ready || !name)
		return NULL;

	for (int i = 0; i < RAMFS_MAX_FILES; i++)
		if (files[i].used &&
		    k_strcmp(files[i].name, name) == 0)
			return &files[i];
	return NULL;
}

int ramfs_create(const char *name) {
	if (!ready || !name || k_strlen(name) >= RAMFS_NAME_LEN)
		return -1;

	if (lookup(name))
		return -1;

	for (int i = 0; i < RAMFS_MAX_FILES; i++) {
		if (files[i].used)
			continue;

		files[i].used = true;
		k_strncpy(files[i].name, name,
			RAMFS_NAME_LEN - 1);
		files[i].data = NULL;
		files[i].size = 0;
		return i;
	}
	return -1;
}

int ramfs_write(const char *name, const void *data, u32 len) {
	struct ramfs_file *f = lookup(name);

	if (!f)
		return -1;

	u8 *fresh = kmalloc(len ? len : 1);

	if (!fresh)
		return -1;

	memcpy(fresh, data, len);

	if (f->data)
		kfree(f->data);

	f->data = fresh;
	f->size = len;
	return 0;
}

int ramfs_read(const char *name, void *out, u32 max_len) {
	const struct ramfs_file *f = lookup(name);

	if (!f || !out)
		return -1;

	u32 n = f->size < max_len ? f->size : max_len;

	memcpy(out, f->data, n);
	return (int)n;
}

u32 ramfs_size(const char *name) {
	const struct ramfs_file *f = lookup(name);

	return f ? f->size : 0;
}

int ramfs_list(char names[][RAMFS_NAME_LEN], int max) {
	int n = 0;

	for (int i = 0; i < RAMFS_MAX_FILES && n < max; i++)
		if (files[i].used)
			k_strncpy(names[n++], files[i].name,
				RAMFS_NAME_LEN - 1);
	return n;
}
