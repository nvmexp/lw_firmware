/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "t18x/t186/arxusb_padctl.h"
#include "t186xusbpadctrl.h"
#include "cheetah/include/tegradrf.h"
#include "cheetah/include/sysutil.h"
#include "cheetah/include/tegradrf.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "cheetah/dev/fuse/skut186.h"

T186XusbPadCtrl::T186XusbPadCtrl()
{

}

T186XusbPadCtrl::~T186XusbPadCtrl()
{

}

RC T186XusbPadCtrl::EnablePadTrack(void)
{
    RC rc = OK;
    UINT32 regVal;

    // Program the timing parametrs for tracking FSM
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0_TRK_START_TIMER_RANGE, 0x20, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0_TRK_DONE_RESET_TIMER_RANGE, 0x10, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0, regVal));

    CHECK_RC(RegRd(XUSB_PADCTL_HSIC_PAD_TRK_CTL_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD_TRK_CTL_0_TRK_START_TIMER_RANGE, 0x20, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD_TRK_CTL_0_TRK_DONE_RESET_TIMER_RANGE, 0x10, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_HSIC_PAD_TRK_CTL_0, regVal));
    // Power up tracking circuit and enable HW FSM
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0_PD_TRK_RANGE, 0x0, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0, regVal));

    CHECK_RC(RegRd(XUSB_PADCTL_HSIC_PAD_TRK_CTL_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD_TRK_CTL_0_PD_TRK_RANGE, 0x0, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_HSIC_PAD_TRK_CTL_0, regVal));

    Tasker::Sleep(1);

    return rc;
}

