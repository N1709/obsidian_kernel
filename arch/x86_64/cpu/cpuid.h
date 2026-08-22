#ifndef OBSIDIAN_CPUID_H
#define OBSIDIAN_CPUID_H

#include "../../include/types.h"

void get_cpu_vendor(char *out13);
void get_cpu_brand(char *out49);
void get_cpu_fms(u32 *family, u32 *model, u32 *stepping);
int  get_logical_cpus(void);
void get_cpu_features(char *out, int max);

#endif
