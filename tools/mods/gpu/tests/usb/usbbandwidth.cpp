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

#include "core/include/deprecat.h"
#include "device/interface/portpolicyctrl.h"
#include "device/interface/xusbhostctrl.h"
#include "gpu/include/gpudev.h"
#include "gpu/tests/gputest.h"
#include "gpu/usb/usbdatatransfer.h"

//TODO - Add debug prints
class UsbBandwidth : public GpuTest
{
public:
    UsbBandwidth();

    bool IsSupported() override
    {
        if (GetBoundGpuSubdevice()->IsSOC()
                || GetBoundGpuDevice()->GetFamily() < GpuDevice::Volta)
        {
            return false;
        }

        if (!GetBoundGpuSubdevice()->SupportsInterface<XusbHostCtrl>())
        {
            Printf(Tee::PriLow,
                   "XUSB Host Controller not supported.\n");
            return false;
        }

        if (!GetBoundGpuSubdevice()->SupportsInterface<PortPolicyCtrl>())
        {
            Printf(Tee::PriLow,
                   "Port Policy Controller not supported.\n");
            return false;
        }

        return true;
    }

    RC Setup() override;
    RC Run() override;
    RC Cleanup() override;
    RC XusbHcWakeupsByFTB();
    RC EnableLowPowerModeForHc();
    RC CheckIfHcIsOutOfLowPower();

    void PrintJsProperties(Tee::Priority pri) override;

    // JS property accessors
    SETGET_PROP(DataTraffic,        UINT32);
    SETGET_PROP(Loops,              UINT32);
    SETGET_PROP(TransferMode,       UINT32);
    SETGET_PROP(FileName,           string);
    SETGET_PROP(Blocks,             UINT16);
    SETGET_PROP(ShowBandwidthData,  bool);
    SETGET_PROP(TimeMs,             UINT64);
    SETGET_PROP(ScsiCmd,            UINT08);
    SETGET_PROP(KeepRunning,        bool);
    SETGET_PROP(EnumerateOnly,      bool);
    SETGET_PROP(SkipDataTransferUsbCntCheck,  bool);
    SETGET_PROP(DataRate,           UINT08);
    SETGET_PROP(DataRateResetAtTestStart, bool);
    SETGET_PROP(DataRateResetAtTestEnd,   bool);
    SETGET_PROP(DataBubbleWaitMs,   UINT64);
    SETGET_PROP(XusbHcWakeup,       UINT08);
    SETGET_PROP(AutoSuspendTimeOutMs, UINT64);
    SETGET_PROP(AllowUsbDeviceOnIntelHost, bool);
    SETGET_PROP(WaitBeforeClaimingUsbDevMs, UINT32);

    bool GetResetDataRate()
    {
        return m_ResetDataRate || m_DataRateResetAtTestEnd;
    }
    RC SetResetDataRate(bool resetDataRate)
    {
        RC rc;
        m_DataRateResetAtTestEnd = m_DataRateResetAtTestEnd || resetDataRate;
        static Deprecation deprec(
                "ResetDataRate", "8/20/2018",
                "ResetDataRate has been deprecated. Instead use:\n"
                "    DataRateResetAtTestStart: To reset data rate at start of the test. "
                "Default: FALSE, data rate will not be reset at start of test\n"
                "    DataRateResetAtTestEnd: To reset data rate at end of the test. "
                "Default: TRUE, data rate will be reset at end of test\n"
                "NOTE: You should not need to use any of the above arguments "
                "if you don't want to do anything special with data rate limiting. "
                "The test will reset the data rate to Super Speed Plus once it is complete\n.");
        Deprecation::Allow("SetResetDataRate");

        JavaScriptPtr pJs;
        JSContext *pContext;
        CHECK_RC(pJs->GetContext(&pContext));
        if (!deprec.IsAllowed(pContext))
        {
            return RC::BAD_PARAMETER;
        }
        return rc;
    }

