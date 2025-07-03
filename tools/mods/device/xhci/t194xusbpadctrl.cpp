/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "t19x/t194/arxusb_padctl.h"
#include "t194xusbpadctrl.h"
#include "cheetah/include/tegradrf.h"
#include "cheetah/include/sysutil.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "cheetah/include/cheetah.h"
#include "cheetah/dev/fuse/skut194.h"
#include "cheetah/include/tegchip.h"

T194XusbPadCtrl::T194XusbPadCtrl()
{
    Printf(Tee::PriLow, "T194 Xusb Pad Ctrl\n");
}

T194XusbPadCtrl::~T194XusbPadCtrl()
{

}

RC T194XusbPadCtrl::PadInit(UINT08 DeviceModePortIndex, bool IsAdbSafe, bool IsForceHS /*= false*/)
{
    RC rc = OK;
    UINT32 regVal;
    UINT32 chipId = 0;

    if ( (DeviceModePortIndex >= XUSB_HS_PORT_NUM)
         && (0xff != DeviceModePortIndex) )
    {
        Printf(Tee::PriError,
               "Invalid port index %d, valid [0,%d] or 0xff \n",
               DeviceModePortIndex,
               XUSB_HS_PORT_NUM - 1);
        return RC::ILWALID_INPUT;
    }

    CHECK_RC(CheetAh::GetChipId(&chipId));
    //https://confluence.lwpu.com/pages/viewpage.action?pageId=89390511
    std::vector<UINT08> usb2ToUsb3Mapping = {2, 3, 0, 1};
    if (chipId == 0x234)
    {
        //https://confluence.lwpu.com/pages/viewpage.action?pageId=761332305
        usb2ToUsb3Mapping = {2, 1, 0, 3};
    }

    UINT08 deviceModePortIndexSs = 0xff;
    if (0xff != DeviceModePortIndex)
    {
        deviceModePortIndexSs= usb2ToUsb3Mapping[DeviceModePortIndex];
    }

    Printf(Tee::PriLow, "T194 Xusb Pad initialization, \n");
    // assign the USB2.0 ports to XUSB or USB2 (Only OTG and HSIC PAD Port 0 could be assigned to USB2 controller)
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_PAD_MUX_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT3_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT2_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT1_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT0_RANGE, 1, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_PAD_MUX_0, regVal));

    // port capabilities for USB2
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_PORT_CAP_0, &regVal));
    for (UINT32 i = 0; i < XUSB_HS_PORT_NUM; i ++)
    {
        UINT32 cap = 1; //HOST
        if ( i == DeviceModePortIndex )
        {
            cap = 2;    //DEV
        }
        switch (i)
        {
            case 0: // program port 0 cap
                if ( (DeviceModePortIndex == 0)
                    || (!IsAdbSafe) )
                {
                    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT0_CAP_RANGE, cap, regVal);
                    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT0_REVERSE_ID_RANGE, 1, regVal);
                }
                break;
            case 1:
                regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT1_CAP_RANGE, cap, regVal);
                break;
            case 2:
                regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT2_CAP_RANGE, cap, regVal);
                break;
            case 3:
                regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT3_CAP_RANGE, cap, regVal);
                break;
        }
    }
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_PORT_CAP_0, regVal));

    // port capabilities for USB3
    CHECK_RC(RegRd(XUSB_PADCTL_SS_PORT_CAP_0, &regVal));
    for (UINT32 i = 0; i < XUSB_HS_PORT_NUM; i ++)
    {
        UINT32 cap = IsForceHS ? 0 : 1; //HOST
        if ( i == deviceModePortIndexSs )
        {
            cap = 2;    //DEV
        }
        switch (i)
        {
            case 0:
                regVal = FLD_SET_RF_NUM(XUSB_PADCTL_SS_PORT_CAP_0_PORT0_CAP_RANGE, cap, regVal);
                break;
            case 1:
                regVal = FLD_SET_RF_NUM(XUSB_PADCTL_SS_PORT_CAP_0_PORT1_CAP_RANGE, cap, regVal);
                break;
            case 2:
                if ( (deviceModePortIndexSs == 2)
                    || (!IsAdbSafe) )
                {
                    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_SS_PORT_CAP_0_PORT2_CAP_RANGE, cap, regVal);
                    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_SS_PORT_CAP_0_PORT2_REVERSE_ID_RANGE, 1, regVal);
                }
                break;
            case 3:
                regVal = FLD_SET_RF_NUM(XUSB_PADCTL_SS_PORT_CAP_0_PORT3_CAP_RANGE, cap, regVal);
                break;
        }
    }
    CHECK_RC(RegWr(XUSB_PADCTL_SS_PORT_CAP_0, regVal));

    /*
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_PORT_CAP_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT3_CAP_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT2_CAP_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT1_CAP_RANGE, 1, regVal);
    if (IsDeviceMode)
    {
        Printf(Tee::PriLow, "Configuring Port 0 for device mode\n");
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT0_CAP_RANGE, 2, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_PORT_CAP_0, regVal));
        CHECK_RC(RegRd(XUSB_PADCTL_SS_PORT_CAP_0, &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_SS_PORT_CAP_0_PORT2_CAP_RANGE, 2, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_SS_PORT_CAP_0, regVal));
    }
    else
    {   // HS port 0 and SS port 2 as ADB
        //https://confluence.lwpu.com/pages/viewpage.action?pageId=89390511
        if(!IsAdbSafe)
        {
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT0_CAP_RANGE, 1, regVal);
        }
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_PORT_CAP_0, regVal));
        if (!IsAdbSafe)
        {
            CHECK_RC(RegRd(XUSB_PADCTL_SS_PORT_CAP_0, &regVal));
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_SS_PORT_CAP_0_PORT2_CAP_RANGE, 1, regVal);
            CHECK_RC(RegWr(XUSB_PADCTL_SS_PORT_CAP_0, regVal));
        }
    }
    CHECK_RC(RegRd(XUSB_PADCTL_SS_PORT_CAP_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_SS_PORT_CAP_0_PORT0_CAP_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_SS_PORT_CAP_0_PORT1_CAP_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_SS_PORT_CAP_0_PORT3_CAP_RANGE, 1, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_SS_PORT_CAP_0, regVal));
    */

    //disable OC since MODS doesn't handle it
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_OC_MAP_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OC_MAP_0_PORT0_OC_PIN_RANGE, 0xf, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OC_MAP_0_PORT1_OC_PIN_RANGE, 0xf, regVal);   // or 4 if use J8110
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OC_MAP_0_PORT2_OC_PIN_RANGE, 0xf, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OC_MAP_0_PORT3_OC_PIN_RANGE, 0xf, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_OC_MAP_0, regVal));

    /*
    CHECK_RC(RegRd(XUSB_PADCTL_SS_OC_MAP_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OC_MAP_0_PORT0_OC_PIN_RANGE, 0xf, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OC_MAP_0_PORT1_OC_PIN_RANGE, 0xf, regVal);   // or 4 if use J8110
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OC_MAP_0_PORT2_OC_PIN_RANGE, 0xf, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_SS_OC_MAP_0, regVal));
    */

    CHECK_RC(RegRd(XUSB_PADCTL_VBUS_OC_MAP_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE1_OC_MAP_RANGE, 0xf, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE0_OC_MAP_RANGE, 0xf, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_VBUS_OC_MAP_0, regVal));

    Printf(Tee::PriLow, "Programming OTG Pad controls \n");
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0_PD_VREG_RANGE, 0x1, regVal);       // PROD settings
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0, regVal));
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD1_CTL1_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD1_CTL1_0_PD_VREG_RANGE, 0x1, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD1_CTL1_0, regVal));
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD2_CTL1_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD2_CTL1_0_PD_VREG_RANGE, 0x1, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD2_CTL1_0, regVal));
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD3_CTL1_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD3_CTL1_0_PD_VREG_RANGE, 0x1, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD3_CTL1_0, regVal));

    // PROD Settings programming - UPHY, see also http://lwbugs/200169894
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_PD_ZI_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_PD_RANGE, 0, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0, regVal));

    CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_PD_ZI_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_PD_RANGE, 0, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0, regVal));

    CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_PD_ZI_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_PD_RANGE, 0, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0, regVal));

    CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0_PD_ZI_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0_PD_RANGE, 0, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0, regVal));

    CHECK_RC(RegRd(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0_PD_RANGE, 0x0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0_HS_DISCON_LEVEL_RANGE, 0x7, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0, regVal));

    Printf(Tee::PriLow, "Programming BIAS Pad controls for T194 \n");
    if(true)
    {
        /* Read USB_CALIB and USB_CALIB_EXT */
        UINT32 usbCalibVal=0;
        UINT32 usbCalibExtVal=0;

        SkuT194 *pFuse = new SkuT194("0xdeadbeaf");
        const UINT32 USB_CALIB_0 = 0x1F0;
        const UINT32 USB_CALIB_EXT_0 = 0x350;

        CHECK_RC(pFuse->Init());
        CHECK_RC(pFuse->ReadFuseValue(USB_CALIB_0, &usbCalibVal));
        CHECK_RC(pFuse->ReadFuseValue(USB_CALIB_EXT_0, &usbCalibExtVal));
        CHECK_RC(pFuse->Uninit());
        delete pFuse;

        UINT32 rpdCtrl = usbCalibExtVal & 0x1f;
        CHECK_RC(RegRd(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0, &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0_HS_SQUELCH_LEVEL_RANGE, (usbCalibVal >> 29) & 0x7, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0, regVal));

        // ==== clear powerdown of both OTD pads and bias pads
        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0, &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_HS_LWRR_LEVEL_RANGE, usbCalibVal & 0x3f, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0, regVal));

        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0, &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_HS_LWRR_LEVEL_RANGE, (usbCalibVal >> 11) & 0x3f, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0, regVal));

        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0, &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_HS_LWRR_LEVEL_RANGE, (usbCalibVal >> 17) & 0x3f, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0, regVal));

        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0, &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0_HS_LWRR_LEVEL_RANGE, (usbCalibVal >> 23) & 0x3f, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0, regVal));
        // PROD Settings programming ends

        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0 , &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0_TERM_RANGE_ADJ_RANGE, (usbCalibVal >> 7) & 0xf, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0_RPD_CTRL_RANGE, rpdCtrl, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0_PD_DR_RANGE, 0, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0 , regVal));

        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0 , &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0_TERM_RANGE_ADJ_RANGE, (usbCalibExtVal >> 5) & 0xf, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0_RPD_CTRL_RANGE, rpdCtrl, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0_PD_DR_RANGE, 0, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0, regVal));

        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0 , &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0_TERM_RANGE_ADJ_RANGE , (usbCalibExtVal >> 9) & 0xf, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0_RPD_CTRL_RANGE, rpdCtrl, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0_PD_DR_RANGE, 0, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0, regVal));

        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0 , &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0_TERM_RANGE_ADJ_RANGE , (usbCalibExtVal >> 13) & 0xf, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0_RPD_CTRL_RANGE, rpdCtrl, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0_PD_DR_RANGE, 0, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0, regVal));
    }
    else
    {
        // ==== clear powerdown of both OTD pads and bias pads
        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0, &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_HS_LWRR_LEVEL_RANGE, 0x12, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0, regVal));

        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0, &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_HS_LWRR_LEVEL_RANGE, 0x12, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0, regVal));

        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0, &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_HS_LWRR_LEVEL_RANGE, 0x12, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0, regVal));

        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0, &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0_HS_LWRR_LEVEL_RANGE, 0x12, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0, regVal));
        // PROD Settings programming ends

        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0 , &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0_TERM_RANGE_ADJ_RANGE, 0x8, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0_RPD_CTRL_RANGE, 0x8, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0 , regVal));

        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0 , &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0_TERM_RANGE_ADJ_RANGE, 0x8, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0_RPD_CTRL_RANGE, 0x8, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0, regVal));

        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0 , &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0_TERM_RANGE_ADJ_RANGE, 0x8, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0_RPD_CTRL_RANGE, 0x8, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0, regVal));

        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0 , &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0_TERM_RANGE_ADJ_RANGE, 0x8, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0_RPD_CTRL_RANGE, 0x8, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0, regVal));
    }

    CHECK_RC(RegWr(XUSB_PADCTL_ELPG_PROGRAM_0_0, 0));
    CHECK_RC(RegWr(XUSB_PADCTL_ELPG_PROGRAM_1_0, 0));
    return rc;
}

