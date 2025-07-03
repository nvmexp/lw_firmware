/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: booter.h
 */

#ifndef BOOTER_H
#define BOOTER_H

#include "bootertypes.h"

#include "dev_fb.h"
#include "dev_fbif_v4.h"

#include "dev_riscv_pri.h"
#include "dev_falcon_v4.h"

#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
#include "dev_sec_csb.h"
#include "dev_sec_pri.h"
#include "dev_sec_csb_addendum.h"
#elif defined(BOOTER_RELOAD)
#include "dev_lwdec_csb.h"
#include "dev_lwdec_pri.h"
#include "dev_lwdec_csb_addendum.h"
#endif

#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"

#include "dev_se_seb.h"
#include "dev_se_seb_addendum.h"
#include "booterse.h"
#include "bootersha.h"

#include "dev_bus.h"
#include "dev_master.h"
#include "dev_fuse.h"
#include "dev_top.h"

#include "dev_pri_ringstation_sys_addendum.h"

#include "rmlsfm.h"
#include "rmlsfm_new_wpr_blob.h"
#include "wpr_ctrl_cmd.h"
#include "mmu/mmucmn.h"

#include "lwuproc.h"
#include "lwctassert.h"
#include "sec2shamutex.h"
#include "gsp_fw_wpr_meta.h"

#include "booter_status_codes.h"
#include "booterutils.h"
#include "booter_static_inline_functions.h"
#include "booter_signature.h"
#include "booter_objbooter.h"

#include "config/g_booter_private.h"
#include "config/g_hal_register.h"

//
// Extern Variables
//

//
// Linker script variables holding the start and end
// of secure overlay.
//
extern LwU32 _booter_code_start[]           ATTR_OVLY(".data");
extern LwU32 _booter_code_end[]             ATTR_OVLY(".data");
extern LwU32 _booter_resident_code_start[]  ATTR_OVLY(".data");
extern LwU32 _booter_resident_code_end[]    ATTR_OVLY(".data");

// Linker Script Variable to identify start of BSS
extern LwU32 _bss_start[]                   ATTR_OVLY(".data");

// PRIV_LEVEL mask defines
// TODO: These hardcoded masks need to be cleaned up. Bug 2629958
#if defined(PRI_SOURCE_ISOLATION_PLM)
#define BOOTER_PLMASK_READ_L2_WRITE_L2 0xffffffcc
#define BOOTER_PLMASK_READ_L0_WRITE_L2 0xffffffcf
#define BOOTER_PLMASK_READ_L0_WRITE_L0 0xffffffff
#define BOOTER_PLMASK_READ_L0_WRITE_L3 0xffffff8f
#define BOOTER_PLMASK_READ_L3_WRITE_L3 0xffffff88
#else
#define BOOTER_PLMASK_READ_L2_WRITE_L2 0xcc
#define BOOTER_PLMASK_READ_L0_WRITE_L2 0xcf
#define BOOTER_PLMASK_READ_L0_WRITE_L0 0xff
#define BOOTER_PLMASK_READ_L0_WRITE_L3 0x8f
#define BOOTER_PLMASK_READ_L3_WRITE_L3 0x88
#endif

#define BOOTER_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED    0xF
#define BOOTER_SUB_WPR_MMU_WMASK_ALL_LEVELS_ENABLED    0xF
#define BOOTER_SUB_WPR_MMU_RMASK_L2_AND_L3             0xC
#define BOOTER_SUB_WPR_MMU_WMASK_L2_AND_L3             0xC
#define BOOTER_SUB_WPR_MMU_RMASK_L3                    0x8
#define BOOTER_SUB_WPR_MMU_WMASK_L3                    0x8
#define BOOTER_SUB_WPR_MMU_RMASK_ALL_LEVELS_DISABLED   0x0
#define BOOTER_SUB_WPR_MMU_WMASK_ALL_LEVELS_DISABLED   0x0

#define ILWALID_WPR_ADDR_LO                         (0xFFFFFFFF)
#define ILWALID_WPR_ADDR_HI                         (0x0)

#define NON_WPR_REGION_ID 0
#define WPR_REGION_ID     2

#define BOOTER_UNLOCK_READ_MASK      (0x0)
#define BOOTER_UNLOCK_WRITE_MASK     (0x0)

//
// Additional defines to ease the programming of SubWpr MMU registers
//
#define FALCON_SUB_WPR_INDEX_REGISTER_STRIDE            (LW_PFB_PRI_MMU_FALCON_GSP_CFGA(1) - LW_PFB_PRI_MMU_FALCON_GSP_CFGA(0))
#define FALCON_SUB_WPR_CONFIG_REGISTER_OFFSET_DIFF      (LW_PFB_PRI_MMU_FALCON_GSP_CFGB(0) - LW_PFB_PRI_MMU_FALCON_GSP_CFGA(0))

