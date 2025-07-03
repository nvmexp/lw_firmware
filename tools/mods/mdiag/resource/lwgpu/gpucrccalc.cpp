/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2018, 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpucrccalc.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/tests/gpu/trace_3d/tracechan.h"
#include "mdiag/utils/utils.h"

#include "class/cla297.h" // KEPLER_C
#include "kepler/gk104/dev_mmu.h"
#include "mdiag/smc/smcengine.h"

// --------------------------------------------------------------------------
//  c'tor
// --------------------------------------------------------------------------
GpuCrcCallwlator::GpuCrcCallwlator(WfiType wfiType, LWGpuChannel* ch, UINT32 size, EngineType engineType)
    : DmaReader(wfiType, ch, size, engineType)
{
}

GpuCrcCallwlator* GpuCrcCallwlator::CreateGpuCrcCallwlator
(
    WfiType wfiType,
    LWGpuResource* lwgpu,
    LWGpuChannel* channel,
    UINT32 size,
    const ArgReader* params,
    LwRm *pLwRm,
    SmcEngine *pSmcEngine,
    UINT32 subdev
)
{
    GpuCrcCallwlator* result = nullptr;
    MASSERT(lwgpu);
    UINT32 engineId = LW2080_ENGINE_TYPE_NULL;

    if (DmaReader::GetEngineType(params, lwgpu) == GPUCRC_FECS)
    {
        result = new FECSCrcCallwlator(wfiType, channel, size);
        result->m_Class = KEPLER_C;
        // EngineId for channels is relevant only starting from Ampere
        if (lwgpu->GetGpuDevice()->GetSubdevice(0)->HasFeature(Device:: GPUSUB_HOST_REQUIRES_ENGINE_ID))
        {
            engineId = MDiagUtils::GetGrEngineId(0);
        }
        result->SetCreateDmaChannel(pLwRm, pSmcEngine, engineId);
        result->m_LWGpuResource = lwgpu;
        result->m_Subdev = subdev;
        result->m_Params = params;
    }
    else
    {
        MASSERT(!"Can't find a supported class for gpu CRC calculator!");
    }

    return result;
}

// --------------------------------------------------------------------------
//  d'tor
// --------------------------------------------------------------------------
GpuCrcCallwlator::~GpuCrcCallwlator()
{
}

// --------------------------------------------------------------------------
//  c'tor
// --------------------------------------------------------------------------
FECSCrcCallwlator::FECSCrcCallwlator(WfiType wfiType, LWGpuChannel* ch, UINT32 size)
    : GpuCrcCallwlator(wfiType, ch, size, GPUCRC_FECS)
{
}

// --------------------------------------------------------------------------
//  d'tor
// --------------------------------------------------------------------------
FECSCrcCallwlator::~FECSCrcCallwlator()
{
}

bool FECSCrcCallwlator::PollMailbox(void *pVoidPollArgs)
{
    PollMailboxArgs *pPollArgs = (PollMailboxArgs *)pVoidPollArgs;
    SmcEngine *pSmcEngine = pPollArgs->channel->GetSmcEngine();
    LwRm* pLwRm = pPollArgs->channel->GetLwRmPtr();
    RegHal& regHal = pPollArgs->pGpuRes->GetRegHal(pPollArgs->gpuSubdevIdx,
        pLwRm, pSmcEngine);
    UINT32 regValue = regHal.Read32(MODS_PGRAPH_PRI_FECS_CTXSW_MAILBOX, 0);
    if (pPollArgs->checkIfSet)
    {
        return ((regValue & pPollArgs->flag) != 0);
    }
    else
    {
        return ((regValue & pPollArgs->flag) == 0);
    }
}

bool FECSCrcCallwlator::PollMailboxForNonzero(void *pVoidPollArgs)
{
    PollMailboxArgs *pPollArgs = (PollMailboxArgs *)pVoidPollArgs;
    SmcEngine *pSmcEngine = pPollArgs->channel->GetSmcEngine();
    LwRm* pLwRm = pPollArgs->channel->GetLwRmPtr();
    RegHal& regHal = pPollArgs->pGpuRes->GetRegHal(pPollArgs->gpuSubdevIdx,
        pLwRm, pSmcEngine);
    UINT32 regValue = regHal.Read32(MODS_PGRAPH_PRI_FECS_CTXSW_MAILBOX, 0);
    return (regValue != 0);
}