RC T194XusbPadCtrl::SetupUsb2PadMux(UINT08 Port, UINT08 Owned)
{
    RC rc = OK;
    UINT32 regVal;

    switch (Port)
    {
        case 0:
            if (Owned == 1)
            {
                CHECK_RC(RegRd(XUSB_PADCTL_USB2_PAD_MUX_0, &regVal));
                regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT0_RANGE, XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT0_XUSB, regVal);
                CHECK_RC(RegWr(XUSB_PADCTL_USB2_PAD_MUX_0, regVal));
            }
            else if (Owned == 2)
            {
                //program pinMux registers to let UARTH come from USB0
                CHECK_RC(RegRd(XUSB_PADCTL_USB2_PAD_MUX_0, &regVal));
                regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT0_RANGE, XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT0_UART, regVal);
                CHECK_RC(RegWr(XUSB_PADCTL_USB2_PAD_MUX_0, regVal));
            }
            break;
        case 1:
            if (Owned == 1)
            {
                CHECK_RC(RegRd(XUSB_PADCTL_USB2_PAD_MUX_0, &regVal));
                regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT1_RANGE, XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT1_XUSB, regVal);
                CHECK_RC(RegWr(XUSB_PADCTL_USB2_PAD_MUX_0, regVal));
            }
            else if (Owned == 2)
            {
                //program pinMux registers to let UARTH come from USB1
                CHECK_RC(RegRd(XUSB_PADCTL_USB2_PAD_MUX_0, &regVal));
                regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT1_RANGE, XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT1_UART, regVal);
                CHECK_RC(RegWr(XUSB_PADCTL_USB2_PAD_MUX_0, regVal));
            }
            break;
        default:
            Printf(Tee::PriError, "Pad configuraton not supported for Port %d. Valid values are [0-1]\n", Port);
            rc = RC::BAD_PARAMETER;
            break;
    }

    return rc;
}


