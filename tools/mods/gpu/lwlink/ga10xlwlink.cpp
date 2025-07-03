/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "ga10xlwlink.h"

//------------------------------------------------------------------------------
FLOAT64 GA10xLwLink::DoGetDefaultErrorThreshold
(
    LwLink::ErrorCounts::ErrId errId,
    bool bRateErrors
) const
{
    MASSERT(LwLink::ErrorCounts::IsThreshold(errId));
    if (bRateErrors)
    {
        if (errId == LwLink::ErrorCounts::LWL_RX_CRC_FLIT_ID)
            return 1e-15;
        if (errId == LwLink::ErrorCounts::LWL_PRE_FEC_ID)
            return 1e-5;    
    }
    return 0.0;
}

//-----------------------------------------------------------------------------
void GA10xLwLink::DoGetEomDefaultSettings
(
    UINT32 link,
    EomSettings* pSettings
) const
{
    TuringLwLink::DoGetEomDefaultSettings(link, pSettings);
}

//-----------------------------------------------------------------------------
bool GA10xLwLink::DoSupportsFomMode(FomMode mode) const
{
    switch (mode)
    {
        case EYEO_Y:
            return true;
        default:
            return false;
    }
}

RC GA10xLwLink::GetEomConfigValue
(
    FomMode mode,
    UINT32 numErrors,
    UINT32 numBlocks,
    UINT32 *pConfigVal
) const
{
    // GA10x uses the same EOM config value as Volta
    return VoltaLwLink::GetEomConfigValue(mode, numErrors, numBlocks, pConfigVal);
}
