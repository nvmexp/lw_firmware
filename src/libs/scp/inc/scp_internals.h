/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SCP_INTERNALS_H
#define SCP_INTERNALS_H

#if SEC2_CSB_ACCESS
#include "dev_sec_csb.h"
#elif SOE_CSB_ACCESS
#include "dev_soe_csb.h"
#elif GSPLITE_CSB_ACCESS
#include "dev_gsp_csb.h"
#elif PMU_CSB_ACCESS
#include "dev_pwr_csb.h"
#else
// Nothing for now. Only SEC2 is using the library today.
#endif

#include "csb.h"
#include "scp_common.h"

//*****************************************************************************
// SCP register and DRF macros
//*****************************************************************************
#if SEC2_CSB_ACCESS

#define SCP_REG(name) ((unsigned int *)(LW_CSEC_SCP##name))
#define DRF_NUM_SCP(r,f,n) (DRF_NUM(_CSEC, _SCP##r, f, n))
#define DRF_DEF_SCP(r,f,c) (DRF_DEF(_CSEC, _SCP##r, f, c))
#define DRF_VAL_SCP(r,f,v) (DRF_VAL(_CSEC, _SCP##r, f, v))
#define FLD_TEST_DRF_SCP(r,f,c,v) (FLD_TEST_DRF(_CSEC, _SCP##r, f, c, v))

#define SCP_REG_RD32(name) REG_RD32(CSB, SCP_REG(name))

#elif SOE_CSB_ACCESS
#define SCP_REG(name) ((unsigned int *)(LW_CSOE_SCP##name))
#define DRF_NUM_SCP(r,f,n) (DRF_NUM(_CSOE, _SCP##r, f, n))
#define DRF_DEF_SCP(r,f,c) (DRF_DEF(_CSOE, _SCP##r, f, c))
#define DRF_VAL_SCP(r,f,v) (DRF_VAL(_CSOE, _SCP##r, f, v))
#define FLD_TEST_DRF_SCP(r,f,c,v) (FLD_TEST_DRF(_CSOE, _SCP##r, f, c, v))

#define SCP_REG_RD32(name) REG_RD32(CSB, SCP_REG(name))

#elif GSPLITE_CSB_ACCESS

#define SCP_REG(name) ((unsigned int *)(LW_CGSP_SCP##name))
#define DRF_NUM_SCP(r,f,n) (DRF_NUM(_CGSP, _SCP##r, f, n))
#define DRF_DEF_SCP(r,f,c) (DRF_DEF(_CGSP, _SCP##r, f, c))
#define DRF_VAL_SCP(r,f,v) (DRF_VAL(_CGSP, _SCP##r, f, v))
#define FLD_TEST_DRF_SCP(r,f,c,v) (FLD_TEST_DRF(_CGSP, _SCP##r, f, c, v))

#define SCP_REG_RD32(name) REG_RD32(CSB, SCP_REG(name))

#elif PMU_CSB_ACCESS

#define SCP_REG(name) ((unsigned int *)(LW_CPWR_PMU_SCP##name))
#define DRF_NUM_SCP(r,f,n) (DRF_NUM(_CPWR_PMU, _SCP##r, f, n))
#define DRF_DEF_SCP(r,f,c) (DRF_DEF(_CPWR_PMU, _SCP##r, f, c))
#define DRF_VAL_SCP(r,f,v) (DRF_VAL(_CPWR_PMU, _SCP##r, f, v))
#define FLD_TEST_DRF_SCP(r,f,c,v) (FLD_TEST_DRF(_CPWR_PMU, _SCP##r, f, c, v))

#define SCP_REG_RD32(name) REG_RD32(CSB, SCP_REG(name))

#else
// Nothing for now. Only SEC2 is using the library today.
#endif

//*****************************************************************************
// SCP CCI intrinsics
//*****************************************************************************
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

#define SCP_R0         (0x0000)
#define SCP_R1         (0x0001)
#define SCP_R2         (0x0002)
#define SCP_R3         (0x0003)
#define SCP_R4         (0x0004)
#define SCP_R5         (0x0005)
#define SCP_R6         (0x0006)
#define SCP_R7         (0x0007)
#define SCP_CCI_MASK   (0x8000)

