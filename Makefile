# Obsidian Kernel build system.
#
# Usage:
#   make            build everything (both architectures, both variants)
#   make x86_64     64-bit kernels only
#   make x86_32     32-bit kernels only
#   make arm64      aarch64 kernels (needs aarch64-linux-gnu-gcc)
#   make iso        bootable GRUB images with the English boot menu
#   make run        QEMU the 64-bit ISO
#   make run32      QEMU the 32-bit ISO
#   make secure     rebuild only the Secure Kernels
#   make clean      delete all artifacts
#
# The kernel version lives here and is injected into every compile.

OBSIDIAN_NAME       := obsidian
OBSIDIAN_VERSION    := 0.3.0

BUILD_DATE          := $(shell date +"%Y-%m-%d %H:%M:%S %Z")

CC      := gcc
LD      := gcc
NASM    := nasm
RUSTC   := rustc
GRUB_MK := grub-mkrescue

export CC LD NASM RUSTC GRUB_MK

QEMU64  ?= qemu-system-x86_64
QEMU32  ?= qemu-system-i386

ARCHES  := x86_64 x86 arm64
VARIANTS:= standard secure

.PHONY: all help x86_64 x86_32 arm64 iso run run32 img run-hd run32-hd run-arm64 secure clean distclean

all:
	@for a in $(ARCHES); do \
		for v in $(VARIANTS); do \
			$(MAKE) --no-print-directory build-one ARCH=$$a VARIANT=$$v || exit 1; \
		done; \
	done

x86_64:
	@for v in $(VARIANTS); do \
		$(MAKE) --no-print-directory build-one ARCH=x86_64 VARIANT=$$v || exit 1; \
	done

x86_32:
	@for v in $(VARIANTS); do \
		$(MAKE) --no-print-directory build-one ARCH=x86 VARIANT=$$v || exit 1; \
	done

arm64:
	@if ! command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then \
		echo "arm64 skipped: install gcc-aarch64-linux-gnu"; exit 0; \
	fi
	@for v in $(VARIANTS); do \
		$(MAKE) --no-print-directory build-one ARCH=arm64 VARIANT=$$v || exit 1; \
	done

secure:
	@for a in $(ARCHES); do \
		$(MAKE) --no-print-directory build-one ARCH=$$a VARIANT=secure || exit 1; \
	done

build-one:
	@$(MAKE) --no-print-directory -f Makefile.build \
		ARCH=$(ARCH) VARIANT=$(VARIANT) \
		OBSIDIAN_VERSION=$(OBSIDIAN_VERSION) \
		BUILD_DATE="$(BUILD_DATE)" kernel

iso: all
	@$(MAKE) --no-print-directory iso-x86_64
	@$(MAKE) --no-print-directory iso-x86

iso-x86_64:
	@rm -rf stage/x86_64
	@mkdir -p stage/x86_64/boot/grub
	@cp out/x86_64-standard/obsidian_core.elf   stage/x86_64/boot/
	@cp out/x86_64-secure/obsidian_secure.elf     stage/x86_64/boot/
	@sed -e "s/@VERSION@/$(OBSIDIAN_VERSION)/g" \
	     -e "s/@ARCHNAME@/x86_64/g" \
	     -e "s/@LOADERCMD@/multiboot2/g" grub.cfg.in \
	     > stage/x86_64/boot/grub/grub.cfg
	$(GRUB_MK) -o obsidian-x86_64.iso stage/x86_64

iso-x86:
	@rm -rf stage/x86
	@mkdir -p stage/x86/boot/grub
	@cp out/x86-standard/obsidian_core.elf      stage/x86/boot/
	@cp out/x86-secure/obsidian_secure.elf        stage/x86/boot/
	@sed -e "s/@VERSION@/$(OBSIDIAN_VERSION)/g" \
	     -e "s/@ARCHNAME@/i686/g" \
	     -e "s/@LOADERCMD@/multiboot/g" grub.cfg.in \
	     > stage/x86/boot/grub/grub.cfg
	$(GRUB_MK) -o obsidian-i386.iso stage/x86

run: iso-x86_64
	$(QEMU64) -m 512 -vga std -cdrom obsidian-x86_64.iso

run32: iso-x86
	$(QEMU32) -m 512 -cdrom obsidian-i386.iso

# ---- ext4 persistence demo -------------------------------------------
# make img    : create obsidian.img formatted as ext4
# make run-hd : boot the 64-bit ISO with the image attached as hard disk

IMG     := obsidian.img
IMG_MB  ?= 64

img:
	truncate -s $(IMG_MB)M $(IMG)
	mke2fs -q -F -t ext4 -b 4096 -I 256 \
		-E lazy_itable_init=0,lazy_journal_init=0 $(IMG)

run-hd: iso-x86_64 img
	$(QEMU64) -m 512 -vga std -cdrom obsidian-x86_64.iso -hda $(IMG)

run32-hd: iso-x86 img
	$(QEMU32) -m 512 -cdrom obsidian-i386.iso -hda $(IMG)

run-arm64: img
	qemu-system-aarch64 -M virt -cpu cortex-a57 -m 512 -nographic \
		-kernel out/arm64-standard/obsidian_core.elf \
		-drive if=none,file=$(IMG),id=hd0,format=raw \
		-device virtio-blk-device,drive=hd0

clean:
	rm -rf out stage *.iso

distclean: clean
