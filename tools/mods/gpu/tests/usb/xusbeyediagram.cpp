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

#include "device/interface/xusbhostctrl.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/gputest.h"

class XusbEyeDiagram : public GpuTest
{
public:
    XusbEyeDiagram();
    virtual ~XusbEyeDiagram() {}

    bool IsSupported() override
    {
        if (!GetBoundGpuSubdevice()->SupportsInterface<XusbHostCtrl>())
        {
            Printf(Tee::PriLow,
                   "XUSB Host Controller not supported.\n");
            return false;
        }
        return true;
    }

    RC Setup();
    RC Run();
    RC Cleanup();

    SETGET_PROP(PrintPerLoop,   bool);
    SETGET_PROP(XPassThreshold, UINT32);
    SETGET_PROP(XFailThreshold, UINT32);
    SETGET_PROP(YPassThreshold, UINT32);
    SETGET_PROP(YFailThreshold, UINT32);
    SETGET_PROP(DumpCfgRegisters, bool);

private:
    UINT32 m_NumLoops        = 10;
    bool   m_PrintPerLoop    = false;
    UINT32 m_XPassThreshold  = 0;
    UINT32 m_XFailThreshold  = 0;
    UINT32 m_YPassThreshold  = 0;
    UINT32 m_YFailThreshold  = 0;
    UINT32 m_DumpCfgRegisters = false;
    XusbHostCtrl *m_pXusbHostCtrl = nullptr;

    RC PrintEomStatus(Tee::Priority pri,
                      string mode,
                      string type,
                      XusbHostCtrl::LaneDeviceInfo laneDeviceInfo,
                      UINT32 status);
    RC CheckZeroEye(string mode,
                    string type,
                    XusbHostCtrl::LaneDeviceInfo laneDeviceInfo,
                    UINT32 val);
    RC CheckAvgVals(string mode,
                    string type,
                    XusbHostCtrl::LaneDeviceInfo laneDeviceInfo,
                    UINT32 val,
                    UINT32 passThreshold,
                    UINT32 failThreshold);
};

XusbEyeDiagram::XusbEyeDiagram()
{
    SetName("XusbEyeDiagram");
}

RC XusbEyeDiagram::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());

    if (m_DumpCfgRegisters && !m_PrintPerLoop)
    {
        Printf (Tee::PriError,
                "DumpCfgRegisters testarg is valid only if PrintPerLoop = true\n");
        return RC::ILWALID_ARGUMENT;
    }

    m_NumLoops = GetTestConfiguration()->Loops();
    if (m_NumLoops == 0)
    {
        Printf(Tee::PriError, "Loops cannot be zero\n");
        return RC::ILWALID_ARGUMENT;
    }

    if (m_XFailThreshold > m_XPassThreshold)
    {
        Printf(Tee::PriError, "XFailThreshold cannot be greater than XPassThreshold\n");
        return RC::ILWALID_ARGUMENT;
    }
    if (m_YFailThreshold > m_YPassThreshold)
    {
        Printf(Tee::PriError, "YFailThreshold cannot be greater than YPassThreshold\n");
        return RC::ILWALID_ARGUMENT;
    }

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        SetVerbosePrintPri(Tee::PriNone);
    }

    m_pXusbHostCtrl = GetBoundGpuSubdevice()->GetInterface<XusbHostCtrl>();
    if (!m_pXusbHostCtrl)
    {
        Printf(Tee::PriError,
               "USB Host Controller interface not found\n");
        return RC::USBTEST_CONFIG_FAIL;
    }
    CHECK_RC(m_pXusbHostCtrl->MapAvailBARs());

    return rc;
}

