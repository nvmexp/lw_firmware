/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file smbmgr.h
//! \brief Header file with class for Smbus manager
//!
//! Contains class definition for Smbus manager
//!

#ifndef INCLUDED_SMBMGR_H
#define INCLUDED_SMBMGR_H

#ifndef INCLUDED_MCPMGR_H
    #include "device/include/mcpmgr.h"
#endif

//! \brief Class definition for Smbus manager
//-----------------------------------------------------------------------------
class SmbManager : public McpDeviceManager
{
protected:
    virtual RC PrivateFindDevices();
    virtual RC PrivateValidateDevice(UINT32 DomainNum, UINT32 BusNum, UINT32 DeviceNum, UINT32 FunctionNum,
                                     UINT32 * ChipVersion);
    virtual RC SetupDevices(){return OK;}
private:
    UINT32 m_ClassCode = 0;
};

//! \brief Namespace for Smbus manager
namespace SmbMgr
{
    SmbManager *Mgr();
}

#endif  // INCLUDED_SMBMGR_H

