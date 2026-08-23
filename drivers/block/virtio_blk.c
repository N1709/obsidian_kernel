// SPDX-License-Identifier: GPL-2.0-only
#include "blkdev.h"
#if defined(__aarch64__)

/*
 * virtio-blk over virtio-MMIO, as exposed by QEMU's 'virt' machine
 * (-device virtio-blk-device,drive=hd0).
 *
 * Both transports are handled: legacy (register version reads 1,
 * ring handed over as a single page frame number) and modern virtio
 * 1.0 (version 2, ring tables programmed piecewise). The split
 * virtqueue layout itself is identical for both. Everything runs
 * polled; interrupts stay masked at the GIC.
 *
 * The kernel maps RAM one-to-one, so virtual addresses double as
 * DMA addresses exactly like the e1000 driver assumes.
 */

#include "../../include/io.h"
#include "../../include/printk.h"
#include "../../lib/string.h"

#define VIRTIO_MMIO_BASE 0x0A000000ull
#define VIRTIO_MMIO_STEP 0x200
#define VIRTIO_MMIO_SLOTS 32

/* Register offsets shared by both versions */
#define REG_MAGIC      0x000
#define REG_VERSION    0x004
#define REG_DEVID      0x008
#define REG_VENDOR     0x00C
#define REG_HFEATURES  0x010
#define REG_HFSEL      0x014
#define REG_GFEATURES  0x020
#define REG_GFSEL      0x024

#define REG_QSEL       0x028	/* modern only */
#define REG_QNUMMAX    0x034
#define REG_QNUM       0x030

/* legacy only */
#define REG_QALIGN     0x03C
#define REG_QPFN       0x040
#define REG_QNOTIFY    0x050
#define REG_IRQSTATUS  0x060
#define REG_IRQACK     0x064
#define REG_STATUS     0x070

/* modern only */
#define REG_QREADY     0x044
#define REG_QDESC_LO   0x080
#define REG_QDESC_HI   0x084
#define REG_QAVAIL_LO  0x090
#define REG_QAVAIL_HI  0x094
#define REG_QUSED_LO   0x0A0
#define REG_QUSED_HI   0x0A4

#define REG_CONFIG     0x100

#define VM_MAGIC  0x74726976u
#define VM_DEVBLK 2u

/* status bits */
#define VS_ACK       1u
#define VS_DRIVER    2u
#define VS_DRIVER_OK 4u
#define VS_FEAT_OK   8u

#define VRING_DESC_F_NEXT  1u
#define VRING_DESC_F_WRITE 2u

#define REQ_TYPE_IN  0u
#define REQ_TYPE_OUT 1u
#define REQ_TYPE_FLUSH 4u

#define QUEUE_ENTS  64
#define RING_BYTES  8192
#define BOUNCE_SECS 32
#define SECTOR_SIZE 512

struct vring_desc {
	u64 addr;
	u32 len;
	u16 flags;
	u16 next;
} __attribute__((packed));

struct vring_avail {
	u16 flags;
	u16 idx;
	u16 ring[QUEUE_ENTS];
} __attribute__((packed));

struct vring_used_elem {
	u32 id;
	u32 len;
} __attribute__((packed));

struct vring_used {
	u16 flags;
	u16 idx;
	struct vring_used_elem ring[QUEUE_ENTS];
} __attribute__((packed));

struct virtio_blk_config {
	u64 capacity;		/* in 512-byte sectors */
	u32 size_max;
	u32 seg_max;
};

struct blk_req_hdr {
	u32 type;
	u32 ioprio;
	u64 sector;
} __attribute__((packed));

static volatile u32 *mmio;
static bool modern;

static u8 ring_mem[RING_BYTES] __attribute__((aligned(4096)));
static u8 bounce[BOUNCE_SECS][SECTOR_SIZE] __attribute__((aligned(16)));

static struct vring_desc *desc_tab;
static struct vring_avail *avail;
static struct vring_used *used;
static u16 free_head, free_count;
static u16 last_used;

static u32 rd(u32 off) {
	return mmio[off / 4];
}