RC T186XusbPadCtrl::PadInit(UINT08 DeviceModePortIndex, bool IsAdbSafe)
{
    RC rc = OK;
    UINT32 regVal;

    /* Read USB_CALIB and USB_CALIB_EXT */
    UINT32 usbCalibVal=0;
    UINT32 usbCalibExtVal=0;

    SkuT186 *pFuse = new SkuT186("0xdeadbeaf");
    const UINT32 USB_CALIB_0 = 0x1F0;
    const UINT32 USB_CALIB_EXT_0 = 0x350;

    CHECK_RC(pFuse->Init());
    CHECK_RC(pFuse->ReadFuseValue(USB_CALIB_0, &usbCalibVal));
    CHECK_RC(pFuse->ReadFuseValue(USB_CALIB_EXT_0, &usbCalibExtVal));
    CHECK_RC(pFuse->Uninit());
    delete pFuse;

    UINT32 termRange = (usbCalibVal >> 7) & 0xf;
    UINT32 rpdCtrl = usbCalibExtVal & 0x1f;

    Printf(Tee::PriLow, "T186 Xusb Pad initialization\n");
    // assign the USB2.0 ports to XUSB or USB2 (Only OTG and HSIC PAD Port 0 could be assigned to USB2 controller)
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_PAD_MUX_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PAD_MUX_0_HSIC_PORT0_CONFIG_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT2_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT1_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT0_RANGE, 1, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_PAD_MUX_0, regVal));

    // port capabilities
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_PORT_CAP_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT2_CAP_RANGE, 1, regVal);
    //regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT2_INTERNAL_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT1_CAP_RANGE, 1, regVal);
    //regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT1_INTERNAL_RANGE, 0, regVal);
    //regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT0_INTERNAL_RANGE, 0, regVal);
    if (0xff != DeviceModePortIndex)
    {
        Printf(Tee::PriLow, "Configuring Port 0 for device mode\n");
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT0_CAP_RANGE, 2, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_PORT_CAP_0, regVal));
        CHECK_RC(RegRd(XUSB_PADCTL_SS_PORT_CAP_0, &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_SS_PORT_CAP_0_PORT1_CAP_RANGE, 2, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_SS_PORT_CAP_0, regVal));
    }
    else
    {
        if(!IsAdbSafe)
        {
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_PORT_CAP_0_PORT0_CAP_RANGE, 1, regVal);
        }
        CHECK_RC(RegWr(XUSB_PADCTL_USB2_PORT_CAP_0, regVal));
        CHECK_RC(RegRd(XUSB_PADCTL_SS_PORT_CAP_0, &regVal));
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_SS_PORT_CAP_0_PORT1_CAP_RANGE, 1, regVal);
        CHECK_RC(RegWr(XUSB_PADCTL_SS_PORT_CAP_0, regVal));
    }
    CHECK_RC(RegRd(XUSB_PADCTL_SS_PORT_CAP_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_SS_PORT_CAP_0_PORT0_CAP_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_SS_PORT_CAP_0_PORT2_CAP_RANGE, 1, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_SS_PORT_CAP_0, regVal));

    //disable OC since MODS doesn't handle it
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_OC_MAP_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OC_MAP_0_PORT0_OC_PIN_RANGE, 0xf, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OC_MAP_0_PORT1_OC_PIN_RANGE, 0xf, regVal);   // or 4 if use J8110
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OC_MAP_0_PORT2_OC_PIN_RANGE, 0xf, regVal);
    //regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OC_MAP_0_PORT3_OC_PIN_RANGE, 0xf, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_OC_MAP_0, regVal));

    CHECK_RC(RegRd(XUSB_PADCTL_SS_OC_MAP_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OC_MAP_0_PORT0_OC_PIN_RANGE, 0xf, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OC_MAP_0_PORT1_OC_PIN_RANGE, 0xf, regVal);   // or 4 if use J8110
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OC_MAP_0_PORT2_OC_PIN_RANGE, 0xf, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_SS_OC_MAP_0, regVal));

    CHECK_RC(RegRd(XUSB_PADCTL_VBUS_OC_MAP_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE1_OC_MAP_RANGE, 0xf, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE0_OC_MAP_RANGE, 0xf, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_VBUS_OC_MAP_0, regVal));

    Printf(Tee::PriLow, "Programming OTG Pad controls \n");
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0, &regVal));
    if (0xff != DeviceModePortIndex)
    {   // todo: should we fllow the tegrashell script to set following to 0?
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0_VREG_LEV_RANGE, 0x0, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0_VREG_DIR_RANGE, 0x0, regVal);
    }
    else
    {
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0_VREG_LEV_RANGE, 0x0, regVal);
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0_VREG_DIR_RANGE, 0x0, regVal);
    }
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0_PD_VREG_RANGE, 0x0, regVal);       // PROD settings
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0, regVal));
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD1_CTL1_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD1_CTL1_0_PD_VREG_RANGE, 0x0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD1_CTL1_0_VREG_LEV_RANGE, 0x0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD1_CTL1_0_VREG_DIR_RANGE, 0x0, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD1_CTL1_0, regVal));
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD2_CTL1_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD2_CTL1_0_PD_VREG_RANGE, 0x0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD2_CTL1_0_VREG_LEV_RANGE, 0x0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD2_CTL1_0_VREG_DIR_RANGE, 0x0, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD2_CTL1_0, regVal));

    CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_PD_ZI_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_TERM_SEL_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_LS_FSLEW_RANGE, 0x6, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_LS_RSLEW_RANGE, 0x6, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0, regVal));

    // PROD Settings programming - UPHY, see also http://lwbugs/200169894
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_PD_ZI_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_TERM_SEL_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_LS_FSLEW_RANGE, 0x6, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_LS_RSLEW_RANGE, 0x6, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0, regVal));

    CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_PD_ZI_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_TERM_SEL_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_LS_FSLEW_RANGE, 0x6, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_LS_RSLEW_RANGE, 0x6, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0, regVal));

    Printf(Tee::PriLow, "Programming BIAS Pad controls \n");
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0_SPARE_RANGE, 0x3, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0_CHG_DIV_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0_TEMP_COEF_RANGE, 0x3, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0_VREF_CTRL_RANGE, 0x4, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0_PD_RANGE, 0x0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0_TERM_OFFSET_RANGE, 0x3, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0_HS_CHIRP_LEVEL_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0_HS_DISCON_LEVEL_RANGE, 0x7, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0_HS_SQUELCH_LEVEL_RANGE, (usbCalibVal >> 29) & 0x7, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0, regVal));

    // ==== clear powerdown of both OTD pads and bias pads
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_PD_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_HS_LWRR_LEVEL_RANGE, usbCalibVal & 0x3f, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0, regVal));

    CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_PD_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_HS_LWRR_LEVEL_RANGE, (usbCalibVal >> 17) & 0x3f, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0, regVal));
    // PROD Settings programming ends

    CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_PD_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_HS_LWRR_LEVEL_RANGE, (usbCalibVal >> 11) & 0x3f, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0, regVal));

    CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0 , &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0_TERM_RANGE_ADJ_RANGE , termRange, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0_RPD_CTRL_RANGE, rpdCtrl, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0_PD_DR_RANGE, 0, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0 , regVal));

    CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0 , &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0_TERM_RANGE_ADJ_RANGE , termRange, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0_RPD_CTRL_RANGE, rpdCtrl, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0_PD_DR_RANGE, 0, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0, regVal));

    CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0 , &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0_TERM_RANGE_ADJ_RANGE , termRange, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0_RPD_CTRL_RANGE, rpdCtrl, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0_PD_DR_RANGE, 0, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0, regVal));

    Printf(Tee::PriLow, "Programming HSIC Pads\n");
    CHECK_RC(RegRd(XUSB_PADCTL_HSIC_PAD0_CTL_0_0 , &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_PD_TX_DATA0_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_PD_TX_STROBE_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_PD_RX_DATA0_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_PD_RX_STROBE_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_PD_ZI_DATA0_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_PD_ZI_STROBE_RANGE, 0, regVal);

    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_RPD_STROBE_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_RPU_STROBE_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_RPD_DATA0_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_RPU_DATA0_RANGE, 0, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_HSIC_PAD0_CTL_0_0 , regVal));

    CHECK_RC(RegRd(XUSB_PADCTL_HSIC_PAD0_CTL_1_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_1_0_TX_RTUNEP_RANGE, 8, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_1_0_TX_SLEW_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_1_0_HSIC_OPT_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_1_0_RTERM_RANGE, 0x10, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_HSIC_PAD0_CTL_1_0, regVal));

    CHECK_RC(RegRd(XUSB_PADCTL_HSIC_STRB_TRIM_CONTROL_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_STRB_TRIM_CONTROL_0_STRB_TRIM_VAL_RANGE, 0x2D, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_HSIC_STRB_TRIM_CONTROL_0, regVal));

    CHECK_RC(RegRd(XUSB_PADCTL_HSIC_PAD0_CTL_2_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_2_0_RX_STROBE_TRIM_RANGE, 0xd, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_HSIC_PAD0_CTL_2_0  , regVal));

    Printf(Tee::PriLow, "Driving Idle to HSIC Pads \n");
    CHECK_RC(RegRd(XUSB_PADCTL_HSIC_PAD0_CTL_0_0 , &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_RPD_STROBE_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_RPU_STROBE_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_RPD_DATA0_RANGE, 1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_HSIC_PAD0_CTL_0_0_RPU_DATA0_RANGE, 0, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_HSIC_PAD0_CTL_0_0 , regVal));
    // ELPG settings
    CHECK_RC(RegWr(XUSB_PADCTL_ELPG_PROGRAM_0_0, 0));
    CHECK_RC(RegWr(XUSB_PADCTL_ELPG_PROGRAM_1_0, 0));

    return rc;
}

