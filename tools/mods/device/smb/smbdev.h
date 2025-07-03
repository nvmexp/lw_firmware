/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file smbdev.h
//! \brief Header file with class for Smbus devices
//!
//! Contains class definition for Smbus devices
//!

#ifndef INCLUDED_SMBDEV_H
#define INCLUDED_SMBDEV_H

#ifndef INCLUDED_MCPDEV_H
    #include "device/include/mcpdev.h"
#endif

#ifndef INCLUDED_SMBMASK_H
#include "smbmask.h"
#endif

#include<vector>
class SmbPort;

//! \brief Class definition for Smbus devices
//-----------------------------------------------------------------------------
class SmbDevice : public McpDev
{
public:
    SmbDevice(UINT32 Domain, UINT32 Bus, UINT32 Dev, UINT32 Func, UINT32 ChipVersion); //Constructor
    virtual ~SmbDevice();               //Destructor

    virtual RC Initialize(InitStates Target);
    virtual RC Uninitialize(InitStates Target);
    virtual RC Search();
    virtual RC PrintReg(Tee::Priority Pri = Tee::PriNormal);

protected:
    virtual RC DeviceRegRd(UINT32 Offset, UINT32* pData);
    virtual RC DeviceRegWr(UINT32 Offset, UINT32 Data);
    virtual RC PrintInfoController(Tee::Priority priority);
    virtual RC PrintInfoExternal(Tee::Priority priority){return OK;}

private:
    UINT32    m_InitCount;
};

#endif  // INCLUDED_SMBDEV_H