static inline void falc_scp_encrypt(LwU32 rx, LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_encrypt << 10) | (rx << 4) | ry));
}
static inline void falc_scp_key(LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_key << 10) | ry));
}
static inline void falc_scp_secret(LwU32 imm, LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_secret << 10) | (imm << 4) | ry));
}
static inline void falc_scp_xor(LwU32 rx, LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_xor << 10) | (rx << 4) | ry));
}
static inline void falc_scp_rand(LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_rand << 10) | ry));
}
static inline void falc_scp_mov(LwU32 rx, LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_mov << 10) | (rx << 4) | ry));
}
static inline void falc_scp_xor_r(LwU32 rx, LwU32 ry) {
    asm volatile("cci %0 %1;" : : "r" ((ry << 8) | rx), "i" (ENUM_SCP_OP_xor));
}
static inline void falc_scp_rkey10(LwU32 rx, LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_rkey10 << 10) | (rx << 4) | ry));
}
static inline void falc_scp_load_trace0(LwU32 imm) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_load_trace0 << 10) | (imm << 4)));
}
static inline void falc_scp_load_trace1(LwU32 imm) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_load_trace1 << 10) | (imm << 4)));
}
static inline void falc_scp_loop_trace0(LwU32 imm) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_loop_trace0 << 10) | (imm << 4)));
}
static inline void falc_scp_loop_trace1(LwU32 imm) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_loop_trace1 << 10) | (imm << 4)));
}
static inline void falc_scp_loop_trace0_r(LwU32 loop_count) {
    asm volatile("cci %0 %1;" : : "r" (loop_count), "i" (ENUM_SCP_OP_loop_trace0));
}
static inline void falc_scp_loop_trace1_r(LwU32 loop_count) {
    asm volatile("cci %0 %1;" : : "r" (loop_count), "i" (ENUM_SCP_OP_loop_trace1));
}
static inline void falc_scp_push(LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_push << 10) | ry));
}
static inline void falc_scp_fetch(LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_fetch << 10) | ry));
}
static inline void falc_scp_decrypt(LwU32 rx, LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_decrypt << 10) | (rx << 4) | ry));
}
static inline void falc_scp_add(LwU32 imm, LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_add << 10) | (imm << 4) | ry));
}
static inline void falc_scp_forget_sig(void) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | ENUM_SCP_OP_forget_sig << 10));
}

//*****************************************************************************
// SCP CCR intrinsics
//*****************************************************************************
#define CCR_SUPPRESSED (0x0020)
#define CCR_SHORTLWT   (0x0040)
#define CCR_MT_IMEM    (0x0080)
#define TC_INFINITY    (0x001f)

static inline void falc_scp_trap(LwU32 tc) {
    asm volatile("cci %0;" : : "i" (tc));
}
static inline void falc_scp_trap_suppressed(LwU32 tc) {
    asm volatile("cci %0;" : : "i" (CCR_SUPPRESSED | tc));
}
static inline void falc_scp_trap_shortlwt(LwU32 tc) {
    asm volatile("cci %0;" : : "i" (CCR_SHORTLWT | CCR_SUPPRESSED | tc));
}
static inline void falc_scp_trap_imem(LwU32 tc) {
    asm volatile("cci %0;" : : "i" (CCR_MT_IMEM | tc));
}
static inline void falc_scp_trap_imem_suppressed(LwU32 tc) {
    asm volatile("cci %0;" : : "i" (CCR_MT_IMEM | CCR_SUPPRESSED | tc));
}
static inline void falc_scp_encrypt_sig(LwU32 rx, LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_encrypt_sig << 10) | (rx << 4) | ry));
}


//*****************************************************************************
// trapped dmwrite copies 16 bytes from DMEM to SCP
//*****************************************************************************
static inline void falc_trapped_dmwrite(LwU32 addr)
{
    asm volatile("dmwrite %0 %0;" :: "r"(addr));
}

//*****************************************************************************
// trapped dmread copies 16 bytes from SCP to DMEM
//*****************************************************************************
static inline void falc_trapped_dmread(LwU32 addr)
{
    asm volatile("dmread %0 %0;" :: "r"(addr));
}

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

#endif // SCP_INTERNALS_H
