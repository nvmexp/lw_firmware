/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file smbmask.h
//! \brief CPP file with class for Smbus Mask
//!
//! Contains class implementation for Smbus Mask
//!

#include "smbmask.h"
#include "core/include/platform.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME  "SmbusMask"

static const UINT32 SMB_MASK_BIT = 0x3;
static const UINT32 SMB_MASK_OFFSET = 0x3418;
static const UINT32 SMB_MASK_REG_DOMAIN = 0;
static const UINT32 SMB_MASK_REG_BUS = 0;
static const UINT32 SMB_MASK_REG_DEV = 31;
static const UINT32 SMB_MASK_REG_FUN = 0;
static const UINT32 SMB_MASK_REG_OFFSET = 0xf0;
static const UINT32 IOBIT = 1;

bool SmbusMask::IsToggled = false;

RC SmbusMask::ToggleOn()
{
    RC rc = OK;
    UINT32 SmbMaskReg;
    UINT32 SmbMaskVal;
    UINT32 *pSmbMaskAddr = NULL;

    Printf(Tee::PriHigh,"Smbus not found; toggling mask bit\n");

    CHECK_RC(Platform::PciRead32(SMB_MASK_REG_DOMAIN, SMB_MASK_REG_BUS, SMB_MASK_REG_DEV,
                                 SMB_MASK_REG_FUN, SMB_MASK_REG_OFFSET,&SmbMaskReg));
    SmbMaskReg = (SmbMaskReg & (~IOBIT)) + SMB_MASK_OFFSET;
    CHECK_RC(Platform::MapDeviceMemory((void**)&pSmbMaskAddr, SmbMaskReg, 4, Memory::UC,
                                       Memory::ReadWrite));
    SmbMaskVal = (UINT32)(*pSmbMaskAddr);
    *pSmbMaskAddr = SmbMaskVal & (~(1 << SMB_MASK_BIT));
    Platform::RescanPciBus(SMB_MASK_REG_DOMAIN, SMB_MASK_REG_BUS);
    Platform::UnMapDeviceMemory(pSmbMaskAddr);
    IsToggled = true;
    return rc;
}

void SmbusMask::ToggleOff()
{
    UINT32 SmbMaskReg;
    UINT32 SmbMaskVal;
    UINT32 *pSmbMaskAddr = NULL;

    if (!IsToggled)
        return;

    Platform::PciRead32(SMB_MASK_REG_DOMAIN, SMB_MASK_REG_BUS, SMB_MASK_REG_DEV,
                        SMB_MASK_REG_FUN, SMB_MASK_REG_OFFSET,&SmbMaskReg);
    SmbMaskReg = (SmbMaskReg & (~IOBIT)) + SMB_MASK_OFFSET;
    Platform::MapDeviceMemory((void**)&pSmbMaskAddr, SmbMaskReg, 4, Memory::UC,
                                       Memory::ReadWrite);

    SmbMaskVal = *pSmbMaskAddr;
    *pSmbMaskAddr = SmbMaskVal | (1 << SMB_MASK_BIT);
    Platform::UnMapDeviceMemory(pSmbMaskAddr);
}
