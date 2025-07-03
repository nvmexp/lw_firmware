/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "simpcie.h"

//------------------------------------------------------------------------------
/* virtual */ UINT32 SimPcie::DoDeviceId() const
{
    return 0;
}

//------------------------------------------------------------------------------
/* virtual */ RC SimPcie::DoGetAspmL1ssEnabled(UINT32 *pAspm)
{
    MASSERT(pAspm);
    *pAspm = 0;
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 SimPcie::DoGetAspmState()
{
    return 0;
}

//------------------------------------------------------------------------------
/* virtual */ RC SimPcie::DoGetLTREnabled(bool *pEnabled)
{
    MASSERT(pEnabled);
    *pEnabled = false;
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ Pci::PcieLinkSpeed SimPcie::DoGetLinkSpeed(Pci::PcieLinkSpeed expectedSpeed)
{
    return Pci::Speed8000MBPS;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 SimPcie::DoGetLinkWidth()
{
    return 0;
}

//------------------------------------------------------------------------------
/* virtual */ RC SimPcie::DoGetLinkWidths(UINT32* pLocalWidth, UINT32* pHostWidth)
{
    MASSERT(pLocalWidth);
    MASSERT(pHostWidth);
    *pLocalWidth = 0;
    *pHostWidth = 0;
    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC SimPcie::DoGetUpStreamInfo(PexDevice** pPexDev)
{
    MASSERT(pPexDev);
    *pPexDev = nullptr;
    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC SimPcie::DoGetUpStreamInfo(PexDevice** pPexDev, UINT32* pPort)
{
    MASSERT(pPexDev);
    MASSERT(pPort);
    *pPexDev = nullptr;
    *pPort   = 0;
    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC SimPcie::DoSetUpStreamDevice(PexDevice* pPexDev, UINT32 index)
{
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 SimPcie::DoSubsystemDeviceId()
{
    return 0;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 SimPcie::DoSubsystemVendorId()
{
    return 0;
}