RC T194XusbPadCtrl::EnablePadTrack(void)
{
    RC rc = OK;
    UINT32 regVal;

    // Program the timing parametrs for tracking FSM
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0_TRK_START_TIMER_RANGE, 0x1E, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0_TRK_DONE_RESET_TIMER_RANGE, 0xA, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0_PD_TRK_RANGE, 0x0, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0, regVal));

    // Power up tracking circuit and enable HW FSM
    // Wait for tracking completion
    UINT32 elapseUs = 0;
    UINT32 timeoutUs = 1000000;     // 1 second
    do{
        Platform::SleepUS(100);
        elapseUs+=100;
        CHECK_RC(RegRd(XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0, &regVal));
    }while(FLD_TEST_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0_TRK_COMPLETED_RANGE, 0, regVal) && (elapseUs<=timeoutUs));
    if (elapseUs > timeoutUs)
    {
        Printf(Tee::PriError, "Waiting for TRK_COMPLETED timeout\n");
        return RC::TIMEOUT_ERROR;
    }

    //clear the done bit
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0_TRK_COMPLETED_RANGE, 0x1, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0, regVal));

    elapseUs = 0;
    do{
        Platform::SleepUS(100);
        elapseUs+=100;
        CHECK_RC(RegRd(XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0, &regVal));
    }while(FLD_TEST_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0_TRK_COMPLETED_RANGE, 1, regVal) && (elapseUs<=timeoutUs));
    if (elapseUs > timeoutUs)
    {
        Printf(Tee::PriError, "Waiting for TRK_COMPLETED bit clear timeout\n");
        return RC::TIMEOUT_ERROR;
    }
    return rc;
}

