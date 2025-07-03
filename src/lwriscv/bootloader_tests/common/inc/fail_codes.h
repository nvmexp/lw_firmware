/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   fail_codes.h
 * @brief  Header defining fail code values.
 */

#ifndef FAIL_CODES_H_
#define FAIL_CODES_H_

#define FAILCODE_BUILD(major, minor, idx) \
    (((major) << 16) + ((minor) << 8) + (idx))

// Test succeeded
#define FAILCODE_SUCCESS                        -1

// MPU setting in mstatus is not correct
#define FAILCODE_MPU_NOT_ENABLED                FAILCODE_BUILD(1, 0,0)
// MPU region VA setting did not match expected value
#define FAILCODE_MPU_REGION_VA_MISMATCH(idx)    FAILCODE_BUILD(2, 0, idx)
// MPU region PA setting did not match expected value
#define FAILCODE_MPU_REGION_PA_MISMATCH(idx)    FAILCODE_BUILD(2, 1, idx)
// MPU region RNG setting did not match expected value
#define FAILCODE_MPU_REGION_RNG_MISMATCH(idx)   FAILCODE_BUILD(2, 2, idx)
// MPU region ATTR setting did not match expected value
#define FAILCODE_MPU_REGION_ATTR_MISMATCH(idx)  FAILCODE_BUILD(2, 3, idx)

// Bootloader did not jump to _start
#define FAILCODE_ENTRYPOINT_WRONG               FAILCODE_BUILD(3, 0,0)
// Bootloader did not clear all GPRs (other than ra)
#define FAILCODE_GPRS_NOT_CLEARED               FAILCODE_BUILD(4, 0,0)
// Bootloader did not set mstatus correctly
// This is generally caused by incorrect MPU configuration
#define FAILCODE_MSTATUS_NOT_CORRECT            FAILCODE_BUILD(5, 0,0)
// Bootloader did not clear MPU regions
#define FAILCODE_MPU_REGION_CONTENTS            FAILCODE_BUILD(6, 0,0)
// .text section loaded improperly
#define FAILCODE_TEXT_SECTION                   FAILCODE_BUILD(7, 0,0)
// .rodata section loaded improperly
#define FAILCODE_RODATA_SECTION                 FAILCODE_BUILD(7, 0,1)
// .data section loaded improperly
#define FAILCODE_DATA_SECTION                   FAILCODE_BUILD(7, 0,2)
// .bss section loaded improperly
#define FAILCODE_BSS_SECTION                    FAILCODE_BUILD(7, 0,3)
// .stack section loaded improperly
#define FAILCODE_STACK_SECTION                  FAILCODE_BUILD(7, 0,4)
// .noload0 section (not) loaded improperly
#define FAILCODE_NOLOAD0_SECTION                FAILCODE_BUILD(7, 0,5)
// .noload1 section (not) loaded improperly
#define FAILCODE_NOLOAD1_SECTION                FAILCODE_BUILD(7, 0,6)
// .noload2 section (not) loaded improperly
#define FAILCODE_NOLOAD2_SECTION                FAILCODE_BUILD(7, 0,7)
// cycle perfctr is zero
#define FAILCODE_PERFCTR_CYCLE_ZERO             FAILCODE_BUILD(8, 0,0)
// cycle perfctr is stuck
#define FAILCODE_PERFCTR_CYCLE_STUCK            FAILCODE_BUILD(8, 0,1)
// time perfctr is zero
#define FAILCODE_PERFCTR_TIME_ZERO              FAILCODE_BUILD(8, 1,0)
// time perfctr is stuck
#define FAILCODE_PERFCTR_TIME_STUCK             FAILCODE_BUILD(8, 1,1)
// instret perfctr is zero
#define FAILCODE_PERFCTR_INSTRET_ZERO           FAILCODE_BUILD(8, 2,0)
// instret perfctr is stuck
#define FAILCODE_PERFCTR_INSTRET_STUCK          FAILCODE_BUILD(8, 2,1)
// exception handler contents are incorrect
#define FAILCODE_BAD_EXCEPTION_HANDLER          FAILCODE_BUILD(9, 0,0)
// DTCM is not cleared to 0x5A
#define FAILCODE_DTCM_NOT_CLEAN                 FAILCODE_BUILD(10, 0,0)
// permutedData did not result in the correct value
#define FAILCODE_PERMUTED_DATA_WRONG            FAILCODE_BUILD(11, 0,0)

#endif
