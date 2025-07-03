/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#pragma once

#include "core/include/rc.h"
#include "core/include/jscript.h"

#include <vector>
#include <set>

//------------------------------------------------------------------------------
// Utility class to save and restore the raw PCI configuration data using the
// bus,device,function as the starting address.
class PciCfgSpace
{
public:
    PciCfgSpace() = default;
    virtual ~PciCfgSpace() {}
    RC Initialize(UINT32 domain, UINT32 bus, UINT32 device, UINT32 function);
    RC CheckCfgSpaceReady(FLOAT64 TimeoutMs);
    RC CheckCfgSpaceNotReady(FLOAT64 TimeoutMs);
    bool IsCfgSpaceReady() { return PollCfgSpaceReady(this); }
    RC Restore();
    virtual RC Save();
    virtual RC UseXveRegMap(UINT32 regMap);
    UINT32 PciDomainNumber() const { return m_Domain; }
    UINT32 PciBusNumber() const { return m_Bus; }
    UINT32 PciDeviceNumber() const { return m_Device; }
    UINT32 PciFunctionNumber() const { return m_Function; }
    void Clear(){m_SavedPciCfgSpace.clear();}
    void SkipRegRestore(UINT32 reg) { m_SkipRestoreRegs.insert(reg); }

protected:
    enum Function
    {
        FUNC_GPU = 0,
        FUNC_AZALIA = 1,
        FUNC_XUSB = 2,
        FUNC_PPC = 3
    };

    RC SaveEntry(UINT32 reg) { return SaveEntry(reg, 0xffffffff); }
    RC SaveEntry(UINT32 reg, UINT32 mask);
    bool   IsSaved() const { return !m_SavedPciCfgSpace.empty(); }
    UINT16 SavedVendorId() const { return m_SavedVendorId; }
    static UINT32 s_XveRegWriteMap[];
    static UINT32 s_Xve1RegWriteMap[];

    UINT32* m_XveRegMap       = nullptr;
    UINT32  m_XveRegMapLength = 0;

private:
    static bool PollCfgSpaceReady(void* param);
    static bool PollCfgSpaceNotReady(void* param);
    struct SavedEntry
    {
        UINT32 reg    = 0;   //!< Saved register in PCI config space
        UINT32 value  = 0;   //!< Value of saved register
        UINT32 mask   = 0;   //!< Mask of saved bits to restore
    };

    UINT32 m_Domain           = 0;
    UINT32 m_Bus              = 0;
    UINT32 m_Device           = 0;
    UINT32 m_Function         = 0;
    UINT16 m_SavedVendorId    = 0;
    bool m_Initialized        = false;
    vector<SavedEntry> m_SavedPciCfgSpace;
    set<UINT32> m_SkipRestoreRegs;
};
