/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwmisc.h"
#include "core/include/pci.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "device/interface/xusbhostctrl.h"
#include "gpu/include/testdevice.h"
#include "gpu/usb/functionaltestboard/uartftb.h"
#include "gpu/usb/functionaltestboard/vdmftb.h"
#include "gpu/usb/portpolicyctrl/portpolicyctrlimpl.h"

namespace
{
    // tuple: number of USB 3 and USB 2 devices in various ALT Modes
    typedef tuple<UINT08, UINT08> tupleUsbDevices;
    const map<PortPolicyCtrl::UsbAltModes, tupleUsbDevices> mapAltModeUsbDevCnt =
    {
        { PortPolicyCtrl::USB_ALT_MODE_0,       make_tuple(1, 0) },
        { PortPolicyCtrl::USB_ALT_MODE_1,       make_tuple(0, 1) },
        { PortPolicyCtrl::USB_ALT_MODE_2,       make_tuple(1, 1) },
        { PortPolicyCtrl::USB_ALT_MODE_3,       make_tuple(1, 1) },
        { PortPolicyCtrl::USB_ALT_MODE_4,       make_tuple(1, 0) },
        { PortPolicyCtrl::USB_LW_TEST_ALT_MODE, make_tuple(0, 0) }
    };
}

//-----------------------------------------------------------------------------
PortPolicyCtrlImpl::PortPolicyCtrlImpl()
{
    m_pAccessPpcMutex = Tasker::AllocMutex("PortPolicyCtrlImpl::m_pAccessPpcMutex",
                                           Tasker::mtxUnchecked);
}

//-----------------------------------------------------------------------------
/*virtual*/ PortPolicyCtrlImpl::~PortPolicyCtrlImpl()
{
    if (m_pAccessPpcMutex)
    {
        Tasker::FreeMutex(m_pAccessPpcMutex);
        m_pAccessPpcMutex = nullptr;
    }
}

