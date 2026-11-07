OUTPUT_ARCH(aarch64)
ENTRY(_head)

SECTIONS
{
    . = 0;
    .text : ALIGN(0x800) {
        KEEP(*(.text.head))
        *(.text .text.*)
    }
    .rodata : ALIGN(16) {
        *(.rodata .rodata.*)
    }
    .data : ALIGN(16) {
        *(.data .data.*)
    }
    .bss (NOLOAD) : ALIGN(16) {
        _bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        _bss_end = .;
    }
    . = ALIGN(0x1000);
    _sec_stacks = .;
    . += 0x8000; /* eight 4 KiB secondary stacks, indexed by MPIDR */
    _stack_top = .;
    . += 0x4000; /* 16 KiB boot CPU stack */
    _end = .;
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
