.section .text
.global trap_entry
.global set_trap_vector

# struct trap_frame is 544 bytes and 16-byte aligned. Offsets mirror
# kernel/sched.h. A complete frame is required before timer preemption can
# safely switch execution contexts.
.equ TF_SIZE,    544
.equ TF_SEPC,    248
.equ TF_SSTATUS, 256
.equ TF_STVAL,   264
.equ TF_F0,      272
.equ TF_FCSR,    528
.equ TF_RESERVED,536

set_trap_vector:
    la t0, trap_entry
    csrw stvec, t0
    csrw sscratch, zero
    ret

.balign 4
trap_entry:
    # U-mode returns leave the task's kernel-stack top in sscratch. Swap
    # immediately without destroying any general register. For supervisor
    # traps sscratch is zero, so swap back and continue on the current stack.
    csrrw sp, sscratch, sp
    bnez sp, .Ltrap_from_user
    csrrw sp, sscratch, sp
    addi sp, sp, -TF_SIZE

    # Save t0 before using it to reconstruct the interrupted sp.
    sd t0, 32(sp)
    sd ra, 0(sp)
    addi t0, sp, TF_SIZE
    sd t0, 8(sp)
    sd zero, TF_RESERVED(sp)
    j .Ltrap_save_common

.Ltrap_from_user:
    addi sp, sp, -TF_SIZE
    sd t0, 32(sp)
    sd ra, 0(sp)
    csrr t0, sscratch
    sd t0, 8(sp)                  # interrupted user sp
    addi t0, sp, TF_SIZE
    sd t0, TF_RESERVED(sp)        # kernel stack top for the next U trap
    csrw sscratch, zero            # nested supervisor traps stay on kernel sp

.Ltrap_save_common:
    sd gp, 16(sp)
    sd tp, 24(sp)
    sd t1, 40(sp)
    sd t2, 48(sp)
    sd s0, 56(sp)
    sd s1, 64(sp)
    sd a0, 72(sp)
    sd a1, 80(sp)
    sd a2, 88(sp)
    sd a3, 96(sp)
    sd a4, 104(sp)
    sd a5, 112(sp)
    sd a6, 120(sp)
    sd a7, 128(sp)
    sd s2, 136(sp)
    sd s3, 144(sp)
    sd s4, 152(sp)
    sd s5, 160(sp)
    sd s6, 168(sp)
    sd s7, 176(sp)
    sd s8, 184(sp)
    sd s9, 192(sp)
    sd s10, 200(sp)
    sd s11, 208(sp)
    sd t3, 216(sp)
    sd t4, 224(sp)
    sd t5, 232(sp)
    sd t6, 240(sp)

    csrr t0, sepc
    sd t0, TF_SEPC(sp)
    csrr t0, sstatus
    sd t0, TF_SSTATUS(sp)
    csrr t0, stval
    sd t0, TF_STVAL(sp)

    # Floating-point instructions trap when sstatus.FS is Off. Save the
    # complete FP register file only when FP state is enabled.
    ld t0, TF_SSTATUS(sp)
    li t1, 0x6000
    and t0, t0, t1
    beqz t0, 1f
    fsd f0,  TF_F0+0(sp)
    fsd f1,  TF_F0+8(sp)
    fsd f2,  TF_F0+16(sp)
    fsd f3,  TF_F0+24(sp)
    fsd f4,  TF_F0+32(sp)
    fsd f5,  TF_F0+40(sp)
    fsd f6,  TF_F0+48(sp)
    fsd f7,  TF_F0+56(sp)
    fsd f8,  TF_F0+64(sp)
    fsd f9,  TF_F0+72(sp)
    fsd f10, TF_F0+80(sp)
    fsd f11, TF_F0+88(sp)
    fsd f12, TF_F0+96(sp)
    fsd f13, TF_F0+104(sp)
    fsd f14, TF_F0+112(sp)
    fsd f15, TF_F0+120(sp)
    fsd f16, TF_F0+128(sp)
    fsd f17, TF_F0+136(sp)
    fsd f18, TF_F0+144(sp)
    fsd f19, TF_F0+152(sp)
    fsd f20, TF_F0+160(sp)
    fsd f21, TF_F0+168(sp)
    fsd f22, TF_F0+176(sp)
    fsd f23, TF_F0+184(sp)
    fsd f24, TF_F0+192(sp)
    fsd f25, TF_F0+200(sp)
    fsd f26, TF_F0+208(sp)
    fsd f27, TF_F0+216(sp)
    fsd f28, TF_F0+224(sp)
    fsd f29, TF_F0+232(sp)
    fsd f30, TF_F0+240(sp)
    fsd f31, TF_F0+248(sp)
    csrr t0, fcsr
    sd t0, TF_FCSR(sp)
