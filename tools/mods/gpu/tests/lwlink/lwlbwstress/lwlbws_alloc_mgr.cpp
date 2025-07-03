/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <memory>

#include "lwlbws_alloc_mgr.h"
#include "core/include/utility.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/channel.h"
#include "gpu/tests/gputestc.h"
#include "gpu/utility/surf2d.h"
#include "core/include/color.h"
#include "class/cla06fsubch.h"
#include "gpu/include/dmawrap.h"
#include "core/include/tee.h"
#include "core/include/lwrm.h"
#include "ctrl/ctrl2080.h"
#include "class/clc0b5.h" // PASCAL_DMA_COPY_A
#include "class/clc3b5.h" // VOLTA_DMA_COPY_A
#include "class/clc5b5.h" // TURING_DMA_COPY_A
#include "class/clc6b5.h" // AMPERE_DMA_COPY_A
#include "class/clc7b5.h" // AMPERE_DMA_COPY_B
#include "class/clc8b5.h" // HOPPER_DMA_COPY_A
#include "class/clc9b5.h" // BLACKWELL_DMA_COPY_A
#include "class/cle26e.h" // LWE2_CHANNEL_DMA
#include "gpu/utility/tsg.h"
#include "core/include/platform.h"

namespace
{
    const UINT32 SEMAPHORE_SIZE = 256;
};

using namespace LwLinkDataTestHelper;

//------------------------------------------------------------------------------
//! \brief Constructor
//!
AllocMgrCe2d::AllocMgrCe2d
(
    GpuTestConfiguration * pTestConfig,
    UINT32 numSemaphores,
    Tee::Priority pri,
    bool bFailRelease
)
 :  AllocMgr(pTestConfig, pri, bFailRelease)
   ,m_NumSemaphores(numSemaphores)
{
    m_Semaphores.resize(m_NumSemaphores);

    m_pGoldSurfA.reset(new Surface2D);
    m_pGoldSurfB.reset(new Surface2D);
    m_pSemSurf.reset(new Surface2D);
}

