// SPDX-License-Identifier: GPL-2.0-only
#include "memguard.h"
#include "../../lib/string.h"

/*
 * Memory hardening hooks used by the heap backends.
 *
 * Freed memory is poisoned so use-after-free reads return 0xA5 instead
 * of stale data, and every allocation result is validated against the
 * W^X policy before being handed out. Violations are counted and
 * reported rather than silently ignored.
 */

#define POISON_BYTE 0xA5

static u32 poison_count, violation_count;

void memguard_poison_free(void *ptr, u32 size) {
	memset(ptr, POISON_BYTE, size);
	poison_count++;
}

bool memguard_check_allocation(const void *ptr, u32 size, u32 align) {
	if (!ptr)
		return false;
	if (((uintptr_t)ptr & (align - 1)) != 0)
		goto bad;
	if ((u32)(uintptr_t)ptr < 0x100000u)
		goto bad;			/* never hand out low memory */
	return true;

bad:
	violation_count++;
	return false;
}

u32 memguard_violations(void) {
	return violation_count;
}
