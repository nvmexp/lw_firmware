/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "ad10xfuseaccessor.h"

#include "gpu/include/gpusbdev.h"
#include "gpu/include/testdevice.h"
#include "gpu/fuse/fuse.h"
#include "mods_reg_hal.h"
#include "gpu/reghal/reghal.h"

AD10xFuseAccessor::AD10xFuseAccessor(TestDevice* pDev)
    : GA10xFuseAccessor(pDev),
      m_pDev(pDev),
      m_pFuse(nullptr)
{
    MASSERT(pDev);
    MASSERT(dynamic_cast<GpuSubdevice*>(pDev) != nullptr);
}

void AD10xFuseAccessor::SetParent(Fuse* pFuse)
{
    MASSERT(pFuse != nullptr);
    GA10xFuseAccessor::SetParent(pFuse);
    m_pFuse = pFuse;
}

RC AD10xFuseAccessor::CheckPSDefaultState()
{
    RegHal& regHal = m_pDev->Regs();

    if (regHal.Test32(MODS_FUSE_DEBUGCTRL_PS09_SET, 0) &&
        regHal.Test32(MODS_FUSE_DEBUGCTRL_PS09_RESET, 1))
    {
        return RC::OK;
    }

    Printf(Tee::PriError, "PS09 power switch is not in its default state\n");
    return RC::SOFTWARE_ERROR;
}

RC AD10xFuseAccessor::ProgramPSLatch(bool bSwitchOn)
{
    RC rc;
    RegHal& regHal = m_pDev->Regs();

    if (!regHal.HasRWAccess(MODS_FUSE_DEBUGCTRL))
    {
        return RC::PRIV_LEVEL_VIOLATION;
    }

    if (bSwitchOn)
    {
        CHECK_RC(CheckPSDefaultState());

        // Bring the power switch to hold state
        UINT32 regVal = regHal.Read32(MODS_FUSE_DEBUGCTRL);
        regHal.SetField(&regVal, MODS_FUSE_DEBUGCTRL_PS09_SET, 0);
        regHal.SetField(&regVal, MODS_FUSE_DEBUGCTRL_PS09_RESET, 0);
        regHal.Write32(MODS_FUSE_DEBUGCTRL, regVal);

        // Set the power switch to turn on power for the fuse macro
        regHal.Write32(MODS_FUSE_DEBUGCTRL_PS09_SET, 1);
    }
    else
    {
        // Bring the power switch to hold state
        UINT32 regVal = regHal.Read32(MODS_FUSE_DEBUGCTRL);
        regHal.SetField(&regVal, MODS_FUSE_DEBUGCTRL_PS09_SET, 0);
        regHal.SetField(&regVal, MODS_FUSE_DEBUGCTRL_PS09_RESET, 0);
        regHal.Write32(MODS_FUSE_DEBUGCTRL, regVal);

        // Reset the power switch to turn off power for the fuse macro
        regHal.Write32(MODS_FUSE_DEBUGCTRL_PS09_RESET, 1);
     }

     Utility::SleepUS(PS09_RESET_DELAY_US);
     return rc;
}
