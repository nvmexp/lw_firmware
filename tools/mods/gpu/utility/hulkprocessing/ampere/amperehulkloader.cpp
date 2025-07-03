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

#include "amperehulkloader.h"
#include "gpu/amperegpu.h"
#include "gpu/reghal/reghal.h"

bool IsScrubbingComplete(void* typeErasedRegs);

void AmpereHulkLoader::GetLoadStatus(UINT32* pProgress, UINT32* pErrorCode) noexcept
{
    // For pProgress address see LW_UCODE_POST_CODE_HULK_REG in https://resman.lwpu.com/source/xref/sw/main/bios/common/cert20/ucode_postcodes.h
    // For pErrorCode address, see LW_UCODE_POST_CODE_HULK_REG4 in the same file
    *pProgress = (m_pGpuSubdevice->Regs().Read32(MODS_PBUS_SW_SCRATCH, 0x15) >> 16) & 0xF;
    *pErrorCode = m_pGpuSubdevice->Regs().Read32(MODS_PBUS_SW_SCRATCH, 0x23) & 0xFFFF;
}
RC AmpereHulkLoader::DoSetup()
{
    return WaitForScrubbing();
}

/**
 * Spins until memory scrubbing is complete.
 */
RC AmpereHulkLoader::WaitForScrubbing()
{
    constexpr FLOAT64 ScrubbingDefaultTimeoutMultiplier = 1000;
    RC rc;
    RegHal& regs = m_pGpuSubdevice->Regs();
    FLOAT64 timeout = ScrubbingDefaultTimeoutMultiplier * Tasker::GetDefaultTimeoutMs();
    CHECK_RC(POLLWRAP_HW(IsScrubbingComplete, &regs, timeout));
    return rc;
}

/**
 *
 * \param typeErasedRegs A pointer to a RegHal *
 * \return True if scrubbing is complete. False otherwise.
 */
bool IsScrubbingComplete(void* typeErasedRegs)
{
    RegHal* regs = static_cast<RegHal *>(typeErasedRegs);
    return !regs->Test32(MODS_PFB_NISO_SCRUB_STATUS_FLAG_PENDING);
}
