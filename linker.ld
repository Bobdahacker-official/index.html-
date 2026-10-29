OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS {
    . = 0x1000;
    .text : { *(.text.entry) *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    __kernel_file_end = .;
    .bss : { __bss_start = .; *(.bss*) *(COMMON) __bss_end = .; }
    /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) }
    ASSERT(__kernel_file_end <= 0x10000, "Kernel exceeds boot loader segment")
    ASSERT(__bss_end < 0x80000, "Kernel overlaps stack")
}
