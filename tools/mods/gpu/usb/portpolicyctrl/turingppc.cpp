/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/pci.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "gpu/include/testdevice.h"
#include "gpu/usb/portpolicyctrl/turingppc.h"
#include "core/include/xp.h"

namespace
{
    const UINT32 PORT_POLICY_CONTROLLER_FUNCTION_NUMBER = 0x3;
}

//-----------------------------------------------------------------------------
/* virtual */ bool TuringPPC::DoIsSupported() const
{
    if (GetDevice()->Regs().IsSupported(MODS_FUSE_OPT_PCIE_MAGIC_BITS_D_DATA))
    {
        const UINT32 magicBitsD = GetDevice()->Regs().Read32(MODS_FUSE_OPT_PCIE_MAGIC_BITS_D_DATA);
        const UINT32 magicBit = 0x200000;
        if (magicBitsD & magicBit)
        {
            // PPC has been fused disabled
            return false;
        }
    }

    UINT16 gpuDomain = ~0;
    UINT16 gpuBus = ~0;
    UINT16 gpuDevice = ~0;
    GetDevice()->GetId().GetPciDBDF(&gpuDomain, &gpuBus, &gpuDevice);

    // Find all the device with class code = CLASS_CODE_SERIAL_BUS_CONTROLLER
    // and try to map against current GPU's domain, bus and device number
    for (UINT32 index = 0; /* until all devices found or error */; index++)
    {
        UINT32 domain, bus, device, function;
        if (OK != Platform::FindPciClassCode(Pci::CLASS_CODE_SERIAL_BUS_CONTROLLER,
                                            index,
                                            &domain,
                                            &bus,
                                            &device,
                                            &function))
        {
            return false;
        }

        if (domain == gpuDomain &&
            bus == gpuBus &&
            device == gpuDevice &&
            function == PORT_POLICY_CONTROLLER_FUNCTION_NUMBER)
        {
            return true;
        }
    }

    return false;
}

//-----------------------------------------------------------------------------
/* virtual */ RC TuringPPC::DoGetPciDBDF
(
    UINT32 *pDomain,
    UINT32 *pBus,
    UINT32 *pDevice,
    UINT32 *pFunction
) const
{
    RC rc;

    UINT16 domain, bus, device;
    GetDevice()->GetId().GetPciDBDF(&domain, &bus, &device);

    if (pDomain)
    {
        *pDomain = domain;
    }
    if (pBus)
    {
        *pBus = bus;
    }
    if (pDevice)
    {
        *pDevice = device;
    }
    if (pFunction)
    {
        *pFunction = PORT_POLICY_CONTROLLER_FUNCTION_NUMBER;
    }

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC TuringPPC::DoWaitForConfigToComplete() const
{
    RC rc;

    JavaScriptPtr pJs;
    UINT32 configTimeoutMs = USB_PPC_CONFIG_TIMEOUT_MS;
    pJs->GetProperty(pJs->GetGlobalObject(), "g_UsbPpcConfigTimeOutMs", &configTimeoutMs);
    rc = Tasker::PollHw(configTimeoutMs,
            [=]() -> bool
            { 
                return GetDevice()->Regs().Test32(MODS_XVE_PPC_STATUS_ALT_MODE_NEG_STATUS_DONE);
            },
            __FUNCTION__);

    if (rc == RC::TIMEOUT_ERROR)
    {
        Printf(Tee::PriError, "Functional test board configuration timed out\n");
        return RC::USBTEST_CONFIG_FAIL;
    }

    // WAR for bug# 2134058
    // PPC driver takes ~4 sec to regenerate and update the sysfs nodes with
    // the correct values for ALT mode. Above bug has been filed against PPC driver team
    // to reduce this time.
    UINT32 configWaitTimeMs = PortPolicyCtrl::USB_PPC_CONFIG_WAIT_TIME_MS;
    pJs->GetProperty(pJs->GetGlobalObject(), "g_UsbPpcConfigWaitTimeMs", &configWaitTimeMs);

    Tasker::Sleep(configWaitTimeMs);

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC TuringPPC::DoIsOrientationFlipped(bool *pIsOrientationFlipped) const
{
    RC rc;
    MASSERT(pIsOrientationFlipped);

    auto &regs = GetDevice()->Regs();
    const ModsGpuRegValue regValue = MODS_XVE_PPC_STATUS_ORIENTATION_FLIPPED;
    if (!regs.IsSupported(regValue))
    {
        Printf(Tee::PriError,
                   "Register not supported %s\n",
                   regs.ColwertToString(regValue));
        return RC::REGISTER_READ_WRITE_FAILED;
    }

    *pIsOrientationFlipped = regs.Test32(regValue);

    return rc;
}
