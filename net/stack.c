// SPDX-License-Identifier: GPL-2.0-only
#include "netstack.h"
#include "../drivers/net/e1000.h"
#include "../drivers/net/net.h"
#include "../include/printk.h"
#include "../lib/string.h"

/*
 * Compact network stack over the polled e1000 driver.
 *
 * One ARP cache slot, ICMP echo replies, UDP datagrams and a single-
 * connection TCP with active open and orderly close cover the use
 * cases a hobby kernel meets on QEMU user networking.
 */

struct eth_hdr {
	u8  dst[6];
	u8  src[6];
	u16 type;
} __attribute__((packed));

struct arp_pkt {
	u16 htype;
	u16 ptype;
	u8  hlen;
	u8  plen;
	u16 op;
	u8  sha[6];
	u32 spa;
	u8  tha[6];
	u32 tpa;
} __attribute__((packed));

struct ipv4_hdr {
	u8  ver_ihl;
	u8  tos;
	u16 len;
	u16 id;
	u16 frag;
	u8  ttl;
	u8  proto;
	u16 csum;
	u32 src;
	u32 dst;
} __attribute__((packed));

struct icmp_hdr {
	u8  type;
	u8  code;
	u16 csum;
	u16 id;
	u16 seq;
} __attribute__((packed));

struct udp_hdr {
	u16 sport, dport, len, csum;
} __attribute__((packed));

struct tcp_hdr {
	u16 sport, dport;
	u32 seq, ack;
	u8  off_res;
	u8  flags;
	u16 win, csum, urg;
} __attribute__((packed));

#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10

static u8  local_mac[6];
static u32 local_ip   = 0x0F00020Au;	/* 10.0.2.15 */
static u32 gateway_ip = 0x0200020Au;	/* 10.0.2.2 */

void net_stack_set_mac(const u8 mac[6]) {
	memcpy(local_mac, mac, 6);
}

void net_stack_set_ip(u32 ip, u32 gw) {
	local_ip = ip;
	gateway_ip = gw;
}

u32 net_ip_addr(void) {
	return local_ip;
}

const u8 *net_mac_addr(void) {
	return local_mac;
}

/* Checksum helpers */

static u16 inet_csum(const void *data, u16 len,
		     const void *pseudo, u16 plen) {
	const u8 *p = data;
	const u8 *q = pseudo;
	u32 sum = 0;

	for (u16 i = 0; i + 1 < plen; i += 2)
		sum += ((u16)q[i] << 8) | q[i + 1];

	for (u16 i = 0; i + 1 < len; i += 2)
		sum += ((u16)p[i] << 8) | p[i + 1];

	if (len & 1)
		sum += (u16)p[len - 1] << 8;

	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);
	return (u16)(~sum & 0xFFFF);
}

/* Ethernet transmit */

static bool eth_send(u16 ethertype, const u8 dst[6],
		     const void *payload, u16 plen) {
	u8 frame[1600];
	struct eth_hdr *eth = (struct eth_hdr *)frame;

	if (plen > sizeof(frame) - sizeof(*eth))
		return false;

	memcpy(eth->dst, dst, 6);
	memcpy(eth->src, local_mac, 6);
	eth->type = (u16)((ethertype >> 8) |
			  (ethertype << 8));
	memcpy(frame + sizeof(*eth), payload, plen);

	return e1000_send(frame,
			  (u16)(sizeof(*eth) + plen)) == 0;
}

/* Address resolution protocol */

static u8  arp_tha[6];
static u32 arp_spa;		/* resolved address, 0 = empty */

static void arp_rx(const struct arp_pkt *a) {
	if (a->op == ((0x00 << 8) | 0x01)) {	/* request for us */
		struct arp_pkt reply;

		reply.htype = a->htype;
		reply.ptype = a->ptype;
		reply.hlen  = 6;
		reply.plen  = 4;
		reply.op    = 0x0002;
		memcpy(reply.sha, local_mac, 6);
		reply.spa = local_ip;
		memcpy(reply.tha, a->sha, 6);
		reply.tpa = a->spa;

		eth_send(ETHERTYPE_ARP, a->sha,
			 &reply, sizeof(reply));
	} else if (a->op == ((0x00 << 8) | 0x02)) {
		memcpy(arp_tha, a->sha, 6);
		arp_spa = a->spa;
	}
}

