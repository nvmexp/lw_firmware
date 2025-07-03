/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/platform.h"
#include "gpu/tests/gputest.h"
#include "gpu/vmiop/vmiopelwmgr.h"

class SriovSync : public GpuTest
{

public:
    SriovSync();

    bool IsSupported() override
    {
        if (!Platform::IsPhysFunMode() && !Platform::IsVirtFunMode())
        {
            return false;
        }
        return true;
    }
    bool IsSupportedVf() override { return true; }
    // Skip overriding Setup() and Cleanup() since there is nothing to be done in them

    RC Run() override;

    void PrintJsProperties(Tee::Priority pri) override;

    // JS property accessors
    SETGET_PROP(TimeoutMs, UINT32);
    SETGET_PROP(SyncPf, bool);
    // KeepRunning testarg is just added as a hack so that this test can
    // be run as a foreground test for test 145 (and variants) without any
    // modifications to those test. KeepRunning is not being used anywhere
    // in this test. If KeepRunning arg is present, test 145 exits as soon
    // as foreground test ends and that is the expected behavior in this case.
    SETGET_PROP(KeepRunning, bool);

private:
    // JS properties
    UINT32 m_TimeoutMs = 0;
    bool m_SyncPf = false;
    bool m_KeepRunning = false;
};

SriovSync::SriovSync()
{
    SetName("SriovSync");
}

RC SriovSync::Run()
{
    RC rc;

    VmiopElwManager *pMgr = VmiopElwManager::GetInstance();
    if (!pMgr)
    {
        Printf(Tee::PriError, "Unable to retrieve Vmiop manager\n");
        MASSERT(pMgr);
        return RC::DEVICE_NOT_FOUND;
    }

    if (Platform::IsPhysFunMode())
    {
        // Keep polling until all VFs are waiting. Calling this test as a 
        // foreground or background test (with either of these parameters)
        // ensures that the test keeps running on PF mods while VF mods is
        // running it's own spec and doesn't complete the spec or calls this test.
        CHECK_RC(Tasker::PollHw(m_TimeoutMs ? m_TimeoutMs : Tasker::NO_TIMEOUT,
                           [&] { return (pMgr->AreAllVfsWaiting() == true); },
                           __FUNCTION__));
        if (m_SyncPf)
        {
            // Release all VFs. If m_SyncPf is set to false, all VFs will
            // keep waiting until PF mods calls this test again with
            // m_SyncPf = true  or until PF mods completes running it's
            // test list. This ensures that all the VFs keep waiting
            // until PF mods runs it own test list and is ready to let
            // VF mods continue
            const bool handlerTerminated = false;
            const bool pfTestRunComplete = false;
            CHECK_RC(pMgr->SyncPfVfs(m_TimeoutMs,
                                     m_SyncPf,
                                     handlerTerminated,
                                     pfTestRunComplete));
        }
    }
    else
    {
        UINT32 gfid = pMgr->GetGfidAttachedToProcess();
        VmiopElw* pVmiopElw = pMgr->GetVmiopElw(gfid);
        if (!pVmiopElw)
        {
            Printf(Tee::PriError, "Vmiop elw for gfid %u not found\n", gfid);
            MASSERT(pVmiopElw);
            return RC::DEVICE_NOT_FOUND;
        }

        CHECK_RC(pVmiopElw->CallPluginSyncPfVfs(m_TimeoutMs, m_SyncPf));
    }

    return rc;
}

void SriovSync::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);
    Printf(pri, "SriovSync Js Properties:\n");
    Printf(pri, "\tTimeoutMs             : %u\n", m_TimeoutMs);
    Printf(pri, "\tSyncPf                : %s\n", m_SyncPf ? "true" : "false");
    Printf(pri, "\tKeepRunning           : %s\n", m_KeepRunning ? "true" : "false");
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

JS_CLASS_INHERIT(SriovSync, GpuTest,
                 "Test used to sync PF and all VF's");

CLASS_PROP_READWRITE(SriovSync, TimeoutMs, UINT32,
        "UINT32: Wait time in ms for the VF's to sync, 0 -> no timeout. Default: No timeout");

CLASS_PROP_READWRITE(SriovSync, SyncPf, bool,
        "bool: Sync PF with all VFs. Default: false, only VFs will be synced");

CLASS_PROP_READWRITE(SriovSync, KeepRunning, bool,
        "bool: Keep running flag to indicate test completion. Default: false");
