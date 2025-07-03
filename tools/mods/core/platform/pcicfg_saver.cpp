/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/pcicfg_saver.h"
#include "core/include/pci.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "lwmisc.h"

//------------------------------------------------------------------------------
PciCfgSpaceSaver::PciCfgSpaceSaver
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    UINT08 pcieCapBase
)
   : m_Domain(domain)
    ,m_Bus(bus)
    ,m_Device(device)
    ,m_Function(function)
    ,m_PcieCapBase(pcieCapBase)
{
}

//------------------------------------------------------------------------------
PciCfgSpaceSaver::~PciCfgSpaceSaver()
{
    if (m_pMsixTableBase)
    {
        Platform::UnMapDeviceMemory(m_pMsixTableBase);
        m_pMsixTableBase = nullptr;
    }
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::Save()
{
    RC rc;
    CHECK_RC(SavePciConfigSpace());
    CHECK_RC(SavePcieState());
    CHECK_RC(SavePcixState());
    CHECK_RC(SaveLtrState());
    CHECK_RC(SaveDpcState());
    CHECK_RC(SaveAerState());
    CHECK_RC(SaveAllVcStates());
    CHECK_RC(SavePasidState());
    CHECK_RC(SavePriState());
    CHECK_RC(SaveAtsState());
    CHECK_RC(SaveResizableBarState());
    CHECK_RC(SaveAcsState());
    CHECK_RC(SaveMsiState());
    CHECK_RC(SaveIovState());
    return rc;
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::Restore()
{
    StickyRC rc;
    rc = RestoreLtrState();
    rc = RestorePcieState();
    rc = RestorePasidState();
    rc = RestorePriState();
    rc = RestoreAtsState();
    rc = RestoreAllVcState();
    rc = RestoreResizableBarState();
    rc = RestoreDpcState();
    rc = RestoreAerState();
    rc = RestorePciConfigSpace();
    rc = RestorePcixState();
    rc = RestoreMsiState();
    rc = RestoreAcsState();
    rc = RestoreIovState();

    UINT16 temp;
    if (Platform::PciRead16(m_Domain, m_Bus, m_Device, m_Function,
                            LW_PCI_COMMAND, &temp) == RC::OK)
    {
        const UINT16 intxmask = DRF_SHIFTMASK(LW_PCI_COMMAND_INTX_DISABLE);
        if (FLD_TEST_DRF_NUM(_PCI, _COMMAND, _INTX_DISABLE, 1, m_PciConfigSpace[1].data))
            temp |= intxmask;
        else
            temp &= ~intxmask;
        rc = Platform::PciWrite16(m_Domain, m_Bus, m_Device, m_Function, LW_PCI_COMMAND, temp);
    }
    return rc;
}

//------------------------------------------------------------------------------
// Base PCI Capability
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::SavePcieState()
{
    RC rc;
    CHECK_RC(SaveConfigReg(&m_PciPcieState, m_PcieCapBase + LW_PCI_CAP_DEVICE_CTL,      ConfigRegType::WORD));
    CHECK_RC(SaveConfigReg(&m_PciPcieState, m_PcieCapBase + LW_PCI_CAP_PCIE_LINK_CTRL,  ConfigRegType::WORD));
    CHECK_RC(SaveConfigReg(&m_PciPcieState, m_PcieCapBase + LW_PCI_CAP_PCIE_SLOT_CTRL,  ConfigRegType::WORD));
    CHECK_RC(SaveConfigReg(&m_PciPcieState, m_PcieCapBase + LW_PCI_CAP_PCIE_ROOT_CTRL,  ConfigRegType::WORD));
    CHECK_RC(SaveConfigReg(&m_PciPcieState, m_PcieCapBase + LW_PCI_CAP_PCIE_CTRL2,      ConfigRegType::WORD));
    CHECK_RC(SaveConfigReg(&m_PciPcieState, m_PcieCapBase + LW_PCI_CAP_PCIE_LINK_CTRL2, ConfigRegType::WORD));
    CHECK_RC(SaveConfigReg(&m_PciPcieState, m_PcieCapBase + LW_PCI_CAP_PCIE_SLOT_CTRL2, ConfigRegType::WORD));
    return rc;
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::RestorePcieState()
{
    return RestoreConfigRegs(m_PciPcieState);
}

//------------------------------------------------------------------------------
// PCIX Capability
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::SavePcixState()
{
    UINT08 capPtr;
    const RC rc = Pci::FindCapBase(m_Domain, m_Bus, m_Device,
                                   m_Function, PCI_CAP_ID_PCIX, &capPtr);
    if (rc == RC::OK)
    {
        return SaveConfigReg(&m_PciPcixCmd, capPtr + LW_PCI_CAP_PCIX_CMD, ConfigRegType::WORD);
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::RestorePcixState()
{
    if (m_PciPcixCmd.regType == ConfigRegType::INVALID)
        return RC::OK;
    return RestoreConfigReg(m_PciPcixCmd);
}

//------------------------------------------------------------------------------
// LTR Extended Capability
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::SaveLtrState()
{
    UINT16 extCapPtr, extCapSize;

    RC rc = Pci::GetExtendedCapInfo(m_Domain, m_Bus, m_Device, m_Function, Pci::LTR_ECI,
                                    m_PcieCapBase, &extCapPtr, &extCapSize);
    if (rc == RC::OK)
    {
        CHECK_RC(SaveConfigReg(&m_PciLtrState,
                               extCapPtr + LW_PCI_CAP_LTR_MAX_SNOOP_LAT,
                               ConfigRegType::WORD));
        CHECK_RC(SaveConfigReg(&m_PciLtrState,
                               extCapPtr + LW_PCI_CAP_LTR_MAX_NOSNOOP_LAT,
                               ConfigRegType::WORD));
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::RestoreLtrState()
{
    if (m_PciLtrState.empty())
        return RC::OK;
    return RestoreConfigRegs(m_PciLtrState);
}

//------------------------------------------------------------------------------
// DPC Extended Capability
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::SaveDpcState()
{
    UINT16 extCapPtr, extCapSize;
    const RC rc = Pci::GetExtendedCapInfo(m_Domain, m_Bus, m_Device, m_Function, Pci::DPC_ECI,
                                          m_PcieCapBase, &extCapPtr, &extCapSize);
    if (rc == RC::OK)
    {
        return SaveConfigReg(&m_PciDpcCtrl, extCapPtr + LW_PCI_DPC_CTRL, ConfigRegType::WORD);
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::RestoreDpcState()
{
    if (m_PciDpcCtrl.regType == ConfigRegType::INVALID)
        return RC::OK;
    return RestoreConfigReg(m_PciDpcCtrl);
}

//------------------------------------------------------------------------------
// AER Extended Capability
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::SaveAerState()
{
    UINT16 extCapPtr, extCapSize;
    RC rc = Pci::GetExtendedCapInfo(m_Domain, m_Bus, m_Device, m_Function, Pci::AER_ECI,
                                    m_PcieCapBase, &extCapPtr, &extCapSize);
    if (rc == RC::OK)
    {
        CHECK_RC(SaveConfigReg(&m_PciAerState,
                               extCapPtr + LW_PCI_CAP_AER_UNCORR_MASK,
                               ConfigRegType::DWORD));
        CHECK_RC(SaveConfigReg(&m_PciAerState,
                               extCapPtr + LW_PCI_CAP_AER_UNCORR_SEVERITY,
                               ConfigRegType::DWORD));
        CHECK_RC(SaveConfigReg(&m_PciAerState,
                               extCapPtr + LW_PCI_CAP_AER_CORR_MASK,
                               ConfigRegType::DWORD));
        CHECK_RC(SaveConfigReg(&m_PciAerState,
                               extCapPtr + LW_PCI_CAP_AER_CTRL_OFFSET,
                               ConfigRegType::DWORD));
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::RestoreAerState()
{
    if (m_PciAerState.empty())
        return RC::OK;

    // If AER state is non-empty then this cannot fail
    UINT16 extCapPtr, extCapSize;
    Pci::GetExtendedCapInfo(m_Domain, m_Bus, m_Device, m_Function, Pci::AER_ECI, m_PcieCapBase,
                            &extCapPtr, &extCapSize);

    StickyRC rc;
    UINT32 temp = 0;

    // Clear AER errors before restoring
    if (RC::OK == Platform::PciRead32(m_Domain, m_Bus, m_Device, m_Function,
                                      extCapPtr + LW_PCI_CAP_AER_UNCORR_ERR_STATUS_OFFSET,
                                      &temp))
    {
        rc = Platform::PciWrite32(m_Domain, m_Bus, m_Device, m_Function,
                                  extCapPtr + LW_PCI_CAP_AER_UNCORR_ERR_STATUS_OFFSET,
                                  temp);
    }
    if (RC::OK == Platform::PciRead32(m_Domain, m_Bus, m_Device, m_Function,
                                      extCapPtr + LW_PCI_CAP_AER_CORR_ERR_STATUS_OFFSET,
                                      &temp))
    {
        rc = Platform::PciWrite32(m_Domain, m_Bus, m_Device, m_Function,
                                  extCapPtr + LW_PCI_CAP_AER_CORR_ERR_STATUS_OFFSET,
                                  temp);
    }
    rc = RestoreConfigRegs(m_PciAerState);
    return rc;
}

//------------------------------------------------------------------------------
// Virtual Channel Extended Capabilities
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::SaveAllVcStates()
{
    UINT16 extCapPtr, extCapSize;

    RC rc;
    if (Pci::GetExtendedCapInfo(m_Domain, m_Bus, m_Device, m_Function, Pci::VC_ECI, m_PcieCapBase,
                                &extCapPtr, &extCapSize) == RC::OK)
    {
        CHECK_RC(VcExtCapSaveRestore(m_PciVcState, extCapPtr, true));
    }

    if (Pci::GetExtendedCapInfo(m_Domain, m_Bus, m_Device, m_Function, Pci::MFVC_ECI,
                                m_PcieCapBase, &extCapPtr, &extCapSize) == RC::OK)
    {
        CHECK_RC(VcExtCapSaveRestore(m_PciMfvcState, extCapPtr, true));
    }

    if (Pci::GetExtendedCapInfo(m_Domain, m_Bus, m_Device, m_Function, Pci::VC9_ECI,
                                m_PcieCapBase, &extCapPtr, &extCapSize) == RC::OK)
    {
        CHECK_RC(VcExtCapSaveRestore(m_PciVc9State, extCapPtr, true));
    }
    return rc;
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::RestoreAllVcState()
{
    StickyRC rc;

    UINT16 vcCapBase, vcCapSize;
    if (!m_PciVcState.empty())
    {
        Pci::GetExtendedCapInfo(m_Domain, m_Bus, m_Device, m_Function, Pci::VC_ECI,
                                m_PcieCapBase, &vcCapBase, &vcCapSize);
        rc = VcExtCapSaveRestore(m_PciVcState, vcCapBase, false);
    }
    if (!m_PciMfvcState.empty())
    {
        Pci::GetExtendedCapInfo(m_Domain, m_Bus, m_Device, m_Function, Pci::MFVC_ECI,
                                m_PcieCapBase, &vcCapBase, &vcCapSize);
        rc = VcExtCapSaveRestore(m_PciMfvcState, vcCapBase, false);
    }
    if (!m_PciVc9State.empty())
    {
        Pci::GetExtendedCapInfo(m_Domain, m_Bus, m_Device, m_Function, Pci::VC9_ECI,
                                m_PcieCapBase, &vcCapBase, &vcCapSize);
        rc = VcExtCapSaveRestore(m_PciVc9State, vcCapBase, false);
    }
    return rc;
}
//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::VcLoadArbTable(UINT32 vcCapBase)
{
    RC rc;
    UINT16 portCtrl = 0;

    CHECK_RC(Platform::PciRead16(m_Domain, m_Bus, m_Device, m_Function,
                                 vcCapBase + LW_PCI_CAP_VC_PORT_CTRL,
                                 &portCtrl));
    portCtrl = FLD_SET_DRF_NUM(_PCI, _CAP_VC_PORT_CTRL, _LOAD_TABLE, 1, portCtrl);
    CHECK_RC(Platform::PciWrite16(m_Domain, m_Bus, m_Device, m_Function,
                                  vcCapBase + LW_PCI_CAP_VC_PORT_CTRL,
                                  portCtrl));

    RC pollRc;
    rc = Tasker::Poll(Tasker::GetDefaultTimeoutMs(), [&]()->bool
    {
        pollRc = Platform::PciRead16(m_Domain, m_Bus, m_Device, m_Function,
                                     vcCapBase + LW_PCI_CAP_VC_PORT_CTRL,
                                     &portCtrl);
        bool bLoaded = FLD_TEST_DRF_NUM(_PCI, _CAP_VC_PORT_CTRL, _LOAD_TABLE, 0, portCtrl);
        return (pollRc != RC::OK) || bLoaded;
    });
    if (rc == RC::OK)
        return pollRc;
    return rc;
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::VcLoadPortArbTable(UINT32 ctrl)
{
    RC rc;
    UINT32 resCtrl = 0;

    CHECK_RC(Platform::PciRead32(m_Domain, m_Bus, m_Device, m_Function, ctrl, &resCtrl));
    resCtrl = FLD_SET_DRF_NUM(_PCI, _CAP_VC_RES_CTRL, _LOAD_TABLE, 1, resCtrl);
    CHECK_RC(Platform::PciWrite32(m_Domain, m_Bus, m_Device, m_Function, ctrl, resCtrl));

    UINT32 status = ctrl + (LW_PCI_CAP_VC_RES_STATUS - LW_PCI_CAP_VC_RES_CTRL);
    RC pollRc;
    rc = Tasker::Poll(Tasker::GetDefaultTimeoutMs(), [&]()->bool
    {
        UINT32 resStatus = 0;
        pollRc = Platform::PciRead32(m_Domain, m_Bus, m_Device, m_Function, status, &resStatus);
        bool bLoaded = FLD_TEST_DRF_NUM(_PCI, _CAP_VC_RES_STATUS, _TABLE, 0, resStatus);
        return (pollRc != RC::OK) || bLoaded;
    });
    if (rc == RC::OK)
        return pollRc;
    return rc;
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::VcExtCapSaveRestore
(
    vector<PciRegister> & vcSavedRegs,
    UINT16                vcCapBase,
    bool                  bSave
)
{
    StickyRC rc;
    UINT32 cap1 = 0;
    CHECK_RC(Platform::PciRead32(m_Domain, m_Bus, m_Device, m_Function,
                                 vcCapBase + LW_PCI_CAP_VC_CAP1, &cap1));

    const UINT32 extVcCount = DRF_VAL(_PCI, _CAP_VC_CAP1, _EVC_COUNT, cap1);
    const UINT32 lpExtVcCount = DRF_VAL(_PCI, _CAP_VC_CAP1, _LPEVC_COUNT, cap1);
    const UINT32 arbSize = 1 << DRF_VAL(_PCI, _CAP_VC_CAP1, _ARB_SIZE, cap1);

    UINT32 lwrIdx = 0;

    // Port VC Control Register contains VC Arbitration Select, which
    // cannot be modified when more than one LPVC is in operation.  We
    // therefore save/restore it first, as only VC0 should be enabled
    // after m_Device reset.
    if (bSave)
    {
        PciRegister pciReg;
        CHECK_RC(SaveConfigReg(&pciReg, vcCapBase + LW_PCI_CAP_VC_PORT_CTRL, ConfigRegType::WORD));
        vcSavedRegs.push_back(pciReg);
    }
    else
    {
        rc = RestoreConfigRegs(vcSavedRegs, lwrIdx, 1);
        lwrIdx++;
    }

    // If we have any Low Priority VCs and a VC Arbitration Table Offset
    // in Port VC Capability Register 2 then save/restore it next.
    if (lpExtVcCount)
    {
        UINT32 cap2 = 0;
        UINT32 vcArbOffset;

        const RC tempRc = Platform::PciRead32(m_Domain, m_Bus, m_Device, m_Function,
                                              vcCapBase + LW_PCI_CAP_VC_CAP2, &cap2);
        if (bSave && tempRc != RC::OK)
            return tempRc;
        rc = tempRc;

        if (tempRc == RC::OK)
        {
            vcArbOffset = DRF_VAL(_PCI, _CAP_VC_CAP2, _ARB_OFF_QWORD, cap2) * 16;

            if (vcArbOffset)
            {
                UINT32 size;
                UINT32 vcArbPhases = 0;

                if (FLD_TEST_DRF_NUM(_PCI, _CAP_VC_CAP2, _128_PHASE, 1, cap2))
                    vcArbPhases = 128;
                else if (FLD_TEST_DRF_NUM(_PCI, _CAP_VC_CAP2, _64_PHASE, 1, cap2))
                    vcArbPhases = 64;
                else if (FLD_TEST_DRF_NUM(_PCI, _CAP_VC_CAP2, _32_PHASE, 1, cap2))
                    vcArbPhases = 32;

                // Fixed 4 bits per phase per lpevcc (plus VC0)
                size = ((lpExtVcCount + 1) * vcArbPhases * 4) / 8;

                if (size)
                {
                    if (bSave)
                    {
                        CHECK_RC(SaveConfigRegs(vcCapBase + vcArbOffset, size / 4, &vcSavedRegs));
                    }
                    else
                    {
                        rc = RestoreConfigRegs(vcSavedRegs, lwrIdx, size / 4);
                        lwrIdx += size / 4;
                        // On restore, we need to signal hardware to re-load the VC Arbitration
                        // Table.
                        rc = VcLoadArbTable(vcCapBase);
                    }
                }
            }
        }
    }

    // In addition to each VC Resource Control Register, we may have a
    // Port Arbitration Table attached to each VC.  The Port Arbitration
    // Table Offset in each VC Resource Capability Register tells us if
    // it exists.  The entry size is global from the Port VC Capability
    // Register1 above.  The number of phases is determined per VC.
    for (UINT32 i = 0; i < extVcCount + 1; i++)
    {
        UINT32 resCap = 0;
        UINT32 vcArbOffset;

        const RC tempRc = Platform::PciRead32(m_Domain, m_Bus, m_Device, m_Function,
                            vcCapBase + LW_PCI_CAP_VC_RES_CAP + (i * LW_PCI_CAP_VC_PER_VC_STRIDE),
                            &resCap);

        if (bSave && tempRc != RC::OK)
            return tempRc;
        rc = tempRc;

        if (tempRc != RC::OK)
            continue;

        vcArbOffset = DRF_VAL(_PCI, _CAP_VC_RES_CAP, _ARB_OFF_QWORD, resCap) * 16;

        if (vcArbOffset)
        {
            UINT32 size;
            UINT32 vcArbPhases = 0;

            if (FLD_TEST_DRF_NUM(_PCI, _CAP_VC_RES_CAP, _256_PHASE, 1, resCap))
                vcArbPhases = 128;
            if ((FLD_TEST_DRF_NUM(_PCI, _CAP_VC_RES_CAP, _128_PHASE, 1, resCap)) ||
                (FLD_TEST_DRF_NUM(_PCI, _CAP_VC_RES_CAP, _128_PHASE_TB, 1, resCap)))
                vcArbPhases = 128;
            else if (FLD_TEST_DRF_NUM(_PCI, _CAP_VC_RES_CAP, _64_PHASE, 1, resCap))
                vcArbPhases = 64;
            else if (FLD_TEST_DRF_NUM(_PCI, _CAP_VC_RES_CAP, _32_PHASE, 1, resCap))
                vcArbPhases = 32;

            size = (arbSize * vcArbPhases) / 8;

            if (size)
            {
                if (bSave)
                {
                    CHECK_RC(SaveConfigRegs(vcCapBase + vcArbOffset, size / 4, &vcSavedRegs));
                }
                else
                {
                    rc = RestoreConfigRegs(vcSavedRegs, lwrIdx, size / 4);
                    lwrIdx += size / 4;
                }
            }
        }

        // VC Resource Control Register
        int ctrlPos = vcCapBase + LW_PCI_CAP_VC_RES_CTRL +
                      (i * LW_PCI_CAP_VC_PER_VC_STRIDE);
        if (bSave)
        {
            CHECK_RC(SaveConfigRegs(ctrlPos, 1, &vcSavedRegs));
        }
        else
        {
            UINT32 resCtrl = 0;
            const RC tempRc2 = Platform::PciRead32(m_Domain, m_Bus, m_Device, m_Function,
                                                   ctrlPos, &resCtrl);
            rc = tempRc2;

            if (tempRc2 == RC::OK)
            {
                const UINT32 enableMask = DRF_SHIFTMASK(LW_PCI_CAP_VC_RES_CTRL_ENABLE);
                resCtrl &= enableMask;
                resCtrl |= vcSavedRegs[lwrIdx].data & ~enableMask;
                rc = Platform::PciWrite32(m_Domain, m_Bus, m_Device, m_Function, ctrlPos,
                                          resCtrl);
                lwrIdx++;

                // Load port arbitration table if used
                if (DRF_VAL(_PCI, _CAP_VC_RES_CTRL, _ARB_SELECT, vcSavedRegs[lwrIdx].data) != 0)
                {
                    rc = VcLoadPortArbTable(resCtrl);
                }
            }

        }
    }

    return rc;
}

//------------------------------------------------------------------------------
// PASID Extended Capability
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::SavePasidState()
{
    UINT16 extCapPtr, extCapSize;
    RC rc = Pci::GetExtendedCapInfo(m_Domain, m_Bus, m_Device, m_Function, Pci::PASID_ECI,
                                    m_PcieCapBase, &extCapPtr, &extCapSize);
    if (rc == RC::OK)
    {
        UINT16 temp16 = 0;
        CHECK_RC(Platform::PciRead16(m_Domain, m_Bus, m_Device, m_Function,
                                     extCapPtr + LW_PCI_CAP_PASID_CTRL,
                                     &temp16));
        if (FLD_TEST_DRF_NUM(_PCI, _CAP_PASID_CTRL, _ENABLE, 1, temp16))
        {
            CHECK_RC(SaveConfigReg(&m_PciPasidCtrl, extCapPtr + LW_PCI_CAP_PASID_CTRL,
                                   ConfigRegType::WORD));

            m_PciPasidCtrl.data &= (DRF_SHIFTMASK(LW_PCI_CAP_PASID_CTRL_ENABLE) |
                                    DRF_SHIFTMASK(LW_PCI_CAP_PASID_CTRL_EXEC) |
                                    DRF_SHIFTMASK(LW_PCI_CAP_PASID_CTRL_PRIV));
        }
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::RestorePasidState()
{
    if (m_PciPasidCtrl.regType == ConfigRegType::INVALID)
        return RC::OK;
    return RestoreConfigReg(m_PciPasidCtrl);
}

//------------------------------------------------------------------------------
// PRI Extended Capability
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::SavePriState()
{
    UINT16 extCapPtr, extCapSize;
    RC rc = Pci::GetExtendedCapInfo(m_Domain, m_Bus, m_Device, m_Function, Pci::PRI_ECI,
                                    m_PcieCapBase, &extCapPtr, &extCapSize);
    if (rc == RC::OK)
    {
        UINT16 temp16 = 0;
        CHECK_RC(Platform::PciRead16(m_Domain, m_Bus, m_Device, m_Function,
                                     extCapPtr + LW_PCI_CAP_PRI_CTRL,
                                     &temp16));
        if (FLD_TEST_DRF_NUM(_PCI, _CAP_PRI_CTRL, _ENABLE, 1, temp16))
        {
            CHECK_RC(SaveConfigReg(&m_PciPriState, extCapPtr + LW_PCI_CAP_PRI_ALLOC_REQ,
                                   ConfigRegType::DWORD));
            CHECK_RC(SaveConfigReg(&m_PciPriState, extCapPtr + LW_PCI_CAP_PRI_CTRL,
                                   ConfigRegType::WORD));
        }
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::RestorePriState()
{
    if (m_PciPriState.empty())
        return RC::OK;
    return RestoreConfigRegs(m_PciPriState);
}

//------------------------------------------------------------------------------
// ATS Extended Capability
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::SaveAtsState()
{
    UINT16 extCapPtr, extCapSize;
    RC rc = Pci::GetExtendedCapInfo(m_Domain, m_Bus, m_Device, m_Function, Pci::ATS_ECI,
                                    m_PcieCapBase, &extCapPtr, &extCapSize);
    if (rc == RC::OK)
    {
        UINT16 temp16 = 0;
        CHECK_RC(Platform::PciRead16(m_Domain, m_Bus, m_Device, m_Function,
                                     extCapPtr + LW_PCI_CAP_PRI_CTRL,
                                     &temp16));
        if (FLD_TEST_DRF_NUM(_PCI, _CAP_PRI_CTRL, _ENABLE, 1, temp16))
        {
            CHECK_RC(SaveConfigReg(&m_PciAtsCtrl, extCapPtr + LW_PCI_CAP_ATS_CTRL,
                                   ConfigRegType::WORD));
        }
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::RestoreAtsState()
{
    if (m_PciAtsCtrl.regType == ConfigRegType::INVALID)
        return RC::OK;
    return RestoreConfigReg(m_PciAtsCtrl);
}

//------------------------------------------------------------------------------
// Resizable BAR Extended Capability
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::SaveResizableBarState()
{
    UINT16 extCapPtr, extCapSize;

    RC rc = Pci::GetExtendedCapInfo(m_Domain, m_Bus, m_Device, m_Function, Pci::RESIZABLE_BAR_ECI,
                                    m_PcieCapBase, &extCapPtr, &extCapSize);

    if (rc == RC::OK)
    {
        UINT16 temp16 = 0;
        CHECK_RC(Platform::PciRead16(m_Domain, m_Bus, m_Device, m_Function,
                                     extCapPtr + LW_PCI_CAP_RESIZABLE_BAR_CTRL,
                                     &temp16));
        const UINT32 numBars = DRF_VAL(_PCI, _CAP_RESIZABLE_BAR_CTRL, _NUM_RESIZABLE_BARS,
                                       temp16);
        for (UINT32 lwrBar = 0;
              lwrBar < numBars;
              lwrBar++, extCapPtr += LW_PCI_CAP_RESIZABLE_BAR_STRIDE)
        {
            CHECK_RC(SaveConfigReg(&m_PciResizableBarState,
                                   extCapPtr + LW_PCI_CAP_RESIZABLE_BAR_CTRL,
                                   ConfigRegType::DWORD));
        }
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::RestoreResizableBarState()
{
    if (m_PciResizableBarState.empty())
        return RC::OK;

    StickyRC rc;
    const UINT32 barSizeMask = DRF_SHIFTMASK(LW_PCI_CAP_RESIZABLE_BAR_CTRL_BAR_SIZE);
    for (auto lwrSavedBarReg : m_PciResizableBarState)
    {
        UINT32 lwrBarVal = 0;
        if (RC::OK == Platform::PciRead32(m_Domain, m_Bus, m_Device, m_Function,
                                          lwrSavedBarReg.address, &lwrBarVal))
        {
            UINT32 newBarVal = lwrBarVal & ~barSizeMask;
            newBarVal |= (lwrSavedBarReg.data & barSizeMask);
            rc = Platform::PciWrite32(m_Domain, m_Bus, m_Device, m_Function,
                                      lwrSavedBarReg.address, newBarVal);
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
// ACS Extended Capability
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::SaveAcsState()
{
    UINT16 extCapPtr, extCapSize;
    RC rc = Pci::GetExtendedCapInfo(m_Domain, m_Bus, m_Device, m_Function, Pci::ACS_ECI,
                                    m_PcieCapBase, &extCapPtr, &extCapSize);

    if (rc == RC::OK)
    {
        CHECK_RC(SaveConfigReg(&m_PciAcsCtrl, extCapPtr + LW_PCI_CAP_ACS_CTRL,
                               ConfigRegType::WORD));
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::RestoreAcsState()
{
    if (m_PciAcsCtrl.regType == ConfigRegType::INVALID)
        return RC::OK;
    return RestoreConfigReg(m_PciAcsCtrl);
}

//------------------------------------------------------------------------------
// MSI and MSI-X Capabilities
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::SaveMsiState()
{
    RC rc;
    UINT08 capPtr;
    if (RC::OK == Pci::FindCapBase(m_Domain, m_Bus, m_Device, m_Function, PCI_CAP_ID_MSI, &capPtr))
    {
        UINT16 temp = 0;
        CHECK_RC(Platform::PciRead16(m_Domain, m_Bus, m_Device, m_Function,
                                     capPtr + LW_PCI_CAP_MSI_CTRL, &temp));
        if (FLD_TEST_DRF_NUM(_PCI, _CAP_MSI_CTRL, _ENABLE, 1, temp))
        {
            CHECK_RC(SaveConfigReg(&m_PciMsiState, capPtr + LW_PCI_CAP_MSI_CTRL,
                                   ConfigRegType::WORD));
            if (FLD_TEST_DRF_NUM(_PCI, _CAP_MSI_CTRL, _64BIT, 1, temp))
            {
                CHECK_RC(SaveConfigReg(&m_PciMsiState, capPtr + LW_PCI_CAP_MSI_MASK_64BIT,
                                       ConfigRegType::DWORD));
            }
            else
            {
                CHECK_RC(SaveConfigReg(&m_PciMsiState, capPtr + LW_PCI_CAP_MSI_MASK_32BIT,
                                       ConfigRegType::DWORD));
            }
        }
    }

    if (RC::OK == Pci::FindCapBase(m_Domain, m_Bus, m_Device, m_Function, PCI_CAP_ID_MSIX,
                                   &capPtr))
    {
        UINT16 temp = 0;
        CHECK_RC(Platform::PciRead16(m_Domain, m_Bus, m_Device, m_Function,
                                     capPtr + LW_PCI_CAP_MSIX_CTRL, &temp));
        if (FLD_TEST_DRF_NUM(_PCI, _CAP_MSIX_CTRL, _ENABLE, 1, temp))
        {
            CHECK_RC(SaveConfigReg(&m_PciMsixCtrl, capPtr + LW_PCI_CAP_MSIX_CTRL,
                                   ConfigRegType::WORD));

            UINT32 tableData = 0;
            CHECK_RC(Platform::PciRead32(m_Domain, m_Bus, m_Device, m_Function,
                                         capPtr + LW_PCI_CAP_MSIX_TABLE, &tableData));

            // Find the physical device of the MSIx table in the system, map the
            // memory and save mask state of each entry in the table
            UINT32 tableBarIndexReg = 0;
            switch (DRF_VAL(_PCI, _CAP_MSIX_TABLE, _BIR, tableData))
            {
                case LW_PCI_CAP_MSIX_TABLE_BIR_10H: tableBarIndexReg = 0x10; break;
                case LW_PCI_CAP_MSIX_TABLE_BIR_14H: tableBarIndexReg = 0x14; break;
                case LW_PCI_CAP_MSIX_TABLE_BIR_18H: tableBarIndexReg = 0x18; break;
                case LW_PCI_CAP_MSIX_TABLE_BIR_1CH: tableBarIndexReg = 0x1C; break;
                case LW_PCI_CAP_MSIX_TABLE_BIR_20H: tableBarIndexReg = 0x20; break;
                case LW_PCI_CAP_MSIX_TABLE_BIR_24H: tableBarIndexReg = 0x24; break;
                default:
                    MASSERT(!"Unknown MSIX table Bar index register");
                    return RC::SOFTWARE_ERROR;
            }

            UINT32 temp32 = 0;
            CHECK_RC(Platform::PciRead32(m_Domain, m_Bus, m_Device, m_Function,
                                         tableBarIndexReg, &temp32));

            PHYSADDR physTableBase = temp32 & 0xFFFFFFF0;
            if (FLD_TEST_DRF(_PCI, _BAR_REG, _ADDR_TYPE, _64BIT, temp32))
            {
                CHECK_RC(Platform::PciRead32(m_Domain, m_Bus, m_Device, m_Function,
                                             tableBarIndexReg + 4, &temp32));
                physTableBase |= static_cast<PHYSADDR>(temp32) << 32;
            }

            physTableBase += (tableData & DRF_SHIFTMASK(LW_PCI_CAP_MSIX_TABLE_BIR));

            const UINT32 tableSize = DRF_VAL(_PCI, _CAP_MSIX_CTRL, _TABLE_SIZE,
                                             m_PciMsixCtrl.data);
            CHECK_RC(Platform::MapDeviceMemory(static_cast<void **>(&m_pMsixTableBase),
                                               physTableBase,
                                               tableSize * LW_PCI_MSIX_TABLE_ENTRY_SIZE,
                                               Memory::WC,
                                               Memory::ReadWrite));
            for (UINT32 idx = 0; idx < tableSize; idx++)
            {
                const UINT32 ctrlMask =
                    MEM_RD32(static_cast<UINT08 *>(m_pMsixTableBase) +
                             LW_PCI_MSIX_TABLE_CONTROL(idx));
                m_MsixVectorMasked.push_back(FLD_TEST_DRF(_PCI,
                                                          _MSIX_TABLE_CONTROL,
                                                          _MASK,
                                                          _ENABLED,
                                                          ctrlMask));
            }
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::RestoreMsiState()
{
    StickyRC rc;
    RC tempRc;
    if (!m_PciMsiState.empty())
    {
        UINT16 temp = 0;
        rc = Platform::PciRead16(m_Domain, m_Bus, m_Device, m_Function,
                                 LW_PCI_COMMAND, &temp);
        if (rc == RC::OK)
        {
            temp = FLD_SET_DRF_NUM(_PCI, _COMMAND, _INTX_DISABLE, 1, temp);
            rc = Platform::PciWrite16(m_Domain, m_Bus, m_Device, m_Function,
                                      LW_PCI_COMMAND, temp);
        }

        // Index 0 is the control register
        tempRc = Platform::PciRead16(m_Domain, m_Bus, m_Device, m_Function,
                                     m_PciMsiState[0].address, &temp);
        if (tempRc == RC::OK)
        {
            temp = FLD_SET_DRF_NUM(_PCI, _CAP_MSI_CTRL, _ENABLE, 0, temp);
            rc = Platform::PciWrite16(m_Domain, m_Bus, m_Device, m_Function,
                                      m_PciMsiState[0].address, temp);
        }
        if (FLD_TEST_DRF_NUM(_PCI, _CAP_MSI_CTRL, _PER_VEC_MASKING, 1, m_PciMsiState[0].data))
        {
            rc = RestoreConfigReg(m_PciMsiState[1]);
        }
        if (tempRc == RC::OK)
        {
            temp = FLD_SET_DRF_NUM(_PCI, _CAP_MSI_CTRL, _ENABLE, 1, temp);
            const UINT16 multMskMask = DRF_SHIFTMASK(LW_PCI_CAP_MSI_CTRL_MULT_MESSAGE);
            temp &= ~multMskMask;
            temp |= (m_PciMsiState[0].data & multMskMask);
            rc = Platform::PciWrite16(m_Domain, m_Bus, m_Device, m_Function,
                                      m_PciMsiState[0].address, temp);
        }
        rc = tempRc;
        tempRc.Clear();
    }
    if (m_PciMsixCtrl.regType != ConfigRegType::INVALID)
    {
        UINT16 temp = 0;
        tempRc = Platform::PciRead16(m_Domain, m_Bus, m_Device, m_Function,
                                     LW_PCI_COMMAND, &temp);
        if (tempRc == RC::OK)
        {
            temp = FLD_SET_DRF_NUM(_PCI, _COMMAND, _INTX_DISABLE, 1, temp);
            rc = Platform::PciWrite16(m_Domain, m_Bus, m_Device, m_Function,
                                      LW_PCI_COMMAND, temp);
        }
        rc = tempRc;
        tempRc.Clear();

        // Index 0 is the control register
        tempRc = Platform::PciRead16(m_Domain, m_Bus, m_Device, m_Function,
                                     m_PciMsixCtrl.address, &temp);
        if (tempRc == RC::OK)
        {
            temp |= DRF_NUM(_PCI, _CAP_MSIX_CTRL, _ENABLE, 1) |
                    DRF_NUM(_PCI, _CAP_MSIX_CTRL, _MASKALL, 1);
            rc = Platform::PciWrite16(m_Domain, m_Bus, m_Device, m_Function,
                                      m_PciMsixCtrl.address, temp);
        }

        UINT08 * pMsixTableBase = static_cast<UINT08 *>(m_pMsixTableBase);
        if (m_pMsixTableBase)
        {
            for (size_t idx = 0; idx < m_MsixVectorMasked.size(); idx++)
            {
                UINT32 ctrlMask = MEM_RD32(pMsixTableBase + LW_PCI_MSIX_TABLE_CONTROL(idx));
                if (m_MsixVectorMasked[idx])
                {
                    ctrlMask = FLD_SET_DRF(_PCI, _MSIX_TABLE_CONTROL, _MASK, _ENABLED, ctrlMask);
                }
                else
                {
                    ctrlMask = FLD_SET_DRF(_PCI, _MSIX_TABLE_CONTROL, _MASK, _DISABLED, ctrlMask);
                }
                MEM_WR32(pMsixTableBase + LW_PCI_MSIX_TABLE_CONTROL(idx), ctrlMask);
            }

            Platform::UnMapDeviceMemory(m_pMsixTableBase);
            m_pMsixTableBase = nullptr;
        }

        if (tempRc == RC::OK)
        {
            temp = FLD_SET_DRF_NUM(_PCI, _CAP_MSIX_CTRL, _MASKALL, 0, temp);
            rc = Platform::PciWrite16(m_Domain, m_Bus, m_Device, m_Function,
                                      m_PciMsixCtrl.address, temp);
        }
        rc = tempRc;
    }
    return rc;
}

//------------------------------------------------------------------------------
// SRIOV Capability
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::SaveIovState()
{
    UINT16 extCapPtr, extCapSize;
    RC rc = Pci::GetExtendedCapInfo(m_Domain, m_Bus, m_Device, m_Function, Pci::SRIOV,
                                    m_PcieCapBase, &extCapPtr, &extCapSize);

    if (rc == RC::OK)
    {
        CHECK_RC(SaveConfigReg(&m_PciSriovState, extCapPtr + LW_PCI_CAP_SRIOV_CTRL,
                               ConfigRegType::WORD));
        for (UINT32 barIdx = 0; barIdx < LW_PCI_CAP_NUM_BARS; barIdx++)
        {
            CHECK_RC(SaveConfigReg(&m_PciSriovState,
                                   extCapPtr + LW_PCI_CAP_SRIOV_BAR(barIdx),
                                   ConfigRegType::DWORD));
        }
        CHECK_RC(SaveConfigReg(&m_PciSriovState,
                               extCapPtr + LW_PCI_CAP_SRIOV_SYS_PAGE_SIZE, ConfigRegType::DWORD));
        CHECK_RC(SaveConfigReg(&m_PciSriovState,
                               extCapPtr + LW_PCI_CAP_SRIOV_NUM_VFS, ConfigRegType::WORD));
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::RestoreIovState()
{
    if (m_PciSriovState.empty())
        return RC::OK;

    StickyRC rc;
    UINT16 temp = 0;
    CHECK_RC(Platform::PciRead16(m_Domain, m_Bus, m_Device, m_Function,
                                 m_PciSriovState[0].address, &temp));
    if (FLD_TEST_DRF_NUM(_PCI, _CAP_SRIOV_CTRL, _VF_ENABLE, 1, temp))
        return rc;

    const UINT16 ariMask = DRF_SHIFTMASK(LW_PCI_CAP_SRIOV_CTRL_ARI);
    temp &= ~ariMask;
    temp |= (m_PciSriovState[0].data & ariMask);
    rc = Platform::PciWrite16(m_Domain, m_Bus, m_Device, m_Function,
                              m_PciSriovState[0].address, temp);

    // Restore all registers other than the control which needs to be last
    rc = RestoreConfigRegs(m_PciSriovState, 1, static_cast<UINT32>(m_PciSriovState.size()) - 1);

    // Restore control
    rc = RestoreConfigReg(m_PciSriovState[0]);

    if (FLD_TEST_DRF_NUM(_PCI, _CAP_SRIOV_CTRL, _VF_ENABLE, 1, m_PciSriovState[0].data))
    {
        const UINT32 sleepTime =
            (Platform::GetSimulationMode() == Platform::Hardware) ? 100000 : 20;
        Platform::SleepUS(sleepTime);
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
// Save/Restore helper functions
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::SaveConfigReg
(
    vector<PciRegister> * pPciRegisters,
    UINT32                address,
    ConfigRegType         regType
)
{
    PciRegister pciReg = { };
    RC rc;
    CHECK_RC(SaveConfigReg(&pciReg, address, regType));
    pPciRegisters->push_back(pciReg);
    return rc;
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::SaveConfigReg(PciRegister * pReg, UINT32 address, ConfigRegType regType)
{
    if (regType == ConfigRegType::INVALID)
        return RC::BAD_PARAMETER;

    RC rc;
    if (regType == ConfigRegType::WORD)
    {
        UINT16 temp16 = 0;
        CHECK_RC(Platform::PciRead16(m_Domain, m_Bus, m_Device, m_Function, address, &temp16));
        pReg->data = temp16;
        Printf(Tee::PriLow, "Saved config space at offset 0x%x was 0x%x\n",
               address, temp16);
    }
    else
    {
        MASSERT(regType == ConfigRegType::DWORD);
        CHECK_RC(Platform::PciRead32(m_Domain, m_Bus, m_Device,
                                     m_Function, address, &pReg->data));
        Printf(Tee::PriLow, "Saved config space at offset 0x%x was 0x%x\n",
               address, pReg->data);
    }
    pReg->regType = regType;
    pReg->address = address;
    return rc;
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::SaveConfigRegs
(
    UINT32                 startOffset,
    UINT32                 regCount,
    vector<PciRegister> *  pPciRegisters
)
{
    RC rc;
    for (UINT32 i = 0; i < regCount; i++)
    {
        PciRegister pciReg = { };
        CHECK_RC(SaveConfigReg(&pciReg, startOffset + (i * 4), ConfigRegType::DWORD));
        pPciRegisters->push_back(pciReg);
    }
    return rc;
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::RestoreConfigReg(PciRegister &reg)
{
    RC rc;
    if (reg.regType == ConfigRegType::WORD)
    {
        UINT16 temp = 0;
        CHECK_RC(Platform::PciRead16(m_Domain, m_Bus, m_Device, m_Function,
                                      reg.address, &temp));
        if (temp == reg.data)
        {
            Printf(Tee::PriLow, "Config space at offset 0x%x was already 0x%x\n",
                   reg.address, reg.data);
            return RC::OK;
        }

        Printf(Tee::PriLow, "Restoring config space at offset 0x%x (was 0x%x, writing 0x%x)\n",
               reg.address, temp, reg.data);
        CHECK_RC(Platform::PciWrite16(m_Domain, m_Bus, m_Device, m_Function,
                                      reg.address, reg.data));
    }
    else
    {
        MASSERT(reg.regType == ConfigRegType::DWORD);
        UINT32 temp = 0;
        CHECK_RC(Platform::PciRead32(m_Domain, m_Bus, m_Device, m_Function,
                                      reg.address, &temp));
        if (temp == reg.data)
        {
            Printf(Tee::PriLow, "Config space at offset 0x%x was already 0x%x\n",
                   reg.address, reg.data);
            return RC::OK;
        }

        Printf(Tee::PriLow, "Restoring config space at offset 0x%x (was 0x%x, writing 0x%x)\n",
               reg.address, temp, reg.data);
        CHECK_RC(Platform::PciWrite32(m_Domain, m_Bus, m_Device, m_Function,
                                      reg.address, reg.data));
    }
    return rc;
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::RestoreConfigRegs(vector<PciCfgSpaceSaver::PciRegister> & pciSavedData)
{
    return RestoreConfigRegs(pciSavedData, 0, static_cast<UINT32>(pciSavedData.size()));
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::RestoreConfigRegs
(
    vector<PciCfgSpaceSaver::PciRegister> & pciSavedData,
    UINT32 startIdx,
    UINT32 regCount)
{
    StickyRC rc;
    for (size_t idx = startIdx; idx < (startIdx + regCount); idx++)
    {
        rc = RestoreConfigReg(pciSavedData[idx]);
    }
    return rc;
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::SavePciConfigSpace()
{
    // Save the first 16 DWORDS of config space;
    return SaveConfigRegs(0, 16, &m_PciConfigSpace);
}

//------------------------------------------------------------------------------
RC PciCfgSpaceSaver::RestorePciConfigSpace()
{
    // 16 DWORDS should have been saved
    MASSERT(m_PciConfigSpace.size() == 16);

    StickyRC rc;
    for (INT32 lwrIdx = 15; lwrIdx >= 10; lwrIdx--)
    {
        rc = RestoreConfigReg(m_PciConfigSpace[lwrIdx]);
    }

    // Restoring the BARs requires retries
    for (INT32 lwrIdx = 9; lwrIdx >= 4; lwrIdx--)
    {
        UINT32 retries = 10;
        UINT32 temp = 0;
        do
        {
            rc = RestoreConfigReg(m_PciConfigSpace[lwrIdx]);
            rc = Platform::PciRead32(m_Domain, m_Bus, m_Device, m_Function,
                                     m_PciConfigSpace[lwrIdx].address, &temp);
            if (temp != m_PciConfigSpace[lwrIdx].data)
            {
                const UINT32 sleepTime =
                    (Platform::GetSimulationMode() == Platform::Hardware) ? 1000 : 20;
                Platform::SleepUS(sleepTime);
            }
        } while ((retries != 0) && (temp != m_PciConfigSpace[lwrIdx].data));
    }

    for (INT32 lwrIdx = 3; lwrIdx >= 0; lwrIdx--)
    {
        rc = RestoreConfigReg(m_PciConfigSpace[lwrIdx]);
    }

    return rc;
}