static bool arp_resolve(u32 ip, u8 out_mac[6]) {
	if (arp_spa == ip) {
		memcpy(out_mac, arp_tha, 6);
		return true;
	}

	struct arp_pkt req;

	req.htype = 0x0100;
	req.ptype = 0x0008;
	req.hlen  = 6;
	req.plen  = 4;
	req.op    = 0x0001;
	memcpy(req.sha, local_mac, 6);
	req.spa = local_ip;
	memset(req.tha, 0, 6);
	req.tpa = ip;

	/* Broadcast */
	static const u8 bcast[6] =
		{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

	arp_spa = 0;
	eth_send(ETHERTYPE_ARP, bcast, &req, sizeof(req));

	for (int spin = 0; spin < 200000 && !arp_spa; spin++)
		net_poll();

	if (arp_spa == ip) {
		memcpy(out_mac, arp_tha, 6);
		return true;
	}
	return false;
}

/* IPv4 transmit used by every upper layer */

static bool ipv4_send(u32 dst, u8 proto,
		      const void *seg, u16 seg_len) {
	u8 gw_mac[6];
	u32 nexthop = (dst & 0xFFFFFFC0) ==
		      (local_ip & 0xFFFFFFC0) ? dst : gateway_ip;

	if (!arp_resolve(nexthop, gw_mac))
		return false;

	u8 pkt[sizeof(struct ipv4_hdr) + 1480];

	if (seg_len > sizeof(pkt) - sizeof(struct ipv4_hdr))
		return false;

	struct ipv4_hdr ip;

	ip.ver_ihl = 0x45;
	ip.tos     = 0;
	ip.len     = (u16)(((seg_len +
			     sizeof(ip)) & 0xFF) << 8) |
		     (u16)(((seg_len + sizeof(ip)) >> 8) & 0xFF);
	ip.id      = 0;
	ip.frag    = 0;
	ip.ttl     = 64;
	ip.proto   = proto;
	ip.csum    = 0;
	ip.src     = ((local_ip & 0xFF) << 24) |
		     ((local_ip & 0xFF00) << 8) |
		     ((local_ip >> 8) & 0xFF00) |
		     ((local_ip >> 24) & 0xFF);
	ip.dst     = ((dst & 0xFF) << 24) |
		     ((dst & 0xFF00) << 8) |
		     ((dst >> 8) & 0xFF00) |
		     ((dst >> 24) & 0xFF);

	ip.csum = inet_csum(&ip, sizeof(ip), NULL, 0);

	memcpy(pkt, &ip, sizeof(ip));
	memcpy(pkt + sizeof(ip), seg, seg_len);

	return eth_send(ETHERTYPE_IPV4, gw_mac, pkt,
			(u16)(sizeof(ip) + seg_len));
}

/* ICMP */

static void icmp_rx(const void *body, u16 len,
		    u32 src, const struct ipv4_hdr *ip) {
	const struct icmp_hdr *ih = body;

	if (len < sizeof(*ih))
		return;

	if (ih->type != 8)		/* only echo requests */
		return;

	u8 reply[1500];

	memcpy(reply, body, len);
	reply[0] = 0;			/* type = echo reply */
	reply[2] = reply[3] = 0;

	struct ipv4_hdr rip = *ip;

	u32 s = rip.src, d = rip.dst;

	rip.src = d;
	rip.dst = s;
	rip.csum = inet_csum(&rip, sizeof(rip), NULL, 0);

	u16 c = inet_csum(reply, len, NULL, 0);

	reply[2] = (u8)(c >> 8);
	reply[3] = (u8)c;

	u8 hdr[sizeof(struct ipv4_hdr)];

	memcpy(hdr, &rip, sizeof(rip));

	u8 full[1600];

	memcpy(full, hdr, sizeof(hdr));
	memcpy(full + sizeof(hdr), reply, len);

	ipv4_send(src, PROTO_ICMP, reply, len);
	(void)full;
}

/* UDP: tiny sink that logs datagrams and can send one back */

static void udp_rx(const void *seg, u16 len, u32 src) {
	struct udp_hdr uh;

	if (len < sizeof(uh))
		return;

	memcpy(&uh, seg, sizeof(uh));

	u16 sport = ((uh.sport & 0xFF) << 8) | (uh.sport >> 8);

	kprintf("net udp %u bytes from %u.%u.%u.%u port %u\n",
		len - (u16)sizeof(uh),
		src & 0xFF, (src >> 8) & 0xFF,
		(src >> 16) & 0xFF, src >> 24, sport);
}

int udp_send(u32 dst, u16 dport, u16 sport,
	     const void *data, u16 len) {
	u8 buf[sizeof(struct udp_hdr) + 1400];

	if (len > sizeof(buf) - sizeof(struct udp_hdr))
		return -1;

	struct udp_hdr uh;

	uh.sport = ((sport & 0xFF) << 8) | (sport >> 8);
	uh.dport = ((dport & 0xFF) << 8) | (dport >> 8);
	uh.len   = (u16)((((len + 8) & 0xFF) << 8) |
			 (((len + 8) >> 8) & 0xFF));
	uh.csum  = 0;

	memcpy(buf, &uh, sizeof(uh));
	memcpy(buf + sizeof(uh), data, len);

	return ipv4_send(dst, PROTO_UDP, buf,
			 (u16)(len + sizeof(uh))) ? 0 : -1;
}

/* TCP */

enum tcp_state { TCP_CLOSED, TCP_SYN_SENT, TCP_ESTABLISHED,
		 TCP_FIN_WAIT };

static enum tcp_state st = TCP_CLOSED;
static u32 rseq, lseq;
static u16 rport_want, lport = 40000;

static u16 tcp_checksum(u32 peer, const void *t,
			const void *payload, u16 plen) {
	struct {
		u32 src, dst;
		u8 zero, proto;
		u16 len;
	} pseudo;

	pseudo.src  = ((local_ip & 0xFF) << 24) |
		      ((local_ip & 0xFF00) << 8) |
		      ((local_ip >> 8) & 0xFF00) |
		      ((local_ip >> 24) & 0xFF);
	pseudo.dst  = peer;
	pseudo.zero = 0;
	pseudo.proto = PROTO_TCP;
	pseudo.len  = (u16)(((sizeof(struct tcp_hdr) +
			      plen) & 0xFF) << 8) |
		      (((sizeof(struct tcp_hdr) + plen)
			>> 8) & 0xFF);

	return inet_csum(t, sizeof(struct tcp_hdr), &pseudo,
			 sizeof(pseudo));
}

int tcp_connect(u32 ip, u16 port) {
	if (st != TCP_CLOSED || !e1000_present())
		return -1;

	rport_want = port;
	lseq = 1000;
	rseq = 0;

	struct tcp_hdr t;

	memset(&t, 0, sizeof(t));
	t.sport = ((lport & 0xFF) << 8) | (lport >> 8);
	t.dport = ((port & 0xFF) << 8) | (port >> 8);
	t.seq   = lseq;
	t.off_res = 0x50;
	t.flags = TCP_SYN;
	t.win   = ((4096 & 0xFF) << 8) | 0x00;
	t.csum  = tcp_checksum(((ip & 0xFF) << 24) |
			       ((ip & 0xFF00) << 8) |
			       ((ip >> 8) & 0xFF00) |
			       ((ip >> 24) & 0xFF),
			       &t, NULL, 0);

	st = TCP_SYN_SENT;

	if (!ipv4_send(ip, PROTO_TCP, &t, sizeof(t)))
		return -1;

	for (int spin = 0; spin < 500000 &&
	     st == TCP_SYN_SENT; spin++)
		net_poll();

	return st == TCP_ESTABLISHED ? 0 : -1;
}

int tcp_send(u32 ip, const void *data, u16 len) {
	if (st != TCP_ESTABLISHED)
		return -1;

	u8 segbuf[sizeof(struct tcp_hdr) + 1400];

	if (len > sizeof(segbuf) - sizeof(struct tcp_hdr))
		return -1;

	struct tcp_hdr t;

	memset(&t, 0, sizeof(t));
	t.sport = ((lport & 0xFF) << 8) | (lport >> 8);
	t.dport = ((rport_want & 0xFF) << 8) | (rport_want >> 8);
	t.seq   = lseq;
	t.ack   = rseq;
	t.off_res = 0x50;
	t.flags = TCP_ACK | TCP_PSH;
	t.win   = ((4096 & 0xFF) << 8) | 0x00;

	u32 be = ((ip & 0xFF) << 24) | ((ip & 0xFF00) << 8) |
		 ((ip >> 8) & 0xFF00) | ((ip >> 24) & 0xFF);

	t.csum = tcp_checksum(be, &t, data, len);

	memcpy(segbuf, &t, sizeof(t));
	memcpy(segbuf + sizeof(t), data, len);

	lseq += len;
	return ipv4_send(ip, PROTO_TCP, segbuf,
			 (u16)(sizeof(t) + len)) ? 0 : -1;
}

int tcp_close(u32 ip) {
	if (st != TCP_ESTABLISHED)
		return -1;

	struct tcp_hdr t;

	memset(&t, 0, sizeof(t));
	t.sport = ((lport & 0xFF) << 8) | (lport >> 8);
	t.dport = ((rport_want & 0xFF) << 8) | (rport_want >> 8);
	t.seq   = lseq;
	t.ack   = rseq;
	t.off_res = 0x50;
	t.flags = TCP_FIN | TCP_ACK;

	u32 be = ((ip & 0xFF) << 24) | ((ip & 0xFF00) << 8) |
		 ((ip >> 8) & 0xFF00) | ((ip >> 24) & 0xFF);

	t.csum = tcp_checksum(be, &t, NULL, 0);
	ipv4_send(ip, PROTO_TCP, &t, sizeof(t));
	st = TCP_CLOSED;
	return 0;
}

static void tcp_rx(const void *seg, u16 len, u32 src,
		   const struct ipv4_hdr *ip) {
	const struct tcp_hdr *t = seg;

	if (len < sizeof(*t))
		return;

	u16 flags = t->flags;
	u32 seq = ((t->seq & 0xFF) << 24) |
		  ((t->seq & 0xFF00) << 8) |
		  ((t->seq >> 8) & 0xFF00) |
		  (t->seq >> 24);
	u32 ackn = ((t->ack & 0xFF) << 24) |
		   ((t->ack & 0xFF00) << 8) |
		   ((t->ack >> 8) & 0xFF00) |
		   (t->ack >> 24);

	if (st == TCP_SYN_SENT && (flags & TCP_SYN)) {
		rseq = seq + 1;
		lseq = ackn;

		struct tcp_hdr r;

		memset(&r, 0, sizeof(r));
		r.sport = t->dport;
		r.dport = t->sport;
		r.seq   = lseq;
		r.ack   = rseq;
		r.off_res = 0x50;
		r.flags = TCP_ACK;
		r.win   = ((4096 & 0xFF) << 8) | 0x00;
		r.csum  = tcp_checksum(ip->src, &r, NULL, 0);

		ipv4_send(src, PROTO_TCP, &r, sizeof(r));
		st = TCP_ESTABLISHED;
		kputs("net tcp established\n");
		return;
	}

	if (flags & TCP_RST) {
		st = TCP_CLOSED;
		return;
	}

	if (flags & TCP_ACK)
		rseq = seq;

	u16 doff = (t->off_res >> 4) * 4;

	if (len > doff)
		kprintf("net tcp %u bytes received\n",
			len - doff);

	if (flags & TCP_FIN) {
		tcp_close(src);
		kputs("net tcp closed by peer\n");
	}
}

/* Ingress dispatch */

static void ipv4_rx(const u8 *pkt, u16 len) {
	if (len < sizeof(struct ipv4_hdr))
		return;

	struct ipv4_hdr ip;

	memcpy(&ip, pkt, sizeof(ip));

	if ((ip.ver_ihl >> 4) != 4)
		return;

	u16 hlen = (ip.ver_ihl & 0xF) * 4;
	u32 src = ((ip.src & 0xFF) << 24) |
		  ((ip.src & 0xFF00) << 8) |
		  ((ip.src >> 8) & 0xFF00) |
		  (ip.src >> 24);

	switch (ip.proto) {
	case PROTO_ICMP:
		icmp_rx(pkt + hlen, (u16)(len - hlen),
			src, &ip);
		break;
	case PROTO_UDP:
		udp_rx(pkt + hlen, (u16)(len - hlen), src);
		break;
	case PROTO_TCP:
		tcp_rx(pkt + hlen, (u16)(len - hlen), src, &ip);
		break;
	default:
		break;
	}
}

static u8 rxbuf[2048];

void net_poll(void) {
	int got;

	while ((got = e1000_poll(rxbuf, sizeof(rxbuf))) >
	       (int)sizeof(struct eth_hdr)) {
		struct eth_hdr eth;

		memcpy(&eth, rxbuf, sizeof(eth));

		u16 type = (u16)((eth.type >> 8) |
				 (eth.type << 8));

		switch (type) {
		case ETHERTYPE_ARP:
			if (got >= (int)(sizeof(eth) +
					 sizeof(struct arp_pkt)))
				arp_rx((const struct arp_pkt *)
				       (rxbuf + sizeof(eth)));
			break;
		case ETHERTYPE_IPV4:
			ipv4_rx(rxbuf + sizeof(eth),
				(u16)(got - sizeof(eth)));
			break;
		default:
			break;
		}
	}
}

void net_stack_init(void) {
	const u8 *mac = netdev_mac_first();

	if (mac) {
		net_stack_set_mac(mac);
		kprintf("net stack ready %u.%u.%u.%u\n",
			local_ip & 0xFF, (local_ip >> 8) & 0xFF,
			(local_ip >> 16) & 0xFF,
			local_ip >> 24);
	}
}
