// SPDX-License-Identifier: GPL-2.0-only
#include "cmdline.h"
#include "../lib/string.h"

/*
 * Kernel command line parser.
 *
 * Tokens are space separated. A bare token is an action ("version"),
 * key=value pairs are options, and a lone key is a boolean flag.
 */

#define CMDLINE_MAX 512

static char stored[CMDLINE_MAX];
static enum cmdline_action action = CMDLINE_ACTION_NONE;

void cmdline_parse(const char *cmdline) {
	action = CMDLINE_ACTION_NONE;
	stored[0] = 0;

	if (!cmdline)
		return;

	int i = 0;

	for (; cmdline[i] && i < CMDLINE_MAX - 1; i++)
		stored[i] = cmdline[i];
	stored[i] = 0;

	if (k_strstr(stored, "version"))
		action = CMDLINE_ACTION_VERSION;
}

enum cmdline_action cmdline_action(void) {
	return action;
}

bool cmdline_flag(const char *name) {
	const char *p = stored;
	size_t nlen = k_strlen(name);

	while (*p) {
		while (*p == ' ')
			p++;
		if (!*p)
			break;
		const char *tok = p;

		while (*p && *p != ' ' && *p != '=')
			p++;
		if ((size_t)(p - tok) == nlen &&
		    k_strncmp(tok, name, (int)nlen) == 0)
			return true;
		while (*p && *p != ' ')
			p++;
	}
	return false;
}

const char *cmdline_value(const char *key) {
	const char *p = stored;
	size_t klen = k_strlen(key);

	while (*p) {
		while (*p == ' ')
			p++;
		if (!*p)
			break;
		const char *tok = p;

		while (*p && *p != ' ' && *p != '=')
			p++;
		bool match = ((size_t)(p - tok) == klen &&
			      k_strncmp(tok, key, (int)klen) == 0);

		if (*p == '=') {
			p++;
			const char *val = p;

			while (*p && *p != ' ')
				p++;
			if (match)
				return val;
		} else if (match) {
			return "";
		}
	}
	return NULL;
}
