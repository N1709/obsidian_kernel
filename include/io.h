// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_IO_H
#define OBSIDIAN_IO_H

#include "types.h"

#if defined(__x86_64__) || defined(__i386__)

static inline u8 inb(u16 port) {
	u8 ret;
	__asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

static inline void outb(u16 port, u8 val) {
	__asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline u16 inw(u16 port) {
	u16 ret;
	__asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

static inline void outw(u16 port, u16 val) {
	__asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline u32 inl(u16 port) {
	u32 ret;
	__asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

static inline void outl(u16 port, u32 val) {
	__asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline void io_wait(void) {
	outb(0x80, 0);
}

static inline void irq_disable(void) { __asm__ volatile ("cli"); }
static inline void irq_enable(void)  { __asm__ volatile ("sti"); }
static inline void cpu_halt(void)    { __asm__ volatile ("hlt"); }
static inline void cpu_relax(void)   { __asm__ volatile ("pause"); }

static inline u64 rdtsc(void) {
	u32 lo, hi;
	__asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
	return ((u64)hi << 32) | lo;
}

#else
/* Portable stubs for architectures without a port IO space (ARM).
   Real devices there are memory mapped and own their accessors. */
static inline u8  inb(u16 p)  { (void)p; return 0; }
static inline void outb(u16 p, u8 v)  { (void)p; (void)v; }
static inline u16 inw(u16 p)  { (void)p; return 0; }
static inline void outw(u16 p, u16 v) { (void)p; (void)v; }
static inline u32 inl(u16 p)  { (void)p; return 0; }
static inline void outl(u32 v, u16 p) { (void)v; (void)p; }

static inline void io_wait(void) { }

static inline void irq_disable(void) {
	__asm__ volatile ("msr daifset, #2");
}

static inline void irq_enable(void) {
	__asm__ volatile ("msr daifclr, #2");
}

static inline void cpu_halt(void) {
	__asm__ volatile ("wfi");
}

/* Generic timer as the cycle counter for the stack canary seed. */
static inline u64 rdtsc(void) {
	u64 v;
	__asm__ volatile ("mrs %0, cntvct_el0" : "=r"(v));
	return v;
}
#endif

#endif
