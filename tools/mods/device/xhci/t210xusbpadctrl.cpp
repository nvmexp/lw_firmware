/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "t21x/t210/arxusb_padctl.h"
#include "t210xusbpadctrl.h"
#include "cheetah/include/tegradrf.h"

T210XusbPadCtrl::T210XusbPadCtrl()
{

}

T210XusbPadCtrl::~T210XusbPadCtrl()
{

}

RC T210XusbPadCtrl::DisableDiscDetect(UINT08 Port)
{
    RC rc = OK;
    UINT32 regVal;

    switch (Port)
    {
        case 5:
            CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0, &regVal));
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0_PD_DISC_OVRD_RANGE, 0x1, regVal);
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0_PD_DISC_OVRD_VAL_RANGE, 0x1, regVal);
            CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0, regVal));
            break;
        case 6:
            CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0, &regVal));
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0_PD_DISC_OVRD_RANGE, 0x1, regVal);
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0_PD_DISC_OVRD_VAL_RANGE, 0x1, regVal);
            CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0, regVal));
            break;
        case 7:
            CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0, &regVal));
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0_PD_DISC_OVRD_RANGE, 0x1, regVal);
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0_PD_DISC_OVRD_VAL_RANGE, 0x1, regVal);
            CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0, regVal));
            break;
        case 8:
            CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0, &regVal));
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0_PD_DISC_OVRD_RANGE, 0x1, regVal);
            regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0_PD_DISC_OVRD_VAL_RANGE, 0x1, regVal);
            CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0, regVal));
            break;
        default:
            Printf(Tee::PriError, "Disable Disconnect Detect not supported for Port %d. Valid values are [5-8]\n", Port);
            rc = RC::BAD_PARAMETER;
            break;
    }

    return rc;
}
