/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwswitchgpio.h"

#include "device/interface/lwlink/lwlregs.h"
#include "gpu/include/testdevice.h"
#include "gpu/reghal/reghal.h"
#include "mods_reg_hal.h"

bool LwSwitchGpio::DoGetDirection(UINT32 pinNum)
{
    return GetDevice()->GetInterface<LwLinkRegs>()->Regs().Read32(0, MODS_PMGR_GPIO_OUTPUT_CNTL_IO_OUT_EN, pinNum);
}

bool LwSwitchGpio::DoGetInput(UINT32 pinNum)
{
    return GetDevice()->GetInterface<LwLinkRegs>()->Regs().Read32(0, MODS_PMGR_GPIO_OUTPUT_CNTL_IO_INPUT, pinNum);
}

bool LwSwitchGpio::DoGetOutput(UINT32 pinNum)
{
    return GetDevice()->GetInterface<LwLinkRegs>()->Regs().Read32(0, MODS_PMGR_GPIO_OUTPUT_CNTL_IO_OUTPUT, pinNum);
}

RC LwSwitchGpio::DoSetDirection(UINT32 pinNum, bool toOutput)
{
    GetDevice()->GetInterface<LwLinkRegs>()->Regs().Write32(0, MODS_PMGR_GPIO_OUTPUT_CNTL_IO_OUT_EN, pinNum, toOutput ? 1 : 0);
    return RC::OK;
}

RC LwSwitchGpio::DoSetOutput(UINT32 pinNum, bool toHigh)
{
    RegHal& regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
    regs.Write32(0, MODS_PMGR_GPIO_OUTPUT_CNTL_IO_OUTPUT, pinNum, toHigh ? 1 : 0);
    regs.Write32(0, MODS_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE_TRIGGER);
    return RC::OK;
}
