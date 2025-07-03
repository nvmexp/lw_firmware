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

#include "t21x/t210/arxusb_padctl.h"
#include "core/include/massert.h"
#include "core/include/platform.h"
#include "cheetah/include/tegradrf.h"
#include "cheetah/include/devid.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/jstocpp.h"
#include "xusbpadctrl.h"
#include "cheetah/include/gpio_name.h"

namespace XusbPadCtrlMgr
{
    XusbPadCtrl * m_PadCtrl = nullptr;
}

XusbPadCtrl * XusbPadCtrlMgr::GetPadCtrl(bool IsNotCreate /*= false*/)
{
    if(m_PadCtrl || IsNotCreate)
    {
        return m_PadCtrl;
    }

    if(OK!=(CheetAh::SocPtr()->GetPadCtrl(&m_PadCtrl)))
    {
        Printf(Tee::PriError, "Unable to create XusbPadCtrl\n");
        return nullptr;
    }
    return m_PadCtrl;
}

void XusbPadCtrlMgr::Cleanup()
{
    if (m_PadCtrl)
    {
        delete m_PadCtrl;
        m_PadCtrl = nullptr;
    }
}

XusbPadCtrl::XusbPadCtrl()
{

}

XusbPadCtrl::~XusbPadCtrl()
{

}

RC XusbPadCtrl::EnableVbusOverride()
{
    RC rc = OK;
    UINT32 regVal;
    CHECK_RC(RegRd(XUSB_PADCTL_USB2_VBUS_ID_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_VBUS_ID_0_VBUS_OVERRIDE_RANGE, 0x1, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_VBUS_ID_0, regVal));
    return OK;
}

RC XusbPadCtrl::RegWr(UINT32 Offset, UINT32 Data)
{
    RC rc = OK;
    if (Offset%4)
    {
        Printf(Tee::PriError, "Invalid Offset %d, should be DWORD align  \n", Offset);
        return RC::BAD_PARAMETER;
    }
    void *pBar;
    CHECK_RC(CheetAh::SocPtr()->GetApertureByDeviceType(LW_DEVID_XUSB_PADCTL, 0, &pBar));
    MEM_WR32((reinterpret_cast<unsigned long>(pBar) + Offset), Data);

    Printf(Tee::PriLow, "XusbPadCtrl RegWr() 0x%08x -> Offset 0x%x \n", Data, Offset);
    return OK;
}

RC XusbPadCtrl::RegRd(UINT32 Offset, UINT32 *pData)
{
    MASSERT(pData != 0);
    RC rc = OK;
    if (Offset%4)
    {
        Printf(Tee::PriError, "Invalid Offset %d, should be DWORD align  \n", Offset);
        return RC::BAD_PARAMETER;
    }
    void *pBar;
    CHECK_RC(CheetAh::SocPtr()->GetApertureByDeviceType(LW_DEVID_XUSB_PADCTL, 0, &pBar));
    *pData = MEM_RD32(reinterpret_cast<unsigned long>(pBar) + Offset);

    Printf(Tee::PriLow, "PLLRegRd() 0x%08x <- Offset 0x%x \n", *pData, Offset);
    return OK;
}

RC XusbPadCtrl::DisableDiscDetect(UINT08 Port)
{
    Printf(Tee::PriError, "Function not supported\n");
    return RC::UNSUPPORTED_FUNCTION;
}

RC XusbPadCtrl::PadInit(UINT08 DeviceModePortIndex, bool IsAdbSafe, bool IsForceHS /*= false*/)
{
    Printf(Tee::PriError, "Function not supported\n");
    return RC::UNSUPPORTED_FUNCTION;
}

RC XusbPadCtrl::EnablePadTrack(void)
{
    Printf(Tee::PriError, "Function not supported\n");
    return RC::UNSUPPORTED_FUNCTION;
}

