/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file changrp.cpp
//! \brief Class to encaspulate a channel group
//!

#include "changrp.h"
#include <string>          // Only use <> for built-in C++ stuff
#include <vector>
#include <algorithm>
#include <map>
#include "lwos.h"
#include "core/include/channel.h"
#include "gpu/include/gpudev.h"
#include "lwRmApi.h"
#include "core/include/utility.h"
#include "gpu/utility/surf2d.h"
#include "core/include/platform.h"

#include "ctrl/ctrla06f.h"
#include "ctrl/ctrla06c.h"
#include "ctrl/ctrl2080.h"

#include "class/cla06c.h"
#include "class/cla06f.h"
#include "class/cla16f.h"
#include "class/cla06fsubch.h"

// Must be last
#include "core/include/memcheck.h"

ChannelGroup::ChannelGroup(GpuTestConfiguration *pTestConfig,
                           LW_CHANNEL_GROUP_ALLOCATION_PARAMETERS *pChannelGroupParams) :
    Tsg(pChannelGroupParams),
    m_bVirtCtx(false),
    m_bCtxAllocated(false),
    m_bCtxInitialiized(false),
    m_bCtxPromoted(false),
    m_ctxLoc(Memory::Optimal)
{
    MASSERT(pTestConfig);
    SetTestConfiguration(pTestConfig);
}

void ChannelGroup::SetUseVirtualContext(bool bVirtCtx)
{
    MASSERT(!GetHandle());
    m_bVirtCtx = bVirtCtx;
}

/* virtual */ void ChannelGroup::Free()
{
    GpuTestConfiguration *pTestConfig = GetTestConfiguration();

    while (GetNumChannels() > 0)
    {
        pTestConfig->FreeChannel(GetChannel(0));
    }
    Tsg::Free();

    if (m_bVirtCtx)
        FreeEngCtx();
}

RC ChannelGroup::AllocChannelWithSubcontext(Channel **ppCh, Subcontext *pSubctx, LwU32 flags)
{
    GpuTestConfiguration *pTestConfig = GetTestConfiguration();
    LwRm *pLwRm = GetRmClient();
    LwRm::Handle hCh;
    Channel *pCh;
    RC rc;

    if (pSubctx)
    {
        if (pSubctx->GetTsg() != this)
        {
            MASSERT(!"TSG mismatch");
            return RC::SOFTWARE_ERROR;
        }

        CHECK_RC(pTestConfig->AllocateChannel(&pCh, &hCh, nullptr,
                    pSubctx, 0, flags, pSubctx->GetTsg()->GetEngineId()));
    }
    else
    {
        CHECK_RC(pTestConfig->AllocateChannel(&pCh, &hCh, this,
                    nullptr, 0, flags, GetEngineId()));
    }

    pCh->SetAutoSchedule(false);
    if (m_bVirtCtx)
    {
        for (LwU32 i = 0; i < GetGpuDevice()->GetNumSubdevices(); i++)
        {
            GpuSubdevice *gpusubdev = GetGpuDevice()->GetSubdevice(i);
            LwU32 hSubDevDiag = gpusubdev->GetSubdeviceDiag();

            LW208F_CTRL_FIFO_ENABLE_VIRTUAL_CONTEXT_PARAMS vcParams = {0};

            vcParams.hChannel = hCh;
            CHECK_RC(pLwRm->Control(hSubDevDiag,
                                    LW208F_CTRL_CMD_FIFO_ENABLE_VIRTUAL_CONTEXT,
                                    (void*)&vcParams, sizeof(vcParams)));
        }
        if (!m_bCtxAllocated)
        {
            CHECK_RC(AllocEngCtx(pCh, hCh));
            m_bCtxAllocated = true;
        }
        // We only intercept explicit flushes, so disable auto-flush
        CHECK_RC(pCh->SetAutoFlush(false, 0));
    }

    if (ppCh)
        *ppCh = pCh;
    return rc;
}

RC ChannelGroup::AllocChannel(Channel **ppCh, LwU32 flags)
{
    return AllocChannelWithSubcontext(ppCh, NULL, flags);
}

void ChannelGroup::FreeChannel(Channel *pCh)
{
    GetTestConfiguration()->FreeChannel(pCh);
}

