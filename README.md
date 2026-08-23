```
The Obsidian kernel
===================

Obsidian is a hybrid operating-system kernel for three machine families,
written from scratch in C with one Rust component. It manages hardware,
system resources, and provides the fundamental services for all other
software on machines it boots.


Quick Start
-----------

* Build everything: run make in the top of this tree
* Build one architecture: make x86_64 / make x86_32 / make arm64
* Clean artifacts: make clean
* Toolchain requirements: gcc, nasm, rustc for x86_64 targets,
  gcc-aarch64-linux-gnu for arm64


Artifacts
---------

Every build lands under out/<arch>-<variant>/:

    out/x86_64-standard/obsidian_core.elf     full kernel, 64-bit PC
    out/x86_64-secure/obsidian_secure.elf     recovery kernel
    out/x86-standard/obsidian_core.elf        full kernel, 32-bit PC
    out/x86-secure/obsidian_secure.elf        recovery kernel
    out/arm64-standard/obsidian_core.elf      full kernel, arm64 virt
    out/arm64-secure/obsidian_secure.elf      recovery kernel


What Is Inside
--------------

    Boot flow     init/                 entry, dispatch, cmdline, clock
    Memory        mm/, arch/*/pmm.c     frame manager; Rust allocator
                                        behind a stable ABI, C twin
    Console       drivers/gpu/          text VGA, Bochs LFB, backlight
    Input         drivers/input/        PS/2, Synaptics pad, serial
                                        mouse, analog gameport
    Network       net/, drivers/net/    e1000 polled NIC; ARP, IPv4,
                                        ICMP, UDP, TCP active-open
    Storage       fs/                   ramfs, flat in-memory files
    Platform      drivers/platform/     embedded controller, ThinkPad
                                        fan control, battery meter
    Firmware      drivers/firmware/     CMOS NVRAM records, SMBIOS
    ACPI          drivers/acpi/         RSDP discovery, FADT walk
    Guards        security/             stack protector, memory guard
    Recovery      secure/               secure kernel boot flow


Supported Hardware
------------------

    Display     VGA text mode                        stable
    Display     Bochs / std-vga linear framebuffer   stable
    Display     Intel panel PWM backlight            stable
    Keyboard    PS/2 set 1 / set 2                   stable
    Pointing    PS/2 mouse                           stable
    Pointing    Synaptics touchpad, absolute mode    stable
    Pointing    Microsoft serial mouse on 16550      stable
    Joystick    legacy analog gameport               experimental
    Ethernet    Intel e1000                          stable
    Wireless    detection only                       partial
    Laptop      ThinkPad EC fan control, battery     stable
    Serial      16550 UART, ARM PL011                stable
    ARM64       QEMU virt machine via device tree    stable


When A Fault Happens
--------------------

The kernel never dies silently.

1. Screen switches to a blue diagnostic panel listing the fault
2. Every recovery action streams live onto the panel
3. Event stamped into CMOS NVRAM before anything else runs
4. Five second countdown, then the machine resets itself
5. Next boot reads the stamp and warns the operator
6. Secure kernel takes over: explains the failure, enumerates
   PCI devices, tests memory, hands back to normal boot

Relevant files:

    lib/panic.c                  diagnostic panel and reset logic
    drivers/firmware/cmos.c      NVRAM event stamping
    secure/secure_boot.c         recovery personality


Repository Map
--------------

    arch/       per-machine code: x86_64/, x86/, arm64/
    init/       where execution begins and how boot unfolds
    drivers/    everything that touches devices
    include/    interfaces shared across all machines
    lib/        printing, strings, panic machinery
    mm/         allocators and heap plumbing
    net/        protocols layered over polled NICs
    fs/         ramfs implementation
    security/   hardening guards
    secure/     recovery personality


Status And Plans
----------------

Done today: six green builds, working network stack, input family,
fan and battery telemetry, automatic crash recovery loop.

Planned next:

* vendor keyboard-light backends
* brightness through ACPI _BCM methods
* persistent filesystems beyond ramfs
* SMP bring-up on arm64
* interrupt-driven networking


License
-------

GPL-2.0-only. Each file carries an SPDX header matching its origin.
```
