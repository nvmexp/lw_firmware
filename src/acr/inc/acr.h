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

#ifndef ACR_H
#define ACR_H

#include <falcon-intrinsics.h>
#include "acrutils.h"
#include "acrtypes.h"
#include "rmlsfm.h"
#include "wpr_ctrl_cmd.h"
#include "rmlsfm_new_wpr_blob.h"
#include "acr_status_codes.h"
#include "acrse.h"
#include "acrsha.h"

#ifdef AMPERE_PROTECTED_MEMORY
#include "acrapm.h"
#endif

#ifdef ACR_BUILD_ONLY
#include "acr_static_inline_functions.h"
#endif

#include "lwuproc.h"
#include "../lwctassert.h"

// Current boostrap owner
#ifdef ACR_UNLOAD
#define ACR_LSF_LWRRENT_BOOTSTRAP_OWNER   LSF_BOOTSTRAP_OWNER_PMU
#endif

#if defined (AHESASC) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
#define ACR_LSF_LWRRENT_BOOTSTRAP_OWNER   LSF_BOOTSTRAP_OWNER_SEC2
#endif

#if defined(ASB)
#define ACR_LSF_LWRRENT_BOOTSTRAP_OWNER   LSF_BOOTSTRAP_OWNER_GSPLITE
#endif

//
// Extern Variables
//

//
// Linker script variables holding the start and end
// of secure overlay.
//
extern LwU32 _acr_code_start[]           ATTR_OVLY(".data");
extern LwU32 _acr_code_end[]             ATTR_OVLY(".data");
extern LwU32 _acr_resident_code_start[]  ATTR_OVLY(".data");
extern LwU32 _acr_resident_code_end[]    ATTR_OVLY(".data");

// Linker Script Variable to identify start of BSS
extern LwU32 _bss_start[]                  ATTR_OVLY(".data");

// Required for assigning full WPR1 access to ACRlib running falcon
#define ACRLIB_RUNNING_FALCON             LSF_FALCON_ID_SEC2

//
// Masks and Bad values for External and CSB interfaces. Details in wiki below
// https://wiki.lwpu.com/gpuhwmaxwell/index.php/Maxwell_PRIV#Error_Reporting
// HW Bug: 200198584, to get the MASK and BAD Value for CSB and EXT Interface.
// Once it is solved, clean the below MASK and BAD values and use HW defines.
// HW Bug: 200198606, to get the DMEM Aperture define from the HW and use it
// inplace of present define.
//
#define DMEM_APERTURE_OFFSET              BIT(28)
#define EXT_INTERFACE_MASK_VALUE          0xfff00000U
#define EXT_INTERFACE_BAD_VALUE           0xbad00000U
#define CSB_INTERFACE_MASK_VALUE          0xffff0000U
#define CSB_INTERFACE_BAD_VALUE           0xbadf0000U

// Alignment defines
#define ACR_SIG_ALIGN                     (16)
#define ACR_DESC_ALIGN                    (16)
#define ACR_REGION_ALIGN                  (0x20000)

// PRIV_LEVEL mask defines
// TODO: These hardcoded masks need to be cleaned up. Bug 2629958
#if defined(PRI_SOURCE_ISOLATION_PLM)
#define ACR_PLMASK_READ_L2_WRITE_L2       0xffffffcc
#define ACR_PLMASK_READ_L0_WRITE_L2       0xffffffcf
#define ACR_PLMASK_READ_L0_WRITE_L0       0xffffffff
#define ACR_PLMASK_READ_L0_WRITE_L3       0xffffff8f
#define ACR_PLMASK_READ_L3_WRITE_L3       0xffffff88
#else
#define ACR_PLMASK_READ_L2_WRITE_L2       0xcc
#define ACR_PLMASK_READ_L0_WRITE_L2       0xcf
#define ACR_PLMASK_READ_L0_WRITE_L0       0xff
#define ACR_PLMASK_READ_L0_WRITE_L3       0x8f
#define ACR_PLMASK_READ_L3_WRITE_L3       0x88
#endif

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

#define ILWALID_REG_ADDR          (0x00000001)

