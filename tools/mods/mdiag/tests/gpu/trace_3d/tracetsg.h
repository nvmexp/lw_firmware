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
 * @brief  TraceTsg: per-Tsg resources used by Trace3DTest.
 */
#ifndef INCLUDED_TRACETSG_H
#define INCLUDED_TRACETSG_H

class Trace3DTest;
class TraceChannel;
class LWGpuTsg;
#include "pe_configurefile.h"

//------------------------------------------------------------------------------
// TraceTsg -- holds per-Tsg resources for Trace3DTest.
//
class TraceTsg
{
public:
    TraceTsg(const string& name, Trace3DTest * test,
             const shared_ptr<VaSpace> &pVaSpace, bool bShared);
    ~TraceTsg();

    void Free();

    string GetName() const { return m_TsgName; }
    RC AllocLWGpuTsg(UINT32 hwClass,  LWGpuTsg** ppGpuTsg, LWGpuResource* lwgpu, UINT32 engineId);
    LWGpuTsg* GetLWGpuTsg() const;

    RC AddTraceChannel(TraceChannel* traceCh);
    RC RemoveTraceChannel(TraceChannel* traceCh);
    list<TraceChannel*>& GetTraceChannels() { return m_TraceChannels; }

    TraceChannel* GetCpuMngTraceChannel() const;

    UINT32 GetTraceChannelNum() const { return static_cast<UINT32>(m_TraceChannels.size()); }
    bool IsSharedTsg() const { return m_IsShared; }

    RC SetPartitionMode(PEConfigureFile::PARTITION_MODE mode);
    PEConfigureFile::PARTITION_MODE GetPartitionMode() { return m_PartitionMode; }
private:
    RC CheckTsgTimeSlice();

    string m_TsgName;
    Trace3DTest* m_Trace3DTest;
    shared_ptr<VaSpace> m_pVaSpace;
    bool m_IsShared; // true: lwgputsg object shared between t3d/v2d tests
                     // false: lwgputsg object owned by a t3d test locally
    LWGpuTsg* m_pGpuTsg;

    list<TraceChannel*> m_TraceChannels;

    bool m_TsgTimeSliceOverride;
    UINT32 m_TsgTimeSliceUs;
    PEConfigureFile::PARTITION_MODE m_PartitionMode;
};
#endif
