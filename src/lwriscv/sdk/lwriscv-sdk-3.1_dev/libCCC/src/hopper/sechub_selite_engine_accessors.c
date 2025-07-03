/*
 * Copyright (c) 2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

#include <stdint.h>
#include "tegra_cdev.h"
#include "crypto_api.h"
#include "sechub_selite_engine_accessors.h"
#include <liblwriscv/io.h>
#include <liblwriscv/io_dio.h>

inline void dioSeWriteWrapper(volatile void* addr, uint32_t val)
{
    DIO_PORT dioPort;
    dioPort.dioType = DIO_TYPE_SE;
    dioPort.portIdx = 0;
    // TODO: Check return value for error
    dioReadWrite(dioPort, DIO_OPERATION_WR, (uint32_t)(uintptr_t)addr, &val);
}

inline uint32_t dioSeReadWrapper(volatile void* addr)
{
    DIO_PORT dioPort;
    dioPort.dioType = DIO_TYPE_SE;
    dioPort.portIdx = 0;
    uint32_t val = 0xbadfbadf;
    // TODO: Check return value for error
    dioReadWrite(dioPort, DIO_OPERATION_RD, (uint32_t)(uintptr_t)addr, &val);
    return val;
}

inline void writeWrapper(volatile void* addr, uint32_t val)
{
    localWrite((uint32_t)(uintptr_t)addr, val);
}

inline uint32_t readWrapper(volatile void* addr)
{
    return localRead((uint32_t)(uintptr_t)addr);
}

static inline void ccc_device_set_reg_lwstom(const se_cdev_t *cdev, uint32_t reg, uint32_t val)
{
    if ((cdev->cd_id == SE_CDEV_SE0) || (cdev->cd_id == SE_CDEV_SE1))
    {
        writeWrapper(&cdev->cd_base[reg], val);
    } 
    else if ((cdev->cd_id == SE_CDEV_RNG1) || (cdev->cd_id == SE_CDEV_PKA1))
    {
        dioSeWriteWrapper(&cdev->cd_base[reg], val);
    }

    // TODO: What to do in case device is not recognized?
}

static inline uint32_t ccc_device_get_reg_lwstom(const se_cdev_t *cdev, uint32_t reg)
{
    if ((cdev->cd_id == SE_CDEV_SE0) || (cdev->cd_id == SE_CDEV_SE1))
    {
        return readWrapper(&cdev->cd_base[reg]);
    }
    else if ((cdev->cd_id == SE_CDEV_RNG1) || (cdev->cd_id == SE_CDEV_PKA1))
    {
        return dioSeReadWrapper(&cdev->cd_base[reg]);
    }

    // TODO: Return error in case device is not recognized
    return 0;
}

uint32_t ccc_device_get_reg_normal_redirect(uint32_t *dev, uint32_t reg)
{
    return ccc_device_get_reg_lwstom((const se_cdev_t *)dev, reg);
}

void ccc_device_set_reg_normal_redirect(uint32_t *dev, uint32_t reg, uint32_t data)
{
    ccc_device_set_reg_lwstom((const se_cdev_t *)dev, reg, data);
}