RC ChannelGroup::Bind(LwU32 engineType)
{
    RC rc;
    LwRm *pLwRm = GetRmClient();
    LWA06C_CTRL_BIND_PARAMS bindParams = {0};

    bindParams.engineType = engineType;
    CHECK_RC(pLwRm->Control(GetHandle(), LWA06C_CTRL_CMD_BIND,
                &bindParams, sizeof(bindParams)));

    return rc;
}

RC ChannelGroup::Schedule(bool bEnable, bool bSkipSubmit)
{
    RC rc;
    LwRm *pLwRm = GetRmClient();
    LWA06C_CTRL_GPFIFO_SCHEDULE_PARAMS gpFifoScheduleParams;

    gpFifoScheduleParams.bEnable     = bEnable;
    gpFifoScheduleParams.bSkipSubmit = bSkipSubmit;
    CHECK_RC(pLwRm->Control(GetHandle(),
                LWA06C_CTRL_CMD_GPFIFO_SCHEDULE, &gpFifoScheduleParams,
                sizeof(gpFifoScheduleParams)));
    return rc;
}

RC ChannelGroup::Flush()
{
    RC rc;

    if (m_bVirtCtx)
    {
        if (!m_bCtxInitialiized)
        {
            CHECK_RC(InitializeEngCtx());
            m_bCtxInitialiized = true;
        }

        if (!m_bCtxPromoted)
        {
            CHECK_RC(PromoteEngCtx());
            m_bCtxPromoted = true;
        }
    }

    for (LwU32 i = 0; i < GetNumChannels(); i++)
    {
        CHECK_RC(GetChannel(i)->Flush());
    }
    return rc;
}

RC ChannelGroup::WaitIdle()
{
    GpuTestConfiguration *pTestConfig = GetTestConfiguration();
    RC rc;

    for (LwU32 i = 0; i < GetNumChannels(); i++)
    {
        Channel *pCh = GetChannel(i);
        CHECK_RC(pCh->WaitIdle(pTestConfig->TimeoutMs()));
    }

    if (m_bVirtCtx)
    {
        CHECK_RC(EvictEngCtx());
        m_bCtxPromoted = false;
    }
    return rc;
}

RC ChannelGroup::Preempt(bool bWaitIdle)
{
    RC rc;
    LwRm *pLwRm = GetRmClient();
    LWA06C_CTRL_PREEMPT_PARAMS preemptParams = {0};

    preemptParams.bWait = bWaitIdle;
    CHECK_RC(pLwRm->Control(GetHandle(),
            LWA06C_CTRL_CMD_PREEMPT, &preemptParams, sizeof(preemptParams)));

    return rc;
}

RC ChannelGroup::AllocEngCtx(Channel *pCh, LwRm::Handle hCh)
{
    LwRm *pLwRm = GetRmClient();
    RC rc;
    LW0080_CTRL_FIFO_GET_ENGINE_CONTEXT_PROPERTIES_PARAMS ctxPropParams = {0};

    ctxPropParams.engineId = LW0080_CTRL_FIFO_GET_ENGINE_CONTEXT_PROPERTIES_ENGINE_ID_GRAPHICS;

    CHECK_RC(pLwRm->ControlByDevice(GetGpuDevice(),
                            LW0080_CTRL_CMD_FIFO_GET_ENGINE_CONTEXT_PROPERTIES,
                            (void*)&ctxPropParams,
                            sizeof(ctxPropParams)));

    LwU32 EngCtxSize = ctxPropParams.size;

    m_engCtxMem.SetAddressModel(Memory::Paging);
    m_engCtxMem.SetWidth(EngCtxSize);
    m_engCtxMem.SetPitch(EngCtxSize);
    m_engCtxMem.SetHeight(1);
    m_engCtxMem.SetColorFormat(ColorUtils::VOID32);
    // RM doesn't support big pages or non-contig memory for context memory
    m_engCtxMem.SetPageSize(4);
    m_engCtxMem.SetPhysContig(true);
    m_engCtxMem.SetLocation(m_ctxLoc);
    m_engCtxMem.SetVirtAlignment(ctxPropParams.alignment);

    CHECK_RC(m_engCtxMem.Alloc(GetGpuDevice(), GetRmClient()));
    CHECK_RC(m_engCtxMem.Map());

    return rc;

}

