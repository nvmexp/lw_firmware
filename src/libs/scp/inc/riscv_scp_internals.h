/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef RISCV_SCP_INTERNALS_H
#define RISCV_SCP_INTERNALS_H

#include "dev_therm.h"
#include "scp_common.h"
#include "engine.h"
#include <lwriscv/print.h>
#include <syslib/syslib.h>
#include <drivers/drivers.h>
#include <osptimer.h>

 //*****************************************************************************
 // SCP register and DRF macros
 //*****************************************************************************
#if GSP_PRIV_ACCESS
//*****************************************************************************
// GSP RISCV Defines
//*****************************************************************************
#define RV_PRFX(A)                  ((LW_PGSP##A))
#define RV_SCP_REG_PTR(name)        ((unsigned int *)(LW_PGSP_SCP##name))
#define SCP_REG(name)               ((LW_PGSP_SCP##name))
#define RV_SCP_REG(name)            ((LW_PGSP_RISCV##name))
#define DRF_NUM_SCP(r,f,n)          (DRF_NUM(_PGSP, _SCP##r, f, n))
#define DRF_DEF_SCP(r,f,c)          (DRF_DEF(_PGSP, _SCP##r, f, c))
#define DRF_VAL_SCP(r,f,v)          (DRF_VAL(_PGSP, _SCP##r, f, v))
#define FLD_TEST_DRF_SCP(r,f,c,v)   (FLD_TEST_DRF(_PGSP, _SCP##r, f, c, v))
#define RV_SCP_REG_RD32(name)       bar0Read(SCP_REG(name))
// RISCV Specific expanded versions
#define DRF_NUM_RISCV(r,f,n)        (DRF_NUM(_PGSP_RISCV, _SCP##r, f, n))
#define DRF_DEF_RISCV(r,f,c)        (DRF_DEF(_PGSP_RISCV, _SCP##r, f, c))
#define DRF_VAL_RISCV(r,f,v)        (DRF_VAL(_PGSP_RISCV, _SCP##r, f, v))
#define REF_NUM32_RISCV(rf, v)      (REF_NUM32(LW_PGSP_RISCV_SCP##rf, v))
#else  // PMU_PRIV_ACCESS
//*****************************************************************************
// PMU RISCV Defines
//*****************************************************************************
#define RV_PRFX(A)                  ((LW_PPWR##A))
#define RV_SCP_REG_PTR(name)        ((unsigned int *)(LW_PPWR_PMU##name))
#define SCP_REG(name)               ((LW_PPWR_PMU_SCP##name))
#define RV_SCP_REG(name)            ((LW_PPWR_RISCV##name))
#define DRF_NUM_SCP(r,f,n)          (DRF_NUM(_PPWR_PMU, _SCP##r, f, n))
#define DRF_DEF_SCP(r,f,c)          (DRF_DEF(_PPWR_PMU, _SCP##r, f, c))
#define DRF_VAL_SCP(r,f,v)          (DRF_VAL(_PPWR_PMU, _SCP##r, f, v))
#define FLD_TEST_DRF_SCP(r,f,c,v)   (FLD_TEST_DRF(_PPWR, _PMU_SCP##r, f, c, v))
#define RV_SCP_REG_RD32(name)       bar0Read(SCP_REG(name))
// RISCV Specific expanded versions
#define DRF_NUM_RISCV(r,f,n)        (DRF_NUM(_PPWR_RISCV, _SCP##r, f, n))
#define DRF_DEF_RISCV(r,f,c)        (DRF_DEF(_PPWR_RISCV, _SCP##r, f, c))
#define DRF_VAL_RISCV(r,f,v)        (DRF_VAL(_PPWR_RISCV, _SCP##r, f, v))
#define REF_NUM32_RISCV(rf, v)      (REF_NUM32(LW_PPWR_RISCV_SCP##rf, v))
#endif