// Timeouts
#define ACR_DEFAULT_TIMEOUT_NS            (100*1000*1000)  // 100 ms

// Common defines
#define FLCN_IMEM_BLK_SIZE_IN_BYTES       (256)
#define FLCN_IMEM_BLK_ALIGN_BITS          (8)

//
// Setting bottom limit of stack to 48KB
// That means the stack can grow from 64KB till 48KB
// We are not using linker script variable for bottom of stack coz any
// NS entity can change the global variable in DMEM and can forfiet our countermeasures.
//
#define ACR_STACK_BOTTOM_LIMIT            (0xC000)

// DMA size passed to acrIssueDma_HAL
#define DMA_SIZE                          (0x400)
#define COPY_BUFFER_A_SIZE_BYTE           (DMA_SIZE)
#define COPY_BUFFER_B_SIZE_BYTE           (DMA_SIZE)
#define COPY_BUFFER_A_SIZE_DWORD          (DMA_SIZE/4)
#define COPY_BUFFER_B_SIZE_DWORD          (DMA_SIZE/4)

// Start Addr of Acr's Dmem
#define ACR_START_ADDR_OF_DMEM            (0x0)

//
// Key indexes
// From Turing onwards, we are using prod key index 43 which is only secure keyable
//
#define ACR_LS_VERIF_KEY_INDEX_DEBUG                    (0)
#define ACR_LS_VERIF_KEY_INDEX_PROD_GM20X_THRU_GV100    (5)
#define ACR_LS_VERIF_KEY_INDEX_PROD_TU10X_AND_LATER     (43)

// Encryption Key Indexes
#define ACR_LS_ENCRYPT_KEY_INDEX_DEBUG                  (ACR_LS_VERIF_KEY_INDEX_DEBUG)

// Signature indexes
#define ACR_LS_VERIF_SIGNATURE_CODE_INDEX (0x0)
#define ACR_LS_VERIF_SIGNATURE_DATA_INDEX (0x1)

// Validation defines
#define IS_FALCONID_ILWALID(id, off) ((id == LSF_FALCON_ID_ILWALID) || (id >= LSF_FALCON_ID_END) || (!off))

#define ACR_REGION_START_IDX       LSF_WPR_EXPECTED_REGION_ID
#define ACR_REGION_END_IDX         LSF_WPR_EXPECTED_REGION_ID

// WPR indexes used while referring g_desc.regions.regionProps which is filled by RM
#define ACR_WPR1_REGION_IDX               (0x0)
#define ACR_WPR2_REGION_IDX               (0x1)

// Used while copying FRTS data from WPR2 to WPR1
#define ACR_WPR2_MMU_REGION_ID            (0x2)

// Get WPR header for particular falcon index
#define GET_WPR_HEADER(ind)        ((PLSF_WPR_HEADER)(g_pWprHeader + (ind * sizeof(LSF_WPR_HEADER))))

// Get WPR header WRAPPER for particular falcon index
#define GET_WPR_HEADER_WRAPPER(ind) ((PLSF_WPR_HEADER_WRAPPER)(g_wprHeaderWrappers + (ind * sizeof(LSF_WPR_HEADER_WRAPPER))))

// Get SUB WPR header for particular falcon index
//#define GET_SUBWPR_HEADER(ind)     ((PLSF_SHARED_SUB_WPR_HEADER)(g_pSubWprHeader + (ind * sizeof(LSF_SHARED_SUB_WPR_HEADER))))

// Invalid ACR ucode version
#define ACR_UCODE_BUILD_VERSION_ILWALID     0xFFFFFFFF

//
// Defines for decode trap used for Locking falcon reg space
// 6 MSB bits indicate SUBIDs, other bits indicate the decode trap mask
// TODO: Use pri-src-isolation TARGET_MASK to block falcon reg space instead of decode traps - Bug 2155192
//
#define DECODE_TRAP_MASK_LSF_FALCON_PMU          (0xfc001fff)   // PMU FALCON range is from 0x10a000 to 0x10bfff on TU10X (we do not support older chips in chips_a and r400_and_later)
#define DECODE_TRAP_MASK_LSF_FALCON_SEC2         (0xfc003fff)   // SEC2 FALCON range is from 0x840000 to 0x843fff on TU10X (we do not support older chips in chips_a and r400_and_later)
#define DECODE_TRAP_FOR_FALCON_REG_SPACE_INDEX   (20)
#define ACR_PC_TRIM(pc)                          (pc & 0x0FFFFFFF)

