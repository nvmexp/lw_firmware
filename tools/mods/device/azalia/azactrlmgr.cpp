/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "azactrlmgr.h"
#include "azactrl.h"
#include "cheetah/include/devid.h"

#include "core/include/platform.h"
#include "core/include/pci.h"
#include "ada/ad102/dev_lw_xve1.h"

#include "cheetah/include/cheetah.h"
#include "device/include/memtrk.h"
#include "core/include/xp.h"
#include "device/include/mcp.h"

DEFINE_CONTROLLER_MGR(AzaliaController, LW_DEVID_HDA);

namespace AzaliaControllerMgr
{
    bool s_IsIntelAzaliaAllowed = false;
}

RC AzaliaControllerManager::PrivateValidateController(UINT32 Index)
{
    RC rcV = OK, rcD = OK;
    UINT16 vendorId, deviceId;

    if (Platform::IsTegra())
    {
        RC rc = OK;

        string chipName;
        CHECK_RC(CheetAh::GetChipName(&chipName));
        Printf(Tee::PriLow, "CheetAh (%s) Azalia Found\n", chipName.c_str());
        return OK;
    }
    else
    {
        rcV = m_pControllers[Index]->CfgRd16(LW_PCI_VENDOR_ID, &vendorId);
        rcD = m_pControllers[Index]->CfgRd16(LW_PCI_DEVICE_ID, &deviceId);

        if ((rcV == OK) && (rcD == OK))
        {
            if (vendorId == Pci::VendorIds::Lwpu)
            {
                RC rc = OK;
                switch(deviceId)
                {

                    //-----------------------------------------------
                    // Add GPU Azalia Devices here
                    //-----------------------------------------------
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GK104: Printf(Tee::PriLow, "GK104 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GK106: Printf(Tee::PriLow, "GK106 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GK107: Printf(Tee::PriLow, "GK107 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GK110: Printf(Tee::PriLow, "GK110 Azalia: DeviceId: 0x%04x.\n", deviceId); break;

                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GK208: Printf(Tee::PriLow, "GK208 Azalia: DeviceId: 0x%04x.\n", deviceId); break;

                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GM107: Printf(Tee::PriLow, "GM107 Azalia: DeviceId: 0x%04x.\n", deviceId); break;

                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GM200: Printf(Tee::PriLow, "GM200 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GM204: Printf(Tee::PriLow, "GM204 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GM206: Printf(Tee::PriLow, "GM206 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GP100: Printf(Tee::PriLow, "GP100 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GP102: Printf(Tee::PriLow, "GP102 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GP104: Printf(Tee::PriLow, "GP104 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GP106: Printf(Tee::PriLow, "GP106 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GP107: Printf(Tee::PriLow, "GP107 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GP108: Printf(Tee::PriLow, "GP108 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GV100: Printf(Tee::PriLow, "GV100 Azalia: DeviceId: 0x%04x.\n", deviceId); break;

                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_TU102: Printf(Tee::PriLow, "TU102 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_TU104: Printf(Tee::PriLow, "TU104 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_TU106: Printf(Tee::PriLow, "TU106 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_TU108: Printf(Tee::PriLow, "TU108 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_TU116: Printf(Tee::PriLow, "TU116 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_TU117: Printf(Tee::PriLow, "TU117 Azalia: DeviceId: 0x%04x.\n", deviceId); break;

                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GA100: Printf(Tee::PriLow, "GA100 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GA102: Printf(Tee::PriLow, "GA102 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GA103: Printf(Tee::PriLow, "GA103 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GA104: Printf(Tee::PriLow, "GA104 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GA106: Printf(Tee::PriLow, "GA106 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GA107: Printf(Tee::PriLow, "GA107 Azalia: DeviceId: 0x%04x.\n", deviceId); break;

                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_AD102 :Printf(Tee::PriLow, "AD102 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_AD103 :Printf(Tee::PriLow, "AD103 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_AD104 :Printf(Tee::PriLow, "AD104 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_AD106 :Printf(Tee::PriLow, "AD106 Azalia: DeviceId: 0x%04x.\n", deviceId); break;
                    case LW_XVE1_ID_DEVICE_CHIP_AZALIA_AD107 :Printf(Tee::PriLow, "AD107 Azalia: DeviceId: 0x%04x.\n", deviceId); break;

                    default:
                        Printf(Tee::PriWarn, "UNKNOWN DeviceId: 0x%04x\n", deviceId);
                        return RC::ILWALID_DEVICE_ID;
                }

                CHECK_RC(MemoryTracker::SetIsAllocFromPcieNode(m_pControllers[m_NumControllers], true));
                CHECK_RC(((AzaliaController *)m_pControllers[m_NumControllers])->SetIsAllocFromPcieNode(true));

                return rc;
            }
            else
            {
                // Allow to run on Intel parts with override
                if (((vendorId == Pci::VendorIds::Intel) && (!AzaliaControllerMgr::s_IsIntelAzaliaAllowed)) ||
                    (vendorId != Pci::VendorIds::Intel))
                {
                    Printf(Tee::PriLow, "Bypassing unsupported vendor: 0x%04x", vendorId);
                    // stop here for non Lwpu parts.
                    return RC::NOT_LW_DEVICE;
                }
            }
        }
    }

    return RC::ILWALID_DEVICE_ID;
}
