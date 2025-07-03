/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr.h
 */

#ifndef TEGRA_ACR_H
#define TEGRA_ACR_H

#include "acrutils.h"
#include "acrtypes.h"
#include "acrdbg.h"
#include "acr_status_codes.h"
#include "rmlsfm.h"
#include "rng_scp.h"

#ifdef ACR_FALCON_PMU
// Current boostrap owner
#define ACR_LSF_LWRRENT_BOOTSTRAP_OWNER   LSF_BOOTSTRAP_OWNER_PMU
#else
#define ACR_LSF_LWRRENT_BOOTSTRAP_OWNER   LSF_BOOTSTRAP_OWNER_SEC2
#endif

//
// Masks and Bad values for External and CSB interfaces. Details in wiki below
// https://wiki.lwpu.com/gpuhwmaxwell/index.php/Maxwell_PRIV#Error_Reporting
// HW Bug: 200198584, to get the MASK and BAD Value for CSB and EXT Interface.
// Once it is solved, clean the below MASK and BAD values and use HW defines.
// HW Bug: 200198606, to get the DMEM Aperture define from the HW and use it
// inplace of present define.
//
#define DMEM_APERTURE_OFFSET              BIT32(28)
#define ACR_EXT_INTERFACE_MASK_VALUE      0xfff00000U
#define ACR_EXT_INTERFACE_BAD_VALUE       0xbad00000U
#define CSB_INTERFACE_MASK_VALUE          0xffff0000U
#define CSB_INTERFACE_BAD_VALUE           0xbadf0000U

// Alignment defines
#define ACR_SIG_ALIGN                     (16)
#define ACR_DESC_ALIGN                    (16)
#define ACR_REGION_ALIGN                  (0x20000U)
#define ACR_DEFAULT_ALIGNMENT             (0x20000U)

// PRIV_LEVEL mask defines
#ifdef ACR_PRI_SOURCE_ISOLATION_ENABLED
// These defines will grant access to all PRI sources having required PRIV level authorization.
// POR PLM values registers are at //hw/doc/gpu/turing/turing/design/Functional_Descriptions/Turing_PRI_source_isolation_FD.docx
#define ACR_PLMASK_READ_L2_WRITE_L2       0xffffffclw
#define ACR_PLMASK_READ_L0_WRITE_L2       0xffffffcfU
#define ACR_PLMASK_READ_L0_WRITE_L0       0xffffffffU
#define ACR_PLMASK_READ_L0_WRITE_L3       0xffffff8fU
#define ACR_PLMASK_READ_L3_WRITE_L3       0xffffff88U
#else
#define ACR_PLMASK_READ_L2_WRITE_L2       0x44
#define ACR_PLMASK_READ_L0_WRITE_L2       0x47
#define ACR_PLMASK_READ_L0_WRITE_L0       0x77
#define ACR_PLMASK_READ_L0_WRITE_L3       0x07
#define ACR_PLMASK_READ_L3_WRITE_L3       0x0
#endif

#ifdef ACR_SUB_WPR
// SUB-WPR mask defines
#define ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED    0xF
#define ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_ENABLED    0xF
#define ACR_SUB_WPR_MMU_RMASK_L2_AND_L3             0xC
#define ACR_SUB_WPR_MMU_WMASK_L2_AND_L3             0xC
#define ACR_SUB_WPR_MMU_RMASK_L3                    0x8
#define ACR_SUB_WPR_MMU_WMASK_L3                    0x8
#define ACR_SUB_WPR_MMU_RMASK_ALL_LEVELS_DISABLED   0x0
#define ACR_SUB_WPR_MMU_WMASK_ALL_LEVELS_DISABLED   0x0
#define ACR_UNLOCK_READ_MASK      (0x0)
#define ACR_UNLOCK_WRITE_MASK     (0x0)

// Additional defines to ease the programming of SubWpr MMU registers
#define FALCON_SUB_WPR_INDEX_REGISTER_STRIDE            (LW_PFB_PRI_MMU_FALCON_PMU_CFGA(1) - LW_PFB_PRI_MMU_FALCON_PMU_CFGA(0))
#define FALCON_SUB_WPR_CONFIG_REGISTER_OFFSET_DIFF      (LW_PFB_PRI_MMU_FALCON_PMU_CFGB(0) - LW_PFB_PRI_MMU_FALCON_PMU_CFGA(0))
#endif

#define ILWALID_REG_ADDR          (0x00000001)

// Timeouts
#define ACR_DEFAULT_TIMEOUT_NS            (100*1000*1000)  // 100 ms

// Common defines
#define FLCN_IMEM_BLK_SIZE_IN_BYTES       (256U)
#define FLCN_DMEM_BLK_SIZE_IN_BYTES       (256U)
#define FLCN_IMEM_BLK_ALIGN_BITS          (8U)

// Key indexes
#define ACR_LS_VERIF_KEY_INDEX_DEBUG      (0x0U)
#define ACR_LS_VERIF_KEY_INDEX_PROD       (0x5U)

