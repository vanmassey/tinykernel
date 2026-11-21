# tinykernel

A small **x86_64** hobby kernel in C. It boots through Multiboot2, enters long mode, then brings up a GDT, IDT, PIC, PIT, PS/2 keyboard, VGA text, serial, a 4 KiB physical page allocator, a kernel heap, a higher-half page map, a round-robin task switcher, and a kernel shell over an in-memory ramfs.

```
tinykernel/
  src/boot.S      Multiboot2 header, paging, long-mode trampoline
  src/kernel.c    kernel_main
  src/gdt.c       64-bit GDT
  src/idt.c       IDT + CPU exceptions
  src/isr.S       ISR / IRQ stubs
  src/pic.c       8259 PIC remap
  src/pit.c       100 Hz timer
  src/kbd.c       PS/2 keyboard (set 1, US)
  src/irq.c       IRQ dispatch
  src/mb2.c       Multiboot2 tag walk
  src/pmm.c       physical page bitmap
  src/kheap.c     kmalloc / kfree
  src/string.c    memset / memcpy / strlen
  src/vga.c       80x25 text
  src/serial.c    COM1
  src/ramfs.c     fixed in-memory files
  src/vfs.c       open/read/write on top of ramfs
  src/klog.c      kernel message ring
  src/rtc.c       CMOS clock
  src/pipe.c      byte pipes
  src/shell.c     kernel shell
  src/commands.c  date, dmesg, rm, mv, cp, hex, logcat, ...
  include/        public headers
  linker.ld       load at 1 MiB
  iso/boot/grub/  GRUB config for an optional ISO
```

## Roadmap

1. **Boot + core** — Multiboot2, long mode, GDT, exception IDT, VGA, serial. **Done.**
2. **Interrupts + input** — PIC, PIT, keyboard, idle with `sti; hlt`. **Done.**
3. **Physical memory** — Multiboot2 mmap, 4 KiB page bitmap (first 1 GiB). **Done.**
4. **Kernel heap** — `kmalloc` / `kfree` on top of the PMM. **Done.**
5. **Virtual memory** — proper page tables, higher-half kernel, user mappings.
6. **Tasks** — kernel threads, context switch, round-robin on the PIT.
7. **Userland** — ring 3, syscalls (`write`, `exit`), a tiny init program.
8. **Storage / VFS** — in-memory ramfs plus a kernel shell. **Done** (no disk yet).

## Build

You need a **freestanding ELF toolchain**:

- Clang + LLD (`clang`, `ld.lld`), or
- an `x86_64-elf-gcc` cross compiler

On Windows, WSL is the least painful path.

```sh
make
```

## Run

QEMU can load the ELF directly (no ISO required):

```sh
qemu-system-x86_64 -kernel build/tinykernel.elf -serial stdio
```

or `make run`.

To wrap it in a GRUB ISO (needs `grub-mkrescue` / xorriso):

```sh
make iso
qemu-system-x86_64 -cdrom build/tinykernel.iso -serial stdio
```

You should see a boot banner, then a `tk>` prompt. Keys are queued by the keyboard IRQ. Try `help`, `ls`, `cat readme`, `write note hi`, `ps`, `mem`, `uptime`, and `sleep 500`. The timer preempts only when another task is ready; `sleep` parks the shell and the idle task `hlt`s until it wakes.

## Layout notes

- Identity-maps the first 1 GiB with 2 MiB pages so VGA (`0xB8000`) and the kernel (1 MiB) are valid. The PMM only hands out pages in that window.
- PIC IRQs 0–15 are remapped to IDT vectors 32–47. IRQ0 (timer) and IRQ1 (keyboard) are unmasked.
- `kernel_main` panics if the Multiboot2 magic is not `0x36d76289`.
