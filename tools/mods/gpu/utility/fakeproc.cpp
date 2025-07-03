/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "fakeproc.h"
#include "core/include/tasker.h"

namespace RMFakeProcess
{

static bool s_bFakeProcessIdTlsAlloc = false;
static UINT32 s_fakeProcessIdTls = 0;

static void _allocTls()
{
    static volatile UINT32 spinlockval = 0;

    // Allocating the TLS index and setting s_bFakeProcessIdTlsAlloc are not atomic
    Tasker::SpinLock spinlock(&spinlockval);

    //
    // Did someone just allocate the index?  Check here to avoid acquiring the
    // spinlock every time we check s_bFakeProcessIdTlsAlloc.
    //
    if (s_bFakeProcessIdTlsAlloc)
        return;

    s_fakeProcessIdTls = Tasker::TlsAlloc(true);
    s_bFakeProcessIdTlsAlloc = true;
}

UINT32 GetFakeProcessID()
{
    if (!s_bFakeProcessIdTlsAlloc)
        _allocTls();

    return (UINT32) (uintptr_t) Tasker::TlsGetValue(s_fakeProcessIdTls);
}

void SetFakeProcessID(UINT32 pid)
{
    if (!s_bFakeProcessIdTlsAlloc)
        _allocTls();

    Tasker::TlsSetValue(s_fakeProcessIdTls, (void*)(uintptr_t)pid);
}

AsFakeProcess::AsFakeProcess(UINT32 pid)
{
    m_oldPid = GetFakeProcessID();
    SetFakeProcessID(pid);
}

AsFakeProcess::~AsFakeProcess()
{
    SetFakeProcessID(m_oldPid);
}

} // namespace Process
