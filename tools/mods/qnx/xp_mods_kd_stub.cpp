/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "xp_mods_kd.h"

// stub functionality to have dependent code compile in QNX.
namespace Xp
{
    bool MODSKernelDriver::IsOpen() const { return false; }
    bool MODSKernelDriver::IsDevMemOpen() const { return false; }
    RC  MODSKernelDriver::Open(const char* exePath) { return OK; } ;
    RC  MODSKernelDriver::Close() { return OK; } ;
    UINT32  MODSKernelDriver::GetVersion() const { return 0; }
    RC  MODSKernelDriver::GetKernelVersion(UINT64* pVersion) { return OK; } ;
    int MODSKernelDriver::Ioctl(unsigned long fn, void* param) const {return 0;};

    MODSKernelDriver s_Obj;
}

Xp::MODSKernelDriver* Xp::GetMODSKernelDriver()
{
    return &s_Obj;
}

