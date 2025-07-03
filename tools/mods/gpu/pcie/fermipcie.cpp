/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "fermipcie.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/testdevice.h"
#include "gpu/uphy/uphyreglogger.h"
#include "ctrl/ctrl2080.h"
#include "rm.h"
#include "core/include/lwrm.h"
#include "core/include/massert.h"
#include "core/include/platform.h"
#include "core/include/utility.h"

// for LW_XVE_AER_CAP_HDR
#include "fermi/gf100/dev_lw_xve.h"

//-----------------------------------------------------------------------------
//! \brief Constructor
//!
FermiPcie::FermiPcie()
  : m_UsePolledErrors(false)
  , m_PciInfoLoaded(false)
  , m_ForceCountersToZero(false)
  , m_bRestorePexAer(false)
  , m_bPexAerEnabled(false)
{
    memset(&m_PciInfo, 0, sizeof (m_PciInfo));
}

//-----------------------------------------------------------------------------
//! \brief Shutdown the PCIE implementation (restore any settings, etc.)
//!
//! \return OK if successful, not OK otherwise
RC FermiPcie::DoShutdown()
{
    if (m_bRestorePexAer)
    {
        EnableAer(m_bPexAerEnabled);
        m_bRestorePexAer = false;
    }
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Get the PCIE Subsystem vendor ID
//!
//! \return PCIE Subsystem vendor ID (0 if unable to retrieve)
/* virtual */ UINT32 FermiPcie::DoSubsystemVendorId()
{
    if (!m_PciInfoLoaded)
      LoadPcieInfo();
    if (!m_PciInfoLoaded)
      return 0;

    return m_PciInfo.pciSubSystemId & 0xFFFF;
}

//-----------------------------------------------------------------------------
//! \brief Get the PCIE Subsystem device ID
//!
//! \return PCIE Subsystem device ID (0 if unable to retrieve)
/* virtual */ UINT32 FermiPcie::DoSubsystemDeviceId()
{
    if (!m_PciInfoLoaded)
      LoadPcieInfo();
    if (!m_PciInfoLoaded)
      return 0;

    // The device ID is the upper 16 bits; see lwcm.h
    return m_PciInfo.pciSubSystemId >> 16;
}

//-----------------------------------------------------------------------------
//! \brief Returns the fastest PCIE link speed that the GPU supports
//!
//! \return The fastest PCIE link speed that the GPU supports
/* virtual */ Pci::PcieLinkSpeed FermiPcie::DoLinkSpeedCapability() const
{
    if (!GpuPtr()->IsRmInitCompleted())
    {
        return PcieImpl::DoLinkSpeedCapability();
    }
    // Ask RM to check the VBIOS capability
    LW2080_CTRL_BUS_INFO busInfo = { 0 };
    busInfo.index                = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GEN_INFO;
    busInfo.data                 = 0;
    LW2080_CTRL_BUS_GET_INFO_PARAMS infoParam = {0};
    infoParam.busInfoListSize    = 1;
    infoParam.busInfoList        = LW_PTR_TO_LwP64(&busInfo);

    if (LwRmPtr()->ControlBySubdevice(GetSubdevice(),
                                      LW2080_CTRL_CMD_BUS_GET_INFO,
                                      &infoParam,
                                      sizeof(infoParam)) != OK)
    {
        Printf(Tee::PriNormal, "Rm call failed \n");
        return Pci::SpeedUnknown;
    }

    const UINT32 cap = DRF_VAL(2080_CTRL,_BUS_INFO,_PCIE_LINK_CAP_GPU_GEN,
                               busInfo.data);
    Pci::PcieLinkSpeed supportedSpeed = Pci::SpeedUnknown;
    switch (cap)
    {
        case LW2080_CTRL_BUS_INFO_PCIE_LINK_CAP_GPU_GEN_GEN1:
            supportedSpeed = Pci::Speed2500MBPS;
            break;
        case LW2080_CTRL_BUS_INFO_PCIE_LINK_CAP_GPU_GEN_GEN2:
            supportedSpeed = Pci::Speed5000MBPS;
            break;
        case LW2080_CTRL_BUS_INFO_PCIE_LINK_CAP_GPU_GEN_GEN3:
            supportedSpeed = Pci::Speed8000MBPS;
            break;
        case LW2080_CTRL_BUS_INFO_PCIE_LINK_CAP_GPU_GEN_GEN4:
            supportedSpeed = Pci::Speed16000MBPS;
            break;
        case LW2080_CTRL_BUS_INFO_PCIE_LINK_CAP_GPU_GEN_GEN5:
            supportedSpeed = Pci::Speed32000MBPS;
            break;
    }
    MASSERT(supportedSpeed != Pci::SpeedUnknown);
    return supportedSpeed;
}

//-----------------------------------------------------------------------------
//! \brief Returns the current PCIE link speed of the GPU
//!
//! \return The current PCIE link speed of the GPU
/* virtual */ Pci::PcieLinkSpeed FermiPcie::DoGetLinkSpeed
(
    Pci::PcieLinkSpeed expectedSpeed
)
{
    // If this is not PCI Express return 0
    if(Gpu::PciExpress != GetSubdevice()->BusType())
        return Pci::SpeedUnknown;

    LwRmPtr pLwRm;
    LwRm::Handle hRmSubdev = pLwRm->GetSubdeviceHandle(GetSubdevice());
    MASSERT(hRmSubdev);

    // Bug 849638 requires that RM force the link speed to Gen1 when changing
    // voltage.  Since in some situations this can be done asynchronously with
    // other MODS operations, read the link speed 3 times (with a slight delay
    // and report the highest sampled link speed)
    #define BUG_849638_LINK_SPEED_RETRIES    30
    #define BUG_849638_LINK_SPEED_DELAY_MS   10
    const UINT32 retryCount =
                  GetSubdevice()->HasBug(849638) ? BUG_849638_LINK_SPEED_RETRIES : 1;

    if (expectedSpeed == Pci::SpeedUnknown)
        expectedSpeed = Pci::SpeedMax;

    if (GetSubdevice()->HasBug(849638))
    {
        PexDevice *pUpStreamDev;
        UINT32 ParentPort;
        if ((OK == GetUpStreamInfo(&pUpStreamDev, &ParentPort)) &&
            pUpStreamDev &&
            (pUpStreamDev->GetDownStreamLinkSpeedCap(ParentPort) < expectedSpeed))
        {
            expectedSpeed = pUpStreamDev->GetDownStreamLinkSpeedCap(ParentPort);
        }

        // Attempting to read the current pstate and limit the maxLinkSpeed by
        // the speed at the current pstate will FAIL here.  RM apparently
        // changes pstate while doing adjustments for bug 849638 so reading
        // the expected link speed for the current pstate reads a Gen1 pstate
    }

    LW2080_CTRL_BUS_INFO busInfo = {0};
    busInfo.index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_CTRL_STATUS;

    LW2080_CTRL_BUS_GET_INFO_PARAMS infoParam = {0};
    infoParam.busInfoListSize = 1;
    infoParam.busInfoList = LW_PTR_TO_LwP64(&busInfo);

    Pci::PcieLinkSpeed linkSpeed = Pci::SpeedUnknown;
    for (UINT32 i = 0 ;
          (i < retryCount) && (linkSpeed < expectedSpeed); i++)
    {
        if (i)
        {
            Tasker::Sleep(BUG_849638_LINK_SPEED_DELAY_MS);
        }

        busInfo.data = 0;
        if (pLwRm->Control(hRmSubdev, LW2080_CTRL_CMD_BUS_GET_INFO,
                                &infoParam, sizeof(infoParam)) != OK)
        {
            MASSERT(!"LwRm control failed\n");
            return Pci::SpeedUnknown;
        }
        UINT32 Speed = DRF_VAL(2080_CTRL,_BUS_INFO,
                               _PCIE_LINK_CTRL_STATUS_LINK_SPEED,
                               busInfo.data);

        Pci::PcieLinkSpeed lwrlinkSpeed = Pci::SpeedUnknown;
        switch(Speed)
        {
            case LW2080_CTRL_BUS_INFO_PCIE_LINK_CTRL_STATUS_LINK_SPEED_2500MBPS:
                lwrlinkSpeed = Pci::Speed2500MBPS;
                break;
            case LW2080_CTRL_BUS_INFO_PCIE_LINK_CTRL_STATUS_LINK_SPEED_5000MBPS:
                lwrlinkSpeed = Pci::Speed5000MBPS;
                break;
            case LW2080_CTRL_BUS_INFO_PCIE_LINK_CTRL_STATUS_LINK_SPEED_8000MBPS:
                lwrlinkSpeed = Pci::Speed8000MBPS;
                break;
            case LW2080_CTRL_BUS_INFO_PCIE_LINK_CTRL_STATUS_LINK_SPEED_16000MBPS:
                lwrlinkSpeed = Pci::Speed16000MBPS;
                break;
            case LW2080_CTRL_BUS_INFO_PCIE_LINK_CTRL_STATUS_LINK_SPEED_32000MBPS:
                lwrlinkSpeed = Pci::Speed32000MBPS;
                break;
            default:
                Printf(Tee::PriNormal, "speed unknown\n");
                return Pci::SpeedUnknown;
        };
        if (lwrlinkSpeed > linkSpeed)
            linkSpeed = lwrlinkSpeed;
    }

    return linkSpeed;
}

//-----------------------------------------------------------------------------
//! \brief Set the PCIE link speed of the GPU
//!
//! \param NewSpeed : New PCIE link speed to set
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC FermiPcie::DoSetLinkSpeed(Pci::PcieLinkSpeed NewSpeed)
{
    RC rc;
    LW2080_CTRL_BUS_SET_PCIE_SPEED_PARAMS param = {0};
    switch (NewSpeed)
    {
        case Pci::Speed2500MBPS:
            param.busSpeed = LW2080_CTRL_BUS_SET_PCIE_SPEED_2500MBPS;
            break;
        case Pci::Speed5000MBPS:
            param.busSpeed = LW2080_CTRL_BUS_SET_PCIE_SPEED_5000MBPS;
            break;
        case Pci::Speed8000MBPS:
            param.busSpeed = LW2080_CTRL_BUS_SET_PCIE_SPEED_8000MBPS;
            break;
        case Pci::Speed16000MBPS:
            param.busSpeed = LW2080_CTRL_BUS_SET_PCIE_SPEED_16000MBPS;
            break;
        case Pci::Speed32000MBPS:
            param.busSpeed = LW2080_CTRL_BUS_SET_PCIE_SPEED_32000MBPS;
            break;
        default:
            return RC::BAD_PARAMETER;
    }

    Perf* pPerf = GetSubdevice()->GetPerf();
    if (pPerf->IsPState30Supported() && pPerf->IsArbitrated(Gpu::ClkPexGen))
    {
        Printf(Tee::PriWarn,
               "Forcing PCIE gen speed behind the perf arbiter\n");
    }

    // set the speed of GPU and the link above
    rc = LwRmPtr()->ControlBySubdevice(GetSubdevice(),
                                       LW2080_CTRL_CMD_BUS_SET_PCIE_SPEED,
                                       &param,
                                       sizeof(param));
    if (OK != rc)
    {
        Printf(Tee::PriNormal, "Cannot train Gpu to %dMB/s\n", NewSpeed);
        return RC::PCIE_BUS_ERROR;
    }

    if (UphyRegLogger::GetLogPointMask() & UphyRegLogger::LogPoint::PostLinkTraining)
    {
        TestDevicePtr pTestDev;
        CHECK_RC(GetDevicePtr(&pTestDev));
        CHECK_RC(UphyRegLogger::LogRegs(pTestDev,
                                        UphyRegLogger::UphyInterface::Pcie,
                                        UphyRegLogger::PostLinkTraining,
                                        RC::OK));
    }

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Return whether ASLM is possible on the GPU
//!
//! \return true if ASLM is possible, false if not
/* virtual */ bool FermiPcie::DoIsASLMCapable()
{
    LW2080_CTRL_BUS_INFO busInfo = {0};
    busInfo.index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_ASLM_STATUS;
    busInfo.data  = 0;

    LW2080_CTRL_BUS_GET_INFO_PARAMS infoParam = {0};
    infoParam.busInfoListSize = 1;
    infoParam.busInfoList     = LW_PTR_TO_LwP64(&busInfo);

    if(LwRmPtr()->ControlBySubdevice(GetSubdevice(),
                                     LW2080_CTRL_CMD_BUS_GET_INFO,
                                     &infoParam,
                                     sizeof(infoParam)) != OK)
    {
        Printf(Tee::PriNormal, "Rm call failed. default False.\n");
        return false;
    }

    return DRF_VAL(2080_CTRL,_BUS_INFO,_PCIE_ASLM_STATUS_SUPPORTED,
                   busInfo.data);
}

//-----------------------------------------------------------------------------
//! \brief Returns the widest PCIE link width that the GPU supports
//!
//! \return The widest PCIE link width that the GPU supports
/* virtual */ UINT32 FermiPcie::DoLinkWidthCapability()
{
    // If this is not PCI Express return 0
    if(Gpu::PciExpress != GetSubdevice()->BusType())
        return 0;

    LW2080_CTRL_BUS_INFO busInfo = {0};
    busInfo.index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_CAPS;
    busInfo.data = 0;

    LW2080_CTRL_BUS_GET_INFO_PARAMS infoParam = {0};
    infoParam.busInfoListSize = 1;
    infoParam.busInfoList = LW_PTR_TO_LwP64(&busInfo);

    if(LwRmPtr()->ControlBySubdevice(GetSubdevice(), LW2080_CTRL_CMD_BUS_GET_INFO,
                            &infoParam, sizeof(infoParam)) != OK)
    {
        MASSERT(!"LwRm control failed\n");
        return 0;
    }
    UINT32 Width = DRF_VAL(2080_CTRL, _BUS_INFO, _PCIE_LINK_CAP_MAX_WIDTH,
                      busInfo.data);

    return Width;
}

//-----------------------------------------------------------------------------
//! \brief Returns the current PCIE link width of the GPU
//!
//! \return The current PCIE link width of the GPU
/* virtual */ UINT32 FermiPcie::DoGetLinkWidth()
{
    // If this is not PCI Express return 0
    if(Gpu::PciExpress != GetSubdevice()->BusType())
        return 0;

    LwRmPtr pLwRm;
    LwRm::Handle hRmSubdev = pLwRm->GetSubdeviceHandle(GetSubdevice());
    MASSERT(hRmSubdev);

    LW2080_CTRL_BUS_INFO busInfo = {0};
    busInfo.index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_CTRL_STATUS;
    busInfo.data = 0;

    LW2080_CTRL_BUS_GET_INFO_PARAMS infoParam = {0};
    infoParam.busInfoListSize = 1;
    infoParam.busInfoList = LW_PTR_TO_LwP64(&busInfo);

    if(pLwRm->Control(hRmSubdev, LW2080_CTRL_CMD_BUS_GET_INFO,
                            &infoParam, sizeof(infoParam)) != OK)
    {
        MASSERT(!"LwRm control failed\n");
        return 0;
    }

    UINT32 Width = DRF_VAL(2080_CTRL,_BUS_INFO,
                           _PCIE_LINK_CTRL_STATUS_LINK_WIDTH,
                           busInfo.data);

    return Width;
}

//-----------------------------------------------------------------------------
//! \brief Set the PCIE link width of the GPU
//!
//! \param NewSpeed : New PCIE link width to set
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC FermiPcie::DoSetLinkWidth(UINT32 LinkWidth)
{
    RC rc;
    LW2080_CTRL_BUS_SET_PCIE_LINK_WIDTH_PARAMS LwParam = {0};
    LwParam.pcieLinkWidth = LinkWidth;

    rc = LwRmPtr()->ControlBySubdevice(GetSubdevice(),
                                       LW2080_CTRL_CMD_BUS_SET_PCIE_LINK_WIDTH,
                                       &LwParam,
                                       sizeof(LwParam));
    if (rc != RC::OK)
    {
        string reasonStr;

        if (LwParam.failingReason == LW2080_CTRL_BUS_SET_PCIE_LINK_WIDTH_ERROR_TRAINING)
        {
            reasonStr = " due to a training error";
        }
        else if (!DoIsASLMCapable())
        {
            reasonStr = " because ASLM is disabled";
        }

        Printf(Tee::PriError, "Cannot change PCIE link width to x%d%s\n",
            LinkWidth, reasonStr.c_str());
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Set PCIE error polling status.  When PCIE errors are polled, flag
//!        based counters are used instead of full width hardware counters that
//!        are present on modern GPUs (used to force new GPUs to behave like
//!        older GPUs)
//!
//! \param UsePoll  : true to poll PCIE error flags, false to read PCIE error
//!                   counters
/* virtual */ void FermiPcie::DoSetUsePolledErrors(bool UsePoll)
{
    m_UsePolledErrors = UsePoll;
}

/* virtual */ void FermiPcie::DoSetForceCountersToZero(bool ForceZero)
{
    m_ForceCountersToZero = ForceZero;
}

//-----------------------------------------------------------------------------
//! \brief Read the PCIE errors both at the GPU and on the downstream port of
//!        the bridge/rootport directly upstream of the gpu. PCIE errors need
//!        be checked check on both side of the link.  The GPU errors are
//!        retrieved by checking flags and *not* hardware counters (even though
//!        all modern GPUs have hardware counters).  This is maintained in order
//!        to provide backward compatibility.  Note : These error flags are
//!        self-clearing
//!
//! \param pGpuErrors  : Pointer to returned GPU PCIE errors mask
//! \param pHostErrors : Pointer to returned downstream port PCIE errors mask
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC FermiPcie::DoGetPolledErrors(Pci::PcieErrorsMask *pGpuErrors,
                                               Pci::PcieErrorsMask *pHostErrors)
{
    RC rc;
    LW2080_CTRL_BUS_INFO busInfo = {0};
    LW2080_CTRL_BUS_GET_INFO_PARAMS infoParam = {0};
    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(GetSubdevice());
    MASSERT(pRmHandle);

    // Pre-fill the info params
    infoParam.busInfoListSize = 1;
    infoParam.busInfoList = LW_PTR_TO_LwP64(&busInfo);

    if (pGpuErrors)
    {
        *pGpuErrors = Pci::NO_ERROR;

        // get the error status from the Gpu side
        busInfo.index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_ERRORS;

        CHECK_RC(pLwRm->Control(pRmHandle, LW2080_CTRL_CMD_BUS_GET_INFO,
                          &infoParam, sizeof(infoParam)));

        // maybe use a compile time check (between LW2080 defines and
        // Pcie::PcieErrors instead...
        if (busInfo.data & LW2080_CTRL_BUS_INFO_PCIE_LINK_ERRORS_CORR_ERROR)
            *pGpuErrors |= Pci::CORRECTABLE_ERROR;
        if (busInfo.data & LW2080_CTRL_BUS_INFO_PCIE_LINK_ERRORS_NON_FATAL_ERROR)
            *pGpuErrors |= Pci::NON_FATAL_ERROR;
        if (busInfo.data & LW2080_CTRL_BUS_INFO_PCIE_LINK_ERRORS_FATAL_ERROR)
            *pGpuErrors |= Pci::FATAL_ERROR;
        if (busInfo.data & LW2080_CTRL_BUS_INFO_PCIE_LINK_ERRORS_UNSUPP_REQUEST)
            *pGpuErrors |= Pci::UNSUPPORTED_REQ;
    }

    if (pHostErrors)
    {
        // Get the error status on the upper side of the link
        // try to et the bridge - null means Gpu is connected to chipset
        PexDevice *pUpStreamDev;
        UINT32 ParentPort;

        *pHostErrors = Pci::NO_ERROR;
        CHECK_RC(GetUpStreamInfo(&pUpStreamDev, &ParentPort));
        if (!pUpStreamDev)
        {
            // Todo: remove this case? Maybe just return Software error here
            //
            // this gets the status on the root port
            memset(&busInfo, 0, sizeof(busInfo));
            busInfo.index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_ROOT_LINK_ERRORS;
            CHECK_RC(pLwRm->Control(pRmHandle, LW2080_CTRL_CMD_BUS_GET_INFO,
                              &infoParam, sizeof(infoParam)));

            if (busInfo.data & LW2080_CTRL_BUS_INFO_PCIE_LINK_ERRORS_CORR_ERROR)
                *pHostErrors |= Pci::CORRECTABLE_ERROR;
            if (busInfo.data & LW2080_CTRL_BUS_INFO_PCIE_LINK_ERRORS_NON_FATAL_ERROR)
                *pHostErrors |= Pci::NON_FATAL_ERROR;
            if (busInfo.data & LW2080_CTRL_BUS_INFO_PCIE_LINK_ERRORS_FATAL_ERROR)
                *pHostErrors |= Pci::FATAL_ERROR;
            if (busInfo.data & LW2080_CTRL_BUS_INFO_PCIE_LINK_ERRORS_UNSUPP_REQUEST)
                *pHostErrors |= Pci::UNSUPPORTED_REQ;
        }
        else
        {
            CHECK_RC(pUpStreamDev->GetDownStreamErrors(pHostErrors,
                                                       ParentPort));
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Read the PCIE errors both at the GPU and on the downstream port of
//!        the bridge/rootport directly upstream of the gpu. PCIE errors need
//!        be checked check on both side of the link.
//!
//! \param pLocCounts  : Pointer to returned GPU PCIE error counts
//! \param pHostCounts : Pointer to returned downstream port PCIE error counts
//! \param CountType   : Type of errors to retrieve (flag based only, counter
//!                      based only, or both)
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC FermiPcie::DoGetErrorCounts
(
    PexErrorCounts *pLocCounts,
    PexErrorCounts *pHostCounts,
    PexDevice::PexCounterType CountType
)
{
    RC rc;
    CHECK_RC(GetUpstreamErrorCounts(pHostCounts, CountType));
    if (!pLocCounts)
        return rc;

    PexErrorCounts LocCounts;
    if ((CountType != PexDevice::PEX_COUNTER_HW))
    {
        CHECK_RC(GetGpuPolledErrorCounts(&LocCounts));
    }

    // If the implementation doesnt have hardware error counters, then use
    // whatever counts were retrieved
    //
    // UsePolledErrors() makes newer GPUs (that have actual hw counters
    // for PEX) behave like older GPUs (that are only flag based).
    if (UsePolledErrors())
    {
        *pLocCounts = LocCounts;
        ResetErrorCounters();
        return rc;
    }

    // If only retrieving flag based counters, just return immediately.
    // The call to ResetPcieErrorCounters() is purposefully skipped below
    // since retrieving flag based counters should not affect hw counter
    // based counters
    if (CountType == PexDevice::PEX_COUNTER_FLAG)
        return rc;

    CHECK_RC(DoGetErrorCounters(pLocCounts));
    CHECK_RC(UpdateAerLog());
    CHECK_RC(ResetErrorCounters());

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Return whether Advanced Error Reporting (AER) is enabled.
//!
//! \return true if AER is enabled, false otherwise
bool FermiPcie::DoAerEnabled() const
{
    return GetSubdevice()->Regs().Test32(MODS_XVE_PRIV_MISC_1_CYA_AER_ENABLE);
}

//-----------------------------------------------------------------------------
//! \brief Enable or disable Advanced Error Reporting (AER)
//!
//! \param bEnable : true to enable AER, false to disable
void FermiPcie::DoEnableAer(bool bEnable)
{
    RegHal& regs = GetSubdevice()->Regs();
    UINT32 xveMisc1 = regs.Read32(MODS_XVE_PRIV_MISC_1);
    if (!m_bRestorePexAer)
    {
        m_bPexAerEnabled = regs.TestField(xveMisc1,
                                          MODS_XVE_PRIV_MISC_1_CYA_AER_ENABLE);
        m_bRestorePexAer = true;
    }
    //update using mask
    regs.SetField(&xveMisc1, MODS_XVE_PRIV_MISC_1_CYA_AER, bEnable);
    regs.Write32(MODS_XVE_PRIV_MISC_1, xveMisc1);
}

//-----------------------------------------------------------------------------
RC FermiPcie::DoEnableAspmDeepL1(bool bEnable)
{
    RC rc;

    bool bLwrrentDeepL1State = false;
    CHECK_RC(GetAspmDeepL1Enabled(&bLwrrentDeepL1State));
    if (bEnable == bLwrrentDeepL1State)
        return RC::OK;

    LW2080_CTRL_LPWR_FEATURE_PARAMETER_SET_PARAMS params = {};
    params.listSize = 1;
    params.list[0].feature.id    = LW2080_CTRL_LPWR_FEATURE_ID_PEX;
    params.list[0].feature.subId = LW2080_CTRL_LPWR_SUB_FEATURE_ID_PEX_DEEP_L1;
    params.list[0].param.id      = LW2080_CTRL_LPWR_PARAMETER_ID_PEX_CTRL_ENABLE;
    params.list[0].param.val     = bEnable ? 1 : 0;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        GetSubdevice(),
        LW2080_CTRL_CMD_LPWR_FEATURE_PARAMETER_SET,
        &params,
        sizeof(params)));

    return (FLD_TEST_DRF(2080_CTRL, _LPWR_FEATURE, _PARAMETER_FLAG_SUCCEED, _YES,
                         params.list[0].param.flag)) ? RC::OK : RC::LWRM_ERROR;
}

//-----------------------------------------------------------------------------
//! \return the offset of AER in the PCI Extended Caps list
UINT16 FermiPcie::DoGetAerOffset() const
{
    return LW_XVE_AER_CAP_HDR;
}

//-----------------------------------------------------------------------------
//! \brief Get the type of ASPM that the GPU PCIE link supports.
//!
//! \return The type of ASPM that the GPU PCIE link supports
/* virtual */ UINT32 FermiPcie::DoAspmCapability()
{
    const RegHal& regs = GetSubdevice()->Regs();
    const UINT32 cap =
        Platform::IsVirtFunMode() ? 0 :
        regs.Read32(MODS_EP_PCFG_GPU_LINK_CAPABILITIES_ASPM_SUPPORT);
    return static_cast<Pci::ASPMState>(cap);
}

//-----------------------------------------------------------------------------
/* virtual */ UINT32 FermiPcie::DoDeviceId() const
{
    return GetSubdevice()->PciDeviceId();
}

//-----------------------------------------------------------------------------
//! \brief Get the current ASPM state of the GPU.
//!
//! \return The current ASPM state of the GPU
/* virtual */ UINT32 FermiPcie::DoGetAspmState()
{
    if (Platform::IsVirtFunMode())
    {
        return Pci::ASPM_DISABLED;
    }

    const RegHal& regs       = GetSubdevice()->Regs();
    const UINT32  ctrlStatus = regs.Read32(MODS_EP_PCFG_GPU_LINK_CONTROL_STATUS);
    const UINT32  state      = DRF_VAL(2080_CTRL, _BUS_INFO,
                                       _PCIE_LINK_CTRL_STATUS_ASPM, ctrlStatus);

    Pci::ASPMState aspmState = Pci::ASPM_UNKNOWN_STATE;
    switch (state)
    {
        case LW2080_CTRL_BUS_INFO_PCIE_LINK_CTRL_STATUS_ASPM_L0S:
            aspmState = Pci::ASPM_L0S;
            break;
        case LW2080_CTRL_BUS_INFO_PCIE_LINK_CTRL_STATUS_ASPM_L1:
            aspmState = Pci::ASPM_L1;
            break;
        case LW2080_CTRL_BUS_INFO_PCIE_LINK_CTRL_STATUS_ASPM_L0S_L1:
            aspmState = Pci::ASPM_L0S_L1;
            break;
        case LW2080_CTRL_BUS_INFO_PCIE_LINK_CTRL_STATUS_ASPM_DISABLED:
            aspmState = Pci::ASPM_DISABLED;
            break;
    }
    MASSERT(aspmState != Pci::ASPM_UNKNOWN_STATE);
    return aspmState;
}

//-----------------------------------------------------------------------------
//! \brief Get the current ASPM CYA state of the GPU.  CYA is the setting the RM
//!        uses to do back door ASPM changes. This can be different per pstate.
//!        ASPM state is the final status - which can be motherboard dependent
//!
//! \return The current ASPM CYA state of the GPU
/* virtual */ UINT32 FermiPcie::DoGetAspmCyaState()
{
    LW2080_CTRL_BUS_INFO busInfo = {0};
    busInfo.index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_CYA_ASPM;
    busInfo.data  = 0;

    LW2080_CTRL_BUS_GET_INFO_PARAMS infoParam = {0};
    infoParam.busInfoListSize    = 1;
    infoParam.busInfoList        = LW_PTR_TO_LwP64(&busInfo);

    if (LwRmPtr()->ControlBySubdevice(GetSubdevice(),
                                      LW2080_CTRL_CMD_BUS_GET_INFO,
                                      &infoParam,
                                      sizeof(infoParam)) != OK)
    {
        MASSERT(!"Rm control call for querying ASPM Cya failed\n");
        return Pci::ASPM_UNKNOWN_STATE;
    }

    UINT32 Valid = DRF_VAL(2080_CTRL,_BUS_INFO,_PCIE_GPU_CYA_ASPM_VALID,
                           busInfo.data);
    if (Valid == LW2080_CTRL_BUS_INFO_PCIE_GPU_CYA_ASPM_VALID_NO)
    {
        return Pci::ASPM_UNKNOWN_STATE;
    }

    const UINT32 state = DRF_VAL(2080_CTRL,_BUS_INFO,_PCIE_GPU_CYA_ASPM,
                                 busInfo.data);
    Pci::ASPMState aspmState = Pci::ASPM_UNKNOWN_STATE;
    switch (state)
    {
        case LW2080_CTRL_BUS_INFO_PCIE_GPU_CYA_ASPM_L0S:
            aspmState = Pci::ASPM_L0S;
            break;
        case LW2080_CTRL_BUS_INFO_PCIE_GPU_CYA_ASPM_L1:
            aspmState = Pci::ASPM_L1;
            break;
        case LW2080_CTRL_BUS_INFO_PCIE_GPU_CYA_ASPM_L0S_L1:
            aspmState = Pci::ASPM_L0S_L1;
            break;
        case LW2080_CTRL_BUS_INFO_PCIE_GPU_CYA_ASPM_DISABLED:
            aspmState = Pci::ASPM_DISABLED;
            break;
    }

    MASSERT(aspmState != Pci::ASPM_UNKNOWN_STATE);
    return aspmState;
}

//-----------------------------------------------------------------------------
//! \brief Set the current ASPM state of the GPU.  Note that this sets *only*
//!        the ASPM state and not the CYA state so this will not force ASPM if
//!        the current RM setting of CYA differs from the requested state
//!
//! \param State : ASPM state to set
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC FermiPcie::DoSetAspmState(UINT32 state)
{
    if (Platform::IsVirtFunMode())
    {
        return OK;
    }

    RegHal& regs       = GetSubdevice()->Regs();
    UINT32  ctrlStatus = regs.Read32(MODS_EP_PCFG_GPU_LINK_CONTROL_STATUS);

    switch (state)
    {
        case Pci::ASPM_DISABLED:
        case Pci::ASPM_L0S:
        case Pci::ASPM_L1:
        case Pci::ASPM_L0S_L1:
            regs.SetField(
                    &ctrlStatus,
                    MODS_EP_PCFG_GPU_LINK_CONTROL_STATUS_ASPM_CTRL,
                    state);
            break;
        default:
            return RC::BAD_PARAMETER;
    }
    Printf(Tee::PriLow, "SetAspm on GPU: 0x%x\n", ctrlStatus);
    regs.Write32(MODS_EP_PCFG_GPU_LINK_CONTROL_STATUS, ctrlStatus);
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Get the ASPM entry counters
//!
//! \param pCounts : Pointer to returned ASPM entry counts
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC FermiPcie::DoGetAspmEntryCounts
(
    Pcie::AspmEntryCounters *pCounts
)
{
    MASSERT(pCounts);

    RC rc;
    LW2080_CTRL_BUS_GET_PEX_COUNTERS_PARAMS params = { 0 };
    params.pexCounterMask =
        LW2080_CTRL_BUS_PEX_COUNTER_CHIPSET_XMIT_L0S_ENTRY_COUNT |
        LW2080_CTRL_BUS_PEX_COUNTER_GPU_XMIT_L0S_ENTRY_COUNT |
        LW2080_CTRL_BUS_PEX_COUNTER_L1_ENTRY_COUNT |
        LW2080_CTRL_BUS_PEX_COUNTER_L1P_ENTRY_COUNT |
        LW2080_CTRL_BUS_PEX_COUNTER_DEEP_L1_ENTRY_COUNT;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(GetSubdevice(),
                                           LW2080_CTRL_CMD_BUS_GET_PEX_COUNTERS,
                                           &params,
                                           sizeof(params)));

    pCounts->XmitL0SEntry = params.pexCounters[Utility::BitScanForward(
            LW2080_CTRL_BUS_PEX_COUNTER_GPU_XMIT_L0S_ENTRY_COUNT)];
    pCounts->UpstreamL0SEntry =params.pexCounters[Utility::BitScanForward(
            LW2080_CTRL_BUS_PEX_COUNTER_CHIPSET_XMIT_L0S_ENTRY_COUNT)];
    pCounts->L1Entry =params.pexCounters[Utility::BitScanForward(
            LW2080_CTRL_BUS_PEX_COUNTER_L1_ENTRY_COUNT)];
    pCounts->L1PEntry = params.pexCounters[Utility::BitScanForward(
            LW2080_CTRL_BUS_PEX_COUNTER_L1P_ENTRY_COUNT)];
    pCounts->DeepL1Entry = params.pexCounters[Utility::BitScanForward(
            LW2080_CTRL_BUS_PEX_COUNTER_DEEP_L1_ENTRY_COUNT)];
    pCounts->L1_1_Entry         = 0;
    pCounts->L1_2_Entry         = 0;
    pCounts->L1_2_Abort         = 0;
    pCounts->L1SS_DeepL1Timouts = 0;
    pCounts->L1_ShortDuration   = 0;

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Reset the ASPM entry counters
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC FermiPcie::DoResetAspmEntryCounters()
{
    LW2080_CTRL_BUS_CLEAR_PEX_COUNTERS_PARAMS params = { 0 };
    params.pexCounterMask =
        LW2080_CTRL_BUS_PEX_COUNTER_CHIPSET_XMIT_L0S_ENTRY_COUNT |
        LW2080_CTRL_BUS_PEX_COUNTER_GPU_XMIT_L0S_ENTRY_COUNT |
        LW2080_CTRL_BUS_PEX_COUNTER_L1_ENTRY_COUNT |
        LW2080_CTRL_BUS_PEX_COUNTER_L1P_ENTRY_COUNT |
        LW2080_CTRL_BUS_PEX_COUNTER_DEEP_L1_ENTRY_COUNT;

    return LwRmPtr()->ControlBySubdevice(GetSubdevice(),
                                         LW2080_CTRL_CMD_BUS_CLEAR_PEX_COUNTERS,
                                         &params,
                                         sizeof(params));
}

//-----------------------------------------------------------------------------
RC FermiPcie::DoHasAspmDeepL1(bool *pbHasDeepL1)
{
    MASSERT(pbHasDeepL1);

    RC rc;

    LW2080_CTRL_LPWR_FEATURE_PARAMETER_GET_PARAMS params = {};
    params.listSize = 1;
    params.list[0].feature.id    = LW2080_CTRL_LPWR_FEATURE_ID_PEX;
    params.list[0].feature.subId = LW2080_CTRL_LPWR_SUB_FEATURE_ID_PEX_DEEP_L1;
    params.list[0].param.id      = LW2080_CTRL_LPWR_PARAMETER_ID_PEX_CTRL_SUPPORT;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        GetSubdevice(),
        LW2080_CTRL_CMD_LPWR_FEATURE_PARAMETER_GET,
        &params,
        sizeof(params)));

    if (!FLD_TEST_DRF(2080_CTRL, _LPWR_FEATURE, _PARAMETER_FLAG_SUCCEED, _YES,
                      params.list[0].param.flag))
    {
        return RC::LWRM_ERROR;
    }

    *pbHasDeepL1 = params.list[0].param.val ? true : false;
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Return whether the GPU hardware has ASPM L1 Substates
//!
//! \return true if hardware has ASPM L1 substates, false otherwise
/* virtual */ bool FermiPcie::DoHasAspmL1Substates()
{
    MASSERT(GetSubdevice());
    return GetSubdevice()->HasFeature(Device::GPUSUB_HAS_PEX_ASPM_L1SS);
}

//-----------------------------------------------------------------------------
//! \brief Retrieve the ASPM L1SS capability mask from the hardware
//!
//! \param pCap : Pointer to returned ASPM L1SS capability mask
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC FermiPcie::DoGetAspmL1SSCapability(UINT32 *pCap)
{
    Printf(Tee::PriNormal, "This GPU does not support ASPM L1 substates\n");
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC FermiPcie::DoGetAspmCyaL1SubState(UINT32 *pState)
{
    Printf(Tee::PriNormal, "This GPU does not support ASPM L1 substates\n");
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
RC FermiPcie::DoGetAspmDeepL1Enabled(bool *pbEnabled)
{
    MASSERT(pbEnabled);

    RC rc;

    LW2080_CTRL_LPWR_FEATURE_PARAMETER_GET_PARAMS params = {};
    params.listSize = 1;
    params.list[0].feature.id    = LW2080_CTRL_LPWR_FEATURE_ID_PEX;
    params.list[0].feature.subId = LW2080_CTRL_LPWR_SUB_FEATURE_ID_PEX_DEEP_L1;
    params.list[0].param.id      = LW2080_CTRL_LPWR_PARAMETER_ID_PEX_CTRL_ENABLE;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        GetSubdevice(),
        LW2080_CTRL_CMD_LPWR_FEATURE_PARAMETER_GET,
        &params,
        sizeof(params)));

    if (!FLD_TEST_DRF(2080_CTRL, _LPWR_FEATURE, _PARAMETER_FLAG_SUCCEED, _YES,
                      params.list[0].param.flag))
    {
        return RC::LWRM_ERROR;
    }

    *pbEnabled = params.list[0].param.val ? true : false;
    return rc;
}

/* virtual */ RC FermiPcie::DoGetAspmL1ssEnabled(UINT32*)
{
    Printf(Tee::PriNormal, "This GPU does not support ASPM L1 substates\n");
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC FermiPcie::DoGetLTREnabled(bool *pEnabled)
{
    Printf(Tee::PriLow, "This GPU does not support LTR\n");
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
//! \brief Returns the fastest PCIE link speed supported by the downstream port
//!        of the PEX bridge/rootport immediately upstream of the GPU
//!
//! \return The fastest PCIE link speed supported by the downstream port
//!         of the PEX bridge/rootport immediately upstream of the GPU
/* virtual */ Pci::PcieLinkSpeed FermiPcie::DoGetUpstreamLinkSpeedCap()
{
    PexDevice* pPexDev;
    UINT32 Port;
    if (OK != GetUpStreamInfo(&pPexDev, &Port))
    {
        Printf(Tee::PriNormal, "cannot get upstream PCIE device\n");
        return Pci::Speed2500MBPS;
    }
    return pPexDev->GetDownStreamLinkSpeedCap(Port);
}

//-----------------------------------------------------------------------------
//! \brief Returns the widest PCIE link width supported by the downstream port
//!        of the PEX bridge/rootport immediately upstream of the GPU
//!
//! \return The widest PCIE link width supported by the downstream port
//!         of the PEX bridge/rootport immediately upstream of the GPU
/* virtual */ UINT32 FermiPcie::DoGetUpstreamLinkWidthCap()
{
    PexDevice* pPexDev;
    UINT32 Port;
    if (OK != GetUpStreamInfo(&pPexDev, &Port))
    {
        Printf(Tee::PriNormal, "cannot get upstream PCIE device\n");
        return 1;
    }
    return pPexDev->GetDownStreamLinkWidthCap(Port);
}

//-----------------------------------------------------------------------------
//! \brief Detect whether the GPU PCIE link entered recovery
//!
//! \return true if the PCIE link entered recovery, false otherwise
/* virtual */ bool FermiPcie::DoDidLinkEnterRecovery()
{
    LW2080_CTRL_BUS_INFO busInfo = {0};
    busInfo.index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_ERRORS;
    LW2080_CTRL_BUS_GET_INFO_PARAMS infoParam = {0};
    infoParam.busInfoListSize = 1;
    infoParam.busInfoList = LW_PTR_TO_LwP64(&busInfo);
    if (OK == LwRmPtr()->ControlBySubdevice(GetSubdevice(), LW2080_CTRL_CMD_BUS_GET_INFO,
                                            &infoParam, sizeof(infoParam)))
    {
        return ((busInfo.data & LW2080_CTRL_BUS_INFO_PCIE_LINK_ERRORS_ENTERED_RECOVERY) != 0);
    }
    return false;
}

//-----------------------------------------------------------------------------
//! \brief Get the PCIE error counts on the GPU via hardware counters
//!
//! \param pCounts  : Pointer to GPU counts.
//!
//! \return OK if successful, not OK otherwise
RC FermiPcie::DoGetErrorCounters(PexErrorCounts *pCounts)
{
    RC rc;

    // rely on RM to do the counting. RM will keep track of these counters
    LW2080_CTRL_BUS_INFO busInfo[8];
    memset(busInfo, 0, sizeof(busInfo));
    busInfo[0].index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_LINECODE_ERRORS;
    busInfo[1].index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_CRC_ERRORS;
    busInfo[2].index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_NAKS_RECEIVED;
    busInfo[3].index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_FAILED_L0S_EXITS;
    busInfo[4].index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_CORRECTABLE_ERRORS;
    busInfo[5].index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_NONFATAL_ERRORS;
    busInfo[6].index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_FATAL_ERRORS;
    busInfo[7].index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_UNSUPPORTED_REQUESTS;

    LW2080_CTRL_BUS_GET_INFO_PARAMS infoParam = {0};
    infoParam.busInfoListSize = 8;
    infoParam.busInfoList = LW_PTR_TO_LwP64(busInfo);

    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(GetSubdevice());
    MASSERT(pRmHandle);
    CHECK_RC(LwRmPtr()->Control(pRmHandle, LW2080_CTRL_CMD_BUS_GET_INFO,
                                               &infoParam, sizeof(infoParam)));

    if (GetForceCountersToZero())
    {
        const size_t numElems = NUMELEMS(busInfo);
        for (size_t busInfoIdx = 0; busInfoIdx < numElems; busInfoIdx++)
            busInfo[busInfoIdx].data = 0;
    }

    CHECK_RC(pCounts->SetCount(PexErrorCounts::DETAILED_LINEERROR_ID, true,
                               busInfo[0].data));
    CHECK_RC(pCounts->SetCount(PexErrorCounts::DETAILED_CRC_ID, true,
                               busInfo[1].data));
    CHECK_RC(pCounts->SetCount(PexErrorCounts::DETAILED_NAKS_R_ID, true,
                               busInfo[2].data));
    CHECK_RC(pCounts->SetCount(PexErrorCounts::DETAILED_FAILEDL0SEXITS_ID,
                               true,
                               busInfo[3].data));
    CHECK_RC(pCounts->SetCount(PexErrorCounts::CORR_ID, true,
                               busInfo[4].data));
    CHECK_RC(pCounts->SetCount(PexErrorCounts::NON_FATAL_ID, true,
                               busInfo[5].data));
    CHECK_RC(pCounts->SetCount(PexErrorCounts::FATAL_ID, true,
                               busInfo[6].data));
    CHECK_RC(pCounts->SetCount(PexErrorCounts::UNSUP_REQ_ID, true,
                               busInfo[7].data));

    // set NAKsSent to zero for fermi and older cards
    //
    // Even though there is a validity bit, it is necessary to pretend to
    // read NAKsSent on Fermi and older so the JS behavior doesnt change
    CHECK_RC(pCounts->SetCount(PexErrorCounts::DETAILED_NAKS_S_ID, true, 0));

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Reset the PCIE error counts on the GPU
//!
//! \return OK if successful, not OK otherwise
RC FermiPcie::DoResetErrorCounters()
{
    RC rc;
    LW2080_CTRL_BUS_INFO busInfo[8];
    memset(busInfo, 0, sizeof(busInfo));
    busInfo[0].index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_LINECODE_ERRORS_CLEAR;
    busInfo[1].index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_CRC_ERRORS_CLEAR;
    busInfo[2].index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_NAKS_RECEIVED_CLEAR;
    busInfo[3].index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_FAILED_L0S_EXITS_CLEAR;
    busInfo[4].index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_CORRECTABLE_ERRORS_CLEAR;
    busInfo[5].index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_NONFATAL_ERRORS_CLEAR;
    busInfo[6].index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_FATAL_ERRORS_CLEAR;
    busInfo[7].index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_UNSUPPORTED_REQUESTS_CLEAR;

    LW2080_CTRL_BUS_GET_INFO_PARAMS infoParam = {0};
    infoParam.busInfoListSize = 8;
    infoParam.busInfoList = LW_PTR_TO_LwP64(busInfo);

    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(GetSubdevice());
    MASSERT(pRmHandle);
    CHECK_RC(LwRmPtr()->Control(pRmHandle, LW2080_CTRL_CMD_BUS_GET_INFO,
                                               &infoParam, sizeof(infoParam)));

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Private function to read the PCIE errors via polled flags at the GPU
//!
//! \param pCounts  : Pointer to returned GPU PCIE error counts
//!
//! \return OK if successful, not OK otherwise
RC FermiPcie::GetGpuPolledErrorCounts(PexErrorCounts * pCounts)
{
    RC rc;
    if (pCounts)
    {
        Pci::PcieErrorsMask LocError;
        CHECK_RC(GetPolledErrors(&LocError, NULL));
        CHECK_RC(pCounts->SetCount(PexErrorCounts::CORR_ID,
                             false,
                             (LocError & Pci::CORRECTABLE_ERROR) ? 1 : 0));
        CHECK_RC(pCounts->SetCount(PexErrorCounts::NON_FATAL_ID,
                             false,
                             (LocError & Pci::NON_FATAL_ERROR) ? 1 : 0));
        CHECK_RC(pCounts->SetCount(PexErrorCounts::FATAL_ID,
                             false,
                             (LocError & Pci::FATAL_ERROR) ? 1 : 0));
        CHECK_RC(pCounts->SetCount(PexErrorCounts::UNSUP_REQ_ID,
                             false,
                             (LocError & Pci::UNSUPPORTED_REQ) ? 1 : 0));
    }
    return rc;
}

//-----------------------------------------------------------------------------
GpuSubdevice* FermiPcie::GetSubdevice()
{
    return GetDevice()->GetInterface<GpuSubdevice>();
}

//-----------------------------------------------------------------------------
const GpuSubdevice* FermiPcie::GetSubdevice() const
{
    return GetDevice()->GetInterface<GpuSubdevice>();
}

//-----------------------------------------------------------------------------
//! \brief Private functino to load PCIE information from RM
//!
//! \return OK if successful, not OK otherwise
RC FermiPcie::LoadPcieInfo()
{
    if (m_PciInfoLoaded) return OK;

    RC rc;

    rc = LwRmPtr()->ControlBySubdevice(GetSubdevice(),
                                       LW2080_CTRL_CMD_BUS_GET_PCI_INFO,
                                       &m_PciInfo, sizeof(m_PciInfo));

    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "ERROR LOADING PCI INFO! RmControl call failed! RC = %d\n",
               (UINT32)rc);
        memset(&m_PciInfo, 0, sizeof (m_PciInfo));
#ifndef SEQ_FAIL
        MASSERT(!"Failed to load PCI info through RmControl");
#endif
        return rc;
    }

    m_PciInfoLoaded = true;
    return OK;
}
