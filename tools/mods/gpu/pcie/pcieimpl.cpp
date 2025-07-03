/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/pcie/pcieimpl.h"
#include "gpu/include/testdevice.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gpudev.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include "core/include/platform.h"
#include "core/include/mgrmgr.h"
#include "lwmisc.h"
#include "rm.h"

//-----------------------------------------------------------------------------
//! \brief Get PCI AD Normal (?) - doesnt appear to be used, but there is a JS
//!        interface to it so it is probably diffilwlt to remove
//!
//! \return ??
/* virtual */ UINT32 PcieImpl::DoAdNormal()
{
   return 1;
}

//-----------------------------------------------------------------------------
/* virtual */ UINT32 PcieImpl::DoBusNumber() const
{
    UINT16 bus = _UINT16_MAX;
    GetDevice()->GetId().GetPciDBDF(nullptr, &bus);
    return bus;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PcieImpl::DoClearAerLog()
{
    m_AerLog.clear();
    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ UINT32 PcieImpl::DoDeviceId() const
{
    return GetDevice()->GetDeviceId();
}

//-----------------------------------------------------------------------------
/* virtual */ UINT32 PcieImpl::DoDeviceNumber() const
{
    UINT16 device = _UINT16_MAX;
    GetDevice()->GetId().GetPciDBDF(nullptr, nullptr, &device);
    return device;
}

//-----------------------------------------------------------------------------
/* virtual */ UINT32 PcieImpl::DoDomainNumber() const
{
    UINT16 domain = _UINT16_MAX;
    GetDevice()->GetId().GetPciDBDF(&domain);
    return domain;
}

//-----------------------------------------------------------------------------
RC PcieImpl::DoEnableAspmDeepL1(bool bEnable)
{
    MASSERT(!"Deep L1 state changes not supported on this device");
    return RC::UNSUPPORTED_HARDWARE_FEATURE;
}

/* virtual */ UINT32 PcieImpl::DoFunctionNumber() const
{
    UINT16 function = _UINT16_MAX;
    GetDevice()->GetId().GetPciDBDF(nullptr, nullptr, nullptr, &function);
    return function;
}

//-----------------------------------------------------------------------------
RC PcieImpl::DoEnableAtomics(bool *pbAtomicsEnabled, UINT32 *pAtomicTypes)
{
    MASSERT(pbAtomicsEnabled);
    MASSERT(pAtomicTypes);

    RC rc;
    *pbAtomicsEnabled = false;
    *pAtomicTypes = 0;
    CHECK_RC(GetAtomicTypesSupported(pAtomicTypes));
    if (*pAtomicTypes)
    {
        RegHal& regs = GetDevice()->Regs();
        regs.Write32(MODS_EP_PCFG_GPU_DEVICE_CONTROL_STATUS_2_ATOMIC_OP_REQUESTER_ENABLE, 1);

        // This mirrors how the real driver enables atomics, on GPUs that do not support
        // atomics the bit remains zero even after writing a 1
        *pbAtomicsEnabled =
            regs.Test32(MODS_EP_PCFG_GPU_DEVICE_CONTROL_STATUS_2_ATOMIC_OP_REQUESTER_ENABLE, 1);
    }
    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PcieImpl::DoGetAerLog(vector<PexDevice::AERLogEntry>* pLogEntries) const
{
    MASSERT(pLogEntries);
    pLogEntries->clear();
    pLogEntries->insert(pLogEntries->begin(),
                        m_AerLog.begin(),
                        m_AerLog.end());
    return OK;
}

//-----------------------------------------------------------------------------
RC PcieImpl::DoGetAspmCyaL1SubStateProtected(UINT32 *pState) const
{
    MASSERT(pState);
    *pState = Pci::PM_UNKNOWN_SUB_STATE;
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC PcieImpl::DoGetAspmCyaStateProtected(UINT32 *pState) const
{
    MASSERT(pState);
    *pState = Pci::ASPM_UNKNOWN_STATE;
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC PcieImpl::DoGetAspmDeepL1Enabled(bool *pbEnabled)
{
    MASSERT(pbEnabled);
    MASSERT(!"Deep L1 state query not supported on this device");
    *pbEnabled = false;
    return RC::UNSUPPORTED_HARDWARE_FEATURE;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PcieImpl::DoGetAspmL1ssEnabled(UINT32* pAspm)
{
    *pAspm = 0;

    RC rc;

    UINT16 domain, bus, device, function;
    GetDevice()->GetId().GetPciDBDF(&domain, &bus, &device, &function);

    UINT08 capPtr;
    CHECK_RC(Pci::FindCapBase(domain,
                              bus,
                              device,
                              function,
                              PCI_CAP_ID_PCIE,
                              &capPtr));

    UINT16 l1ssOffset = 0;
    UINT16 l1ssSize = 0;
    if (OK == Pci::GetExtendedCapInfo(domain,
                                      bus,
                                      device,
                                      function,
                                      Pci::L1SS_ECI,
                                      capPtr,
                                      &l1ssOffset,
                                      &l1ssSize))
    {
        UINT16 l1ssCtrl = 0;
        CHECK_RC(Platform::PciRead16(domain,
                                     bus,
                                     device,
                                     function,
                                     l1ssOffset + LW_PCI_L1SS_CONTROL,
                                     &l1ssCtrl));

        if (FLD_TEST_DRF_NUM(_PCI, _L1SS_CONTROL, _L12_ASPM, 1, l1ssCtrl))
        {
            *pAspm |= Pci::PM_SUB_L12;
        }
        if (FLD_TEST_DRF_NUM(_PCI, _L1SS_CONTROL, _L11_ASPM, 1, l1ssCtrl))
        {
            *pAspm |= Pci::PM_SUB_L11;
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Get the current ASPM state of the GPU.
//!
//! \return The current ASPM state of the GPU
/* virtual */ UINT32 PcieImpl::DoGetAspmState()
{
    UINT16 linkCtrl = 0;
    if (OK != ReadPciCap(LW_PCI_CAP_PCIE_LINK_CTRL, &linkCtrl))
    {
        Printf(Tee::PriError, "%s : Cannot read ASPM State!\n", __FUNCTION__);
        return 0;
    }
    return DRF_VAL(_PCI, _CAP_PCIE_LINK_CTRL, _ASPM, linkCtrl);
}

//-----------------------------------------------------------------------------
//! \brief Returns the current PCIE link speed of the GPU
//!
//! \return The current PCIE link speed of the GPU
/* virtual */ Pci::PcieLinkSpeed PcieImpl::DoGetLinkSpeed
(
    Pci::PcieLinkSpeed expectedSpeed
)
{
    UINT16 linkStatus = 0;
    if (OK != ReadPciCap(LW_PCI_CAP_PCIE_LINK_STS, &linkStatus))
    {
        Printf(Tee::PriError, "%s : Cannot read PCIE Link Speed!\n", __FUNCTION__);
        return Pci::SpeedUnknown;
    }

    switch (DRF_VAL(_PCI_CAP, _PCIE_LINK_STS, _LINK_SPEED, linkStatus))
    {
        case 1:
            return Pci::Speed2500MBPS;
        case 2:
            return Pci::Speed5000MBPS;
        case 3:
            return Pci::Speed8000MBPS;
        case 4:
            return Pci::Speed16000MBPS;
        case 5:
            return Pci::Speed32000MBPS;
        default:
            Printf(Tee::PriError, "%s : Unknown PCIE link speed %d\n",
                   __FUNCTION__, DRF_VAL(_PCI_CAP, _PCIE_LINK_STS, _LINK_SPEED, linkStatus));
            return Pci::SpeedUnknown;
    }
}

//-----------------------------------------------------------------------------
//! \brief Returns the current PCIE link width of the GPU
//!
//! \return The current PCIE link width of the GPU
/* virtual */ UINT32 PcieImpl::DoGetLinkWidth()
{
    UINT16 linkStatus = 0;
    if (OK != ReadPciCap(LW_PCI_CAP_PCIE_LINK_STS, &linkStatus))
    {
        Printf(Tee::PriError, "%s : Cannot read PCIE Link Width!\n", __FUNCTION__);
        return 0;
    }
    return DRF_VAL(_PCI_CAP, _PCIE_LINK_STS, _LINK_WIDTH, linkStatus);
}

//-----------------------------------------------------------------------------
//! \brief Returns the current PCIE link width of the GPU and the width of the
//!        downstream port of the bridge/root device to which the GPU is
//!        connected.
//!
//! \param pGpuWidth : Pointer to returned GPU width.  Must not be NULL
//! \param pHostWidth : Pointer to returned downstream port width.
//!                     Must not be NULL
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC PcieImpl::DoGetLinkWidths(UINT32 *pDevWidth,
                                            UINT32 *pHostWidth)
{
    MASSERT(pDevWidth);
    MASSERT(pHostWidth);
    RC rc;
    *pDevWidth = GetLinkWidth();

    // Get the error status on the upper side of the link
    // try to et the bridge - null means Gpu is connected to chipset
    PexDevice *pUpStreamDev = NULL;
    UINT32 ParentPort;
    CHECK_RC(GetUpStreamInfo(&pUpStreamDev, &ParentPort));
    if (pUpStreamDev)
    {
        return pUpStreamDev->GetDownStreamLinkWidth(pHostWidth, ParentPort);
    }

    // Even if Gpu is connected directly to Chipset PCIE controller,
    // chipset PCIE controller should be seen as device
    return RC::SOFTWARE_ERROR;
}

//-----------------------------------------------------------------------------
//! \brief Helper function to get the PCIE lanes for Rx or Tx
//!
//! \param isTx : Set to 'true' for Tx, 'false' for Rx
//!
//! \return PCIE lanes (16 bits, each one corresponding to a lane)
/* virtual */ UINT32 PcieImpl::GetRegLanes(bool isTx) const
{
    UINT32 val = 0;
    const RegHal& regs = GetDevice()->Regs();

    UINT32 regVal = regs.Read32(MODS_XP_PL_LINK_STATUS_0);

    // Map the 5 bit register field value to the corresponding lanes
    if (isTx)
    {
        if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_NULL))
        {
            val = 0x0;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_00_00))
        {
            val = 0x1;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_01_01))
        {
            val = 0x2;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_02_02))
        {
            val = 0x4;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_03_03))
        {
            val = 0x8;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_04_04))
        {
            val = 0x10;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_05_05))
        {
            val = 0x20;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_06_06))
        {
            val = 0x40;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_07_07))
        {
            val = 0x80;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_08_08))
        {
            val = 0x100;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_09_09))
        {
            val = 0x200;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_10_10))
        {
            val = 0x400;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_11_11))
        {
            val = 0x800;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_12_12))
        {
            val = 0x1000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_13_13))
        {
            val = 0x2000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_14_14))
        {
            val = 0x4000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_15_15))
        {
            val = 0x8000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_01_00))
        {
            val = 0x3;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_03_02))
        {
            val = 0xC;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_05_04))
        {
            val = 0x30;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_07_06))
        {
            val = 0xC0;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_09_08))
        {
            val = 0x300;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_11_10))
        {
            val = 0xC00;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_13_12))
        {
            val = 0x3000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_15_14))
        {
            val = 0xC000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_03_00))
        {
            val = 0xF;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_07_04))
        {
            val = 0xF0;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_11_08))
        {
            val = 0xF00;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_15_12))
        {
            val = 0xF000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_07_00))
        {
            val = 0xFF;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_15_08))
        {
            val = 0xFF00;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_15_00))
        {
            val = 0xFFFF;
        }
    }
    else
    {
        if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_NULL))
        {
            val = 0x0;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_00_00))
        {
            val = 0x1;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_01_01))
        {
            val = 0x2;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_02_02))
        {
            val = 0x4;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_03_03))
        {
            val = 0x8;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_04_04))
        {
            val = 0x10;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_05_05))
        {
            val = 0x20;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_06_06))
        {
            val = 0x40;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_07_07))
        {
            val = 0x80;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_08_08))
        {
            val = 0x100;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_09_09))
        {
            val = 0x200;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_10_10))
        {
            val = 0x400;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_11_11))
        {
            val = 0x800;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_12_12))
        {
            val = 0x1000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_13_13))
        {
            val = 0x2000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_14_14))
        {
            val = 0x4000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_15_15))
        {
            val = 0x8000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_01_00))
        {
            val = 0x3;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_03_02))
        {
            val = 0xC;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_05_04))
        {
            val = 0x30;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_07_06))
        {
            val = 0xC0;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_09_08))
        {
            val = 0x300;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_11_10))
        {
            val = 0xC00;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_13_12))
        {
            val = 0x3000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_15_14))
        {
            val = 0xC000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_03_00))
        {
            val = 0xF;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_07_04))
        {
            val = 0xF0;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_11_08))
        {
            val = 0xF00;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_15_12))
        {
            val = 0xF000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_07_00))
        {
            val = 0xFF;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_15_08))
        {
            val = 0xFF00;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_15_00))
        {
            val = 0xFFFF;
        }
    }

    return val;
}

