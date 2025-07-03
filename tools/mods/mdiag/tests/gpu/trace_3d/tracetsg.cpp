/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/**
 * @file   tracetsg.h
 * @brief  TraceTsg: A local view of TSG in trace level of trace_3d;
 *                   Hold parameters and resources to create LWGpuTsg.
 *
Relation diagram:

Trace3DTest
     ||        ------------------------>-------------------------
     ||        |                                                 |
     <==> TraceTsg <====> TraceChannel ---> LWGpuChannel <====> LWGpuTsg
     ||        ||                                                ||
     ||        ||==<====> TraceChannel ---> LWGpuChannel <====>==||
     ||
     ||<==> TraceTsg ....
     ||<==> TraceTsg ....
*/
#include <list>
#include <string>
#include "tracechan.h"
#include "tracetsg.h"
#include "trace_3d.h"
#include "mdiag/tests/gpu/lwgpu_tsg.h"

TraceTsg::TraceTsg
(
    const string& name
    ,Trace3DTest * test
    ,const shared_ptr<VaSpace> &pVaSpace
    ,bool bShared
) :
    m_TsgName(name),
    m_Trace3DTest(test),
    m_pVaSpace(pVaSpace),
    m_IsShared(bShared),
    m_pGpuTsg(0),
    m_TsgTimeSliceOverride(false),
    m_TsgTimeSliceUs(0),
    m_PartitionMode(PEConfigureFile::NONE)
{
    MASSERT(0 != m_Trace3DTest);

    const ArgReader* param = m_Trace3DTest->GetParam();
    if (param)
    {
        m_TsgTimeSliceOverride = param->ParamPresent("-tsg_timeslice") > 0;
        m_TsgTimeSliceUs = param->ParamUnsigned("-tsg_timeslice", 0);
    }
}

TraceTsg::~TraceTsg()
{
    Free();
}

void TraceTsg::Free()
{
    if (m_pGpuTsg)
    {
        m_pGpuTsg->RelRef();
        if (0 == m_pGpuTsg->GetRefCount())
        {
            delete m_pGpuTsg;
        }

        // TraceTsg does not need to reference the
        // LWGpuTsg pointer no matter it's freed or not
        m_pGpuTsg = nullptr;
    }
    m_pVaSpace.reset();
}

RC TraceTsg::AllocLWGpuTsg
(
    UINT32 hwClass,
    LWGpuTsg** ppGpuTsg,
    LWGpuResource* lwgpu,
    UINT32 engineId
)
{
    MASSERT(0 != lwgpu);
    MASSERT(0 == m_pGpuTsg);
    RC rc;
    Utility::CleanupOnReturn<TraceTsg> Cleanup(this, &TraceTsg::Free);

    LwRm::Handle hVaSpace = !m_pVaSpace ? 0 : m_pVaSpace->GetHandle();
    m_pGpuTsg = LWGpuTsg::Factory(m_TsgName, lwgpu, m_Trace3DTest->GetParam(),
                                  hwClass, hVaSpace, m_IsShared, m_Trace3DTest->GetLwRmPtr(), 
                                  engineId, m_Trace3DTest->GetSmcEngine());
    if (!m_pGpuTsg)
    {
        return RC::SOFTWARE_ERROR;
    }

    if (engineId != m_pGpuTsg->GetEngineId())
    {
        ErrPrintf("%s: Trace's engine (%s) does not match TSG's engine (%s)\n",
                    __FUNCTION__, 
                    lwgpu->GetGpuDevice()->GetEngineName(engineId), 
                    lwgpu->GetGpuDevice()->GetEngineName(m_pGpuTsg->GetEngineId()));
        MASSERT(0);
        return RC::SOFTWARE_ERROR;
    }

    m_pGpuTsg->AddRef();

    if (m_TsgTimeSliceOverride)
    {
        CHECK_RC(CheckTsgTimeSlice());
        CHECK_RC(m_pGpuTsg->SetTsgTimeSlice((UINT64)m_TsgTimeSliceUs));
    }

    if (ppGpuTsg) *ppGpuTsg = m_pGpuTsg;

    Cleanup.Cancel();
    return OK;
}

LWGpuTsg* TraceTsg::GetLWGpuTsg() const
{
    return m_pGpuTsg;
}

RC TraceTsg::AddTraceChannel(TraceChannel* traceCh)
{
    list<TraceChannel*>::iterator it = m_TraceChannels.begin();
    for (; it != m_TraceChannels.end(); it ++)
    {
        if (*it == traceCh)
        {
            break;
        }
    }

    if (it == m_TraceChannels.end())
    {
        m_TraceChannels.push_back(traceCh);
    }

    return OK;
}

RC TraceTsg::RemoveTraceChannel(TraceChannel* traceCh)
{
    list<TraceChannel*>::iterator it = m_TraceChannels.begin();
    for (; it != m_TraceChannels.end(); it ++)
    {
        if (*it == traceCh)
        {
            m_TraceChannels.erase(it);
            break;
        }
    }

    return OK;
}

// Scan the TraceChannel list to find the first
// CPU managed channel; return 0 if not found
TraceChannel* TraceTsg::GetCpuMngTraceChannel() const
{
    list<TraceChannel*>::const_iterator it = m_TraceChannels.begin();
    for (; it != m_TraceChannels.end(); it ++)
    {
        if (!(*it)->IsGpuManagedChannel())
        {
            return *it;
        }
    }

    return 0;
}

RC TraceTsg::CheckTsgTimeSlice()
{
    if (m_TsgTimeSliceOverride)
    {
        if (0 == m_TsgTimeSliceUs)
        {
            ErrPrintf("%s: can't use 0 to override tsg timeslice! "
                "Please check the value of -tsg_timeslice.\n", __FUNCTION__);
            return RC::ILWALID_INPUT;
        }

        if (m_pGpuTsg)
        {
            UINT64 timeslice = m_pGpuTsg->GetTimeSliceUs();
            if (Tsg::DEFAULT_TIME_SLICE != timeslice &&
                m_TsgTimeSliceUs != static_cast<UINT32>(timeslice))
            {
                ErrPrintf("%s: tsg timeslice %u has been set. The new value %u"
                    " to be set is different from the old one."
                    " Please check the value of -tsg_timeslice.\n",
                    __FUNCTION__, static_cast<UINT32>(timeslice),
                    m_TsgTimeSliceUs);

                return RC::ILWALID_INPUT;
            }
        }
    }

    return OK;
}

RC TraceTsg::SetPartitionMode(PEConfigureFile::PARTITION_MODE mode)
{
    RC rc;

    if(m_PartitionMode == PEConfigureFile::NONE ||
        m_PartitionMode == mode)
    {
        m_PartitionMode = mode;
    }
    else
    {
        ErrPrintf("%s: Ilwalide reset the tsg mode in one tsg %s. Please check the "
            "partition table modle setting.\n", __FUNCTION__, GetName().c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    return rc;
}
