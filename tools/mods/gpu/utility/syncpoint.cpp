/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  syncpoint.cpp
 * @brief Abstraction representing a syncpoint (CheetAh) and a syncpoint base - imlementation
 */

#include "syncpoint.h"
#include "core/include/massert.h"
#include "lwmisc.h"
#include "core/include/tasker.h"
#include "class/cle2ad.h"
#include "ctrl/ctrle2ad.h"

SyncPoint::SyncPoint()
: m_pLwRm(0)
, m_Handle(0)
, m_InitValue(0)
, m_ForceIndex(0)
, m_AutoIncrement(false)
, m_Threshold(~0U)
, m_Index(0)
{
}

SyncPoint::~SyncPoint()
{
    Free();
}

RC SyncPoint::Alloc(GpuDevice* pGpuDev, LwRm* pLwRm)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;

}

void SyncPoint::Free()
{
    if (m_Handle)
    {
        m_pLwRm->Free(m_Handle);
        m_Handle = 0;
    }
}

RC SyncPoint::SetValue(UINT32 value)
{

    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

RC SyncPoint::GetValue(UINT32* pValue, UINT32* pThreshold) const
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

RC SyncPoint::SetThreshold(UINT32 threshold, bool autoIncrement)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

RC SyncPoint::Reset()
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

RC SyncPoint::Wait(UINT32 Value, FLOAT64 Timeout) const
{
    SyncPointWaitArgs waitArgs;
    waitArgs.pSyncPoint = this;
    waitArgs.value = Value;
    return POLLWRAP_HW(SyncPoint::WaitValue, &waitArgs, Timeout);
}

/* static */ bool SyncPoint::WaitValue(void * pvWaitArgs)
{
    SyncPointWaitArgs * pWaitArgs = static_cast<SyncPointWaitArgs *>(pvWaitArgs);

    UINT32 lwrValue;
    pWaitArgs->pollRc = pWaitArgs->pSyncPoint->GetValue(&lwrValue);

    if ((OK != pWaitArgs->pollRc) || (lwrValue >= pWaitArgs->value))
        return true;

    return false;
}

SyncPointBase::SyncPointBase()
: m_pLwRm(0)
, m_Handle(0)
, m_InitValue(0)
, m_ForceIndex(LW_SYNCPOINT_BASE_ALLOC_INDEX_NONE)
, m_Index(LW_SYNCPOINT_BASE_ALLOC_INDEX_NONE)
{
}

SyncPointBase::~SyncPointBase()
{
    Free();
}

RC SyncPointBase::Alloc(GpuDevice* pGpuDev, LwRm* pLwRm)
{
    if (m_Handle)
    {
        Printf(Tee::PriHigh, "SyncPointBase has already been initialized!\n");
        return RC::SOFTWARE_ERROR;
    }

    m_pLwRm = pLwRm ? pLwRm : LwRmPtr(0).Get();

    LW_SYNCPOINT_BASE_ALLOCATION_PARAMETERS allocParams = {0};
    allocParams.initialValue = m_InitValue;
    allocParams.initialIndex = m_ForceIndex;

    RC rc;
    CHECK_RC(m_pLwRm->Alloc(
                m_pLwRm->GetDeviceHandle(pGpuDev),
                &m_Handle,
                LWE2_SYNCPOINT_BASE,
                &allocParams));

    m_Index = allocParams.index;
    return rc;
}

void SyncPointBase::Free()
{
    if (m_Handle)
    {
        m_pLwRm->Free(m_Handle);
        m_Handle = 0;
    }
}

RC SyncPointBase::SetValue(UINT32 value)
{
    if (m_Handle)
    {
        LWE2AD_CTRL_SET_VALUE_PARAMS params = {0};
        params.value = value;
        return m_pLwRm->Control(m_Handle, LWE2AD_CTRL_CMD_SET_VALUE, &params, sizeof(params));
    }
    else
    {
        m_InitValue = value;
        return OK;
    }
}

RC SyncPointBase::GetValue(UINT32* pValue) const
{
    MASSERT(pValue);

    MASSERT(m_Handle);
    if (!m_Handle)
    {
        Printf(Tee::PriHigh, "SyncPointBase has not been initialized yet!\n");
        return RC::SOFTWARE_ERROR;
    }

    LWE2AD_CTRL_GET_VALUE_PARAMS params = {0};
    RC rc;
    CHECK_RC(m_pLwRm->Control(m_Handle, LWE2AD_CTRL_CMD_GET_VALUE, &params, sizeof(params)));

    *pValue = params.value;

    return OK;
}

RC SyncPointBase::Reset()
{
    MASSERT(m_Handle);
    if (!m_Handle)
    {
        Printf(Tee::PriHigh, "SyncPointBase has not been initialized yet!\n");
        return RC::SOFTWARE_ERROR;
    }

    return m_pLwRm->Control(m_Handle, LWE2AD_CTRL_CMD_RESET, 0, 0);
}

