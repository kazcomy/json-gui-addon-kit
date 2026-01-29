/**
 * Linker script for CH32V003F4P6
 *
 * Memory layout:
 *   FLASH: 16KB (0x08000000 - 0x08004000)
 *   RAM:   2KB  (0x20000000 - 0x20000800)
 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 16K
    RAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 2K
}

ENTRY(_start)

SECTIONS
{
    /* Interrupt vector table */
    .init : ALIGN(4)
    {
        KEEP(*(.init))
        KEEP(*(.vector))
    } > FLASH

    /* Program code and constants */
    .text : ALIGN(4)
    {
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        *(.srodata)
        *(.srodata*)

        /* C++ constructors/destructors (if needed) */
        . = ALIGN(4);
        KEEP(*(.preinit_array))
        KEEP(*(.init_array))
        KEEP(*(.fini_array))

        . = ALIGN(4);
        _etext = .;
    } > FLASH

    /* ARM.exidx for exception handling (usually not needed for RISC-V) */
    .ARM.exidx : {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > FLASH

    /* Initialized data will be copied from FLASH to RAM */
    .data : ALIGN(4)
    {
        _data_start = .;
        *(.data)
        *(.data*)
        *(.sdata)
        *(.sdata*)
        . = ALIGN(4);
        _data_end = .;
    } > RAM AT > FLASH

    _data_lma = LOADADDR(.data);

    /* Uninitialized data (BSS) */
    .bss : ALIGN(4)
    {
        _bss_start = .;
        *(.bss)
        *(.bss*)
        *(.sbss)
        *(.sbss*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
    } > RAM

    /* Stack (grows downward from end of RAM) */
    .stack : ALIGN(4)
    {
        . = ALIGN(4);
        _heap_end = .;
        . = . + 0x100; /* 256 bytes for heap (minimal) */
        _stack_start = .;
        . = ORIGIN(RAM) + LENGTH(RAM);
        _stack_end = .;
    } > RAM

    /* Define end of RAM for heap management */
    _end = .;
    PROVIDE(end = .);

    /* Remove debug sections to save space */
    /DISCARD/ :
    {
        *(.note*)
        *(.comment)
    }
}

/* Symbols for startup code */
PROVIDE(_stack_top = ORIGIN(RAM) + LENGTH(RAM));
PROVIDE(_heap_start = _bss_end);