//*****************************************************************************
// SCP CCI intrinsics
//*****************************************************************************
/*
#define SCP_OP_ENUM_DECL enum SCP_OP {nop = 0, mov = 1, push = 2, fetch = 3, rand = 4, load_trace0 = 5, loop_trace0 = 6, load_trace1 = 7, loop_trace1 = 8, chmod = 10, xor = 11, add = 12, and = 13, bswap = 14, cmac_sk = 15, secret = 16, key = 17, rkey10 = 18, rkey1 = 19, encrypt = 20, decrypt = 21, compare_sig = 22, encrypt_sig = 23, forget_sig = 24};
#define  ENUM_SCP_OP_nop 0
#define  ENUM_SCP_OP_mov 1
#define  ENUM_SCP_OP_push 2
#define  ENUM_SCP_OP_fetch 3
#define  ENUM_SCP_OP_rand 4
#define  ENUM_SCP_OP_load_trace0 5
#define  ENUM_SCP_OP_loop_trace0 6
#define  ENUM_SCP_OP_load_trace1 7
#define  ENUM_SCP_OP_loop_trace1 8
#define  ENUM_SCP_OP_chmod 10
#define  ENUM_SCP_OP_xor 11
#define  ENUM_SCP_OP_add 12
#define  ENUM_SCP_OP_and 13
#define  ENUM_SCP_OP_bswap 14
#define  ENUM_SCP_OP_cmac_sk 15
#define  ENUM_SCP_OP_secret 16
#define  ENUM_SCP_OP_key 17
#define  ENUM_SCP_OP_rkey10 18
#define  ENUM_SCP_OP_rkey1 19
#define  ENUM_SCP_OP_encrypt 20
#define  ENUM_SCP_OP_decrypt 21
#define  ENUM_SCP_OP_compare_sig 22
#define  ENUM_SCP_OP_encrypt_sig 23
#define  ENUM_SCP_OP_forget_sig 24
*/

#define SCP_R0         (0x0000)
#define SCP_R1         (0x0001)
#define SCP_R2         (0x0002)
#define SCP_R3         (0x0003)
#define SCP_R4         (0x0004)
#define SCP_R5         (0x0005)
#define SCP_R6         (0x0006)
#define SCP_R7         (0x0007)
#define SCP_CCI_MASK   (0x8000)


#define RD_RISCV_SCP_REG32(reg) (*(volatile unsigned int *)(reg))
#define WR_RISCV_SCP_REG32(reg,wrVal) (*(volatile unsigned int *)(reg) = (wrVal))

#define FIELD_SHIFT32(regField) ((0?regField)%32)
#define FIELD_MASK32(regField) ((0xFFFFFFFF >> (31-(1?regField)%32 + (0?regField)%32)) << (0?regField)%32)
#define REF_VAL32(regField,val) (((val) & FIELD_MASK32(regField)) >> FIELD_SHIFT32(regField))
#define REF_NUM32(regField,num) (((num) << FIELD_SHIFT32(regField)) & FIELD_MASK32(regField))
#define WR_FIELD32(reg,regField,num) WR_RISCV_SCP_REG32(reg,(RD_RISCV_SCP_REG32(reg) & (~FIELD_MASK32(regField))) | REF_NUM32(regField,num))


// MMINTS-TODO: 500 ms for now, re-evalute timeout to a more reasonable value later.
#define LW_SCP_DMA_WAIT_TIMEOUT_MS (500U)
#define LW_SCP_DMA_WAIT_TIMEOUT_US (LW_SCP_DMA_WAIT_TIMEOUT_MS * 1000U)

//*****************************************************************************
// memory routines
//*****************************************************************************
#define DMSIZE_4B       (0x0000)
#define DMSIZE_8B       (0x0001)
#define DMSIZE_16B      (0x0002)
#define DMSIZE_32B      (0x0003)
#define DMSIZE_64B      (0x0004)
#define DMSIZE_128B     (0x0005)
#define DMSIZE_256B     (0x0006)


static inline void CCI_2_SCP_DMA_TRF_CMD(const unsigned int opcode, const unsigned int Rx_or_imm, const unsigned int Ry) {
    unsigned int wrVal = 0;
    wrVal = (REF_NUM32_RISCV(DMATRFCMD_CCI_EX, RV_PRFX(_RISCV_SCPDMATRFCMD_CCI_EX_CCI)) |
        REF_NUM32_RISCV(DMATRFCMD_OPCODE, opcode) |
        REF_NUM32_RISCV(DMATRFCMD_RX_OR_IMM, Rx_or_imm) |
        REF_NUM32_RISCV(DMATRFCMD_RY, Ry)
        );
    bar0Write(RV_PRFX(_RISCV_SCPDMATRFCMD), wrVal);
}

