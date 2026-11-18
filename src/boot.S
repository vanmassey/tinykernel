/* Multiboot2 header + 32-bit trampoline into long mode. */

#define MULTIBOOT2_MAGIC        0xE85250D6
#define MULTIBOOT2_ARCH_I386    0
#define CR0_PG                  (1 << 31)
#define CR4_PAE                 (1 << 5)
#define EFER_MSR                0xC0000080
#define EFER_LME                (1 << 8)
#define PAGE_PRESENT            0x01
#define PAGE_WRITE              0x02
#define PAGE_HUGE               0x80

.section .multiboot, "a"
.align 8
mb_header_start:
    .long MULTIBOOT2_MAGIC
    .long MULTIBOOT2_ARCH_I386
    .long mb_header_end - mb_header_start
    .long -(MULTIBOOT2_MAGIC + MULTIBOOT2_ARCH_I386 + (mb_header_end - mb_header_start))

    /* framebuffer request: 80x25 text */
    .short 5
    .short 0
    .long 20
    .long 80
    .long 25
    .long 0
    .align 8

    /* end tag */
    .short 0
    .short 0
    .long 8
mb_header_end:

.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:

.align 4096
boot_pml4:
    .skip 4096
boot_pdpt:
    .skip 4096
boot_pd:
    .skip 4096

mb_magic:
    .long 0
mb_info:
    .long 0

.section .data
.align 16
gdt64:
    .quad 0x0000000000000000
    .quad 0x00209A0000000000   /* 0x08: 64-bit kernel code */
    .quad 0x0000920000000000   /* 0x10: kernel data */
gdt64_end:

gdt64_ptr:
    .word gdt64_end - gdt64 - 1
    .quad gdt64

.section .text.boot, "ax"
.code32
.global _start
.type _start, @function
_start:
    cli
    movl $stack_top, %esp
    movl %eax, mb_magic
    movl %ebx, mb_info

    /* Mask PIC IRQs until the IDT is installed. */
    movb $0xFF, %al
    outb %al, $0xA1
    outb %al, $0x21

    call check_long_mode
    call setup_page_tables

    movl $boot_pml4, %eax
    movl %eax, %cr3

    movl %cr4, %eax
    orl $CR4_PAE, %eax
    movl %eax, %cr4

    movl $EFER_MSR, %ecx
    rdmsr
    orl $EFER_LME, %eax
    wrmsr

    movl %cr0, %eax
    orl $CR0_PG, %eax
    movl %eax, %cr0

    lgdt gdt64_ptr
    ljmp $0x08, $enter_long_mode

check_long_mode:
    /* CPUID bit 21 must be writable. */
    pushfl
    popl %eax
    movl %eax, %ecx
    xorl $(1 << 21), %eax
    pushl %eax
    popfl
    pushfl
    popl %eax
    pushl %ecx
    popfl
    xorl %ecx, %eax
    jz .no_long_mode

    movl $0x80000000, %eax
    cpuid
    cmpl $0x80000001, %eax
    jb .no_long_mode

    movl $0x80000001, %eax
    cpuid
    testl $(1 << 29), %edx
    jz .no_long_mode
    ret

.no_long_mode:
    movl $0xB8000, %edi
    movl $0x4F4E4F4F, (%edi)   /* "NO" in red-on-white-ish VGA words */
    hlt
    jmp .no_long_mode

setup_page_tables:
    /* Identity-map the first 1 GiB with 2 MiB pages. */
    movl $boot_pdpt + PAGE_PRESENT + PAGE_WRITE, %eax
    movl %eax, boot_pml4

    movl $boot_pd + PAGE_PRESENT + PAGE_WRITE, %eax
    movl %eax, boot_pdpt

    movl $0, %ecx
    movl $boot_pd, %edi
.pd_loop:
    movl %ecx, %eax
    shll $21, %eax
    orl $(PAGE_PRESENT + PAGE_WRITE + PAGE_HUGE), %eax
    movl %eax, (%edi)
    movl $0, 4(%edi)
    addl $8, %edi
    incl %ecx
    cmpl $512, %ecx
    jl .pd_loop
    ret

.code64
enter_long_mode:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw %ax, %gs

    movq $stack_top, %rsp
    xorq %rbp, %rbp

    movl mb_magic, %edi
    movl mb_info, %esi
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang
.size _start, . - _start
