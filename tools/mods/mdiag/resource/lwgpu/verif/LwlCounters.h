/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_LWLCOUNTERS_H
#define INCLUDED_LWLCOUNTERS_H

#include "GpuVerif.h"

// LWLink counters for traffic check.
class LwlCounters : public IVerifChecker
{
public:
    LwlCounters();

    virtual TestEnums::TEST_STATUS Check(ICheckInfo* info);
    virtual void Setup(GpuVerif* verifier);

private:
    // Probe links and environment.
    RC Probe(const ArgReader *params);
    RC Init();
    void Cleanup();
    // Alloc sub dev obj & reserve counters.
    RC Reserve();
    // Config counters.
    RC Config();
    RC GetCounters(bool bCheck) const;

    LWGpuResource* m_pLwgpu;
    LwRm::Handle m_SubDevObj;
    UINT32 m_LinkMask;
    UINT32 m_CheckedLinkMask;
    // Should be counters check enabled?
    bool m_bEnabled;
    LwRm* m_pLwRm;
};

#endif /* INCLUDED_LWLCOUNTERS_H */