static inline void RISCV_SCP_DMA_TRF_CMD(const unsigned int offset, const unsigned int suppress, const unsigned int shortlwt, const unsigned int target, const unsigned int direction, const unsigned int size_or_gpr) {
    unsigned int wrVal = 0;
    wrVal = (REF_NUM32_RISCV(DMATRFCMD_CCI_EX, RV_PRFX(_RISCV_SCPDMATRFCMD_CCI_EX_SCPDMA)) |
        REF_NUM32_RISCV(DMATRFCMD_SUPPRESS, suppress) |
        REF_NUM32_RISCV(DMATRFCMD_SHORTLWT, shortlwt) |
        REF_NUM32_RISCV(DMATRFCMD_IMEM, target) |
        REF_NUM32_RISCV(DMATRFCMD_WRITE, direction) |
        REF_NUM32_RISCV(DMATRFCMD_GPR, size_or_gpr)
        );
    bar0Write(RV_PRFX(_RISCV_SCPDMATRFMOFFS), REF_NUM32_RISCV(DMATRFMOFFS_OFFS, offset));
    bar0Write(RV_PRFX(_RISCV_SCPDMATRFCMD), wrVal);
}

// rx/ry : symbolic Src/Destn

static inline void riscv_scp_rand(LwU32 ry) {
    CCI_2_SCP_DMA_TRF_CMD(RV_PRFX(_RISCV_SCPDMATRFCMD_OPCODE_RAND), 0, ry);
}

static inline void riscv_scp_xor(LwU32 rx, LwU32 ry) {
    CCI_2_SCP_DMA_TRF_CMD(RV_PRFX(_RISCV_SCPDMATRFCMD_OPCODE_XOR), rx, ry);
}

static inline void riscv_scp_key(LwU32 ry) {
    CCI_2_SCP_DMA_TRF_CMD(RV_PRFX(_RISCV_SCPDMATRFCMD_OPCODE_KEY), 0, ry);
}

static inline void riscv_scp_encrypt(LwU32 rx, LwU32 ry) {
    CCI_2_SCP_DMA_TRF_CMD(RV_PRFX(_RISCV_SCPDMATRFCMD_OPCODE_ENCRYPT), rx, ry);
}

static inline void riscv_scp_mov(LwU32 rx, LwU32 ry) {
    CCI_2_SCP_DMA_TRF_CMD(RV_PRFX(_RISCV_SCPDMATRFCMD_OPCODE_MOV), rx, ry);
}

static inline void riscv_scp_rkey10(LwU32 rx, LwU32 ry) {
    CCI_2_SCP_DMA_TRF_CMD(RV_PRFX(_RISCV_SCPDMATRFCMD_OPCODE_RKEY10), rx, ry);
}

static inline void riscv_scp_load_trace0(LwU32 imm) {
    CCI_2_SCP_DMA_TRF_CMD(RV_PRFX(_RISCV_SCPDMATRFCMD_OPCODE_LOAD_TRACE0), imm, 0);
}

static inline void riscv_scp_push(LwU32 ry) {
    CCI_2_SCP_DMA_TRF_CMD(RV_PRFX(_RISCV_SCPDMATRFCMD_OPCODE_PUSH), 0, ry);
}

static inline void riscv_scp_decrypt(LwU32 rx, LwU32 ry) {
    CCI_2_SCP_DMA_TRF_CMD(RV_PRFX(_RISCV_SCPDMATRFCMD_OPCODE_DECRYPT), rx, ry);
}

static inline void riscv_scp_fetch(LwU32 ry) {
    CCI_2_SCP_DMA_TRF_CMD(RV_PRFX(_RISCV_SCPDMATRFCMD_OPCODE_FETCH), 0, ry);
}


static inline void riscv_scp_loop_trace0(LwU32 imm) {
    CCI_2_SCP_DMA_TRF_CMD(RV_PRFX(_RISCV_SCPDMATRFCMD_OPCODE_LOOP_TRACE0), imm, 0);
}


// Non Suppress Reads/Writes have fixed size transfers of 16 Bytes
//*****************************************************************************
// dmwrite copies 16 bytes from DMEM to SCP(RISCV trasactions are always trapped)
//*****************************************************************************
static inline void riscv_dmwrite(LwU32 addr, LwU32 gpr)
{
    RISCV_SCP_DMA_TRF_CMD(addr, RV_PRFX(_RISCV_SCPDMATRFCMD_SUPPRESS_DISABLE), RV_PRFX(_RISCV_SCPDMATRFCMD_SHORTLWT_DISABLE), RV_PRFX(_RISCV_SCPDMATRFCMD_IMEM_FALSE), RV_PRFX(_RISCV_SCPDMATRFCMD_WRITE_TRUE), gpr);
}