    bool GetHostInitiatedWakeup()
    {
        return (m_HostInitiatedWakeup || (m_XusbHcWakeup == USB_HC_HOST_INITIATED_WAKEUP));
    }
    RC SetHostInitiatedWakeup(bool hostInitiatedWakeup)
    {
        RC rc;
        static Deprecation deprec(
                "HostInitiatedWakeup", "9/15/2018",
                "HostInitiatedWakeup has been deprecated. Instead use testarg XusbHcWakeup\n");
        Deprecation::Allow("SetHostInitiatedWakeup");

        JavaScriptPtr pJs;
        JSContext *pContext;
        CHECK_RC(pJs->GetContext(&pContext));
        if (!deprec.IsAllowed(pContext))
        {
            return RC::BAD_PARAMETER;
        }

        if (m_XusbHcWakeup != USB_HC_NO_WAKEUPS)
        {
            Printf(Tee::PriError,
                   "Testarg XusbHcWakeup and HostInitiatedWakeup cannot be used at "
                   "the same time. Remove testarg HostInitiatedWakeup\n");
            return RC::BAD_PARAMETER;
        }

        if (hostInitiatedWakeup)
        {
            m_XusbHcWakeup = USB_HC_HOST_INITIATED_WAKEUP;
        }

        return rc;
    }

private:
    enum XusbHcWakeupType
    {
        USB_HC_HOST_INITIATED_WAKEUP
       ,USB_HC_WAKEUP_DUE_TO_FTB
       /* New wakeup event goes here */
       ,USB_HC_NO_WAKEUPS
    };

    // JS properties
    UINT32 m_DataTraffic;
    UINT32 m_Loops;
    UINT32 m_TransferMode;
    string m_FileName;
    UINT16 m_Blocks;
    bool   m_ShowBandwidthData;
    UINT64 m_TimeMs;
    UINT08 m_ScsiCmd;
    UINT32 m_Seed;
    bool   m_KeepRunning;
    bool   m_EnumerateOnly;
    bool   m_SkipDataTransferUsbCntCheck;
    UINT08 m_DataRate;
    bool   m_ResetDataRate;
    bool   m_DataRateResetAtTestStart;
    bool   m_DataRateResetAtTestEnd;
    UINT64 m_DataBubbleWaitMs;
    UINT08 m_XusbHcWakeup;
    bool   m_HostInitiatedWakeup;
    UINT64 m_AutoSuspendTimeOutMs;
    bool   m_AllowUsbDeviceOnIntelHost;
    UINT32 m_WaitBeforeClaimingUsbDevMs;

    UsbDataTransfer m_UsbTransfer;
    XusbHostCtrl *m_pXusbHostCtrl;
    PortPolicyCtrl *m_pPortPolicyCtrl;
    XusbHostCtrl::HostCtrlDataRate m_DefaultDataRate;
};

UsbBandwidth::UsbBandwidth()
    :m_DataTraffic(UsbDataTransfer::USB_DATA_MODE_RANDOM)
    ,m_Loops(1)
    ,m_TransferMode(UsbDataTransfer::USB_DATA_MODE_RD_WR)
    ,m_Blocks(UsbDataTransfer::USB_DEFAULT_NUM_OF_BLOCKS)
    ,m_ShowBandwidthData(false)
    ,m_TimeMs(0)
    ,m_ScsiCmd(UsbDataTransfer::USB_READ_WRITE_10)
    ,m_Seed(1)
    ,m_KeepRunning(false)
    ,m_EnumerateOnly(false)
    ,m_SkipDataTransferUsbCntCheck(false)
    ,m_DataRate(XusbHostCtrl::USB_UNKNOWN_DATA_RATE)
    ,m_ResetDataRate(false)
    ,m_DataRateResetAtTestStart(false)
    ,m_DataRateResetAtTestEnd(true)
    ,m_DataBubbleWaitMs(0)
    ,m_XusbHcWakeup(USB_HC_NO_WAKEUPS)
    ,m_HostInitiatedWakeup(false)
    ,m_AutoSuspendTimeOutMs(10)
    ,m_AllowUsbDeviceOnIntelHost(false)
    ,m_WaitBeforeClaimingUsbDevMs(0)
    ,m_pXusbHostCtrl(nullptr)
    ,m_pPortPolicyCtrl(nullptr)
    ,m_DefaultDataRate(XusbHostCtrl::USB_SUPER_SPEED_PLUS)
{
    SetName("UsbBandwidth");
}

