// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_SECURITY_H
#define OBSIDIAN_SECURITY_H

#include "../../include/types.h"

/* Arms the stack canary; must run before any protected function. */
void security_early_init(void);

/* Prints the hardening report line by line at boot. */
void security_report(void);

#endif