RC XusbPadCtrl::EnableVbus(bool IsAdbPort /*= false*/)
{
    RC rc;
    UINT32 regVal;
    UINT32 chipId = 0;

    PinmuxBase * pinMux = PinMuxMgr::GetPinmux();
    CHECK_RC(CheetAh::GetChipId(&chipId));
    if (chipId == 0x194 || chipId == 0x186)
    {
        CHECK_RC(pinMux->WriteField("PADCTL_UART_USB_VBUS_EN0_0", "E_IO_HV", 1));
        CHECK_RC(pinMux->WriteField("PADCTL_UART_USB_VBUS_EN0_0", "GPIO_SFIO_SEL", 1));
        CHECK_RC(pinMux->WriteField("PADCTL_UART_USB_VBUS_EN0_0", "PUPD", 0));
        CHECK_RC(pinMux->WriteField("PADCTL_UART_USB_VBUS_EN0_0", "E_INPUT", 1));
        CHECK_RC(pinMux->WriteField("PADCTL_UART_USB_VBUS_EN0_0", "PAD_OUT", 0));

        CHECK_RC(pinMux->WriteField("PADCTL_UART_USB_VBUS_EN1_0", "E_IO_HV", 1));
        CHECK_RC(pinMux->WriteField("PADCTL_UART_USB_VBUS_EN1_0", "GPIO_SFIO_SEL", 1));
        CHECK_RC(pinMux->WriteField("PADCTL_UART_USB_VBUS_EN1_0", "PUPD", 0));
        CHECK_RC(pinMux->WriteField("PADCTL_UART_USB_VBUS_EN1_0", "E_INPUT", 1));
        CHECK_RC(pinMux->WriteField("PADCTL_UART_USB_VBUS_EN1_0", "PAD_OUT", 0));
    }
    else if (chipId == 0x234)
    {
        // GPIO - PADCTL_G2_USB_VBUS_EN0_0
        SysUtil::Gpio::Enable(TEGRA_GPIO_PZ1);
        SysUtil::Gpio::DirectionOutput(TEGRA_GPIO_PZ1, 1);

        // GPIO - PADCTL_G2_USB_VBUS_EN0_1
        SysUtil::Gpio::Enable(TEGRA_GPIO_PZ2);
        SysUtil::Gpio::DirectionOutput(TEGRA_GPIO_PZ2, 1);
    }

    CHECK_RC(RegRd(XUSB_PADCTL_VBUS_OC_MAP_0, &regVal));
    if(IsAdbPort)
    {
        regVal = FLD_SET_RF_NUM(XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE0_RANGE, 0x1, regVal);
    }
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE1_RANGE, 0x1, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_VBUS_OC_MAP_0, regVal));

    return OK;
}

RC XusbPadCtrl::DisableVbus()
{
    RC rc;
    UINT32 regVal;
    UINT32 chipId = 0;

    PinmuxBase * pinMux = PinMuxMgr::GetPinmux();
    CHECK_RC(CheetAh::GetChipId(&chipId));
    if (chipId == 0x194 || chipId == 0x186)
    {
        CHECK_RC(pinMux->WriteField("PADCTL_UART_USB_VBUS_EN0_0", "PAD_OUT", 0));
        CHECK_RC(pinMux->WriteField("PADCTL_UART_USB_VBUS_EN1_0", "PAD_OUT", 0));
    }
    else if (chipId == 0x234)
    {
        // GPIO - PADCTL_G2_USB_VBUS_EN0_0
        SysUtil::Gpio::DirectionOutput(TEGRA_GPIO_PZ1, 0);
        // GPIO - PADCTL_G2_USB_VBUS_EN0_1
        SysUtil::Gpio::DirectionOutput(TEGRA_GPIO_PZ2, 0);
    }

    CHECK_RC(RegRd(XUSB_PADCTL_VBUS_OC_MAP_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE0_RANGE, 0, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE1_RANGE, 0, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_VBUS_OC_MAP_0, regVal));

    return OK;
}

