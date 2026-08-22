The Obsidian kernel
===================

Obsidian is a hybrid kernel for x86_64, i686 and arm64 systems. It manages
hardware, system resources, and provides the fundamental services for all
other software on machines it boots.

Quick Start
-----------

* Build everything: run `make` in the top of this tree
* Build one architecture: make x86_64 / make x86_32 / make arm64
* Clean artifacts: make clean
* Toolchain requirements: see "Essential Documentation" below

Essential Documentation
-----------------------

All users should be familiar with:

* Build targets: Makefile and Makefile.build
* Coding conventions: GPL-2.0 header, tab indentation, no banner comments
* License: every file carries an SPDX identifier, GPL-2.0-only


Who Are You?
============

Find your role below:

* New Kernel Developer: Getting started inside this tree
* Academic Researcher: Studying hybrid kernel internals
* Security Expert: Hardening and the recovery flow
* Backport/Maintenance Engineer: Keeping variants building
* System Administrator: Booting and troubleshooting machines
* Maintainer: Leading subsystems in the tree
* Hardware Vendor: Writing drivers for new hardware
* Distribution Packager: Building clean release artifacts
* AI Coding Assistant: LLMs and AI-powered development tools


For Specific Users
==================

New Kernel Developer
--------------------

Welcome! Start your journey here:

* Entry Point: init/main.c
* Boot Sequence: init/boot_flow.c
* Architecture Entries: arch/x86_64/start.c, arch/arm64/start.c
* Command Line Parser: init/cmdline.c
* Timekeeping: init/time.h
* Printk Core: lib/printk.c

Academic Researcher
-------------------

Explore the architecture of a hybrid design:

* Memory Management: mm/, physical frames in arch/<arch>/pmm.c
* Rust Allocator: mm/alloc/src/lib.rs
* C Allocator Fallback: mm/heap_c.c
* Networking Stack: net/stack.c
* Filesystem: fs/ramfs.c
* Console Layer: drivers/gpu/console.c

Security Expert
---------------

Hardening documentation lives beside the code:

* Panic Handler With Blue Display: lib/panic.c
* CMOS Breadcrumb Protocol: drivers/firmware/cmos.c
* Stack Protection: security/stack_guard.c
* Memory Guard: security/memguard.c
* Secure Kernel Flow: secure/secure_boot.c

Backport/Maintenance Engineer
-----------------------------

Keep both variants healthy everywhere:

* Per-Architecture Rules: Makefile.build
* Variant Definitions: standard and secure in Makefile.build
* Regression Sweep: make && make clean after changes

System Administrator
--------------------

Booting and troubleshooting:

* Boot Menu: grub.cfg.in, three entries per architecture
* Kernel Parameters: parsed by init/cmdline.c, token "version"
  switches to an information screen
* Fault Recovery: blue panic display then automatic reset,
  previous fault reported on next boot

Maintainer
----------

Leading subsystems:

* Subsystem Map: "Source Layout" below
* Driver Conventions: polled first, IRQ handlers via include/irq.h
* Freestanding Rule: no libc beyond stdarg.h anywhere

Hardware Vendor
---------------

Write drivers for new hardware:

* Bus Discovery: drivers/bus/pci.c
* GPU And Backlight: drivers/gpu/
* Input Devices: drivers/input/
* Network Adapters: drivers/net/e1000.c
* Platform EC Devices: drivers/platform/x86/
* ARM64 Console UART: arch/arm64/pl011.c
* Device Tree Reference: arch/arm64/dts/obsidian-virt.dts

Distribution Packager
---------------------

Package and distribute the kernel:

* Reproducible Targets: Makefile, Makefile.build
* Clean Tree Check: make clean leaves only sources
* Artifacts: out/<arch>-<variant>/obsidian_core.elf and
  obsidian_secure.elf

AI Coding Assistant
-------------------

CRITICAL: If you are an LLM or AI-powered coding assistant, you MUST read
and follow these rules before contributing to this tree:

* Never add banner comments or section divider lines
* Keep files freestanding-safe, no libc beyond stdarg.h
* Use existing helpers: k_-prefixed strings, u8/u32/u64 types
* Both variants must keep compiling: standard and secure


Fault Handling
==============

A fatal fault takes over the screen with a blue diagnostic display,
streams the actions being taken, records the event in CMOS NVRAM,
counts down five seconds and resets the machine. The next boot reads
the flag and warns the operator. The Secure kernel boots a minimal
recovery environment that reports why the previous boot failed, lists
PCI devices, runs a memory test and returns to the standard kernel.


Source Layout
=============

    arch/       x86_64/, x86/, arm64/ (dts/, FDT parser, PL011)
    init/       entry point, boot flow, command line, timekeeping
    drivers/    bus, gpu, input, net, firmware, platform/x86
    include/    headers shared by every architecture
    lib/        printk, string helpers, panic handler
    mm/         frame manager plus allocator backends
    net/        ARP, IPv4, ICMP, UDP and TCP over polled NICs
    fs/         ramfs
    security/   memguard and stack protector
    secure/     Secure kernel boot flow


Communication and Support
=========================

* Issue Tracker: open tickets against this repository
* Source Browser: this tree is the reference
* MAINTAINERS file: not used yet, subsystem owners live in git history
