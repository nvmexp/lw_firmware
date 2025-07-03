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

#include "lwlwl_test_route.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/lwca/lwdawrap.h"

#include <cmath>

using namespace LwLinkDataTestHelper;

//------------------------------------------------------------------------------
//! \brief Constructor
//!
TestRouteLwda::TestRouteLwda
(
     const Lwca::Instance* pLwda
    ,TestDevicePtr pLocDev
    ,LwLinkRoutePtr pRoute
    ,Tee::Priority pri
) : TestRoute(pLocDev, pRoute, pri)
   ,m_pLwda(pLwda)
{
}

//------------------------------------------------------------------------------
// Get the number of Sms required to saturate each direction of the route
RC TestRouteLwda::GetRequiredSms
(
    TransferDir transDir
   ,UINT32 transferWidth
   ,bool bHwLocal
   ,UINT32 readBwPerSmKiBps
   ,UINT32 writeBwPerSmKiBps
   ,UINT32 minSms
   ,UINT32 maxSms
   ,UINT32 *pDutInSmCount
   ,UINT32 *pDutOutSmCount
)
{
    MASSERT(pDutInSmCount);
    MASSERT(pDutOutSmCount);

    *pDutInSmCount = 0;
    *pDutOutSmCount = 0;
    if (!bHwLocal && (GetTransferType() != TT_P2P))
    {
        Printf(Tee::PriError,
               "%s : Remote hardware may only be used with P2P transfers\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    if ((GetTransferType() == TT_LOOPBACK) && (transDir != TD_IN_OUT))
    {
        Printf(Tee::PriError,
               "%s : Loopback only supported in in/out mode!\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    const UINT32 bwKiBps = GetMaxBandwidthKiBps(transferWidth);

    UINT32 minSmCount = minSms;
    if (minSmCount == ALL_SMS)
        minSmCount = bHwLocal ? m_LocalMaxSms : m_RemoteMaxSms;

    if (transDir != TD_OUT_ONLY)
    {
        const UINT32 bwPerSmKiBps = bHwLocal ? readBwPerSmKiBps : writeBwPerSmKiBps;
        *pDutInSmCount = static_cast<UINT32>(ceil(static_cast<FLOAT64>(bwKiBps) / bwPerSmKiBps));
        if (*pDutInSmCount > maxSms)
            *pDutInSmCount = maxSms;
        if (*pDutInSmCount < minSmCount)
            *pDutInSmCount = minSmCount;
    }

    if (transDir != TD_IN_ONLY)
    {
        const UINT32 bwPerSmKiBps = bHwLocal ? writeBwPerSmKiBps : readBwPerSmKiBps;
        *pDutOutSmCount = static_cast<UINT32>(ceil(static_cast<FLOAT64>(bwKiBps) / bwPerSmKiBps));
        if (*pDutOutSmCount > maxSms)
            *pDutOutSmCount = maxSms;
        if (*pDutOutSmCount < minSmCount)
            *pDutOutSmCount = minSmCount;
    }
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Initialize the test route
//!
//! \return OK if successful, not OK otherwise
RC TestRouteLwda::Initialize()
{
    RC rc;

    CHECK_RC(TestRoute::Initialize());

    CHECK_RC(m_pLwda->FindDevice(*GetLocalSubdevice()->GetParentDevice(), &m_LocalLwdaDev));
    m_LocalMaxSms     = m_LocalLwdaDev.GetShaderCount();
    m_LocalMaxThreads =
        m_LocalMaxSms *
        m_LocalLwdaDev.GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR);

    m_RemoteMaxSms = 0;
    m_RemoteMaxThreads  = 0;
    if (GetTransferType() == TT_P2P)
    {
        CHECK_RC(m_pLwda->FindDevice(*GetRemoteSubdevice()->GetParentDevice(), &m_RemoteLwdaDev));
        m_RemoteMaxSms     = m_RemoteLwdaDev.GetShaderCount();
        m_RemoteMaxThreads =
            m_RemoteMaxSms *
            m_RemoteLwdaDev.GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR);
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Get the maximum possible bandwidth on this route based on the transfer width
UINT32 TestRouteLwda::GetMaxBandwidthKiBps(UINT32 transferWidth)
{
    FLOAT64 readDataFlits;
    FLOAT64 writeDataFlits;
    UINT32  totalReadFlits;
    UINT32  totalWriteFlits;
    auto pLwLink = GetLocalSubdevice()->GetInterface<LwLink>();
    pLwLink->GetFlitInfo(transferWidth,
                         LwLinkImpl::HT_SM,
                         false,
                         &readDataFlits,
                         &writeDataFlits,
                         &totalReadFlits,
                         &totalWriteFlits);

    TestDevicePtr pDev = GetLocalLwLinkDev();
    const UINT32 rteMaxBwKiBps = GetRoute()->GetMaxDataBwKiBps(pDev, LwLink::DT_REQUEST);

    // The maximum possible bandwidth depends ulitmately on how the HW is assigned,
    // however the top bandwidth will occur in unidirectional non-system memory writes
    const FLOAT64 packetEfficiency =
        (writeDataFlits / (static_cast<FLOAT64>(totalWriteFlits) + 1.0));

    return static_cast<UINT32>(rteMaxBwKiBps * packetEfficiency);
}
