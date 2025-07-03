/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2017,2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwlbws_test_route.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gralloc.h"
#include "gpu/lwlink/lwlinkimpl.h"
#include "ctrl/ctrl0000.h"
#include "ctrl/ctrl2080.h"

#include <cmath>

using namespace LwLinkDataTestHelper;

//------------------------------------------------------------------------------
//! \brief Constructor
//!
TestRouteCe2d::TestRouteCe2d
(
     TestDevicePtr  pLocDev
    ,LwLinkRoutePtr pRoute
    ,Tee::Priority  pri
) : TestRoute(pLocDev, pRoute, pri)
{
}

//------------------------------------------------------------------------------
//! \brief Get the number of CEs required to saturate each direction of the
//! route
//!
//! \param transDir       : Direction of transfers being performed
//! \param bLocalCes      : True if the CEs are allocated locally
//! \param pDutInCeCount  : Pointer to returned CE count for testing DUT input
//! \param pDutOutCeCount : Pointer to returned CE count for testing DUT output
//!
//! \return OK if successful, not OK otherwise
RC TestRouteCe2d::GetRequiredCes
(
     TransferDir transDir
    ,bool bLocalCes
    ,UINT32 *pDutInCeCount
    ,UINT32 *pDutOutCeCount
)
{
    MASSERT(pDutInCeCount);
    MASSERT(pDutOutCeCount);

    *pDutInCeCount = 0;
    *pDutOutCeCount = 0;
    if (!bLocalCes && (GetTransferType() != TT_P2P))
    {
        Printf(Tee::PriError,
               "%s : Remote CEs may only be used with P2P transfers\n",
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

    UINT32 bwKiBps = GetMaxBandwidthKiBps();
    UINT32 ceBw = bLocalCes ? m_LocalCeBw : m_RemoteCeBw;
    MASSERT(ceBw);

    if (transDir != TD_OUT_ONLY)
    {
        // Dont allow oversubscribing CEs to the link, at POR clocks one CE per
        // width should saturate the bandwidth.  Allowing oversubscribing can
        // cause the test to fail due to CE allocation failing
        *pDutInCeCount = min((bwKiBps + ceBw - 1) / ceBw, GetWidth());
    }

    if (transDir != TD_IN_ONLY)
    {
        *pDutOutCeCount = min((bwKiBps + ceBw - 1) / ceBw, GetWidth());
    }
    return OK;
}

//------------------------------------------------------------------------------
RC TestRouteCe2d::GetUsableCeMasks
(
     bool        bLocalCes
    ,UINT32 *    pDutInCeMask
    ,UINT32 *    pDutOutCeMask
)
{
    MASSERT(pDutInCeMask);
    MASSERT(pDutOutCeMask);

    *pDutInCeMask = bLocalCes ? m_LocalReadCeMask : m_RemoteWriteCeMask;
    *pDutOutCeMask = bLocalCes ? m_LocalWriteCeMask : m_RemoteReadCeMask;

    return RC::OK;
}

//------------------------------------------------------------------------------
//! \brief Initialize the test route
//!
//! \param pErrorInjector : Pointer to error injector for the route
//!
//! \return OK if successful, not OK otherwise
RC TestRouteCe2d::Initialize()
{
    RC rc;

    CHECK_RC(TestRoute::Initialize());

    CHECK_RC(GetLocalSubdevice()->GetMaxCeBandwidthKiBps(&m_LocalCeBw));

    // Determine the number of copy engines that are present on the DUT
    DmaCopyAlloc ceAlloc;
    const UINT32 *pClassList;
    UINT32  numClasses;
    ceAlloc.GetClassList(&pClassList, &numClasses);
    vector<UINT32> engines;
    vector<UINT32> classIds(pClassList, pClassList + numClasses);

    GpuDevice *pDev = GetLocalSubdevice()->GetParentDevice();
    CHECK_RC(pDev->GetPhysicalEnginesFilteredByClass(classIds, &engines));
    m_LocalCeCount = static_cast<UINT32>(engines.size());
    m_LocalCeCopyWidthSys  =
        GetLocalSubdevice()->GetInterface<LwLink>()->GetCeCopyWidth(LwLinkImpl::CET_SYSMEM);
    m_LocalCeCopyWidthPeer =
        GetLocalSubdevice()->GetInterface<LwLink>()->GetCeCopyWidth(LwLinkImpl::CET_PEERMEM);

    m_LocalReadCeMask = 0;
    m_LocalWriteCeMask = 0;
    if (GetTransferType() != TT_SYSMEM)
    {
        // gpuIds[0] is always the source GPU and gpuIds[1] is the target GPU
        // in the RM control call
        LW0000_CTRL_SYSTEM_GET_P2P_CAPS_V2_PARAMS  p2pCapsParams = { };
        p2pCapsParams.gpuIds[0] = GetLocalSubdevice()->GetGpuId();
        if (GetTransferType() == TT_LOOPBACK)
        {
            p2pCapsParams.gpuIds[1] = p2pCapsParams.gpuIds[0];
        }
        else
        {
            p2pCapsParams.gpuIds[1] = GetRemoteSubdevice()->GetGpuId();
        }
        p2pCapsParams.gpuCount = 2;
        CHECK_RC(LwRmPtr()->Control(LwRmPtr()->GetClientHandle(),
                                    LW0000_CTRL_CMD_SYSTEM_GET_P2P_CAPS_V2,
                                    &p2pCapsParams,
                                    sizeof(p2pCapsParams)));
        m_LocalReadCeMask  = p2pCapsParams.p2pOptimalReadCEs;
        m_LocalWriteCeMask = p2pCapsParams.p2pOptimalWriteCEs;

        GpuSubdevice * pRem = (GetTransferType() == TT_LOOPBACK) ? GetLocalSubdevice() :
                                                                   GetRemoteSubdevice();
        Printf(Tee::PriLow, "Access CEs from GPU %s to %s : Read 0x%x, Write 0x%x\n",
               GetLocalSubdevice()->GpuIdentStr().c_str(),
               pRem->GpuIdentStr().c_str(),
               m_LocalReadCeMask,
               m_LocalWriteCeMask);
    }
    else
    {
        UINT32 genericSysmemMask = 0;

        // First try to find a RM recommended LCE for system memory transfers
        // if one does not exist fall back and find a LCE with at least one PCE
        // per-lwlink
        for (auto const & lwrEngine : engines)
        {
            LW2080_CTRL_CE_GET_CAPS_V2_PARAMS capParams = { };
            capParams.ceEngineType = lwrEngine;
            CHECK_RC(LwRmPtr()->ControlBySubdevice(GetLocalSubdevice(),
                                                   LW2080_CTRL_CMD_CE_GET_CAPS_V2,
                                                   &capParams,
                                                   sizeof(capParams)));

            if (LW2080_CTRL_CE_GET_CAP(capParams.capsTbl, LW2080_CTRL_CE_CAPS_CE_SYSMEM_READ))
            {
                m_LocalReadCeMask  |= LW2080_ENGINE_TYPE_COPY_IDX(lwrEngine);
            }

            if (LW2080_CTRL_CE_GET_CAP(capParams.capsTbl, LW2080_CTRL_CE_CAPS_CE_SYSMEM_WRITE))
            {
                m_LocalWriteCeMask  |= LW2080_ENGINE_TYPE_COPY_IDX(lwrEngine);
            }

            if (LW2080_CTRL_CE_GET_CAP(capParams.capsTbl, LW2080_CTRL_CE_CAPS_CE_SYSMEM))
            {
                genericSysmemMask  |= LW2080_ENGINE_TYPE_COPY_IDX(lwrEngine);
            }
        }

        if (genericSysmemMask)
        {
            if (!m_LocalReadCeMask)
                m_LocalReadCeMask = genericSysmemMask;

            if (!m_LocalWriteCeMask)
                m_LocalWriteCeMask = genericSysmemMask;
        }

        if (!m_LocalReadCeMask || !m_LocalWriteCeMask)
        {
            genericSysmemMask = 0;
            for (auto const & lwrEngine : engines)
            {
                LW2080_CTRL_CE_GET_CE_PCE_MASK_PARAMS pceMaskParams = { };
                pceMaskParams.ceEngineType = lwrEngine;
                CHECK_RC(LwRmPtr()->ControlBySubdevice(GetLocalSubdevice(),
                                                       LW2080_CTRL_CMD_CE_GET_CE_PCE_MASK,
                                                       &pceMaskParams,
                                                       sizeof(pceMaskParams)));
                if (static_cast<UINT32>(Utility::CountBits(pceMaskParams.pceMask)) >= GetWidth())
                {
                    genericSysmemMask  |= LW2080_ENGINE_TYPE_COPY_IDX(lwrEngine);
                }
            }

            if (genericSysmemMask)
            {
                if (!m_LocalReadCeMask)
                    m_LocalReadCeMask = genericSysmemMask;

                if (!m_LocalWriteCeMask)
                    m_LocalWriteCeMask = genericSysmemMask;
            }
        }
    }

    m_RemoteCeBw        = 0;
    m_RemoteCeCount     = 0;
    m_RemoteReadCeMask  = 0;
    m_RemoteWriteCeMask = 0;
    if (GetTransferType() == TT_P2P)
    {
        pDev = GetRemoteSubdevice()->GetParentDevice();

        CHECK_RC(GetRemoteSubdevice()->GetMaxCeBandwidthKiBps(&m_RemoteCeBw));

        engines.clear();
        CHECK_RC(pDev->GetPhysicalEnginesFilteredByClass(classIds, &engines));
        m_RemoteCeCount = static_cast<UINT32>(engines.size());
        m_RemoteCeCopyWidthPeer =
            GetRemoteSubdevice()->GetInterface<LwLink>()->GetCeCopyWidth(LwLinkImpl::CET_PEERMEM);

        LW0000_CTRL_SYSTEM_GET_P2P_CAPS_V2_PARAMS  p2pCapsParams = { };
        p2pCapsParams.gpuIds[0] = GetRemoteSubdevice()->GetGpuId();
        p2pCapsParams.gpuIds[1] = GetLocalSubdevice()->GetGpuId();
        p2pCapsParams.gpuCount = 2;
        CHECK_RC(LwRmPtr()->Control(LwRmPtr()->GetClientHandle(),
                                    LW0000_CTRL_CMD_SYSTEM_GET_P2P_CAPS_V2,
                                    &p2pCapsParams,
                                    sizeof(p2pCapsParams)));
        m_RemoteReadCeMask  = p2pCapsParams.p2pOptimalReadCEs;
        m_RemoteWriteCeMask = p2pCapsParams.p2pOptimalWriteCEs;
        GpuSubdevice * pRem = (GetTransferType() == TT_LOOPBACK) ? GetLocalSubdevice() :
                                                                   GetRemoteSubdevice();
        Printf(Tee::PriLow, "Access CEs from GPU %s to %s : Read 0x%x, Write 0x%x\n",
               pRem->GpuIdentStr().c_str(),
               GetLocalSubdevice()->GpuIdentStr().c_str(),
               m_RemoteReadCeMask,
               m_RemoteWriteCeMask);
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Restore the original CE copy width
//!
void TestRouteCe2d::RestoreCeCopyWidth()
{
    GetLocalSubdevice()->GetInterface<LwLink>()->SetCeCopyWidth(LwLinkImpl::CET_SYSMEM,
                                                      m_LocalCeCopyWidthSys);
    GetLocalSubdevice()->GetInterface<LwLink>()->SetCeCopyWidth(LwLinkImpl::CET_PEERMEM,
                                                      m_LocalCeCopyWidthPeer);

    if (GetTransferType() == TT_P2P)
    {
        GetRemoteSubdevice()->GetInterface<LwLink>()->SetCeCopyWidth(LwLinkImpl::CET_PEERMEM,
                                                           m_RemoteCeCopyWidthPeer);
    }
}

//------------------------------------------------------------------------------
//! \brief Set the CE copy width to the specified value
//!
//! \param w : width to use for CE copies
//!
void TestRouteCe2d::SetCeCopyWidth(UINT32 w)
{
    const LwLinkImpl::CeWidth ceWidth = (w == 128) ? LwLinkImpl::CEW_128_BYTE :
                                                     LwLinkImpl::CEW_256_BYTE;

    GetLocalSubdevice()->GetInterface<LwLink>()->SetCeCopyWidth(LwLinkImpl::CET_SYSMEM, ceWidth);
    GetLocalSubdevice()->GetInterface<LwLink>()->SetCeCopyWidth(LwLinkImpl::CET_PEERMEM, ceWidth);

    if (GetTransferType() == TT_P2P)
    {
        GetRemoteSubdevice()->GetInterface<LwLink>()->SetCeCopyWidth(LwLinkImpl::CET_PEERMEM, ceWidth);
    }
}