static void wr(u32 off, u32 val) {
	mmio[off / 4] = val;
}

static void dma_barrier(void) {
	__asm__ volatile ("dsb sy" ::: "memory");
}

/* Legacy rings live in one contiguous page-aligned blob laid out as
   desc | avail | pad-to-page | used. */
static size_t legacy_vring_size(u32 num, u32 align) {
	size_t a = (size_t)num * 16 + 2 * (3 + (size_t)num);

	a = (a + align - 1) & ~((size_t)align - 1);
	return a + 6 + 8u * num;
}

static int negotiate_features(void) {
	wr(REG_HFSEL, 0);
	u32 lo = rd(REG_HFEATURES);

	if (!modern) {
		wr(REG_GFSEL, 0);
		wr(REG_GFEATURES, 0);
		return 0;
	}

	/* Modern devices want VIRTIO_F_VERSION_1 (bit 32) offered */
	wr(REG_HFSEL, 1);
	u32 hi = rd(REG_HFEATURES);

	if (!(hi & 1u))
		return -1;

	wr(REG_GFSEL, 1);
	wr(REG_GFEATURES, 1u);
	wr(REG_GFSEL, 0);
	wr(REG_GFEATURES, 0);

	(void)lo;
	return 0;
}

static int setup_queue(void) {
	if (modern) {
		wr(REG_QSEL, 0);
	} else {
		/* legacy has no queue selector; queue 0 implied */
	}

	u32 qmax = rd(REG_QNUMMAX);

	if (!(qmax & 0xFFFFu) || qmax == 0xFFFFFFFFu)
		return -1;

	u32 qn = qmax < QUEUE_ENTS ? qmax : QUEUE_ENTS;

	wr(REG_QNUM, qn);
	desc_tab = (struct vring_desc *)ring_mem;
	avail = (struct vring_avail *)(ring_mem + qn * 16);
	used = (struct vring_used *)(((uintptr_t)avail +
				      sizeof(u16) * (3 + qn) + 4095) &
				     ~4095u);

	memset(ring_mem, 0, RING_BYTES);

	if (modern) {
		u64 d = (uintptr_t)desc_tab;
		u64 a = (uintptr_t)avail;
		u64 u = (uintptr_t)used;

		dma_barrier();
		wr(REG_QDESC_LO, (u32)d);
		wr(REG_QDESC_HI, (u32)(d >> 32));
		wr(REG_QAVAIL_LO, (u32)a);
		wr(REG_QAVAIL_HI, (u32)(a >> 32));
		wr(REG_QUSED_LO, (u32)u);
		wr(REG_QUSED_HI, (u32)(u >> 32));
		wr(REG_QREADY, 1);
	} else {
		wr(REG_QALIGN, 4096);
		dma_barrier();
		wr(REG_QPFN, (u32)((uintptr_t)ring_mem >> 12));
	}

	free_head = 0;
	free_count = (u16)qn;
	last_used = 0;

	for (u32 i = 0; i < qn; i++) {
		desc_tab[i].next = (u16)(i + 1);
		desc_tab[i].flags = VRING_DESC_F_NEXT;
	}
	desc_tab[qn - 1].flags = 0;	/* last descriptor terminates */

	dma_barrier();
	return 0;
}

static u8 dev_status;
static struct blk_req_hdr req_hdr;

