// SPDX-License-Identifier: GPL-2.0-only
#include "e1000.h"
#include "net.h"
#include "../../include/io.h"
#include "../../include/printk.h"
#include "../../lib/string.h"

/*
 * Intel 8254x (e1000) Ethernet controller driver.
 *
 * Present on QEMU defaults and countless real machines. The device is
 * memory mapped through PCI BAR0; this driver keeps it poll based with
 * interrupts disabled so bring-up stays deterministic.
 */

#define REG_CTRL   0x0000
#define REG_STATUS 0x0008
#define REG_EEPROM 0x0014
#define REG_IMASK  0x00D0
#define REG_RCTL   0x0100
#define REG_TCTL   0x0400

#define REG_RDBAL  0x2800
#define REG_RDBAH  0x2804
#define REG_RDLEN  0x2808
#define REG_RDH    0x2810
#define REG_RDT    0x2818

#define REG_TDBAL  0x3800
#define REG_TDBAH  0x3804
#define REG_TDLEN  0x3808
#define REG_TDH    0x3810
#define REG_TDT    0x3818

struct rx_desc {
	u64 addr;
	u16 length;
	u16 csum;
	u8  status;
	u8  errors;
	u16 special;
} __attribute__((packed));

struct tx_desc {
	u64 addr;
	u16 length;
	u8  cso;
	u8  cmd;
	u8  status;
	u8  css;
	u16 special;
} __attribute__((packed));

#define RX_DESCS 32
#define TX_DESCS 8
#define BUF_SIZE 2048

static volatile u32 *regs;

static struct rx_desc rx_ring[RX_DESCS] __attribute__((aligned(16)));
static struct tx_desc tx_ring[TX_DESCS] __attribute__((aligned(16)));
static u8 rx_buffers[RX_DESCS][BUF_SIZE];
static u8 tx_buffer[BUF_SIZE];

static u16 rx_tail, tx_tail;
static bool link_up;

static u32 rd(u32 off) {
	return regs[off / 4];
}

static void wr(u32 off, u32 val) {
	regs[off / 4] = val;
}

/* Legacy EEPROM word read; QEMU and most 8254x parts implement it. */
static bool eeprom_read(u8 word, u16 *out) {
	wr(REG_EEPROM, 1u | ((u32)word << 8));

	for (int i = 0; i < 200000; i++) {
		u32 v = rd(REG_EEPROM);

		if (v & (1u << 4)) {
			*out = (u16)(v >> 16);
			return true;
		}
	}
	return false;
}

bool e1000_present(void) {
	return regs != NULL;
}

bool e1000_init(const struct pci_dev *dev) {
	if (!dev->bar_base[0])
		return false;

	regs = (volatile u32 *)(uintptr_t)dev->bar_base[0];

	wr(REG_CTRL, rd(REG_CTRL) | (1u << 26));
	for (int i = 0; i < 500000; i++)
		if (!(rd(REG_CTRL) & (1u << 26)))
			break;

	wr(REG_IMASK, 0);

	struct net_device nd;

	memset(&nd, 0, sizeof(nd));
	nd.name = "intel-e1000";

	bool mac_ok = true;

	for (int i = 0; i < 3; i++)
		mac_ok &= eeprom_read((u8)i,
				      (u16 *)&nd.mac[i * 2]);

	if (!mac_ok)
		return false;

	link_up = (rd(REG_STATUS) & (1u << 1)) != 0;
	nd.link_up = link_up;

	/* Receive ring: hand the controller physical addresses; linear
	   identity mapping makes virtual == physical here. */
	for (int i = 0; i < RX_DESCS; i++) {
		rx_ring[i].addr = (u64)(uintptr_t)rx_buffers[i];
		rx_ring[i].status = 0;
	}

	wr(REG_RDBAL, (u32)((uintptr_t)rx_ring));
#ifdef __x86_64__
	wr(REG_RDBAH, (u32)(((u64)(uintptr_t)rx_ring) >> 32));
#else
	wr(REG_RDBAH, 0);
#endif
	wr(REG_RDLEN, sizeof(rx_ring));
	wr(REG_RDH, 0);
	wr(REG_RDT, RX_DESCS - 1);
	rx_tail = RX_DESCS - 1;

	/* Enable receiver: broadcast accept, promiscuous unicast/multicast,
	   2048-byte buffers. */
	wr(REG_RCTL, (1u << 1) | (1u << 3) | (1u << 5) | (1u << 15));

	for (int i = 0; i < TX_DESCS; i++)
		tx_ring[i].status = 0;

	wr(REG_TDBAL, (u32)((uintptr_t)tx_ring));
#ifdef __x86_64__
	wr(REG_TDBAH, (u32)(((u64)(uintptr_t)tx_ring) >> 32));
#else
	wr(REG_TDBAH, 0);
#endif
	wr(REG_TDLEN, sizeof(tx_ring));
	wr(REG_TDH, 0);
	wr(REG_TDT, 0);
	tx_tail = 0;

	/* Enable transmitter, pad short frames below 64 bytes. */
	wr(REG_TCTL, (1u << 1) | (1u << 3));

	netdev_register(&nd);

	kprintf("net %s mac "
		"%02X:%02X:%02X:%02X:%02X:%02X link %s\n",
		nd.name,
		nd.mac[0], nd.mac[1], nd.mac[2],
		nd.mac[3], nd.mac[4], nd.mac[5],
		link_up ? "up" : "down");
	return true;
}

int e1000_send(const void *frame, u16 len) {
	if (!regs || !link_up || len == 0 || len > BUF_SIZE)
		return -1;

	memcpy(tx_buffer, frame, len);

	u16 t = tx_tail;
	struct tx_desc *d = &tx_ring[t];

	d->addr = (u64)(uintptr_t)tx_buffer;
	d->length = len;
	d->cso = 0;
	d->cmd = 0x0B;		/* end of packet | insert FCS | report status */
	d->status = 0;
	d->css = 0;
	d->special = 0;

	tx_tail = (u16)((t + 1) % TX_DESCS);
	wr(REG_TDT, tx_tail);

	for (int i = 0; i < 200000; i++)
		if (d->status & 0x01)
			return 0;
	return -1;
}

int e1000_poll(u8 *buf, u16 max_len) {
	if (!regs)
		return -1;

	u16 next = (u16)((rx_tail + 1) % RX_DESCS);
	struct rx_desc *d = &rx_ring[next];

	if (!(d->status & 0x01))
		return 0;

	u16 got = d->length < max_len ? d->length : max_len;

	memcpy(buf, rx_buffers[next], got);

	d->status = 0;
	rx_tail = next;
	wr(REG_RDT, rx_tail);
	return got;
}
