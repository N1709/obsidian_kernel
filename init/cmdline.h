// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_CMDLINE_H
#define OBSIDIAN_CMDLINE_H

#include "../include/types.h"

enum cmdline_action {
	CMDLINE_ACTION_NONE,
	CMDLINE_ACTION_VERSION,
};

void cmdline_parse(const char *cmdline);
enum cmdline_action cmdline_action(void);
bool cmdline_flag(const char *name);
const char *cmdline_value(const char *key);

#endif
