/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2012,2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "azactrl.h"
#include "azactrlmgr.h"
#include "cheetah/include/devid.h"
#include "cheetah/include/jstocpp.h"
#include "core/include/platform.h"
//#include "azautil.h"
#include "core/include/utility.h"
#include "core/include/version.h"

EXPOSE_CONTROLLER_CLASS_WITH_PCI_CFG(AzaliaController);
EXPOSE_CONTROLLER_MGR(Azalia, AzaliaController, "Azalia(HDA) Controller");

JSIF_METHOD_0(AzaliaController, UINT32, GetWallClockCount, "Get the wall clock count");
JSIF_METHOD_0(AzaliaController, UINT32, GetWallClockCountAlias, "Get the wall clock count alias");
JSIF_METHOD_0(AzaliaController, UINT32, GetMaxNumInputStreams, "Get the maximum number of input streams supported");
JSIF_METHOD_0(AzaliaController, UINT32, GetMaxNumOutputStreams, "Get the maximum number of output streams supported");
JSIF_METHOD_0(AzaliaController, UINT32, GetMaxNumStreams, "Get the maximum number of streams supported");
JSIF_METHOD_0(AzaliaController, UINT08, GetNumCodecs, "Get the number of codecs connected to the device");
JSIF_METHOD_0(AzaliaController, UINT32, ReadIResponse, "Read response and status from IRII and Ics");
JSIF_METHOD_0(AzaliaController, UINT32, GetAspmState, "Get the ASPM state");

JSIF_TEST_0(AzaliaController, ReleaseAllStreams, "Release all the reserved streams");
JSIF_TEST_0(AzaliaController, MapPins, "Show which pins have cable connected");
JSIF_TEST_0(AzaliaController, IsIch6, "Query whether controller is Intel ICH6 version");
JSIF_TEST_1(AzaliaController, DisableStream, UINT32, StreamIndex, "Disable the stream specified");
JSIF_TEST_1(AzaliaController, EnableStream, UINT32, StreamIndex, "Enable the stream specified");
JSIF_TEST_1(AzaliaController, SetAspmState, UINT32, Aspm, "Set the ASPM state");
JSIF_TEST_1(AzaliaController, ToggleControllerInterrupt, bool, IsEnable, "Enable interrupts for Azalia");
JSIF_TEST_1(AzaliaController, SetIsAllocFromPcieNode, bool, IsPcie, "Set whether to allocate memory from pcie node");
JSIF_TEST_1(AzaliaController, SetIsUseMappedPhysAddr, bool, IsMapped, "Set whether to use mapped physical address");

JS_STEST_LWSTOM(AzaliaController, ResetState, 1, "Reset Azalia Controller State")
{
    JSIF_INIT(AzaliaController, ResetState, 1, "Reset State");
    JSIF_GET_ARG(0, bool, PutIntoReset);
    JSIF_CALL_TEST(AzaliaController, Reset, PutIntoReset);
    JSIF_RETURN(rc);
}

JS_SMETHOD_LWSTOM(AzaliaController, IsFormatSupported, 4, "Query if attached codecs support a stream format")
{
    JSIF_INIT(AzaliaController, IsFormatSupported, 4, Direction, Rate, Size(BPS), Sdin);
    JSIF_GET_ARG(0, UINT32, Direction);
    JSIF_GET_ARG(1, UINT32, Rate);
    JSIF_GET_ARG(2, UINT32, Size);
    JSIF_GET_ARG(3, UINT32, Sdin);
    bool supported = ((AzaliaController*) JS_GetPrivate(pContext, pObject))->IsFormatSupported(Direction, Rate, Size, Sdin);
    JSIF_RETURN(supported);
}

JS_STEST_LWSTOM(AzaliaController, SetPowerState, 1, "Set the power state to the desired level (0=D0, 3=D3HOT")
{
    JSIF_INIT(AzaliaController, SetPowerState, 1, "TargetPowerState");
    JSIF_GET_ARG(0, UINT32, TargetPowerState);
    JSIF_CALL_TEST(AzaliaController, SetPowerState, (AzaliaController::PowerState)TargetPowerState);
    JSIF_RETURN(rc);
}

JS_SMETHOD_LWSTOM(AzaliaController, GetNextAvailableStream, 1, "Get the next free stream index in the given direction")
{
    JSIF_INIT(AzaliaController, GetNextAvailableStream, 1, "Direction");
    JSIF_GET_ARG(0, INT32, Direction);
    JSIF_CALL_METHOD(AzaliaController, UINT32, GetNextAvailableStream, (AzaliaDmaEngine::DIR)Direction);
    JSIF_RETURN(result);
}