RC XusbEyeDiagram::Run()
{
    RC rc;

    const FLOAT64 timeoutMs = GetTestConfiguration()->TimeoutMs();
    const bool verbose = GetTestConfiguration()->Verbose() &&
                           (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK);

    UINT32 xStatusMin = _UINT32_MAX;
    UINT32 xStatusMax = 0;
    UINT32 xStatusAvg = 0;

    UINT32 yStatusMin = _UINT32_MAX;
    UINT32 yStatusMax = 0;
    UINT32 yStatusAvg = 0;

    XusbHostCtrl::LaneDeviceInfo laneDeviceInfo;

    for (UINT32 loops = 0; loops < m_NumLoops; loops++)
    {
        UINT32 eomStatus = 0;
        string loopIndexStr = Utility::StrPrintf("(%u)", loops);

        CHECK_RC(m_pXusbHostCtrl->GetEomStatus(XusbHostCtrl::EYEO_X,
                                               timeoutMs,
                                               m_DumpCfgRegisters,
                                               &eomStatus,
                                               &laneDeviceInfo));
        if (eomStatus < xStatusMin)
            xStatusMin = eomStatus;
        if (eomStatus > xStatusMax)
            xStatusMax = eomStatus;
        xStatusAvg += eomStatus;

        if (m_PrintPerLoop)
        {
            CHECK_RC(PrintEomStatus(Tee::PriNormal, 
                                    "X",
                                    loopIndexStr.c_str(),
                                    laneDeviceInfo,
                                    eomStatus));
        }

        CHECK_RC(CheckZeroEye("X", loopIndexStr.c_str(), laneDeviceInfo, eomStatus));

        eomStatus = 0;
        CHECK_RC(m_pXusbHostCtrl->GetEomStatus(XusbHostCtrl::EYEO_Y,
                                               timeoutMs,
                                               m_DumpCfgRegisters,
                                               &eomStatus,
                                               &laneDeviceInfo));
        if (eomStatus < yStatusMin)
            yStatusMin = eomStatus;
        if (eomStatus > yStatusMax)
            yStatusMax = eomStatus;
        yStatusAvg += eomStatus;

        if (m_PrintPerLoop)
        {
            CHECK_RC(PrintEomStatus(Tee::PriNormal,
                                    "Y",
                                    loopIndexStr.c_str(),
                                    laneDeviceInfo,
                                    eomStatus));
        }

        CHECK_RC(CheckZeroEye("Y", loopIndexStr.c_str(), laneDeviceInfo, eomStatus));
    }

    xStatusAvg /= m_NumLoops;
    yStatusAvg /= m_NumLoops;

    CHECK_RC(PrintEomStatus(GetVerbosePrintPri(),
                            "X",
                            "MIN",
                            laneDeviceInfo,
                            xStatusMin));
    CHECK_RC(PrintEomStatus(GetVerbosePrintPri(),
                            "X",
                            "MAX",
                            laneDeviceInfo,
                            xStatusMax));
    CHECK_RC(PrintEomStatus(Tee::PriNormal,
                            "X",
                            (verbose) ? "AVG" : "",
                            laneDeviceInfo,
                            xStatusAvg));
    CHECK_RC(PrintEomStatus(GetVerbosePrintPri(),
                            "Y",
                            "MIN",
                            laneDeviceInfo,
                            yStatusMin));
    CHECK_RC(PrintEomStatus(GetVerbosePrintPri(),
                            "Y",
                            "MAX",
                            laneDeviceInfo,
                            yStatusMax));
    CHECK_RC(PrintEomStatus(Tee::PriNormal,
                            "Y",
                            (verbose) ? "AVG" : "",
                            laneDeviceInfo,
                            yStatusAvg));

    CHECK_RC(CheckAvgVals("X",
                          "AVG",
                          laneDeviceInfo,
                          xStatusAvg,
                          m_XPassThreshold,
                          m_XFailThreshold));
    CHECK_RC(CheckAvgVals("Y",
                          "AVG",
                          laneDeviceInfo,
                          yStatusAvg,
                          m_YPassThreshold,
                          m_YFailThreshold));

    return rc;
}

RC XusbEyeDiagram::Cleanup()
{
    RC rc;

    if (m_pXusbHostCtrl)
    {
        m_pXusbHostCtrl->UnMapAvailBARs();
    }

    return rc;
}

RC XusbEyeDiagram::PrintEomStatus
(
    Tee::Priority pri,
    string mode,
    string index,
    XusbHostCtrl::LaneDeviceInfo laneDeviceInfo,
    UINT32 status
)
{
    RC rc;
    JavaScriptPtr pJs;

    string deviceSpeedShortStr;
    string deviceSpeedLongStr;
    CHECK_RC(m_pXusbHostCtrl->UsbDeviceSpeedEnumToString(laneDeviceInfo.speed,
                                                         &deviceSpeedShortStr,
                                                         &deviceSpeedLongStr));

    string statusStr = Utility::StrPrintf("%s : XUSB_%s(L%u) %s_STATUS %s%s= %02u",
                                          GetBoundTestDevice()->GetName().c_str(),
                                          deviceSpeedShortStr.c_str(),
                                          laneDeviceInfo.laneNum,
                                          mode.c_str(),
                                          index.c_str(),
                                          index.empty() ? "" : " ",
                                          status);

    if (JsonLogStream::IsOpen())
    {
        JsonItem jsi;
        jsi.SetTag("xusb_eye_measurement");
        jsi.SetField("device", GetBoundTestDevice()->GetName().c_str());
        jsi.SetField("USB speed", deviceSpeedLongStr.c_str());
        jsi.SetField("lane", laneDeviceInfo.laneNum);
        jsi.SetField("mode", mode.c_str());
        jsi.SetField("index", index.c_str());
        jsi.SetField("data", status);

        jsi.WriteToLogfile();
    }

    Printf(pri, "%s\n", statusStr.c_str());

    return rc;
}

