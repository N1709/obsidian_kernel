// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_NET_H
#define OBSIDIAN_NET_H

#include "../../include/types.h"

#define ETH_ALEN 6

struct net_device {
	const char *name;
	u8  mac[ETH_ALEN];
	u16 io_base;
	bool wireless;
	bool link_up;
	u32 tx_frames;
	u32 rx_frames;
};

/* Registry filled by individual NIC drivers at boot. */
void netdev_register(struct net_device *nd);
int  netdev_count(void);
const u8 *netdev_mac_first(void);
const struct net_device *netdev_get(int idx);

/* Human readable name for known PCI NICs, "" when unknown. */
const char *net_pci_name(u16 vendor, u16 device);

#endif