RC ChannelGroup::InitializeEngCtx()
{
    LwRm *pLwRm = GetRmClient();

    LW2080_CTRL_GPU_INITIALIZE_CTX_PARAMS initCtx = {0};
    RC rc;

    Printf(Tee::PriDebug, "%s: Initializing Virtual Context memory\n",
            __FUNCTION__);

    MASSERT(m_engCtxMem.HasVirtual());
    MASSERT(m_engCtxMem.HasPhysical());
    MASSERT(m_engCtxMem.HasMap());

    initCtx.engineType  = LW2080_ENGINE_TYPE_GRAPHICS;
    initCtx.hClient     = pLwRm->GetClientHandle();
    initCtx.hChanClient = pLwRm->GetClientHandle();
    initCtx.hObject     = GetHandle();
    initCtx.hVirtMemory = m_engCtxMem.GetVirtMemHandle();
    initCtx.physAddress = m_engCtxMem.GetVidMemOffset();

    switch(m_engCtxMem.GetLocation())
    {
        case Memory::Fb:
            initCtx.physAttr = DRF_DEF(2080, _CTRL_GPU_INITIALIZE_CTX, _APERTURE, _VIDMEM);
            break;
        case Memory::Coherent:
            initCtx.physAttr = DRF_DEF(2080, _CTRL_GPU_INITIALIZE_CTX, _APERTURE, _COH_SYS);
            break;
        case Memory::NonCoherent:
            initCtx.physAttr = DRF_DEF(2080, _CTRL_GPU_INITIALIZE_CTX, _APERTURE, _NCOH_SYS);
            break;
        default:
            CHECK_RC(RC::SOFTWARE_ERROR);
    }

    for (LwU32 i = 0; i < GetGpuDevice()->GetNumSubdevices(); i++)
    {
        GpuSubdevice *gpusubdev = GetGpuDevice()->GetSubdevice(i);

        CHECK_RC(pLwRm->ControlBySubdevice(gpusubdev,
                                LW2080_CTRL_CMD_GPU_INITIALIZE_CTX,
                                (void*)&initCtx,
                                sizeof(initCtx)));
    }
    return rc;
}

RC ChannelGroup::PromoteEngCtx()
{
    LwRm *pLwRm = GetRmClient();
    RC rc;
    LW2080_CTRL_GPU_PROMOTE_CTX_PARAMS promoteCtx = {0};

    Printf(Tee::PriDebug, "%s: Promoting Virtual Context memory\n",
            __FUNCTION__);

    promoteCtx.engineType  = LW2080_ENGINE_TYPE_GRAPHICS;
    promoteCtx.hClient     = pLwRm->GetClientHandle();;
    promoteCtx.hChanClient = pLwRm->GetClientHandle();
    promoteCtx.hObject     = GetHandle();
    promoteCtx.hVirtMemory = m_engCtxMem.GetVirtMemHandle();

    for (LwU32 i = 0; i < GetGpuDevice()->GetNumSubdevices(); i++)
    {
        GpuSubdevice *gpusubdev = GetGpuDevice()->GetSubdevice(i);

        CHECK_RC(pLwRm->ControlBySubdevice(gpusubdev,
                                LW2080_CTRL_CMD_GPU_PROMOTE_CTX,
                                (void*)&promoteCtx,
                                sizeof(promoteCtx)));
    }
    return rc;
}

RC ChannelGroup::EvictEngCtx()
{
    LwRm *pLwRm = GetRmClient();
    RC rc;
    LW2080_CTRL_GPU_EVICT_CTX_PARAMS evictCtx = {0};

    Printf(Tee::PriDebug, "%s: Evicting Virtual Context memory\n",
            __FUNCTION__);

    evictCtx.engineType  = LW2080_ENGINE_TYPE_GRAPHICS;
    evictCtx.hClient     = pLwRm->GetClientHandle();
    evictCtx.hChanClient = pLwRm->GetClientHandle();
    evictCtx.hObject     = GetHandle();

    for (LwU32 i = 0; i < GetGpuDevice()->GetNumSubdevices(); i++)
    {
        GpuSubdevice *gpusubdev = GetGpuDevice()->GetSubdevice(i);

        CHECK_RC(pLwRm->ControlBySubdevice(gpusubdev,
                                LW2080_CTRL_CMD_GPU_EVICT_CTX,
                                (void*)&evictCtx,
                                sizeof(evictCtx)));
    }
    return rc;
}

void ChannelGroup::FreeEngCtx()
{
    Printf(Tee::PriDebug, "%s: Freeing Virtual Context memory\n",
            __FUNCTION__);
    m_engCtxMem.Free();
    m_bCtxAllocated = false;
}