//-----------------------------------------------------------------------------
//! \brief Get the Rx and Tx PCIE lanes
//!
//! \param pRxLanes : Rx lanes
//! \param pTxLanes : Tx lanes
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC PcieImpl::DoGetPcieLanes(UINT32 *pRxLanes, UINT32 *pTxLanes)
{
    if (!GetDevice()->Regs().IsSupported(MODS_XPL_PL_LTSSM_LINK_STATUS_0) &&
        !GetDevice()->Regs().IsSupported(MODS_XP_PL_LINK_STATUS_0))
    {
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    if (!pRxLanes || !pTxLanes)
    {
        return RC::ILWALID_INPUT;
    }

    *pRxLanes = GetRegLanesRx();
    *pTxLanes = GetRegLanesTx();

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Get the detected PCIE lanes
//!
//! \param pLanes : Detected lanes
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC PcieImpl::DoGetPcieDetectedLanes(UINT32 *pLanes)
{
    GpuSubdevice* pGpuSubdev = GetDevice()->GetInterface<GpuSubdevice>();

    if (!pGpuSubdev->Regs().IsSupported(MODS_XP_PL_LANES_DETECTED_IN_DETECT))
    {
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    if (!pLanes)
    {
        return RC::ILWALID_INPUT;
    }

    *pLanes = pGpuSubdev->Regs().Read32(MODS_XP_PL_LANES_DETECTED_IN_DETECT);

    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PcieImpl::DoGetLTREnabled(bool *pEnabled)
{
    *pEnabled = false;

    RC rc;
    UINT16 linkCtrl = 0;
    CHECK_RC(ReadPciCap(LW_PCI_CAP_PCIE_CTRL2, &linkCtrl));
    *pEnabled = FLD_TEST_DRF(_PCI, _CAP_PCIE_CTRL2, _LTR, _ENABLE, linkCtrl);
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Retrieve the upstream pex device for the GPU
//!
//! \param pPexDev : Pointer to returned upstream PEX device
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC PcieImpl::DoGetUpStreamInfo(PexDevice **pPexDev)
{
    if (!pPexDev)
    {
        return RC::BAD_PARAMETER;
    }
    *pPexDev = m_UpStreamInfo.pPexDev;
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Retrieve the upstream pex device and downstream port for the GPU
//!
//! \param pPexDev : Pointer to returned upstream PEX device
//! \param pPort   : Pointer to returned downstream port index on the upstream
//!                  device to which the GPU is connected
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC PcieImpl::DoGetUpStreamInfo(PexDevice **pPexDev,
                                              UINT32* pPort)
{
    if (!pPexDev || !pPort)
    {
        return RC::BAD_PARAMETER;
    }
    *pPexDev = m_UpStreamInfo.pPexDev;
    *pPort   = m_UpStreamInfo.portIndex;

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Returns the fastest PCIE link speed supported by the downstream port
//!        of the PEX bridge/rootport immediately upstream of the GPU
//!
//! \return The fastest PCIE link speed supported by the downstream port
//!         of the PEX bridge/rootport immediately upstream of the GPU
/* virtual */ Pci::PcieLinkSpeed PcieImpl::DoGetUpstreamLinkSpeedCap()
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
/* virtual */ UINT32 PcieImpl::DoGetUpstreamLinkWidthCap()
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
RC PcieImpl::DoHasAspmDeepL1(bool *pbHasDeepL1)
{
    MASSERT(pbHasDeepL1);
    MASSERT(!"Deep L1 state query not supported on this device");
    *pbHasDeepL1 = false;
    return RC::UNSUPPORTED_HARDWARE_FEATURE;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PcieImpl::DoHasL1ClockPm(bool *pbHasL1ClockPm) const
{
    MASSERT(pbHasL1ClockPm);

    RC rc;
    UINT32 linkCap = 0;
    CHECK_RC(ReadPciCap32(LW_PCI_CAP_PCIE_LINK_CAP, &linkCap));
    *pbHasL1ClockPm = FLD_TEST_DRF_NUM(_PCI, _CAP_PCIE_LINK_CAP, _CLK_PWR_MGT, 1, linkCap);
    return RC::OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PcieImpl::DoInitialize
(
    LwLinkDevIf::LibInterface::LibDeviceHandle libHandle
)
{
    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PcieImpl::DoIsL1ClockPmEnabled(bool *pbEnabled) const
{
    MASSERT(pbEnabled);

    RC rc;
    UINT16 linkControl = 0;
    CHECK_RC(ReadPciCap(LW_PCI_CAP_PCIE_LINK_CTRL, &linkControl));
    *pbEnabled = FLD_TEST_DRF_NUM(_PCI, _CAP_PCIE_LINK_CTRL, _EN_CLK_PWR_MGT, 1, linkControl);
    return RC::OK;
}

//-----------------------------------------------------------------------------
//! \brief Returns the fastest PCIE link speed that the GPU supports
//!
//! \return The fastest PCIE link speed that the GPU supports
/* virtual */ Pci::PcieLinkSpeed PcieImpl::DoLinkSpeedCapability() const
{
    UINT16 linkStatus = 0;
    if (OK != ReadPciCap(LW_PCI_CAP_PCIE_LINK_CAP, &linkStatus))
    {
        Printf(Tee::PriError, "%s : Cannot read PCIE Link Speed!\n", __FUNCTION__);
        return Pci::SpeedUnknown;
    }

    switch (DRF_VAL(_PCI_CAP, _PCIE_LINK_CAP, _LINK_SPEED, linkStatus))
    {
        case 1:
            return Pci::Speed2500MBPS;
        case 2:
            return Pci::Speed5000MBPS;
        case 3:
            return Pci::Speed8000MBPS;
        case 4:
            return Pci::Speed16000MBPS;
        case 5:
            return Pci::Speed32000MBPS;
        default:
            Printf(Tee::PriError, "%s : Unknown PCIE link speed %d\n",
                   __FUNCTION__, DRF_VAL(_PCI_CAP, _PCIE_LINK_CAP, _LINK_SPEED, linkStatus));
            return Pci::SpeedUnknown;
    }
}

//-----------------------------------------------------------------------------
//! \brief Returns the widest PCIE link width that the GPU supports
//!
//! \return The widest PCIE link width that the GPU supports
/* virtual */ UINT32 PcieImpl::DoLinkWidthCapability()
{
    UINT16 linkStatus = 0;
    if (OK != ReadPciCap(LW_PCI_CAP_PCIE_LINK_CAP, &linkStatus))
    {
        Printf(Tee::PriError, "%s : Cannot read PCIE Link Width!\n", __FUNCTION__);
        return 0;
    }
    return DRF_VAL(_PCI_CAP, _PCIE_LINK_CAP, _LINK_WIDTH, linkStatus);
}

//-----------------------------------------------------------------------------
RC PcieImpl::DoSetAspmCyaState(UINT32 state)
{
    RegHal& regs = GetDevice()->Regs();

    UINT32 aspmCya = regs.Read32(MODS_XVE_PRIV_XV_0);

    regs.SetField(&aspmCya,
                  (state & Pci::ASPM_L0S) ? MODS_XVE_PRIV_XV_0_CYA_L0S_ENABLE_ENABLED :
                                            MODS_XVE_PRIV_XV_0_CYA_L0S_ENABLE_DISABLED);
    regs.SetField(&aspmCya,
                  (state & Pci::ASPM_L1) ? MODS_XVE_PRIV_XV_0_CYA_L1_ENABLE_ENABLED :
                                           MODS_XVE_PRIV_XV_0_CYA_L1_ENABLE_DISABLED);

    regs.Write32(MODS_XVE_PRIV_XV_0, aspmCya);

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC PcieImpl::DoSetAspmCyaL1SubState(UINT32 state)
{
    RegHal& regs = GetDevice()->Regs();
    if (!HasAspmL1Substates() || !regs.IsSupported(MODS_XP_L1_SUBSTATE_4))
        return RC::UNSUPPORTED_HARDWARE_FEATURE;

    UINT32 aspmL1SSCya = regs.Read32(MODS_XP_L1_SUBSTATE_4);

    regs.SetField(&aspmL1SSCya,
                  (state & Pci::PM_SUB_L11) ? MODS_XP_L1_SUBSTATE_4_CYA_ASPM_L1_1_ENABLED_YES :
                                              MODS_XP_L1_SUBSTATE_4_CYA_ASPM_L1_1_ENABLED_NO);
    regs.SetField(&aspmL1SSCya,
                  (state & Pci::PM_SUB_L12) ? MODS_XP_L1_SUBSTATE_4_CYA_ASPM_L1_2_ENABLED_YES :
                                              MODS_XP_L1_SUBSTATE_4_CYA_ASPM_L1_2_ENABLED_NO);

    regs.Write32(MODS_XP_L1_SUBSTATE_4, aspmL1SSCya);

    return RC::OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PcieImpl::DoEnableL1ClockPm(bool bEnable)
{
    RC rc;
    UINT16 linkControl = 0;
    CHECK_RC(ReadPciCap(LW_PCI_CAP_PCIE_LINK_CTRL, &linkControl));
    linkControl = FLD_SET_DRF_NUM(_PCI, _CAP_PCIE_LINK_CTRL, _EN_CLK_PWR_MGT,
                                  bEnable ? 1 : 0, linkControl);
    CHECK_RC(WritePciCap(LW_PCI_CAP_PCIE_LINK_CTRL, linkControl));
    return RC::OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PcieImpl::DoSetResetCrsTimeout(UINT32 timeUs)
{
    return RC::UNSUPPORTED_HARDWARE_FEATURE;
}

//-----------------------------------------------------------------------------
//! \brief Set the upstream PEX device/port to which the GPU is connected.
//!        This is called in PexDevMgr init when the topology is being detected
//!
//! \param pPexDev : Upstream PEX device
//! \param Index   : Downstream port index on the upstream device to which the
//!                  GPU is connected
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC PcieImpl::DoSetUpStreamDevice(PexDevice* pPexDev,
                                                  UINT32 Index)
{
    MASSERT(pPexDev);
    // this should prevent this function from being called twice per instance
    if (m_UpStreamInfo.pPexDev)
    {
        Printf(Tee::PriNormal, "Up stream bridge device already set\n");
        return RC::SOFTWARE_ERROR;
    }
    m_UpStreamInfo.pPexDev = pPexDev;
    m_UpStreamInfo.portIndex  = Index;
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Get the PCIE Subsystem device ID
//!
//! \return PCIE Subsystem device ID (0 if unable to retrieve)
/* virtual */ UINT32 PcieImpl::DoSubsystemDeviceId()
{
    UINT16 deviceId = 0;
    if (OK != ReadPci(LW_PCI_DEVICE_ID, &deviceId))
    {
        Printf(Tee::PriError, "%s : Cannot read PCI Subsystem Device ID!\n", __FUNCTION__);
        return 0x0;
    }
    return deviceId;
}

//-----------------------------------------------------------------------------
//! \brief Get the PCIE Subsystem vendor ID
//!
//! \return PCIE Subsystem vendor ID (0 if unable to retrieve)
/* virtual */ UINT32 PcieImpl::DoSubsystemVendorId()
{
    UINT16 svid = 0;
    if (OK != ReadPci(LW_PCI_SUBSYSTEM_VENDOR, &svid))
    {
        Printf(Tee::PriError, "%s : Cannot read PCI Subsystem Vendor ID!\n", __FUNCTION__);
        return 0x0;
    }
    return svid;
}

//-----------------------------------------------------------------------------
RC PcieImpl::ClearAerErrorStatus() const
{
    RC rc;

    UINT16 domain, bus, device, function;
    GetDevice()->GetId().GetPciDBDF(&domain, &bus, &device, &function);

    UINT32 aerCtrlReg = 0;
    CHECK_RC(Platform::PciRead32(domain, bus, device, function,
                                 GetAerOffset() + LW_PCI_CAP_AER_CTRL_OFFSET,
                                 &aerCtrlReg));
    bool multipleHeaderRecCapable = REF_VAL(LW_PCI_CAP_AER_CTRL_MUTIPLE_HEADER_REC_SUPP,
                                            aerCtrlReg);
    bool multipleHeaderRecEnabled = REF_VAL(LW_PCI_CAP_AER_CTRL_MUTIPLE_HEADER_REC_EN,
                                            aerCtrlReg);

    // Clear uncorrectable error status bits
    if (multipleHeaderRecCapable && multipleHeaderRecEnabled)
    {
        /* From PCIe Base Spec Rev 3.1, Sec 6.2.4.2 :
         * If multiple error handling is supported and enabled,
         * clear error pointed by First Error Pointer (FEP)
         * successively to get the sequence of uncorrectable errors.
         * All errors are cleared when FEP points to a reserved status bit
         */

        const UINT32 validStatusBitMask =
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_DATA_LINK_PROTOCOL)   |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_SURPRISE_DOWN)        |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_POISONED_TLP)         |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_FLOW_CTRL_PROTOCOL)   |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_COMPLETION_TIMEOUT)   |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_COMPLETER_ABORT)      |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_UNEXPECTED_COMPLETION)|
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_RECEIVER_OVERFLOW)    |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_MALFORMED_TLP)        |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_ECRC)                 |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_UNSUPPORTED_REQ)      |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_ACS_VIOLATION)        |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_UCORR_INTERNAL)       |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_MC_BLOCKED_TLP)       |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_ATOMICOP_BLOCKED)     |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_TLP_PREFIX_BLOCKED);

        while (true)
        {
            CHECK_RC(Platform::PciRead32(domain, bus, device, function,
                                         GetAerOffset() + LW_PCI_CAP_AER_CTRL_OFFSET,
                                         &aerCtrlReg));
            UINT32 fepStatusBit = REF_VAL(LW_PCI_CAP_AER_CTRL_FEP_BITS, aerCtrlReg);
            if (!(validStatusBitMask & (1U << fepStatusBit)))
            {
                break;
            }
        }
    }
    else
    {
        // Writing 1s to all the status bits clears them
        // Reserved bits are hardwired to 0, and are unaffected by this
        CHECK_RC(Platform::PciWrite32(domain, bus, device, function,
                                      GetAerOffset() + LW_PCI_CAP_AER_UNCORR_ERR_STATUS_OFFSET,
                                      ~0U));
    }

    // Clear correctable error status bits by writing 1
    // Reserved bits are hardwired to 0, so are unaffected
    CHECK_RC(Platform::PciWrite32(domain, bus, device, function,
                                  GetAerOffset() + LW_PCI_CAP_AER_CORR_ERR_STATUS_OFFSET,
                                  ~0U));

    return rc;
}