static int submit(bool is_write, u64 sector, u32 nsect, u8 *data_out) {
	req_hdr.type = is_write ? REQ_TYPE_OUT : REQ_TYPE_IN;
	req_hdr.ioprio = 0;
	req_hdr.sector = sector;
	dev_status = 0xEE;

	u16 h = free_head;
	u16 d_data = (u16)((h + 1) % QUEUE_ENTS);
	u16 d_stat = (u16)((h + 2) % QUEUE_ENTS);

	desc_tab[h].addr = (uintptr_t)&req_hdr;
	desc_tab[h].len = sizeof(req_hdr);
	desc_tab[h].flags = VRING_DESC_F_NEXT;

	desc_tab[d_data].addr = (uintptr_t)data_out;
	desc_tab[d_data].len = nsect * SECTOR_SIZE;
	desc_tab[d_data].flags = VRING_DESC_F_NEXT |
				 (is_write ? 0 : VRING_DESC_F_WRITE);

	desc_tab[d_stat].addr = (uintptr_t)&dev_status;
	desc_tab[d_stat].len = 1;
	desc_tab[d_stat].flags = VRING_DESC_F_WRITE;

	dma_barrier();

	avail->ring[avail->idx % QUEUE_ENTS] = h;
	avail->idx++;

	dma_barrier();
	wr(REG_QNOTIFY, 0);

	while ((u16)(used->idx - last_used) == 0)
		cpu_relax();

	dma_barrier();
	last_used++;

	free_head = (u16)((d_stat + 1) % QUEUE_ENTS);
	dma_barrier();

	return dev_status == 0 ? 0 : -1;
}

static int vblk_read(u64 lba, u32 count, void *buf) {
	u8 *out = buf;

	while (count) {
		u32 chunk = count < BOUNCE_SECS ? count : BOUNCE_SECS;

		if (submit(false, lba, chunk, bounce[0]))
			return -1;

		memcpy(out, bounce[0], chunk * SECTOR_SIZE);
		out += chunk * SECTOR_SIZE;
		lba += chunk;
		count -= chunk;
	}
	return 0;
}

static int vblk_write(u64 lba, u32 count, const void *buf) {
	const u8 *in = buf;

	while (count) {
		u32 chunk = count < BOUNCE_SECS ? count : BOUNCE_SECS;

		memcpy(bounce[0], in, chunk * SECTOR_SIZE);

		if (submit(true, lba, chunk, bounce[0]))
			return -1;

		in += chunk * SECTOR_SIZE;
		lba += chunk;
		count -= chunk;
	}
	return 0;
}

void virtio_blk_probe(void) {
	for (int slot = 0; slot < VIRTIO_MMIO_SLOTS; slot++) {
		volatile u32 *base = (volatile u32 *)
			(uintptr_t)(VIRTIO_MMIO_BASE +
				    (u64)slot * VIRTIO_MMIO_STEP);

		if (base[REG_MAGIC / 4] != VM_MAGIC)
			continue;

		u32 ver = base[REG_VERSION / 4];
		u32 devid = base[REG_DEVID / 4];

		if (devid != VM_DEVBLK || (ver != 1 && ver != 2))
			continue;

		mmio = base;
		modern = ver >= 2;

		wr(REG_STATUS, 0);
		wr(REG_STATUS, VS_ACK | VS_DRIVER);

		if (negotiate_features()) {
			kputs("blk virtio unsupported feature set");
			mmio = NULL;
			return;
		}

		wr(REG_STATUS, VS_ACK | VS_DRIVER | VS_FEAT_OK);

		if (setup_queue())
			goto fail;

		volatile struct virtio_blk_config *cfg =
			(volatile struct virtio_blk_config *)
			((uintptr_t)mmio + REG_CONFIG);

		u64 sectors = cfg->capacity;

		if (!sectors)
			goto fail;

		u32 ro = rd(REG_HFEATURES) & (1u << 5);

		wr(REG_STATUS, VS_ACK | VS_DRIVER | VS_FEAT_OK | VS_DRIVER_OK);

		char name[] = "virtio0";

		name[6] = (char)('0' + blk_count());

		struct blk_device bd;

		memset(&bd, 0, sizeof(bd));
		k_strcpy(bd.name, name);
		bd.present = true;
		bd.sector_size = SECTOR_SIZE;
		bd.sectors = sectors;
		bd.writable = ro == 0;
		bd.read_sectors = vblk_read;
		bd.write_sectors = vblk_write;

		if (!blk_register(&bd))
			goto fail;

		kprintf("blk %s virtio %llu MB%s\n", name,
			(unsigned long long)(sectors >> 11),
			ro ? " (ro)" : "");
		return;

fail:
		mmio = NULL;
		return;
	}
}

#else /* !arm64 */

void virtio_blk_probe(void) { }

#endif
