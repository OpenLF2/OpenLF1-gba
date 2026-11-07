/* SPDX-License-Identifier: MIT */
/* Memory map for a GBA cartridge: code and constants in ROM, working data in the fast IWRAM with the
   rest of it (at least 16 KiB) left for the stack, and the heap and bulky data in EWRAM. */

ENTRY(_start)

MEMORY
{
    rom   (rx)  : ORIGIN = 0x08000000, LENGTH = 32M
    iwram (rwx) : ORIGIN = 0x03000000, LENGTH = 0x4000
    ewram (rwx) : ORIGIN = 0x02000000, LENGTH = 256K
}

SECTIONS
{
    .text : {
        KEEP(*(.text.header))
        KEEP(*(.text.boot))
        *(.text .text.* .gnu.linkonce.t.*)
        *(.glue_7) *(.glue_7t) *(.vfp11_veneer)
    } > rom

    .init : { KEEP(*(.init)) } > rom
    .fini : { KEEP(*(.fini)) } > rom

    .rodata : {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
        . = ALIGN(4);
    } > rom

    /* Static constructors and destructors, run by __libc_init_array. */
    .preinit_array : {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } > rom
    .init_array : {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN(__init_array_end = .);
    } > rom
    .fini_array : {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } > rom

    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > rom
    .ARM.exidx : {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > rom

    /* Initialised data lives in ROM and is copied to IWRAM at startup. */
    .data : {
        __data_start = .;
        *(.data .data.* .gnu.linkonce.d.*)
        . = ALIGN(4);
        /* Code that runs from IWRAM is loaded and copied together with the data. */
        *(.iwram.text .iwram.text.*)
        . = ALIGN(4);
        __data_end = .;
    } > iwram AT > rom
    __data_lma = LOADADDR(.data);

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > iwram

    _end = .;

    /* Not cleared or loaded: the program fills these itself. */
    .ewram (NOLOAD) : {
        *(.ewram .ewram.*)
        . = ALIGN(4);
        __ewram_heap_start = .;
    } > ewram

    /DISCARD/ : { *(.comment) *(.note*) }
}