//-----------------------------------------------------------------------------
RC PcieImpl::GetDevicePtr(TestDevicePtr *ppTestDevice)
{
    TestDevice * pTestDev = GetDevice();
    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    return pTestDeviceMgr->GetDevice(pTestDev->DevInst(), ppTestDevice);
}

//-----------------------------------------------------------------------------
//! \brief Private function to et the downstream PCIE error counts from the
//!        PCIE bridge/rootport immediately above the GPU
//!
//! \param pCounts    : Pointer to host (PCIE bridge/rootport) counts.
//!                     May be NULL
//! \param CountType  : Type of errors to retrieve (flag based only, counter
//!                     based only, or both)
//!
//! \return OK if successful, not OK otherwise
RC PcieImpl::GetUpstreamErrorCounts
(
    PexErrorCounts            *pCounts,
    PexDevice::PexCounterType  countType
)
{
    RC rc;
    if (pCounts)
    {
        PexDevice *pUpStreamDev;
        UINT32 parentPort;

        CHECK_RC(GetUpStreamInfo(&pUpStreamDev, &parentPort));

        if (pUpStreamDev)
        {
            CHECK_RC(pUpStreamDev->GetDownStreamErrorCounts(pCounts,
                                                            parentPort,
                                                            countType));
        }
        else if (countType != PexDevice::PEX_COUNTER_HW)
        {
            Pci::PcieErrorsMask HostError;
            CHECK_RC(GetPolledErrors(NULL, &HostError));
            CHECK_RC(pCounts->SetCount(PexErrorCounts::CORR_ID,
                                false,
                                (HostError & Pci::CORRECTABLE_ERROR) ? 1 : 0));
            CHECK_RC(pCounts->SetCount(PexErrorCounts::NON_FATAL_ID,
                                  false,
                                  (HostError & Pci::NON_FATAL_ERROR) ? 1 : 0));
            CHECK_RC(pCounts->SetCount(PexErrorCounts::FATAL_ID,
                                  false,
                                  (HostError & Pci::FATAL_ERROR) ? 1 : 0));
            CHECK_RC(pCounts->SetCount(PexErrorCounts::UNSUP_REQ_ID,
                                  false,
                                  (HostError & Pci::UNSUPPORTED_REQ) ? 1 : 0));
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
RC PcieImpl::ReadPci(UINT32 offset, UINT16 *pVal)
{
    RC rc;

    UINT16 domain, bus, device, function;
    GetDevice()->GetId().GetPciDBDF(&domain, &bus, &device, &function);
    CHECK_RC(Platform::PciRead16(domain, bus, device, function, offset, pVal));

    return rc;
}

//------------------------------------------------------------------------------
RC PcieImpl::ReadPciCap(UINT32 offset, UINT16 *pVal) const
{
    RC rc;

    UINT16 domain, bus, device, function;
    GetDevice()->GetId().GetPciDBDF(&domain, &bus, &device, &function);

    UINT08 capPtr;
    CHECK_RC(Pci::FindCapBase(domain,
                              bus,
                              device,
                              function,
                              PCI_CAP_ID_PCIE,
                              &capPtr));
    CHECK_RC(Platform::PciRead16(domain, bus, device, function, capPtr+offset, pVal));

    return rc;
}

//------------------------------------------------------------------------------
RC PcieImpl::ReadPciCap32(UINT32 offset, UINT32 *pVal) const
{
    RC rc;

    UINT16 domain, bus, device, function;
    GetDevice()->GetId().GetPciDBDF(&domain, &bus, &device, &function);

    UINT08 capPtr;
    CHECK_RC(Pci::FindCapBase(domain,
                              bus,
                              device,
                              function,
                              PCI_CAP_ID_PCIE,
                              &capPtr));
    CHECK_RC(Platform::PciRead32(domain, bus, device, function, capPtr+offset, pVal));

    return rc;
}

//------------------------------------------------------------------------------
RC PcieImpl::WritePciCap(UINT32 offset, UINT16 val)
{
    RC rc;

    UINT16 domain, bus, device, function;
    GetDevice()->GetId().GetPciDBDF(&domain, &bus, &device, &function);

    UINT08 capPtr;
    CHECK_RC(Pci::FindCapBase(domain,
                              bus,
                              device,
                              function,
                              PCI_CAP_ID_PCIE,
                              &capPtr));
    CHECK_RC(Platform::PciWrite16(domain, bus, device, function, capPtr+offset, val));

    return rc;
}

//-----------------------------------------------------------------------------
RC PcieImpl::UpdateAerLog()
{
    RC rc;

    if (!AerEnabled())
        return OK;

    auto pPexDevMgr = static_cast<PexDevMgr*>(DevMgrMgr::d_PexDevMgr);
    const UINT32 pexVerboseFlags = pPexDevMgr->GetVerboseFlags();

    UINT16 domain, bus, device, function;
    GetDevice()->GetId().GetPciDBDF(&domain, &bus, &device, &function);

    UINT32 status = 0;
    CHECK_RC(Platform::PciRead32(domain, bus, device, function, LW_PCI_CAP_DEVICE_CTL, &status));

    constexpr UINT32 ILWALID_STATUS_REG = 0xFFFFFFFFU;
    if (status == ILWALID_STATUS_REG)
        return OK;

    const Pci::PcieErrorsMask errMask = REF_VAL(LW_PCI_CAP_DEVICE_CTL_ERROR_BITS, status);

    if ((errMask != 0) &&
        !(pexVerboseFlags & (1 << PexDevMgr::PEX_OFFICIAL_MODS_VERBOSITY)) &&
         (pexVerboseFlags & (1 << PexDevMgr::PEX_LOG_AER_DATA)))
    {
        PexDevice::AERLogEntry aerEntry;
        aerEntry.errMask = errMask;

        CHECK_RC(Pci::ReadExtendedCapBlock(domain, bus, device, function,
                                           Pci::AER_ECI, &aerEntry.aerData));

        UINT32 aerLogMaxEntries = 0;
        CHECK_RC(PexDevice::GetAerLogMaxEntries(&aerLogMaxEntries));
        UINT32 onAerLogFull = 0;
        CHECK_RC(PexDevice::GetOnAerLogFull(&onAerLogFull));

        if (m_AerLog.size() == aerLogMaxEntries &&
            (onAerLogFull == PexDevice::AER_LOG_DELETE_OLD))
        {
            m_AerLog.pop_front();
        }

        if (m_AerLog.size() < aerLogMaxEntries)
        {
            m_AerLog.push_back(aerEntry);
        }
    }

    if (errMask)
    {
        status |= DRF_SHIFTMASK(LW_PCI_CAP_DEVICE_CTL_ERROR_BITS);
        CHECK_RC(Platform::PciWrite32(domain, bus, device, function, LW_PCI_CAP_DEVICE_CTL, status));

        CHECK_RC(ClearAerErrorStatus());
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC PcieImpl::GetAtomicTypesSupported(UINT32 * pAtomicTypes)
{
    MASSERT(pAtomicTypes);

    RC rc;
    *pAtomicTypes = 0;

    // This implementation is a mirror of what the kernel driver does on linux
    // when inserted into the linux kernel
    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        PexDevice *pUpStreamDev = nullptr;
        PexDevice::PciDevice * pPciDev;
        UINT32 parentPort;
        CHECK_RC(GetUpStreamInfo(&pUpStreamDev, &parentPort));
        while (pUpStreamDev)
        {
            if (pUpStreamDev->IsRoot())
            {
                CHECK_RC(pUpStreamDev->GetDownstreamAtomicsEnabledMask(parentPort,
                                                                       pAtomicTypes));
            }
            else
            {
                bool bRoutingCapable;

                CHECK_RC(pUpStreamDev->GetDownstreamAtomicRoutingCapable(parentPort,
                                                                         &bRoutingCapable));
                if (!bRoutingCapable)
                {
                    pPciDev = pUpStreamDev->GetDownStreamPort(parentPort);
                    Printf(Tee::PriLow,
                           "Downstream port at %04x:%02x:%02x.%x will not route atomics\n",
                           pPciDev->Domain,
                           pPciDev->Bus,
                           pPciDev->Dev,
                           pPciDev->Func);

                    return RC::OK;
                }

                CHECK_RC(pUpStreamDev->GetUpstreamAtomicRoutingCapable(&bRoutingCapable));
                if (!bRoutingCapable)
                {
                    pPciDev = pUpStreamDev->GetUpStreamPort();
                    Printf(Tee::PriLow,
                           "Upstream port at %04x:%02x:%02x.%x will not route atomics\n",
                           pPciDev->Domain,
                           pPciDev->Bus,
                           pPciDev->Dev,
                           pPciDev->Func);
                    return RC::OK;
                }
                CHECK_RC(pUpStreamDev->GetUpstreamAtomicEgressBlocked(&bRoutingCapable));
                if (!bRoutingCapable)
                {
                    pPciDev = pUpStreamDev->GetUpStreamPort();
                    Printf(Tee::PriLow,
                           "Upstream port at %04x:%02x:%02x.%x blocks atomic egress\n",
                           pPciDev->Domain,
                           pPciDev->Bus,
                           pPciDev->Dev,
                           pPciDev->Func);
                    return RC::OK;
                }
            }
            parentPort   = pUpStreamDev->GetUpStreamIndex();
            pUpStreamDev = pUpStreamDev->GetUpStreamDev();
        }
    }
    else
    {
        *pAtomicTypes = Pci::ATOMIC_32BIT | Pci::ATOMIC_64BIT | Pci::ATOMIC_128BIT;
    }

    return RC::OK;
}