RC UsbBandwidth::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());

    if (m_EnumerateOnly)
    {
        if (m_XusbHcWakeup == USB_HC_HOST_INITIATED_WAKEUP)
        {
            Printf(Tee::PriError,
                   "Host initiated wakeup needs USB data transfer to be performed\n");
            return RC::BAD_PARAMETER;
        }
        Printf(Tee::PriLow,
               "EnumerateOnly set to true. No USB data transfers will be performed\n");
    }
    else
    {
        if (!m_ShowBandwidthData)
        {
            Printf(Tee::PriLow, "Recommend using \"-testarg UsbBandwidth ShowBandwidthData true\"\n");
        }

        if (!m_Loops)
        {
            Printf(Tee::PriError, "Number of loops must be greater than 0\n");
            return RC::BAD_PARAMETER;
        }

        if (!m_Blocks)
        {
            Printf(Tee::PriError, "Number of blocks must be greater than 0\n");
            return RC::BAD_PARAMETER;
        }

        m_Seed = GetTestConfiguration()->Seed();
    }

    CHECK_RC(m_UsbTransfer.Initialize(GetBoundGpuSubdevice()));
    if (m_AllowUsbDeviceOnIntelHost)
    {
        Printf(Tee::PriLow,
               "Test will check for USB devices connected to all XUSB host controllers. "
               "Make sure USB devices are connected to only 1 root hub\n");
        CHECK_RC(m_UsbTransfer.AllowUsbDeviceOnIntelHost(m_AllowUsbDeviceOnIntelHost));
    }
    CHECK_RC(m_UsbTransfer.WaitBeforeClaimingUsbDevMs(m_WaitBeforeClaimingUsbDevMs));

    m_pPortPolicyCtrl = GetBoundGpuSubdevice()->GetInterface<PortPolicyCtrl>();
    if (!m_SkipDataTransferUsbCntCheck)
    {
        PortPolicyCtrl::UsbAltModes lwrrAltMode;
        UINT08 expectedUsb3Dev = 0;
        UINT08 expectedUsb2Dev = 0;
        CHECK_RC(m_pPortPolicyCtrl->GetLwrrAltMode(&lwrrAltMode));
        CHECK_RC(m_pPortPolicyCtrl->GetExpectedUsbDevForAltMode(lwrrAltMode,
                                                                &expectedUsb3Dev,
                                                                &expectedUsb2Dev));
        if (m_DataRate == XusbHostCtrl::USB_HIGH_SPEED)
        {
            expectedUsb3Dev = 0;
        }
        CHECK_RC(m_UsbTransfer.SetExpectedUsbDevices(expectedUsb3Dev, expectedUsb2Dev));
    }

    m_pXusbHostCtrl = GetBoundGpuSubdevice()->GetInterface<XusbHostCtrl>();
    CHECK_RC(m_pXusbHostCtrl->MapAvailBARs());

    bool skipDataRateCheck = true;
    if (m_DataRateResetAtTestStart && (m_DataRate != m_DefaultDataRate))
    {
        Printf(Tee::PriLow, "Resetting DataRate at test start\n");
        CHECK_RC(m_pXusbHostCtrl->LimitDataRate(m_DefaultDataRate, skipDataRateCheck));
    }

    return rc;
}