// --------------------------------------------------------------------------
//  read line from dma to sysmem
// --------------------------------------------------------------------------
RC FECSCrcCallwlator::ReadLine
(
    UINT32 hSrcCtxDma, UINT64 SrcOffset, UINT32 SrcPitch,
    UINT32 LineCount, UINT32 LineLength,
    UINT32 gpuSubdevice, const Surface2D* srcSurf,
    MdiagSurf* dstSurf, UINT64 DstOffset
)
{
    RC rc = OK;

    bool wfiSleep = (m_wfiType == SLEEP);

    InfoPrintf("FECS CRC callwlation readline start:");
    InfoPrintf("SrcOffset: %x\n", LwU64_LO32(SrcOffset));
    InfoPrintf("LineLength: %d\n", LineLength);

    if (LineCount == 0)
        return OK;

    LWGpuResource* lwgpu = m_Channel->GetLWGpu();
    GpuSubdevice* pSubdev = lwgpu->GetGpuSubdevice(gpuSubdevice);
    SmcEngine *pSmcEngine = m_Channel->GetSmcEngine();
    RegHal& regHal = lwgpu->GetRegHal(pSubdev, m_Channel->GetLwRmPtr(), pSmcEngine);

    LW2080_CTRL_FIFO_MEM_INFO instProperties = MDiagUtils::GetInstProperties(pSubdev, m_Channel->ChannelHandle(), m_Channel->GetLwRmPtr());
    UINT32 contextPointer = LwU64_LO32(instProperties.base >> 12);
    regHal.SetField(&contextPointer, MODS_PGRAPH_PRI_FECS_NEW_CTX_VALID_TRUE);

    UINT32 contextTarget = 0;
    switch (instProperties.aperture)
    {
        case LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_APERTURE_VIDMEM:
            contextTarget = regHal.SetField(MODS_PGRAPH_PRI_FECS_NEW_CTX_TARGET_VID_MEM);
            break;
        case LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_APERTURE_SYSMEM_COH:
            contextTarget = regHal.SetField(MODS_PGRAPH_PRI_FECS_NEW_CTX_TARGET_SYS_MEM_COHERENT);
            break;
        case LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_APERTURE_SYSMEM_NCOH:
            contextTarget = regHal.SetField(MODS_PGRAPH_PRI_FECS_NEW_CTX_TARGET_SYS_MEM_NONCOHERENT);
            break;
        default:
            contextTarget = regHal.SetField(MODS_PGRAPH_PRI_FECS_NEW_CTX_TARGET_VID_MEM);
            break;
    }

    LWGpuChannel* ch = m_Channel;
    ch->CtxSwEnable(false);

    Tasker::MutexHolder mutexHolder(lwgpu->GetFECSMailboxMutex());

    PollMailboxArgs PollArgs;
    PollArgs.pGpuRes = lwgpu;
    PollArgs.gpuSubdevIdx = gpuSubdevice;
    PollArgs.channel = m_Channel;

    UINT64 dest_offset = m_DstGpuOffset;

    UINT32 lwrrentCtx = regHal.Read32(MODS_PGRAPH_PRI_FECS_LWRRENT_CTX);

    while( LineCount-- > 0 )
    {
        //bind
        regHal.Write32(MODS_PGRAPH_PRI_FECS_CTXSW_MAILBOX, 0, 0);
        regHal.Write32(MODS_PGRAPH_PRI_FECS_METHOD_DATA, contextPointer | contextTarget);
        regHal.Write32(MODS_PGRAPH_PRI_FECS_METHOD_PUSH, regHal.LookupValue(MODS_PGRAPH_PRI_FECS_METHOD_PUSH_ADR_BIND_POINTER_ONLY));
        //wait_for(LW_PGRAPH_PRI_FECS_CTXSW_MAILBOX(0)==0x10)
        PollArgs.flag = 0x10;
        PollArgs.checkIfSet = true;
        CHECK_RC(POLLWRAP(PollMailbox, &PollArgs, m_Channel->GetDefaultTimeoutMs()));

        //set size
        regHal.Write32(MODS_PGRAPH_PRI_FECS_CTXSW_MAILBOX, 0, 0);
        regHal.Write32(MODS_PGRAPH_PRI_FECS_METHOD_DATA, LineLength);
        regHal.Write32(MODS_PGRAPH_PRI_FECS_METHOD_PUSH, regHal.LookupValue(MODS_PGRAPH_PRI_FECS_METHOD_PUSH_ADR_CRC_CHECK_SET_IN_SURF_SIZE));
        //wait_for(LW_PGRAPH_PRI_FECS_CTXSW_MAILBOX(0)!=0x0)
        CHECK_RC(POLLWRAP(PollMailboxForNonzero, &PollArgs, m_Channel->GetDefaultTimeoutMs()));

        //set input offset lower bits
        regHal.Write32(MODS_PGRAPH_PRI_FECS_CTXSW_MAILBOX, 0, 0);
        regHal.Write32(MODS_PGRAPH_PRI_FECS_METHOD_DATA, (UINT32) (SrcOffset>>8));
        regHal.Write32(MODS_PGRAPH_PRI_FECS_METHOD_PUSH, regHal.LookupValue(MODS_PGRAPH_PRI_FECS_METHOD_PUSH_ADR_CRC_CHECK_SET_IN_SURF_OFFSET_LO));
        //wait_for(LW_PGRAPH_PRI_FECS_CTXSW_MAILBOX(0)!=0x0)
        CHECK_RC(POLLWRAP(PollMailboxForNonzero, &PollArgs, m_Channel->GetDefaultTimeoutMs()));

        //set input offset higher bits if offset exceeds 40 bits
        if (SrcOffset>>40)
        {
            regHal.Write32(MODS_PGRAPH_PRI_FECS_CTXSW_MAILBOX, 0, 0);
            regHal.Write32(MODS_PGRAPH_PRI_FECS_METHOD_DATA, (UINT32) (SrcOffset>>40));
            regHal.Write32(MODS_PGRAPH_PRI_FECS_METHOD_PUSH, regHal.LookupValue(MODS_PGRAPH_PRI_FECS_METHOD_PUSH_ADR_CRC_CHECK_SET_IN_SURF_OFFSET_HI));
            //wait_for(LW_PGRAPH_PRI_FECS_CTXSW_MAILBOX(0)!=0x0)
            CHECK_RC(POLLWRAP(PollMailboxForNonzero, &PollArgs, m_Channel->GetDefaultTimeoutMs()));
        }

        //set output offset lower bits
        regHal.Write32(MODS_PGRAPH_PRI_FECS_CTXSW_MAILBOX, 0, 0);
        regHal.Write32(MODS_PGRAPH_PRI_FECS_METHOD_DATA, (UINT32) (dest_offset>>8));
        regHal.Write32(MODS_PGRAPH_PRI_FECS_METHOD_PUSH, regHal.LookupValue(MODS_PGRAPH_PRI_FECS_METHOD_PUSH_ADR_CRC_CHECK_SET_OUT_SURF_OFFSET_LO));
        //wait_for(LW_PGRAPH_PRI_FECS_CTXSW_MAILBOX(0)!=0x0)
        CHECK_RC(POLLWRAP(PollMailboxForNonzero, &PollArgs, m_Channel->GetDefaultTimeoutMs()));

        //set output offset higher bits if offset exceeds 40 bits
        if (dest_offset>>40)
        {
            regHal.Write32(MODS_PGRAPH_PRI_FECS_CTXSW_MAILBOX, 0, 0);
            regHal.Write32(MODS_PGRAPH_PRI_FECS_METHOD_DATA, (UINT32) (dest_offset>>40));
            regHal.Write32(MODS_PGRAPH_PRI_FECS_METHOD_PUSH, regHal.LookupValue(MODS_PGRAPH_PRI_FECS_METHOD_PUSH_ADR_CRC_CHECK_SET_OUT_SURF_OFFSET_HI));
            //wait_for(LW_PGRAPH_PRI_FECS_CTXSW_MAILBOX(0)!=0x0)
            CHECK_RC(POLLWRAP(PollMailboxForNonzero, &PollArgs, m_Channel->GetDefaultTimeoutMs()));
        }

        //execute command
        regHal.Write32(MODS_PGRAPH_PRI_FECS_CTXSW_MAILBOX, 0, 0);
        regHal.Write32(MODS_PGRAPH_PRI_FECS_METHOD_DATA, 0);
        regHal.Write32(MODS_PGRAPH_PRI_FECS_METHOD_PUSH, regHal.LookupValue(MODS_PGRAPH_PRI_FECS_METHOD_PUSH_ADR_CRC_CHECK_CALLWLATE));
        //wait_for(LW_PGRAPH_PRI_FECS_CTXSW_MAILBOX(0)!=0x0)
        if (wfiSleep)
        {
            Platform::SleepUS(10000000);
        }
        else
        {
            CHECK_RC(POLLWRAP(PollMailboxForNonzero, &PollArgs, m_Channel->GetDefaultTimeoutMs()));
            CHECK_RC(lwgpu->PollRegValue(regHal.LookupAddress(MODS_PGRAPH_ENGINE_STATUS), 0, ~0U, m_Channel->GetDefaultTimeoutMs(), gpuSubdevice, pSmcEngine));
        }

        SrcOffset += SrcPitch;
        dest_offset += SrcPitch;
    }

    // binding back to the current resident context to prevent fecs ucode being messed up (bug 3001919)
    regHal.Write32(MODS_PGRAPH_PRI_FECS_CTXSW_MAILBOX, 0, 0);
    regHal.Write32(MODS_PGRAPH_PRI_FECS_METHOD_DATA, lwrrentCtx);
    regHal.Write32(MODS_PGRAPH_PRI_FECS_METHOD_PUSH, regHal.LookupValue(MODS_PGRAPH_PRI_FECS_METHOD_PUSH_ADR_BIND_POINTER_ONLY));
    //wait_for(LW_PGRAPH_PRI_FECS_CTXSW_MAILBOX(0)==0x10)
    PollArgs.flag = 0x10;
    PollArgs.checkIfSet = true;
    CHECK_RC(POLLWRAP(PollMailbox, &PollArgs, m_Channel->GetDefaultTimeoutMs()));

    mutexHolder.Release();

    ch->CtxSwEnable(true);

    return OK;
}

