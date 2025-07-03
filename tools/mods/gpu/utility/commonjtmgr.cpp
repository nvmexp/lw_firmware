/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "commonjtmgr.h"

#include "acpigenfuncs.h"
#include "core/include/mgrmgr.h"
#include "device/interface/pcie.h"
#include "gpu/include/gpumgr.h"

//------------------------------------------------------------------------------
CommonJTMgr::~CommonJTMgr()
{
    m_SmbEc.Cleanup();
}

//------------------------------------------------------------------------------
// Construct a singleton for this GpuInstance
CommonJTMgr::CommonJTMgr(UINT32 GpuInstance) : m_GpuInstance(GpuInstance)
{
}

void CommonJTMgr::Initialize()
{
    GpuDevMgr* const pGpuDevMgr =
        static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);

    if (pGpuDevMgr)
    {
        m_pSubdev =
            pGpuDevMgr->GetSubdeviceByGpuInst(m_GpuInstance);
        if (m_pSubdev)
        {
            auto pGpuPcie = m_pSubdev->GetInterface<Pcie>();
            m_Domain = pGpuPcie->DomainNumber();
            m_Bus = pGpuPcie->BusNumber();
            m_Device = pGpuPcie->DeviceNumber();
            m_Function = pGpuPcie->FunctionNumber();
        }
    }
    // Determine if ACPI is supported
    if (DiscoverACPI() != OK)
    {
        // Determine if SMBUS is supported
        DiscoverSMBus();
        DiscoverNativeACPI();
    }

    string acpiType;
    if (m_JTACPISupport)
    {
        acpiType += "JT";
    }
    else if (m_NativeACPISupport)
    {
        acpiType += "Native";
    }

    if (!acpiType.empty())
    {
        Printf(Tee::PriNormal, "Low Power ACPI : %s\n", acpiType.c_str());
    }
}

//------------------------------------------------------------------------------
// Get the current JT capabilities
UINT32 CommonJTMgr::GetCapabilities()
{
    return m_Caps;
}

