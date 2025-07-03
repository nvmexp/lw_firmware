/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mem_repair_config.h"

#include <sstream>

/******************************************************************************/
// MemRepairSeqMode

//!
//! \brief Check is the given flags are set in the given memory repair sequence
//! mode.
//!
//! \param mode Memory repair sequence mode.
//! \param flags Flags to check for membership.
//!
//! \return True if the \a flags are set in \a mode, false otherwise.
//!
bool Repair::IsSet(MemRepairSeqMode mode, MemRepairSeqMode flags)
{
    return (static_cast<UINT32>(mode & flags) != 0);
}

Repair::MemRepairSeqMode
Repair::operator&(MemRepairSeqMode lhs, MemRepairSeqMode rhs)
{
    return static_cast<MemRepairSeqMode>(static_cast<UINT32>(lhs) & static_cast<UINT32>(rhs));
}

Repair::MemRepairSeqMode&
Repair::operator&=(MemRepairSeqMode& lhs, MemRepairSeqMode rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

Repair::MemRepairSeqMode
Repair::operator|(MemRepairSeqMode lhs, MemRepairSeqMode rhs)
{
    return static_cast<MemRepairSeqMode>(static_cast<UINT32>(lhs) | static_cast<UINT32>(rhs));
}

Repair::MemRepairSeqMode&
Repair::operator|=(MemRepairSeqMode& lhs, MemRepairSeqMode rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

/******************************************************************************/
// Settings

string Repair::Settings::ToString() const
{
    string repairStr = "Settings(";
    repairStr += Utility::StrPrintf("\n  runningInstInSys=%s",
                                    Utility::ToString(runningInstInSys).c_str());
    repairStr += "\n  Modifiers(";
    repairStr += Utility::StrPrintf("\n    hbmResetDurationMs=%u",
                                    modifiers.hbmResetDurationMs);
    repairStr += Utility::StrPrintf("\n    forceHtoJ=%s",
                                    Utility::ToString(modifiers.forceHtoJ).c_str());
    repairStr += Utility::StrPrintf("\n    pseudoHardRepair=%s",
                                    Utility::ToString(modifiers.pseudoHardRepair).c_str());
    repairStr += Utility::StrPrintf("\n    skipHbmFuseRepairCheck=%s",
                                    Utility::ToString(modifiers.skipHbmFuseRepairCheck).c_str());
    repairStr += Utility::StrPrintf("\n    ignoreHbmFuseRepairCheckResult=%s",
                                    Utility::ToString(modifiers.ignoreHbmFuseRepairCheckResult).c_str());
    repairStr += Utility::StrPrintf("\n    printRegSeq=%s",
                                    Utility::ToString(modifiers.printRegSeq).c_str());
    repairStr += Utility::StrPrintf("\n    memRepairSeqMode=0x%x",
                                    static_cast<UINT32>(modifiers.memRepairSeqMode));
    repairStr += "\n  )\n)";

    return repairStr;
}

/******************************************************************************/
// InitializationConfig

bool Repair::InitializationConfig::operator==(const Repair::InitializationConfig& o) const
{
    return initializeGpuTests == o.initializeGpuTests
        && hbmDeviceInitMethod == o.hbmDeviceInitMethod
        && skipRmStateInit == o.skipRmStateInit;
}

bool Repair::InitializationConfig::operator!=(const Repair::InitializationConfig& o) const
{
    return !(*this == o);
}

/******************************************************************************/
// SystemState

bool Repair::SystemState::IsLwrrentGpuSet() const
{
    const bool isLwrrentGpuSet = !lwrrentGpu.rawEcid.empty();
    MASSERT(((isLwrrentGpuSet && lwrrentGpu.pSubdev)
             || (!isLwrrentGpuSet && !lwrrentGpu.pSubdev))
            && "Both the raw ECID and subdevice should be in the same state, "
            "either set or unset");

    return isLwrrentGpuSet;
}
