/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#pragma once

#include "core/include/rc.h"
#include <vector>

// A targeted PCI spec compliant config space save/restore
class PciCfgSpaceSaver
{
public:
    PciCfgSpaceSaver
    (
        UINT32 domain,
        UINT32 bus,
        UINT32 device,
        UINT32 function,
        UINT08 pcieCapBase
    );
    PciCfgSpaceSaver(const PciCfgSpaceSaver&) = delete;
    PciCfgSpaceSaver& operator=(const PciCfgSpaceSaver&) = delete;

    ~PciCfgSpaceSaver();

    RC Save();
    RC Restore();

private:
    enum class ConfigRegType
    {
        WORD
       ,DWORD
       ,INVALID
    };
    struct PciRegister
    {
        UINT32 address          = 0;
        UINT32 data             = 0;
        ConfigRegType   regType = ConfigRegType::INVALID;
    };

    RC SavePciConfigSpace();
    RC RestorePciConfigSpace();

    RC SavePcieState();
    RC RestorePcieState();

    RC SavePcixState();
    RC RestorePcixState();

    RC SaveLtrState();
    RC RestoreLtrState();

    RC SaveDpcState();
    RC RestoreDpcState();

    RC SaveAerState();
    RC RestoreAerState();

    RC SaveAllVcStates();
    RC RestoreAllVcState();
    RC VcLoadArbTable(UINT32 vcCapBase);
    RC VcLoadPortArbTable(UINT32 ctrl);
    RC VcExtCapSaveRestore(vector<PciRegister> & vcSavedRegs, UINT16 vcCapPtr, bool bSave);

    RC SavePasidState();
    RC RestorePasidState();

    RC SavePriState();
    RC RestorePriState();

    RC SaveAtsState();
    RC RestoreAtsState();

    RC SaveResizableBarState();
    RC RestoreResizableBarState();

    RC SaveAcsState();
    RC RestoreAcsState();

    RC SaveMsiState();
    RC RestoreMsiState();

    RC SaveIovState();
    RC RestoreIovState();

    RC SaveConfigReg(PciRegister * pReg, UINT32 address, ConfigRegType regType);
    RC SaveConfigReg
    (
        vector<PciRegister> * pPciRegisters,
        UINT32                address,
        ConfigRegType         regType
    );
    RC SaveConfigRegs
    (
        UINT32                 startOffset,
        UINT32                 regCount,
        vector<PciRegister> *  pPciRegisters
    );
    RC RestoreConfigReg(PciRegister &reg);
    RC RestoreConfigRegs(vector<PciRegister> & pciSavedData);
    RC RestoreConfigRegs(vector<PciRegister> & pciSavedData, UINT32 startIdx, UINT32 regCount);

    UINT32 m_Domain      = 0;
    UINT32 m_Bus         = 0;
    UINT32 m_Device      = 0;
    UINT32 m_Function    = 0;
    UINT08 m_PcieCapBase = 0;

    vector<PciRegister> m_PciConfigSpace;
    vector<PciRegister> m_PciPcieState;
    PciRegister         m_PciPcixCmd;
    vector<PciRegister> m_PciLtrState;
    PciRegister         m_PciDpcCtrl;
    vector<PciRegister> m_PciAerState;
    vector<PciRegister> m_PciVcState;
    vector<PciRegister> m_PciMfvcState;
    vector<PciRegister> m_PciVc9State;
    PciRegister         m_PciPasidCtrl;
    vector<PciRegister> m_PciPriState;
    PciRegister         m_PciAtsCtrl;
    vector<PciRegister> m_PciResizableBarState;
    PciRegister         m_PciAcsCtrl;
    vector<PciRegister> m_PciMsiState;
    PciRegister         m_PciMsixCtrl;
    void *              m_pMsixTableBase = nullptr;
    vector<bool>        m_MsixVectorMasked;
    vector<PciRegister> m_PciSriovState;
};

