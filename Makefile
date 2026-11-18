CC      ?= clang
LD      ?= ld.lld
OBJCOPY ?= llvm-objcopy

BUILD   := build
KERNEL  := $(BUILD)/tinykernel.elf

CFLAGS  := -target x86_64-unknown-none \
           -ffreestanding -fno-stack-protector -fno-pic -fno-builtin \
           -mno-red-zone -m64 \
           -Wall -Wextra -std=c11 -O2 -Iinclude
ASFLAGS := -target x86_64-unknown-none -m64
LDFLAGS := -T linker.ld -nostdlib -static --no-dynamic-linker

C_SRCS  := src/kernel.c src/vga.c src/serial.c src/gdt.c src/idt.c
S_SRCS  := src/boot.S src/isr.S
OBJS    := $(patsubst src/%.c,$(BUILD)/%.o,$(C_SRCS)) \
           $(patsubst src/%.S,$(BUILD)/%.o,$(S_SRCS))

.PHONY: all clean run iso

all: $(KERNEL)

$(BUILD):
	@mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: src/%.S | $(BUILD)
	$(CC) $(ASFLAGS) -c $< -o $@

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

run: $(KERNEL)
	qemu-system-x86_64 -kernel $(KERNEL) -serial stdio -no-reboot -no-shutdown

iso: $(KERNEL)
	@mkdir -p iso/boot
	cp $(KERNEL) iso/boot/tinykernel.elf
	grub-mkrescue -o $(BUILD)/tinykernel.iso iso

clean:
	rm -rf $(BUILD)
