/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef ACRUTILS_H
#define ACRUTILS_H

#include <lwtypes.h>
#include <lwmisc.h>

//
// BAR0/CSB register access macros
// Replaced all CSB non blocking call with blocking calls
//
#define ACR_CSB_REG_RD32(addr)                      ACR_CSB_REG_RD32_STALL(addr)
#define ACR_CSB_REG_WR32(addr, data)                ACR_CSB_REG_WR32_STALL(addr, data)
#define ACR_CSB_REG_RD32_STALL(addr)                falc_ldxb_i_with_halt(addr)
#define ACR_CSB_REG_WR32_STALL(addr, data)          falc_stxb_i_with_halt(addr, data)
#define ACR_CSB_REG_RD32_INLINE(addr)               _acrReadCsbInlineWithPriErrHandling(addr)
#define ACR_CSB_REG_WR32_INLINE(addr, data)         _acrWriteCsbInlineWithPriErrHandling(addr, data)


#ifdef DMEM_APERT_ENABLED

#define ACR_BAR0_REG_RD32(addr)                     acrlibBar0RegReadDmemApert_HAL(pAcrlib, addr)
#define ACR_BAR0_REG_WR32(addr, data)               acrlibBar0RegWriteDmemApert_HAL(pAcrlib, addr, data)

#else
    // Use BAR0 initiator to write registers if DMEM Aperture is not present
#define ACR_BAR0_REG_RD32(addr)                     acrlibBar0RegRead_HAL(pAcrlib, addr)
#define ACR_BAR0_REG_WR32(addr, data)               acrlibBar0RegWrite_HAL(pAcrlib, addr, data)
#define ACR_BAR0_REG_WR32_NON_BLOCKING(addr, data)  acrlibBar0RegWriteNonBlocking_HAL(pAcrlib, addr, data)
#endif //End of Dmem Apert

#define ACR_REG_RD32(bus,addr)                      ACR_##bus##_REG_RD32(addr)
#define ACR_REG_WR32(bus,addr,data)                 ACR_##bus##_REG_WR32(addr, data)
#define ACR_REG_RD32_STALL(bus,addr)                ACR_##bus##_REG_RD32_STALL(addr)
#define ACR_REG_WR32_STALL(bus,addr,data)           ACR_##bus##_REG_WR32_STALL(addr,data)
#define ACR_REG_WR32_NON_BLOCKING(bus,addr,data)    ACR_##bus##_REG_WR32_NON_BLOCKING(addr,data)

// GCC attributes
#define ATTR_OVLY(ov)          __attribute__ ((section(ov)))
#define ATTR_ALIGNED(align)    __attribute__ ((aligned(align)))

// SIZEOF macros
#ifndef LW_SIZEOF32
#define LW_SIZEOF32(v) (sizeof(v))
#endif

#ifndef NULL
#define NULL                                 (void*)(0)
#endif

// Status checking macros
#define CHECK_STATUS_AND_RETURN_IF_NOT_OK(x) if((status=(x)) != ACR_OK) \
                                             {                          \
                                               return status;           \
                                             }

#define CHECK_STATUS_OK_OR_HALT_WITH_ERROR_CODE(stmt) \
    if ((status = (stmt)) != ACR_OK) \
    {\
        acrWriteStatusToFalconMailbox_HAL(pAcr, status); \
        acrCleanupAndHalt(); \
    }

#define CHECK_STATUS_OK_OR_GOTO_CLEANUP(x) \
    if((status=(x)) != ACR_OK) \
    {\
        goto Cleanup; \
    }


// CCC status checking macros
#define CHECK_STATUS_CCC_AND_RETURN_IF_NOT_OK(status_ccc, status_acr) \
    if((statusCcc =(status_ccc)) != NO_ERROR) \
    {\
        return status_acr; \
    }

#define CHECK_STATUS_CCC_OK_OR_GOTO_CLEANUP(status_ccc, status_acr) \
    if((statusCcc =(status_ccc)) != NO_ERROR) \
    {\
        status = status_acr; \
        goto Cleanup; \
    }

#ifdef ACR_BUILD_ONLY
#define FAIL_ACR_WITH_ERROR(status)                  do { \
    acrWriteStatusToFalconMailbox_HAL(pAcr, status);      \
    falc_halt();                                          \
} while (0)
#else
#define FAIL_ACR_WITH_ERROR(status)          falc_halt();
#endif // ACR_BUILD_ONLY

// DMA
#define DMA_XFER_ESIZE_MIN               (0x00U) //   4-bytes
#define DMA_XFER_ESIZE_MAX               (0x06U) // 256-bytes
#ifndef DMA_XFER_SIZE_BYTES
#define DMA_XFER_SIZE_BYTES(e)           (LWBIT32((e) + 2U))
#endif
#define VAL_IS_ALIGNED   LW_IS_ALIGNED


#define POWER_OF_2_FOR_4K                                   (12U)
#define POWER_OF_2_FOR_128K                                 (17U)
#define POWER_OF_2_FOR_1MB                                  (20U)
#define COLWERT_MB_ALIGNED_TO_4K_ALIGNED_SHIFT              (POWER_OF_2_FOR_1MB  - POWER_OF_2_FOR_4K)
#define COLWERT_128K_ALIGNED_TO_4K_ALIGNED_SHIFT            (POWER_OF_2_FOR_128K - POWER_OF_2_FOR_4K)

// Timeouts
#define ACR_TIMEOUT_FOR_PIPE_RESET                          (0x400)

