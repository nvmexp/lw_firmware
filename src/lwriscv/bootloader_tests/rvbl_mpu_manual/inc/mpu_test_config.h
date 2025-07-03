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
 * @file   mpu_test_config.h
 * @brief  Test data for MPU bootloader test.
 */

// Must match the load base in make-profile.lwmk
#define LOAD_BASE_ADDR 0x6180000000807000

#define PERMUTED_DATA_STARTING_VALUE (0x87654321)
#define PERMUTED_DATA_EXPECTED_VALUE (0x87657326)

#define EXPECTED_MPU_SETTINGS { \
    /* Manual region 0 (.text) */ \
    {   .vaBase = 0x0000000100000001, \
        .paBase = 0x0000000000000000 + LOAD_BASE_ADDR, \
        .range  = 0x0000000000001000, \
        .attribute = CSR_LW_MMPUATTR_MR | CSR_LW_MMPUATTR_MX, \
    }, \
    /* Manual region 1 (.rodata) */ \
    {   .vaBase = 0x0000000100001001, \
        .paBase = 0x0000000000001000 + LOAD_BASE_ADDR, \
        .range  = 0x0000000000001000, \
        .attribute = CSR_LW_MMPUATTR_MR, \
    }, \
    /* Manual region 2 (.text) */ \
    {   .vaBase = 0x0000000100002001, \
        .paBase = 0x0000000000002000 + LOAD_BASE_ADDR, \
        .range  = 0x0000000000003000, \
        .attribute = CSR_LW_MMPUATTR_MR | CSR_LW_MMPUATTR_MW, \
    }, \
    /* Manual region 3 */ \
    {   .vaBase = 0x0000000200000001, \
        .paBase = 0x6180000000000000, \
        .range  = 0x0000000100000000, \
        .attribute = CSR_LW_MMPUATTR_MR | CSR_LW_MMPUATTR_MW | CSR_LW_MMPUATTR_MX | \
                     CSR_LW_MMPUATTR_CACHEABLE | \
                     CSR_LW_MMPUATTR_COHERENT, \
    }, \
    /* Manual region 4 */ \
    {   .vaBase = 0x0000000300000001, \
        .paBase = 0x2000000000000000, \
        .range  = 0x0000000100000000, \
        .attribute = CSR_LW_MMPUATTR_MR | CSR_LW_MMPUATTR_MW | CSR_LW_MMPUATTR_MX, \
    }, \
    /* remainder are zero */ \
}