RC UsbBandwidth::Run()
{
    RC rc;

    if (m_DataRate != XusbHostCtrl::USB_UNKNOWN_DATA_RATE)
    {
        Printf(Tee::PriLow, "Limiting DataRate\n");
        bool skipDataRateCheck = false;
        CHECK_RC(m_pXusbHostCtrl->LimitDataRate(
                            static_cast<XusbHostCtrl::HostCtrlDataRate>(m_DataRate),
                            skipDataRateCheck));
    }

    if (m_XusbHcWakeup == USB_HC_HOST_INITIATED_WAKEUP)
    {
        CHECK_RC(EnableLowPowerModeForHc());
    }
    else if (m_XusbHcWakeup == USB_HC_WAKEUP_DUE_TO_FTB)
    {
        CHECK_RC(XusbHcWakeupsByFTB());
    }

    if (m_EnumerateOnly)
    {
        CHECK_RC(m_UsbTransfer.EnumerateDevices());
    }
    else
    {

        CHECK_RC(m_UsbTransfer.OpenAvailUsbDevices());

        bool reOpenDevices = false;
        bool closeDevices = false;
        bool printBwInUtilityFunc = false;
        map<UsbDataTransfer::TupleDevInfo, UsbDataTransfer::BwDataGbps> mapDevTotalBw;

        do
        {
            CHECK_RC(m_UsbTransfer.TransferUsbData(reOpenDevices,
                                     closeDevices,
                                     printBwInUtilityFunc,
                                     m_Loops,
                                     m_Blocks,
                                     m_DataTraffic,
                                     m_FileName,
                                     m_TransferMode,
                                     m_TimeMs,
                                     m_Seed,
                                     static_cast<UsbDataTransfer::ScsiCommand>(m_ScsiCmd),
                                     m_DataBubbleWaitMs));

            if (m_ShowBandwidthData)
            {
                map<UsbDataTransfer::TupleDevInfo, UsbDataTransfer::BwDataGbps> mapDevLwrrBw;
                CHECK_RC(m_UsbTransfer.CallwlateBwGbps(&mapDevLwrrBw));
                for (const auto &bwItr : mapDevLwrrBw)
                {
                    mapDevTotalBw[bwItr.first].readTimeSec
                                        += mapDevLwrrBw[bwItr.first].readTimeSec;
                    mapDevTotalBw[bwItr.first].readGbits
                                        += mapDevLwrrBw[bwItr.first].readGbits;
                    mapDevTotalBw[bwItr.first].writeTimeSec
                                        += mapDevLwrrBw[bwItr.first].writeTimeSec;
                    mapDevTotalBw[bwItr.first].writeGbits
                                        += mapDevLwrrBw[bwItr.first].writeGbits;
                }
            }
        } while(m_KeepRunning);
        if (m_ShowBandwidthData)
        {
            m_UsbTransfer.PrintBandwidthData(mapDevTotalBw);
        }
        CHECK_RC(m_UsbTransfer.CloseUsbDevices());
    }

    if (m_XusbHcWakeup == USB_HC_HOST_INITIATED_WAKEUP)
    {
        CHECK_RC(CheckIfHcIsOutOfLowPower());
    }

    return rc;
}

RC UsbBandwidth::Cleanup()
{
    RC rc;

    // Reset the data rate back to Super Speed plus if data rate has been updated during
    // this run
    if (m_DataRateResetAtTestEnd)
    {
        if ((m_DataRate != XusbHostCtrl::USB_UNKNOWN_DATA_RATE)
             && (m_DataRate != m_DefaultDataRate))
        {
            Printf(Tee::PriLow, "Resetting DataRate at test end\n");
            bool skipDataRateCheck = true;
            CHECK_RC(m_pXusbHostCtrl->LimitDataRate(m_DefaultDataRate, skipDataRateCheck));
        }
        else
        {
            Printf(Tee::PriLow,
                   "DataRate was not changed during this run, it will not be reset. "
                   "Please use DataRateResetAtTestStart if you wish to reset the DataRate.\n");
        }
    }
    else
    {
        Printf(Tee::PriLow, "Skipping DataRate reset. DataRate will be %u\n", m_DataRate);
    }

    if (m_pXusbHostCtrl)
    {
        m_pXusbHostCtrl->UnMapAvailBARs();
    }

    return rc;
}

RC UsbBandwidth::XusbHcWakeupsByFTB()
{
    RC rc;

    // Get Current ALT mode
    PortPolicyCtrl::UsbAltModes lwrrAltMode = PortPolicyCtrl::USB_UNKNOWN_ALT_MODE;
    CHECK_RC(m_pPortPolicyCtrl->GetLwrrAltMode(&lwrrAltMode));

    UINT16 delay = 0;
    UINT08 attachedUsb3_1Dev = 0;
    UINT08 attachedUsb3_0Dev = 0;
    UINT08 attachedUsb2Dev = 0;

    CHECK_RC(m_pXusbHostCtrl->GetNumOfConnectedUsbDev(&attachedUsb3_1Dev,
                                                      &attachedUsb3_0Dev,
                                                      &attachedUsb2Dev));
    if (!attachedUsb3_1Dev && !attachedUsb3_0Dev && !attachedUsb2Dev)
    {
        Printf(Tee::PriError,
               "No external USB device connected to XUSB host controller, "
               "cannot perform wakeup due to FTB's attach/detach\n");
        return RC::USBTEST_CONFIG_FAIL;

    }
    CHECK_RC(EnableLowPowerModeForHc());
    CHECK_RC(m_pPortPolicyCtrl->GenerateCableAttachDetach(PortPolicyCtrl::USB_CABLE_DETACH,
                                                          delay));
    CHECK_RC(m_pPortPolicyCtrl->WaitForConfigToComplete());
    CHECK_RC(m_pPortPolicyCtrl->ConfirmAltModeConfig(lwrrAltMode));

    CHECK_RC(CheckIfHcIsOutOfLowPower());

    return rc;
}

