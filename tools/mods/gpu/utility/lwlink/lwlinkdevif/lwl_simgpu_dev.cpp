/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwl_simgpu_dev.h"
#include "lwl_devif.h"
#include "core/include/gpu.h"
#include "core/include/massert.h"

//------------------------------------------------------------------------------
RC LwLinkDevIf::SimGpuDev::GetAteIddq(UINT32 *pIddq, UINT32 *pVersion)
{
    MASSERT(pIddq && pVersion);
    *pIddq = 0;
    *pVersion = 0;
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::SimGpuDev::GetAteRev(UINT32 *pAteRev)
{
    MASSERT(pAteRev);
    *pAteRev = 0;
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::SimGpuDev::GetAteSpeedos(vector<UINT32> *pValues, UINT32 * pVersion)
{
    MASSERT(pValues);
    pValues->clear();
    *pVersion = 0;
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::SimGpuDev::GetChipPrivateRevision(UINT32 *pPrivRev)
{
    MASSERT(pPrivRev);
    *pPrivRev = 0;
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::SimGpuDev::GetChipRevision(UINT32 *pRev)
{
    MASSERT(pRev);
    *pRev = 0;
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::SimGpuDev::GetChipSkuModifier(string *pStr)
{
    MASSERT(pStr);
    *pStr = "0";
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::SimGpuDev::GetChipSkuNumber(string *pStr)
{
    MASSERT(pStr);
    *pStr = "SIMGPU";
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::SimGpuDev::GetChipTemps(vector<FLOAT32> *pTempsC)
{
    MASSERT(pTempsC);
    pTempsC->push_back(0.0);
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::SimGpuDev::GetClockMHz(UINT32 *pClkMhz)
{
    MASSERT(pClkMhz);
    *pClkMhz = 0;
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::SimGpuDev::GetFoundry(ChipFoundry *pFoundry)
{
    MASSERT(pFoundry);
    *pFoundry = CF_UNKNOWN;
    return OK;
}
