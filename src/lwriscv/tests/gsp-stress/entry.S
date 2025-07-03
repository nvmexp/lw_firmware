/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <lwriscv/asm-helpers.h>
#include <dev_riscv_csr_64.h>

.macro inlineIcd reg
    li      \reg, 0x012813d0
    sw      zero, 0(\reg)
    fence   io,io
.endm

/* This code runs in M mode unlike other files */
FUNC app_entry
    // write mtvec and make sure it sticks
    la    t0, app_panic
    csrw  stvec, t0

    csrw sscratch, a0 // This is fb base of ucode (not fmc)

    jal app_prepare_image

    // relocate code
    la t1, identity_start // start of our carveout
    ld t1, 0(t1)

    csrr a0, sscratch // fb base
    la t0, relocated
    sub t0, t0, a0
    add t0, t0, t1
    jr t0
relocated:

    jal app_prepare

    // Zero exception stack
    la    sp, __exception_stack_top - 0x10
    sd    zero, 0(sp)
    sd    zero, 8(sp)

    // Zero ordinary stack
    la    sp, __stack_top - 0x10
    sd    zero, 0(sp)
    sd    zero, 8(sp)

    // setup nice exception vector
    la    t0, app_exception_entry
    csrw  stvec, t0

// MK TODO: why long jump?
    la t0, main
    jalr   t0

1:
    jal app_icd
    j 1b
EFUNC app_entry

#define MPU_ATTR_RWX (0x7 | (0x7 << 3))
#define MPU_ATTR_RX (0x5 | (0x5 << 3))
#define MPU_ATTR_RW (0x3 | (0x3 << 3))
#define MPU_ATTR_CACHE ((1 << 18) | (1 << 19))

.macro  MPU_RGN idx, va, pa, range, attr
    .dword \idx
    .dword \va
    .dword \pa
    .dword \range
    .dword \attr
.endm

.balign 8
.global mpu_initial_mappings
mpu_initial_mappings:
    MPU_RGN 0,           0x100001,           0x100000,  LWRISCV_IMEM_SIZE, MPU_ATTR_RWX
zmpu_dmem:
    MPU_RGN 1,           0x180001,           0x180000,  LWRISCV_DMEM_SIZE, MPU_ATTR_RW
              zmpu_emem:
    MPU_RGN 2,          0x1200001,          0x1200000,  LWRISCV_EMEM_SIZE, MPU_ATTR_RW
              zmpu_local:
    MPU_RGN 3,          0x1280001,          0x1280000,           0x200000, MPU_ATTR_RW
              zmpu_pri:
    MPU_RGN 4, 0x2000000000000001, 0x2000000000000000,          0x4000000, MPU_ATTR_RW
              zmpu_fbpa:
    MPU_RGN 5, 0x6060000000000001, 0x6060000000000000, 0x0020000000000000, (MPU_ATTR_RWX | MPU_ATTR_CACHE)
    // Placeholder mappings, PA is to be patched
    // Code - FB
  zmpu_code:
    MPU_RGN 8, 0xA00001, 0, __code_size, (MPU_ATTR_RWX | MPU_ATTR_CACHE)
  // Code - imem copy
  zmpu_codei:
    MPU_RGN 9, 0xA00001, 0, __code_size, (MPU_ATTR_RWX)
    // Data - FB
  zmpu_data:
    MPU_RGN 10, 0xB00001, 0, __data_size, (MPU_ATTR_RW | MPU_ATTR_CACHE)
    // DataFB
  zmpu_datafb:
    MPU_RGN 11, 0xC00001, 0, __data_fb_size, (MPU_ATTR_RW | MPU_ATTR_CACHE)
.size mpu_initial_mappings, .-mpu_initial_mappings
mpu_initial_mappings_end:

// some pre-relocation variables
.balign 8
identity_start:
    .dword __identity_copy_start
identity_size:
    .dword __identity_size
// Copy identity image and enable MPU
FUNC app_prepare_image

    // we must load identity address indirect, otherwise we get pc-relative addressing
    mv t0, a0 // src - fb base
    la t1, identity_start // destination, start of our carveout
    ld t1, 0(t1)
    la t2, identity_size // size
    ld t2, 0(t2)