RC T194XusbPadCtrl::DisableDiscDetect(UINT08 Port)
{
    RC rc = OK;
    UINT32 regVal;
    UINT32 portRegGap = 0x40;

    if (( Port >= 5 )
        && ( Port <=8 ))
    {
        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0 + (Port - 5) * portRegGap, &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0_PD_DISC_OVRD_RANGE, 0x1, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0_PD_DISC_OVRD_VAL_RANGE, 0x1, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0 + (Port - 5) * portRegGap, regVal));
    }
    else
    {
        Printf(Tee::PriError, "Disable Disconnect Detect not supported for Port %d. Valid values are [5-8]\n", Port);
        rc = RC::BAD_PARAMETER;
    }
    return rc;
}

RC T194XusbPadCtrl::ForceSsSpd(UINT08 Gen /* = 0*/)
{
    RC rc;
    UINT32 regVal;

    CHECK_RC(RegRd(XUSB_PADCTL_SS_PORT_CFG_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_SS_PORT_CFG_0_PORT3_SPEED_SUPPORT_RANGE, Gen, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_SS_PORT_CFG_0_PORT2_SPEED_SUPPORT_RANGE, Gen, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_SS_PORT_CFG_0_PORT1_SPEED_SUPPORT_RANGE, Gen, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_SS_PORT_CFG_0_PORT0_SPEED_SUPPORT_RANGE, Gen, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_SS_PORT_CFG_0, regVal));

    return OK;
}

