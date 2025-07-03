/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Kepler Channel

#include "core/include/channel.h"
#include "gpu/utility/rchelper.h"
#include "gpu/utility/tsg.h"
#include "class/cla06f.h" // KEPLER_CHANNEL_GPFIFO_A
#include "class/cla16f.h" // KEPLER_CHANNEL_GPFIFO_B
#include "class/cla26f.h" // KEPLER_CHANNEL_GPFIFO_C
#include "class/clb06f.h" // MAXWELL_CHANNEL_GPFIFO_A
#include "class/clc06f.h" // PASCAL_CHANNEL_GPFIFO_A
#include "ctrl/ctrla06c.h"
#include "ctrl/ctrla06f.h"
#include "ctrl/ctrla16f.h"
#include "ctrl/ctrla26f.h"
#include "ctrl/ctrlb06f.h"
#include "ctrl/ctrlc06f.h"

KeplerGpFifoChannel::KeplerGpFifoChannel
(
        UserdAllocPtr pUserdAlloc,
        void *        pPushBufferBase,
        UINT32        PushBufferSize,
        void *        pErrorNotifierMemory,
        LwRm::Handle  hChannel,
        UINT32        ChID,
        UINT32        Class,
        UINT64        PushBufferOffset,
        void *        pGpFifoBase,
        UINT32        GpFifoEntries,
        LwRm::Handle  hVASpace,
        GpuDevice *   pGrDev,
        LwRm *        pLwRm,
        Tsg *         pTsg
)
:   FermiGpFifoChannel(pUserdAlloc,
                       pPushBufferBase,
                       PushBufferSize,
                       pErrorNotifierMemory,
                       hChannel,
                       ChID,
                       Class,
                       PushBufferOffset,
                       pGpFifoBase,
                       GpFifoEntries,
                       hVASpace,
                       pGrDev,
                       pLwRm)
    , m_NeedsSchedule(true)
    , m_pTsg(pTsg)
{
    if (m_pTsg)
    {
        MASSERT(pErrorNotifierMemory == NULL);
        m_pRcHelper.reset();
        m_pTsg->AddChannel(this);
    }
}

KeplerGpFifoChannel::~KeplerGpFifoChannel()
{
    if (m_pTsg)
    {
        m_pTsg->RemoveChannel(this);
    }
}

/* virtual */ RC KeplerGpFifoChannel::ScheduleChannel(bool Enable)
{
    RC rc;

    if (m_NeedsSchedule)
    {
        // Make sure there are no channel errors before attempting to schedule
        //
        CHECK_RC(CheckError());

        if (m_pTsg)
        {
            // Schedule the tsg which the channel belongs to
            LWA06C_CTRL_GPFIFO_SCHEDULE_PARAMS schedule_params = {0};
            schedule_params.bEnable = Enable? LW_TRUE : LW_FALSE;

            CHECK_RC(GetRmClient()->Control(m_pTsg->GetHandle(),
                                            LWA06C_CTRL_CMD_GPFIFO_SCHEDULE,
                                            &schedule_params,
                                            sizeof(schedule_params)));
        }
        else
        {
            // Schedule the channel, as we know now what
            // object will be used on it and RM can pick up
            // a matching per-engine round list.
            CHECK_RC(RmcGpfifoSchedule(Enable));
        }

        m_NeedsSchedule = false;
    }

    return rc;
}

/* virtual */ RC KeplerGpFifoChannel::SetObject
(
    UINT32 Subchannel,
    LwRm::Handle Handle
)
{
    RC rc;

    CHECK_RC(ScheduleChannel(true));
    CHECK_RC(FermiGpFifoChannel::SetObject(Subchannel, Handle));

    return rc;
}