// Signature indexes
#define ACR_LS_VERIF_SIGNATURE_CODE_INDEX (0x0U)
#define ACR_LS_VERIF_SIGNATURE_DATA_INDEX (0x1U)

// Validation defines
#define IS_FALCONID_ILWALID(id, off) (((id) == LSF_FALCON_ID_ILWALID) || ((id) >= LSF_FALCON_ID_END) || ((off) == 0U))

// Size defines
#define ACR_UCODE_LOAD_BUF_SIZE      ACR_FALCON_BLOCK_SIZE_IN_BYTES
#define DMA_SIZE                     0x400U

// SCP mask defines
// LW_SCP_RNDCTL0_HOLDOFF_INIT_LOWER_INIT is set as 0x01C9C380 and
// LW_SCP_RNDCTL1_HOLDOFF_INTRA_MASK_INIT is set as 0xFFFF by defualt.
// Values need to be 0xFFU and 0xFU respectively to reduce wait time for RNG
// These macros are defined to do the same while being MISRA compliant.
//
#define LW_CPWR_PMU_SCP_RNDCTL0_HOLDOFF_INIT_LOWER_ACR_INIT 0xFFU
#define LW_CPWR_PMU_SCP_RNDCTL1_HOLDOFF_INTRA_MASK_ACR_INIT 0xFU
#define LW_CPWR_PMU_SCP_RNDCTL11_AUTOCAL_STATIC_TAP_A_SEVEN 0x7
#define LW_CPWR_PMU_SCP_RNDCTL11_AUTOCAL_STATIC_TAP_B_FIVE  0x5

//
// Extern declarations
//


// Linker script variables holding the start and end
// of secure overlay.
//
extern LwU32 acr_code_start[]          __asm__("__acr_code_start")                          ATTR_OVLY(".data");
extern LwU32 acr_code_end[]            __asm__("__acr_code_end")                            ATTR_OVLY(".data");
extern LwU32 acr_resident_code_start[] __asm__("__acr_resident_code_start")                 ATTR_OVLY(".data");
extern LwU32 acr_resident_code_end[]   __asm__("__acr_resident_code_end")                   ATTR_OVLY(".data");

extern ACR_DMA_PROP              g_dmaProp;
extern ACR_FLCN_BL_CMDLINE_ARGS  g_blCmdLineArgs;
extern ACR_RESERVED_DMEM         g_reserved;
extern ACR_SCRUB_STATE           g_scrubState;
extern LSF_LSB_HEADER            g_lsbHeader;
extern LwBool                    g_bIsDebug;
extern LwBool                    g_bSetReqCtx;
extern LwU32                     g_signature[4];
extern LwU32                     xmemStore[FLCN_IMEM_BLK_SIZE_IN_BYTES/sizeof(LwU32)];
extern LwU32                     acrHsScratchPad[SCP_REG_SIZE_IN_DWORDS];
extern LwU32                     g_copyBuffer1[DMA_SIZE];
extern LwU32                     g_copyBuffer2[DMA_SIZE];
extern LSF_WPR_HEADER            g_pWprHeader[LSF_WPR_HEADERS_TOTAL_SIZE_MAX/sizeof(LSF_WPR_HEADER)];
extern ACR_LS_SIG_GRP_HEADER     g_LsSigGrpLoadBuf[ACR_LS_SIG_GRP_LOAD_BUF_SIZE/sizeof(ACR_LS_SIG_GRP_HEADER)];
extern LwU8                      g_UcodeLoadBuf[2][ACR_UCODE_LOAD_BUF_SIZE];
extern LwU8                      g_dmHashForGrp[ACR_ENUM_LS_SIG_GRP_ID_TOTAL][ACR_AES128_DMH_SIZE_IN_BYTES];
extern LwU8                      g_dmHash[ACR_AES128_DMH_SIZE_IN_BYTES];
extern LwU8                      g_kdfSalt[ACR_AES128_SIG_SIZE_IN_BYTES];
extern RM_FLCN_ACR_DESC          g_desc;

//
// Function prototypes
// TODO: Need a better way
//

#define OVLINIT_NAME            ".imem_acrinit"
#define OVL_NAME                ".imem_acr"
#define OVL_END_NAME            ".imem_acr_end"

int         main (int argc, char **Argv)                                   GCC_ATTRIB_NO_STACK_PROTECT() ATTR_OVLY(".text");

void        acrEntryFunction(void)                                                              ATTR_OVLY(OVLINIT_NAME);

#ifdef ACR_UNLOAD_PATH
ACR_STATUS  acrDeInit(void)                                                                     ATTR_OVLY(OVL_NAME);
#endif

#ifdef ACR_BSI_PHASE
ACR_STATUS  acrBsiPhase(void)                                                                   ATTR_OVLY(OVL_NAME);
#endif

