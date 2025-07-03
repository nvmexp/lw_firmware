/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/tests/gputest.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gpudev.h"
#include "gpu/perf/avfssub.h"
#include <map>

class QuickSlowdown : public GpuTest
{
public:
    QuickSlowdown()
    {
        SetName("QuickSlowdown");
    }

    bool IsSupported() override;
    void PrintJsProperties(Tee::Priority pri) override;
    RC Setup() override;
    RC Run() override;

    RC Cleanup() override;

    // JS Property accessors
    SETGET_PROP(GpcTestMask, UINT32);
    SETGET_PROP(RuntimeMs, UINT32);
    SETGET_PROP(KeepRunning, bool);
    SETGET_PROP(PulseUs, UINT64);
    SETGET_PROP(DelayUs, UINT64);
    SETGET_PROP(UseSWQuickSlowdown, bool);
    SETGET_PROP(VfLwrveIdx, UINT08);

private:
    vector<UINT32> m_TestGpcs;
    map<UINT32, UINT32> m_InitRegs;

    // JS properties
    UINT32 m_GpcTestMask = ~0u;
    UINT32 m_RuntimeMs = 5000;
    bool   m_KeepRunning = false;
    UINT64 m_PulseUs = 0;
    UINT64 m_DelayUs = 0;
    bool   m_UseSWQuickSlowdown = false;
    UINT08 m_VfLwrveIdx = LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_SEC_0;
};
JS_CLASS_INHERIT(QuickSlowdown, GpuTest, "Pulsed slowdown of quick_slowdown module (Volta-939)");
CLASS_PROP_READWRITE(QuickSlowdown, GpcTestMask, UINT32,
    "Mask of GPCs on which to toggle the slowdown mode");
CLASS_PROP_READWRITE(QuickSlowdown, RuntimeMs, UINT32,
    "Maximum amount of time the test can run for");
CLASS_PROP_READWRITE(QuickSlowdown, KeepRunning, bool,
    "While this is true, Run will continue even beyond RuntimeMs");
CLASS_PROP_READWRITE(QuickSlowdown, PulseUs, UINT64,
    "Duration in microseconds in which to force slowdown");
CLASS_PROP_READWRITE(QuickSlowdown, DelayUs, UINT64,
    "Delay in microseconds between the pulses of slowdown");
CLASS_PROP_READWRITE(QuickSlowdown, UseSWQuickSlowdown, bool,
"Request Software Quick Slowdown mode");
CLASS_PROP_READWRITE(QuickSlowdown, VfLwrveIdx, UINT08,
"Request Software Quick Slowdown to this VF lwrve");


RC QuickSlowdown::Cleanup()
{
    RC rc;
    RegHal &regs = GetBoundGpuSubdevice()->Regs();
    // Restore settings on test exit
    if (!m_UseSWQuickSlowdown)
    {
        for (const auto& init : m_InitRegs)
        {
            CHECK_RC(regs.Write32Priv(MODS_PTRIM_GPC_AVFS_QUICK_SLOWDOWN_CTRL, init.first, init.second));
        }
    };
    return rc;
}

bool QuickSlowdown::IsSupported()
{
    const RegHal &regs = GetBoundGpuSubdevice()->Regs();
    if (!GpuTest::IsSupported())
    {
        return false;
    }

    if (GetBoundGpuDevice()->GetFamily() < GpuDevice::Turing)
    {
        Printf(Tee::PriLow, "quick_slowdown module is only supported on Turing+\n");
        return false;
    }

    FloorsweepImpl* pFs = GetBoundGpuSubdevice()->GetFsImpl();
    for (UINT32 i = 0; i < GetBoundGpuSubdevice()->GetMaxGpcCount(); i++)
    {
        // If a valid GPC doesn't have slowdown enabled, return unsupported
        if (((1u << i) & pFs->GpcMask()) &&
            (!regs.Test32(MODS_PTRIM_GPC_AVFS_QUICK_SLOWDOWN_CTRL_ENABLE_YES, i)))
        {
            Printf(Tee::PriLow, "quick_slowdown module is not enabled on phys GPC %d!\n", i);
            return false;
        }
    }

    return true;
}

void QuickSlowdown::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);
    Printf(pri, "\tGpcTestMask:                    0x%x\n", m_GpcTestMask);
    Printf(pri, "\tRuntimeMs:                      %u\n", m_RuntimeMs);
    Printf(pri, "\tKeepRunning:                    %s\n", m_KeepRunning? "true" : "false");
    Printf(pri, "\tPulseUs:                        %llu\n", m_PulseUs);
    Printf(pri, "\tDelayUs:                        %llu\n", m_DelayUs);
    Printf(pri, "\tUseSWQuickSlowdown:             %s\n", m_UseSWQuickSlowdown? "true": "false");
    Printf(pri, "\tVfLwrveIdx:                     %u\n", m_VfLwrveIdx);
}

RC QuickSlowdown::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());

#ifndef USE_NEW_TASKER
    Printf(Tee::PriWarn,
           "QuickSlowdown performs poorly on SIM builds and other builds lacking the new Tasker.\n"
           "Consider using an MFG build of MODS.\n");
#endif

    FloorsweepImpl* pFs = GetBoundGpuSubdevice()->GetFsImpl();
    for (UINT32 i = 0; i < GetBoundGpuSubdevice()->GetMaxGpcCount(); i++)
    {
        // Only test GPCs that are (a) valid and (b) in the testing mask
        if ((1u << i) & pFs->GpcMask() & m_GpcTestMask)
        {
            m_TestGpcs.push_back(i);
        }
    }
    return rc;
}

