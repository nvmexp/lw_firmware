/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2002-2021 by LWPU Corporation.  All rights
 * reserved.  All information contained herein is proprietary and confidential
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// I2C interface tests.

#include "core/include/jscript.h"
#include "core/include/massert.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "ctrl/ctrl2080.h"
#include "device/interface/i2c.h"
#include "gpu/js_gpusb.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/gputest.h"

#include "class/cl0073.h"   // LW04_DISPLAY_COMMON
#include "ctrl/ctrl0073.h"  // for LW0073_CTRL_CMD_SYSTEM_GET_CONNECT_STATE
// I2CTest Class
class I2CTest : public GpuTest
{
    RC GetVirtualDispPort(map<UINT32, bool> *pVirtPorts, GpuSubdevice *pSubdev);
public:
    // Constructor
    I2CTest();

    // Test pull-up resistors on a specified port.
    RC PullupTest(unsigned long Port, GpuSubdevice *subdev);

    // Virtual methods from ModsTest
    RC Run();
    bool IsSupported();
};

//------------------------------------------------------------------------------
// Method implementation
//------------------------------------------------------------------------------

bool I2CTest::IsSupported()
{
    if (GetBoundGpuSubdevice()->IsSOC())
    {
        Printf(Tee::PriLow, "Skipping I2CTest on SOC GPU\n");
        return false;
    }

    if (!GetBoundGpuSubdevice()->SupportsInterface<I2c>())
    {
        return false;
    }

    if (!GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_HAS_I2C))
    {
        Printf(Tee::PriNormal, "Skipping I2CTest because it is not supported"
                               " pre LW35\n");
        return false;
    }

    if (GetBoundGpuSubdevice()->IsEmOrSim())
    {
        JavaScriptPtr pJs;
        bool sanityMode = false;
        pJs->GetProperty(pJs->GetGlobalObject(), "g_SanityMode", &sanityMode);
        if (!sanityMode)
        {
            Printf(Tee::PriNormal, "Skipping I2CTest because it is not supported"
                                   " on emulation/simulation\n");
            return false;
        }
    }

    return true;
}

I2CTest::I2CTest()
{
    SetName("I2CTest");
}

static void FreeDispHandle(LwRm::Handle Handle)
{
    if (Handle)
        LwRmPtr()->Free(Handle);
}

RC I2CTest::GetVirtualDispPort(map<UINT32, bool> *pVirtPorts, GpuSubdevice *pSubdev)
{
    RC rc;
    LwRm::Handle cls073Handle;
    rc = LwRmPtr()->Alloc(
                 LwRmPtr()->GetDeviceHandle(GetBoundGpuDevice()),
                 &cls073Handle,
                 LW04_DISPLAY_COMMON,
                 NULL);

    if (rc != OK)
    {
        Printf(Tee::PriLow, "Error allocating display class handle, "
                "test might fail if port with virtual entries are present\n");
        return OK;
    }

    Utility::CleanupOnReturnArg<void, void, LwRm::Handle>
        freeCls073Handle(FreeDispHandle, cls073Handle);

    MASSERT(pVirtPorts);
    LW0073_CTRL_CMD_SYSTEM_GET_DISPLAY_ID_PROPS_PARAMS virtParams = {0};

    virtParams.subDeviceInstance = pSubdev->GetSubdeviceInst();
    CHECK_RC(LwRmPtr()->Control(
                cls073Handle,
                LW0073_CTRL_CMD_SYSTEM_GET_DISPLAY_ID_PROPS,
                &virtParams,
                sizeof(virtParams)));

    if (virtParams.numDisplays >
            LW0073_CTRL_SYSTEM_DISPLAY_ID_PROPS_MAX_ENTRIES)
    {
        Printf(Tee::PriError, "Number of valid entries in display device list"
                "exceeds maximum number of display entries\n");
        return RC::SOFTWARE_ERROR;
    }

    for (UINT32 i = 0; i < virtParams.numDisplays; i++)
    {
        LW0073_CTRL_SYSTEM_DISPLAY_ID_PROPS rmVirtualInfo =
            virtParams.displayDeviceProps[i];
        if (FLD_TEST_DRF(0073_CTRL, _DISPLAY_ID_PROP, _IS_VIRTUAL,
                    _TRUE, rmVirtualInfo.props))
        {
            LW0073_CTRL_SPECIFIC_GET_I2C_PORTID_PARAMS I2CPort = {0};
            I2CPort.subDeviceInstance = pSubdev->GetSubdeviceInst();
            I2CPort.displayId = rmVirtualInfo.displayId;
            CHECK_RC(LwRmPtr()->Control(
                        cls073Handle,
                        LW0073_CTRL_CMD_SPECIFIC_GET_I2C_PORTID,
                        &I2CPort,
                        sizeof(I2CPort)));
            // API from RM returns one-based I2CPort
            (*pVirtPorts)[I2CPort.ddcPortId - 1] = true;
        }
    }

    return rc;
}

