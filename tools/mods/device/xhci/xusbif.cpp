/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "cheetah/include/jstocpp.h"
#include "xusbctrlmgr.h"
#include "xusbctrl.h"
#include "xhciring.h"
#include "xhcitrb.h"

EXPOSE_CONTROLLER_CLASS_WITH_PCI_CFG(XusbController);
EXPOSE_CONTROLLER_MGR(Xusb, XusbController, "Xusb Device Mode");

PROP_CONST(Xusb, CB_CONN, XUSB_JS_CALLBACK_CONN);
PROP_CONST(Xusb, CB_DISCONN, XUSB_JS_CALLBACK_DISCONN);
PROP_CONST(Xusb, CB_U3U0, XUSB_JS_CALLBACK_U3U0);

JS_STEST_LWSTOM(XusbController, SetScsiResp, 2, "Set Response Data to SCSI commands")
{
    JSIF_INIT(Xusb, SetScsiResp, 2, Cmd, [Data08]);
    JSIF_GET_ARG(0, UINT08, cmd);
    JSIF_GET_ARG(1, vector<UINT08>, vData08);
    JSIF_CALL_TEST(XusbController, SetScsiResp, cmd, &vData08);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XusbController, SetDeviceDescriptor, 1, "Set Device Descriptor")
{
    JSIF_INIT(Xusb, SetDeviceDescriptor, 1, [Data08]);
    JSIF_GET_ARG(0, vector<UINT08>, vData08);
    JSIF_CALL_TEST(XusbController, SetDeviceDescriptor, &vData08);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XusbController, SetBosDescriptor, 1, "Set BOS Descriptor")
{
    JSIF_INIT(Xusb, SetBosDescriptor, 1, [Data08]);
    JSIF_GET_ARG(0, vector<UINT08>, vData08);
    JSIF_CALL_TEST(XusbController, SetBosDescriptor, &vData08);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XusbController, SetConfigurationDescriptor, 1, "Set Configuration Descriptor")
{
    JSIF_INIT(Xusb, SetConfigurationDescriptor, 1, [Data08]);
    JSIF_GET_ARG(0, vector<UINT08>, vData08);
    JSIF_CALL_TEST(XusbController, SetConfigurationDescriptor, &vData08);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XusbController, SetStringDescriptor, 1, "Set STRING Descriptor")
{
    JSIF_INIT(Xusb, SetStringDescriptor, 1, [Data08]);
    JSIF_GET_ARG(0, vector<UINT08>, vData08);
    JSIF_CALL_TEST(XusbController, SetStringDescriptor, &vData08);
    JSIF_RETURN(rc);
}

JSIF_TEST_1(XusbController, SetBulkDelay, UINT32, DelayUs, "Set delay to be added to Bulk transfer")
JSIF_TEST_1(XusbController, SetEventRingSize, UINT32, Size, "Set default size of Event Ring")
JSIF_TEST_1(XusbController, SetXferRingSize, UINT32, Size, "Set default size of Xfer Ring")
JSIF_TEST_1(XusbController, SetIsoTdSize, UINT32, Size, "Set default buffer size of Isoch TD")
JSIF_TEST_2(XusbController, WaitXfer, UINT08, Endpoint, bool, IsDataOut, "Wait for Transfer Completion for specified Endpoint")

/*
JS_STEST_LWSTOM(XusbController, InitXferRing, 3, "Initialize Xfer Ring for specific Pipe")
{
    JSIF_INIT_VAR(XusbController, InitXferRing, 2, 3, Endpoint, IsDataOut, *StreamId);
    JSIF_GET_ARG(0, UINT08, Endpoint);
    JSIF_GET_ARG(1, bool, IsDataOut);
    JSIF_GET_ARG_OPT(2, UINT16, StreamId, 0);
    JSIF_CALL_TEST(XusbController, InitXferRing, Endpoint, IsDataOut, StreamId);
    JSIF_RETURN(rc);
}
*/

JSIF_METHOD_0(XusbController, UINT32, GetDeviceState, "Get the current device state");
JS_STEST_LWSTOM(XusbController, DoorBell, 3, "Write the DoorBell Register")
{
    JSIF_INIT_VAR(XusbController, DoorBell, 0, 3, *Endpoint, *IsDataOut, *StreamId);
    JSIF_GET_ARG_OPT(0, UINT08, Endpoint, 0);
    JSIF_GET_ARG_OPT(1, bool, IsDataOut, true);
    JSIF_GET_ARG_OPT(2, UINT16, StreamId, 0);
    JSIF_CALL_TEST(XusbController, RingDoorBell, Endpoint, IsDataOut, StreamId);
    JSIF_RETURN(rc);
}

