/**
 * Linker script for CH32V003F4P6
 * Optimized for code size with RISC-V compressed instructions
 *
 * Memory layout:
 *   FLASH: 16KB (mapped to 0x00000000 for optimal C extension encoding)
 *   RAM:   2KB  (0x20000000 - 0x20000800)
 *
 * Note: Using 0x00000000 instead of 0x08000000 allows better use of
 * RISC-V compressed (16-bit) instructions, reducing code size by ~5%.
 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 16K
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

    /* Initialized data will be copied from FLASH to RAM */
    .dalign : ALIGN(4)
    {
        PROVIDE(_data_vma = .);
    } > RAM AT > FLASH

    .dlalign : ALIGN(4)
    {
        PROVIDE(_data_lma = .);
    } > FLASH AT > FLASH

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

    _data_loadaddr = LOADADDR(.data);

    /* Uninitialized data (zero-initialized) */
    .bss (NOLOAD) : ALIGN(4)
    {
        _bss_start = .;
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(.sbss)
        *(.sbss*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
        _ebss = .;
    } > RAM

    /* End of data section */
    _edata = _data_end;

    /* Stack grows downward from end of RAM */
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
    _eusrstack = _stack_top;

    /* Global pointer for RISC-V small data optimization */
    __global_pointer$ = MIN(_data_start + 0x800, MAX(_data_start + 0x800, _bss_end - 0x800));

    /* Symbols for memory usage reporting */
    _heap_start = _bss_end;
    _heap_end = _stack_top;

    /* Discard unnecessary sections */
    /DISCARD/ :
    {
        *(.note*)
        *(.comment)
        *(.ARM.exidx*)
        *(.gnu.linkonce.armexidx.*)
    }
}
