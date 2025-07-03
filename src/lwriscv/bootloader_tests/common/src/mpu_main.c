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
 * @file   main.c
 * @brief  MPU bootloader test application main.
 */

#include <stdint.h>

#include <lwriscv-csr.h>
#include <lwriscv-mpu.h>
#include "fail_codes.h"

// 2 bootloader reserved entries
#define MPU_SETTING_CHECK_COUNT (CSR_LW_MPU_ENTRY_COUNT - 2)

typedef struct LW_RISCV_MPU_REGION
{
    uint64_t vaBase;
    uint64_t paBase;
    uint64_t range;
    uint64_t attribute;
} LW_RISCV_MPU_REGION,
*PLW_RISCV_MPU_REGION;

#include "mpu_test_config.h"

static const LW_RISCV_MPU_REGION expectedMpuSettings[MPU_SETTING_CHECK_COUNT] = EXPECTED_MPU_SETTINGS;

static LW_RISCV_MPU_REGION readMpuSettings[MPU_SETTING_CHECK_COUNT];

static uint32_t permutedData = PERMUTED_DATA_STARTING_VALUE;

int main(void)
{
    if (((csr_read(CSR_MSTATUS) >> 24) & 0x1F) != 0x1E)
    {
        return FAILCODE_MPU_NOT_ENABLED;
    }

    for (int i = 0; i < MPU_SETTING_CHECK_COUNT; i++)
    {
        csr_write(CSR_LW_MMPUIDX, i);
        readMpuSettings[i].vaBase = csr_read(CSR_LW_MMPUVA);
        readMpuSettings[i].paBase = csr_read(CSR_LW_MMPUPA);
        readMpuSettings[i].range = csr_read(CSR_LW_MMPURNG);
        readMpuSettings[i].attribute = csr_read(CSR_LW_MMPUATTR);
        permutedData += readMpuSettings[i].vaBase;
    }

    for (int i = 0; i < MPU_SETTING_CHECK_COUNT; i++)
    {
        if (readMpuSettings[i].vaBase != expectedMpuSettings[i].vaBase)
        {
            return FAILCODE_MPU_REGION_VA_MISMATCH(i);
        }
        if (readMpuSettings[i].paBase != expectedMpuSettings[i].paBase)
        {
            return FAILCODE_MPU_REGION_PA_MISMATCH(i);
        }
        if (readMpuSettings[i].range != expectedMpuSettings[i].range)
        {
            return FAILCODE_MPU_REGION_RNG_MISMATCH(i);
        }
        if (readMpuSettings[i].attribute != expectedMpuSettings[i].attribute)
        {
            return FAILCODE_MPU_REGION_ATTR_MISMATCH(i);
        }
    }

    if (permutedData != PERMUTED_DATA_EXPECTED_VALUE)
    {
        return FAILCODE_PERMUTED_DATA_WRONG;
    }

    return FAILCODE_SUCCESS;
}
