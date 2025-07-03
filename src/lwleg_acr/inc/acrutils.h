/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef TEGRA_ACRUTILS_H
#define TEGRA_ACRUTILS_H

#include "lwtypes.h"

#ifdef PMU_RTOS
// When building as PMU submake, we want to use the "real" versions
// of the headers to avoid conflicts.
# include "lwmisc.h"
# include "lwuproc.h"
# include "cpu-intrinsics.h"
#else // PMU_RTOS
# include "external/lwmisc.h"
# include "external/lwuproc.h"
# ifndef lwuc_halt
#  define lwuc_halt() falc_halt()
# endif // lwuc_halt
#endif // PMU_RTOS

#ifdef UPROC_RISCV
# include "engine.h"
#endif // UPROC_RISCV

#ifdef NULL
# define LW_NULL  NULL
#else // NULL
# define LW_NULL  ((void*)0)
#endif // NULL

/*!
 * @brief Cast a U64 number to U32 after checking its value is not greater than maximum value of a U32.
 *
 * @param[in] ul_a U64 number to be cast to U32.
 *
 * @return ul_a cast to unsigned 32 bit integer.
 */
static LW_FORCEINLINE LwU32
lwSafeCastU64ToU32(LwU64 ul_a)
{
    if (ul_a > LW_U32_MAX)
    {
        return 0xDEADBEEFU;
    }
    else
    {
        return (LwU32)ul_a;
    }
}

/*!
 * @brief Funtion to return lower 32 bits of a U64 integer.
 *
 * @param[in] n 64 bit unsigned integer whose lower 32 bit is to be returned.
 *
 * @return Lower 32 bits of n.
 */
static inline LwU32
lwU64Lo32(LwU64 n)
{
    return lwSafeCastU64ToU32(n & 0xFFFFFFFFU);
}

/*!
 * @brief Funtion to return higher 32 bits of a U64 integer.
 *
 * @param[in] n 64 bit unsigned integer whose higher 32 bit is to be returned.
 *
 * @return Higher 32 bits of n.
 */
static inline LwU32
lwU64Hi32(LwU64 n)
{
    return lwSafeCastU64ToU32((n >> 32U) & 0xFFFFFFFFU);
}

//
// The new SVEC field definition is ((num_blks << 24) | (e << 17) | (s << 16) | tag_val)
// assuming size is always multiple of 256, then (size << 16) equals (num_blks << 24)
//
#define SVEC_REG(base, size, s, e)  (((LwU32)(base) >> 8) | ((LwU32)((size) | (s)) << 16) | ((LwU32)(e) << 17))

//
// BAR0/CSB register access macros
// Replaced all CSB non blocking call with blocking calls
//
#define ACR_CSB_REG_RD32(addr)                      ACR_CSB_REG_RD32_STALL(addr)
#define ACR_CSB_REG_WR32(addr, data)                ACR_CSB_REG_WR32_STALL(addr, data)
#define ACR_CSB_REG_RD32_STALL(addr)                falc_ldxb_i_with_halt(addr)
#define ACR_CSB_REG_WR32_STALL(addr, data)          falc_stxb_i_with_halt(addr, data)

#ifdef UPROC_RISCV
# ifdef PMU_TEGRA_ACR_PARTITION
// Redefine for the case of a bare-mode RISCV partition (PMU's CheetAh-ACR partition).
#  define ACR_BAR0_REG_RD32(addr)                   bar0ReadPA(addr)
#  define ACR_BAR0_REG_WR32(addr, data)             do                                                       \
                                                    {                                                        \
                                                        bar0WritePA(addr, data); /* fence to catch MEMERR */ \
                                                        __asm__ volatile ("fence io,io");                    \
                                                    } while (LW_FALSE)
# else // PMU_TEGRA_ACR_PARTITION
#  define ACR_BAR0_REG_RD32(addr)                   bar0Read(addr)
#  define ACR_BAR0_REG_WR32(addr, data)             do                                                       \
                                                    {                                                        \
                                                        bar0Write(addr, data); /* fence to catch MEMERR */   \
                                                        __asm__ volatile ("fence io,io");                    \
                                                    } while (LW_FALSE)
# endif // PMU_TEGRA_ACR_PARTITION
#else // UPROC_RISCV
# ifdef DMEM_APERT_ENABLED

#  define ACR_BAR0_REG_RD32(addr)                   acrBar0RegReadDmemApert(addr)
#  define ACR_BAR0_REG_WR32(addr, data)             acrBar0RegWriteDmemApert(addr, data)

# else
    // Use BAR0 master to write registers if EMEM Aperture is not present
#  define ACR_BAR0_REG_RD32(addr)                   _acrBar0RegRead(addr)
#  define ACR_BAR0_REG_WR32(addr, data)             _acrBar0RegWrite(addr,data)
# endif //End of Dmem Apert
#endif // UPROC_RISCV

#define ACR_REG_RD32(bus,addr)                      ACR_##bus##_REG_RD32(addr)
#define ACR_REG_WR32(bus,addr,data)                 ACR_##bus##_REG_WR32(addr, data)
#define ACR_REG_RD32_STALL(bus,addr)                ACR_##bus##_REG_RD32_STALL(addr)
#define ACR_REG_WR32_STALL(bus,addr,data)           ACR_##bus##_REG_WR32_STALL(addr,data)

// GCC attributes
#define ATTR_OVLY(ov)          __attribute__ ((section(ov)))
#define ATTR_ALIGNED(align)    __attribute__ ((aligned(align)))

// SIZEOF macros
#ifndef LW_SIZEOF32
#define LW_SIZEOF32(v) (sizeof(v))
#endif