//
// Additional defines to ease the programming of SubWpr MMU registers
//
#define FALCON_SUB_WPR_INDEX_REGISTER_STRIDE            (LW_PFB_PRI_MMU_FALCON_PMU_CFGA(1) - LW_PFB_PRI_MMU_FALCON_PMU_CFGA(0))
#define FALCON_SUB_WPR_CONFIG_REGISTER_OFFSET_DIFF      (LW_PFB_PRI_MMU_FALCON_PMU_CFGB(0) - LW_PFB_PRI_MMU_FALCON_PMU_CFGA(0))

//
// defines to CLEAR/SET DISABLE_EXCEPTIONS in SEC SPR
// For more info please check Falcon_6_arch doc (Section 2.3.1)
//
#define LW_FALC_SEC_SPR_DISABLE_EXCEPTIONS          19:19
#define LW_FALC_SEC_SPR_DISABLE_EXCEPTIONS_CLEAR    0x00000000
#define LW_FALC_SEC_SPR_DISABLE_EXCEPTIONS_SET      0x00000001

//
// Function prototypes
// TODO: Need a better way
//

#define OVLINIT_NAME            ".imem_acrinit"
#define OVL_NAME                ".imem_acr"
#define OVL_END_NAME            ".imem_acr_end"

#ifdef ACR_BUILD_ONLY
int         main (void)                                           GCC_ATTRIB_NO_STACK_PROTECT() ATTR_OVLY(".text");
#endif

void        acrEntryWrapper(void)                                                               ATTR_OVLY(OVLINIT_NAME);
void        acrEntryFunction(void)                                                              ATTR_OVLY(OVL_NAME);
#if defined(ACR_UNLOAD) || defined(ACR_UNLOAD_ON_SEC2)
ACR_STATUS  acrDeInit(void)                                                                     ATTR_OVLY(OVL_NAME);
#endif

#ifdef ACR_BSI_PHASE
ACR_STATUS  acrBsiPhase(void)                                                                   ATTR_OVLY(OVL_NAME);
#endif

void        acrCleanupAndHalt(void)                                                             ATTR_OVLY(OVL_NAME);
void        acrForceHalt(void)                                                                  ATTR_OVLY(OVL_NAME);

#ifdef AHESASC
LwBool      acrIsFalconSupported(LwU32)                                                         ATTR_OVLY(OVL_NAME);
LwU8        acrMemcmp(const void *, const void *, LwU32)                                        ATTR_OVLY(OVL_NAME);
void        acrMemsetByte(void *, LwU32, LwU8)                                                  ATTR_OVLY(OVL_NAME);
#endif // AHESASC

#ifdef AMPERE_PROTECTED_MEMORY
ACR_STATUS acrReadMsrGroup(LwU32, PAPM_MSR_GROUP)                                               ATTR_OVLY(OVL_NAME);
ACR_STATUS acrMsrExtend(LwU32, LwU8 *, LwBool)                                                  ATTR_OVLY(OVL_NAME);
ACR_STATUS acrMsrResetAll(void)                                                                 ATTR_OVLY(OVL_NAME);
#endif

void        acrExceptionHandlerSelwreHang(void)                                                 ATTR_OVLY(OVL_NAME);
LwU32       falc_ldxb_i_with_halt(LwU32)                                                        ATTR_OVLY(OVL_NAME);
void        falc_stxb_i_with_halt(LwU32, LwU32)                                                 ATTR_OVLY(OVL_NAME);

//
// End block of HS code to just keep it away from usable code
//
void        acrTrustedNSBlock(void)                                                             ATTR_OVLY(OVL_END_NAME);

void        __stack_chk_fail(void)                                                              ATTR_OVLY(OVL_NAME);

#endif // ACR_H
