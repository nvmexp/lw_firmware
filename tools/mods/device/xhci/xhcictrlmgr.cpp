/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "cheetah/include/controllermgr.h"
#include "xhcictrlmgr.h"
#include "cheetah/include/devid.h"

// Define the class
DEFINE_CONTROLLER_MGR(XhciController, LW_DEVID_XUSB_HOST);

RC XhciControllerManager::PrivateValidateController(UINT32 Index)
{
    return OK;
}

// todo, expose GetBasicInfo() and enable following function
/*
RC XhciDeviceMgr::PrivateValidateDevice(UINT32 BusNum, UINT32 DevNum, UINT32 FunNum,
                                        UINT32 * ChipVersion)
{
    LOG_ENT();
    RC rc;
    UINT16 deviceId, vendorId;

    CHECK_RC(Platform::PciRead16(BusNum, DevNum, FunNum,
                                 PCI_VENDOR_ID_OFFSET, &vendorId));
    CHECK_RC(Platform::PciRead16(BusNum, DevNum, FunNum,
                                 PCI_DEVICE_ID_OFFSET, &deviceId));

    Printf(Tee::PriLow, "\tVendor: 0x%04x\t", vendorId);
    Printf(Tee::PriLow, "\tDevice: 0x%04x\n", deviceId);

    if ( (vendorId!=VENDOR_ID_LW)
         &&(vendorId!=VENDOR_ID_NEC) )
    {
        Printf(Tee::PriWarn, "Unknown Vendor, bypass validation");
        *ChipVersion = Chipset::INVALID;
    }
    else
    {
        switch ( deviceId )
        {
        case 0x27c1:
        case 0x0194:
            Printf(Tee::PriLow, "\tNEC xHCI controller");
            *ChipVersion = Chipset::INVALID;
            break;

        case DEVICE_ID_MCP8D_XHCI:
        case 0x0E16:
            *ChipVersion = Chipset::MCP8D;
            Printf(Tee::PriLow, "\tMcp8D XHCI (0x%04x)", deviceId);
            break;

        default:
            Printf(Tee::PriWarn, "\tUNKOWN DeviceID 0x%04x, Vendor 0x%04x", deviceId, vendorId);
            *ChipVersion = Chipset::INVALID;
            break;
        }
    }
    LOG_EXT();
    return OK;
}
*/
