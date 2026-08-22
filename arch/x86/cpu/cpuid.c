// SPDX-License-Identifier: GPL-2.0-only
#include "cpuid.h"
#include "../../lib/string.h"

void get_cpu_vendor(char *out13) {
    u32 a, b, c, d;
    __asm__ volatile ("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0));
    ((u32 *)out13)[0] = b; ((u32 *)out13)[1] = d; ((u32 *)out13)[2] = c;
    out13[12] = 0;
}

static u32 cpuid_max_extended(void) {
    u32 a, b, c, d;
    __asm__ volatile ("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0x80000000));
    return a;
}

/* Full CPU brand string, e.g. "Intel(R) Core(TM) i7-....". Falls back to
   an empty string on CPUs that don't support the extended leaves. */
void get_cpu_brand(char *out49) {
    out49[0] = 0;
    if (cpuid_max_extended() < 0x80000004) return;
    u32 leaves[3] = { 0x80000002, 0x80000003, 0x80000004 };
    for (int i = 0; i < 3; i++) {
        u32 a, b, c, d;
        __asm__ volatile ("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(leaves[i]));
        ((u32 *)out49)[i * 4 + 0] = a;
        ((u32 *)out49)[i * 4 + 1] = b;
        ((u32 *)out49)[i * 4 + 2] = c;
        ((u32 *)out49)[i * 4 + 3] = d;
    }
    out49[48] = 0;
    char *p = out49;
    while (*p == ' ') p++;
    if (p != out49) { int i = 0; while (p[i]) { out49[i] = p[i]; i++; } out49[i] = 0; }
}

void get_cpu_fms(u32 *family, u32 *model, u32 *stepping) {
    u32 a, b, c, d;
    __asm__ volatile ("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1));
    u32 base_family = (a >> 8) & 0xF, base_model = (a >> 4) & 0xF;
    u32 ext_family  = (a >> 20) & 0xFF, ext_model  = (a >> 16) & 0xF;
    *stepping = a & 0xF;
    *family = (base_family == 0xF) ? base_family + ext_family : base_family;
    *model  = (base_family == 6 || base_family == 0xF) ? (ext_model << 4) + base_model : base_model;
}

int get_logical_cpus(void) {
    u32 a, b, c, d;
    __asm__ volatile ("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1));
    int htt = (d >> 28) & 1;
    int logical = (b >> 16) & 0xFF;
    return (!htt || logical == 0) ? 1 : logical;
}

void get_cpu_features(char *out, int max) {
    u32 a, b, c, d;
    __asm__ volatile ("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1));
    out[0] = 0;
    int pos = 0;
#define ADDFLAG(reg, bit, name) \
    if (((reg) >> (bit)) & 1) { \
        const char *s = name " "; int L = k_strlen(s); \
        if (pos + L < max - 1) { for (int k2 = 0; k2 < L; k2++) out[pos++] = s[k2]; out[pos] = 0; } \
    }
    ADDFLAG(d, 0, "fpu");  ADDFLAG(d, 4, "tsc");   ADDFLAG(d, 5, "msr");  ADDFLAG(d, 6, "pae");
    ADDFLAG(d, 8, "cx8");  ADDFLAG(d, 9, "apic");  ADDFLAG(d, 15, "cmov"); ADDFLAG(d, 23, "mmx");
    ADDFLAG(d, 24, "fxsr"); ADDFLAG(d, 25, "sse"); ADDFLAG(d, 26, "sse2"); ADDFLAG(d, 28, "htt");
    ADDFLAG(c, 0, "sse3"); ADDFLAG(c, 9, "ssse3"); ADDFLAG(c, 19, "sse4_1"); ADDFLAG(c, 20, "sse4_2");
    ADDFLAG(c, 5, "vmx");  ADDFLAG(c, 28, "avx");
#undef ADDFLAG
    if (pos == 0) k_strcpy(out, "(none detected)");
}
