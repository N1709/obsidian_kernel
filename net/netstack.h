// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_NETSTACK_H
#define OBSIDIAN_NETSTACK_H

#include "../include/types.h"

/*
 * Obsidian network stack: Ethernet, ARP, IPv4, ICMP echo, UDP and a
 * compact active-open TCP implementation running over polled NICs.
 */

#define ETHERTYPE_ARP  0x0806
#define ETHERTYPE_IPV4 0x0800

#define PROTO_ICMP 1
#define PROTO_TCP  6
#define PROTO_UDP  17

void net_stack_init(void);

/* Periodic drain: call from the idle loop. */
void net_poll(void);

/* Configuration reported by the discovery layer. */
void net_stack_set_mac(const u8 mac[6]);
void net_stack_set_ip(u32 ip, u32 gateway);

u32 net_ip_addr(void);
const u8 *net_mac_addr(void);

#endif
