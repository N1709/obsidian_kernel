// SPDX-License-Identifier: GPL-2.0-only
#ifndef OBSIDIAN_STRING_H
#define OBSIDIAN_STRING_H

#include "../include/types.h"

int   k_strlen(const char *s);
int   k_strcmp(const char *a, const char *b);
int   k_strncmp(const char *a, const char *b, int n);
void  k_strcpy(char *dst, const char *src);
char *k_strchr(const char *s, int c);
char *k_strstr(const char *hay, const char *needle);
void  k_itoa64(u64 val, char *out, int base);
int   k_atoi(const char *s);

/* Freestanding replacements GCC emits calls to at -O2/-O3. Without these an
   optimized build fails at link time the moment one is synthesized. bcmp is
   also provided because the Rust allocator archive references it. */
void *memcpy(void *dst, const void *src, u32 n);
void *memmove(void *dst, const void *src, u32 n);
void *memset(void *dst, int val, u32 n);
int   memcmp(const void *a, const void *b, u32 n);
int   bcmp(const void *a, const void *b, u32 n);

#endif
char *k_strncpy(char *dst, const char *src, size_t n);
