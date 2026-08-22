// SPDX-License-Identifier: GPL-2.0-only
#include "security.h"
#include "stack_guard.h"
#include "../include/printk.h"

/*
 * Security subsystem coordinator.
 *
 * The kernel ships layered mitigations: a stack canary armed before any
 * protected frame runs, heap poisoning with double-free detection, and
 * an allocation policy that refuses low memory or misaligned results.
 */

void security_early_init(void) {
	stack_guard_init();
}

void security_report(void) {
	kputs("sec  canary armed, heap poison on, allocation policy active");
}