//------------------------------------------------------------------------------
// ACPI Wrapper for JTMgr
RC CommonJTMgr::DSM_Eval
(
    UINT16 domain,
    UINT16 bus,
    UINT08 dev,
    UINT08 func,
    UINT32 acpiFunc,
    UINT32 acpiSubFunc,
    UINT32 *pInOut,
    UINT16 *pSize
)
{
    MASSERT(acpiFunc == MODS_DRV_CALL_ACPI_GENERIC_DSM_JT);
    if (m_JTACPISupport)
    {
        return DSM_Eval_ACPI(
                    domain,bus,dev,func,
                    acpiFunc,
                    acpiSubFunc,
                    pInOut,
                    pSize);
    }

    // Emulate the ACPI calls that can be supported by the SmbEC
    if (m_SMBusSupport)
    {
        RC rc;
        switch(acpiSubFunc)
        {
            case 0: //JT_FUNC_SUPPORT
                *pInOut =   (1 << 0) | // JT_FUNC_SUPPORT
                            (1<<JT_FUNC_CAPS) |
                            (1 << JT_FUNC_POWERCONTROL);
                *pSize = sizeof(UINT32);
                break;

            case JT_FUNC_CAPS:          // Capabilities
                *pInOut = m_Caps;
                *pSize = sizeof(m_Caps);
                break;

            case JT_FUNC_POWERCONTROL:  // dGPU Power Control
                *pSize = sizeof(UINT32);
                rc = SMBusGC6PowerControl(pInOut);
                break;

            case JT_FUNC_POLICYSELECT:  // Query Policy Selector Status (reserved for future use)
            case JT_FUNC_PLATPOLICY:    // Query the Platform Policies (reserved for future use)
            case JT_FUNC_DISPLAYSTATUS: // Query the Display Hot-Key
            case JT_FUNC_MDTL:          // Display Hot-Key Toggle List
            default:
                *pInOut = LW_ACPI_DSM_ERROR_UNSUPPORTED;
                *pSize = sizeof(UINT32);
                rc = RC::UNSUPPORTED_FUNCTION;
                break;
        }
        return rc;
    }
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// Override this when ACPI support is present
RC CommonJTMgr::DiscoverACPI()
{
    m_JTACPISupport = false;
    return RC::UNSUPPORTED_FUNCTION;
}

RC CommonJTMgr::DiscoverNativeACPI()
{
    m_NativeACPISupport = false;
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// Determine if SMBUS is supported
RC CommonJTMgr::DiscoverSMBus()
{
    RC rc;
    CHECK_RC(m_SmbEc.InitializeSmbusDevice(m_pSubdev));
    m_Caps = m_SmbEc.GetCapabilities();
    m_SMBusSupport = true;
    return rc;
}

//------------------------------------------------------------------------------
// Get the GC6 entry/exit stats.
void CommonJTMgr::GetCycleStats(Xp::JTMgr::CycleStats * pStats)
{
    if (m_SMBusSupport)
    {
        m_SmbEc.GetCycleStats(pStats);
    }
    else if (m_JTACPISupport || m_NativeACPISupport)
    {
        *pStats = m_ACPIStats;
    }
}

//------------------------------------------------------------------------------
// For ACPI implementation this function is not supported.
RC CommonJTMgr::GetLwrrentFsmMode(UINT08 *pFsmMode)
{
    if (m_SMBusSupport)
    {
        return m_SmbEc.GetLwrrentFsmMode(pFsmMode);
    }
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// For ACPI implementation this function is not supported.
RC CommonJTMgr::SetLwrrentFsmMode(UINT08 fsmMode)
{
    if (m_SMBusSupport)
    {
        return m_SmbEc.SetLwrrentFsmMode(fsmMode);
    }
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// For ACPI implementation this function is not supported.
RC CommonJTMgr::SetWakeupEvent(UINT32 eventId)
{
    if (m_JTACPISupport)
    {
        m_ACPIStats.exitWakeupEvent = eventId;
        return OK;
    }
    else if (m_SMBusSupport)
    {
        m_SmbEc.SetWakeupEvent(eventId);
        return OK;
    }
    else
        return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// For ACPI implementation this function is not supported.
RC CommonJTMgr::AssertHpd(bool bAssert)
{
    if (m_SMBusSupport)
    {
        return m_SmbEc.AssertHpd(bAssert);
    }
    else
        return RC::UNSUPPORTED_FUNCTION;

}
//------------------------------------------------------------------------------
// Get/Set GC6 entry/exit debug options
// For ACPI implementation this function is not supported.
void CommonJTMgr::SetDebugOptions(Xp::JTMgr::DebugOpts * pOpts)
{
    if (m_SMBusSupport)
    {
        m_SmbEc.SetDebugOptions(pOpts);
    }
}

RC CommonJTMgr::GetDebugOptions(Xp::JTMgr::DebugOpts * pOpts)
{
    return m_SMBusSupport ? m_SmbEc.GetDebugOptions(pOpts) : RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// Execute one of the GC6 power control functions.
// pInOut - determines what specific control to execute.
RC CommonJTMgr::GC6PowerControl(UINT32 *pInOut)
{
    MASSERT(pInOut);

    if (m_JTACPISupport)
    {
        return ACPIGC6PowerControl(pInOut);
    }
    else if(m_SMBusSupport)
    {
        return SMBusGC6PowerControl(pInOut);
    }
    else
        return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC CommonJTMgr::SMBusGC6PowerControl(UINT32 *pInOut)
{
    RC      rc;
    if (!m_SMBusSupport)
    {
        *pInOut = LW_JT_FUNC_POWERCONTROL_GPU_GC_STATE_TRANSITION;
        return RC::UNSUPPORTED_FUNCTION;
    }

    switch (DRF_VAL(_JT_FUNC, _POWERCONTROL,_GPU_POWER_CONTROL,*pInOut))
    {
        case LW_JT_FUNC_POWERCONTROL_GPU_POWER_CONTROL_GSS:
            rc = m_SmbEc.GetPowerStatus(pInOut);
            break;
        case LW_JT_FUNC_POWERCONTROL_GPU_POWER_CONTROL_EGNS:
        case LW_JT_FUNC_POWERCONTROL_GPU_POWER_CONTROL_EGIS:
            rc = m_SmbEc.EnterGC6Power(pInOut);
            break;

        case LW_JT_FUNC_POWERCONTROL_GPU_POWER_CONTROL_XGXS:
        case LW_JT_FUNC_POWERCONTROL_GPU_POWER_CONTROL_XGIS:
            rc = m_SmbEc.ExitGC6Power(pInOut);
            break;
        default:
            rc = RC::LWRM_ILWALID_COMMAND;
            break;
    }
    return rc;
}

//------------------------------------------------------------------------------
// Override this when ACPI support is present
RC CommonJTMgr::DSM_Eval_ACPI
(
    UINT16 domain,
    UINT16 bus,
    UINT08 dev,
    UINT08 func,
    UINT32 acpiFunc,
    UINT32 acpiSubFunc,
    UINT32 *pInOut,
    UINT16 *pSize
)
{
    return RC::SOFTWARE_ERROR;
}

//------------------------------------------------------------------------------
RC CommonJTMgr::ACPIGC6PowerControl(UINT32 *pInOut)
{
    RC rc;
    LwU16   rtnSize = sizeof(UINT32);
    UINT32 acpiDsmSubFunction = JT_FUNC_POWERCONTROL;
    UINT64 start=0, end=0, freq = Xp::QueryPerformanceFrequency();

    start = Xp::QueryPerformanceCounter();

    // Note may need to check the rtnSize value for non-zero to confirm the
    // call was actually handled.
    rc = Xp::CallACPIGeneric(m_GpuInstance,
                          MODS_DRV_CALL_ACPI_GENERIC_DSM_JT,
                          (void*)(LwUPtr)acpiDsmSubFunction,
                          pInOut,
                          &rtnSize);

    end = Xp::QueryPerformanceCounter();
    // Try to collect some timming stats
    switch (DRF_VAL(_JT_FUNC, _POWERCONTROL,_GPU_POWER_CONTROL,*pInOut))
    {
        case LW_JT_FUNC_POWERCONTROL_GPU_POWER_CONTROL_EGNS:
        case LW_JT_FUNC_POWERCONTROL_GPU_POWER_CONTROL_EGIS:
            m_ACPIStats.entryTimeMs = ((end-start)*1000)/freq;
            m_ACPIStats.entryStatus = rc;
            m_ACPIStats.entryPwrStatus = 0;
            break;

        case LW_JT_FUNC_POWERCONTROL_GPU_POWER_CONTROL_XGXS:
        case LW_JT_FUNC_POWERCONTROL_GPU_POWER_CONTROL_XGIS:
            m_ACPIStats.exitTimeMs = ((end-start)*1000)/freq;
            m_ACPIStats.exitStatus = rc;
            m_ACPIStats.exitPwrStatus = 0;
            break;
        case LW_JT_FUNC_POWERCONTROL_GPU_POWER_CONTROL_GSS:
        default:
            break;
    }
    return rc;
}

RC CommonJTMgr::RTD3PowerCycle(Xp::PowerState state)
{
    return RC::UNSUPPORTED_FUNCTION;
}