1:
    ld s0, 0(t0)
    sd s0, 0(t1)
    addi t0, t0, 8
    addi t1, t1, 8
    addi t2, t2, -8
    bnez t2, 1b

// Write MPU entries (identity + io + pa)
    la t0, mpu_initial_mappings
    la t1, mpu_initial_mappings_end
1:
    ld s0, 0x0(t0)
    csrw LW_RISCV_CSR_SMPUIDX, s0
    ld s0, 0x8(t0)
    csrw LW_RISCV_CSR_SMPUVA, s0
    ld s0, 0x10(t0)
    csrw LW_RISCV_CSR_SMPUPA, s0
    ld s0, 0x18(t0)
    csrw LW_RISCV_CSR_SMPURNG, s0
    ld s0, 0x20(t0)
    csrw LW_RISCV_CSR_SMPUATTR, s0
    addi t0, t0, 5 * 8
    bne t0, t1, 1b

    // enable MPU
    li t0, 0xF
    slli t0, t0, 60
    csrw LW_RISCV_CSR_SATP, t0
    fence.i
    sfence.vma
    ret
EFUNC app_prepare_image

    // A0 - offset of code
FUNC app_prepare

    mv s0, a0
    la t0, __identity_size
    add s0, s0, t0
    // patch entries 8-11

    // code va / fb
    li t0, 8
    csrw LW_RISCV_CSR_SMPUIDX, t0
    csrw LW_RISCV_CSR_SMPUPA, s0

    // code va / imem
    li t0, 9
    csrw LW_RISCV_CSR_SMPUIDX, t0
    la t0, __identity_carveout_end
    csrw LW_RISCV_CSR_SMPUPA, t0

    la t0, __code_size
    add s0, s0, t0

    // data va / fb
    li t0, 10
    csrw LW_RISCV_CSR_SMPUIDX, t0
    csrw LW_RISCV_CSR_SMPUPA, s0

    la t0, __data_size
    add s0, s0, t0

    // datafb va / fb
    li t0, 11
    csrw LW_RISCV_CSR_SMPUIDX, t0
    csrw LW_RISCV_CSR_SMPUPA, s0

    fence.i
    sfence.vma

    // now copy rest of code from fb to imem
    li t0, 0xA00000
    la t1, __identity_carveout_end
    la t2, __code_size
1:
    ld s0, 0(t0)
    sd s0, 0(t1)
    addi t0, t0, 8
    addi t1, t1, 8
    addi t2, t2, -8
    bnez t2, 1b

    // t1 is first block of free imem, save that address
    la t0, g_beginOfImemFreeSpace
    sd t1, 0(t0)

    // setup
    ret
EFUNC app_prepare

.balign 16
FUNC app_exception_entry
    la  t0, app_icd
    csrw stvec, t0

    la    sp, __exception_stack_top - 0x10

    // MK TODO: why long jump?
    la t0, appException
    jalr t0

    j app_icd
EFUNC app_exception_entry

.balign 16
FUNC app_icd
    /* ICD stop */
    li t0, 0x1
    slli t0, t0, 61
    lui t1, 0x111       /* 0x111... -> which RISCV */
    addi t1, t1, 0x3d0 /* 0x...3d0 -> GA10x GSP ICD */
    add t0, t0, t1
    sw zero, 0(t0)
    fence
    j app_panic
EFUNC app_icd

.balign 16
FUNC app_halt
/* If we are effed we should loop in panic. */
    la t0, app_panic
    csrw  stvec, t0

/* store crash CSRs in DMEM */
    lui t1, 0x00180
    addi t1, t1, 0x10
    csrr t0, scause
    sd t0, 0(t1)
    csrr t0, 0x7d4 /*mcause2 on GA10x*/
    sd t0, 8(t1)
    csrr t0, sepc
    sd t0, 16(t1)
    csrr t0, stval
    sd t0, 24(t1)
EFUNC app_halt

.balign 16
FUNC app_panic
1:
    j 1b
EFUNC app_panic