//-----------------------------------------------------------------------------
/* virtual */ bool PortPolicyCtrlImpl::DoIsSupported() const
{
    return true;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PortPolicyCtrlImpl::DoInitialize()
{
    RC rc;
    if (!m_Init)
    {
        CHECK_RC(GetPciDBDF(&m_Domain, &m_Bus, &m_Device, &m_Function));
        m_Init = true;
    }
    return rc;
}

/* virtual */ RC PortPolicyCtrlImpl::DoInitializeFtb()
{
    // Initialize the FTB separately, -usb_ftb_serial_device needs devinst initialized.
    RC rc;
    if (!m_pFtb)
    {
        JavaScriptPtr pJs;
        JsArray jsFtbSerialDevices;
        if (OK == pJs->GetProperty(pJs->GetGlobalObject(),
                                   "g_UsbFtbSerialDevices",
                                   &jsFtbSerialDevices))
        {
            UINT32 devinst = GetDevice()->DevInst();
            JSContext *cx;
            pJs->GetContext(&cx);

            if ((jsFtbSerialDevices.size() > devinst) &&
                (JS_TypeOfValue(cx, jsFtbSerialDevices[devinst]) == JSTYPE_STRING))
            {
                string ftbSerialDevice;
                if (OK == pJs->FromJsval(jsFtbSerialDevices[devinst], &ftbSerialDevice))
                {
                    m_pFtb = make_unique<UartFtb>(ftbSerialDevice);
                }
            }
        }

        if (!m_pFtb)
        {
            m_pFtb = make_unique<VdmFtb>(m_Domain, m_Bus, m_Device, m_Function);
        }

        CHECK_RC(m_pFtb->Initialize());
    }
    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PortPolicyCtrlImpl::DoSetAltMode(UsbAltModes altMode)
{
    RC rc;

    if (!m_Init)
    {
        Printf(Tee::PriError, "Port Policy Controller not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    if (!m_pFtb)
    {
        Printf(Tee::PriError, "Port Policy Controller Functional Test Board not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    if (altMode == USB_UNKNOWN_ALT_MODE)
    {
        Printf(Tee::PriError, "Unsupported ALT mode requested\n");
        return RC::BAD_PARAMETER;
    }

    if (m_UsbAltMode == altMode)
    {
        Printf(Tee::PriLow,
               "Functional Test Board already in %u ALT Mode\n",
               static_cast<UINT08>(altMode));
        return rc;
    }

    Printf(Tee::PriLow,
           "Configuring Functional Test Board, setting ALT Mode to %u\n",
           static_cast<UINT08>(altMode));

    CHECK_RC(m_pFtb->SetAltMode(altMode));

    CHECK_RC(WaitForConfigToComplete());

    CHECK_RC(ConfirmAltModeConfig(altMode));

    Printf(Tee::PriLow,
           "Functional Test Board configuration successful. ALT Mode set to %u\n",
           static_cast<UINT08>(altMode));

    m_UsbAltMode = altMode;

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PortPolicyCtrlImpl::DoSetPowerMode
(
    UsbPowerModes powerMode,
    bool resetPowerMode,
    bool skipPowerCheck
)
{
    RC rc;

    if (!m_Init)
    {
        Printf(Tee::PriError, "Port Policy Controller not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    if (!m_pFtb)
    {
        Printf(Tee::PriError, "Port Policy Controller Functional Test Board not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    if (powerMode == USB_UNKNOWN_POWER_MODE)
    {
        Printf(Tee::PriError, "Unsupported Power mode requested\n");
        return RC::BAD_PARAMETER;
    }

    if (m_UsbPowerMode == powerMode)
    {
        Printf(Tee::PriLow,
               "Functional Test Board already in %u Power Mode\n",
               static_cast<UINT08>(powerMode));
        return rc;
    }

    JavaScriptPtr pJs;
    UINT32 powerConfigWaitTimeMs = PortPolicyCtrl::USB_POWER_CONFIG_WAIT_TIME_MS;
    pJs->GetProperty(pJs->GetGlobalObject(), "g_UsbSysfsNodeWaitTimeMs", &powerConfigWaitTimeMs);

    Printf(Tee::PriLow,
           "Configuring Functional Test Board, setting Power Mode to %u\n",
           static_cast<UINT08>(powerMode));

    CHECK_RC(m_pFtb->SetPowerMode(powerMode));

    if (powerConfigWaitTimeMs != 0)
    {
        Tasker::Sleep(powerConfigWaitTimeMs);
    }
    m_UsbPowerMode = powerMode;

    Printf(Tee::PriLow,
           "Functional Test Board configuration successful. Power Mode set to %u\n",
           static_cast<UINT08>(powerMode));

    if (powerMode != USB_DEFAULT_POWER_MODE)
    {
        // If power consumption is not as expected, VerifyPrintPowerValues
        // will return bad RC. Before returning, we need to reset the
        // power mode so that FTB is left in a better state.
        rc = VerifyPrintPowerValues(powerMode, skipPowerCheck, m_PrintPowerValue);

        if (resetPowerMode || rc != OK)
        {
            // Switch back to default powermode
            RC powerResetRC;
            powerResetRC = m_pFtb->SetPowerMode(USB_DEFAULT_POWER_MODE);

            if (powerConfigWaitTimeMs != 0)
            {
                Tasker::Sleep(powerConfigWaitTimeMs);
            }
            if (powerResetRC == OK)
            {
                m_UsbPowerMode = USB_DEFAULT_POWER_MODE;
                Printf(Tee::PriLow,
                       "Power Mode set to default\n");
            }
            // if rc is not OK, VerifyPrintPowerValues(...) failed that would be a 
            // more relevant error to return
            else if (rc == OK)
            {
                return powerResetRC;
            }
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PortPolicyCtrlImpl::DoSetOrientation
(
    CableOrientation orient,
    UINT16 orientDelay
)
{
    RC rc;

    if (!m_Init)
    {
        Printf(Tee::PriError, "Port Policy Controller not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    if (!m_pFtb)
    {
        Printf(Tee::PriError, "Port Policy Controller Functional Test Board not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    if (orient == USB_UNKNOWN_ORIENTATION)
    {
        Printf(Tee::PriError, "Unsupported Orientation requested\n");
        return RC::BAD_PARAMETER;
    }

    if (m_Orient == orient)
    {
        Printf(Tee::PriLow,
               "Functional Test Board already configured for %s cable orientation\n",
               (orient == USB_CABLE_ORIENT_ORIGINAL ? "original" : "flipped"));
        return rc;
    }

    Printf(Tee::PriLow,
           "Configuring Functional Test Board, setting %s orienatation\n",
           (orient == USB_CABLE_ORIENT_ORIGINAL ? "original" : "flipped"));

    bool isOrientFlippedBeforeConfig = false;
    CHECK_RC(IsOrientationFlipped(&isOrientFlippedBeforeConfig));

    CHECK_RC(m_pFtb->SetOrientation(orient, orientDelay));

    CHECK_RC(WaitForConfigToComplete());

    bool isOrientFlippedAfterConfig = false;
    CHECK_RC(IsOrientationFlipped(&isOrientFlippedAfterConfig));

    if (!m_SkipGpuOrientationCheck)
    {
        if (isOrientFlippedBeforeConfig == isOrientFlippedAfterConfig)
        {
            Printf(Tee::PriError,
                   "Orientatation flip failed. GPU's current orientation: %s. "
                   "Try using ResetFtb = true testarg\n",
                   isOrientFlippedAfterConfig ? "Flipped" : "Original");
            return RC::USBTEST_CONFIG_FAIL;
        }
        Printf(Tee::PriLow,
               "GPU's Orientation status\n"
               "    Previous: %s\n"
               "    Current : %s\n",
               (isOrientFlippedBeforeConfig ? "Flipped" : "Original"),
               (isOrientFlippedAfterConfig ? "Flipped" : "Original"));
    }
    else
    {
        Printf(Tee::PriLow,
               "Orientation check on GPU side skipped.\n");
    }

    Printf(Tee::PriLow,
           "Configuring Functional Test Board to switch orientation complete.\n");

    m_Orient = orient;
    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PortPolicyCtrlImpl::DoReverseOrientation()
{
    RC rc;
    if (!m_Init)
    {
        Printf(Tee::PriError, "Port Policy Controller not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    bool isOrientFlippedBeforeConfig = false;
    CHECK_RC(IsOrientationFlipped(&isOrientFlippedBeforeConfig));

    Printf(Tee::PriLow,
           "Configuring Functional Test Board to switch orientation.\n");

    CHECK_RC(m_pFtb->ReverseOrientation());

    CHECK_RC(WaitForConfigToComplete());

    bool isOrientFlippedAfterConfig = false;
    CHECK_RC(IsOrientationFlipped(&isOrientFlippedAfterConfig));

    if (!m_SkipGpuOrientationCheck)
    {
        if (isOrientFlippedBeforeConfig == isOrientFlippedAfterConfig)
        {
            Printf(Tee::PriError,
                   "Orientatation flip failed. GPU's current orientation: %s\n",
                   isOrientFlippedAfterConfig ? "Flipped" : "Original");
            return RC::USBTEST_CONFIG_FAIL;
        }
        Printf(Tee::PriLow,
               "GPU's Orientation status\n"
               "    Previous: %s\n"
               "    Current : %s\n",
               (isOrientFlippedBeforeConfig ? "Flipped" : "Original"),
               (isOrientFlippedAfterConfig ? "Flipped" : "Original"));
    }
    else
    {
        Printf(Tee::PriLow,
               "Orientation check on GPU side skipped.\n");
    }

    Printf(Tee::PriLow,
           "Configuring Functional Test Board to switch orientation complete.\n");

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PortPolicyCtrlImpl::DoGenerateCableAttachDetach
(
    CableAttachDetach eventType,
    UINT16 detachAttachDelay
)
{
    RC rc;

    if (!m_Init)
    {
        Printf(Tee::PriError, "Port Policy Controller not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    if (!m_pFtb)
    {
        Printf(Tee::PriError, "Port Policy Controller Functional Test Board not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriLow,
           "Configuring Functional Test Board, detach FTB after %u ms and reattach after %u ms\n",
           (eventType == USB_CABLE_DETACH ? detachAttachDelay : 0),
           (eventType == USB_CABLE_DETACH ? 0 : detachAttachDelay));

    CHECK_RC(m_pFtb->GenerateCableAttachDetach(eventType, detachAttachDelay));

    // Detaching/Attaching the cable doesn't change any configuration, last known
    // config is set.
    // Caller of this function should handle what happens after this event

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PortPolicyCtrlImpl::DoResetFtb()
{
    RC rc;

    if (!m_Init)
    {
        Printf(Tee::PriError, "Port Policy Controller not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    if (!m_pFtb)
    {
        Printf(Tee::PriError, "Port Policy Controller Functional Test Board not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriLow,
           "Configuring Functional Test Board, resetting to original settings\n");

    CHECK_RC(m_pFtb->Reset());

    CHECK_RC(WaitForConfigToComplete());

    // Set the modes to default values
    m_UsbAltMode = USB_ALT_MODE_0;
    m_UsbPowerMode = USB_DEFAULT_POWER_MODE;
    m_Orient = USB_CABLE_ORIENT_ORIGINAL;
    m_IsocDevice = USB_ISOC_DEVICE_DISABLE;
    m_DpLoopback = USB_DISABLE_DP_LOOPBACK;

    CHECK_RC(ConfirmAltModeConfig(m_UsbAltMode));

    /* TODO anahar: Add support to confirm power mode, orientation and isoc device */

    Printf(Tee::PriLow, "Functional Test Board reset successful!\n");

    return rc;
}


//-----------------------------------------------------------------------------
/* virtual */ RC PortPolicyCtrlImpl::DoGenerateInBandWakeup(UINT16 inBandDelay)
{
    RC rc;

    if (!m_Init)
    {
        Printf(Tee::PriError, "Port Policy Controller not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    if (!m_pFtb)
    {
        Printf(Tee::PriError, "Port Policy Controller Functional Test Board not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriLow,
           "Configuring Functional Test board to generate InBand wakeup after %u ms\n",
           inBandDelay);

    CHECK_RC(m_pFtb->GenerateInBandWakeup(inBandDelay));

    // There is no need to send a disconnect/reconnect VDM for Inband wakeup event
    // Also, there is no configuration update. An event will be generated by the FTB
    // after given milliseconds and caller of this function is responsible to handle that event.

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PortPolicyCtrlImpl::DoSetDpLoopback(DpLoopback dpLoopback)
{
    RC rc;

    if (!m_Init)
    {
        Printf(Tee::PriError, "Port Policy Controller not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    if (!m_pFtb)
    {
        Printf(Tee::PriError, "Port Policy Controller Functional Test Board not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    if (dpLoopback == USB_UNKNOWN_DP_LOOPBACK_STATUS)
    {
        Printf(Tee::PriError, "Unsupported DP Looback configuration requested\n");
        return RC::BAD_PARAMETER;
    }

    const string dpLoopbackStatus = (dpLoopback == USB_DISABLE_DP_LOOPBACK
                                     ? "disable"
                                     : "enable");

    if (m_DpLoopback == dpLoopback)
    {
        Printf(Tee::PriLow, "DpLoopback is already %sd\n", dpLoopbackStatus.c_str());
        return rc;
    }

    Printf(Tee::PriLow,
           "Configuring Functional Test board to %s DpLoopback Mode \n",
           dpLoopbackStatus.c_str());

    CHECK_RC(m_pFtb->SetDpLoopback(dpLoopback));

    CHECK_RC(WaitForConfigToComplete());

    // No configuration to check, ideally MODS DPLoopback test (test 99)
    // should be ilwoked after this function call
    
    m_DpLoopback = dpLoopback;

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PortPolicyCtrlImpl::DoSetIsocDevice(IsocDevice enableIsocDev)
{
    RC rc;

    bool configUpdated = false;
    if (!m_Init)
    {
        Printf(Tee::PriError, "Port Policy Controller not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    if (!m_pFtb)
    {
        Printf(Tee::PriError, "Port Policy Controller Functional Test Board not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    if (enableIsocDev == USB_UNKNOWN_ISOC_DEVICE_STATUS)
    {
        Printf(Tee::PriError, "Unsupported isochronous device configuration requested\n");
        return RC::BAD_PARAMETER;
    }

    const string isocDevStatus = (enableIsocDev == USB_ISOC_DEVICE_DISABLE
                                  ? "disable"
                                  : "enable");

    Printf(Tee::PriLow,
           "Configuring functional test board to %s isochronous device\n",
           isocDevStatus.c_str());

    if (m_IsocDevice == enableIsocDev)
    {
        Printf(Tee::PriLow,
               "isochronous device is already %sd on Functional Test board\n",
               isocDevStatus.c_str());
    }
    else
    {
        CHECK_RC(m_pFtb->SetIsocDevice(enableIsocDev));

        CHECK_RC(WaitForConfigToComplete());

        configUpdated = true;
    }

    if (enableIsocDev == USB_ISOC_DEVICE_ENABLE)
    {
        if (m_UsbAltMode != USB_ALT_MODE_3)
        {
            Printf(Tee::PriLow,
                   "Isochronous device can only be enumerated in Lwpu Test ALT mode."
                   " Configuring Functionsl test board for Lwpu Test ALT mode\n");

            // ISOC device only works in Lwpu Test ALT mode
            // Setting the ALT mode to USB_ALT_MODE_3 (Native USB mode)
            // will cause the configuration to enter Lwpu Test ALT mode 1st
            CHECK_RC(m_pFtb->SetAltMode(USB_ALT_MODE_3));

            CHECK_RC(WaitForConfigToComplete());

            configUpdated = true;
        }
        else
        {
            Printf(Tee::PriLow,
                   "Isochronous device can only be enumerated in Lwpu Test ALT mode."
                   " Functional Test Board already in Lwpu Test ALT mode\n");
        }
    }

    if (configUpdated)
    {
        if ((enableIsocDev == USB_ISOC_DEVICE_ENABLE) &&
            (m_UsbAltMode != USB_LW_TEST_ALT_MODE))
        {
            CHECK_RC(ConfirmAltModeConfig(USB_LW_TEST_ALT_MODE));
            m_UsbAltMode = USB_LW_TEST_ALT_MODE;
        }

        /* TODO anahar: add function to check ISOC device configuration*/

        Printf(Tee::PriLow,
               "Functional Test Board configuration successful, isochronous device %sd\n",
               isocDevStatus.c_str());

        m_IsocDevice = enableIsocDev;
    }

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PortPolicyCtrlImpl::DoPrintFtbConfigDetails () const
{
    RC rc;

    if (!m_Init)
    {
        Printf(Tee::PriError, "Port Policy Controller not initialized\n");
        return RC::SOFTWARE_ERROR;
    }
    Printf(Tee::PriLow,
           "Functional Test Board's current configuration:\n"); 

    Printf(Tee::PriLow,
           "    AltMode        : %u\n",
           static_cast<UINT08>(m_UsbAltMode));
    Printf(Tee::PriLow,
           "    PowerMode      : %u\n",
           static_cast<UINT08>(m_UsbPowerMode));
    Printf(Tee::PriLow,
           "    Orientation    : %s\n",
           (m_Orient == USB_CABLE_ORIENT_ORIGINAL ? "Original" : "Flipped"));
    Printf(Tee::PriLow,
           "    DP Loopback    : %s\n",
           (m_DpLoopback == USB_ENABLE_DP_LOOPBACK ? "Enable" : "Disable"));
    Printf(Tee::PriLow,
           "    Isoc Device    : %s\n",
           (m_IsocDevice == USB_ISOC_DEVICE_ENABLE ? "Enable" : "Disable"));

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PortPolicyCtrlImpl::DoGetLwrrAltMode(UsbAltModes *pLwrrAltMode) const
{
    RC rc;
    MASSERT(pLwrrAltMode);

    if (!m_pFtb)
    {
        Printf(Tee::PriError, "Port Policy Controller Functional Test Board not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    if (m_UsbAltMode == USB_UNKNOWN_ALT_MODE)
    {
        CHECK_RC(m_pFtb->GetLwrrUsbAltMode(pLwrrAltMode));
    }
    else
    {
        *pLwrrAltMode = m_UsbAltMode;
    }

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PortPolicyCtrlImpl::DoGetExpectedUsbDevForAltMode
(
    UsbAltModes altMode,
    UINT08 *pNumOfUsb3Dev,
    UINT08 *pNumOfUsb2Dev
) const
{
    RC rc;
    MASSERT(pNumOfUsb3Dev);
    MASSERT(pNumOfUsb2Dev);

    UsbAltModes lwrrAltMode = altMode;
    if (lwrrAltMode == USB_UNKNOWN_ALT_MODE)
    {
        CHECK_RC(GetLwrrAltMode(&lwrrAltMode));
    }

    const auto usbDevCnt = mapAltModeUsbDevCnt.find(lwrrAltMode);
    if (usbDevCnt == mapAltModeUsbDevCnt.end())
    {
        Printf(Tee::PriError,
               "Number of USB devices for current ALT mode (%u) not found\n",
               static_cast<UINT08>(lwrrAltMode));
        return RC::USBTEST_CONFIG_FAIL;
    }

    *pNumOfUsb3Dev = get<0>(usbDevCnt->second);
    *pNumOfUsb2Dev = get<1>(usbDevCnt->second);

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PortPolicyCtrlImpl::DoGetPpcFwVersion
(
    string *pPrimaryVersion,
    string *pSecondaryVersion
)
{
    RC rc;
    MASSERT(pPrimaryVersion);
    MASSERT(pSecondaryVersion);

    if (!m_pFtb)
    {
        Printf(Tee::PriError, "Port Policy Controller Functional Test Board not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    rc = m_pFtb->GetPpcFwVersion(pPrimaryVersion, pSecondaryVersion);
    if (rc != OK)
    {
        Printf(Tee::PriLow, "Unable to determine the PPC FW version\n");
        return rc;
    }

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PortPolicyCtrlImpl::DoGetPpcDriverVersion(string *pDriverVersion)
{
    RC rc;
    MASSERT(pDriverVersion);

    if (!m_pFtb)
    {
        Printf(Tee::PriError, "Port Policy Controller Functional Test Board not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    rc = m_pFtb->GetPpcDriverVersion(pDriverVersion);
    if (rc != OK)
    {
        Printf(Tee::PriLow, "Unable to determine the PPC driver version\n");
        return rc;
    }

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PortPolicyCtrlImpl::DoGetFtbFwVersion(string *pFtbFwVersion)
{
    StickyRC stickyRc;
    MASSERT(pFtbFwVersion);

    if (!m_pFtb)
    {
        Printf(Tee::PriError, "Port Policy Controller Functional Test Board not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    pFtbFwVersion->clear();
    stickyRc = m_pFtb->GetFtbFwVersion(pFtbFwVersion);
    if (stickyRc != OK)
    {
        Printf(Tee::PriError, "Unable to determine the FTB FW version.\n");
    }

    // If we read the FTB FW version via PPC driver, we need to get the
    // PPC back to it's default state since, GetFtbFwVersion() will leave
    // the FTB in Lwpu test ALT mode and no external device will be detected
    // unless the FTB is explicitely configured.
    if (dynamic_cast<VdmFtb *>(m_pFtb.get()) != nullptr)
    {
        RC resetFtbRc = ResetFtb();
        if (resetFtbRc != OK)
        {
            Printf(Tee::PriError,
                   "Unable to reset functional test board after reading the FW version. "
                   "Run without -usb_print_ftb_fw_version arg\n");
            stickyRc = resetFtbRc;
        }
    }

    return stickyRc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PortPolicyCtrlImpl::DoPrintPowerDrawnByFtb()
{
    RC rc;
    bool skipPowerCheck = true;
    bool printPowerValue = true;
    CHECK_RC(VerifyPrintPowerValues(m_UsbPowerMode, skipPowerCheck, printPowerValue));
    return rc;
}

//-----------------------------------------------------------------------------
RC PortPolicyCtrlImpl::VerifyPrintPowerValues
(
    UsbPowerModes powerMode,
    bool skipPowerCheck,
    bool printPowerValue
) const
{
    RC rc;

    FLOAT64 actualLwrrentA = 0.0;
    FLOAT64 actualVoltageV = 0.0;

    CHECK_RC(m_pFtb->GetPowerDrawn(&actualVoltageV, &actualLwrrentA));

    if (printPowerValue)
    {
        Printf(Tee::PriNormal,
               "Power consumption for power mode (%u)\n"
               "    Voltage: %.02f volts\n"
               "    Current: %.02f amps\n",
               static_cast<UINT08>(powerMode),
               actualVoltageV,
               actualLwrrentA);
    }

    if (skipPowerCheck)
    {
        return rc;
    }

    FLOAT64 expectedLwrrentA = 0.0;
    FLOAT64 expectedVoltageV = 0.0;
    switch (powerMode)
    {
        case USB_POWER_MODE_0:
            expectedLwrrentA = 3.0;
            expectedVoltageV = 5.0;
            break;

        case USB_POWER_MODE_1:
            expectedLwrrentA = 3.0;
            expectedVoltageV = 9.0;
            break;

        case USB_POWER_MODE_2:
            expectedLwrrentA = 2.25;
            expectedVoltageV = 12.0;
            break;

        default:
            Printf(Tee::PriError,
                   "Incorrect power mode (%u) selected\n",
                   static_cast<UINT08>(powerMode));
            return RC::USBTEST_CONFIG_FAIL;
    }

    FLOAT64 maxLwrrentA = expectedLwrrentA + (expectedLwrrentA * m_LwrrentMarginPercent / 100.0);
    FLOAT64 minLwrrentA = expectedLwrrentA - (expectedLwrrentA * m_LwrrentMarginPercent / 100.0);
    FLOAT64 maxVoltageV = expectedVoltageV + (expectedVoltageV * m_VoltageMarginPercent / 100.0);
    FLOAT64 milwoltageV = expectedVoltageV - (expectedVoltageV * m_VoltageMarginPercent / 100.0);

    if ((milwoltageV > actualVoltageV) || (actualVoltageV > maxVoltageV))
    {
        Printf(Tee::PriError,
               "Incorrect Voltage for Power Mode %u \n"
               "    Min Expected Voltage = %0.2f Volts\n"
               "    Max Expected Voltage = %0.2f Volts\n"
               "    Actual Voltage       = %0.2f Volts\n",
               static_cast<UINT08>(powerMode),
               milwoltageV,
               maxVoltageV,
               actualVoltageV);
        rc = RC::VOLTAGE_OUT_OF_RANGE;
    }

    if ((minLwrrentA > actualLwrrentA) || (actualLwrrentA > maxLwrrentA))
    {
        Printf(Tee::PriError,
               "Incorrect Current for Power Mode %u \n"
               "    Min Expected Current = %0.2f Amps\n"
               "    Max Expected Current = %0.2f Amps\n"
               "    Actual Current       = %0.2f Amps\n",
               static_cast<UINT08>(powerMode),
               minLwrrentA,
               maxLwrrentA,
               actualLwrrentA);
        rc.Clear();
        if (minLwrrentA > actualLwrrentA)
        {
            rc = RC::POWER_TOO_LOW;
        }
        else
        {
            rc = RC::POWER_TOO_HIGH;
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC PortPolicyCtrlImpl::DoConfirmAltModeConfig(UsbAltModes altMode)
{
    RC rc;

    CHECK_RC(m_pFtb->ConfirmUsbAltModeConfig(altMode));

    if (m_SkipUsbEnumerationCheck)
    {
        return rc;
    }

    // Get expected number of devices
    UINT08 expectedUsb3Dev = 0;
    UINT08 expectedUsb2Dev = 0;
    CHECK_RC(GetExpectedUsbDevForAltMode(altMode, &expectedUsb3Dev, &expectedUsb2Dev));

    if (!GetDevice()->SupportsInterface<XusbHostCtrl>())
    {
        Printf(Tee::PriError,
               "USB Host Controller not found. Unable to find connected devices\n");
        return RC::USBTEST_CONFIG_FAIL;
    }
    // Retrieve the actual number of USB devices connected to the host controller
    UINT08 attachedUsb3_1Dev = 0;
    UINT08 attachedUsb3_0Dev = 0;
    UINT08 attachedUsb2Dev = 0;
    auto xusbHostCtrl = GetDevice()->GetInterface<XusbHostCtrl>();

    // TODO anahar: add polling for devices which are slower to
    // reenumerate
    CHECK_RC(xusbHostCtrl->GetNumOfConnectedUsbDev(&attachedUsb3_1Dev,
                                                   &attachedUsb3_0Dev,
                                                   &attachedUsb2Dev));

    if ((expectedUsb3Dev != (attachedUsb3_1Dev + attachedUsb3_0Dev))
            || (expectedUsb2Dev != attachedUsb2Dev))
    {
        Printf(Tee::PriError,
               "Incorrect number of USB devices enumerated in AltMode(%u)\n"
               "  Expected devices\n"
               "    USB 3         : %u\n"
               "    USB 2         : %u\n"
               "  Connected devices\n"
               "    USB 3         : %u\n"
               "    USB 2         : %u\n",
               altMode,
               expectedUsb3Dev,
               expectedUsb2Dev,
               attachedUsb3_1Dev + attachedUsb3_0Dev,
               attachedUsb2Dev);
        return RC::USBTEST_CONFIG_FAIL;
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC PortPolicyCtrlImpl::DoGetAspmState(UINT32* pState)
{
    MASSERT(pState);
    RC rc;

    UINT08 pciCapBase = 0;
    CHECK_RC(Pci::FindCapBase(m_Domain,
                              m_Bus,
                              m_Device,
                              m_Function,
                              PCI_CAP_ID_PCIE,
                              &pciCapBase));

    UINT32 linkCtrl = 0;
    CHECK_RC(Platform::PciRead32(m_Domain,
                                 m_Bus,
                                 m_Device,
                                 m_Function,
                                 pciCapBase + LW_PCI_CAP_PCIE_LINK_CTRL,
                                 &linkCtrl));

    *pState = DRF_VAL(_PCI_CAP, _PCIE_LINK_CTRL, _ASPM, linkCtrl);

    return OK;
}

//-----------------------------------------------------------------------------
RC PortPolicyCtrlImpl::DoSetAspmState(UINT32 state)
{
    RC rc;

    UINT08 pciCapBase = 0;
    CHECK_RC(Pci::FindCapBase(m_Domain,
                              m_Bus,
                              m_Device,
                              m_Function,
                              PCI_CAP_ID_PCIE,
                              &pciCapBase));

    UINT32 linkCtrl = 0;
    CHECK_RC(Platform::PciRead32(m_Domain,
                                 m_Bus,
                                 m_Device,
                                 m_Function,
                                 pciCapBase + LW_PCI_CAP_PCIE_LINK_CTRL,
                                 &linkCtrl));

    linkCtrl = FLD_SET_DRF_NUM(_PCI_CAP, _PCIE_LINK_CTRL, _ASPM, state, linkCtrl);

    CHECK_RC(Platform::PciWrite32(m_Domain,
                                  m_Bus,
                                  m_Device,
                                  m_Function,
                                  pciCapBase + LW_PCI_CAP_PCIE_LINK_CTRL,
                                  linkCtrl));

    return OK;
}
