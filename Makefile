CC      ?= clang
LD      ?= ld.lld
OBJCOPY ?= llvm-objcopy

BUILD   := build
KERNEL  := $(BUILD)/tinykernel.elf

CFLAGS  := -target x86_64-unknown-none \
           -ffreestanding -fno-stack-protector -fno-pic -fno-builtin \
           -mno-red-zone -m64 -mno-sse -mno-sse2 -mno-mmx -mno-80387 \
           -Wall -Wextra -std=c11 -O2 -Iinclude
ASFLAGS := -target x86_64-unknown-none -m64
LDFLAGS := -T linker.ld -nostdlib -static --no-dynamic-linker -z max-page-size=0x1000

C_SRCS  := src/kernel.c src/vga.c src/serial.c src/io.c src/gdt.c src/idt.c \
           src/pic.c src/pit.c src/kbd.c src/irq.c src/mb2.c src/pmm.c \
           src/string.c src/kheap.c src/spinlock.c src/vmm.c src/task.c \
           src/syscall.c src/ramfs.c src/shell.c src/printf.c src/klog.c \
           src/rtc.c src/vfs.c src/pipe.c src/reboot.c src/ticker.c \
           src/commands.c
S_SRCS  := src/boot.S src/isr.S src/sched.S
OBJS    := $(patsubst src/%.c,$(BUILD)/%.o,$(C_SRCS)) \
           $(patsubst src/%.S,$(BUILD)/%.o,$(S_SRCS))

HEADERS := $(wildcard include/*.h)

.PHONY: all clean run run-debug gdb iso check syntax

all: $(KERNEL)

$(BUILD):
	@mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: src/%.S | $(BUILD)
	$(CC) $(ASFLAGS) -c $< -o $@

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# Fast syntax/type check of every C source; no output files are produced.
syntax:
	@for src in $(C_SRCS); do \
		echo "  CC $$src"; \
		$(CC) $(CFLAGS) -fsyntax-only $$src || exit 1; \
	done
	@echo "syntax ok"

run: $(KERNEL)
	qemu-system-x86_64 -kernel $(KERNEL) -serial stdio -display none -no-reboot -no-shutdown

run-debug: $(KERNEL)
	qemu-system-x86_64 -kernel $(KERNEL) -serial stdio -display none -d int,cpu_reset -no-reboot -no-shutdown

gdb: $(KERNEL)
	qemu-system-x86_64 -kernel $(KERNEL) -serial stdio -display none -s -S

# Build a GRUB ISO for the Multiboot2 kernel.
iso: $(KERNEL)
	@mkdir -p iso/boot
	cp $(KERNEL) iso/boot/tinykernel.elf
	grub-mkrescue -o $(BUILD)/tinykernel.iso iso

# Boot through GRUB (QEMU -kernel expects Linux/PVH, not Multiboot2).
# The kernel intentionally halts after init, so timeout is expected.
check: iso
	@output=$$(timeout 15 qemu-system-x86_64 -cdrom $(BUILD)/tinykernel.iso \
		-boot d -serial stdio -display none -no-reboot 2>&1); \
	printf '%s\\n' "$$output"; \
	printf '%s' "$$output" | grep -q 'tinykernel' || \
		{ echo "FAIL: kernel never booted"; exit 1; }; \
	printf '%s' "$$output" | grep -q 'init' || \
		{ echo "FAIL: init task never ran"; exit 1; }; \
	if printf '%s' "$$output" | grep -q 'PANIC'; then \
		echo "FAIL: kernel panicked"; exit 1; fi; \
	echo "PASS: booted and init task ran"

clean:
	rm -rf $(BUILD)