RC QuickSlowdown::Run()
{
    RC rc;
    Tasker::DetachThread detach;
    RegHal &regs = GetBoundGpuSubdevice()->Regs();
    Avfs *pAvfs = GetBoundGpuSubdevice()->GetAvfs();
    UINT32 ctrlReg = 0x0;

    // When Quick Slowdown Mode is used don't try to enable the override via register writes
    // Quick slowdown mode will be enabled by RM after it initialized the NAFLL LUTs Bug: 3394247
    if (!m_UseSWQuickSlowdown)
    {
        // Save quick_slowdown CTRL register settings
        for (const auto gpc : m_TestGpcs)
        {
            m_InitRegs[gpc] = regs.Read32(MODS_PTRIM_GPC_AVFS_QUICK_SLOWDOWN_CTRL, gpc);
        }

        // Enable override and disable slowdown
        regs.SetField(&ctrlReg, MODS_PTRIM_GPC_AVFS_QUICK_SLOWDOWN_CTRL_ENABLE_YES);
        regs.SetField(&ctrlReg, MODS_PTRIM_GPC_AVFS_QUICK_SLOWDOWN_CTRL_REQ_OVR_VAL_NO);
        regs.SetField(&ctrlReg, MODS_PTRIM_GPC_AVFS_QUICK_SLOWDOWN_CTRL_REQ_OVR_YES);
        for (const auto gpc : m_TestGpcs)
        {
            CHECK_RC(regs.Write32Priv(MODS_PTRIM_GPC_AVFS_QUICK_SLOWDOWN_CTRL, gpc, ctrlReg));
        }
    }

    // Run until the timer limit is reached or while KeepRunning is true
    const UINT64 startTimeMs = Xp::GetWallTimeMS();
    while (Xp::GetWallTimeMS() - startTimeMs < m_RuntimeMs || m_KeepRunning)
    {
        // If both PulseUs and DelayUs are 0 toggle as fast as possible
        if (m_PulseUs == 0 && m_DelayUs == 0)
        {
            for (UINT32 i = 0; i < 1000; i++)
            {
                // Toggle each GPC one at a time to minimize the time slowdown mode is engaged
                // Also use SetField to avoid the implicit reg read of using Write32 on a field
                if (m_UseSWQuickSlowdown)
                {
                    CHECK_RC(pAvfs->SetQuickSlowdown(m_VfLwrveIdx, m_GpcTestMask, 1));

                    CHECK_RC(pAvfs->SetQuickSlowdown(m_VfLwrveIdx, m_GpcTestMask, 0));
                }
                else
                {
                    for (const auto gpc : m_TestGpcs)
                    {
                        // Force enable quick_slowdown
                        regs.SetField(&ctrlReg,
                                    MODS_PTRIM_GPC_AVFS_QUICK_SLOWDOWN_CTRL_REQ_OVR_VAL_YES);
                        CHECK_RC(regs.Write32Priv(MODS_PTRIM_GPC_AVFS_QUICK_SLOWDOWN_CTRL, gpc, ctrlReg));
                        // Force disable quick_slowdown
                        regs.SetField(&ctrlReg,
                                    MODS_PTRIM_GPC_AVFS_QUICK_SLOWDOWN_CTRL_REQ_OVR_VAL_NO);
                        CHECK_RC(regs.Write32Priv(MODS_PTRIM_GPC_AVFS_QUICK_SLOWDOWN_CTRL, gpc, ctrlReg));
                    }
                }
                #ifndef USE_NEW_TASKER
                    Tasker::Yield();
                #endif
            }
        }
        // Otherwise use OS timer to configure the pulses
        else
        {
            if (m_UseSWQuickSlowdown)
            {
                // Request SW to enable the instruction-slowdown mode for a duration of 'PulseUs'
                CHECK_RC(pAvfs->SetQuickSlowdown(m_VfLwrveIdx, m_GpcTestMask, 1));
                const UINT64 pulseStart = Xp::GetWallTimeUS();
                while (Xp::GetWallTimeUS() - pulseStart < m_PulseUs);

                // Request SW to disable the instruction-slowdown mode for a duration of 'DelayUs'
                CHECK_RC(pAvfs->SetQuickSlowdown(m_VfLwrveIdx, m_GpcTestMask, 0));
                const UINT64 delayStart = Xp::GetWallTimeUS();
                while (Xp::GetWallTimeUS() - delayStart < m_DelayUs);
            }
            else
            {
                regs.SetField(&ctrlReg, MODS_PTRIM_GPC_AVFS_QUICK_SLOWDOWN_CTRL_REQ_OVR_VAL_YES);
                for (const auto gpc : m_TestGpcs)
                {
                    CHECK_RC(regs.Write32Priv(MODS_PTRIM_GPC_AVFS_QUICK_SLOWDOWN_CTRL, gpc, ctrlReg));
                }
                const UINT64 pulseStart = Xp::GetWallTimeUS();
                while (Xp::GetWallTimeUS() - pulseStart < m_PulseUs);

                // Force disable the instruction-slowdown mode for a duration of 'DelayUs'
                regs.SetField(&ctrlReg, MODS_PTRIM_GPC_AVFS_QUICK_SLOWDOWN_CTRL_REQ_OVR_VAL_NO);
                for (const auto gpc : m_TestGpcs)
                {
                    CHECK_RC(regs.Write32Priv(MODS_PTRIM_GPC_AVFS_QUICK_SLOWDOWN_CTRL, gpc, ctrlReg));
                }
                const UINT64 delayStart = Xp::GetWallTimeUS();
                while (Xp::GetWallTimeUS() - delayStart < m_DelayUs);
            }
#ifndef USE_NEW_TASKER
            Tasker::Yield();
#endif
        }
    }
    return rc;
}