RC I2CTest::Run()
{
    RC rc = OK;

    for (UINT32 subdev = 0;
         subdev < GetBoundGpuDevice()->GetNumSubdevices();
         subdev ++)
    {
        GpuSubdevice * pSubdev = GetBoundGpuDevice()->GetSubdevice(subdev);
        // Iterate through all I2C ports.
        I2c::PortInfo PortInfo;
        CHECK_RC(pSubdev->GetInterface<I2c>()->GetPortInfo(&PortInfo));

        map<UINT32, bool> virtDispPorts;
        //Initialize virtual display port map
        for (UINT32 Port = 0; Port < PortInfo.size(); ++Port)
        {
            if (PortInfo[Port].Implemented)
                virtDispPorts[Port] = false;
        }
        CHECK_RC(GetVirtualDispPort(&virtDispPorts, pSubdev));

        for (UINT32 Port = 0; Port < PortInfo.size(); ++Port)
        {
            Printf(Tee::PriDebug,
                   "GpuSubdev %d: Port %d is %simplemented, %sDCB declared, "
                   "%sDDC channel, %sCRTC mapped.\n", subdev, Port,
                   (PortInfo[Port].Implemented) ? "    " : "not ",
                   (PortInfo[Port].DcbDeclared) ? "    " : "not ",
                   (PortInfo[Port].DdcChannel)  ? "    " : "not ",
                   (PortInfo[Port].CrtcMapped)  ? "    " : "not ");

            // Only test ports that are implemented and
            // declared in the DCB or used as a DDC channel.
            if (PortInfo[Port].Implemented &&
                (PortInfo[Port].DcbDeclared || PortInfo[Port].DdcChannel))
            {
                if (pSubdev->HasBug(997557) && Port > 3)
                {
                    Printf(Tee::PriLow,
                           "Skipping I2C Port %d due to bug 997557\n", Port);
                    continue;
                }

                if (virtDispPorts[Port])
                {
                    Printf(Tee::PriLow, "Skipping I2C port %d due to "
                            "virtual display entry\n", Port);
                    continue;
                }

                Printf(Tee::PriLow,
                       "Testing I2C Port %d on Gpu SubDev %d.\n",
                       Port, subdev);
                CHECK_RC(PullupTest(Port, pSubdev));
            }
            else
            {
                Printf(Tee::PriLow,
                       "Skipping I2C Port %d since it is neither "
                       "declared in the DCB nor a DDC channel.\n", Port);
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
// Test pull-up resistors on a specified port.
//------------------------------------------------------------------------------
RC I2CTest::PullupTest(unsigned long Port, GpuSubdevice *subdev)
{
    MASSERT(subdev != 0);

    I2c::Dev i2cDev = subdev->GetInterface<I2c>()->I2cDev(Port, 0);
    RC rc;

    rc = i2cDev.TestPort();

    if (OK != rc)
    {
        Printf(Tee::PriHigh,
               "I2CTest: SCL and/or SDA pullup is broken on GpuSubdevice %d"
               " I2C port %ld\n", subdev->GetSubdeviceInst(), Port);
        // We may want to create a new errors75.in file and a new
        // RC:I2CTEST_PULLUP_MISSING define if overloading the SCL
        // return is incorrect.
        rc = RC::I2CTEST_SCL_PULLUP_MISSING;
    }

    return rc;
}

//------------------------------------------------------------------------------
// I2CTest object, properties and methods.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(I2CTest, GpuTest, "I2CTest Test");

JS_STEST_LWSTOM(I2CTest,
                PullupTest,
                2,
                "Test the pull-up resistors on the specified I2C ports.")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    const char usage[] = "Usage: I2CTest.PullupTest(Port, GpuSubdevice)";

    if (NumArguments != 2)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    JSObject     *jsGpuSubdevice;

    // Get the JsGpuSubdevice JSObject from JS
    if (OK != pJavaScript->FromJsval(pArguments[0], &jsGpuSubdevice))
    {
       JS_ReportError(pContext, usage);
       return JS_FALSE;
    }

    JsGpuSubdevice *pJsGpuSubdevice;
    if ((pJsGpuSubdevice = JS_GET_PRIVATE(JsGpuSubdevice, pContext, jsGpuSubdevice, "GpuSubdevice")) == 0)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    GpuSubdevice *subdev = pJsGpuSubdevice->GetGpuSubdevice();

    UINT32 Port;

    if (OK != pJavaScript->FromJsval(pArguments[0], &Port))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    I2CTest *pTest;
    if ((pTest = JS_GET_PRIVATE(I2CTest, pContext, pObject, "I2CTest")) != 0)
    {
        RETURN_RC(pTest->PullupTest(Port, subdev));
    }
    return JS_FALSE;
}