//------------------------------------------------------------------------------
//! \brief Acquire a channel on the specified GPU device with a particular HW
//! class
//!
//! \param pGpuDev        : GPU device to acquire the ce channel on
//! \param transferHw     : Engine type to acquire on the channel
//! \param engineInstance : Instance of the engine to acquire
//! \param ppCh           : Pointer to returned channel pointer
//! \param pChHandle      : Pointer to returned channel handle
//! \param pCopyClass     : Pointer to returned GPU copy class assigned to the channel
//!
//! \return OK if successful, not OK if acquisition failed
RC AllocMgrCe2d::AcquireChannel
(
     GpuDevice *pGpuDev
    ,LwLinkImpl::HwType transferHw
    ,UINT32 engineInstance
    ,Channel **ppCh
    ,LwRm::Handle *pChHandle
    ,UINT32 *pCopyClass
)
{
    if ((transferHw != LwLinkImpl::HT_CE) && (engineInstance != 0))
    {
        Printf(Tee::PriError, "%s : %s engine only has one instance!\n",
               LwLinkImpl::GetHwTypeName(transferHw), __FUNCTION__);
        return RC::BAD_PARAMETER;
    }

    // First try to find an unused channel
    if (m_Channels.find(pGpuDev) != m_Channels.end())
    {
        for (size_t ii = 0; ii < m_Channels[pGpuDev].size(); ii++)
        {
            if (!m_Channels[pGpuDev][ii].bAcquired &&
                (m_Channels[pGpuDev][ii].transferHw == transferHw) &&
                (m_Channels[pGpuDev][ii].engineInstance == engineInstance))
            {
                LwRm::Handle handle = 0;
                UINT32       cclass = 0;
                handle = m_Channels[pGpuDev][ii].pDmaAlloc->GetHandle();
                cclass = m_Channels[pGpuDev][ii].pDmaAlloc->GetClass();
                *ppCh       = m_Channels[pGpuDev][ii].pCh;
                *pChHandle  = m_Channels[pGpuDev][ii].hCh;
                *pCopyClass = cclass;
                m_Channels[pGpuDev][ii].bAcquired = true;
                Printf(GetPrintPri(),
                       "%s : Acquired %s[%u] channel for GPU(%u, 0) with hCh=0x%08x,"
                       " hCe=0x%08x\n",
                       __FUNCTION__,
                       LwLinkImpl::GetHwTypeName(transferHw),
                       engineInstance,
                       pGpuDev->GetDeviceInst(),
                       m_Channels[pGpuDev][ii].hCh,
                       handle);
                return OK;
            }
        }
    }
    else
    {
        vector<ChannelAlloc> emptyAlloc;
        m_Channels.emplace(pGpuDev, move(emptyAlloc));
    }

    // Temporarily rebind the GPU device so that the allocation happens on the
    // correct device
    GpuTestConfiguration * pTestConfig = GetTestConfig();
    GpuDevice *pOrigGpuDev = pTestConfig->GetGpuDevice();
    Utility::CleanupOnReturnArg<GpuTestConfiguration,
                                void,
                                GpuDevice *>
        restoreGpuDev(pTestConfig,
                      &GpuTestConfiguration::BindGpuDevice,
                      pOrigGpuDev);
    pTestConfig->BindGpuDevice(pGpuDev);

    RC rc;
    ChannelAlloc newAlloc;
    LwRm::Handle dmaAllocHandle = 0;
    if (transferHw == LwLinkImpl::HT_TWOD)
    {
        auto pTwoD = make_unique<TwodAlloc>();
        CHECK_RC(pTestConfig->AllocateChannelWithEngine(&newAlloc.pCh,
                                                        &newAlloc.hCh,
                                                        pTwoD.get()));
        dmaAllocHandle = pTwoD->GetHandle();
        newAlloc.pDmaAlloc = move(pTwoD);
    }
    else if (transferHw == LwLinkImpl::HT_CE)
    {
        UINT32 grceInst;
        UINT32 engineId;
        auto pCeAlloc = make_unique<DmaCopyAlloc>();

        CHECK_RC(GetGrceInstance(pGpuDev, engineInstance, &grceInst));
        CHECK_RC(pCeAlloc->GetEngineId(pGpuDev, LwRmPtr().Get(), engineInstance, &engineId));

        if (grceInst != ALL_CES)
        {
            UINT32 flags = 0;
            if (grceInst == 1)
                flags = DRF_DEF(OS04, _FLAGS, _GROUP_CHANNEL_RUNQUEUE, _ONE);

            if (!m_GrceTsgs.count(pGpuDev))
            {
                unique_ptr<Tsg> pGrceTsg = make_unique<Tsg>();
                GpuTestConfiguration * pTsgTestConfig = pGrceTsg->GetTestConfiguration();
                pTsgTestConfig->BindGpuDevice(pGpuDev);
                pTsgTestConfig->BindRmClient(GetTestConfig()->GetRmClient());
                pTsgTestConfig->SetAllowMultipleChannels(true);
                m_GrceTsgs.emplace(pGpuDev, move(pGrceTsg));
            }

            CHECK_RC(pTestConfig->AllocateChannelGr(&newAlloc.pCh,
                                                    &newAlloc.hCh,
                                                    m_GrceTsgs[pGpuDev].get(),
                                                    0,
                                                    pCeAlloc.get(),
                                                    flags,
                                                    engineId));
        }
        else
        {
            CHECK_RC(pTestConfig->AllocateChannelWithEngine(&newAlloc.pCh,
                                                            &newAlloc.hCh,
                                                            pCeAlloc.get(),
                                                            engineId));
        }
        dmaAllocHandle = pCeAlloc->GetHandle();
        newAlloc.pDmaAlloc = move(pCeAlloc);
    }
    else
    {
        MASSERT(!"Unsupported transferHw");
        return RC::BAD_PARAMETER;
    }

    DEFERNAME(freeAllocs)
    {
        if (newAlloc.pCh)
            pTestConfig->FreeChannel(newAlloc.pCh);
        newAlloc.pDmaAlloc->Free();
    };

    CHECK_RC(newAlloc.pCh->SetObject((transferHw == LwLinkImpl::HT_TWOD)
                                        ? LWA06F_SUBCHANNEL_2D : LWA06F_SUBCHANNEL_COPY_ENGINE,
                                     newAlloc.pDmaAlloc->GetHandle()));

    freeAllocs.Cancel();

    newAlloc.transferHw = transferHw;
    newAlloc.engineInstance = engineInstance;
    newAlloc.bAcquired = true;
    m_Channels[pGpuDev].push_back(move(newAlloc));
    *ppCh = newAlloc.pCh;
    *pChHandle = newAlloc.hCh;

    Printf(GetPrintPri(),
           "%s : Created %s[%u] channel for GPU(%u, 0) with hCh=0x%08x, hCe=0x%08x\n",
           __FUNCTION__,
           LwLinkImpl::GetHwTypeName(transferHw),
           engineInstance,
           pGpuDev->GetDeviceInst(),
           newAlloc.hCh,
           dmaAllocHandle);

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Acquire a semaphore on the specified GPU device
//!
//! \param pGpuDev  : GPU device to acquire the semaphore on
//! \param pSemData : Returned semaphore data
//!
//! \return OK if successful, not OK if acquisition failed
RC AllocMgrCe2d::AcquireSemaphore
(
    GpuDevice *pGpuDev
   ,SemaphoreData *pSemData
)
{
    // Find the first unallocated semaphore (all semaphores are allocated at
    // initialization time)
    SemaphoreAlloc *pSemAlloc = nullptr;
    for (size_t ii = 0; ii < m_Semaphores.size(); ii++)
    {
        if (!m_Semaphores[ii].bAcquired)
        {
            pSemAlloc = &m_Semaphores[ii];
            break;
        }
    }

    // All semaphores are allocated at initialization time, if one is not
    // available it is an error
    if (nullptr == pSemAlloc)
    {
        Printf(Tee::PriError, "%s : Unable to allocate semaphore\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    RC rc;
    pSemData->pSemSurf = m_pSemSurf;

    // If the memory was allocated on a different GPU device, then share it with
    // the requested GPU device
    if (pGpuDev != m_pSemSurf->GetGpuDev())
    {
        if (!m_pSemSurf->IsMapShared(pGpuDev))
            CHECK_RC(m_pSemSurf->MapShared(pGpuDev));

        pSemData->ctxDmaOffsetGpu = m_pSemSurf->GetCtxDmaOffsetGpuShared(pGpuDev);
    }
    else
    {
        pSemData->ctxDmaOffsetGpu = m_pSemSurf->GetCtxDmaOffsetGpu();
    }

    pSemAlloc->bAcquired = true;

    pSemData->ctxDmaOffsetGpu += pSemAlloc->surfOffset;
    pSemData->pAddress         = pSemAlloc->pAddress;
    Platform::MemSet(pSemAlloc->pAddress, 0, SEMAPHORE_SIZE);

    Printf(GetPrintPri(),
           "%s : Acquired semapore for GPU(%u, 0) at offset 0x%08x\n",
           __FUNCTION__,
           pGpuDev->GetDeviceInst(),
           pSemAlloc->surfOffset);
    return rc;
}
//------------------------------------------------------------------------------
//! \brief Initialize the allocation manager
//!
//! \param goldSurfWidth    : Width of golden surfaces (pixels)
//! \param goldSurfHeight   : Height of golden surfaces (lines)
//! \param surfMode         : Mode to use when filling the surfaces
//!
//! \return OK if successful, not OK if initialization failed
RC AllocMgrCe2d::Initialize
(
    UINT32 goldSurfWidth,
    UINT32 goldSurfHeight,
    UINT32 surfMode
)
{
    RC rc;
    CHECK_RC(AllocSurface(m_pGoldSurfA
                         ,Memory::NonCoherent
                         ,Surface2D::Pitch
                         ,ST_NORMAL
                         ,goldSurfWidth
                         ,goldSurfHeight
                         ,GetTestConfig()->GetGpuDevice()));
    CHECK_RC(AllocSurface(m_pGoldSurfB
                         ,Memory::NonCoherent
                         ,Surface2D::Pitch
                         ,ST_NORMAL
                         ,goldSurfWidth
                         ,goldSurfHeight
                         ,GetTestConfig()->GetGpuDevice()));

    switch (surfMode)
    {
        case 0:
            CHECK_RC(FillSurface(m_pGoldSurfA, PT_RANDOM, 0, 0, true));
            CHECK_RC(FillSurface(m_pGoldSurfB, PT_BARS, 0, 0, true));
            break;
        case 1:
            CHECK_RC(FillSurface(m_pGoldSurfA, PT_RANDOM, 0, 0, true));
            CHECK_RC(FillSurface(m_pGoldSurfB, PT_RANDOM, 0, 0, true));
            break;
        case 2:
            CHECK_RC(FillSurface(m_pGoldSurfA, PT_SOLID, 0, 0, true));
            CHECK_RC(FillSurface(m_pGoldSurfB, PT_SOLID, 0, 0, true));
            break;
        case 3:
            CHECK_RC(FillSurface(m_pGoldSurfA, PT_ALTERNATE, 0x00000000, 0xFFFFFFFF, true));
            CHECK_RC(FillSurface(m_pGoldSurfB, PT_ALTERNATE, 0x00000000, 0xFFFFFFFF, true));
            break;
        case 4:
            CHECK_RC(FillSurface(m_pGoldSurfA, PT_ALTERNATE, 0xAAAAAAAA, 0x55555555, true));
            CHECK_RC(FillSurface(m_pGoldSurfB, PT_ALTERNATE, 0xAAAAAAAA, 0x55555555, true));
            break;
        case 5:
            CHECK_RC(FillSurface(m_pGoldSurfA, PT_RANDOM, 0, 0, true));
            CHECK_RC(FillSurface(m_pGoldSurfB, PT_SOLID, 0, 0, true));
            break;
        case 6:
            CHECK_RC(FillSurface(m_pGoldSurfA, PT_ALTERNATE, 0x0F0F0F0F, 0xF0F0F0F0, true));
            CHECK_RC(FillSurface(m_pGoldSurfB, PT_ALTERNATE, 0x0F0F0F0F, 0xF0F0F0F0, true));
            break;
        default:
            Printf(Tee::PriError, "%s : Invalid SrcSurfMode = %u\n", __FUNCTION__, surfMode);
            return RC::ILWALID_ARGUMENT;
    }

    CHECK_RC(SetupSemaphores());

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Release a channel
//!
//! \param pCh : Channel pointer to release
//!
//! \return OK if successful, not OK if release failed
RC AllocMgrCe2d::ReleaseChannel(Channel *pCh)
{
    GpuDevice *pGpuDev = pCh->GetGpuDevice();
    if (m_Channels.find(pGpuDev) == m_Channels.end())
    {
        Printf(GetReleaseFailPri(), "%s : No channels allocated on Gpu(%u, 0)\n",
               __FUNCTION__, pGpuDev->GetDeviceInst());
        return GetReleaseFailRc();
    }

    for (size_t ii = 0; ii < m_Channels[pGpuDev].size(); ii++)
    {
        if (m_Channels[pGpuDev][ii].pCh == pCh)
        {
            m_Channels[pGpuDev][ii].bAcquired = false;
            LwRm::Handle dmaAllocHandle = 0;
            if (m_Channels[pGpuDev][ii].pDmaAlloc.get())
            {
                dmaAllocHandle = m_Channels[pGpuDev][ii].pDmaAlloc->GetHandle();
            }
            Printf(GetPrintPri(),
                   "%s : Released %s[%u] channel for GPU(%u, 0) with hCh=0x%08x, "
                   "hCe=0x%08x\n",
                   __FUNCTION__,
                   LwLink::GetHwTypeName(m_Channels[pGpuDev][ii].transferHw),
                   m_Channels[pGpuDev][ii].engineInstance,
                   pGpuDev->GetDeviceInst(),
                   m_Channels[pGpuDev][ii].hCh,
                   dmaAllocHandle);
            return OK;
        }
    }
    Printf(GetReleaseFailPri(), "%s : Channel no found on Gpu(%u, 0)\n",
           __FUNCTION__, pGpuDev->GetDeviceInst());
    return GetReleaseFailRc();
}

//------------------------------------------------------------------------------
//! \brief Release a semaphore
//!
//! \param pAddress : CPU virtual pointer for semaphore to release
//!
//! \return OK if successful, not OK if release failed
RC AllocMgrCe2d::ReleaseSemaphore(void *pAddress)
{
    for (size_t ii = 0; ii < m_Semaphores.size(); ii++)
    {
        if (m_Semaphores[ii].pAddress == pAddress)
        {
            m_Semaphores[ii].bAcquired = false;
            Printf(GetPrintPri(),
                   "%s : Released semapore at offset 0x%08x\n",
                   __FUNCTION__,
                   m_Semaphores[ii].surfOffset);
            return OK;
        }
    }
    Printf(GetReleaseFailPri(), "%s : Semaphore surface not found with address %p\n",
           __FUNCTION__, pAddress);
    return GetReleaseFailRc();
}

//------------------------------------------------------------------------------
RC AllocMgrCe2d::ShareSemaphore
(
    const SemaphoreData & lwrSemData
   ,GpuDevice *pGpuDev
   ,SemaphoreData *pSharedSemData
)
{
    for (auto const & lwrSem : m_Semaphores)
    {
        if (lwrSem.pAddress == lwrSemData.pAddress)
        {
            if (!lwrSem.bAcquired)
            {
                Printf(Tee::PriError,
                       "%s : Attempt to share unacquired sempahore\n",
                       __FUNCTION__);
                return RC::SOFTWARE_ERROR;
            }
            *pSharedSemData = lwrSemData;
            if (lwrSemData.pSemSurf->GetGpuDev() != pGpuDev)
            {
                RC rc;
                if (!lwrSemData.pSemSurf->IsMapShared(pGpuDev))
                {
                    CHECK_RC(lwrSemData.pSemSurf->MapShared(pGpuDev));
                }
                pSharedSemData->ctxDmaOffsetGpu =
                    lwrSemData.pSemSurf->GetCtxDmaOffsetGpuShared(pGpuDev);
            }
            else
            {
                pSharedSemData->ctxDmaOffsetGpu =
                    lwrSemData.pSemSurf->GetCtxDmaOffsetGpu();
            }
            pSharedSemData->ctxDmaOffsetGpu += lwrSem.surfOffset;
            Printf(GetPrintPri(),
                   "%s : Shared semapore at offset 0x%08x with GPU(%u, 0)\n",
                   __FUNCTION__,
                   lwrSem.surfOffset,
                   pGpuDev->GetDeviceInst());
            return OK;
        }
    }
    Printf(Tee::PriError, "%s : Failed to find semaphore to share with address %p\n",
           __FUNCTION__, lwrSemData.pAddress);
    return RC::SOFTWARE_ERROR;
}

//------------------------------------------------------------------------------
//! \brief Shutdown the allocation manager
//!
//! \return OK if successful, not OK if shutdown failed
RC AllocMgrCe2d::Shutdown()
{
    StickyRC rc;

    map<GpuDevice *, vector<SurfaceAlloc> >::iterator pGpuSurfs = m_Surfaces.begin();
    for (; pGpuSurfs != m_Surfaces.end(); pGpuSurfs++)
    {
        for (size_t ii = 0; ii < pGpuSurfs->second.size(); ii++)
        {
            if (!pGpuSurfs->second[ii].pSurf.unique() ||
                pGpuSurfs->second[ii].bAcquired)
            {
                Printf(Tee::PriError,
                       "Multiple references to a surface on GpuDevice(%u) "
                       "during Cleanup\n",
                       pGpuSurfs->first->GetDeviceInst());
                rc = RC::SOFTWARE_ERROR;
            }
            pGpuSurfs->second[ii].pSurf->Free();
        }
    }
    m_Surfaces.clear();

    for (auto & gpuChannels : m_Channels)
    {
        for (auto & lwrChan : gpuChannels.second)
        {
            if (lwrChan.bAcquired)
            {
                Printf(Tee::PriError,
                       "Multiple references to a channel on GpuDevice(%u) "
                       "during Cleanup\n",
                       gpuChannels.first->GetDeviceInst());
                rc = RC::SOFTWARE_ERROR;
            }

            if (lwrChan.pDmaAlloc.get())
            {
                lwrChan.pDmaAlloc->Free();
            }
            GetTestConfig()->FreeChannel(lwrChan.pCh);
        }
    }
    m_Channels.clear();

    for (auto & grceEntry : m_GrceTsgs)
        grceEntry.second->Free();
    m_GrceTsgs.clear();
    
    m_Semaphores.clear();
    if (!m_pSemSurf.unique() || !m_pGoldSurfA.unique() || !m_pGoldSurfB.unique())
    {
        Printf(Tee::PriError,
               "External references to a golden/semaphore surfaceduring "
               "Cleanup\n");
        rc = RC::SOFTWARE_ERROR;
    }
    if (m_pSemSurf.get())
        m_pSemSurf->Free();
    if (m_pGoldSurfA.get())
        m_pGoldSurfA->Free();
    if (m_pGoldSurfB.get())
        m_pGoldSurfB->Free();

    rc = AllocMgr::Shutdown();

    return rc;
}

RC AllocMgrCe2d::GetGrceInstance(GpuDevice *pGpuDev, UINT32 ceInstance, UINT32 *pGrceInst)
{
    RC rc;

    vector<UINT32> classIds;
    classIds.push_back(BLACKWELL_DMA_COPY_A);
    classIds.push_back(HOPPER_DMA_COPY_A);
    classIds.push_back(AMPERE_DMA_COPY_B);
    classIds.push_back(AMPERE_DMA_COPY_A);
    classIds.push_back(TURING_DMA_COPY_A);
    classIds.push_back(VOLTA_DMA_COPY_A);
    classIds.push_back(PASCAL_DMA_COPY_A);

    vector<UINT32> engineList;

    *pGrceInst = ALL_CES;

    CHECK_RC(pGpuDev->GetPhysicalEnginesFilteredByClass(classIds, &engineList));

    sort(engineList.begin(), engineList.end());

    UINT32 grceInst = 0;
    for (auto const & lwrEngine : engineList)
    {
        LW2080_CTRL_CE_GET_CAPS_V2_PARAMS params = {};
        params.ceEngineType = lwrEngine;
        CHECK_RC(LwRmPtr()->ControlBySubdevice(pGpuDev->GetSubdevice(0),
                                               LW2080_CTRL_CMD_CE_GET_CAPS_V2,
                                               &params, sizeof(params)));
        if (LW2080_CTRL_CE_GET_CAP(params.capsTbl, LW2080_CTRL_CE_CAPS_CE_GRCE))
        {
            if (grceInst == 2)
            {
                Printf(Tee::PriError,
                       "%s : Too many GRCEs detected, update GRCE allocation algorithm!\n",
                       __FUNCTION__);
                return RC::UNSUPPORTED_HARDWARE_FEATURE;
            }

            if (LW2080_ENGINE_TYPE_COPY_IDX(lwrEngine) == ceInstance)
            {
                *pGrceInst = grceInst;
            }
            grceInst++;
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Setup all the semaphore data
//!
//! \return OK if successful, not OK if initialization failed
RC AllocMgrCe2d::SetupSemaphores()
{
    RC rc;

    m_pSemSurf->SetWidth(SEMAPHORE_SIZE);
    m_pSemSurf->SetHeight(m_NumSemaphores);
    m_pSemSurf->SetLocation(Memory::NonCoherent);
    m_pSemSurf->SetColorFormat(ColorUtils::VOID8);
    m_pSemSurf->SetProtect(Memory::ReadWrite);
    m_pSemSurf->SetLayout(Surface2D::Pitch);
    CHECK_RC(m_pSemSurf->Alloc(GetTestConfig()->GetGpuDevice()));
    CHECK_RC(m_pSemSurf->Fill(0));
    CHECK_RC(m_pSemSurf->Map());

    for (size_t ii = 0; ii < m_Semaphores.size(); ii++)
    {
        m_Semaphores[ii].bAcquired  = false;
        m_Semaphores[ii].surfOffset =
            UNSIGNED_CAST(UINT32, m_pSemSurf->GetPixelOffset(0, ii));
        m_Semaphores[ii].pAddress   =
            static_cast<UINT08 *>(m_pSemSurf->GetAddress()) +
            m_Semaphores[ii].surfOffset;
    }
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Acquire a surface on the specified GPU device
//!
//! \param pGpuDev       : GPU device to acquire the surface on
//! \param loc           : Location of the surface
//! \param layout        : Layout of the surface
//! \param type          : SurfaceType of the surface to allocate
//! \param surfaceWidth  : Width of the surface (pixels)
//! \param surfaceHeight : Height of the surface (lines)
//! \param pSurf         : Pointer to returned surface pointer
//!
//! \return OK if successful, not OK if acquisition failed
RC AllocMgrCe2d::AcquireSurface
(
     GpuDevice *pGpuDev
    ,Memory::Location loc
    ,Surface2D::Layout layout
    ,SurfaceType type
    ,UINT32 surfaceWidth
    ,UINT32 surfaceHeight
    ,Surface2DPtr *pSurf
)
{
    // First try to find an unused surface in the correct location
    if (m_Surfaces.find(pGpuDev) != m_Surfaces.end())
    {
        for (size_t ii = 0; ii < m_Surfaces[pGpuDev].size(); ii++)
        {
            SurfaceAlloc& surfAlloc = m_Surfaces[pGpuDev][ii];
            if (!surfAlloc.bAcquired &&
                (surfAlloc.type == type) &&
                (surfAlloc.pSurf->GetWidth() == surfaceWidth) &&
                (surfAlloc.pSurf->GetHeight() == surfaceHeight) &&
                (surfAlloc.pSurf->GetLocation() == loc) &&
                (surfAlloc.pSurf->GetLayout() == layout))
            {
                *pSurf = surfAlloc.pSurf;
                surfAlloc.bAcquired = true;
                Printf(GetPrintPri(),
                       "%s : Acquired surface for GPU(%u, 0) at offset 0x%llx\n",
                       __FUNCTION__,
                       pGpuDev->GetDeviceInst(),
                       surfAlloc.pSurf->GetCtxDmaOffsetGpu());
                return OK;
            }
        }
    }
    else
    {
        vector<SurfaceAlloc> emptyAlloc;
        m_Surfaces[pGpuDev] = emptyAlloc;
    }

    RC rc;
    SurfaceAlloc newAlloc;
    newAlloc.bAcquired = true;
    newAlloc.type = type;
    newAlloc.pSurf.reset(new Surface2D);
    CHECK_RC(AllocSurface(newAlloc.pSurf, loc, layout, type, surfaceWidth, surfaceHeight, pGpuDev));
    m_Surfaces[pGpuDev].push_back(newAlloc);
    *pSurf = newAlloc.pSurf;

    Printf(GetPrintPri(),
           "%s : Created surface for GPU(%u, 0) at offset 0x%llx\n",
           __FUNCTION__,
           pGpuDev->GetDeviceInst(),
           newAlloc.pSurf->GetCtxDmaOffsetGpu());
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Release a surface
//!
//! \param pSurf   : Pointer to surface for release
//!
//! \return OK if successful, not OK if release failed
RC AllocMgrCe2d::ReleaseSurface(Surface2DPtr pSurf)
{
    GpuDevice *pGpuDev = pSurf->GetGpuDev();
    if (m_Surfaces.find(pGpuDev) == m_Surfaces.end())
    {
        Printf(Tee::PriLow, "%s : No surfaces allocated on Gpu(%u, 0)\n",
               __FUNCTION__, pGpuDev->GetDeviceInst());
    }
    else
    {
        for (size_t ii = 0; ii < m_Surfaces[pGpuDev].size(); ii++)
        {
            if (m_Surfaces[pGpuDev][ii].pSurf == pSurf)
            {
                m_Surfaces[pGpuDev][ii].bAcquired = false;
                Printf(GetPrintPri(),
                       "%s : Released surface for GPU(%u, 0) at offset 0x%llx\n",
                       __FUNCTION__,
                       pGpuDev->GetDeviceInst(),
                       m_Surfaces[pGpuDev][ii].pSurf->GetCtxDmaOffsetGpu());
                return OK;
            }
        }
    }
    
    if (OK == AllocMgr::ReleaseSurface(pSurf))
        return OK;

    return GetReleaseFailRc();
}

//------------------------------------------------------------------------------
//! \brief Allocate a test surface
//!
//! \param pSurf         : Pointer to surface to allocate
//! \param loc           : Location of surface to allocate
//! \param layout        : Layout of the surface to allocate
//! \param type          : SurfaceType of the surface to allocate
//! \param surfaceWidth  : Width of the surface (pixels)
//! \param surfaceHeight : Height of the surface (lines)
//! \param pGpuDev       : Pointer to GPU device to allocate the surface on
//!
//! \return OK if successful, not OK if allocation failed
RC AllocMgrCe2d::AllocSurface
(
    Surface2DPtr pSurf,
    Memory::Location loc,
    Surface2D::Layout layout,
    SurfaceType type,
    UINT32 surfaceWidth,
    UINT32 surfaceHeight,
    GpuDevice *pGpuDev
)
{
    RC rc;

    if (loc == Memory::Fb)
    {
        if (type == ST_FULLFABRIC)
        {
            pSurf->SetPhysContig(true);
            pSurf->SetPageSize(4); //4K
            pSurf->SetGpuPhysAddrHintMax(1LLU << 34); //16GB
        }
        else
        {
            pSurf->SetPageSize(m_FbSurfPageSizeKb);
        }

        if (m_bUseFla)
        {
            pSurf->SetFLAPeerMapping(true);
            pSurf->SetFLAPageSizeKb(m_FlaPageSizeKb);
            pSurf->SetPageSize(2048); // Using FLA requires that memory be allocated with 2MB pages
        }
    }
    CHECK_RC(AllocMgr::AllocSurface(pSurf, loc, layout, surfaceWidth, surfaceHeight, pGpuDev));
    return rc;
}