//JSIF_TEST_0(XusbController, DevCfg, "Configure the device")
JS_STEST_LWSTOM(XusbController, DevCfg, 1, "Configure the device")
{
    JSIF_INIT_VAR(XusbController, DevCfg, 0, 1, *SecondToRun);
    JSIF_GET_ARG_OPT(0, UINT32, SecondToRun, 0);
    JSIF_CALL_TEST(XusbController, DevCfg, SecondToRun);
    JSIF_RETURN(rc);
}

JSIF_TEST_0(XusbController, RingDoorbells, "Ring doorbell for all endpoints with pending TD")
//JSIF_TEST_1(XusbController, SetDebugMode, UINT32, Mode, "Set debug mode")
JS_STEST_LWSTOM(XusbController, SetDebugMode, 2, "Set debug mode")
{
    JSIF_INIT_VAR(XusbController, SetDebugMode, 1, 2, DebugMode, *IsForceValue);
    JSIF_GET_ARG(0, UINT32, DebugMode);
    JSIF_GET_ARG_OPT(1, bool, IsForceValue, false);
    JSIF_CALL_TEST(XusbController, SetDebugMode, DebugMode, IsForceValue);
    JSIF_RETURN(rc);
}

JSIF_TEST_1(XusbController, SetRetry, UINT32, Times,  "Set retry mode / times")
JSIF_TEST_0(XusbController, EngineIsoch, "Launch Isoch test engine")

JS_STEST_LWSTOM(XusbController, PrintRing, 1, "Print content of specific Ring")
{
    MASSERT((pContext!=0)&&(pArguments!= 0)&&(pReturlwalue!= 0));
    RC rc;
    if ( ( NumArguments != 0)
         && (NumArguments != 1 ) )
    {
        JS_ReportError(pContext,"Usage: Xhci.PrintRing(RingId*)");
        return JS_FALSE;
    }

    if ( NumArguments == 0 )
    {
        JSIF_CALL_TEST(XusbController, PrintRingList);
    }
    else
    {
        INT32 ringId;
        if ( OK != JavaScriptPtr()->FromJsval(pArguments[0], &ringId) )
        {
            JS_ReportError(pContext, "Js Arg bad value"); return JS_FALSE;
        }
        if (-1 == ringId) 
        {   // use wants to print all ring's content
            JSIF_CALL_TEST(XusbController, PrintRingList, Tee::PriNormal, true);
        }
        JSIF_CALL_TEST(XusbController, PrintRing, ringId);
    }

    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(XusbController, SetPattern32, 1, "Set data pattern reponding to host's ReadSector() command")
{
    JSIF_INIT(Xusb, SetPattern32, 1, [Data32]);
    JSIF_GET_ARG(0, vector<UINT32>, vData32);
    JSIF_CALL_TEST(XusbController, SetPattern32, &vData32);
    JSIF_RETURN(rc);
}

JSIF_TEST_0(XusbController, PrintContext,  "Print content of Device Context")

JSIF_TEST_1(XusbController, EnBufParams, bool, IsEnable, "Enable/Disable buffer management")
JSIF_TEST_2(XusbController, SetupJsCallback, UINT32, HookIndex, JSFunction*, pJsCallback, "Setup JS callback funtions. (CB_CONN/CB_DISCONN/CB_U3U0...)")
JS_STEST_LWSTOM(XusbController, SetBufParams, 10, "Setup pameters for buffers used by Read/Write")
{
    JSIF_INIT_VAR(XusbController, SetBufParams, 7, 10, SizeMin, SizeMax, NumMin, NumMax, OffMin, OffMax, OffStep, *DataWidth, *Align, *IsShuffle);
    JSIF_GET_ARG(0, UINT32, SizeMin);
    JSIF_GET_ARG(1, UINT32, SizeMax);
    JSIF_GET_ARG(2, UINT32, NumMin);
    JSIF_GET_ARG(3, UINT32, NumMax);
    JSIF_GET_ARG(4, UINT32, OffMin);
    JSIF_GET_ARG(5, UINT32, OffMax);
    JSIF_GET_ARG(6, UINT32, OffStep);
    JSIF_GET_ARG_OPT(7, UINT32, DataWidth, 4);
    JSIF_GET_ARG_OPT(8, UINT32, Align, 4096);
    JSIF_GET_ARG_OPT(9, bool, IsShuffle, true);
    JSIF_CALL_TEST(XusbController, SetBufParams, SizeMin, SizeMax, NumMin, NumMax, OffMin, OffMax, OffStep, DataWidth, Align, IsShuffle);
    JSIF_RETURN(rc);
}

JSIF_TEST_1(XusbController, SetTimeout, UINT32, TimeoutMs, "Set default timeout(ms)")
