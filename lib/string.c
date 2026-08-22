// SPDX-License-Identifier: GPL-2.0-only
#include "string.h"

int k_strlen(const char *s) {
	int n = 0;
	while (s[n]) n++;
	return n;
}

int k_strcmp(const char *a, const char *b) {
	while (*a && (*a == *b)) { a++; b++; }
	return (u8)*a - (u8)*b;
}

int k_strncmp(const char *a, const char *b, int n) {
	for (int i = 0; i < n; i++) {
		if (a[i] != b[i]) return (u8)a[i] - (u8)b[i];
		if (a[i] == 0) return 0;
	}
	return 0;
}

void k_strcpy(char *dst, const char *src) {
	while ((*dst++ = *src++)) { }
}

char *k_strchr(const char *s, int c) {
	for (; *s; s++)
		if (*s == (char)c) return (char *)s;
	return (c == 0) ? (char *)s : NULL;
}

char *k_strstr(const char *hay, const char *needle) {
	int nl = k_strlen(needle);
	for (int i = 0; hay[i]; i++) {
		if (k_strncmp(hay + i, needle, nl) == 0) return (char *)(hay + i);
	}
	return NULL;
}

void k_itoa64(u64 val, char *out, int base) {
	char tmp[70];
	int i = 0, j = 0;

	if (base < 2 || base > 36) base = 10;
	if (val == 0) { out[0] = '0'; out[1] = 0; return; }

	while (val) {
		u64 d = val % (u64)base;
		tmp[i++] = (d < 10) ? ('0' + (char)d) : ('a' + (char)(d - 10));
		val /= (u64)base;
	}
	while (i > 0) out[j++] = tmp[--i];
	out[j] = 0;
}

int k_atoi(const char *s) {
	int n = 0, neg = 0, i = 0;

	if (s[0] == '-') { neg = 1; i = 1; }
	for (; s[i]; i++) {
		if (s[i] < '0' || s[i] > '9') break;
		n = n * 10 + (s[i] - '0');
	}
	return neg ? -n : n;
}

void *memcpy(void *dst, const void *src, u32 n) {
	u8 *d = (u8 *)dst;
	const u8 *s = (const u8 *)src;
	while (((uintptr_t)d & 3) && n) { *d++ = *s++; n--; }
	u32 *dw = (u32 *)d;
	const u32 *sw = (const u32 *)s;
	while (n >= 4) { *dw++ = *sw++; n -= 4; }
	d = (u8 *)dw; s = (const u8 *)sw;
	while (n--) *d++ = *s++;
	return dst;
}

void *memmove(void *dst, const void *src, u32 n) {
	u8 *d = (u8 *)dst;
	const u8 *s = (const u8 *)src;
	if (d == s || n == 0) return dst;
	if (d < s) {
		while (n--) *d++ = *s++;
	} else {
		d += n; s += n;
		while (n--) *--d = *--s;
	}
	return dst;
}

void *memset(void *dst, int val, u32 n) {
	u8 *d = (u8 *)dst;
	u8 v = (u8)val;
	while (((uintptr_t)d & 3) && n) { *d++ = v; n--; }
	u32 vw = v | (v << 8) | (v << 16) | ((u32)v << 24);
	u32 *dw = (u32 *)d;
	while (n >= 4) { *dw++ = vw; n -= 4; }
	d = (u8 *)dw;
	while (n--) *d++ = v;
	return dst;
}

static int mem_cmp(const void *a, const void *b, u32 n) {
	const u8 *pa = (const u8 *)a, *pb = (const u8 *)b;
	while (n--) {
		if (*pa != *pb) return (int)*pa - (int)*pb;
		pa++; pb++;
	}
	return 0;
}

int memcmp(const void *a, const void *b, u32 n) {
	return mem_cmp(a, b, n);
}

int bcmp(const void *a, const void *b, u32 n) {
	return mem_cmp(a, b, n);
}

/* Copy at most n bytes, zero padding the remainder; always
   terminates when src fits, unlike the libc foot-gun. */
char *k_strncpy(char *dst, const char *src, size_t n) {
	size_t i;

	for (i = 0; i < n && src[i]; i++)
		dst[i] = src[i];
	for (; i < n; i++)
		dst[i] = 0;
	return dst;
}
