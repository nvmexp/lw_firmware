/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012,2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "socpcie.h"

//-----------------------------------------------------------------------------
// Stub PCIE functionality that causes problems with SoC devices since they are
// not really PCIE devices

//-----------------------------------------------------------------------------
/* virtual */ UINT32 SocPcie::DoSubsystemVendorId()
{
    return 0;
}
//-----------------------------------------------------------------------------
/* virtual */ UINT32 SocPcie::DoSubsystemDeviceId()
{
    return 0;
}
/* virtual */ RC SocPcie::DoGetErrorCounts
(
    PexErrorCounts *pLocCounts,
    PexErrorCounts *pHostCounts,
    PexDevice::PexCounterType CountType
)
{
    Printf(Tee::PriHigh, "PCI not available on integrated GPU!\n");
    return RC::UNSUPPORTED_FUNCTION;
}
/* virtual */ RC SocPcie::DoResetErrorCounters()
{
    Printf(Tee::PriHigh, "PCI not available on integrated GPU!\n");
    return RC::UNSUPPORTED_FUNCTION;
}
bool SocPcie::DoAerEnabled() const
{
    return false;
}
void SocPcie::DoEnableAer(bool bEnable)
{
}
/* virtual */ UINT32 SocPcie::DoAspmCapability()
{
    return 0;
}
/* virtual */ RC SocPcie::DoSetAspmState(UINT32 State)
{
    return RC::UNSUPPORTED_FUNCTION;
}
/* virtual */ UINT32 SocPcie::DoAdNormal()
{
    return 0;
}
/* virtual */ RC SocPcie::DoGetErrorCounters(PexErrorCounts *pCounts)
{
    return RC::UNSUPPORTED_FUNCTION;
}
