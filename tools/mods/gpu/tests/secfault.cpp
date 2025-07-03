/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.
 * All rights reserved. All information contained herein is proprietary and
 * confidential to LWPU Corporation. Any use, reproduction, or disclosure
 * without the written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/tests/gputest.h"

class CheckSelwrityFaults : public GpuTest
{
public:
    CheckSelwrityFaults();

    bool IsSupported() override;
    RC Setup() override;
    RC Run() override;
    void PrintJsProperties(Tee::Priority pri) override;

    SETGET_PROP(CheckSCPM, bool);
    SETGET_PROP(CheckPOD, bool);
    SETGET_PROP(ClearFaults, bool);

private:
    bool m_CheckSCPM = true;
    bool m_CheckPOD = true;
    bool m_ClearFaults = false;

    struct FaultInfo
    {
        ModsGpuRegField RegField;
        const char* FieldName;
        ModsGpuRegAddress ClearFaultReg;
        ModsGpuRegValue ClearFaultValue;
    };
    const vector<FaultInfo> m_ScpmFaultInfos =
    {
        {
            MODS_FUSE_SELWRITY_MONITOR_SCPM_STATUS,
            "LW_FUSE_SELWRITY_MONITOR_SCPM_STATUS",
            MODS_FUSE_SELWRITY_MONITOR_SCPM_CTRL,
            MODS_FUSE_SELWRITY_MONITOR_SCPM_CTRL_FAULT_OVERRIDE_VALUE_CLEAR
        },
        {
            MODS_CGSP_SCPM_OUT_MACRO_TIMING_STATUS,
            "LW_CGSP_SCPM_OUT_MACRO_TIMING_STATUS",
            MODS_CGSP_SCPM_CTRL,
            MODS_CGSP_SCPM_CTRL_FAULT_OVERRIDE_VALUE_CLEAR
        },
        {
            MODS_CSEC_SCPM_OUT_MACRO_TIMING_STATUS,
            "LW_CSEC_SCPM_OUT_MACRO_TIMING_STATUS",
            MODS_CSEC_SCPM_CTRL,
            MODS_CSEC_SCPM_CTRL_FAULT_OVERRIDE_VALUE_CLEAR
        },
        {
            MODS_PGSP_SCPM_OUT_MACRO_TIMING_STATUS,
            "LW_PGSP_SCPM_OUT_MACRO_TIMING_STATUS",
            MODS_PGSP_SCPM_CTRL,
            MODS_PGSP_SCPM_CTRL_FAULT_OVERRIDE_VALUE_CLEAR
        },
        {
            MODS_PSEC_SCPM_OUT_MACRO_TIMING_STATUS,
            "LW_PSEC_SCPM_OUT_MACRO_TIMING_STATUS",
            MODS_PSEC_SCPM_CTRL,
            MODS_PSEC_SCPM_CTRL_FAULT_OVERRIDE_VALUE_CLEAR
        }
    };
    const vector<FaultInfo> m_PodFaultInfos =
    {
        {
            MODS_FUSE_SELWRITY_MONITOR_POD_STATUS,
            "LW_FUSE_SELWRITY_MONITOR_POD_STATUS",
            MODS_FUSE_SELWRITY_MONITOR_POD_CTRL,
            MODS_FUSE_SELWRITY_MONITOR_POD_CTRL_FAULT_OVERRIDE_VALUE_CLEAR
        }
    };
    RC CheckFault(const FaultInfo& faultInfo);
};

JS_CLASS_INHERIT(CheckSelwrityFaults, GpuTest, "Checks for SCPM and POD faults");

CLASS_PROP_READWRITE(CheckSelwrityFaults, CheckSCPM, bool,
    "Checks for SCPM faults");
CLASS_PROP_READWRITE(CheckSelwrityFaults, CheckPOD, bool,
    "Checks for POD faults");
CLASS_PROP_READWRITE(CheckSelwrityFaults, ClearFaults, bool,
    "Clears any faults (SCPM or POD) after the test detects them");

CheckSelwrityFaults::CheckSelwrityFaults()
{
    SetName("CheckSelwrityFaults");
}

bool CheckSelwrityFaults::IsSupported()
{
    return GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_SCPM_AND_POD);
}

RC CheckSelwrityFaults::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());

    return rc;
}

RC CheckSelwrityFaults::Run()
{
    StickyRC sticky;

    RegHal& regs = GetBoundGpuSubdevice()->Regs();

    if (m_CheckSCPM)
    {
        for (const auto& faultInfo : m_ScpmFaultInfos)
        {
            sticky = CheckFault(faultInfo);
        }
        const Tee::Priority pri = sticky == RC::OK ? GetVerbosePrintPri() : Tee::PriError;
        const vector<pair<ModsGpuRegAddress, const char*>> debugRegs =
        {
            { MODS_FUSE_SELWRITY_MONITOR_SCPM_MON, "LW_FUSE_SELWRITY_MONITOR_SCPM_MON" },
            { MODS_CGSP_SCPM_PATH_TIMING_STATUS, "LW_CGSP_SCPM_PATH_TIMING_STATUS" },
            { MODS_CSEC_SCPM_PATH_TIMING_STATUS, "LW_CSEC_SCPM_PATH_TIMING_STATUS" },
            { MODS_PGSP_SCPM_PATH_TIMING_STATUS, "LW_PGSP_SCPM_PATH_TIMING_STATUS" },
            { MODS_PSEC_SCPM_PATH_TIMING_STATUS, "LW_PSEC_SCPM_PATH_TIMING_STATUS" }
        };
        for (const auto& debugReg : debugRegs)
        {
            if (regs.IsSupported(debugReg.first))
            {
                Printf(pri, "%s=0x%08x\n", debugReg.second, regs.Read32(debugReg.first));
            }
        }
    }
    if (m_CheckPOD)
    {
        for (const auto& faultInfo : m_PodFaultInfos)
        {
            sticky = CheckFault(faultInfo);
        }
    }

    return sticky;
}

RC CheckSelwrityFaults::CheckFault(const FaultInfo& faultInfo)
{
    StickyRC sticky;

    RegHal& regs = GetBoundGpuSubdevice()->Regs();

    if (!regs.IsSupported(faultInfo.RegField))
    {
        VerbosePrintf("Skipping %s\n", faultInfo.FieldName);
        return sticky;
    }
    VerbosePrintf("Checking %s\n", faultInfo.FieldName);
    UINT32 status = 0;
    sticky = regs.Read32Priv(faultInfo.RegField, &status);
    if (status)
    {
        sticky = RC::HW_STATUS_ERROR;
        Printf(Tee::PriError, "%s faulted\n", faultInfo.FieldName);
        if (m_ClearFaults)
        {
            if (regs.HasRWAccess(faultInfo.ClearFaultReg))
            {
                VerbosePrintf("Clearing %s fault\n", faultInfo.FieldName);
                regs.Write32(faultInfo.ClearFaultValue);
            }
            else
            {
                Printf(Tee::PriError, "Cannot clear %s fault\n", faultInfo.FieldName);
            }
        }
    }

    return sticky;
}

void CheckSelwrityFaults::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);
    Printf(pri, "CheckSelwrityFaults Js Properties:\n");
    Printf(pri, "\t%-31s %s\n", "CheckSCPM:", m_CheckSCPM ? "true" : "false");
    Printf(pri, "\t%-31s %s\n", "CheckPOD:", m_CheckPOD ? "true" : "false");
    Printf(pri, "\t%-31s %s\n", "ClearFaults:", m_ClearFaults ? "true" : "false");
}