RC UsbBandwidth::EnableLowPowerModeForHc()
{
    RC rc;

    bool isLowPowerStateEnabled = false;
    CHECK_RC(m_pXusbHostCtrl->IsLowPowerStateEnabled(&isLowPowerStateEnabled));
    if (!isLowPowerStateEnabled)
    {
        CHECK_RC(m_pXusbHostCtrl->EnableLowPowerState(m_AutoSuspendTimeOutMs));
    }

    return rc;
}

RC UsbBandwidth::CheckIfHcIsOutOfLowPower()
{
    RC rc;
    bool isLowPowerStateEnabled = false;
    CHECK_RC(m_pXusbHostCtrl->IsLowPowerStateEnabled(&isLowPowerStateEnabled));
    if (isLowPowerStateEnabled)
    {
        Printf(Tee::PriError,
               "XUSB Host controller still in D3. Wakeup failed\n");
        return RC::USBTEST_CONFIG_FAIL;
    }
    return rc;
}

void UsbBandwidth::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);
    Printf(pri, "UsbBandwidth Js Properties:\n");
    Printf(pri, "\tDataTraffic:                    %u\n", m_DataTraffic);
    Printf(pri, "\tTransferMode:                   %u\n", m_TransferMode);
    Printf(pri, "\tFileName:                       %s\n", m_FileName.c_str());
    Printf(pri, "\tBlocks:                         %u\n", m_Blocks);
    Printf(pri, "\tShowBandwidthData:              %s\n", m_ShowBandwidthData? "true" : "false");
    Printf(pri, "\tTimeMs:                         %llu\n", m_TimeMs);
    Printf(pri, "\tScsiCmd:                        %u\n", m_ScsiCmd);
    Printf(pri, "\tKeepRunning:                    %s\n", m_KeepRunning? "true" : "false");
    Printf(pri, "\tEnumerateOnly:                  %s\n", m_EnumerateOnly? "true" : "false");
    Printf(pri, "\tSkipDataTransferUsbCntCheck:    %s\n",
            m_SkipDataTransferUsbCntCheck ? "true" : "false");
    Printf(pri, "\tDataRate:                       %u\n", m_DataRate);
    Printf(pri, "\tResetDataRate:                  %s\n", m_ResetDataRate? "true" : "false");
    Printf(pri,
           "\tDataRateResetAtTestStart:            %s\n",
           m_DataRateResetAtTestStart ? "true" : "false");
    Printf(pri,
           "\tDataRateResetAtTestEnd:          %s\n",
           m_DataRateResetAtTestEnd ? "true" : "false");
    Printf(pri, "\tDataBubbleWaitMs:               %llu\n", m_DataBubbleWaitMs);
    Printf(pri, "\tXusbHcWakeup:                   %u\n", m_XusbHcWakeup);
    Printf(pri, "\tHostInitiatedWakeup:            %s\n", m_HostInitiatedWakeup? "true" : "false");
    Printf(pri, "\tAutoSuspendTimeOutMs:           %llu\n", m_AutoSuspendTimeOutMs);
    Printf(pri, "\tAllowUsbDeviceOnIntelHost:      %s\n",
            m_AllowUsbDeviceOnIntelHost ? "true" : "false");
    Printf(pri, "\tWaitBeforeClaimingUsbDevMs:     %u\n", m_WaitBeforeClaimingUsbDevMs);
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

JS_CLASS_INHERIT(UsbBandwidth, GpuTest,
                 "USB Bandwidth Stress test");