//
// defines to CLEAR/SET DISABLE_EXCEPTIONS in SEC SPR
// For more info please check Falcon_6_arch doc (Section 2.3.1)
//
#define LW_FALC_SEC_SPR_DISABLE_EXCEPTIONS          19:19
#define LW_FALC_SEC_SPR_DISABLE_EXCEPTIONS_CLEAR    0x00000000
#define LW_FALC_SEC_SPR_DISABLE_EXCEPTIONS_SET      0x00000001

//
// Masks and Bad values for CSB interfaces. Details in wiki below
// https://wiki.lwpu.com/gpuhwmaxwell/index.php/Maxwell_PRIV#Error_Reporting
//
#define CSB_INTERFACE_MASK_VALUE          0xffff0000U
#define CSB_INTERFACE_BAD_VALUE           0xbadf0000U

// Alignment defines
#define BOOTER_SIG_ALIGN                     (16)
#define BOOTER_DESC_ALIGN                    (16)

#define BOOTER_WPR_ALIGNMENT                 (0x20000)
#define BOOTER_SUB_WPR_ALIGNMENT             (0x1000)

#define FLCN_IMEM_BLK_SIZE_IN_BYTES          (256)

#define BOOTER_DEFAULT_TIMEOUT_NS            (100*1000*1000)  // 100 ms

#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
//
// Setting bottom limit of stack to 48KB
// That means the stack can grow from 64KB till 48KB
// We are not using linker script variable for bottom of stack coz any
// NS entity can change the global variable in DMEM and can forfiet our countermeasures.
//
#define BOOTER_STACK_BOTTOM_LIMIT         (0xC000)

#elif defined(BOOTER_RELOAD)
//
// Setting bottom limit of stack to 32KB
// That means the stack can grow from 64KB till 32KB
//
#define BOOTER_STACK_BOTTOM_LIMIT         (0x4000)

#else

ct_assert(0);

#endif

// DMA size passed to acrIssueDma_HAL
#define DMA_SIZE                          (0x400)
#define COPY_BUFFER_A_SIZE_BYTE           (DMA_SIZE)
#define COPY_BUFFER_B_SIZE_BYTE           (DMA_SIZE)
#define COPY_BUFFER_A_SIZE_DWORD          (DMA_SIZE/4)
#define COPY_BUFFER_B_SIZE_DWORD          (DMA_SIZE/4)

// Start Addr of Booter's Dmem
#define BOOTER_START_ADDR_OF_DMEM            (0x0)

#define BOOTER_PC_TRIM(pc)                   (pc & 0x0FFFFFFF)

#define BOOTER_UCODE_BUILD_VERSION_ILWALID   0xFFFFFFFF

//
// Function prototypes
// TODO: Need a better way
//

#define OVLINIT_NAME            ".imem_booterinit"
#define OVL_NAME                ".imem_booter"
#define OVL_END_NAME            ".imem_booter_end"

int             main (void) GCC_ATTRIB_NO_STACK_PROTECT()               ATTR_OVLY(".text");
void            __attribute__ ((noinline)) booterEntryWrapper(void)     ATTR_OVLY(OVLINIT_NAME);
void            booterEntryFunction(void)                               ATTR_OVLY(OVL_NAME);
void            booterCleanupAndHalt(void)                              ATTR_OVLY(OVL_NAME);
LwU8            booterMemcmp(const void *, const void *, LwU32)         ATTR_OVLY(OVL_NAME);
void            booterMemsetByte(void *, LwU32, LwU8)                   ATTR_OVLY(OVL_NAME);
void            __stack_chk_fail(void)                                  ATTR_OVLY(OVL_NAME);
LwU32           falc_ldxb_i_with_halt(LwU32)                            ATTR_OVLY(OVL_NAME);
void            falc_stxb_i_with_halt(LwU32, LwU32)                     ATTR_OVLY(OVL_NAME);

#if defined(BOOTER_LOAD)
BOOTER_STATUS   booterInit(void)                                        ATTR_OVLY(OVL_NAME);
#endif

#if defined(BOOTER_RELOAD)
BOOTER_STATUS   booterReInit(void)                                      ATTR_OVLY(OVL_NAME);
#endif

#if defined(BOOTER_UNLOAD)
BOOTER_STATUS   booterDeInit(void)                                      ATTR_OVLY(OVL_NAME);
#endif

#endif // BOOTER_H