RC T186XusbPadCtrl::EnableVbusOverride()
{
    RC rc = OK;
    UINT32 regVal;
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_VBUS_ID_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_VBUS_ID_0_VBUS_OVERRIDE_RANGE, 0x1, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_VBUS_ID_0, regVal));
    return OK;
}

RC T186XusbPadCtrl::DisableDiscDetect(UINT08 Port)
{
    RC rc = OK;
    UINT32 regVal;

    switch (Port)
    {
        case 4:
            CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0, &regVal));
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0_PD_DISC_OVRD_RANGE, 0x1, regVal);
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0_PD_DISC_OVRD_VAL_RANGE, 0x1, regVal);
            CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0, regVal));
            break;
        case 5:
            CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0, &regVal));
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0_PD_DISC_OVRD_RANGE, 0x1, regVal);
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0_PD_DISC_OVRD_VAL_RANGE, 0x1, regVal);
            CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0, regVal));
            break;
        case 6:
            CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0, &regVal));
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0_PD_DISC_OVRD_RANGE, 0x1, regVal);
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0_PD_DISC_OVRD_VAL_RANGE, 0x1, regVal);
            CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0, regVal));
            break;
        default:
            Printf(Tee::PriError, "Disable Disconnect Detect not supported for Port %d. Valid values are [4-6]\n", Port);
            rc = RC::BAD_PARAMETER;
            break;
    }

    return rc;
}
