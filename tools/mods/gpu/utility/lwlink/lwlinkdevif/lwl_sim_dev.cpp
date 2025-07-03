/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwl_sim_dev.h"

#include "core/include/gpu.h"
#include "core/include/massert.h"
#include "device/interface/lwlink.h"
#include "device/interface/pcie.h"
#include "gpu/lwlink/simlwlink.h"
#include "lwl_devif.h"

//--------------------------------------------------------------------------
//! \brief Constructor
LwLinkDevIf::SimDev::SimDev(Id i)
  : TestDevice(i)
   ,m_Initialized(false)
{
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::SimDev::GetDeviceErrorList(vector<DeviceError>* pErrorList)
{
    MASSERT(pErrorList);
    pErrorList->clear();
    return OK;
}

//------------------------------------------------------------------------------
 RC LwLinkDevIf::SimDev::GetEcidString(string *pStr)
{
    MASSERT(pStr);
    *pStr = GetName();
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::SimDev::GetRawEcid(vector<UINT32> *pEcids, bool bReverseFields)
{
    MASSERT(pEcids);
    pEcids->clear();
    pEcids->push_back(0);
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Initialize the device
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::SimDev::Initialize()
{
    RC rc;

    if (m_Initialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : LwLink device %s already initialized, skipping\n",
               __FUNCTION__, GetName().c_str());
        return OK;
    }

    Printf(Tee::PriLow, GetTeeModule()->GetCode(),
           "%s : Initializing LwLink device %s\n",
           __FUNCTION__, GetName().c_str());

    CHECK_RC(TestDevice::Initialize());
    if (TestDevice::SupportsInterface<Pcie>())
        CHECK_RC(TestDevice::GetInterface<Pcie>()->Initialize());
    if (TestDevice::SupportsInterface<LwLink>())
        CHECK_RC(TestDevice::GetInterface<LwLink>()->Initialize());

    m_Initialized = true;

    return rc;
}


//------------------------------------------------------------------------------
void LwLinkDevIf::SimDev::Print
(
    Tee::Priority pri,
    UINT32 code,
    string prefix
) const
{
    Printf(pri, code, "%s%s\n", prefix.c_str(), GetName().c_str());
}

//------------------------------------------------------------------------------
string LwLinkDevIf::SimDev::GetName() const
{
    string devTypeInstStr = "";
    if (GetDeviceTypeInstance() != ~0U)
        devTypeInstStr = Utility::StrPrintf("%u ", GetDeviceTypeInstance());

    string topologyStr = "";
    auto pLwLink = TestDevice::GetInterface<LwLink>();
    if (pLwLink && pLwLink->GetTopologyId() != ~0U)
        topologyStr = Utility::StrPrintf("(%u) ", pLwLink->GetTopologyId());

    return Utility::StrPrintf("%s %s%s[%s]",
                              GetTypeString().c_str(),
                              devTypeInstStr.c_str(),
                              topologyStr.c_str(),
                              GetId().GetString().c_str());
}
