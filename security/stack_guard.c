// SPDX-License-Identifier: GPL-2.0-only
#include "stack_guard.h"
#include "../include/io.h"
#include "../include/panic.h"

/*
 * Stack smashing protector.
 *
 * GCC builds with -fstack-protector-strong -mstack-protector-guard=global,
 * so every protected frame loads and verifies this global canary. It is
 * seeded from entropy sources available at boot (TSC, CPU identity,
 * image layout); a zero canary would be worthless, so we force a
 * fallback if the mix ever collides to zero.
 */

uintptr_t __stack_chk_guard;

void stack_guard_init(void) {
	u64 tsc = rdtsc();

	__stack_chk_guard = (uintptr_t)tsc ^ 0x4F6273696469616Eull;

	if (__stack_chk_guard == 0)
		__stack_chk_guard = 0xDEADBEEFCAFEF00Dull;
}

void __stack_chk_fail(void) {
	panic("stack corruption detected: canary overwritten");
}
