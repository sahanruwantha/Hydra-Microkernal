MEMORY
{
    RAM : ORIGIN = 0x80200000, LENGTH = 126M
}

SECTIONS
{
    . = 0x80200000;

    .text : {
        *(.text.boot)
        *(.text*)
    } > RAM

    .rodata : {
        KEEP(*(.rodata*))
    } > RAM

    .data : {
        *(.data*)
    } > RAM

    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    . = ALIGN(16);
    __heap_start = .;
    . += 512K;  /* Increase heap size to 512K */
    __heap_end = .;
    
    . = ALIGN(16);
    . += 128K;  /* Increase stack size to 128K */
    _stack_top = .;
}