JS_STEST_LWSTOM(AzaliaController, DumpCorb, 2, "Dump the contents of the CORB")
{
    JSIF_INIT_VAR(AzaliaController, DumpCorb, 0, 2, "[first], [last]");
    JSIF_GET_ARG_OPT(0, UINT32, FirstCorbEntry, 0);
    JSIF_GET_ARG_OPT(1, UINT32, LastCorbEntry, 255);
    JSIF_CALL_TEST(AzaliaController, DumpCorb, Tee::PriNormal, FirstCorbEntry, LastCorbEntry);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(AzaliaController, DumpRirb, 2, "Dump the contents of the RIRB")
{
    JSIF_INIT_VAR(AzaliaController, DumpRirb, 0, 2, "[first, last]");
    JSIF_GET_ARG_OPT(0, UINT32, FirstRirbEntry, 0);
    JSIF_GET_ARG_OPT(1, UINT32, LastRirbEntry, 255);
    JSIF_CALL_TEST(AzaliaController, DumpRirb, Tee::PriNormal, FirstRirbEntry, LastRirbEntry);
    JSIF_RETURN(rc);
}

JS_STEST_LWSTOM(AzaliaController, CodecLoad, 2, "Load verb table from file into codec")
{
    JSIF_INIT_VAR(AzaliaController, CodecLoad, 1, 2, "codec filename, [IsPrintOnly]");
    JSIF_GET_ARG(0, string, CodecDescFile);
    JSIF_GET_ARG_OPT(1, bool, IsOnlyPrint, false);
    JSIF_CALL_TEST(AzaliaController, CodecLoad, CodecDescFile, IsOnlyPrint);
    JSIF_RETURN(rc);
}

JS_SMETHOD_LWSTOM(AzaliaController, RirbPop, 0, "Pop a single response from the RIRB")
{
    JSIF_INIT(AzaliaController, RirbPop, 0);
    UINT64 response = 0;
    JSIF_CALL_METHOD(AzaliaController, UINT16, RirbPop, &response, 1);
    if (result == 0)
        response = 0xdeadbeef;

    JSIF_RETURN(response);
}

JS_STEST_LWSTOM(AzaliaController, SendICommand, 4, "Send a command to a codec via immediate command/response registers")
{
    JSIF_INIT_VAR(AzaliaController, SendICommand, 1, 4, "(command32) OR (CodecNum, NodeId ,Verb, Payload");
    JSIF_GET_ARG(0, UINT32, CommandOrCodecNum);
    JSIF_GET_ARG_OPT(1, UINT32, NodeId, 0);
    JSIF_GET_ARG_OPT(2, UINT32, Verb, 0);
    JSIF_GET_ARG_OPT(3, UINT32, PayLoad, 0);
    if (NumArguments == 1)
    {
        JSIF_CALL_METHOD(AzaliaController, UINT32, SendICommand, CommandOrCodecNum);
        JSIF_RETURN(result);
    }
    else if (NumArguments == 4)
    {
        JSIF_CALL_METHOD(AzaliaController, UINT32, SendICommand, CommandOrCodecNum,
                         NodeId, Verb, PayLoad);
        JSIF_RETURN(result);
    }
    else
    {
        Printf(Tee::PriHigh, "ERROR: in AzaliaController.SendICommand() - Invalid paramaters");
        return JS_FALSE;
    }
}

/*
MCPDEV_TEST_ONEARG(Azalia, Reset, reset, bool, "Set the reset state of Azalia");
MCP_JSCLASS_METHOD(Azalia, SendCommand, 4, "Send a command to a codec")
C_(Azalia_SendCommand)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 4)
    {
        JS_ReportError(pContext, "Usage: Azalia.SendCommand(codec,node,verb,payload)");
        return JS_FALSE;
    }

    RC rc;
    JavaScriptPtr pJavaScript;
    AzaliaDevice* pAzalia;
    AzaliaCodec* pCodec;
    UINT32 codec, node, verb, payload, response;
    if (OK != pJavaScript->FromJsval(pArguments[0], &codec))
    {
        JS_ReportError(pContext, "Bad value for codec");
        return JS_FALSE;
    }
    if (OK != pJavaScript->FromJsval(pArguments[1], &node))
    {
        JS_ReportError(pContext, "Bad value for node");
        return JS_FALSE;
    }
    if (OK != pJavaScript->FromJsval(pArguments[2], &verb))
    {
        JS_ReportError(pContext, "Bad value for verb");
        return JS_FALSE;
    }
    if (OK != pJavaScript->FromJsval(pArguments[3], &payload))
    {
        JS_ReportError(pContext, "Bad value for payload");
        return JS_FALSE;
    }
    C_CHECK_RC(AzaliaMgr::Mgr()->GetLwrrent((McpDev**) &pAzalia));
    MASSERT(pAzalia);
    C_CHECK_RC(pAzalia->GetCodec(codec, &pCodec));
    if (!pCodec)
    {
        JS_ReportError(pContext, "Could not find codec!");
        return JS_FALSE;
    }
    CHECK_RC(pCodec->SendCommand(node, verb, payload, &response));
    if (OK != pJavaScript->ToJsval(response, pReturlwalue))
    {
        JS_ReportError(pContext, "Error colwerting return value to JS");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

*/

P_(Azalia_Get_AllowIntel);
P_(Azalia_Set_AllowIntel);
static SProperty AllowIntel
(
   Azalia_Object,
   "Flag167Aed01",
   0,
   0,
   Azalia_Get_AllowIntel,
   Azalia_Set_AllowIntel,
   0,
   "."
);
P_(Azalia_Get_AllowIntel)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JavaScriptPtr pJavaScript;

    bool Allow = AzaliaControllerMgr::s_IsIntelAzaliaAllowed;
    if (pJavaScript->ToJsval(Allow, pValue) == 0)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

P_(Azalia_Set_AllowIntel)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    //Allow the override only in internal engineering builds
    if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
    {
        JavaScriptPtr pJavaScript;

        UINT32 Key;
        if (OK != pJavaScript->FromJsval(*pValue, &Key))
        {
            return JS_FALSE;
        }
        bool Allow = false;
        if (Key == 0x20080409)
        {
            Allow = true;
        }

        AzaliaControllerMgr::s_IsIntelAzaliaAllowed = Allow;
    }

    return JS_TRUE;
}

