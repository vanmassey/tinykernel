.PHONY: all clean run run-debug gdb iso check

run: $(KERNEL)
	qemu-system-x86_64 -kernel $(KERNEL) -serial stdio -display none

run-debug: $(KERNEL)
	qemu-system-x86_64 -kernel $(KERNEL) -serial stdio -display none -d int,cpu_reset

gdb: $(KERNEL)
	qemu-system-x86_64 -kernel $(KERNEL) -serial stdio -display none -s -S

# Boot the kernel headless and assert that it reached userland.
check: $(KERNEL)
	@output=$$(timeout 15 qemu-system-x86_64 -kernel $(KERNEL) \
		-serial stdio -display none -no-reboot); \
	printf '%s\n' "$$output"; \
	printf '%s' "$$output" | grep -q 'tinykernel' || \
		{ echo "FAIL: kernel never booted"; exit 1; }; \
	printf '%s' "$$output" | grep -q 'init:' || \
		{ echo "FAIL: init task never ran"; exit 1; }; \
	if printf '%s' "$$output" | grep -q 'PANIC'; then \
		echo "FAIL: kernel panicked"; exit 1; fi; \
	echo "PASS: booted and init task ran"