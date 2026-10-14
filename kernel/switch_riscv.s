.section .text
.global context_switch

# context_switch(struct task_context *old, struct task_context *new)
# a0 = old context pointer
# a1 = new context pointer
context_switch:
    # Save callee-saved registers to old context
    sd ra, 0(a0)   # return address
    sd sp, 8(a0)   # stack pointer
    sd s0, 16(a0)  # saved registers s0-s11
    sd s1, 24(a0)
    sd s2, 32(a0)
    sd s3, 40(a0)
    sd s4, 48(a0)
    sd s5, 56(a0)
    sd s6, 64(a0)
    sd s7, 72(a0)
    sd s8, 80(a0)
    sd s9, 88(a0)
    sd s10, 96(a0)
    sd s11, 104(a0)

    # Preserve the floating-point callee-saved set and control state.
    fsd fs0, 112(a0)
    fsd fs1, 120(a0)
    fsd fs2, 128(a0)
    fsd fs3, 136(a0)
    fsd fs4, 144(a0)
    fsd fs5, 152(a0)
    fsd fs6, 160(a0)
    fsd fs7, 168(a0)
    fsd fs8, 176(a0)
    fsd fs9, 184(a0)
    fsd fs10, 192(a0)
    fsd fs11, 200(a0)
    csrr t0, fcsr
    sd t0, 208(a0)

    # Load callee-saved registers from new context
    ld ra, 0(a1)   # return address
    ld sp, 8(a1)   # stack pointer
    ld s0, 16(a1)  # saved registers s0-s11
    ld s1, 24(a1)
    ld s2, 32(a1)
    ld s3, 40(a1)
    ld s4, 48(a1)
    ld s5, 56(a1)
    ld s6, 64(a1)
    ld s7, 72(a1)
    ld s8, 80(a1)
    ld s9, 88(a1)
    ld s10, 96(a1)
    ld s11, 104(a1)

    fld fs0, 112(a1)
    fld fs1, 120(a1)
    fld fs2, 128(a1)
    fld fs3, 136(a1)
    fld fs4, 144(a1)
    fld fs5, 152(a1)
    fld fs6, 160(a1)
    fld fs7, 168(a1)
    fld fs8, 176(a1)
    fld fs9, 184(a1)
    fld fs10, 192(a1)
    fld fs11, 200(a1)
    ld t0, 208(a1)
    csrw fcsr, t0

    ret
