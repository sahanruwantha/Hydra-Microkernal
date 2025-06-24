ENTRY(kernel_main)

MEMORY
{
  RAM (rwx) : ORIGIN = 1M, LENGTH = 15M  
}

SECTIONS
{
  . = ORIGIN(RAM);

  .text : {
    *(.multiboot)
    *(.text)
  } > RAM

  .rodata : { *(.rodata) } > RAM

  .data : { *(.data) } > RAM

  .bss : {
    *(.bss)
    *(COMMON)
  } > RAM

  /* Reserve space for heap after BSS */
  .heap (NOLOAD) : ALIGN(8) {
    __heap_start = .;
    . = . + 1M;        
    __heap_end = .;
  } > RAM
}