1:
    csrr a0, scause
    ld a1, TF_SEPC(sp)
    ld a2, TF_STVAL(sp)
    mv a3, sp
    call trap_handler

    # The handler returns the trap frame to resume. It may belong to a
    # different task after a yield or timer preemption.
    mv sp, a0

    # The C handler may update sepc/sstatus in the explicit frame.
    ld t0, TF_SEPC(sp)
    csrw sepc, t0
    ld t0, TF_SSTATUS(sp)
    csrw sstatus, t0

    # Prepare the privilege-sensitive stack swap for the context selected by
    # the scheduler. SPP=0 means sret will enter U-mode.
    ld t0, TF_SSTATUS(sp)
    andi t0, t0, 0x100
    bnez t0, 3f
    ld t0, TF_RESERVED(sp)
    csrw sscratch, t0
    j 4f
3:
    csrw sscratch, zero
4:

    ld t0, TF_SSTATUS(sp)
    li t1, 0x6000
    and t0, t0, t1
    beqz t0, 2f
    fld f0,  TF_F0+0(sp)
    fld f1,  TF_F0+8(sp)
    fld f2,  TF_F0+16(sp)
    fld f3,  TF_F0+24(sp)
    fld f4,  TF_F0+32(sp)
    fld f5,  TF_F0+40(sp)
    fld f6,  TF_F0+48(sp)
    fld f7,  TF_F0+56(sp)
    fld f8,  TF_F0+64(sp)
    fld f9,  TF_F0+72(sp)
    fld f10, TF_F0+80(sp)
    fld f11, TF_F0+88(sp)
    fld f12, TF_F0+96(sp)
    fld f13, TF_F0+104(sp)
    fld f14, TF_F0+112(sp)
    fld f15, TF_F0+120(sp)
    fld f16, TF_F0+128(sp)
    fld f17, TF_F0+136(sp)
    fld f18, TF_F0+144(sp)
    fld f19, TF_F0+152(sp)
    fld f20, TF_F0+160(sp)
    fld f21, TF_F0+168(sp)
    fld f22, TF_F0+176(sp)
    fld f23, TF_F0+184(sp)
    fld f24, TF_F0+192(sp)
    fld f25, TF_F0+200(sp)
    fld f26, TF_F0+208(sp)
    fld f27, TF_F0+216(sp)
    fld f28, TF_F0+224(sp)
    fld f29, TF_F0+232(sp)
    fld f30, TF_F0+240(sp)
    fld f31, TF_F0+248(sp)
    ld t0, TF_FCSR(sp)
    csrw fcsr, t0
2:
    ld ra, 0(sp)
    ld gp, 16(sp)
    ld tp, 24(sp)
    ld t1, 40(sp)
    ld t2, 48(sp)
    ld s0, 56(sp)
    ld s1, 64(sp)
    ld a0, 72(sp)
    ld a1, 80(sp)
    ld a2, 88(sp)
    ld a3, 96(sp)
    ld a4, 104(sp)
    ld a5, 112(sp)
    ld a6, 120(sp)
    ld a7, 128(sp)
    ld s2, 136(sp)
    ld s3, 144(sp)
    ld s4, 152(sp)
    ld s5, 160(sp)
    ld s6, 168(sp)
    ld s7, 176(sp)
    ld s8, 184(sp)
    ld s9, 192(sp)
    ld s10, 200(sp)
    ld s11, 208(sp)
    ld t3, 216(sp)
    ld t4, 224(sp)
    ld t5, 232(sp)
    ld t6, 240(sp)
    ld t0, 32(sp)
    ld sp, 8(sp)
    sret
