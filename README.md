# tinykernel

A small **x86_64** hobby kernel in C. It boots through Multiboot2, enters long mode, then brings up a 64-bit GDT, exception IDT, VGA text console, and COM1 serial.

```
tinykernel/
  src/boot.S      Multiboot2 header, paging, long-mode trampoline
  src/kernel.c    kernel_main
  src/gdt.c       64-bit GDT
  src/idt.c       exception IDT
  src/isr.S       ISR stubs
  src/vga.c       80x25 text
  src/serial.c    COM1
  include/        public headers
  linker.ld       load at 1 MiB
  iso/boot/grub/  GRUB config for an optional ISO
```

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

You should see `tinykernel` on the VGA text screen and a boot line on serial. The kernel then halt-loops on purpose — there is no scheduler yet.

## Layout notes

- Identity-maps the first 1 GiB with 2 MiB pages so `0xB8000` VGA and the kernel image (loaded at 1 MiB) are both valid.
- PIC IRQs are masked at boot; only CPU exceptions 0–31 are installed.
- `kernel_main` panics if the Multiboot2 magic is not `0x36d76289`.
