
/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file:  fubScpDefines.h
 * @brief: Defines for generating Random Number from SCP are in this file.
 *         All definations are copied from '/uproc/libs/scp/inc/scp_internals.h'.
 */

#ifndef _FUB_SCP_DEFINES_H_
#define _FUB_SCP_DEFINES_H_

/* ------------------------ Defines --------------------------------------- */
//
// Theoretically these parameters should be characterized on silicon and may change for
// each generation. However SCP RNG has many configuration options and no one would like
// to characterize all the possibilities and pick the best one. SW therefore uses the
// option which was recommended by HW for an earlier generation.
// These values should be good for Turing too(even though not the best). Ideally we should
// generate random numbers with this option and compare against NIST test suite.
// However if we boost performance by doing some post processing e.g. generating 2 conselwtive
// random numbers and encrypting one with the other to derive the final random number, we
// should be good.
//
#define SCP_HOLDOFF_INIT_LOWER_VAL (0x7fff)
#define SCP_HOLDOFF_INTRA_MASK     (0x03ff)
#define SCP_AUTOCAL_STATIC_TAPA    (0x000f)
#define SCP_AUTOCAL_STATIC_TAPB    (0x000f)

// SCP CCR intrinsics
#define TC_INFINITY                (0x001f)

// SCP CCI intrinsics
#define  ENUM_SCP_OP_xor           (11)
#define  ENUM_SCP_OP_key           (17)
#define  ENUM_SCP_OP_encrypt_sig   (23)
#define  ENUM_SCP_OP_rand          (4)
#define  ENUM_SCP_OP_encrypt       (20)
#define  ENUM_SCP_OP_forget_sig    (24)

#define SCP_R0                     (0x0000)
#define SCP_R1                     (0x0001)
#define SCP_R2                     (0x0002)
#define SCP_R3                     (0x0003)
#define SCP_R4                     (0x0004)
#define SCP_R5                     (0x0005)
#define SCP_R6                     (0x0006)
#define SCP_R7                     (0x0007)
#define SCP_CCI_MASK               (0x8000)

static inline void falc_scp_trap(LwU32 tc) {
    asm volatile("cci %0;" : : "i" (tc));
}
static inline void falc_scp_xor(LwU32 rx, LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_xor << 10) | (rx << 4) | ry));
}
static inline void falc_scp_key(LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_key << 10) | ry));
}
static inline void falc_scp_rand(LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_rand << 10) | ry));
}
static inline void falc_scp_encrypt(LwU32 rx, LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_encrypt << 10) | (rx << 4) | ry));
}
// trapped dmread copies 16 bytes from SCP to DMEM
static inline void falc_trapped_dmread(LwU32 addr)
{
    asm volatile("dmread %0 %0;" :: "r"(addr));
}

// trapped dmwrite copies 16 bytes from DMEM to SCP
static inline void falc_trapped_dmwrite(LwU32 addr)
{
    asm volatile("dmwrite %0 %0;" :: "r"(addr));
}

static inline void falc_scp_forget_sig(void) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_forget_sig << 10)));
}

#endif //_FUB_SCP_DEFINES_H_