//*****************************************************************************
// dmread copies 16 bytes from SCP to DMEM(RISCV trasactions are always trapped)
//*****************************************************************************
static inline void riscv_dmread(LwU32 addr, LwU32 gpr) {
    RISCV_SCP_DMA_TRF_CMD(addr, RV_PRFX(_RISCV_SCPDMATRFCMD_SUPPRESS_DISABLE), RV_PRFX(_RISCV_SCPDMATRFCMD_SHORTLWT_DISABLE), RV_PRFX(_RISCV_SCPDMATRFCMD_IMEM_FALSE), RV_PRFX(_RISCV_SCPDMATRFCMD_WRITE_FALSE), gpr);
}

static inline void riscv_read_imem(LwU32 addr, LwU32 gpr) {
    RISCV_SCP_DMA_TRF_CMD(addr, RV_PRFX(_RISCV_SCPDMATRFCMD_SUPPRESS_DISABLE), RV_PRFX(_RISCV_SCPDMATRFCMD_SHORTLWT_DISABLE), RV_PRFX(_RISCV_SCPDMATRFCMD_IMEM_TRUE), RV_PRFX(_RISCV_SCPDMATRFCMD_WRITE_FALSE), gpr);
}

static inline void riscv_write_imem(LwU32 addr, LwU32 gpr) {
    RISCV_SCP_DMA_TRF_CMD(addr, RV_PRFX(_RISCV_SCPDMATRFCMD_SUPPRESS_DISABLE), RV_PRFX(_RISCV_SCPDMATRFCMD_SHORTLWT_DISABLE), RV_PRFX(_RISCV_SCPDMATRFCMD_IMEM_TRUE), RV_PRFX(_RISCV_SCPDMATRFCMD_WRITE_TRUE), gpr);
}

static inline void riscv_write_dmem(LwU32 addr, LwU32 gpr) {
    RISCV_SCP_DMA_TRF_CMD(addr, RV_PRFX(_RISCV_SCPDMATRFCMD_SUPPRESS_DISABLE), RV_PRFX(_RISCV_SCPDMATRFCMD_SHORTLWT_DISABLE), RV_PRFX(_RISCV_SCPDMATRFCMD_IMEM_FALSE), RV_PRFX(_RISCV_SCPDMATRFCMD_WRITE_TRUE), gpr);
}

static inline void riscv_read_imem_suppress(LwU32 addr, LwU32 size) {
    RISCV_SCP_DMA_TRF_CMD(addr, RV_PRFX(_RISCV_SCPDMATRFCMD_SUPPRESS_ENABLE), RV_PRFX(_RISCV_SCPDMATRFCMD_SHORTLWT_DISABLE), RV_PRFX(_RISCV_SCPDMATRFCMD_IMEM_TRUE), RV_PRFX(_RISCV_SCPDMATRFCMD_WRITE_FALSE), size);
}

static inline void riscv_read_dmem_suppress(LwU32 addr, LwU32 size) {
    RISCV_SCP_DMA_TRF_CMD(addr, RV_PRFX(_RISCV_SCPDMATRFCMD_SUPPRESS_ENABLE), RV_PRFX(_RISCV_SCPDMATRFCMD_SHORTLWT_DISABLE), RV_PRFX(_RISCV_SCPDMATRFCMD_IMEM_FALSE), RV_PRFX(_RISCV_SCPDMATRFCMD_WRITE_FALSE), size);
}

static inline void riscv_write_imem_suppress(LwU32 addr, LwU32 size) {
    RISCV_SCP_DMA_TRF_CMD(addr, RV_PRFX(_RISCV_SCPDMATRFCMD_SUPPRESS_ENABLE), RV_PRFX(_RISCV_SCPDMATRFCMD_SHORTLWT_DISABLE), RV_PRFX(_RISCV_SCPDMATRFCMD_IMEM_TRUE), RV_PRFX(_RISCV_SCPDMATRFCMD_WRITE_TRUE), size);
}

static inline void riscv_write_dmem_suppress(LwU32 addr, LwU32 size) {
    RISCV_SCP_DMA_TRF_CMD(addr, RV_PRFX(_RISCV_SCPDMATRFCMD_SUPPRESS_ENABLE), RV_PRFX(_RISCV_SCPDMATRFCMD_SHORTLWT_DISABLE), RV_PRFX(_RISCV_SCPDMATRFCMD_IMEM_FALSE), RV_PRFX(_RISCV_SCPDMATRFCMD_WRITE_TRUE), size);
}