RC KeplerGpFifoChannel::SemaphoreReduction(Reduction Op, ReductionType redType, UINT64 Data)
{
    // We expect semaphor reductions to be supported in GPFIFO_B on
    // up, *except* for GPFIFO_C which is a cheetah class.
    //
    if ((m_Class == KEPLER_CHANNEL_GPFIFO_A) ||
        (m_Class == KEPLER_CHANNEL_GPFIFO_C))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (Data >> 32)
    {
        Printf(Tee::PriError, "64bit semaphores are not supported!\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    UINT32 SemaphoreD = 0;
    SemaphoreD |= DRF_DEF(A16F, _SEMAPHORED, _OPERATION, _REDUCTION);
    switch (redType)
    {
        case REDUCTION_UNSIGNED:
            SemaphoreD |= DRF_DEF(A16F, _SEMAPHORED, _FORMAT, _UNSIGNED);
            break;
        case REDUCTION_SIGNED:
            SemaphoreD |= DRF_DEF(A16F, _SEMAPHORED, _FORMAT, _SIGNED);
            break;
        default:
            MASSERT(!"Bad reduction type in SemaphoreReduction");
            return RC::SOFTWARE_ERROR;
    }
    SemaphoreD |= ((GetSemaphoreReleaseFlags() & FlagSemRelWithWFI) ?
                   DRF_DEF(A16F, _SEMAPHORED, _RELEASE_WFI, _EN) :
                   DRF_DEF(A16F, _SEMAPHORED, _RELEASE_WFI, _DIS));
    SemaphoreD |= ((GetSemaphoreReleaseFlags() & FlagSemRelWithTime) ?
                   DRF_DEF(A16F, _SEMAPHORED, _RELEASE_SIZE, _16BYTE) :
                   DRF_DEF(A16F, _SEMAPHORED, _RELEASE_SIZE, _4BYTE));
    switch (Op)
    {
        case REDUCTION_MIN:
            SemaphoreD |= DRF_DEF(A16F, _SEMAPHORED, _REDUCTION, _MIN);
            break;
        case REDUCTION_MAX:
            SemaphoreD |= DRF_DEF(A16F, _SEMAPHORED, _REDUCTION, _MAX);
            break;
        case REDUCTION_XOR:
            SemaphoreD |= DRF_DEF(A16F, _SEMAPHORED, _REDUCTION, _XOR);
            break;
        case REDUCTION_AND:
            SemaphoreD |= DRF_DEF(A16F, _SEMAPHORED, _REDUCTION, _AND);
            break;
        case REDUCTION_OR:
            SemaphoreD |= DRF_DEF(A16F, _SEMAPHORED, _REDUCTION, _OR);
            break;
        case REDUCTION_ADD:
            SemaphoreD |= DRF_DEF(A16F, _SEMAPHORED, _REDUCTION, _ADD);
            break;
        case REDUCTION_INC:
            SemaphoreD |= DRF_DEF(A16F, _SEMAPHORED, _REDUCTION, _INC);
            break;
        case REDUCTION_DEC:
            SemaphoreD |= DRF_DEF(A16F, _SEMAPHORED, _REDUCTION, _DEC);
            break;
        default:
            MASSERT(!"Bad reduction in SemaphoreReduction");
            return RC::SOFTWARE_ERROR;
    }

    return GetWrapper()->Write(
                0, LWA16F_SEMAPHOREA, 4,
                static_cast<UINT32>(GetHostSemaOffset()>>32),
                static_cast<UINT32>(GetHostSemaOffset()),
                static_cast<UINT32>(Data),
                SemaphoreD);
}

RC KeplerGpFifoChannel::WaitIdle(FLOAT64 TimeoutMs)
{
    // If the channel isn't scheduled, don't WFI
    if (m_NeedsSchedule)
        return OK;

    return FermiGpFifoChannel::WaitIdle(TimeoutMs);
}

RC KeplerGpFifoChannel::SetAutoSchedule(bool bSched)
{
    m_NeedsSchedule = bSched;
    return OK;
}

/* virtual */ RC KeplerGpFifoChannel::CheckError()
{
    if (m_pTsg)
    {
        m_pTsg->UpdateError();
        return m_Error;
    }
    return FermiGpFifoChannel::CheckError();
}

/* virtual */ RC KeplerGpFifoChannel::RmcGetClassEngineId
(
    LwRm::Handle hObject,
    UINT32 *pClassEngineId,
    UINT32 *pClassId,
    UINT32 *pEngineId
)
{
    LwRm *pLwRm = GetRmClient();
    LwRm::Handle hCh = GetHandle();
    RC rc;

    switch (GetClass())
    {
        case KEPLER_CHANNEL_GPFIFO_A:
        {
            LWA06F_CTRL_GET_CLASS_ENGINEID_PARAMS Params = {0};
            Params.hObject = hObject;
            CHECK_RC(pLwRm->Control(hCh, LWA06F_CTRL_GET_CLASS_ENGINEID,
                                    &Params, sizeof(Params)));
            if (pClassEngineId)
                *pClassEngineId = Params.classEngineID;
            if (pClassId)
                *pClassId = Params.classID;
            if (pEngineId)
                *pEngineId = Params.engineID;
            break;
        }
        case KEPLER_CHANNEL_GPFIFO_B:
        {
            LWA16F_CTRL_GET_CLASS_ENGINEID_PARAMS Params = {0};
            Params.hObject = hObject;
            CHECK_RC(pLwRm->Control(hCh, LWA16F_CTRL_GET_CLASS_ENGINEID,
                                    &Params, sizeof(Params)));
            if (pClassEngineId)
                *pClassEngineId = Params.classEngineID;
            if (pClassId)
                *pClassId = Params.classID;
            if (pEngineId)
                *pEngineId = Params.engineID;
            break;
        }
        case KEPLER_CHANNEL_GPFIFO_C:
        {
            LWA26F_CTRL_GET_CLASS_ENGINEID_PARAMS Params = {0};
            Params.hObject = hObject;
            CHECK_RC(pLwRm->Control(hCh, LWA26F_CTRL_GET_CLASS_ENGINEID,
                                    &Params, sizeof(Params)));
            if (pClassEngineId)
                *pClassEngineId = Params.classEngineID;
            if (pClassId)
                *pClassId = Params.classID;
            if (pEngineId)
                *pEngineId = Params.engineID;
            break;
        }
        case MAXWELL_CHANNEL_GPFIFO_A:
        {
            LWB06F_CTRL_GET_CLASS_ENGINEID_PARAMS Params = {0};
            Params.hObject = hObject;
            CHECK_RC(pLwRm->Control(hCh, LWB06F_CTRL_GET_CLASS_ENGINEID,
                                    &Params, sizeof(Params)));
            if (pClassEngineId)
                *pClassEngineId = Params.classEngineID;
            if (pClassId)
                *pClassId = Params.classID;
            if (pEngineId)
                *pEngineId = Params.engineID;
            break;
        }
        case PASCAL_CHANNEL_GPFIFO_A:
        {
            LWC06F_CTRL_GET_CLASS_ENGINEID_PARAMS Params = {0};
            Params.hObject = hObject;
            CHECK_RC(pLwRm->Control(hCh, LWC06F_CTRL_GET_CLASS_ENGINEID,
                                    &Params, sizeof(Params)));
            if (pClassEngineId)
                *pClassEngineId = Params.classEngineID;
            if (pClassId)
                *pClassId = Params.classID;
            if (pEngineId)
                *pEngineId = Params.engineID;
            break;
        }
        default:
        {
            return RC::UNSUPPORTED_FUNCTION;
        }
    }

    return rc;
}

/* virtual */ RC KeplerGpFifoChannel::RmcResetChannel(UINT32 EngineId)
{
    LwRm *pLwRm = GetRmClient();
    LwRm::Handle hCh = GetHandle();

    switch (GetClass())
    {
        case KEPLER_CHANNEL_GPFIFO_A:
        {
            LWA06F_CTRL_CMD_RESET_CHANNEL_PARAMS Params = {0};
            Params.engineID = EngineId;
            return pLwRm->Control(hCh, LWA06F_CTRL_CMD_RESET_CHANNEL,
                                  &Params, sizeof(Params));
        }
        case KEPLER_CHANNEL_GPFIFO_B:
        {
            LWA16F_CTRL_CMD_RESET_CHANNEL_PARAMS Params = {0};
            Params.engineID = EngineId;
            return pLwRm->Control(hCh, LWA16F_CTRL_CMD_RESET_CHANNEL,
                                  &Params, sizeof(Params));
        }
        case KEPLER_CHANNEL_GPFIFO_C:
        {
            LWA26F_CTRL_CMD_RESET_CHANNEL_PARAMS Params = {0};
            Params.engineID = EngineId;
            return pLwRm->Control(hCh, LWA26F_CTRL_CMD_RESET_CHANNEL,
                                  &Params, sizeof(Params));
        }
        case MAXWELL_CHANNEL_GPFIFO_A:
        {
            LWB06F_CTRL_CMD_RESET_CHANNEL_PARAMS Params = {0};
            Params.engineID = EngineId;
            return pLwRm->Control(hCh, LWB06F_CTRL_CMD_RESET_CHANNEL,
                                  &Params, sizeof(Params));
        }
        case PASCAL_CHANNEL_GPFIFO_A:
        {
            LWC06F_CTRL_CMD_RESET_CHANNEL_PARAMS Params = {0};
            Params.engineID = EngineId;
            return pLwRm->Control(hCh, LWC06F_CTRL_CMD_RESET_CHANNEL,
                                  &Params, sizeof(Params));
        }
    }

    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC KeplerGpFifoChannel::RmcGpfifoSchedule(bool Enable)
{
    LwRm *pLwRm = GetRmClient();
    LwRm::Handle hCh = GetHandle();

    switch (GetClass())
    {
        case KEPLER_CHANNEL_GPFIFO_A:
        {
            LWA06F_CTRL_GPFIFO_SCHEDULE_PARAMS Params = {0};
            Params.bEnable = Enable ? LW_TRUE : LW_FALSE;
            return pLwRm->Control(hCh, LWA06F_CTRL_CMD_GPFIFO_SCHEDULE,
                                  &Params, sizeof(Params));
        }
        case KEPLER_CHANNEL_GPFIFO_B:
        {
            LWA16F_CTRL_GPFIFO_SCHEDULE_PARAMS Params = {0};
            Params.bEnable = Enable ? LW_TRUE : LW_FALSE;
            return pLwRm->Control(hCh, LWA16F_CTRL_CMD_GPFIFO_SCHEDULE,
                                  &Params, sizeof(Params));
        }
        case KEPLER_CHANNEL_GPFIFO_C:
        {
            LWA26F_CTRL_GPFIFO_SCHEDULE_PARAMS Params = {0};
            Params.bEnable = Enable ? LW_TRUE : LW_FALSE;
            return pLwRm->Control(hCh, LWA26F_CTRL_CMD_GPFIFO_SCHEDULE,
                                  &Params, sizeof(Params));
        }
        case MAXWELL_CHANNEL_GPFIFO_A:
        {
            LWB06F_CTRL_GPFIFO_SCHEDULE_PARAMS Params = {0};
            Params.bEnable = Enable ? LW_TRUE : LW_FALSE;
            return pLwRm->Control(hCh, LWB06F_CTRL_CMD_GPFIFO_SCHEDULE,
                                  &Params, sizeof(Params));
        }
        case PASCAL_CHANNEL_GPFIFO_A:
        {
            LWC06F_CTRL_GPFIFO_SCHEDULE_PARAMS Params = {0};
            Params.bEnable = Enable ? LW_TRUE : LW_FALSE;
            return pLwRm->Control(hCh, LWC06F_CTRL_CMD_GPFIFO_SCHEDULE,
                                  &Params, sizeof(Params));
        }
    }

    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RcHelper *KeplerGpFifoChannel::GetRcHelper() const
{
    return m_pTsg ? m_pTsg->GetRcHelper() : FermiGpFifoChannel::GetRcHelper();
}