P_(Azalia_Set_TimeScaler);
P_(Azalia_Get_TimeScaler);
static SProperty TimeScaler
(
   Azalia_Object,
   "TimeScaler",
   0,
   0,
   Azalia_Get_TimeScaler,
   Azalia_Set_TimeScaler,
   0,
   "(Simulation only) Amount to scale the wait times by"
);

P_(Azalia_Set_TimeScaler)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JavaScriptPtr pJavaScript;

    UINT32 timeScaler;
    if (OK != pJavaScript->FromJsval(*pValue, &timeScaler))
    {
        return JS_FALSE;
    }
    AzaliaUtilities::SetTimeScaler(timeScaler);
    return JS_TRUE;
}

P_(Azalia_Get_TimeScaler)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JavaScriptPtr pJavaScript;

    UINT32 timeScaler = AzaliaUtilities::GetTimeScaler();
    if (pJavaScript->ToJsval(timeScaler, pValue) == 0)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

P_(Azalia_Set_MaxSingleWaitTime);
P_(Azalia_Get_MaxSingleWaitTime);
static SProperty MaxSingleWaitTime
(
   Azalia_Object,
   "MaxSingleWaitTime",
   0,
   0,
   Azalia_Get_MaxSingleWaitTime,
   Azalia_Set_MaxSingleWaitTime,
   0,
   "(Simulation Only) Maximum amount of time to wait before printing out a message "
   "to prevent failures in simulation"
);

P_(Azalia_Set_MaxSingleWaitTime)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JavaScriptPtr pJavaScript;

    UINT32 maxSingleWaitTime;
    if (OK != pJavaScript->FromJsval(*pValue, &maxSingleWaitTime))
    {
        return JS_FALSE;
    }
    AzaliaUtilities::SetMaxSingleWaitTime(maxSingleWaitTime);
    return JS_TRUE;
}

P_(Azalia_Get_MaxSingleWaitTime)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JavaScriptPtr pJavaScript;

    UINT32 maxSingleWaitTime = AzaliaUtilities::GetMaxSingleWaitTime();
    if (pJavaScript->ToJsval(maxSingleWaitTime, pValue) == 0)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

C_(Azalia_GetTimeNS);
static SMethod Azalia_GetTimeNS
    (
    Azalia_Object,
    "GetTimeNS",
    C_Azalia_GetTimeNS,
    0,
    "(Util)Get Time in nanoseconds."
    );

C_(Azalia_GetTimeNS)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: Azalia.GetTimeNS()");
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;

    UINT64 Time = Platform::GetTimeNS();

    if (OK != pJavaScript->ToJsval(Time, pReturlwalue))
    {
        JS_ReportError(pContext, "Error colwerting return value to JS");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(Azalia_Pause);
static STest Azalia_Pause
    (
    Azalia_Object,
    "Pause",
    C_Azalia_Pause,
    1,
    "(Util)Pause."
    );

C_(Azalia_Pause)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if (NumArguments != 1)
    {
        JS_ReportError(pContext, "Usage: AzaliaUtilities.Pause(time)");
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    UINT32 time;

    if (OK != pJavaScript->FromJsval(pArguments[0], &time))
    {
        JS_ReportError(pContext, "Bad value for time");
        return JS_FALSE;
    }

    AzaliaUtilities::Pause(time);

    return JS_TRUE;
}
