/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    dev_therm_addendum.h
 * @brief   GPU specific defines that are missing in the dev_therm.h manual.
 */

#ifndef __ga102_dev_therm_addendum_h__
#define __ga102_dev_therm_addendum_h__


#ifndef LW_THERM_EVT_SETTINGS
#define LW_THERM_EVT_SETTINGS(i)                (LW_THERM_EVT_EXT_OVERT+((i)*4))
#endif

#ifndef LW_THERM_EVENT_THERMAL__SIZE_1
#define LW_THERM_EVENT_THERMAL__SIZE_1          12
#endif

// Available range for 9.5 signed fixed point temperature
#define LW_THERM_TS_TEMP_FIXED_POINT_RANGE_MIN                                 0x00002000 /* R---V */
#define LW_THERM_TS_TEMP_FIXED_POINT_RANGE_MAX                                 0x00001fff /* R---V */


#define LW_THERM_SELWRE_WR_SCRATCH_1                                      LW_THERM_SELWRE_WR_SCRATCH(1)
#define LW_THERM_SELWRE_WR_SCRATCH_1_ACR_BSI_LOCK_PHASE_TO_GC6_UDE_HANDOFF                          0:0
#define LW_THERM_SELWRE_WR_SCRATCH_1_ACR_BSI_LOCK_PHASE_TO_GC6_UDE_HANDOFF_VAL_RESET         0x00000000
#define LW_THERM_SELWRE_WR_SCRATCH_1_ACR_BSI_LOCK_PHASE_TO_GC6_UDE_HANDOFF_VAL_ACR_DONE      0x00000001

// Allocating LW_THERM_SELWRE_WR_SCRATCH(3) for GFW Boot Use cases
//
// [0:0]    denotes whether it's okay to execute UDE, i.e., UDE was not launched previously
//          In the GFW boot sequence, this bit will be set before actually launching UDE on PMU.
// [1:1]    denotes whether a valid BRSS structure exists in BSIRAM, created at boot by a GFW uCode
// [6:2]    Stores Mem Table Index
// [7:7]    Reserved to start next set of allocations at a byte boundary
// [8:8]    denotes whether UDE exelwtion completed successfully or with errors
// [9:9]    denotes whether the initialize routines of PreOS Apps have exelwted
//
//
#define LW_THERM_SELWRE_WR_SCRATCH_3                                      LW_THERM_SELWRE_WR_SCRATCH(3)
#define LW_THERM_SELWRE_WR_SCRATCH_3_UDE_GC6_UCODE                                                  0:0
#define LW_THERM_SELWRE_WR_SCRATCH_3_UDE_GC6_UCODE_CAN_EXELWTE                               0x00000000
#define LW_THERM_SELWRE_WR_SCRATCH_3_UDE_GC6_UCODE_CANNOT_EXELWTE                            0x00000001
#define LW_THERM_SELWRE_WR_SCRATCH_3_VALID_BRSS_STRUCT                                              1:1
#define LW_THERM_SELWRE_WR_SCRATCH_3_VALID_BRSS_STRUCT_NOT_CREATED                           0x00000000
#define LW_THERM_SELWRE_WR_SCRATCH_3_VALID_BRSS_STRUCT_CREATED                               0x00000001
#define LW_THERM_SELWRE_WR_SCRATCH_3_MEM_TABLE_INDEX                                                6:2
#define LW_THERM_SELWRE_WR_SCRATCH_3_RESERVED                                                       7:7
#define LW_THERM_SELWRE_WR_SCRATCH_3_UDE_EXELWTION_COMPLETED                                        8:8
#define LW_THERM_SELWRE_WR_SCRATCH_3_UDE_EXELWTION_COMPLETED_FAILED                          0x00000000
#define LW_THERM_SELWRE_WR_SCRATCH_3_UDE_EXELWTION_COMPLETED_SUCCESS                         0x00000001
#define LW_THERM_SELWRE_WR_SCRATCH_3_PREOS_INIT_EXELWTION                                           9:9
#define LW_THERM_SELWRE_WR_SCRATCH_3_PREOS_INIT_EXELWTION_NOT_COMPLETED                      0x00000000
#define LW_THERM_SELWRE_WR_SCRATCH_3_PREOS_INIT_EXELWTION_COMPLETED                          0x00000001

// Bug 200201150
#define LW_THERM_SELWRE_WR_SCRATCH_3_MEMINFO_INDEX                                                  6:2

#define LW_THERM_SELWRE_WR_SCRATCH_7                                      LW_THERM_SELWRE_WR_SCRATCH(7)
#define LW_THERM_SELWRE_WR_SCRATCH_7_SYNC_SCRUBBED_REGION_SIZE_MB_AT_VPR_END                       15:0
#define LW_THERM_SELWRE_WR_SCRATCH_7_SYNC_SCRUBBED_REGION_SIZE_MB_AT_VPR_END_ZERO            0x00000000
#define LW_THERM_SELWRE_WR_SCRATCH_7_SYNC_SCRUBBED_REGION_SIZE_MB_AT_VPR_END_256MB           0x00000100

#ifdef LW_THERM_GPC_TSOSC_INDEX_VALUE
#define LW_THERM_GPC_TSOSC_SENSOR_MASK     DRF_MASK(LW_THERM_GPC_TSOSC_INDEX_VALUE_MAX:LW_THERM_GPC_TSOSC_INDEX_VALUE_MIN)
#endif

//
// This register will be used to store the FB base address of the Playready shared struct between SEC2 and LWDEC
// (The unit of the base address is 256 bytes)
//
#define LW_THERM_SELWRE_WR_SCRATCH_SEC2_LWDEC_SHARED_STRUCT               LW_THERM_SELWRE_WR_SCRATCH(2)
#define LW_THERM_SELWRE_WR_SCRATCH_SEC2_LWDEC_SHARED_STRUCT_BASE_256B                              31:0

#define LW_THERM_SELWRE_WR_SCRATCH_SEC2_LWDEC_SHARED_STRUCT_BASE_BIT_ALIGNMENT                        8

#endif // __ga102_dev_therm_addendum_h__