RC T194XusbPadCtrl::DevModeEnableVbus()
{
    RC rc;
    UINT32 regVal;

    CHECK_RC(RegRd(XUSB_PADCTL_USB2_VBUS_ID_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_VBUS_ID_0_VBUS_SOURCE_SELECT_RANGE, 0x1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_VBUS_ID_0_ID_SOURCE_SELECT_RANGE, 0x1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_VBUS_ID_0_VBUS_OVERRIDE_RANGE, 0x1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_VBUS_ID_0_ID_OVERRIDE_RANGE, 0x8, regVal);
    Printf(Tee::PriLow, "Dev VBUS 0x%x -> VBUS_ID_0", regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_VBUS_ID_0, regVal));

    return OK;
}

RC T194XusbPadCtrl::DevModeDisableVbus()
{
    RC rc;
    UINT32 regVal;

    CHECK_RC(RegRd(XUSB_PADCTL_USB2_VBUS_ID_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_VBUS_ID_0_VBUS_OVERRIDE_RANGE, 0, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_VBUS_ID_0, regVal));

    return OK;
}

RC T194XusbPadCtrl::SetProdSetting()
{
    RC rc = OK;
    UINT32 chipId = 0;
    UINT32 regVal = 0;

    CHECK_RC(CheetAh::GetChipId(&chipId));
    if (chipId == 0x234)
    {
        // Bug https://lwbugs/3401316
        // High Speed
        // Port 0, HS_TXEQ from 0x0 to 0x2
        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD0_CTL_3_0, &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_3_0_HS_TXEQ_RANGE, 0x2, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD0_CTL_3_0,  regVal));
        // Port 1, HS_TXEQ from 0x0 to 0x2
        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD1_CTL_3_0, &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_3_0_HS_TXEQ_RANGE, 0x2, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD1_CTL_3_0,  regVal));
        // Port 2, HS_TXEQ from 0x0 to 0x1
        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD2_CTL_3_0, &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_3_0_HS_TXEQ_RANGE, 0x1, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD2_CTL_3_0,  regVal));

        // Low Speed
        // Port 0~ port 3, LS_FSLEW / LS_RSLEW from 0x6 to 0xC
        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0, &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_LS_FSLEW_RANGE, 0xc, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_LS_RSLEW_RANGE, 0xc, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0,  regVal));

        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0, &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_LS_FSLEW_RANGE, 0xc, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_LS_RSLEW_RANGE, 0xc, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0,  regVal));

        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0, &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_LS_FSLEW_RANGE, 0xc, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_LS_RSLEW_RANGE, 0xc, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0,  regVal));

        CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0, &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0_LS_FSLEW_RANGE, 0xc, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0_LS_RSLEW_RANGE, 0xc, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0,  regVal));
    }

    return rc;
}
