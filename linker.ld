OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

PHDRS
{
    limine_requests PT_LOAD FLAGS(1);
    text            PT_LOAD FLAGS(5);  /* Read + Execute */
    rodata          PT_LOAD FLAGS(4);  /* Read */
    data            PT_LOAD FLAGS(6);  /* Read + Write */
}

SECTIONS
{
    /* Base address - where Limine will load us */
    . = 0xffffffff80000000;

    /* Limine requests must be first, within the first 4KB */
    .limine_requests : {
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end_marker))
        KEEP(*(.limine_requests_start_marker))
    } :limine_requests

    . = ALIGN(4K);

    .text : {
        *(.text .text.*)
    } :text

    . = ALIGN(4K);

    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    . = ALIGN(4K);

    .data : {
        *(.data .data.*)
    } :data

    .bss : {
        *(COMMON)
        *(.bss .bss.*)
    } :data

    /* Discard unnecessary sections */
    /DISCARD/ : {
        *(.note .note.*)
        *(.eh_frame)
        *(.comment)
    }
}