RC XusbEyeDiagram::CheckZeroEye
(
    string mode,
    string type,
    XusbHostCtrl::LaneDeviceInfo laneDeviceInfo,
    UINT32 val
)
{
    RC rc;

    if (val == 0)
    {
        string deviceSpeedShortStr;
        CHECK_RC(m_pXusbHostCtrl->UsbDeviceSpeedEnumToString(laneDeviceInfo.speed,
                                                             &deviceSpeedShortStr,
                                                             nullptr));

        Printf(Tee::PriError,
               "%s : XUSB_%s(L%u) %s_%s%sSTATUS = 0\n",
               GetBoundTestDevice()->GetName().c_str(),
               deviceSpeedShortStr.c_str(),
               laneDeviceInfo.laneNum,
               mode.c_str(),
               type.c_str(),
               ((type.length()) ? "_" : ""));
        return RC::UNEXPECTED_RESULT;
    }

    return rc;
}

RC XusbEyeDiagram::CheckAvgVals
(
    string mode,
    string type,
    XusbHostCtrl::LaneDeviceInfo laneDeviceInfo,
    UINT32 val,
    UINT32 passThreshold,
    UINT32 failThreshold)
{
    StickyRC rc;

    const bool verbose = GetTestConfiguration()->Verbose() &&
                         (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK);
    if (!verbose)
        type = "";

    if (val < passThreshold)
    {
        string deviceSpeedShortStr;
        CHECK_RC(m_pXusbHostCtrl->UsbDeviceSpeedEnumToString(laneDeviceInfo.speed,
                                                             &deviceSpeedShortStr,
                                                             nullptr));

        if (val <= failThreshold)
        {
            Printf(Tee::PriError,
                   "%s : XUSB_%s(L%u) %s_%s%sSTATUS = %d is %s Fail threshold = %d\n",
                   GetBoundTestDevice()->GetName().c_str(),
                   deviceSpeedShortStr.c_str(),
                   laneDeviceInfo.laneNum,
                   mode.c_str(),
                   type.c_str(),
                   ((type.length()) ? "_" : ""),
                   val,
                   ((val == failThreshold) ? "at" : "below"),
                   failThreshold);
            rc = RC::UNEXPECTED_RESULT;
        }

        if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
        {
            Printf(Tee::PriWarn,
                   "%s : XUSB_%s(L%u) %s_%s%sSTATUS = %d is below Pass threshold = %d\n",
                   GetBoundTestDevice()->GetName().c_str(),
                   deviceSpeedShortStr.c_str(),
                   laneDeviceInfo.laneNum,
                   mode.c_str(),
                   type.c_str(),
                   ((type.length()) ? "_" : ""),
                   val,
                   passThreshold);
        }
    }

    return rc;
}

JS_CLASS_INHERIT(XusbEyeDiagram, GpuTest, "XusbEyeDiagram");
CLASS_PROP_READWRITE_FULL(XusbEyeDiagram, PrintPerLoop, bool,
                          "bool: Print the status for every loop.",
                          MODS_INTERNAL_ONLY | SPROP_VALID_TEST_ARG, 0);
CLASS_PROP_READWRITE(XusbEyeDiagram, XPassThreshold, UINT32,
                     "UINT32: Avg X values greater than or equal to this value "
                     "will be considered passing.");
CLASS_PROP_READWRITE(XusbEyeDiagram, XFailThreshold, UINT32,
                     "UINT32: Avg X values less than or equal to this value "
                     "will be considered failing.");
CLASS_PROP_READWRITE(XusbEyeDiagram, YPassThreshold, UINT32,
                     "UINT32: Avg Y values greater than or equal to this value "
                     "will be considered passing.");
CLASS_PROP_READWRITE(XusbEyeDiagram, YFailThreshold, UINT32,
                     "UINT32: Avg Y values less than or equal to this value "
                     "will be considered failing.");
CLASS_PROP_READWRITE_FULL(XusbEyeDiagram, DumpCfgRegisters, bool,
                          "bool: Dump CFG resgisters after EOM sequence is complete",
                          MODS_INTERNAL_ONLY | SPROP_VALID_TEST_ARG, 0);
