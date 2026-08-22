// SPDX-License-Identifier: GPL-2.0-only
#include "../include/types.h"

/*
 * 64-bit divide support for i686, where gcc would normally pull
 * __udivmoddi4 from a multilib libgcc we do not ship. Shift-subtract
 * is plenty fast for formatting numbers in printk.
 */

#if defined(__i386__) && !defined(__x86_64__)

u64 __udivmoddi4(u64 num, u64 den, u64 *rem_out) {
	u64 quot = 0, rem = 0;
	int i;

	if (!den) {
		if (rem_out)
			*rem_out = 0;
		return 0;
	}

	for (i = 63; i >= 0; i--) {
		rem = (rem << 1) | ((num >> i) & 1ULL);
		if (rem >= den) {
			rem -= den;
			quot |= 1ULL << i;
		}
	}

	if (rem_out)
		*rem_out = rem;
	return quot;
}

#endif
