// SPDX-License-Identifier: GPL-2.0-only
#include "net.h"
#include "../gpu/console.h"

static struct net_device registry[8];
static int nreg;

void netdev_register(struct net_device *nd) {
	if (!nd || nreg >= 8)
		return;

	registry[nreg] = *nd;
	nreg++;
}

int netdev_count(void) {
	return nreg;
}

const struct net_device *netdev_get(int idx) {
	if (idx < 0 || idx >= nreg)
		return NULL;
	return &registry[idx];
}

/* MAC of the first registered NIC, NULL when none came up. */
const u8 *netdev_mac_first(void) {
	if (!netdev_count())
		return NULL;
	return netdev_get(0)->mac;
}
