// SPDX-License-Identifier: GPL-2.0-only
#include "../include/printk.h"
#include "string.h"
#include "../include/types.h"
#include "../drivers/gpu/console.h"

/*
 * Minimal printf for the kernel: %s %c %d %i %u %x %X %p %o,
 * long modifiers (l, ll, z), width with optional zero pad and
 * left justify. Everything else is passed through verbatim.
 */

struct outbuf {
	char  *p;
	u32    size;
	u32    len;
};

static void ob_putc(struct outbuf *ob, char c) {
	if (ob->p) {
		if (ob->len + 1 < ob->size)
			ob->p[ob->len] = c;
		else if (ob->len < ob->size)
			ob->p[ob->len] = 0;
	}
	ob->len++;
}

static void emit_str(struct outbuf *ob, const char *s, int width, int ljust) {
	int n = k_strlen(s);
	while (!ljust && n < width--) ob_putc(ob, ' ');
	for (int i = 0; i < n; i++) ob_putc(ob, s[i]);
	while (ljust && n < width--) ob_putc(ob, ' ');
}

static void emit_num(struct outbuf *ob, u64 val, int base, int upper,
		     int is_signed, int width, int zero_pad, int ljust) {
	char tmp[70];
	const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	int i = 0, neg = 0;

	if (is_signed && (s64)val < 0) {
		neg = 1;
		val = (u64)(-(s64)val);
	}

	do {
		tmp[i++] = digits[val % (u64)base];
		val /= (u64)base;
	} while (val);

	if (neg) tmp[i++] = '-';

	int total = i;
	int pad = (width > total) ? width - total : 0;

	if (!ljust && !zero_pad)
		for (int k = 0; k < pad; k++) ob_putc(ob, ' ');
	if (!ljust && zero_pad && neg)
		{ ob_putc(ob, '-'); total++; pad--; }
	if (!ljust && zero_pad)
		for (int k = 0; k < pad; k++) ob_putc(ob, '0');

	while (i > 0) ob_putc(ob, tmp[--i]);

	if (ljust)
		for (int k = 0; k < pad - (neg ? 1 : 0); k++) ob_putc(ob, ' ');
}

static void emit_ptr(struct outbuf *ob, uintptr_t v) {
	ob_putc(ob, '0');
	ob_putc(ob, 'x');
	emit_num(ob, (u64)v, 16, 0, 0, sizeof(uintptr_t) * 2, 1, 0);
}

int kvsnprintf(char *buf, u32 size, const char *fmt, va_list ap) {
	struct outbuf ob = { buf, size, 0 };

	for (; *fmt; fmt++) {
		if (*fmt != '%') { ob_putc(&ob, *fmt); continue; }

		fmt++;
		int ljust = 0, zero_pad = 0, width = 0;
		while (*fmt == '-' || *fmt == '0') {
			if (*fmt == '-') ljust = 1; else zero_pad = 1;
			fmt++;
		}
		while (*fmt >= '0' && *fmt <= '9')
			width = width * 10 + (*fmt++ - '0');

		int lmod = 0;
		while (*fmt == 'l' || *fmt == 'z') { lmod++; fmt++; }
		if (*fmt == 'z') { lmod = 2; fmt++; }

		switch (*fmt) {
		case '%':
			ob_putc(&ob, '%');
			break;
		case 'c':
			emit_str(&ob, (char[]){ (char)va_arg(ap, int), 0 },
				 width, ljust);
			break;
		case 's':
			emit_str(&ob, va_arg(ap, const char *) ?
				 va_arg(ap, const char *) : "(null)",
				 width, ljust);
			break;
		case 'd':
		case 'i':
			if (lmod >= 2) emit_num(&ob, va_arg(ap, s64), 10, 0, 1, width, zero_pad, ljust);
			else           emit_num(&ob, (s64)(lmod == 1 ? va_arg(ap, long) : va_arg(ap, int)), 10, 0, 1, width, zero_pad, ljust);
			break;
		case 'u':
			if (lmod >= 2) emit_num(&ob, va_arg(ap, u64), 10, 0, 0, width, zero_pad, ljust);
			else           emit_num(&ob, (u64)(lmod == 1 ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int)), 10, 0, 0, width, zero_pad, ljust);
			break;
		case 'x':
		case 'X':
		case 'o':
		case 'p': {
			u64 v;
			int base = (*fmt == 'o') ? 8 : 16;
			if (*fmt == 'p') { emit_ptr(&ob, (uintptr_t)va_arg(ap, void *)); break; }
			if (lmod >= 2 || lmod == 0) v = va_arg(ap, u64);
			else v = (u64)va_arg(ap, unsigned long);
			if (lmod == 0) v &= 0xFFFFFFFFULL;
			emit_num(&ob, v, base, *fmt == 'X', 0, width, zero_pad, ljust);
			break;
		}
		default:
			ob_putc(&ob, '%');
			ob_putc(&ob, *fmt);
			break;
		}
	}

	if (ob.p && size) {
		u32 end = (ob.len < size) ? ob.len : size - 1;
		ob.p[end] = 0;
	}
	return (int)ob.len;
}

int kprintf(const char *fmt, ...) {
	va_list ap;
	char stackbuf[512];
	va_start(ap, fmt);
	kvsnprintf(stackbuf, sizeof(stackbuf), fmt, ap);
	va_end(ap);
	console_print(stackbuf);
	return 0;
}

void kputs(const char *s) {
	console_print(s);
	console_putc('\n');
}

void kputc(char c) {
	console_putc(c);
}
