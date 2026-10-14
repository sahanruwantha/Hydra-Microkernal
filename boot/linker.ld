MEMORY
{
    RAM : ORIGIN = 0x80200000, LENGTH = 126M
}

ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5); /* read + execute */
    data PT_LOAD FLAGS(6); /* read + write */
}

SECTIONS
{
    . = 0x80200000;

    __kernel_text_start = .;

    .text : {
        *(.text.boot)
        *(.text*)
    } > RAM :text

    .rodata : {
        KEEP(*(.rodata*))
        KEEP(*(.srodata*))
    } > RAM :text

    __kernel_text_end = .;

    /* Code deliberately entered in U-mode. It is linked into the image but
     * placed on dedicated pages so U permission never exposes kernel text. */
    .user_text ALIGN(4096) : {
        __user_text_start = .;
        KEEP(*(.user_text))
        . = ALIGN(4096);
        __user_text_end = .;
    } > RAM :text

    /* Keep writable state on separate pages and in a non-executable ELF
     * segment. This also removes the linker's RWX-segment warning. */
    . = ALIGN(4096);

    __kernel_data_start = .;
    .data ALIGN(4096) : {
        *(.data*)
        *(.sdata*)
    } > RAM :data

    .bss : {
        __bss_start = .;
        *(.bss*)
        *(.sbss*)
        *(COMMON)
        __bss_end = .;
    } > RAM :data

    . = ALIGN(16);
    __heap_start = .;
    . += 512K;  /* Increase heap size to 512K */
    __heap_end = .;
    
    . = ALIGN(16);
    . += 128K;  /* Increase stack size to 128K */
    _stack_top = .;

    /* Page-aligned physical pool used by Sv39 tables and isolated task
     * stacks. It is intentionally outside the small-object heap. */
    . = ALIGN(4096);
    __page_pool_start = .;
    . += 8M;
    __page_pool_end = .;
    __kernel_data_end = .;
}
