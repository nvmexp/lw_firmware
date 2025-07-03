/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/tests/lwdastst.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/lwca/tools/instrpulse.h"

class LwdaInstrPulse : public LwdaStreamTest
{
public:
    LwdaInstrPulse()
    {
        SetName("LwdaInstrPulse");
    }

    bool IsSupported() override;
    void PrintJsProperties(Tee::Priority pri) override;
    RC Setup() override;
    RC Run() override;
    RC Cleanup() override;

    // JS Property accessors
    SETGET_PROP(RuntimeMs, UINT32);
    SETGET_PROP(KeepRunning, bool);
    SETGET_PROP(RunClks,   UINT64);
    SETGET_PROP(PulseClks, UINT64);
    SETGET_PROP(DelayClks, UINT64);

private:
    Lwca::Module m_Module;
    Lwca::HostMemory m_SmidMap;
    Lwca::DeviceMemory m_GpcLocks;
    InstrPulseParams m_Params = {};

    // JS properties
    UINT32 m_RuntimeMs = 5000;
    bool   m_KeepRunning = false;
    UINT64 m_RunClks   = 100'000'000;
    UINT64 m_PulseClks = 0;
    UINT64 m_DelayClks = 1000;
};
JS_CLASS_INHERIT(LwdaInstrPulse, LwdaStreamTest, "Pulsed Instruction Slowdown Test");
CLASS_PROP_READWRITE(LwdaInstrPulse, RuntimeMs, UINT32,
                     "Maximum amount of time the test can run for");
CLASS_PROP_READWRITE(LwdaInstrPulse, KeepRunning, bool,
                     "While this is true, Run will continue even beyond RuntimeMs");
CLASS_PROP_READWRITE(LwdaInstrPulse, RunClks, UINT64,
                     "Duration in clocks of each kernel run");
CLASS_PROP_READWRITE(LwdaInstrPulse, PulseClks, UINT64,
                     "Duration in clocks in which to issue the slowdown instruction");
CLASS_PROP_READWRITE(LwdaInstrPulse, DelayClks, UINT64,
                     "Delay in clocks between the pulses of slowdown instructions");

bool LwdaInstrPulse::IsSupported()
{
    if (!LwdaStreamTest::IsSupported())
        return false;

    // Quick-slowdown feature is not supported pre-Volta or on CheetAh
    const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();
    if (cap < Lwca::Capability::SM_70 || GetBoundGpuSubdevice()->IsSOC())
    {
        return false;
    }

    // TODO add checks for whether slowdown is enabled on the board

    return true;
}

void LwdaInstrPulse::PrintJsProperties(Tee::Priority pri)
{
    LwdaStreamTest::PrintJsProperties(pri);
    Printf(pri, "\tRuntimeMs:                      %u\n", m_RuntimeMs);
    Printf(pri, "\tKeepRunning:                    %s\n", m_KeepRunning?"true":"false");
    Printf(pri, "\tRunClks:                        %llu\n", m_RunClks);
    Printf(pri, "\tPulseClks:                      %llu\n", m_PulseClks);
    Printf(pri, "\tDelayClks:                      %llu\n", m_DelayClks);
}

RC LwdaInstrPulse::Setup()
{
    RC rc;
    CHECK_RC(LwdaStreamTest::Setup());

    const auto smCount = GetBoundLwdaDevice().GetShaderCount();
    const auto maxGpcCount = GetBoundGpuSubdevice()->GetMaxGpcCount();

    // Initialize LWCA module and memory
    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("instrpulse", &m_Module));
    CHECK_RC(GetLwdaInstance().AllocHostMem(smCount * sizeof(UINT32), &m_SmidMap));
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(maxGpcCount * sizeof(UINT32), &m_GpcLocks));

    // Fill in smid -> GPC map for use by kernel
    UINT32* pSmidMap = static_cast<UINT32*>(m_SmidMap.GetPtr());
    for (UINT32 smid = 0; smid < smCount; smid++)
    {
        UINT32 hwGpcNum = -1;
        UINT32 hwTpcNum = -1;
        CHECK_RC(GetBoundGpuSubdevice()->SmidToHwGpcHwTpc(smid, &hwGpcNum, &hwTpcNum));
        pSmidMap[smid] = hwGpcNum;
    }

    // Init per-GPC locks
    CHECK_RC(m_GpcLocks.Fill(LOCK_INIT));

    // Launch kernel to acquire locks
    Lwca::Function initFunc = m_Module.GetFunction("AcquireLocks", smCount, 1);
    CHECK_RC(initFunc.InitCheck());
    m_Params =
    {
        m_SmidMap.GetDevicePtr(GetBoundLwdaDevice()),
        m_GpcLocks.GetDevicePtr(),
        m_RunClks,
        m_PulseClks,
        m_DelayClks
    };
    CHECK_RC(initFunc.Launch(m_Params));

    // Check that all the locks were properly acquired
    const auto gpcCount = GetBoundGpuSubdevice()->GetGpcCount();
    Lwca::HostMemory hostGpcLocks;
    CHECK_RC(GetLwdaInstance().AllocHostMem(maxGpcCount * sizeof(UINT32), &hostGpcLocks));
    CHECK_RC(m_GpcLocks.Get(hostGpcLocks.GetPtr(), hostGpcLocks.GetSize()));
    CHECK_RC(GetLwdaInstance().Synchronize());

    // Check that the lock was acquired for each active GPC
    // Since we are iterating over HW GPCs some locks might not have been acquired
    UINT32* pGpcLocks = static_cast<UINT32*>(hostGpcLocks.GetPtr());
    UINT32 acquired = 0;
    for (UINT32 i = 0; i < maxGpcCount; i++)
    {
        if (pGpcLocks[i] != LOCK_INIT)
        {
            MASSERT(pSmidMap[pGpcLocks[i]] == i);
            acquired++;
        }
    }
    if (acquired != gpcCount)
    {
        Printf(Tee::PriError,
               "%u GPCs but only %u GPC-locks acquired!\n"
               "Make sure there are no other LWCA kernels running lwring LwdaInstrPulse setup\n",
               gpcCount,
               acquired);
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC LwdaInstrPulse::Run()
{
    RC rc;
    Tasker::DetachThread detach;

    // Init function
    Lwca::Function runFunc =
        m_Module.GetFunction("InstructionPulse", GetBoundLwdaDevice().GetShaderCount(), 1);
    CHECK_RC(runFunc.InitCheck());

    // Launch kernels until the timer expires (or as long as KeepRunning is set)
    // Sync after each launch so that CTRL-C is responsive
    UINT64 startTimeMs = Xp::GetWallTimeMS();
    while (Xp::GetWallTimeMS() - startTimeMs < m_RuntimeMs || m_KeepRunning)
    {
        CHECK_RC(runFunc.Launch(m_Params));
        CHECK_RC(GetLwdaInstance().Synchronize());
    }

    return rc;
}

RC LwdaInstrPulse::Cleanup()
{
    m_Module.Unload();
    m_SmidMap.Free();
    m_GpcLocks.Free();

    return LwdaStreamTest::Cleanup();
}
