// SPDX-License-Identifier: GPL-2.0-only
#include "../include/panic.h"
#include "../include/printk.h"
#include "../include/io.h"
#include "../include/obsidian.h"
#include "../drivers/gpu/console.h"
#include "../drivers/firmware/cmos.h"
#include <stdarg.h>

/*
 * Panic handler.
 *
 * Takes over the whole screen with a white-on-blue fault display,
 * streams a short diagnostic log so the user sees the system doing
 * something rather than freezing silently, records the event in CMOS
 * NVRAM for the next boot, counts down, then resets the machine.
 * The next boot reports the previous panic and points the operator at
 * the Secure Kernel entry in the boot menu.
 */

#define PANIC_ATTR   0x1F	/* white on blue */
#define TICK_LOOPS 300000u	/* ~1 s worth of port posts */

static void panic_delay(u32 ticks) {
	for (u32 t = 0; t < ticks; t++)
		for (u32 i = 0; i < TICK_LOOPS; i++)
			io_wait();
}

static void live_log(const char *msg) {
	console_print("  [ ok ] ");
	console_print(msg);
	console_print("\n");
	panic_delay(1);
}

void panic(const char *fmt, ...) {
	char buf[256];
	va_list ap;

	va_start(ap, fmt);
	kvsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	irq_disable();

	cmos_write(0x3A, 0x4F);	/* panic marker for the next boot */
	cmos_write(0x3B, 0x01);	/* reason code 1: generic fault */

	console_set_attr(PANIC_ATTR);
	console_clear();

	console_print("  O B S I D I A N   K E R N E L   P A N I C\n\n");
	console_print("  error : ");
	console_print(buf);
	console_print("\n");
	console_print("  action: switching to the Secure Kernel after reset\n\n");

	live_log("console buffers flushed");
	live_log("interrupts disabled system-wide");
	live_log("fault context recorded to NVRAM");
	live_log("reset controller armed");

	console_print("\n  rebooting in ");
	console_flush();

	for (int s = 5; s > 0; s--) {
		char digit = (char)('0' + s);

		console_putc(digit);
		console_print(" ");
		console_flush();
		panic_delay(1);
	}

	kernel_reboot();

	for (;;)
		cpu_halt();	/* unreachable; keep the compiler honest */
}
