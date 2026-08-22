// SPDX-License-Identifier: GPL-2.0-only
#include "backlight.h"
#include "../bus/pci.h"
#include "../../include/printk.h"

/*
 * Intel integrated graphics backlight.
 *
 * On Gen4+ parts the panel PWM lives in MMIO register BLC_PWM_CTL
 * (offset 0x61254 behind the GTT/MMIO BAR): high half holds the
 * modulation frequency, low half the duty cycle. Duty/freq gives the
 * brightness fraction, so setting brightness is one masked write.
 */

#define BLC_PWM_CTL   0x61254
#define PWM_ENABLE    (1u << 31)
#define PWM_FREQ_MASK 0xFFFF0000u

static volatile u32 *pwm_mmio;
static u8 cur_percent;

static volatile u32 *mmio_base_of(const struct pci_dev *vga) {
	for (int i = 0; i < 6; i++)
		if (!vga->bar_is_io[i] && vga->bar_size[i] >= 0x80000)
			return (volatile u32 *)(uintptr_t)vga->bar_base[i];
	return NULL;
}

static bool intel_vga(const struct pci_dev *d) {
	return d->class_code == 0x03 && d->vendor == 0x8086;
}

bool backlight_probe(void) {
	for (int i = 0; i < pci_count(); i++) {
		const struct pci_dev *d = pci_get(i);

		if (!intel_vga(d))
			continue;

		volatile u32 *mmio = mmio_base_of(d);

		if (!mmio)
			continue;

		u32 ctl = mmio[BLC_PWM_CTL / 4];

		if ((ctl & PWM_FREQ_MASK) == 0)
			continue;	/* no panel PWM wired up */

		pwm_mmio = &mmio[BLC_PWM_CTL / 4];
		cur_percent = 100;
		return true;
	}
	return false;
}

bool backlight_present(void) {
	return pwm_mmio != NULL;
}

u8 backlight_get(void) {
	return cur_percent;
}

void backlight_set(u8 percent) {
	if (!pwm_mmio)
		return;

	if (percent > 100)
		percent = 100;

	u32 ctl = *pwm_mmio;
	u32 freq = (ctl & PWM_FREQ_MASK) >> 16;

	if (!freq)
		return;

	u32 duty = ((u32)percent * freq) / 100;

	*pwm_mmio = PWM_ENABLE | (freq << 16) |
		    (duty ? duty : 1);
	cur_percent = percent;
}
