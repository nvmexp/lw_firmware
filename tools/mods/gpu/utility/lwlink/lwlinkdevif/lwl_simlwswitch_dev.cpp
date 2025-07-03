/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwl_simlwswitch_dev.h"
#include "lwl_devif.h"
#include "lwl_topology_mgr.h"
#include "lwl_topology_mgr_impl.h"
#include "core/include/gpu.h"
#include "core/include/massert.h"
#include "gpu/lwlink/simlwlink.h"

//--------------------------------------------------------------------------
//! \brief Constructor
LwLinkDevIf::SimLwSwitchDev::SimLwSwitchDev(Id i)
: SimDev(i)
{
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::SimLwSwitchDev::GetAteDvddIddq(UINT32 *pIddq)
{
    MASSERT(pIddq);
    *pIddq = 0;
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::SimLwSwitchDev::GetAteIddq(UINT32 *pIddq, UINT32 *pVersion)
{
    MASSERT(pIddq && pVersion);
    *pIddq = 0;
    *pVersion = 0;
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::SimLwSwitchDev::GetAteRev(UINT32 *pAteRev)
{
    MASSERT(pAteRev);
    *pAteRev = 0;
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::SimLwSwitchDev::GetAteSpeedos(vector<UINT32> *pValues, UINT32 * pVersion)
{
    MASSERT(pValues);
    pValues->clear();
    *pVersion = 0;
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::SimLwSwitchDev::GetChipPrivateRevision(UINT32 *pPrivRev)
{
    MASSERT(pPrivRev);
    *pPrivRev = 0;
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::SimLwSwitchDev::GetChipRevision(UINT32 *pRev)
{
    MASSERT(pRev);
    *pRev = 0;
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::SimLwSwitchDev::GetChipSkuModifier(string *pStr)
{
    MASSERT(pStr);
    *pStr = "0";
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::SimLwSwitchDev::GetChipSkuNumber(string *pStr)
{
    MASSERT(pStr);
    *pStr = "SIMSWITCH";
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::SimLwSwitchDev::GetChipTemps(vector<FLOAT32> *pTempsC)
{
    MASSERT(pTempsC);
    pTempsC->push_back(0.0);
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::SimLwSwitchDev::GetClockMHz(UINT32 *pClkMhz)
{
    MASSERT(pClkMhz);
    *pClkMhz = 0;
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::SimLwSwitchDev::GetFoundry(ChipFoundry *pFoundry)
{
    MASSERT(pFoundry);
    *pFoundry = CF_UNKNOWN;
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::SimLwSwitchDev::GetVoltageMv(UINT32 *pMv)
{
    MASSERT(pMv);
    *pMv = 0;
    return OK;
}