//Alignment macro
#define ALIGN_PAGE(size) (size - (size % 0x1000U))

//Safety macros
#if defined ACR_RISCV_PMU || ACR_RISCV_GSPLITE
#define CHECK_WRAP_AND_HALT(cond) do                                                       \
                  {                                                                        \
                      if (cond)                                                            \
                      {                                                                    \
                          acrWriteStatusToFalconMailbox(ACR_ERROR_VARIABLE_SIZE_OVERFLOW); \
                          riscvShutdown();                                                 \
                      }                                                                    \
                  }                                                                        \
                  while(LW_FALSE)
#else
#define CHECK_WRAP_AND_HALT(cond) do                                                       \
                  {                                                                        \
                      if (cond)                                                            \
                      {                                                                    \
                          acrWriteStatusToFalconMailbox(ACR_ERROR_VARIABLE_SIZE_OVERFLOW); \
                          lwuc_halt();                                                     \
                      }                                                                    \
                  }                                                                        \
                  while(LW_FALSE)
#endif
#define CHECK_WRAP_AND_ERROR(cond) do                                                          \
                                  {                                                            \
                                      if (cond)                                                \
                                      {                                                        \
                                          return ACR_ERROR_VARIABLE_SIZE_OVERFLOW;             \
                                      }                                                        \
                                  }                                                            \
                                  while(LW_FALSE)
// Status checking macro. Takes care of MISRA-C Rule 17.7 violations
#define CHECK_STATUS_AND_RETURN_IF_NOT_OK(x) if((status=(x)) != ACR_OK) \
                                             {                          \
                                               return status;           \
                                             }


// DMA
#define  DMA_XFER_ESIZE_MIN               (0x00U) //   4-bytes
#define  DMA_XFER_ESIZE_MAX               (0x06U) // 256-bytes
#define VAL_IS_ALIGNED   LW_IS_ALIGNED


#define POWER_OF_2_FOR_4K                                   (12)
#define POWER_OF_2_FOR_128K                                 (17)
#define POWER_OF_2_FOR_1MB                                  (20)
#define COLWERT_MB_ALIGNED_TO_4K_ALIGNED_SHIFT              (POWER_OF_2_FOR_1MB  - POWER_OF_2_FOR_4K)
#define COLWERT_128K_ALIGNED_TO_4K_ALIGNED_SHIFT            (POWER_OF_2_FOR_128K - POWER_OF_2_FOR_4K)

#ifndef PMU_RTOS
// SCP CCI intrinsics

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

#define LSHIFT32_10(u) ((LwU32)(u) << 10)

#ifndef LW_RISCV_ACR
static inline void falc_scp_mov(LwU32 rx, LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | LSHIFT32_10(ENUM_SCP_OP_mov) | (rx << 4) | ry));
}
static inline void falc_scp_xor(LwU32 rx, LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | LSHIFT32_10(ENUM_SCP_OP_xor) | (rx << 4) | ry));
}
static inline void falc_scp_xor_r(LwU32 rx, LwU32 ry) {
    asm volatile("cci %0 %1;" : : "r" ((ry << 8) | rx), "i" (ENUM_SCP_OP_xor));
}


// SCP CCR intrinsics

#define CCR_SUPPRESSED (0x0020U)
#define CCR_SHORTLWT   (0x0040U)
#define CCR_MT_IMEM    (0x0080U)
#define TC_INFINITY    (0x001fU)

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
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | LSHIFT32_10(ENUM_SCP_OP_encrypt) | (rx << 4) | ry));
}
static inline void falc_scp_key(LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | LSHIFT32_10(ENUM_SCP_OP_key) | ry));
}
static inline void falc_scp_secret(LwU32 imm, LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | LSHIFT32_10(ENUM_SCP_OP_secret) | (imm << 4) | ry));
}
static inline void falc_scp_rand(LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | LSHIFT32_10(ENUM_SCP_OP_rand) | ry));
}
static inline void falc_scp_decrypt(LwU32 rx, LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | LSHIFT32_10(ENUM_SCP_OP_decrypt) | (rx << 4) | ry));
}
static inline void falc_scp_rkey10(LwU32 rx, LwU32 ry) {
    asm volatile("cci %0;" : : "i" (SCP_CCI_MASK | LSHIFT32_10(ENUM_SCP_OP_rkey10) | (rx << 4) | ry));
}

// trapped dmwrite copies 16 bytes from DMEM to SCP

static inline void falc_trapped_dmwrite(LwU32 addr)
{
    asm volatile("dmwrite %0 %0;" :: "r"(addr));
}


// trapped dmread copies 16 bytes from SCP to DMEM

static inline void falc_trapped_dmread(LwU32 addr)
{
    asm volatile("dmread %0 %0;" :: "r"(addr));
}

// falcon NOP instruction.
static inline void falc_nop(void)
{
    asm volatile("nop;");
}

static inline void falc_reset_gprs(void)
{
    // Reset all falcon GPRs (a0-a15)
    asm volatile
    (
        "clr.w a0;"
        "clr.w a1;"
        "clr.w a2;"
        "clr.w a3;"
        "clr.w a4;"
        "clr.w a5;"
        "clr.w a6;"
        "clr.w a7;"
        "clr.w a8;"
        "clr.w a9;"
        "clr.w a10;"
        "clr.w a11;"
        "clr.w a12;"
        "clr.w a13;"
        "clr.w a14;"
        "clr.w a15;"
    );
}
#endif // LW_RISCV_ACR
#endif // PMU_RTOS
#endif // TEGRA_ACRUTILS_H