ChannelGroup::SplitMethodStream::SplitMethodStream(ChannelGroup *chGrp)
    : m_chGrp(chGrp), m_pCh(NULL), m_semaVal(0)
{
    RC rc;
    m_semaSurf.SetForceSizeAlloc(true);
    m_semaSurf.SetArrayPitch(1);
    m_semaSurf.SetArraySize(0x1000);
    m_semaSurf.SetColorFormat(ColorUtils::VOID32);
    m_semaSurf.SetAddressModel(Memory::Paging);
    m_semaSurf.SetLayout(Surface2D::Pitch);
    m_semaSurf.SetLocation(Memory::Fb);
    rc = m_semaSurf.Alloc(chGrp->GetGpuDevice(), chGrp->GetRmClient());
    MASSERT(rc == OK);
    rc = m_semaSurf.Map();
    MASSERT(rc == OK);
    MEM_WR32(m_semaSurf.GetAddress(), m_semaVal);
}

ChannelGroup::SplitMethodStream::SplitMethodStream(Channel *pCh)
    : m_chGrp(NULL), m_pCh(pCh), m_semaVal(0)
{
    // No-op
}

ChannelGroup::SplitMethodStream::~SplitMethodStream()
{
    m_semaSurf.Free();
}

RC ChannelGroup::SplitMethodStream::WritePerChannel(LwRm::Handle hCh, LwU32 subCh, LwU32 method, LwU32 data)
{
    RC rc;

    // This is intended to be used only with channel group
    MASSERT(m_chGrp);

    Channel *pCh = m_chGrp->GetChannelFromHandle(hCh);
    MASSERT(pCh);

    CHECK_RC(pCh->Write(subCh, method, data));

    return rc;
}

RC ChannelGroup::SplitMethodStream::Write(LwRm::Handle hCh, LwU32 subCh, LwU32 method, LwU32 data)
{
    if (hCh)
        return WritePerChannel(hCh, subCh, method, data);
    else
        return Write(subCh, method, data);
}

RC ChannelGroup::SplitMethodStream::Write(LwU32 subCh, LwU32 method, LwU32 data)
{
    RC rc;
    Channel *pCh = NULL;

    if (m_chGrp)
    {
        pCh = m_chGrp->GetChannel(m_semaVal % m_chGrp->GetNumChannels());
        CHECK_RC(pCh->SetSemaphoreOffset(m_semaSurf.GetCtxDmaOffsetGpu()));
        CHECK_RC(pCh->SemaphoreAcquire(m_semaVal));
    }
    else
        pCh = m_pCh;

    CHECK_RC(pCh->Write(subCh, method, data));

    if (m_chGrp)
    {
        CHECK_RC(pCh->SetSemaphoreOffset(m_semaSurf.GetCtxDmaOffsetGpu()));
        CHECK_RC(pCh->SemaphoreRelease(++m_semaVal));
    }
    return rc;
}

RC ChannelGroup::SplitMethodStream::WriteMultipleBegin(LwU32 subCh)
{
    RC rc = OK;

    if (m_chGrp)
    {
        Channel *pCh = m_chGrp->GetChannel(m_semaVal % m_chGrp->GetNumChannels());
        CHECK_RC(pCh->SetSemaphoreOffset(m_semaSurf.GetCtxDmaOffsetGpu()));
        CHECK_RC(pCh->SemaphoreAcquire(m_semaVal));
    }

    return rc;
}

RC ChannelGroup::SplitMethodStream::WriteMultiple(LwU32 subCh, LwU32 method, LwU32 data)
{
    RC       rc;
    Channel *pCh;

    if (m_chGrp)
        pCh = m_chGrp->GetChannel(m_semaVal % m_chGrp->GetNumChannels());
    else
        pCh = m_pCh;

    CHECK_RC(pCh->Write(subCh, method, data));

    return rc;
}

RC ChannelGroup::SplitMethodStream::WriteMultipleEnd(LwU32 subCh)
{
    RC rc = OK;

    if (m_chGrp)
    {
        Channel *pCh = m_chGrp->GetChannel(m_semaVal % m_chGrp->GetNumChannels());

        CHECK_RC(pCh->SetSemaphoreOffset(m_semaSurf.GetCtxDmaOffsetGpu()));
        CHECK_RC(pCh->SemaphoreRelease(++m_semaVal));
    }

    return rc;
}
