/* Obsidian Kernel ARM64 boot entry.
 *
 * Entered at EL1/EL2 with MMU and caches off; x0 holds the DTB pointer
 * from the firmware (QEMU virt, U-Boot or an UEFI stub). Mirrors the
 * contract of linux/arch/arm64/kernel/head.S: park secondary cores,
 * build a stack, clear .bss, then call C with the device tree.
 */

.section .text.boot, "ax"
.global _start
.extern arch_start
.extern __bss_start
.extern __bss_end

_start:
	/* Keep the firmware DTB pointer before anything clobbers x0. */
	ldr	x21, =dtb_ptr
	str	x0, [x21]

	/* Park secondary cores; SMP bring-up arrives later. */
	mrs	x5, mpidr_el1
	and	x5, x5, #3
	cbnz	x5, .park

	/* Stack lives at the end of BSS (see linker script). */
	ldr	x5, =__stack_top
	mov	sp, x5

	/* Zero the BSS. */
	ldr	x6, =__bss_start
	ldr	x7, =__bss_end
.Lzero_loop:
	cmp	x6, x7
	b.hs	.Lzero_done
	str	xzr, [x6], #8
	b	.Lzero_loop
.Lzero_done:

	/* arch_start(dtb) */
	ldr	x0, [x21]
	bl	arch_start

.park:
	wfe
	b	.park

.section .data
.global dtb_ptr
dtb_ptr:
	.quad 0
