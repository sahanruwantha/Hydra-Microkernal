    .section .text
    .globl _start
_start:
    # Set up stack pointer (stack grows downwards)
    la sp, _stack_top

    # Clear .bss section
    la  t0, __bss_start
    la  t1, __bss_end
1:
    bge t0, t1, 2f
    sw  zero, 0(t0)
    addi t0, t0, 4
    j   1b
2:

    # Jump to C kernel entry point
    call kmain

# If kmain ever returns, just spin
hang:
    j hang