// Infinite loop handler
#define INFINITE_LOOP()  while(LW_TRUE)

#ifdef ACR_BUILD_ONLY
//*****************************************************************************
// SCP CCI intrinsics
//*****************************************************************************

//
// The new SVEC field definition is ((num_blks << 24) | (e << 17) | (s << 16) | tag_val)
// assuming size is always multiple of 256, then (size << 16) equals (num_blks << 24)
//
#define SVEC_REG(base, size, s, e)  (((base) >> 8) | (((size) | (s)) << 16) | ((e) << 17))

#define SCP_OP_ENUM_DECL enum SCP_OP {nop = 0, mov = 1, push = 2, fetch = 3, rand = 4, load_trace0 = 5, loop_trace0 = 6, load_trace1 = 7, loop_trace1 = 8, chmod = 10, xor = 11, add = 12, and = 13, bswap = 14, cmac_sk = 15, secret = 16, key = 17, rkey10 = 18, rkey1 = 19, encrypt = 20, decrypt = 21, compare_sig = 22, encrypt_sig = 23, forget_sig = 24};
#define  ENUM_SCP_OP_nop 0U
#define  ENUM_SCP_OP_mov 1U
#define  ENUM_SCP_OP_push 2U
#define  ENUM_SCP_OP_fetch 3U
#define  ENUM_SCP_OP_rand 4U
#define  ENUM_SCP_OP_load_trace0 5U
#define  ENUM_SCP_OP_loop_trace0 6U
#define  ENUM_SCP_OP_load_trace1 7U
#define  ENUM_SCP_OP_loop_trace1 8U
#define  ENUM_SCP_OP_chmod 10U
#define  ENUM_SCP_OP_xor 11U
#define  ENUM_SCP_OP_add 12U
#define  ENUM_SCP_OP_and 13U
#define  ENUM_SCP_OP_bswap 14U
#define  ENUM_SCP_OP_cmac_sk 15U
#define  ENUM_SCP_OP_secret 16U
#define  ENUM_SCP_OP_key 17U
#define  ENUM_SCP_OP_rkey10 18U
#define  ENUM_SCP_OP_rkey1 19U
#define  ENUM_SCP_OP_encrypt 20U
#define  ENUM_SCP_OP_decrypt 21U
#define  ENUM_SCP_OP_compare_sig 22U
#define  ENUM_SCP_OP_encrypt_sig 23U
#define  ENUM_SCP_OP_forget_sig 24U



#define SCP_R0         (0x0000U)
#define SCP_R1         (0x0001U)
#define SCP_R2         (0x0002U)
#define SCP_R3         (0x0003U)
#define SCP_R4         (0x0004U)
#define SCP_R5         (0x0005U)
#define SCP_R6         (0x0006U)
#define SCP_R7         (0x0007U)
#define SCP_CCI_MASK   (0x8000U)

static inline void falc_scp_mov(LwU32 rx, LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_mov << 10) | (rx << 4) | ry));
}
static inline void falc_scp_xor(LwU32 rx, LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_xor << 10) | (rx << 4) | ry));
}
static inline void falc_scp_xor_r(LwU32 rx, LwU32 ry) {
    asm volatile("cci %0 %1;" : : "r" ((ry << 8) | rx), "i" (ENUM_SCP_OP_xor));
}

//*****************************************************************************
// SCP CCR intrinsics
//*****************************************************************************
#define CCR_SUPPRESSED  (0x0020U)
#define CCR_SHORTLWT    (0x0040U)
#define CCR_MT_IMEM     (0x0080U)
#define TC_DISABLE_CCR  (0x0000U)
#define TC_2            (0x0002U) // trap count = 2
#define TC_INFINITY     (0x001fU)

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
static inline void falc_scp_encrypt(LwU32 rx, LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_encrypt << 10) | (rx << 4) | ry));
}
static inline void falc_scp_key(LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_key << 10) | ry));
}
static inline void falc_scp_secret(LwU32 imm, LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_secret << 10) | (imm << 4) | ry));
}
static inline void falc_scp_rand(LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_rand << 10) | ry));
}
static inline void falc_scp_decrypt(LwU32 rx, LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_decrypt << 10) | (rx << 4) | ry));
}
static inline void falc_scp_rkey10(LwU32 rx, LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_rkey10 << 10) | (rx << 4) | ry));
}
static inline void falc_scp_forget_sig(void) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_forget_sig << 10)));
}
static inline void falc_scp_chmod(LwU32 imm, LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_chmod << 10) | (imm << 4) | ry));
}
static inline void falc_scp_load_trace0(LwU32 imm) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_load_trace0 << 10) | (imm << 4)));
}
static inline void falc_scp_loop_trace0(LwU32 imm) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_loop_trace0 << 10) | (imm << 4)));
}
static inline void falc_scp_push(LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_push << 10) | ry));
}
static inline void falc_scp_fetch(LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | (ENUM_SCP_OP_fetch << 10) | ry));
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
#endif //ACR_BUILD_ONLY


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

//*****************************************************************************
// Memory Routines
//*****************************************************************************
#define DMSIZE_4B       (0x0000)
#define DMSIZE_8B       (0x0001)
#define DMSIZE_16B      (0x0002)
#define DMSIZE_32B      (0x0003)
#define DMSIZE_64B      (0x0004)
#define DMSIZE_128B     (0x0005)
#define DMSIZE_256B     (0x0006)

#endif //ACRUTILS_H