CLASS_PROP_READWRITE(UsbBandwidth, DataTraffic, UINT32,
                 "UINT32: 0->Random Data, 1->All 0's, 2->All 1's, "
                 "3->Spec defined data, 4->Alternate 0xAAAAAAAA 0x55555555");

CLASS_PROP_READWRITE(UsbBandwidth, Loops, UINT32,
                 "UINT32: number of times to write to / read from the device. Default: 1");

CLASS_PROP_READWRITE(UsbBandwidth, TransferMode, UINT32,
                 "UINT32: 0->Write-Read-Compare, 1->Write Only, 2->Read Only. Default: 0");

CLASS_PROP_READWRITE(UsbBandwidth, FileName, string,
                 "string: Spec file containing USB data pattern.");

CLASS_PROP_READWRITE(UsbBandwidth, Blocks, UINT16,
                 "UINT16: number of blocks to write to / read from the device in a "
                 "single loop. Default: 12800");

CLASS_PROP_READWRITE(UsbBandwidth, ShowBandwidthData, bool,
                 "bool: Print out bandwidth data. Default: false");

CLASS_PROP_READWRITE(UsbBandwidth, TimeMs, UINT64,
                 "UINT64: Run test for X milliseconds. "
                 "Default: 0 (will run until desired number of blocks/file is transferred");

CLASS_PROP_READWRITE(UsbBandwidth, ScsiCmd, UINT08,
                 "UINT08: SCSI command to use for data transfer "
                 "0->WRITE(10)/READ(10), 1->WRITE(12)/READ(12), 2->WRITE(16)/READ(16). "
                 "Default: 0");

CLASS_PROP_READWRITE(UsbBandwidth, KeepRunning, bool,
                     "bool: The test will keep running as long as this flag is set.");

CLASS_PROP_READWRITE(UsbBandwidth, EnumerateOnly, bool,
                     "bool: Only enumerate the devices, no data transfers.");

CLASS_PROP_READWRITE(UsbBandwidth, SkipDataTransferUsbCntCheck, bool,
                     "bool: Check if correct number of devices are available for data transfer.");

CLASS_PROP_READWRITE(UsbBandwidth, DataRate, UINT08,
                     "UINT08: Limit data rate of XUSB host controller. "
                     "0->High Speed, 1->Super Speed, 2->Super Speed Plus. Default: none");

CLASS_PROP_READWRITE(UsbBandwidth, ResetDataRate, bool,
                     "bool: Reset Data rate to Super Speed Plus. Default: false");

CLASS_PROP_READWRITE(UsbBandwidth, DataRateResetAtTestStart, bool,
                     "bool: Reset Data rate to Super Speed Plus at start of test. "
                     "Default: false, data rate will not be reset at start of test");

CLASS_PROP_READWRITE(UsbBandwidth, DataRateResetAtTestEnd, bool,
                     "bool: Reset Data rate to Super Speed Plus at end of test. "
                     "Default: true, data rate will be reset at end of test");

CLASS_PROP_READWRITE(UsbBandwidth, DataBubbleWaitMs, UINT64,
                     "UINT64: Time in MS to wait between data transfers.");

CLASS_PROP_READWRITE(UsbBandwidth, XusbHcWakeup, UINT08,
                     "UINT08: Perform XUSB host controller wakeups. "
                     "Default: None, No wakeups will be performed");

CLASS_PROP_READWRITE(UsbBandwidth, HostInitiatedWakeup, bool,
                     "bool: Perform host initiated wakeups. "
                     "Default: false, No host initiated wakeups will be performed");

CLASS_PROP_READWRITE(UsbBandwidth, AutoSuspendTimeOutMs, UINT64,
                     "UINT64: Time in MS after which the host controller should enter low power. "
                     "Default: 10ms");

CLASS_PROP_READWRITE(UsbBandwidth, AllowUsbDeviceOnIntelHost, bool,
                     "bool: Check for USB devices on any XUSB host Controller. Default: false");

CLASS_PROP_READWRITE(UsbBandwidth, WaitBeforeClaimingUsbDevMs, UINT32,
                     "UINT32: Wait for the kernel to be ready before claiming the USB drive");
