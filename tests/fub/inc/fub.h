/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017 - 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: fub.h
 */

#ifndef _FUB_H_
#define _FUB_H_

/* ------------------------- System Includes -------------------------------- */
#include <falcon-intrinsics.h>
#include "fubutils.h"
#include "fubtypes.h"
#include "fubovl.h"
#include "fubScpDefines.h"
#include "lwuproc.h"
#include "lwRmReg.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define FUB_DESC_ALIGN                                                      (16)
// Time in ns for which FUB needs to wait for voltage to ramp up from 0v to 1.8v on fuse VQPS pin.
#define WAIT_FOR_LWVDD_RAIL_VOLTAGE_TO_RAMPUP                               (0x100000)

//
// Mask for each use case of FUB.
// For each use case 1 bit is assigned in mask.
#define LW_FUB_USE_CASE_MASK_ENABLE_GSYNC                                   (0x1)
#define LW_FUB_USE_CASE_MASK_REVOKE_DEVID_BASED_HULK                        (0x2)
#define LW_FUB_USE_CASE_MASK_ALLOW_DRAM_CHIPID_READ                         (0x4)
#define LW_FUB_USE_CASE_MASK_BLOCK_DRAM_CHIPID_READ                         (0x8)
#define LW_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY                           (0x10)
#define LW_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY_GFW_REV                   (0x20)
#define LW_FUB_USE_CASE_MASK_LWFLASH_REV_FOR_WP_BYPASS                      (0x40)
#define LW_FUB_USE_CASE_MASK_GFW_REV_FOR_WP_BYPASS                          (0x80)
#define LW_FUB_USE_CASE_MASK_AUTO_QS2_AHESASC_FUSE                          (0x100)
#define LW_FUB_USE_CASE_MASK_AUTO_QS2_ASB_FUSE                              (0x200)
#define LW_FUB_USE_CASE_MASK_AUTO_QS2_LWFLASH_FUSE                          (0x400)
#define LW_FUB_USE_CASE_MASK_AUTO_QS2_IMAGESELECT_FUSE                      (0x800)
#define LW_FUB_USE_CASE_MASK_AUTO_QS2_HULK_FUSE                             (0x1000)
#define LW_FUB_USE_CASE_MASK_AUTO_QS2_FWSEC_FUSE                            (0x2000)

//
// FALCON DMEM size
// TODO - File HW bug to add these defines to chip specific files either as an
//        init value in ref manual or another exported manual 
//        since these defines are chip spec.
//
#ifdef FUB_ON_LWDEC
#define FLCN_DMEM_SIZE_IN_BLK_TURING                                        (128)
#elif FUB_ON_SEC2
#define FLCN_DMEM_SIZE_IN_BLK_TURING                                        (256)
#else
ct_assert(0);
#endif
#define FLCN_DMEM_BLK_ALIGN_BITS_TURING                                     (8)

//
// Setting bottom limit of stack to 48KB for SEC2
// That means the stack can grow from 64KB till 48KB
// LWDEC has DMEM of 32KB, so set stack bottom to 16KB
// 
#ifdef FUB_ON_SEC2
#define FUB_STACK_BOTTOM_LIMIT                                              (0xC000)
#elif FUB_ON_LWDEC
#define FUB_STACK_BOTTOM_LIMIT                                              (0x4000)
#else
ct_assert(0);
#endif

//
// defines to CLEAR/SET DISABLE_EXCEPTIONS in SEC SPR 
// For more info please check Falcon_6_arch doc (Section 2.3.1)
//
#define LW_FALC_SEC_SPR_DISABLE_EXCEPTIONS                                  19:19
#define LW_FALC_SEC_SPR_DISABLE_EXCEPTIONS_CLEAR                            0x00000000
#define LW_FALC_SEC_SPR_DISABLE_EXCEPTIONS_SET                              0x00000001

//
// The item size helps to validate VHR
// The detail dscription of VHR is in //hw/gpu_ip/doc/falcon/arch/TU10x_falcon_6_arch.doc
// 3.5.1.1.4.2.1 PKC HS ucode verification
//
#define VHR_HEADER_SIZE_IN_BYTES                                            (16)
#define VHR_ENTRY_SIZE_IN_BYTES                                             (48)
#define VHR_CHECK_OFFSET_IN_BYTES                                           (4)

// Timeout value for mutex acquire while checking VHR empty.
#define SE_PKA_MUTEX_TIMEOUT_VAL_USEC                                       (0x100000)

// Use case mask defined in FUB and in RM should be in sync.
ct_assert(LW_FUB_USE_CASE_MASK_ENABLE_GSYNC == LW_REG_STR_RM_FUB_USE_CASE_MASK_ENABLE_GSYNC);
ct_assert(LW_FUB_USE_CASE_MASK_REVOKE_DEVID_BASED_HULK == LW_REG_STR_RM_FUB_USE_CASE_MASK_REVOKE_DEVID_BASED_HULK);
ct_assert(LW_FUB_USE_CASE_MASK_ALLOW_DRAM_CHIPID_READ == LW_REG_STR_RM_FUB_USE_CASE_MASK_ALLOW_DRAM_CHIPID_READ_HULK);
ct_assert(LW_FUB_USE_CASE_MASK_BLOCK_DRAM_CHIPID_READ == LW_REG_STR_RM_FUB_USE_CASE_MASK_REVOKE_DRAM_CHIPID_READ_HULK);
ct_assert(LW_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY == LW_REG_STR_RM_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY);
ct_assert(LW_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY_GFW_REV == LW_REG_STR_RM_FUB_USE_CASE_MASK_GEFORCE_SKU_RECOVERY_GFW_REV);
ct_assert(LW_FUB_USE_CASE_MASK_LWFLASH_REV_FOR_WP_BYPASS == LW_REG_STR_RM_FUB_USE_CASE_MASK_LWFLASH_REV_FOR_WP_BYPASS);
ct_assert(LW_FUB_USE_CASE_MASK_GFW_REV_FOR_WP_BYPASS == LW_REG_STR_RM_FUB_USE_CASE_MASK_GFW_REV_FOR_WP_BYPASS);
ct_assert(LW_FUB_USE_CASE_MASK_AUTO_QS2_AHESASC_FUSE == LW_REG_STR_RM_FUB_USE_CASE_MASK_AUTO_QS2_AHESASC_FUSE);
ct_assert(LW_FUB_USE_CASE_MASK_AUTO_QS2_ASB_FUSE == LW_REG_STR_RM_FUB_USE_CASE_MASK_AUTO_QS2_ASB_FUSE);
ct_assert(LW_FUB_USE_CASE_MASK_AUTO_QS2_LWFLASH_FUSE == LW_REG_STR_RM_FUB_USE_CASE_MASK_AUTO_QS2_LWFLASH_FUSE);
ct_assert(LW_FUB_USE_CASE_MASK_AUTO_QS2_IMAGESELECT_FUSE == LW_REG_STR_RM_FUB_USE_CASE_MASK_AUTO_QS2_IMAGESELECT_FUSE);
ct_assert(LW_FUB_USE_CASE_MASK_AUTO_QS2_HULK_FUSE == LW_REG_STR_RM_FUB_USE_CASE_MASK_AUTO_QS2_HULK_FUSE);
ct_assert(LW_FUB_USE_CASE_MASK_AUTO_QS2_FWSEC_FUSE == LW_REG_STR_RM_FUB_USE_CASE_MASK_AUTO_QS2_FWSEC_FUSE);

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

#endif // _FUB_H_