#ifndef ACR_BSI_LOCK_PATH
void        acrValidateBlocks(void)                                                             ATTR_OVLY(OVL_NAME);
#endif

void        acrCleanup(void)                                                                    ATTR_OVLY(OVL_NAME);
#ifndef ACR_SAFE_BUILD
void        acrForceHalt(void)                                                                  ATTR_OVLY(OVL_NAME);
#endif
void        acrMemxor(void *pDst, const void *pSrc, LwU32 sizeInBytes)                          ATTR_OVLY(OVL_NAME);
void        acrMemcpy(void *pDst, const void *pSrc, LwU32 sizeInBytes)                          ATTR_OVLY(OVL_NAME);
void        acrMemset(void *pData, LwU8 val, LwU32 sizeInBytes)                                 ATTR_OVLY(OVL_NAME);
ACR_STATUS  acrMemcmp(const void *pS1, const void *pS2, LwU32 size)                             ATTR_OVLY(OVL_NAME);
void        acrWriteStatusToFalconMailbox(ACR_STATUS status)                                    ATTR_OVLY(OVL_NAME);
#ifndef DMEM_APERT_ENABLED
void _acrBar0RegWrite(LwU32 addr, LwU32 data)                                                   ATTR_OVLY(OVL_NAME);
LwU32 _acrBar0RegRead(LwU32 addr)                                                               ATTR_OVLY(OVL_NAME);
void _acrBar0WaitIdle(void)                                                                     ATTR_OVLY(OVL_NAME);
#else
LwU32 acrBar0RegReadDmemApert(LwU32 addr)                                                       ATTR_OVLY(OVL_NAME);
void  acrBar0RegWriteDmemApert(LwU32 addr, LwU32 data)                                          ATTR_OVLY(OVL_NAME);
#endif

#ifdef UPROC_RISCV

// for lwuc_*
# include "cpu-intrinsics.h"

//
// RISCV hits an exception on failing to read a register,
// and an interrupt on failing to write a register. Therefore
// we do not need special load-halt and store-halt functions.
//
# define falc_ldxb_i_with_halt(addr)        lwuc_ldxb_i((LwU32*)(LwUPtr)(addr), 0)
# define falc_stxb_i_with_halt(addr, data)  lwuc_stxb_i((LwU32*)(LwUPtr)(addr), 0, (data))

#else // UPROC_RISCV

LwU32 falc_ldxb_i_with_halt(LwU32 addr)                                                         ATTR_OVLY(OVL_NAME);

/*!
 * @brief Wrapper for assembly code for load external with immediate offset.
 *
 * @param[in] cfgAdr     Address of the source register.
 * @param[in] index_imm  Offset index.
 * @param[in] result     Address of the destination register.
 */
__attribute__((always_inline)) static inline
void acr_falc_ldx_i(unsigned int cfgAdr, const unsigned int index_imm, unsigned int *result)
{
    asm volatile ( "ldx %0 %1 %2;" : "=r"(*result) : "r"(cfgAdr), "n"(index_imm) );
}

/*!
 * @brief Wrapper for assembly code for load external with immediate offset and CPU stall.
 *
 * @param[in] cfgAdr     Address of the source register.
 * @param[in] index_imm  Offset index.
 * @param[in] result     Address of the destination register.
 */
__attribute__((always_inline)) static inline
void acr_falc_ldxb_i(unsigned int cfgAdr, const unsigned int index_imm, unsigned int *result)
{
    asm volatile ( "ldxb %0 %1 %2;" : "=r"(*result) : "r"(cfgAdr), "n"(index_imm) );
}

void  falc_stxb_i_with_halt(LwU32 addr, LwU32 data)                                             ATTR_OVLY(OVL_NAME);

/*!
 * @brief  Wrapper for assembly code for store external with immediate offset and CPU stall.
 *
 * @param[in] cfgAdr     Address of the destination register.
 * @param[in] index_imm  Offset index.
 * @param[in] val        Value to be stored in the destination register.
 */
__attribute__((always_inline)) static inline
void acr_falc_stxb_i(unsigned int cfgAdr, const unsigned int index_imm, unsigned int val)
{
    asm volatile ( "stxb %0 %1 %2;" :: "r"(cfgAdr), "n"(index_imm), "r"(val) );
}
#endif // UPROC_RISCV

//
// End block of HS code to just keep it away from usable code
//
#ifndef ACR_SAFE_BUILD
void        acrTrustedNSBlock(void)                                                             ATTR_OVLY(OVL_END_NAME);
#endif

// Lib functions
void        acrlibEnableICD(PACR_FLCN_CONFIG pFlcnCfg)                                          ATTR_OVLY(OVL_NAME);
#ifndef ACR_SAFE_BUILD
ACR_STATUS acrBootstrapValidatedUcode(void)                                                     ATTR_OVLY(OVL_NAME);
#endif
void       __stack_chk_fail(void)                                                               ATTR_OVLY(OVL_NAME);

#endif // TEGRA_ACR_H