static inline void riscv_read_dmem_shortlwt(LwU32 addr, LwU32 gpr) {
    RISCV_SCP_DMA_TRF_CMD(addr, RV_PRFX(_RISCV_SCPDMATRFCMD_SUPPRESS_DISABLE), RV_PRFX(_RISCV_SCPDMATRFCMD_SHORTLWT_ENABLE), RV_PRFX(_RISCV_SCPDMATRFCMD_IMEM_FALSE), RV_PRFX(_RISCV_SCPDMATRFCMD_WRITE_FALSE), gpr);
}

static inline void riscv_read_dmem_suppress_shortlwt(LwU32 addr, LwU32 size) {
    RISCV_SCP_DMA_TRF_CMD(addr, RV_PRFX(_RISCV_SCPDMATRFCMD_SUPPRESS_ENABLE), RV_PRFX(_RISCV_SCPDMATRFCMD_SHORTLWT_ENABLE), RV_PRFX(_RISCV_SCPDMATRFCMD_IMEM_FALSE), RV_PRFX(_RISCV_SCPDMATRFCMD_WRITE_FALSE), size);
}

static inline void riscv_scpWait(void)
{
    if (!OS_PTIMER_SPIN_WAIT_US_COND(scpDmaPollIsIdle, NULL, LW_SCP_DMA_WAIT_TIMEOUT_US))
    {
        dbgPrintf(LEVEL_ALWAYS, "Timed out waiting on SCP operation!\n");

        if (lwrtosIS_KERNEL())
        {
            kernelVerboseCrash(0);
        }
        else
        {
            lwrtosHALT();
        }
    }
}

#define LW_SCP_CMN_RISCV_SCPDMATRFCMD                                                                       0xFFFFFFFF     /* RWI4R */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD__DEVICE_MAP                                                           0x00000010     /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD__PRIV_LEVEL_MASK                                                      0x1101b8       /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_SUPPRESS                                                              2:2            /* RWIVF */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_SUPPRESS_DISABLE                                                      0x00000000     /* RWI-V */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_SUPPRESS_ENABLE                                                       0x00000001     /* RW--V */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_SHORTLWT                                                              3:3            /* RWIVF */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_SHORTLWT_DISABLE                                                      0x00000000     /* RWI-V */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_SHORTLWT_ENABLE                                                       0x00000001     /* RW--V */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_IMEM                                                                  4:4            /* RWIVF */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_IMEM_FALSE                                                            0x00000000     /* RWI-V */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_IMEM_TRUE                                                             0x00000001     /* RW--V */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_WRITE                                                                 5:5            /* RWIVF */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_WRITE_FALSE                                                           0x00000000     /* RWI-V */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_WRITE_TRUE                                                            0x00000001     /* RW--V */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_GPR                                                                   8:6            /* RWIVF */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_GPR_INIT                                                              0x00000000     /* RWI-V */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_SIZE                                                                  8:6            /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_SIZE_16B                                                              2              /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_SIZE_32B                                                              3              /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_SIZE_64B                                                              4              /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_SIZE_128B                                                             5              /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_SIZE_256B                                                             6              /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_CCI_EX                                                                15:15          /* RWIVF */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_CCI_EX_SCPDMA                                                         0x00000000     /* RWI-V */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_CCI_EX_CCI                                                            0x00000001     /* RW--V */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_CCI_CMD                                                               31:16          /* RWIVF */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_CCI_CMD_INIT                                                          0x00000000     /* RWI-V */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_RY                                                                    19:16          /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_RX_OR_IMM                                                             25:20          /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE                                                                31:26          /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_NOP                                                            0              /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_MOV                                                            1              /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_PUSH                                                           2              /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_FETCH                                                          3              /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_RAND                                                           4              /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_LOAD_TRACE0                                                    5              /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_LOOP_TRACE0                                                    6              /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_LOAD_TRACE1                                                    7              /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_LOOP_TRACE1                                                    8              /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_CHMOD                                                          10             /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_XOR                                                            11             /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_ADD                                                            12             /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_AND                                                            13             /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_BSWAP                                                          14             /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_CMAC_SK                                                        15             /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_SECRET                                                         16             /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_KEY                                                            17             /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_RKEY10                                                         18             /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_RKEY1                                                          19             /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_ENCRYPT                                                        20             /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_DECRYPT                                                        21             /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_COMPARE_SIG                                                    22             /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_ENCRYPT_SIG                                                    23             /*       */
#define LW_SCP_CMN_RISCV_SCPDMATRFCMD_OPCODE_FORGET_SIG                                                     24             /*       */

#endif // RISCV_SCP_INTERNALS_H
