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

#define PERMUTED_DATA_STARTING_VALUE (0x12345678)
#define PERMUTED_DATA_EXPECTED_VALUE (0x1234867B)

#define EXPECTED_MPU_SETTINGS { \
    /* For figuring out these appropriate values, refer to the readelf */ \
    /* Auto region 0 (.text) */ \
    {   .vaBase = 0x0000000100000001, \
        .paBase = 0x0000000000000000 + LOAD_BASE_ADDR, \
        .range  = 0x0000000000001000, \
        .attribute = CSR_LW_MMPUATTR_UR | CSR_LW_MMPUATTR_UX | \
                     CSR_LW_MMPUATTR_MR | CSR_LW_MMPUATTR_MX | \
                     CSR_LW_MMPUATTR_CACHEABLE | \
                     CSR_LW_MMPUATTR_COHERENT, \
    }, \
    /* Auto region 1 (.rodata) */ \
    {   .vaBase = 0x0000000100001001, \
        .paBase = 0x0000000000001000 + LOAD_BASE_ADDR, \
        .range  = 0x0000000000001000, \
        .attribute = CSR_LW_MMPUATTR_UR | \
                     CSR_LW_MMPUATTR_MR | \
                     CSR_LW_MMPUATTR_CACHEABLE | \
                     CSR_LW_MMPUATTR_COHERENT, \
    }, \
    /* Auto region 2 (.text) */ \
    {   .vaBase = 0x0000000100002001, \
        .paBase = 0x0000000000002000 + LOAD_BASE_ADDR, \
        .range  = 0x0000000000003000, \
        .attribute = CSR_LW_MMPUATTR_UR | CSR_LW_MMPUATTR_UW | \
                     CSR_LW_MMPUATTR_MR | CSR_LW_MMPUATTR_MW | \
                     CSR_LW_MMPUATTR_CACHEABLE | \
                     CSR_LW_MMPUATTR_COHERENT, \
    }, \
    /* remainder are zero */ \
}