RC XusbPadCtrl::DevModeEnableVbus()
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

RC XusbPadCtrl::DevModeDisableVbus()
{
    RC rc;
    UINT32 regVal;

    CHECK_RC(RegRd(XUSB_PADCTL_USB2_VBUS_ID_0, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_VBUS_ID_0_VBUS_OVERRIDE_RANGE, 0, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_VBUS_ID_0, regVal));

    return OK;
}

RC XusbPadCtrl::ForcePortFS(UINT32 Port)
{
    RC rc = OK;
    UINT32 regVal;

    CHECK_RC(RegRd(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0 + Port * 0x40, &regVal));
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_PD2_RANGE, 0x1, regVal);
    regVal = FLD_SET_RF_NUM(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_PD2_OVRD_EN_RANGE, 0x1, regVal);
    CHECK_RC(RegWr(XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0 + Port * 0x40, regVal));

    return rc;
}

RC XusbPadCtrl::SetupUsb2PadMux(UINT08 Port, UINT08 Owned)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC XusbPadCtrlMgr::RegWr(UINT32 Offset, UINT32 Data)
{
    return GetPadCtrl()->RegWr(Offset, Data);
}

RC XusbPadCtrlMgr::RegRd(UINT32 Offset, UINT32 *pData)
{
    return GetPadCtrl()->RegRd(Offset, pData);
}

RC XusbPadCtrlMgr::EnableVbusOverride()
{
    return GetPadCtrl()->EnableVbusOverride();
}

RC XusbPadCtrlMgr::DisableDiscDetect(UINT08 Port)
{
    return GetPadCtrl()->DisableDiscDetect(Port);
}

RC XusbPadCtrlMgr::EnableVbus(bool IsAdbPort /*= false*/)
{
    return GetPadCtrl()->EnableVbus(IsAdbPort);
}

RC XusbPadCtrlMgr::DisableVbus()
{
    return GetPadCtrl()->DisableVbus();
}

RC XusbPadCtrlMgr::DevModeEnableVbus()
{
    return GetPadCtrl()->DevModeEnableVbus();
}

RC XusbPadCtrlMgr::DevModeDisableVbus()
{
    return GetPadCtrl()->DevModeDisableVbus();
}

JSIF_NAMESPACE(XusbPadCtrlMgr, NULL, NULL, "XusbPadCtrl interface");
JSIF_TEST_2_S(XusbPadCtrlMgr,
              RegWr,
              UINT32, Offset,
              UINT32, Data,
              "Write the XUSB_PADCTL registers");

JSIF_METHOD_1_S(XusbPadCtrlMgr,
                UINT32,
                RegRd,
                UINT32, Offset,
                "Read the XUSB_PADCTL registers");

JSIF_TEST_0_S(XusbPadCtrlMgr, EnableVbusOverride, "Enable Vbus Override");

JSIF_TEST_1_S(XusbPadCtrlMgr,
              DisableDiscDetect,
              UINT08, Port,
              "Disable Disconnect Detect for Port");

JS_STEST_LWSTOM(XusbPadCtrlMgr, EnableVbus, 1, "Enable Vbus")
{
    JSIF_INIT_VAR(XusbPadCtrlMgr, EnableVbus, 0, 1, *IsAdbPort);
    JSIF_GET_ARG_OPT(0, bool, IsAdbPort, false);
    JSIF_CALL_TEST_STATIC(XusbPadCtrlMgr, EnableVbus, IsAdbPort);
    JSIF_RETURN(rc);
}

JSIF_TEST_0_S(XusbPadCtrlMgr, DisableVbus, "Disable Vbus");
JSIF_TEST_0_S(XusbPadCtrlMgr, DevModeEnableVbus, "Device Mode Enable Vbus");
JSIF_TEST_0_S(XusbPadCtrlMgr, DevModeDisableVbus, "Device Mode Disable Vbus");
