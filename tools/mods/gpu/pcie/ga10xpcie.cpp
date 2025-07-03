/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "ga10xpcie.h"

RC GA10xPcie::DoGetEomStatus(Pci::FomMode mode, vector<UINT32>* status, FLOAT64 timeoutMs)
{
    if (!SupportsFomMode(mode))
    {
        Printf(Tee::PriError, "Unsupported FOM mode : %d\n", mode);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    MASSERT(status);
    RC rc;
    const UINT32 numLanes = static_cast<UINT32>(status->size());
    const UINT32 rxLaneMask = GetRegLanesRx();
    MASSERT(rxLaneMask);
    MASSERT(numLanes == static_cast<UINT32>(Utility::CountBits(rxLaneMask)));
    const UINT32 firstLane =
        (rxLaneMask) ? static_cast<UINT32>(Utility::BitScanForward(rxLaneMask)) : 0;

    EomSettings settings;
    CHECK_RC(GetEomSettings(mode, &settings));
    LW2080_CTRL_BUS_GET_EOM_STATUS_PARAMS eomParams = { };
    eomParams.eomMode      = settings.mode;
    eomParams.eomNblks     = settings.numBlocks;
    eomParams.eomNerrs     = settings.numErrors;
    eomParams.eomBerEyeSel = settings.berEyeSel;
    eomParams.laneMask     = rxLaneMask;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        GetSubdevice(),
        LW2080_CTRL_CMD_BUS_GET_EOM_STATUS,
        &eomParams,
        sizeof(eomParams)
    ));

    for (UINT32 lwrLane = firstLane; lwrLane < (firstLane + numLanes); lwrLane++)
    {
        status->at(lwrLane - firstLane) = eomParams.eomStatus[lwrLane];
    }
    return RC::OK;
}

RC GA10xPcie::GetEomSettings(Pci::FomMode mode, EomSettings* pSettings)
{
    if (mode != Pci::EYEO_Y)
    {
        Printf(Tee::PriError, "Invalid or unsupported FOM mode : %d\n", mode);
        return RC::ILWALID_ARGUMENT;
    }

    constexpr UINT32 eomModeY = 0xb;
    pSettings->mode      = eomModeY;
    pSettings->numBlocks = 0xa;
    pSettings->numErrors = 0x7;
    return RC::OK;
}
