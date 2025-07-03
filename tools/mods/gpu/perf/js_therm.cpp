/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/perf/js_therm.h"
#include "core/include/deprecat.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "jsapi.h"
#include "core/include/xp.h"
#include "core/include/rc.h"
#include "gpu/perf/thermsub.h"
#include "core/include/utility.h"

#define JSTHERMAL_ILWALID_VALUE                 0x3fffffff

JsThermal::JsThermal()
    : m_pThermal(NULL), m_pJsThermalObj(NULL)
{
}

JS_CLASS(FPT);

//-----------------------------------------------------------------------------
static void C_JsThermal_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsThermal * pJsThermal;
    //! Delete the C++
    pJsThermal = (JsThermal *)JS_GetPrivate(cx, obj);
    delete pJsThermal;
};

//-----------------------------------------------------------------------------
static JSClass JsThermal_class =
{
    "JsThermal",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsThermal_finalize
};

//-----------------------------------------------------------------------------
static SObject JsThermal_Object
(
    "JsThermal",
    JsThermal_class,
    0,
    0,
    "Thermal JS Object"
);

//------------------------------------------------------------------------------
//! \brief Create a JS Object representation of the current associated
//! thermal object
RC JsThermal::CreateJSObject(JSContext *cx, JSObject *obj)
{
    //! Only create one JSObject per Thermal object
    if (m_pJsThermalObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this JsThermal.\n");
        return OK;
    }

    m_pJsThermalObj = JS_DefineObject(cx,
                                      obj, // GpuSubdevice object
                                      "Thermal", // Property name
                                      &JsThermal_class,
                                      JsThermal_Object.GetJSObject(),
                                      JSPROP_READONLY);

    if (!m_pJsThermalObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    //! Store the current JsThermal instance into the private area
    //! of the new JSOBject.
    if (JS_SetPrivate(cx, m_pJsThermalObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of JsThermal.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsThermalObj, "Help", &C_Global_Help, 1, 0);

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Return the private JS data - C++ Thermal object.
//!
Thermal * JsThermal::GetThermal()
{
    MASSERT(m_pThermal);
    return m_pThermal;
}

//------------------------------------------------------------------------------
//! \brief Set the associated thermal object.
//!
//! This is called by JS GpuSubdevice Initialize
void JsThermal::SetThermal(Thermal * pThermal)
{
    m_pThermal = pThermal;
}

#define JSTHERMAL_GETP(rettype, funcname)                                                    \
P_( JsThermal_Get##funcname )                                                                \
{                                                                                            \
    MASSERT(pContext != 0);                                                                  \
    MASSERT(pValue   != 0);                                                                  \
    JavaScriptPtr pJavaScript;                                                               \
    JsThermal *pJsThermal;                                                                   \
    if ((pJsThermal = JS_GET_PRIVATE(JsThermal, pContext, pObject, "JsThermal")) != 0)       \
    {                                                                                        \
        RC rc;                                                                               \
        Thermal* pThermal = pJsThermal->GetThermal();                                        \
        rettype RetVal;                                                                      \
        char msg[256];                                                                       \
        if ((rc = pThermal->Get##funcname(&RetVal))!= OK)                                    \
        {                                                                                    \
            sprintf(msg, "Error %d reading calling Get"#funcname"", (UINT32)rc);             \
            JS_ReportWarning(pContext, msg);                                                 \
            JS_SetPendingException(pContext, INT_TO_JSVAL(rc));                              \
            return JS_FALSE;                                                                 \
        }                                                                                    \
        if ((rc = pJavaScript->ToJsval(RetVal, pValue)) != OK)                               \
        {                                                                                    \
            JS_ReportError(pContext, "Failed to colwert Get" #funcname"");                   \
            JS_SetPendingException(pContext, INT_TO_JSVAL(rc));                              \
            *pValue = JSVAL_NULL;                                                            \
            return JS_FALSE;                                                                 \
        }                                                                                    \
        return JS_TRUE;                                                                      \
    }                                                                                        \
    return JS_FALSE;                                                                         \
}

#define JSTHERMAL_SETP(rettype, funcname)                                                    \
P_( JsThermal_Set##funcname )                                                                \
{                                                                                            \
    MASSERT(pContext != 0);                                                                  \
    MASSERT(pValue   != 0);                                                                  \
    JavaScriptPtr pJavaScript;                                                               \
    JsThermal *pJsTherm;                                                                     \
    if ((pJsTherm = JS_GET_PRIVATE(JsThermal, pContext, pObject, "JsThermal")) != 0)         \
    {                                                                                        \
        RC rc;                                                                               \
        Thermal* pThermal = pJsTherm->GetThermal();                                          \
        rettype RetVal;                                                                      \
        if (OK != pJavaScript->FromJsval(*pValue, &RetVal))                                  \
        {                                                                                    \
            JS_ReportError(pContext, "Failed to colwert Set"#funcname"");                    \
            return JS_FALSE;                                                                 \
        }                                                                                    \
        char msg[256];                                                                       \
        if ((rc = pThermal->Set##funcname(RetVal))!= OK)                                     \
        {                                                                                    \
            sprintf(msg, "Error %d reading calling Set"#funcname"", (UINT32)rc);             \
            JS_ReportError(pContext, msg);                                                   \
            *pValue = JSVAL_NULL;                                                            \
            return JS_FALSE;                                                                 \
        }                                                                                    \
        return JS_TRUE;                                                                      \
    }                                                                                        \
    return JS_FALSE;                                                                         \
}

#define JSTHERMAL_GETPROP(rettype, funcname,helpmsg)                                        \
P_( JsThermal_Get##funcname );                                                              \
static SProperty Get##funcname                                                              \
(                                                                                           \
   JsThermal_Object,                                                                        \
   #funcname,                                                                               \
   0,                                                                                       \
   0,                                                                                       \
   JsThermal_Get##funcname,                                                                 \
   0,                                                                                       \
   JSPROP_READONLY,                                                                         \
   helpmsg                                                                                  \
);                                                                                          \
JSTHERMAL_GETP(rettype, funcname)

JSTHERMAL_GETPROP(FLOAT32, AmbientTemp, "temp on the board measured by external sensor");
JSTHERMAL_GETPROP(FLOAT32, ChipTempViaExt, "temp on the chip measured by external sensor");
JSTHERMAL_GETPROP(FLOAT32, ChipTempViaInt, "temp on the chip measured by internal sensor");
JSTHERMAL_GETPROP(INT32, ChipTempViaSmbus, "temp on the chip measured by Smbus external sensor");
JSTHERMAL_GETPROP(INT32, ChipTempViaIpmi, "temp on the chip measured by Ipmi external sensor");
JSTHERMAL_GETPROP(UINT32, PrimarySensorType, "get primary sensor type");
JSTHERMAL_GETPROP(FLOAT32, ChipTempViaPrimary, "temp on the chip measured by primary sensor");
JSTHERMAL_GETPROP(FLOAT32, ChipHighLimit, "get high limit on die");
JSTHERMAL_GETPROP(bool, HasChipHighLimit, "check if high limit on die is supported");
JSTHERMAL_GETPROP(FLOAT32, ChipShutdownLimit, "get shutdown limit on die");
JSTHERMAL_GETPROP(UINT32, OverTempCounter, "get number of times over temp even oclwred");
JSTHERMAL_GETPROP(bool, IsSupported, "check if the current subdev supports thermal");
JSTHERMAL_GETPROP(bool, ShouldIntBePresent, "should the internal sensor be present");
JSTHERMAL_GETPROP(bool, ShouldExtBePresent, "should the external sensor be present");
JSTHERMAL_GETPROP(UINT32, CoolerPolicy, "RM's current cooler policy");
JSTHERMAL_GETPROP(UINT32, DefaultMinimumFanSpeed, "default minimum fan speed as percent (0 to 100)");
JSTHERMAL_GETPROP(UINT32, DefaultMaximumFanSpeed, "default maximum fan spped as percent (0 to 100)");
JSTHERMAL_GETPROP(UINT32, MinimumFanSpeed, "current minimum fan speed as percent (0 to 100)");
JSTHERMAL_GETPROP(UINT32, MaximumFanSpeed, "current maximum fan speed as percent (0 to 100)");
JSTHERMAL_GETPROP(UINT32, FanSpeed, "current fan speed as percent (0 to 100)");
JSTHERMAL_GETPROP(INT32, VbiosExtTempOffset, "VBIOS Hotspot offset for external sensor");
JSTHERMAL_GETPROP(INT32, VbiosIntTempOffset, "VBIOS Hotspot offset for external sensor");
JSTHERMAL_GETPROP(bool, IsFanSpinning, "is the fan spinning lwrrently");
JSTHERMAL_GETPROP(bool, IsFanActivitySenseSupported, "is the fan activity sensing supported");
JSTHERMAL_GETPROP(bool, IsFanRPMSenseSupported, "is the fan's RPM sensing supported");
JSTHERMAL_GETPROP(UINT32, FanControlSignal, "get the type of fan control signal (none, toggle, variable");
JSTHERMAL_GETPROP(UINT32, FanTachRPM, "get fan rpm");
JSTHERMAL_GETPROP(bool, FanTachExpectedRPMSupported, "get whether this GPU supports callwlating the expected RPM");
JSTHERMAL_GETPROP(UINT32, FanTachMinRPM, "get expected tachometer min fan rpm");
JSTHERMAL_GETPROP(UINT32, FanTachMinPct, "get min fan pct corresponding  to the tachometer min fan rpm");
JSTHERMAL_GETPROP(UINT32, FanTachMaxRPM, "get expected tachometer max fan rpm");
JSTHERMAL_GETPROP(UINT32, FanTachMaxPct, "get max fan pct corresponding  to the tachometer max fan rpm");
JSTHERMAL_GETPROP(UINT32, FanTachInterpolationError, "get expected error for the expected RPM when interpolating between the endpoints.");
JSTHERMAL_GETPROP(UINT32, CoolerCount, "get the number of supported coolers.");
JSTHERMAL_GETPROP(bool, IsVolterraPresent, "is a volterra temperature sensor present");
JSTHERMAL_GETPROP(bool, IsTempControllerPresent, "is a temperature controller present");
JSTHERMAL_GETPROP(bool, IsAcousticTempControllerPresent, "is a acoustic temperature controller present");

#define JSTHERMAL_GETSETPROP(rettype, funcname,helpmsg)                                      \
P_( JsThermal_Get##funcname );                                                               \
P_( JsThermal_Set##funcname );                                                               \
static SProperty Get##funcname                                                               \
(                                                                                            \
    JsThermal_Object,                                                                        \
    #funcname,                                                                               \
    0,                                                                                       \
    0,                                                                                       \
    JsThermal_Get##funcname,                                                                 \
    JsThermal_Set##funcname,                                                                 \
    0,                                                                                       \
    helpmsg                                                                                  \
);                                                                                           \
JSTHERMAL_GETP(rettype, funcname)                                                            \
JSTHERMAL_SETP(rettype, funcname)

JSTHERMAL_GETSETPROP(INT32, ExtTempOffset,
                     "External temperature offset");
JSTHERMAL_GETSETPROP(INT32, IntTempOffset,
                     "Internal temperature offset");
JSTHERMAL_GETSETPROP(INT32, SmbusTempOffset,
                     "Smbus temperature offset");
JSTHERMAL_GETSETPROP(INT32, SmbusTempSensor,
                     "Smbus temperature sensor");
JSTHERMAL_GETSETPROP(string, SmbusTempDevice,
                     "Smbus temperature device name");
JSTHERMAL_GETSETPROP(INT32, IpmiTempOffset,
                     "Ipmi temperature offset");
JSTHERMAL_GETSETPROP(INT32, IpmiTempSensor,
                     "Ipmi temperature sensor");
JSTHERMAL_GETSETPROP(string, IpmiTempDevice,
                     "Smbus temperature device name");
JSTHERMAL_GETSETPROP(bool, ClockThermalSlowdownAllowed,
                     "If true, resource manager is allowed to thermal throttle clocks");
JSTHERMAL_GETSETPROP(UINT32, VolterraMaxTempDelta,
                     "Set the maximum difference between two conselwtive readings on "
                     "the same volterra slave in degrees C (default = 15)");
JSTHERMAL_GETSETPROP(UINT32, HwfsEventIgnoreMask,
                     "Hardware Failsafe events ignore mask");

#define JSTHERMAL_SETFUNC(paramtype, funcname, helpmsg)                        \
JS_STEST_LWSTOM(JsThermal,                                                     \
                funcname,                                                      \
                1,                                                             \
                helpmsg)                                                       \
{                                                                              \
    MASSERT(pContext     !=0);                                                 \
    MASSERT(pArguments   !=0);                                                 \
                                                                               \
    JavaScriptPtr pJavaScript;                                                 \
    paramtype Input;                                                           \
    if ((NumArguments != 1) ||                                                 \
        (OK != pJavaScript->FromJsval(pArguments[0], &Input)))                 \
    {                                                                          \
        JS_ReportError(pContext, "Usage: subdev.Thermal."#funcname"(param)");  \
        return JS_FALSE;                                                       \
    }                                                                          \
                                                                               \
    JsThermal *pJsThermal;                                                     \
    if ((pJsThermal = JS_GET_PRIVATE(JsThermal, pContext, pObject, "JsThermal")) != 0) \
    {                                                                          \
        RC rc;                                                                 \
        Thermal* pThermal = pJsThermal->GetThermal();                          \
        C_CHECK_RC(pThermal->funcname(Input));                                 \
        RETURN_RC(rc);                                                         \
    }                                                                          \
    return JS_FALSE;                                                           \
}

JSTHERMAL_SETFUNC(FLOAT32, SetChipHighLimit, "Set high limit for chip temperature");
JSTHERMAL_SETFUNC(FLOAT32, SetChipShutdownLimit, "Set shutdown limit for chip temperature");
JSTHERMAL_SETFUNC(UINT32, DumpThermalTable, "dump entries of thermal table");
JSTHERMAL_SETFUNC(UINT32, SetCoolerPolicy, "Set RM's cooler policy (1==manual)");
JSTHERMAL_SETFUNC(UINT32, SetMinimumFanSpeed, "Set minimum fan speed as percent (0 to 100)");
JSTHERMAL_SETFUNC(UINT32, SetMaximumFanSpeed, "Set maximum fan speed as percent (0 to 100)");
JSTHERMAL_SETFUNC(UINT32, SetFanSpeed, "Set fan speed as percent (0 to 100)");

#define JSTHERMAL_BOOLPROP(funcname,helpmsg)                                                \
P_( JsThermal_Get##funcname );                                                              \
static SProperty Get##funcname                                                              \
(                                                                                           \
   JsThermal_Object,                                                                        \
   #funcname,                                                                               \
   0,                                                                                       \
   0,                                                                                       \
   JsThermal_Get##funcname,                                                                 \
   0,                                                                                       \
   JSPROP_READONLY,                                                                         \
   helpmsg                                                                                  \
);                                                                                          \
P_( JsThermal_Get##funcname )                                                               \
{                                                                                           \
   MASSERT(pContext != 0);                                                                  \
   MASSERT(pValue   != 0);                                                                  \
   JavaScriptPtr pJavaScript;                                                               \
   JsThermal *pJsThermal;                                                                   \
   if ((pJsThermal = JS_GET_PRIVATE(JsThermal, pContext, pObject, "JsThermal")) != 0)       \
   {                                                                                        \
       RC rc;                                                                               \
       Thermal* pThermal = pJsThermal->GetThermal();                                        \
       bool RetVal;                                                                         \
       if ((rc = pThermal->Get##funcname(&RetVal))!= OK)                                    \
       {                                                                                    \
           Printf(Tee::PriLow, "Thermal::Get" #funcname " returns %d\n", (UINT32)rc);       \
           *pValue = 0;                                                                     \
           return JS_TRUE;                                                                  \
       }                                                                                    \
       if (OK != pJavaScript->ToJsval(RetVal, pValue))                                      \
       {                                                                                    \
           JS_ReportError(pContext, "Failed to colwert Get" #funcname"");                   \
           *pValue = JSVAL_NULL;                                                            \
           return JS_FALSE;                                                                 \
       }                                                                                    \
       return JS_TRUE;                                                                      \
   }                                                                                        \
   return JS_FALSE;                                                                         \
}

JSTHERMAL_BOOLPROP(ForceSmbus, "user forced Smbus temperature senseor");
JSTHERMAL_BOOLPROP(ForceInt, "user forced internal temperature senseor");
JSTHERMAL_BOOLPROP(ForceExt, "user forced external temperature senseor");
JSTHERMAL_BOOLPROP(InternalPresent, "is the internal sensor present");
JSTHERMAL_BOOLPROP(ExternalPresent, "is the external sensor present");
JSTHERMAL_BOOLPROP(PrimaryPresent, "is the primary sensor present");

//Fan Speed end point error margin measurement
JS_STEST_LWSTOM(JsThermal,
                GetFanTachEndpointError,
                1,
                "get expected error for the expected RPM at the endpoints")
{
     MASSERT((pContext !=0) || (pArguments !=0));
     JavaScriptPtr pJs;
     JSObject * pEndPointObj = 0;
     if((NumArguments != 1) || (OK != pJs->FromJsval(pArguments[0], &pEndPointObj)))
     {
         char usage[] = "Error in calling function. Use GetFanTachEndpointError(endPtErrorObj)";
         JS_ReportError(pContext, usage);
         return JS_FALSE;
     }

    JsThermal *pJsThermal;
    if ((pJsThermal = JS_GET_PRIVATE(JsThermal, pContext, pObject, "JsThermal")) != 0)
    {
        Thermal* pThermal = pJsThermal->GetThermal();
        RETURN_RC(pThermal->GetFanTachEndpointError(pEndPointObj));
    }
    return JS_FALSE;
}

// PMU Fan Control Block Object
JS_STEST_LWSTOM(JsThermal, GetPmuFanCtrlBlk, 1, "Fan Control Block Get Access")
{
    MASSERT(pContext != 0);
    JavaScriptPtr pJs;
    JsThermal *pJsThermal;
    if ((pJsThermal = JS_GET_PRIVATE(JsThermal, pContext, pObject, "JsThermal")) != 0)
    {
        RC rc;
        Thermal *pThermal = pJsThermal->GetThermal();
        LW2080_CTRL_PMU_FAN_CONTROL_BLOCK FanCtrlBlk = {{0},{{0},{0}}};
        JSObject *pFCB = 0;

        C_CHECK_RC(pThermal->GetPmuFanCtrlBlk(&FanCtrlBlk));

        if ((NumArguments != 1) ||
            (OK != pJs->FromJsval(pArguments[0], &pFCB)))
        {
            JS_ReportError(pContext, "Usage: subdev.Thermal.GetPmuFanCtrlBlk([rtn])i\n");
            return JS_FALSE;
        }

        if ((OK != pJs->SetProperty(pFCB, "m",               (INT32)FanCtrlBlk.pvtData.pvtData1X.m)) ||
            (OK != pJs->SetProperty(pFCB, "b",               (INT32)FanCtrlBlk.pvtData.pvtData1X.b)) ||
            (OK != pJs->SetProperty(pFCB, "accelGain",       (INT32)FanCtrlBlk.pvtData.pvtData1X.accelGain)) ||
            (OK != pJs->SetProperty(pFCB, "midpointGain",    (INT32)FanCtrlBlk.pvtData.pvtData1X.midpointGain)) ||
            (OK != pJs->SetProperty(pFCB, "historyCount",    (UINT32)FanCtrlBlk.pvtData.pvtData1X.historyCount)) ||
            (OK != pJs->SetProperty(pFCB, "lookback",        (UINT32)FanCtrlBlk.pvtData.pvtData1X.lookback)) ||
            (OK != pJs->SetProperty(pFCB, "controlField",    (UINT32)FanCtrlBlk.pvtData.pvtData1X.controlField)) ||
            (OK != pJs->SetProperty(pFCB, "slopeLimit0",     (INT32)FanCtrlBlk.pvtData.pvtData1X.slopeLimit0)) ||
            (OK != pJs->SetProperty(pFCB, "slope1",          (INT32)FanCtrlBlk.pvtData.pvtData1X.slope1)) ||
            (OK != pJs->SetProperty(pFCB, "gravity",         (INT32)FanCtrlBlk.pvtData.pvtData1X.gravity)) ||
            (OK != pJs->SetProperty(pFCB, "revision",        (UINT32)FanCtrlBlk.pvtData.header.revision)) ||
            (OK != pJs->SetProperty(pFCB, "size",            (UINT32)FanCtrlBlk.pvtData.header.size)) ||
            (OK != pJs->SetProperty(pFCB, "pwmPctMin",       (UINT32)FanCtrlBlk.fanDesc.pwmPctMin)) ||
            (OK != pJs->SetProperty(pFCB, "pwmPctMax",       (UINT32)FanCtrlBlk.fanDesc.pwmPctMax)) ||
            (OK != pJs->SetProperty(pFCB, "pwmPctManual",    (UINT32)FanCtrlBlk.fanDesc.pwmPctManual)) ||
            (OK != pJs->SetProperty(pFCB, "pwmSource",       (UINT32)FanCtrlBlk.fanDesc.pwmSource)) ||
            (OK != pJs->SetProperty(pFCB, "pwmScaleSlope",   (INT32)FanCtrlBlk.fanDesc.pwmScaleSlope)) ||
            (OK != pJs->SetProperty(pFCB, "pwmScaleOffset",  (INT32)FanCtrlBlk.fanDesc.pwmScaleOffset)) ||
            (OK != pJs->SetProperty(pFCB, "pwmRawPeriod",    (UINT32)FanCtrlBlk.fanDesc.pwmRawPeriod)) ||
            (OK != pJs->SetProperty(pFCB, "pwmRampUpSlope",  (UINT32)FanCtrlBlk.fanDesc.pwmRampUpSlope)) ||
            (OK != pJs->SetProperty(pFCB, "pwmRampDownSlope",(UINT32)FanCtrlBlk.fanDesc.pwmRampDownSlope))
            )
        {
            JS_ReportError(pContext, "Failed to write all necessary members in PmuFanCtrlBlk in Javascript!");
            return JS_FALSE;
        }
        RETURN_RC(OK);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsThermal, SetPmuFanCtrlBlk, 1, "Fan Control Block Set Access")
{
    MASSERT(pContext != 0);
    JavaScriptPtr pJavaScript;
    JsThermal *pJsThermal;
    if ((pJsThermal = JS_GET_PRIVATE(JsThermal, pContext, pObject,
        "JsThermal")) != 0)
    {
        RC rc;
        Thermal *pThermal = pJsThermal->GetThermal();
        JSObject *pFCB = 0;
        if (OK != pJavaScript->FromJsval(pArguments[0], &pFCB))
        {
            JS_ReportError(pContext, "Failed to colwert PmuFanCtrlBlk");
            return JS_FALSE;
        }

        INT32   tmpInt;
        UINT32  tmpUint;
        LW2080_CTRL_PMU_FAN_CONTROL_BLOCK FanCtrlBlk = {{0},{{0},{0}}};

        JS_SET_MEMBER_VALUE_MACRO(pFCB, FanCtrlBlk.pvtData.pvtData1X.m,
                        tmpInt,  INT16,  "m");
        JS_SET_MEMBER_VALUE_MACRO(pFCB, FanCtrlBlk.pvtData.pvtData1X.b,
                        tmpInt,  INT16,  "b");
        JS_SET_MEMBER_VALUE_MACRO(pFCB, FanCtrlBlk.pvtData.pvtData1X.accelGain,
                        tmpInt,  INT16,  "accelGain");
        JS_SET_MEMBER_VALUE_MACRO(pFCB,
                        FanCtrlBlk.pvtData.pvtData1X.midpointGain,
                        tmpInt,  INT16,  "midpointGain");
        JS_SET_MEMBER_VALUE_MACRO(pFCB,
                        FanCtrlBlk.pvtData.pvtData1X.historyCount,
                        tmpUint, UINT08, "historyCount");
        JS_SET_MEMBER_VALUE_MACRO(pFCB, FanCtrlBlk.pvtData.pvtData1X.lookback,
                        tmpUint, UINT08, "lookback");
        JS_SET_MEMBER_VALUE_MACRO(pFCB,
                        FanCtrlBlk.pvtData.pvtData1X.controlField,
                        tmpUint, UINT16, "controlField");
        JS_SET_MEMBER_VALUE_MACRO(pFCB,
                        FanCtrlBlk.pvtData.pvtData1X.slopeLimit0,
                        tmpInt,  INT16,  "slopeLimit0");
        JS_SET_MEMBER_VALUE_MACRO(pFCB, FanCtrlBlk.pvtData.pvtData1X.slope1,
                        tmpInt,  INT16,  "slope1");
        JS_SET_MEMBER_VALUE_MACRO(pFCB, FanCtrlBlk.pvtData.pvtData1X.gravity,
                        tmpInt,  INT16,  "gravity");
        JS_SET_MEMBER_VALUE_MACRO(pFCB, FanCtrlBlk.pvtData.header.revision,
                        tmpUint, UINT08, "revision");
        JS_SET_MEMBER_VALUE_MACRO(pFCB, FanCtrlBlk.pvtData.header.size,
                        tmpUint, UINT08, "size");
        JS_SET_MEMBER_VALUE_MACRO(pFCB, FanCtrlBlk.fanDesc.pwmPctMin,
                        tmpUint, UINT08, "pwmPctMin");
        JS_SET_MEMBER_VALUE_MACRO(pFCB, FanCtrlBlk.fanDesc.pwmPctMax,
                        tmpUint, UINT08, "pwmPctMax");
        JS_SET_MEMBER_VALUE_MACRO(pFCB, FanCtrlBlk.fanDesc.pwmPctManual,
                        tmpUint, UINT08, "pwmPctManual");
        JS_SET_MEMBER_VALUE_MACRO(pFCB, FanCtrlBlk.fanDesc.pwmSource,
                        tmpUint, UINT08, "pwmSource");
        JS_SET_MEMBER_VALUE_MACRO(pFCB, FanCtrlBlk.fanDesc.pwmScaleSlope,
                        tmpInt,  INT16,  "pwmScaleSlope");
        JS_SET_MEMBER_VALUE_MACRO(pFCB, FanCtrlBlk.fanDesc.pwmScaleOffset,
                        tmpInt,  INT16,  "pwmScaleOffset");
        JS_SET_MEMBER_VALUE_MACRO(pFCB, FanCtrlBlk.fanDesc.pwmRawPeriod,
                        tmpUint, UINT32, "pwmRawPeriod");
        JS_SET_MEMBER_VALUE_MACRO(pFCB, FanCtrlBlk.fanDesc.pwmRampUpSlope,
                        tmpUint, UINT16, "pwmRampUpSlope");
        JS_SET_MEMBER_VALUE_MACRO(pFCB, FanCtrlBlk.fanDesc.pwmRampDownSlope,
                        tmpUint, UINT16, "pwmRampDownSlope");

        C_CHECK_RC(pThermal->SetPmuFanCtrlBlk(FanCtrlBlk));
        RETURN_RC(OK);
    }
    return JS_FALSE;
}

// Fan 2.0+ controls
JS_STEST_LWSTOM(JsThermal, GetFanObject, 1, "Get Fan 2.0+ object")
{
    MASSERT(pContext != 0);
    JavaScriptPtr pJs;
    JsThermal *pJsThermal;
    if ((pJsThermal =
            JS_GET_PRIVATE(JsThermal, pContext, pObject, "JsThermal")) != 0)
    {
        RC rc;
        Thermal *pThermal = pJsThermal->GetThermal();
        JSObject *pFPT = 0;
        if ((NumArguments != 1) ||
            (OK != pJs->FromJsval(pArguments[0], &pFPT)))
        {
            JS_ReportError(pContext, "Usage: subdev.Thermal.GetFanObject\
                (returnObject)\n");
            return JS_FALSE;
        }
        UINT32 PolicyMask = 0;
        UINT32 CoolerMask = 0;
        UINT32 ArbiterMask = 0;
        bool supportsMultiFan = false;

        C_CHECK_RC(pThermal->GetFanPolicyMask(&PolicyMask));
        C_CHECK_RC(pThermal->GetFanCoolerMask(&CoolerMask));
        C_CHECK_RC(pThermal->GetFanArbiterMask(&ArbiterMask));

        // A non zero mask means Fan 2.0+ is configured
        if(PolicyMask > 0 && CoolerMask > 0)
        {
            supportsMultiFan = true;
        }

        C_CHECK_RC(pJs->PackFields(pFPT, "IIIIIIIII",
            "supportsMultiFan", supportsMultiFan,
            "isFan20Enabled",  supportsMultiFan, // TODO: Deprecate
            "supportsFanArbiter", ArbiterMask > 0,
            "policyMask",  PolicyMask,
            "coolerMask",  CoolerMask,
            "arbiterMask", ArbiterMask,
            "maxPolicies", LW2080_CTRL_FAN_POLICY_MAX_POLICIES,
            "maxCoolers",  LW2080_CTRL_FAN_COOLER_MAX_COOLERS,
            "maxArbiters", LW2080_CTRL_FAN_ARBITER_MAX_ARBITERS));

        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsThermal,
                DumpFanPolicyTable,
                1,
                "Dump Fan 2.0+ Policy table")
{
    MASSERT(pContext!=0);
    JavaScriptPtr pJs;
    bool defaultValues;
    if ((NumArguments != 1) ||
        (OK != pJs->FromJsval(pArguments[0], &defaultValues)))
    {
        JS_ReportError(pContext, "subdev.Thermal.DumpFanPolicyTable(defaultValues)\n");
        return JS_FALSE;
    }

    JsThermal *pJsThermal;
    if ((pJsThermal =
            JS_GET_PRIVATE(JsThermal, pContext, pObject, "JsThermal")) != 0)
    {
        RC rc;
        Thermal* pThermal = pJsThermal->GetThermal();
        C_CHECK_RC(pThermal->DumpFanPolicyTable(defaultValues));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsThermal, GetFanPolicyTable, 3, "Get Fan 2.0+ Policy table")
{
    RC rc;
    STEST_HEADER(3, 3, "Usage: subdev.Thermal.GetFanPolicyTable\
                        (policyMask, default, returnArray)");
    STEST_PRIVATE(JsThermal, pJsThermal, "JsThermal");
    STEST_ARG(0, UINT32, mask);
    STEST_ARG(1, bool, defaultValues);
    STEST_ARG(2, JSObject *, pFanPolicyTable);

    Thermal *pThermal = pJsThermal->GetThermal();
    UINT32 PolicyMask = 0;
    C_CHECK_RC(pThermal->GetFanPolicyMask(&PolicyMask));
    if (!(mask & PolicyMask))
    {
        Printf(Tee::PriNormal,
                "Fan 2.0+ policy for policy mask 0x%x does not exist.\n",
                mask);
        RETURN_RC(RC::SOFTWARE_ERROR);
    }

    LW2080_CTRL_FAN_POLICY_INFO_PARAMS infoParams = {0};
    LW2080_CTRL_FAN_POLICY_STATUS_PARAMS statusParams = {};
    LW2080_CTRL_FAN_POLICY_CONTROL_PARAMS ctrlParams = {0};
    ctrlParams.bDefault = defaultValues;
    C_CHECK_RC(pThermal->GetFanPolicyTable(&infoParams, &statusParams,
               &ctrlParams));
    for (UINT32 index = 0; index < LW2080_CTRL_FAN_POLICY_MAX_POLICIES; index++)
    {
        if (BIT(index) & mask)
        {
            LW2080_CTRL_FAN_POLICY_INFO_DATA infoData =
                infoParams.policies[index].data;
            LW2080_CTRL_FAN_POLICY_STATUS_DATA statusData =
                statusParams.policies[index].data;
            LW2080_CTRL_FAN_POLICY_CONTROL_DATA ctrlData =
                ctrlParams.policies[index].data;

            JSObject *pFPT = JS_NewObject(pContext, &FPTClass, NULL, NULL);
            switch (infoParams.policies[index].type)
            {
            case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20:
                C_CHECK_RC(pJavaScript->PackFields(pFPT, "IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII",
                "type", infoParams.policies[index].type,
                "coolerIdx", infoData.iirTFDSPV20.fanPolicyV20.coolerIdx,
                "thermChIdx", infoParams.policies[index].thermChIdx,
                "samplingPeriodms", infoData.iirTFDSPV20.fanPolicyV20.samplingPeriodms,
                "bUsePwm", infoData.iirTFDSPV20.iirTFDSP.bUsePwm,
                "tjAvgShortTerm", statusData.iirTFDSPV20.iirTFDSP.tjAvgShortTerm,
                "tjAvgLongTerm", statusData.iirTFDSPV20.iirTFDSP.tjAvgLongTerm,
                "targetPwm", statusData.iirTFDSPV20.iirTFDSP.targetPwm,
                "targetRpm", statusData.iirTFDSPV20.iirTFDSP.targetRpm,
                "iirGainMin", ctrlData.iirTFDSPV20.iirTFDSP.iirGainMin,
                "iirGainMax", ctrlData.iirTFDSPV20.iirTFDSP.iirGainMax,
                "iirGainShortTerm", ctrlData.iirTFDSPV20.iirTFDSP.iirGainShortTerm,
                "iirFilterPower", ctrlData.iirTFDSPV20.iirTFDSP.iirFilterPower,
                "iirLongTermSamplingRatio",
                    ctrlData.iirTFDSPV20.iirTFDSP.iirLongTermSamplingRatio,
                "iirFilterWidthUpper", ctrlData.iirTFDSPV20.iirTFDSP.iirFilterWidthUpper,
                "iirFilterWidthLower", ctrlData.iirTFDSPV20.iirTFDSP.iirFilterWidthLower,
                "tj0", ctrlData.iirTFDSPV20.iirTFDSP.fanLwrvePts[0].tj,
                "tj1", ctrlData.iirTFDSPV20.iirTFDSP.fanLwrvePts[1].tj,
                "tj2", ctrlData.iirTFDSPV20.iirTFDSP.fanLwrvePts[2].tj,
                "pwm0", ctrlData.iirTFDSPV20.iirTFDSP.fanLwrvePts[0].pwm,
                "pwm1", ctrlData.iirTFDSPV20.iirTFDSP.fanLwrvePts[1].pwm,
                "pwm2", ctrlData.iirTFDSPV20.iirTFDSP.fanLwrvePts[2].pwm,
                "rpm0", ctrlData.iirTFDSPV20.iirTFDSP.fanLwrvePts[0].rpm,
                "rpm1", ctrlData.iirTFDSPV20.iirTFDSP.fanLwrvePts[1].rpm,
                "rpm2", ctrlData.iirTFDSPV20.iirTFDSP.fanLwrvePts[2].rpm,
                // Fan 2.1 (FanStop) fields
                "bFanStopSupported", infoData.iirTFDSPV20.iirTFDSP.bFanStopSupported,
                "bFanStopEnableDefault", infoData.iirTFDSPV20.iirTFDSP.bFanStopEnableDefault,
                "fanStartMinHoldTimems", infoData.iirTFDSPV20.iirTFDSP.fanStartMinHoldTimems,
                "fanStopPowerTopologyIdx", infoData.iirTFDSPV20.iirTFDSP.fanStopPowerTopologyIdx,
                "tjLwrrent", statusData.iirTFDSPV20.iirTFDSP.tjLwrrent,
                "filteredPwrmW", statusData.iirTFDSPV20.iirTFDSP.filteredPwrmW,
                "bFanStopActive", statusData.iirTFDSPV20.iirTFDSP.bFanStopActive,
                "bFanStopEnable", ctrlData.iirTFDSPV20.iirTFDSP.bFanStopEnable,
                "fanStopTempLimitLower", ctrlData.iirTFDSPV20.iirTFDSP.fanStopTempLimitLower,
                "fanStartTempLimitUpper", ctrlData.iirTFDSPV20.iirTFDSP.fanStartTempLimitUpper,
                "fanStopPowerLimitLowermW", ctrlData.iirTFDSPV20.iirTFDSP.fanStopPowerLimitLowermW,
                "fanStartPowerLimitUppermW", ctrlData.iirTFDSPV20.iirTFDSP.fanStartPowerLimitUppermW,
                "fanStopIIRGainPower", ctrlData.iirTFDSPV20.iirTFDSP.fanStopIIRGainPower
                ));
                break;
            case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30:
                C_CHECK_RC(pJavaScript->PackFields(pFPT, "IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII",
                "type", infoParams.policies[index].type,
                "thermChIdx", infoParams.policies[index].thermChIdx,
                "bUsePwm", infoData.iirTFDSPV30.iirTFDSP.bUsePwm,
                "tjAvgShortTerm", statusData.iirTFDSPV30.iirTFDSP.tjAvgShortTerm,
                "tjAvgLongTerm", statusData.iirTFDSPV30.iirTFDSP.tjAvgLongTerm,
                "targetPwm", statusData.iirTFDSPV30.iirTFDSP.targetPwm,
                "targetRpm", statusData.iirTFDSPV30.iirTFDSP.targetRpm,
                "iirGainMin", ctrlData.iirTFDSPV30.iirTFDSP.iirGainMin,
                "iirGainMax", ctrlData.iirTFDSPV30.iirTFDSP.iirGainMax,
                "iirGainShortTerm", ctrlData.iirTFDSPV30.iirTFDSP.iirGainShortTerm,
                "iirFilterPower", ctrlData.iirTFDSPV30.iirTFDSP.iirFilterPower,
                "iirLongTermSamplingRatio",
                    ctrlData.iirTFDSPV30.iirTFDSP.iirLongTermSamplingRatio,
                "iirFilterWidthUpper", ctrlData.iirTFDSPV30.iirTFDSP.iirFilterWidthUpper,
                "iirFilterWidthLower", ctrlData.iirTFDSPV30.iirTFDSP.iirFilterWidthLower,
                "tj0", ctrlData.iirTFDSPV30.iirTFDSP.fanLwrvePts[0].tj,
                "tj1", ctrlData.iirTFDSPV30.iirTFDSP.fanLwrvePts[1].tj,
                "tj2", ctrlData.iirTFDSPV30.iirTFDSP.fanLwrvePts[2].tj,
                "pwm0", ctrlData.iirTFDSPV30.iirTFDSP.fanLwrvePts[0].pwm,
                "pwm1", ctrlData.iirTFDSPV30.iirTFDSP.fanLwrvePts[1].pwm,
                "pwm2", ctrlData.iirTFDSPV30.iirTFDSP.fanLwrvePts[2].pwm,
                "rpm0", ctrlData.iirTFDSPV30.iirTFDSP.fanLwrvePts[0].rpm,
                "rpm1", ctrlData.iirTFDSPV30.iirTFDSP.fanLwrvePts[1].rpm,
                "rpm2", ctrlData.iirTFDSPV30.iirTFDSP.fanLwrvePts[2].rpm,
                // Fan 2.1 (FanStop) fields
                "bFanStopSupported", infoData.iirTFDSPV30.iirTFDSP.bFanStopSupported,
                "bFanStopEnableDefault", infoData.iirTFDSPV30.iirTFDSP.bFanStopEnableDefault,
                "fanStartMinHoldTimems", infoData.iirTFDSPV30.iirTFDSP.fanStartMinHoldTimems,
                "fanStopPowerTopologyIdx", infoData.iirTFDSPV30.iirTFDSP.fanStopPowerTopologyIdx,
                "tjLwrrent", statusData.iirTFDSPV30.iirTFDSP.tjLwrrent,
                "filteredPwrmW", statusData.iirTFDSPV30.iirTFDSP.filteredPwrmW,
                "bFanStopActive", statusData.iirTFDSPV30.iirTFDSP.bFanStopActive,
                "bFanStopEnable", ctrlData.iirTFDSPV30.iirTFDSP.bFanStopEnable,
                "fanStopTempLimitLower", ctrlData.iirTFDSPV30.iirTFDSP.fanStopTempLimitLower,
                "fanStartTempLimitUpper", ctrlData.iirTFDSPV30.iirTFDSP.fanStartTempLimitUpper,
                "fanStopPowerLimitLowermW", ctrlData.iirTFDSPV30.iirTFDSP.fanStopPowerLimitLowermW,
                "fanStartPowerLimitUppermW", ctrlData.iirTFDSPV30.iirTFDSP.fanStartPowerLimitUppermW,
                "fanStopIIRGainPower", ctrlData.iirTFDSPV30.iirTFDSP.fanStopIIRGainPower,
                // Fan 3.0 fields
                "bFanLwrvePt2TjOverride", infoData.iirTFDSPV30.bFanLwrvePt2TjOverride,
                "bFanLwrveAdjSupported", infoData.iirTFDSPV30.bFanLwrveAdjSupported
                ));
                break;
            default:
                Printf(Tee::PriError,
                        "Unsupported fan policy type: %u\n",
                        infoParams.policies[index].type);
                RETURN_RC(RC::SOFTWARE_ERROR);
            }
            jsval FanJsval;
            C_CHECK_RC(pJavaScript->ToJsval(pFPT, &FanJsval));
            C_CHECK_RC(pJavaScript->SetElement(pFanPolicyTable, index, FanJsval));
        }
    }
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsThermal, SetFanPolicyTable, 2, "Set Fan 2.0+ Policy Table")
{
    RC rc;
    STEST_HEADER(2, 2, "Usage: subdev.Thermal.SetFanPolicyTable\
                        (policyMask, returnArray)");
    STEST_PRIVATE(JsThermal, pJsThermal, "JsThermal");
    STEST_ARG(0, UINT32, mask);
    STEST_ARG(1, JSObject*, pFanPolicyTable);

    Thermal *pThermal = pJsThermal->GetThermal();
    LW2080_CTRL_FAN_POLICY_INFO_PARAMS infoParams = {0};
    LW2080_CTRL_FAN_POLICY_STATUS_PARAMS statusParams = {};
    LW2080_CTRL_FAN_POLICY_CONTROL_PARAMS ctrlParams = {0};
    ctrlParams.bDefault = false;    //get current active values
    C_CHECK_RC(pThermal->GetFanPolicyTable(&infoParams, &statusParams,
               &ctrlParams));

    ctrlParams.policyMask = mask;
    for (UINT32 index = 0; index < LW2080_CTRL_FAN_COOLER_MAX_COOLERS; index++)
    {
        if (BIT(index) & mask)
        {
            JSObject *pFPT = 0;
            CHECK_RC(pJavaScript->GetElement(pFanPolicyTable, index, &pFPT));

            INT32   tmpInt;
            UINT32  tmpUint;
            UINT08  tmpUint8;

            switch (ctrlParams.policies[index].type)
            {
            case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20:
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV20.iirTFDSP.iirGainMin,
                    tmpInt,  INT32, "iirGainMin");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV20.iirTFDSP.iirGainMax,
                    tmpInt,  INT32, "iirGainMax");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV20.iirTFDSP.iirGainShortTerm,
                    tmpInt,  INT32, "iirGainShortTerm");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV20.iirTFDSP.iirFilterPower,
                    tmpUint8,  UINT08, "iirFilterPower");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV20.iirTFDSP.iirLongTermSamplingRatio,
                    tmpUint8, UINT08, "iirLongTermSamplingRatio");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV20.iirTFDSP.iirFilterWidthUpper,
                    tmpInt,  INT32, "iirFilterWidthUpper");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV20.iirTFDSP.iirFilterWidthLower,
                    tmpInt,  INT32, "iirFilterWidthLower");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV20.iirTFDSP.fanLwrvePts[0].tj,
                    tmpInt,  INT32, "tj0");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV20.iirTFDSP.fanLwrvePts[1].tj,
                    tmpInt,  INT32, "tj1");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV20.iirTFDSP.fanLwrvePts[2].tj,
                    tmpInt,  INT32, "tj2");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV20.iirTFDSP.fanLwrvePts[0].pwm,
                    tmpUint,  UINT32, "pwm0");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV20.iirTFDSP.fanLwrvePts[1].pwm,
                    tmpUint,  UINT32, "pwm1");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV20.iirTFDSP.fanLwrvePts[2].pwm,
                    tmpUint,  UINT32, "pwm2");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV20.iirTFDSP.fanLwrvePts[0].rpm,
                    tmpUint,  UINT32, "rpm0");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV20.iirTFDSP.fanLwrvePts[1].rpm,
                    tmpUint,  UINT32, "rpm1");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV20.iirTFDSP.fanLwrvePts[2].rpm,
                    tmpUint,  UINT32, "rpm2");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV20.iirTFDSP.bFanStopEnable,
                    tmpUint8, UINT08, "bFanStopEnable");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV20.iirTFDSP.fanStopTempLimitLower,
                    tmpUint, UINT32, "fanStopTempLimitLower");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV20.iirTFDSP.fanStartTempLimitUpper,
                    tmpUint, UINT32, "fanStartTempLimitUpper");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV20.iirTFDSP.fanStopPowerLimitLowermW,
                    tmpUint, UINT32, "fanStopPowerLimitLowermW");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV20.iirTFDSP.fanStartPowerLimitUppermW,
                    tmpUint, UINT32, "fanStartPowerLimitUppermW");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV20.iirTFDSP.fanStopIIRGainPower,
                    tmpUint, UINT32, "fanStopIIRGainPower");
                break;
            case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30:
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV30.iirTFDSP.iirGainMin,
                    tmpInt,  INT32, "iirGainMin");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV30.iirTFDSP.iirGainMax,
                    tmpInt,  INT32, "iirGainMax");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV30.iirTFDSP.iirGainShortTerm,
                    tmpInt,  INT32, "iirGainShortTerm");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV30.iirTFDSP.iirFilterPower,
                    tmpUint8,  UINT08, "iirFilterPower");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV30.iirTFDSP.iirLongTermSamplingRatio,
                    tmpUint8, UINT08, "iirLongTermSamplingRatio");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV30.iirTFDSP.iirFilterWidthUpper,
                    tmpInt,  INT32, "iirFilterWidthUpper");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV30.iirTFDSP.iirFilterWidthLower,
                    tmpInt,  INT32, "iirFilterWidthLower");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV30.iirTFDSP.fanLwrvePts[0].tj,
                    tmpInt,  INT32, "tj0");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV30.iirTFDSP.fanLwrvePts[1].tj,
                    tmpInt,  INT32, "tj1");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV30.iirTFDSP.fanLwrvePts[2].tj,
                    tmpInt,  INT32, "tj2");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV30.iirTFDSP.fanLwrvePts[0].pwm,
                    tmpUint,  UINT32, "pwm0");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV30.iirTFDSP.fanLwrvePts[1].pwm,
                    tmpUint,  UINT32, "pwm1");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV30.iirTFDSP.fanLwrvePts[2].pwm,
                    tmpUint,  UINT32, "pwm2");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV30.iirTFDSP.fanLwrvePts[0].rpm,
                    tmpUint,  UINT32, "rpm0");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV30.iirTFDSP.fanLwrvePts[1].rpm,
                    tmpUint,  UINT32, "rpm1");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV30.iirTFDSP.fanLwrvePts[2].rpm,
                    tmpUint,  UINT32, "rpm2");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV30.iirTFDSP.bFanStopEnable,
                    tmpUint8, UINT08, "bFanStopEnable");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV30.iirTFDSP.fanStopTempLimitLower,
                    tmpUint, UINT32, "fanStopTempLimitLower");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV30.iirTFDSP.fanStartTempLimitUpper,
                    tmpUint, UINT32, "fanStartTempLimitUpper");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV30.iirTFDSP.fanStopPowerLimitLowermW,
                    tmpUint, UINT32, "fanStopPowerLimitLowermW");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV30.iirTFDSP.fanStartPowerLimitUppermW,
                    tmpUint, UINT32, "fanStartPowerLimitUppermW");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.policies[index].data.iirTFDSPV30.iirTFDSP.fanStopIIRGainPower,
                    tmpUint, UINT32, "fanStopIIRGainPower");
                break;
            default:
                Printf(Tee::PriError,
                        "Unsupported fan policy type: %u\n",
                        ctrlParams.policies[index].type);
                RETURN_RC(RC::SOFTWARE_ERROR);
            }
        }
    }
    C_CHECK_RC(pThermal->SetFanPolicyTable(ctrlParams));
    RETURN_RC(OK);
}

JS_STEST_LWSTOM(JsThermal,
                DumpFanCoolerTable,
                1,
                "Dump Fan 2.0+ Cooler table")
{
    MASSERT(pContext!=0);
    JavaScriptPtr pJs;
    bool defaultValues;
    if ((NumArguments != 1) ||
        (OK != pJs->FromJsval(pArguments[0], &defaultValues)))
    {
        JS_ReportError(pContext, "subdev.Thermal.DumpFanCoolerTable(defaultValues)\n");
        return JS_FALSE;
    }

    JsThermal *pJsThermal;
    if ((pJsThermal = JS_GET_PRIVATE(JsThermal, pContext, pObject, "JsThermal")) != 0)
    {
        RC rc;
        Thermal* pThermal = pJsThermal->GetThermal();
        C_CHECK_RC(pThermal->DumpFanCoolerTable(defaultValues));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

// Fan 2.0+ controls
JS_STEST_LWSTOM(JsThermal, GetFanCoolerTable, 3, "Get Fan 2.0+ Cooler table")
{
    RC rc;
    STEST_HEADER(3, 3, "Usage: subdev.Thermal.GetFanCoolerTable\
                        (coolerMask, default, returnArray)");
    STEST_PRIVATE(JsThermal, pJsThermal, "JsThermal");
    STEST_ARG(0, UINT32, mask);
    STEST_ARG(1, bool, defaultValues);
    STEST_ARG(2, JSObject *, pFanCoolerTable);

    UINT32 CoolerMask = 0;
    Thermal *pThermal = pJsThermal->GetThermal();
    C_CHECK_RC(pThermal->GetFanCoolerMask(&CoolerMask));
    if (!(mask & CoolerMask))
    {
        Printf(Tee::PriNormal, "Fan 2.0+ cooler for cooler mask 0x%x does"
            ", not exist.\n", mask);
        RETURN_RC(RC::SOFTWARE_ERROR);
    }

    LW2080_CTRL_FAN_COOLER_INFO_PARAMS infoParams = {0};
    LW2080_CTRL_FAN_COOLER_STATUS_PARAMS statusParams = {};
    LW2080_CTRL_FAN_COOLER_CONTROL_PARAMS ctrlParams = {0};
    ctrlParams.bDefault = defaultValues;
    C_CHECK_RC(pThermal->GetFanCoolerTable(&infoParams, &statusParams, &ctrlParams));
    for (UINT32 index = 0; index < LW2080_CTRL_FAN_COOLER_MAX_COOLERS; index++)
    {
        if (BIT(index) & mask)
        {
            LW2080_CTRL_FAN_COOLER_INFO_DATA infoData =
                infoParams.coolers[index].data;
            LW2080_CTRL_FAN_COOLER_STATUS_DATA statusData =
                statusParams.coolers[index].data;
            LW2080_CTRL_FAN_COOLER_CONTROL_DATA ctrlData =
                ctrlParams.coolers[index].data;

            JSObject *pFPT = JS_NewObject(pContext, &FPTClass, NULL, NULL);
            switch (infoParams.coolers[index].type)
            {
            case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE:
                C_CHECK_RC(pJavaScript->PackFields(pFPT, "IIIIIIIIIIIII",
                    "tachFuncFan",  infoData.active.tachFuncFan,
                    "tachRate",  infoData.active.tachRate,
                    "bTachSupported",  infoData.active.bTachSupported,
                    "controlUnit",  infoData.active.controlUnit,
                    "rpmLwrr",  statusData.active.rpmLwrr,
                    "levelMin",  statusData.active.levelMin,
                    "levelMax",  statusData.active.levelMax,
                    "levelLwrrent",  statusData.active.levelLwrrent,
                    "levelTarget",  statusData.active.levelTarget,
                    "rpmMin",  ctrlData.active.rpmMin,
                    "rpmMax",  ctrlData.active.rpmMax,
                    "bLevelSimActive",  ctrlData.active.bLevelSimActive,
                    "levelSim",  ctrlData.active.levelSim));
                break;

            case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
                C_CHECK_RC(pJavaScript->PackFields(pFPT, "IIIIIIIIIIIIIIIIIIIIIIIIIIIIII",
                    "scaling", infoData.activePwm.scaling,
                    "offset",  infoData.activePwm.offset,
                    "gpioFuncFan",  infoData.activePwm.gpioFuncFan,
                    "freq",  infoData.activePwm.freq,
                    "maxFanEvtMask", infoData.activePwm.maxFanEvtMask,
                    "maxFanMinTimems", infoData.activePwm.maxFanMinTimems,
                    "tachFuncFan",  infoData.activePwm.active.tachFuncFan,
                    "tachRate",  infoData.activePwm.active.tachRate,
                    "bTachSupported",  infoData.activePwm.active.bTachSupported,
                    "controlUnit",  infoData.activePwm.active.controlUnit,
                    "tachPin", infoData.activePwm.active.tachPin,
                    "maxFanEvtMask", infoData.activePwm.maxFanEvtMask,
                    "maxFanMinTimems", infoData.activePwm.maxFanMinTimems,
                    "pwmRequested",  statusData.activePwm.pwmRequested,
                    "pwmLwrr",  statusData.activePwm.pwmLwrr,
                    "rpmLwrr",  statusData.activePwm.active.rpmLwrr,
                    "levelMin",  statusData.activePwm.active.levelMin,
                    "levelMax",  statusData.activePwm.active.levelMax,
                    "levelLwrrent",  statusData.activePwm.active.levelLwrrent,
                    "levelTarget",  statusData.activePwm.active.levelTarget,
                    "bMaxFanActive", statusData.activePwm.bMaxFanActive,
                    "pwmMin",  ctrlData.activePwm.pwmMin,
                    "pwmMax",  ctrlData.activePwm.pwmMax,
                    "bPwmSimActive",  ctrlData.activePwm.bPwmSimActive,
                    "pwmSim",  ctrlData.activePwm.pwmSim,
                    "rpmMin",  ctrlData.activePwm.active.rpmMin,
                    "rpmMax",  ctrlData.activePwm.active.rpmMax,
                    "bLevelSimActive",  ctrlData.activePwm.active.bLevelSimActive,
                    "levelSim",  ctrlData.activePwm.active.levelSim,
                    "maxFanPwm", ctrlData.activePwm.maxFanPwm));
                break;

            case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:

                C_CHECK_RC(pJavaScript->PackFields(pFPT, "IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII",
                    "scaling", infoData.activePwmTachCorr.activePwm.scaling,
                    "offset", infoData.activePwmTachCorr.activePwm.offset,
                    "gpioFuncFan", infoData.activePwmTachCorr.activePwm.gpioFuncFan,
                    "freq", infoData.activePwmTachCorr.activePwm.freq,
                    "maxFanEvtMask", infoData.activePwmTachCorr.activePwm.maxFanEvtMask,
                    "maxFanMinTimems", infoData.activePwmTachCorr.activePwm.maxFanMinTimems,
                    "tachFuncFan", infoData.activePwmTachCorr.activePwm.active.tachFuncFan,
                    "tachRate", infoData.activePwmTachCorr.activePwm.active.tachRate,
                    "bTachSupported", infoData.activePwmTachCorr.activePwm.active.bTachSupported,
                    "controlUnit", infoData.activePwmTachCorr.activePwm.active.controlUnit,
                    "tachPin", infoData.activePwmTachCorr.activePwm.active.tachPin,
                    "rpmLast", statusData.activePwmTachCorr.rpmLast,
                    "rpmTarget", statusData.activePwmTachCorr.rpmTarget,
                    "pwmRequested",  statusData.activePwmTachCorr.activePwm.pwmRequested,
                    "pwmLwrr", statusData.activePwmTachCorr.activePwm.pwmLwrr,
                    "rpmLwrr", statusData.activePwmTachCorr.activePwm.active.rpmLwrr,
                    "levelMin", statusData.activePwmTachCorr.activePwm.active.levelMin,
                    "levelMax", statusData.activePwmTachCorr.activePwm.active.levelMax,
                    "levelLwrrent", statusData.activePwmTachCorr.activePwm.active.levelLwrrent,
                    "levelTarget", statusData.activePwmTachCorr.activePwm.active.levelTarget,
                    "bMaxFanActive", statusData.activePwmTachCorr.activePwm.bMaxFanActive,
                    "pwmActual", statusData.activePwmTachCorr.pwmActual,
                    "propGain", ctrlData.activePwmTachCorr.propGain,
                    "bRpmSimActive", ctrlData.activePwmTachCorr.bRpmSimActive,
                    "rpmSim", ctrlData.activePwmTachCorr.rpmSim,
                    "pwmMin", ctrlData.activePwmTachCorr.activePwm.pwmMin,
                    "pwmMax", ctrlData.activePwmTachCorr.activePwm.pwmMax,
                    "bPwmSimActive", ctrlData.activePwmTachCorr.activePwm.bPwmSimActive,
                    "pwmSim", ctrlData.activePwmTachCorr.activePwm.pwmSim,
                    "rpmMin", ctrlData.activePwmTachCorr.activePwm.active.rpmMin,
                    "rpmMax", ctrlData.activePwmTachCorr.activePwm.active.rpmMax,
                    "bLevelSimActive", ctrlData.activePwmTachCorr.activePwm.active.bLevelSimActive,
                    "levelSim", ctrlData.activePwmTachCorr.activePwm.active.levelSim,
                    "pwmFloorLimitOffset", ctrlData.activePwmTachCorr.pwmFloorLimitOffset));
                break;
            }
            jsval FanJsval;
            C_CHECK_RC(pJavaScript->ToJsval(pFPT, &FanJsval));
            C_CHECK_RC(pJavaScript->SetElement(pFanCoolerTable, index, FanJsval));
        }
    }
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsThermal, SetFanCoolerTable, 2, "Set Fan 2.0+ Cooler Table")
{
    RC rc;
    STEST_HEADER(2, 2, "Usage: subdev.Thermal.SetFanCoolerTable\
                        (coolerMask, returnArray)");
    STEST_PRIVATE(JsThermal, pJsThermal, "JsThermal");
    STEST_ARG(0, UINT32, mask);
    STEST_ARG(1, JSObject*, pFanCoolerTable);

    LW2080_CTRL_FAN_COOLER_INFO_PARAMS infoParams = {0};
    LW2080_CTRL_FAN_COOLER_STATUS_PARAMS statusParams = {};
    LW2080_CTRL_FAN_COOLER_CONTROL_PARAMS ctrlParams = {0};
    ctrlParams.bDefault = false;
    Thermal *pThermal = pJsThermal->GetThermal();
    C_CHECK_RC(pThermal->GetFanCoolerTable(&infoParams, &statusParams, &ctrlParams));
    ctrlParams.coolerMask = mask;
    for (UINT32 index = 0; index < LW2080_CTRL_FAN_COOLER_MAX_COOLERS; index++)
    {
        if (BIT(index) & mask)
        {
            JSObject *pFPT = 0;
            CHECK_RC(pJavaScript->GetElement(pFanCoolerTable, index, &pFPT));

            INT32 tmpInt;
            UINT32 tmpUint;
            bool tmpbool;
            if (ctrlParams.coolers[index].type ==
                    LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE)
            {
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.active.rpmMin,
                    tmpUint, UINT32, "rpmMin");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.active.rpmMax, tmpUint, UINT32,
                    "rpmMax");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.active.bLevelSimActive, tmpbool,
                    bool, "bLevelSimActive");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.active.levelSim, tmpUint,
                    UINT32, "levelSim");
            }
            else if (ctrlParams.coolers[index].type ==
                    LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM)
            {
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.activePwm.pwmMin, tmpUint,
                    UINT32, "pwmMin");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.activePwm.pwmMax, tmpUint,
                    UINT32, "pwmMax");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.activePwm.bPwmSimActive,
                    tmpbool, bool, "bPwmSimActive");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.activePwm.pwmSim, tmpUint,
                    UINT32, "pwmSim");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.activePwm.active.rpmMin,
                    tmpUint, UINT32, "rpmMin");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.activePwm.active.rpmMax,
                    tmpUint, UINT32, "rpmMax");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.activePwm.active.bLevelSimActive,
                    tmpbool, bool, "bLevelSimActive");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.activePwm.active.levelSim,
                    tmpUint, UINT32, "levelSim");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.activePwm.maxFanPwm,
                    tmpUint, UINT32, "maxFanPwm");
            }
            else if(ctrlParams.coolers[index].type ==
                    LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR)
            {
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.activePwmTachCorr.propGain,
                    tmpInt, INT32, "propGain");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.activePwmTachCorr.bRpmSimActive,
                    tmpbool, bool, "bRpmSimActive");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.activePwmTachCorr.rpmSim,
                    tmpUint, UINT32, "rpmSim");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.activePwmTachCorr.activePwm.pwmMin,
                    tmpUint, UINT32, "pwmMin");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.activePwmTachCorr.activePwm.pwmMax,
                    tmpUint, UINT32, "pwmMax");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.activePwmTachCorr.activePwm.bPwmSimActive,
                    tmpbool, bool, "bPwmSimActive");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.activePwmTachCorr.activePwm.pwmSim,
                    tmpUint, UINT32, "pwmSim");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.activePwmTachCorr.activePwm.active.rpmMin,
                    tmpUint, UINT32, "rpmMin");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.activePwmTachCorr.activePwm.active.rpmMax,
                    tmpUint, UINT32, "rpmMax");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.activePwmTachCorr.activePwm.active.bLevelSimActive,
                    tmpbool, bool, "bLevelSimActive");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.activePwmTachCorr.activePwm.active.levelSim,
                    tmpUint, UINT32, "levelSim");
                JS_SET_MEMBER_VALUE_MACRO(pFPT,
                    ctrlParams.coolers[index].data.activePwmTachCorr.pwmFloorLimitOffset,
                    tmpUint, UINT32, "pwmFloorLimitOffset");
            }
        }
    }
    C_CHECK_RC(pThermal->SetFanCoolerTable(ctrlParams));
    RETURN_RC(rc);
}

JS_SMETHOD_LWSTOM(JsThermal,
                  FanCoolerActiveControlUnitToString,
                  1,
                  "Colwert Fan Cooler controlUnit Id to string")
{
    STEST_HEADER(1, 1, "Usage: let controlUnitStr = subdev.Thermal.FanCoolerActiveControlUnitToString(controlUnit)");
    STEST_ARG(0, UINT32, controlUnit);

    auto controlUnitStr = Thermal::FanCoolerActiveControlUnitToString(controlUnit);

    RC rc = pJavaScript->ToJsval(controlUnitStr, pReturlwalue);
    if (RC::OK != rc)
    {
        pJavaScript->Throw(pContext, rc, "Error oclwrred in subdev.Thermal.FanCoolerActiveControlUnitToString");
        return JS_FALSE;
    }
    return JS_TRUE;
}

JS_STEST_LWSTOM(JsThermal, GetFanTestParams, 1, "Get Fan test entries")
{
    RC rc;
    STEST_HEADER(1, 1, "Usage: subdev.Thermal.GetFanTestParams\
                        (returnArray)");
    STEST_PRIVATE(JsThermal, pJsThermal, "JsThermal");
    STEST_ARG(0, JSObject *, pFanTestParams);

    vector<Thermal::FanTestParams> testParams;
    Thermal *pThermal = pJsThermal->GetThermal();
    C_CHECK_RC(pThermal->GetFanTestParams(&testParams));

    UINT32 index = 0;
    for (const auto& testParam : testParams)
    {
        JSObject *pFPT = JS_NewObject(pContext, &FPTClass, NULL, NULL);
        C_CHECK_RC(pJavaScript->PackFields(pFPT, "III",
            "coolerTableIdx", testParam.coolerIdx,
            "measurementTolerancePct",  testParam.measTolPct,
            "colwergenceTimeMs",  testParam.colwTimeMs));

        jsval FanJsval;
        C_CHECK_RC(pJavaScript->ToJsval(pFPT, &FanJsval));
        C_CHECK_RC(pJavaScript->SetElement(pFanTestParams, index++, FanJsval));
    }
    RETURN_RC(rc);
}

JS_CLASS(FanArbiterStatus);
JS_STEST_LWSTOM(JsThermal,
                GetFanArbiterStatus,
                2,
                "Get Fan Arbiter Status")
{
    STEST_HEADER(2, 2, "Usage: subdev.Thermal.GetFanArbiterStatus(arbiterMask, returnArray)");
    STEST_PRIVATE(JsThermal, pJsThermal, "JsThermal");
    STEST_ARG(0, UINT32, arbiterMask);
    STEST_ARG(1, JSObject *, pJsFanArbiterStatusParams);

    RC rc;
    Thermal *pThermal = pJsThermal->GetThermal();
    LW2080_CTRL_FAN_ARBITER_STATUS_PARAMS fanArbiterStatusParams = {};
    C_CHECK_RC(pThermal->GetFanArbiterStatus(&fanArbiterStatusParams));

    for (UINT32 index = 0; index < LW2080_CTRL_FAN_ARBITER_MAX_ARBITERS; index++)
    {
        if (BIT(index) & arbiterMask)
        {
            LW2080_CTRL_FAN_ARBITER_STATUS fanArbiterStatus = fanArbiterStatusParams.arbiters[index];

            JSObject* pJsFanArbiterStatus = JS_NewObject(pContext, &FanArbiterStatusClass, NULL, NULL);
            C_CHECK_RC(pJavaScript->PackFields(pJsFanArbiterStatus, "IIII",
                "fanCtrlAction",    fanArbiterStatus.fanCtrlAction,
                "drivingPolicyIdx", fanArbiterStatus.drivingPolicyIdx,
                "targetPwm",        fanArbiterStatus.targetPwm,
                "targetRpm",        fanArbiterStatus.targetRpm));
            C_CHECK_RC(pJavaScript->SetElement(pJsFanArbiterStatusParams, index, pJsFanArbiterStatus));
        }
    }
    RETURN_RC(rc);
}

JS_CLASS(FanArbiterInfo);
JS_STEST_LWSTOM(JsThermal,
                GetFanArbiterInfo,
                1,
                "Get Fan Arbiter Info")
{
    STEST_HEADER(1, 1, "Usage: subdev.Thermal.GetFanArbiterInfo(returnArray)");
    STEST_PRIVATE(JsThermal, pJsThermal, "JsThermal");
    STEST_ARG(0, JSObject *, pJsFanArbiterInfoParams);

    RC rc;
    Thermal *pThermal = pJsThermal->GetThermal();
    LW2080_CTRL_FAN_ARBITER_INFO_PARAMS fanArbiterInfoParams = {};

    C_CHECK_RC(pThermal->GetFanArbiterInfo(&fanArbiterInfoParams));

    UINT32 i = 0;
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &fanArbiterInfoParams.super.objMask.super)
        LW2080_CTRL_FAN_ARBITER_INFO fanArbiterInfo = fanArbiterInfoParams.arbiters[i];

        JSObject* pJsFanArbiterInfo = JS_NewObject(pContext, &FanArbiterInfoClass, NULL, NULL);
        C_CHECK_RC(pJavaScript->PackFields(pJsFanArbiterInfo, "IIIII",
            "mode",                 fanArbiterInfo.mode,
            "coolerIdx",            fanArbiterInfo.coolerIdx,
            "samplingPeriodms",     fanArbiterInfo.samplingPeriodms,
            "fanPoliciesMask",      fanArbiterInfo.fanPoliciesMask.super.pData[0],
            "vbiosFanPoliciesMask", fanArbiterInfo.vbiosFanPoliciesMask.super.pData[0]));
        C_CHECK_RC(pJavaScript->SetElement(pJsFanArbiterInfoParams, i, pJsFanArbiterInfo));
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

    RETURN_RC(rc);
}

JS_SMETHOD_LWSTOM(JsThermal,
                  FanArbiterModeToString,
                  1,
                  "Colwert Fan Arbiter mode Id to string")
{
    STEST_HEADER(1, 1, "Usage: let modeStr = subdev.Thermal.FanArbiterModeToString(mode)");
    STEST_ARG(0, UINT32, mode);

    auto modeStr = Thermal::FanArbiterModeToString(mode);

    RC rc = pJavaScript->ToJsval(modeStr, pReturlwalue);
    if (RC::OK != rc)
    {
        pJavaScript->Throw(pContext, rc, "Error oclwrred in subdev.Thermal.FanArbiterModeToString");
        return JS_FALSE;
    }
    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(JsThermal,
                  FanArbiterCtrlActionToString,
                  1,
                  "Colwert Fan Arbiter fanCtrlAction Id to string")
{
    STEST_HEADER(1, 1, "Usage: let fanCtrlActionStr = subdev.Thermal.FanArbiterCtrlActionToString(fanCtrlAction)");
    STEST_ARG(0, UINT32, fanCtrlAction);

    auto fanCtrlActionStr = Thermal::FanArbiterCtrlActionToString(fanCtrlAction);

    RC rc = pJavaScript->ToJsval(fanCtrlActionStr, pReturlwalue);
    if (RC::OK != rc)
    {
        pJavaScript->Throw(pContext, rc, "Error oclwrred in subdev.Thermal.FanArbiterCtrlActionToString");
        return JS_FALSE;
    }
    return JS_TRUE;
}

JS_STEST_LWSTOM(JsThermal,
                ResetCoolerSettings,
                0,
                "Resets the cooler settings.")
{
    MASSERT(pContext != 0);
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "subdev.Thermal.ResetCoolerSettings()\n");
        return JS_FALSE;
    }

    JsThermal *pJsThermal;
    if ((pJsThermal = JS_GET_PRIVATE(JsThermal, pContext, pObject, "JsThermal")) != 0)
    {
        RC rc;
        Thermal* pThermal = pJsThermal->GetThermal();
        C_CHECK_RC(pThermal->ResetCoolerSettings());
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsThermal,
                ResetThermalTable,
                0,
                "clears thermal table and restart thermal")
{
    MASSERT(pContext     !=0);

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "subdev.Thermal.ResetThermalTable()\n");
        return JS_FALSE;
    }

    JsThermal *pJsThermal;
    if ((pJsThermal = JS_GET_PRIVATE(JsThermal, pContext, pObject, "JsThermal")) != 0)
    {
        RC rc;
        Thermal* pThermal = pJsThermal->GetThermal();
        C_CHECK_RC(pThermal->ResetThermalTable());
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsThermal,
                InitOverTempCounter,
                0,
                "Initialize over temp counter")
{
    MASSERT(pContext     !=0);

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "subdev.Thermal.InitOverTempCounter()\n");
        return JS_FALSE;
    }

    JsThermal *pJsThermal;
    if ((pJsThermal = JS_GET_PRIVATE(JsThermal, pContext, pObject, "JsThermal")) != 0)
    {
        RC rc;
        Thermal* pThermal = pJsThermal->GetThermal();
        C_CHECK_RC(pThermal->InitOverTempCounter());
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsThermal,
                ShutdownOverTempCounter,
                0,
                "Shutdown the over temp counter")
{
    MASSERT(pContext     !=0);

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "subdev.Thermal.ShutdownOverTempCounter()\n");
        return JS_FALSE;
    }

    JsThermal *pJsThermal;
    if ((pJsThermal = JS_GET_PRIVATE(JsThermal, pContext, pObject, "JsThermal")) != 0)
    {
        RC rc;
        Thermal* pThermal = pJsThermal->GetThermal();
        C_CHECK_RC(pThermal->ShutdownOverTempCounter());
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsThermal,
                WriteThermalTable,
                2,
                "write an array into the thermal table")
{
    STEST_HEADER(2, 2, "Usage: Thermal.WriteTable(ver, array)");
    STEST_PRIVATE(JsThermal, pJsThermal, "JsThermal");
    STEST_ARG(0, UINT32, ver);
    STEST_ARG(1, JsArray, newTable);
    RC rc;

    vector<UINT32> TableEntries(newTable.size());
    for (UINT32 i = 0; i < newTable.size(); i++)
    {
        UINT32 tmp;
        rc = pJavaScript->FromJsval(newTable[i], &tmp);
        if (OK != rc)
        {
            return JS_FALSE;
        }
        TableEntries[i] = (LwU32)tmp;
    }

    Thermal* pThermal = pJsThermal->GetThermal();
    C_CHECK_RC(pThermal->WriteThermalTable(&TableEntries[0],
                                           static_cast<UINT32>(newTable.size()),
                                           ver));
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsThermal,
                ReadThermalTable,
                2,
                "read an array from the thermal table")
{
    STEST_HEADER(2, 2, "Usage: Thermal.ReadTable(which, returnArray)");
    STEST_PRIVATE(JsThermal, pJsThermal, "JsThermal");
    STEST_ARG(0, UINT32, whichTable);
    STEST_ARG(1, JSObject*, pReturlwals);

    vector<UINT32> pTable(LW2080_CTRL_THERMAL_TABLE_MAX_ENTRIES);
    UINT32 Size, Flags, Version;
    RC rc;

    Thermal* pThermal = pJsThermal->GetThermal();
    C_CHECK_RC(pThermal->ReadThermalTable(whichTable, &pTable[0], &Size,
                                          &Flags, &Version));
    C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 0, Flags ));
    C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 1, Version ));

    JsArray innerArray;
    UINT32 i;
    for (i = 0; i < Size; i++)
    {
        jsval tmp;
        C_CHECK_RC(pJavaScript->ToJsval(pTable[i], &tmp));
        innerArray.push_back(tmp);
    }
    C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 2, &innerArray));

    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsThermal,
                GetPhaseLwrrent,
                2,
                "Get power phase current in mA")
{
    STEST_HEADER(2, 2, "Usage: Gpu.GetPhaseLwrrent(phase_idx, lwrrent_mA)");
    STEST_PRIVATE(JsThermal, pJsThermal, "JsThermal");
    STEST_ARG(0, UINT32, phaseIdx);
    STEST_ARG(1, JSObject*, pReturlwals);

    INT32 Current = 0;
    RC rc;

    Thermal* pThermal = pJsThermal->GetThermal();
    C_CHECK_RC(pThermal->GetPhaseLwrrent(phaseIdx, &Current));
    RETURN_RC(pJavaScript->SetElement(pReturlwals, 0, Current));
}

JS_STEST_LWSTOM(JsThermal,
                GetLwrrThermalSlowdownRatio,
                2,
                "Get current thermal slowdown ratio")
{
    STEST_HEADER(2, 2, "Usage: Gpu.GetThermalSlowdownRatio(clkDomain, ratio)");
    STEST_PRIVATE(JsThermal, pJsThermal, "JsThermal");
    STEST_ARG(0, UINT32, clkDomain);
    STEST_ARG(1, JSObject*, pReturlwals);

    RC rc;
    FLOAT32 ratio = 0;
    Thermal* pThermal = pJsThermal->GetThermal();
    C_CHECK_RC(pThermal->GetThermalSlowdownRatio(
        static_cast<Gpu::ClkDomain>(clkDomain), &ratio));
    RETURN_RC(pJavaScript->SetElement(pReturlwals, 0, ratio));
}

JS_STEST_LWSTOM(JsThermal,
                SetOverTempRange,
                3,
                "SetOverTempRange")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Thermal.SetOverTempRange(Type, Min, Max)";

    JavaScriptPtr pJavaScript;
    INT32 MinTemp;
    INT32 MaxTemp;
    UINT32 Type;
    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Type)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &MinTemp)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &MaxTemp)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsThermal *pJsThermal;
    if ((pJsThermal = JS_GET_PRIVATE(JsThermal, pContext, pObject, "JsThermal")) != 0)
    {
        Thermal *pThermal = pJsThermal->GetThermal();
        RETURN_RC(pThermal->SetOverTempRange((OverTempCounter::TempType)Type,
                                             MinTemp, MaxTemp));
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsThermal,
                SetTempSim,
                2,
                "Sets temperature simulation")
{
    MASSERT(pContext && pArguments);

    const char usage[] = "Usage: subdev.Thermal.SetTempSim(SensorId, Temperature)";

    JavaScriptPtr pJS;
    UINT32 SensorId;
    INT32  Temp;
    if ((NumArguments != 2) ||
        (OK != pJS->FromJsval(pArguments[0], &SensorId)) ||
        (OK != pJS->FromJsval(pArguments[1], &Temp)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsThermal *pJsThermal;
    if ((pJsThermal = JS_GET_PRIVATE(JsThermal, pContext, pObject, "JsThermal")) != 0)
    {
        Thermal *pThermal = pJsThermal->GetThermal();
        RETURN_RC(pThermal->SetSimulatedTemperature(SensorId, Temp));
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsThermal,
                ClearTempSim,
                1,
                "Clears temperature simulation")
{
    MASSERT(pContext && pArguments);

    const char usage[] = "Usage: subdev.Thermal.ClearTempSim(SensorId)";

    JavaScriptPtr pJS;
    UINT32 SensorId;
    if ((NumArguments != 1) || (OK != pJS->FromJsval(pArguments[0], &SensorId)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    JsThermal *pJsThermal;
    if ((pJsThermal = JS_GET_PRIVATE(JsThermal, pContext, pObject, "JsThermal")) != 0)
    {
        Thermal *pThermal = pJsThermal->GetThermal();
        RETURN_RC(pThermal->ClearSimulatedTemperature(SensorId));
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsThermal,
                GetVolterraSlaveTemps,
                1,
                "GetVolterraSlaveTemp")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Thermal.GetVolterraSlaveTemp(TempArray)";

    JavaScriptPtr pJavaScript;
    JSObject *pSlaveTemps = 0;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &pSlaveTemps)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsThermal *pJsThermal;

    if ((pJsThermal = JS_GET_PRIVATE(JsThermal, pContext, pObject, "JsThermal")) != 0)
    {
        Thermal *pThermal = pJsThermal->GetThermal();
        UINT32   slaveTemp;
        set<UINT32> volterraSlaves;
        set<UINT32>::iterator pSlaveId;
        RC                    rc;

        C_CHECK_RC(pThermal->GetVolterraSlaves(&volterraSlaves));
        for (pSlaveId = volterraSlaves.begin(); pSlaveId != volterraSlaves.end(); pSlaveId++)
        {
            C_CHECK_RC(pThermal->GetVolterraSlaveTemp(*pSlaveId, &slaveTemp));
            C_CHECK_RC(pJavaScript->SetElement(pSlaveTemps, *pSlaveId, slaveTemp));
        }
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsThermal,
                SetADT7461Config,
                4,
                "SetADT7461Config")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: subdev.Thermal.SetADT7461Config(Config, LocLimit, ExtLimit, Hysteresis)";

    JavaScriptPtr pJavaScript;
    UINT32 Config, LocLimit, ExtLimit, Hysteresis;
    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Config)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &LocLimit)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &ExtLimit)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &Hysteresis)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsThermal *pJsThermal;

    if ((pJsThermal = JS_GET_PRIVATE(JsThermal, pContext, pObject, "JsThermal")) != 0)
    {
        Thermal *pThermal = pJsThermal->GetThermal();
        RC                    rc;

        C_CHECK_RC(pThermal->SetADT7461Config(Config, LocLimit, ExtLimit, Hysteresis));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsThermal,
                BeginExpectingErrors,
                1,
                "Begin expecting temperature errors for sensor x")
{
    MASSERT(pContext && pArguments);

    const char usage[] = "Usage: subdev.Thermal.BeginExpectingErrors(SensorId)";

    JavaScriptPtr pJS;
    OverTempCounter::TempType SensorId;
    if ((NumArguments != 1) || (OK != pJS->FromJsval(pArguments[0], (UINT32*)&SensorId)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    JsThermal *pJsThermal;
    if ((pJsThermal = JS_GET_PRIVATE(JsThermal, pContext, pObject, "JsThermal")) != 0)
    {
        Thermal *pThermal = pJsThermal->GetThermal();
        pThermal->BeginExpectingErrors(SensorId);
        RETURN_RC(OK);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsThermal,
                EndExpectingErrors,
                1,
                "End expecting temperature errors for sensor x")
{
    MASSERT(pContext && pArguments);

    const char usage[] = "Usage: subdev.Thermal.EndExpectingErrors(SensorId)";

    JavaScriptPtr pJS;
    OverTempCounter::TempType SensorId;
    if ((NumArguments != 1) || (OK != pJS->FromJsval(pArguments[0], (UINT32*)&SensorId)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    JsThermal *pJsThermal;
    if ((pJsThermal = JS_GET_PRIVATE(JsThermal, pContext, pObject, "JsThermal")) != 0)
    {
        Thermal *pThermal = pJsThermal->GetThermal();
        pThermal->EndExpectingErrors(SensorId);
        RETURN_RC(OK);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsThermal,
                ReadExpectedErrorCount,
                1,
                "read an array of expected error counts from the thermal table")
{
    JavaScriptPtr pJavaScript;
    RC rc;

    JSObject * pReturlwals;

    // Check the arguments.
    if( ( NumArguments != 1) ||
        ( OK != pJavaScript->FromJsval(pArguments[0], &pReturlwals)) )
    {
       JS_ReportError(pContext,
                      "Usage: Thermal.ReadExpectedErrorCount(returnArray)");
       return JS_FALSE;
    }

    vector<UINT64> ErrTable(OverTempCounter::NUM_TEMPS);

    JsThermal *pJsThermal;
    if ((pJsThermal = JS_GET_PRIVATE(JsThermal, pContext, pObject, "JsThermal")) != 0)
    {
        Thermal* pThermal = pJsThermal->GetThermal();
        C_CHECK_RC(pThermal->ReadExpectedErrorCount(&ErrTable[0]));

        UINT32 i;
        for (i = 0; i < ErrTable.size(); i++)
        {
            C_CHECK_RC(pJavaScript->SetElement(pReturlwals, i, ErrTable[i]));
        }

        RETURN_RC(rc);
    }
    return JS_FALSE;
}

RC JsThermal::PrintSensors(UINT32 pri)
{
    return m_pThermal->GetThermalSensors()->PrintSensors(static_cast<Tee::Priority>(pri));
}
JS_STEST_BIND_ONE_ARG(JsThermal, PrintSensors, pri, UINT32, "Print thermal sensors info");

JS_STEST_LWSTOM(JsThermal,
                GetProviderStringForGpuAvg,
                1,
                "GetProviderStringForGpuAvg")
{
    RC rc;
    STEST_HEADER(1, 1, "Usage: Thermal.ProviderIndexToString([])");
    STEST_PRIVATE(JsThermal, pJsThermal, "JsThermal");
    STEST_ARG(0, JSObject*, pJsString);

    string chName = TSM20Sensors::ProviderIndexToString(LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU,
        LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_AVG);
    JavaScriptPtr pJs;
    CHECK_RC(pJs->SetProperty(pJsString, "chName", chName));
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsThermal,
                GetProviderStringForGpuOffsetMax,
                1,
                "GetProviderStringForGpuOffsetMax")
{
    RC rc;
    STEST_HEADER(1, 1, "Usage: Thermal.ProviderIndexToString([])");
    STEST_PRIVATE(JsThermal, pJsThermal, "JsThermal");
    STEST_ARG(0, JSObject*, pJsString);

    string chName = TSM20Sensors::ProviderIndexToString(LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU,
        LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_OFFSET_MAX);
    JavaScriptPtr pJs;
    CHECK_RC(pJs->SetProperty(pJsString, "chName", chName));
    RETURN_RC(rc);
}

JS_CLASS(ChannelTemp);
JS_STEST_LWSTOM(JsThermal,
                GetChipTempsViaChannels,
                1,
                "Get chip temperatures via thermal channels")
{
    STEST_HEADER(1, 1, "Usage: Thermal.GetChipTempsViaChannels([temps])");
    STEST_PRIVATE(JsThermal, pJsThermal, "JsThermal");
    STEST_ARG(0, JSObject*, pReturlwals);

    RC rc;
    vector<ThermalSensors::ChannelTempData> channelTemps;
    Thermal* pThermal = pJsThermal->GetThermal();
    C_CHECK_RC(pThermal->GetChipTempsViaChannels(&channelTemps));

    JavaScriptPtr pJs;
    UINT32 ii = 0;
    for (const auto& lwrrTemp : channelTemps)
    {
        JSObject *pJsObjChannelTemp = JS_NewObject(pContext, &ChannelTempClass,
                                                   nullptr, nullptr);
        CHECK_RC(pJs->SetProperty(pJsObjChannelTemp, "chIdx",
                                  static_cast<UINT32>(lwrrTemp.chIdx)));
        CHECK_RC(pJs->SetProperty(pJsObjChannelTemp, "temp",
                                  static_cast<FLOAT32>(lwrrTemp.temp)));
        CHECK_RC(pJs->SetProperty(pJsObjChannelTemp, "devIdx",
                                  static_cast<UINT32>(lwrrTemp.devIdx)));
        CHECK_RC(pJs->SetProperty(pJsObjChannelTemp, "devClass",
                                  static_cast<UINT32>(lwrrTemp.devClass)));
        CHECK_RC(pJs->SetProperty(pJsObjChannelTemp, "provIdx",
                                  static_cast<UINT32>(lwrrTemp.provIdx)));

        string chName = TSM20Sensors::ProviderIndexToString(lwrrTemp.devClass, lwrrTemp.provIdx);
        if (lwrrTemp.devClass == LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_GPC_TSOSC)
        {
            chName += Utility::StrPrintf("_%u", lwrrTemp.tsoscIdx);
        }
        else if (lwrrTemp.devClass == LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_HBM2_SITE)
        {
            chName += Utility::StrPrintf("_%u", lwrrTemp.siteIdx);
        }
        CHECK_RC(pJs->SetProperty(pJsObjChannelTemp, "chName", chName));


        switch (lwrrTemp.devClass)
        {
            case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_ADM1032:
            case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_MAX6649:
            case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_TMP411:
            case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_ADT7461:
            case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_TMP451:
                CHECK_RC(pJs->SetProperty(pJsObjChannelTemp, "i2cDevIdx",
                                          static_cast<UINT32>(lwrrTemp.i2cDevIdx)));
                break;
            case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_GPC_TSOSC:
                CHECK_RC(pJs->SetProperty(pJsObjChannelTemp, "tsoscIdx",
                                          static_cast<UINT32>(lwrrTemp.tsoscIdx)));
                break;
            case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_HBM2_SITE:
                CHECK_RC(pJs->SetProperty(pJsObjChannelTemp, "siteIdx",
                                          static_cast<UINT32>(lwrrTemp.siteIdx)));
                break;
            default:
                // No class specific data exists
                break;
        }

        jsval channelTempJsVal;
        C_CHECK_RC(pJavaScript->ToJsval(pJsObjChannelTemp, &channelTempJsVal));
        C_CHECK_RC(pJavaScript->SetElement(pReturlwals, ii++, channelTempJsVal));
    }
    RETURN_RC(rc);
}

JS_CLASS(ThermalChannelInfo);
JS_STEST_LWSTOM(JsThermal,
                GetChannelInfos,
                1,
                "Returns an array of thermal channel information objects")
{
    STEST_HEADER(1, 1, "Usage: GpuSub.Thermal.GetChannelInfos(outArray)");
    STEST_PRIVATE(JsThermal, pJsThermal, "JsThermal");
    STEST_ARG(0, JSObject*, pReturlwals);

    RC rc;

    ThermalSensors::ChannelInfos chInfos;
    Thermal* pThermal = pJsThermal->GetThermal();
    C_CHECK_RC(pThermal->GetThermalSensors()->GetChannelInfos(&chInfos));

    JavaScriptPtr pJs;
    UINT32 ii = 0;
    for (const auto& chInfo : chInfos)
    {
        JSObject* pJsThermChInfo = JS_NewObject(
            pContext, &ThermalChannelInfoClass, nullptr, nullptr);
        C_CHECK_RC(JsThermal::ChannelInfoToJsObj(chInfo.first, chInfo.second, pJsThermChInfo));
        jsval tmp;
        C_CHECK_RC(pJs->ToJsval(pJsThermChInfo, &tmp));
        C_CHECK_RC(pJs->SetElement(pReturlwals, ii++, tmp));
    }

    RETURN_RC(rc);
}

static FLOAT32 FromLwTemp(LwTemp temp)
{
    return static_cast<FLOAT32>(temp / 256.0);
}

RC JsThermal::ChannelInfoToJsObj
(
    UINT32 chIdx,
    const ThermalSensors::ChannelInfo& chInfo,
    JSObject* pJsObj
)
{
    RC rc;

    JavaScriptPtr pJs;

    CHECK_RC(pJs->SetProperty(pJsObj, "Index", chIdx));
    CHECK_RC(pJs->SetProperty(pJsObj, "Name", chInfo.Name));
    CHECK_RC(pJs->SetProperty(pJsObj, "IsFloorswept", chInfo.IsFloorswept));
    CHECK_RC(pJs->SetProperty(pJsObj, "Type", chInfo.RmInfo.type));
    CHECK_RC(pJs->SetProperty(pJsObj, "ChType", chInfo.RmInfo.chType));
    CHECK_RC(pJs->SetProperty(pJsObj, "RelLoc", chInfo.RmInfo.relLoc));
    CHECK_RC(pJs->SetProperty(pJsObj, "TgtGPU", chInfo.RmInfo.tgtGPU));
    CHECK_RC(pJs->SetProperty(pJsObj, "MinTemp", FromLwTemp(chInfo.RmInfo.minTemp)));
    CHECK_RC(pJs->SetProperty(pJsObj, "MaxTemp", FromLwTemp(chInfo.RmInfo.maxTemp)));
    CHECK_RC(pJs->SetProperty(pJsObj, "Scaling", chInfo.RmInfo.scaling));
    CHECK_RC(pJs->SetProperty(pJsObj, "OffsetSw", FromLwTemp(chInfo.RmInfo.offsetSw)));
    CHECK_RC(pJs->SetProperty(pJsObj, "OffsetHw", FromLwTemp(chInfo.RmInfo.offsetHw)));
    CHECK_RC(pJs->SetProperty(pJsObj, "IsTempSimSupported", chInfo.RmInfo.bIsTempSimSupported));
    CHECK_RC(pJs->SetProperty(pJsObj, "Flags", chInfo.RmInfo.flags));
    CHECK_RC(pJs->SetProperty(pJsObj, "ThermDevIdx", chInfo.RmInfo.data.device.thermDevIdx));
    CHECK_RC(pJs->SetProperty(pJsObj, "ThermDevProvIdx", chInfo.RmInfo.data.device.thermDevProvIdx)); //$

    return rc;
}

JS_STEST_LWSTOM(JsThermal,
                GetCapRange,
                2,
                "Returns the min, rated, and max limits in degC for a thermal policy")
{
    STEST_HEADER(2, 2, "Usage: GpuSub.Thermal.GetCapRange(cap, retObj);");
    STEST_PRIVATE(JsThermal, pJsThermal, "JsThermal");
    STEST_ARG(0, UINT32, thermPolIdx);
    STEST_ARG(1, JSObject*, pRetObj);

    RC rc = RC::OK;
    Thermal* pTherm = pJsThermal->GetThermal();
    FLOAT32 min, rated, max;
    C_CHECK_RC(pTherm->GetCapRange(static_cast<Thermal::ThermalCapIdx>(thermPolIdx), 
                                   &min, &rated, &max));
    C_CHECK_RC(pJavaScript->PackFields(pRetObj, "fff",
                                       "Min", min,
                                       "Rated", rated,
                                       "Max", max));

    RETURN_RC(rc);
}

JS_STEST_LWSTOM(JsThermal,
                GetHwfsEventInfo,
                2,
                "Returns information about a HWFS event")
{
    STEST_HEADER(2, 2, "Usage: GpuSub.Thermal.GetHwfsEventInfo(hwfsEvent, retObj);");
    STEST_PRIVATE(JsThermal, pJsThermal, "JsThermal");
    STEST_ARG(0, UINT32, eventId);
    STEST_ARG(1, JSObject*, pRetObj);
    RC rc;
    Thermal* pTherm = pJsThermal->GetThermal();
    LW2080_CTRL_THERMAL_HWFS_EVENT_SETTINGS_PARAMS params = {};
    C_CHECK_RC(pTherm->GetHwfsEventInfo(&params, eventId));
    C_CHECK_RC(pJavaScript->PackFields(pRetObj, "fBII",
                                       "Temperature", FromLwTemp(params.temperature),
                                       "SensorId", params.sensorId,
                                       "SlowdownNum", params.slowdown.num,
                                       "SlowdownDenom", params.slowdown.denom));
    RETURN_RC(rc);
}

// Deprecated APIs
// ------------------------------------------------------------------------------------------------

#define DEPR_ALIAS_STEST(deprDate, className, obj, func, args, desc, alias, params)            \
JS_STEST_LWSTOM(className, func, args, desc)                                                   \
{                                                                                              \
    static Deprecation depr(obj "." #func, deprDate,                                           \
                            "Use " obj "." #alias "(" params ") instead\n");                   \
    if (!depr.IsAllowed(pContext))                                                             \
        return JS_FALSE;                                                                       \
                                                                                               \
    STEST_HEADER(args, args, "Usage: " obj "." #func "(" params ")");                          \
    return C_##className##_##alias(pContext, pObject, NumArguments, pArguments, pReturlwalue); \
}

DEPR_ALIAS_STEST("5/22/2020", JsThermal, "subdev.Thermal", GetFan20Object,       1, "Get Fan 2.0 object",        GetFanObject,       "returnObject");
DEPR_ALIAS_STEST("5/22/2020", JsThermal, "subdev.Thermal", GetFan20PolicyTable,  3, "Get Fan 2.0 Policy table",  GetFanPolicyTable,  "policyMask, default, returnArray");
DEPR_ALIAS_STEST("5/22/2020", JsThermal, "subdev.Thermal", SetFan20PolicyTable,  2, "Set Fan 2.0 Policy Table",  SetFanPolicyTable,  "policyMask, returnArray");
DEPR_ALIAS_STEST("5/22/2020", JsThermal, "subdev.Thermal", GetFan20CoolerTable,  3, "Get Fan 2.0 Cooler table",  GetFanCoolerTable,  "coolerMask, default, returnArray");
DEPR_ALIAS_STEST("5/22/2020", JsThermal, "subdev.Thermal", SetFan20CoolerTable,  3, "Set Fan 2.0 Cooler table",  SetFanCoolerTable,  "coolerMask, returnArray");
DEPR_ALIAS_STEST("5/22/2020", JsThermal, "subdev.Thermal", GetFan20TestParams,   1, "Get Fan test entries",      GetFanTestParams,   "returnArray");
DEPR_ALIAS_STEST("5/22/2020", JsThermal, "subdev.Thermal", DumpFan20PolicyTable, 1, "Dump Fan 2.0 Policy table", DumpFanPolicyTable, "defaultValues");
DEPR_ALIAS_STEST("5/22/2020", JsThermal, "subdev.Thermal", DumpFan20CoolerTable, 1, "Dump Fan 2.0 Cooler table", DumpFanCoolerTable, "defaultValues");

//! Create a JS class to declare Therm constants on
//!

JS_CLASS(ThermConst);

static SObject ThermConst_Object
(
    "ThermConst",
    ThermConstClass,
    0,
    0,
    "ThermConst JS Object"
);

PROP_CONST(ThermConst, FAN_SIGNAL_NONE, Thermal::FAN_SIGNAL_NONE);
PROP_CONST(ThermConst, FAN_SIGNAL_TOGGLE, Thermal::FAN_SIGNAL_TOGGLE);
PROP_CONST(ThermConst, FAN_SIGNAL_VARIABLE, Thermal::FAN_SIGNAL_VARIABLE);

PROP_CONST(ThermConst, OTC_INTERNAL_TEMP, OverTempCounter::INTERNAL_TEMP);
PROP_CONST(ThermConst, OTC_EXTERNAL_TEMP, OverTempCounter::EXTERNAL_TEMP);
PROP_CONST(ThermConst, OTC_RM_EVENT, OverTempCounter::RM_EVENT);
PROP_CONST(ThermConst, OTC_VOLTERRA_TEMP_1, OverTempCounter::VOLTERRA_TEMP_1);
PROP_CONST(ThermConst, OTC_VOLTERRA_TEMP_2, OverTempCounter::VOLTERRA_TEMP_2);
PROP_CONST(ThermConst, OTC_VOLTERRA_TEMP_3, OverTempCounter::VOLTERRA_TEMP_3);
PROP_CONST(ThermConst, OTC_VOLTERRA_TEMP_4, OverTempCounter::VOLTERRA_TEMP_4);
PROP_CONST(ThermConst, OTC_VOLTERRA_TEMP_LAST, OverTempCounter::VOLTERRA_TEMP_LAST);
PROP_CONST(ThermConst, OTC_SMBUS_TEMP, OverTempCounter::SMBUS_TEMP);
PROP_CONST(ThermConst, OTC_IPMI_TEMP, OverTempCounter::IPMI_TEMP);
PROP_CONST(ThermConst, OTC_MEMORY_TEMP, OverTempCounter::MEMORY_TEMP);
PROP_CONST(ThermConst, OTC_TSENSE_OFFSET_MAX_TEMP, OverTempCounter::TSENSE_OFFSET_MAX_TEMP);
PROP_CONST(ThermConst, OTC_NUM_TEMPS, OverTempCounter::NUM_TEMPS);

PROP_CONST(ThermConst, VbiosTable,    LW2080_CTRL_THERMAL_GET_THERMAL_TABLE_FLAGS_VBIOS);
PROP_CONST(ThermConst, LwrrentTable,  LW2080_CTRL_THERMAL_GET_THERMAL_TABLE_FLAGS_LWRRENT);
PROP_CONST(ThermConst, RegistryTable, LW2080_CTRL_THERMAL_GET_THERMAL_TABLE_FLAGS_REGISTRY);
PROP_CONST(ThermConst, CoolerPolicyNone,           Thermal::CoolerPolicyNone);
PROP_CONST(ThermConst, CoolerPolicyManual,         Thermal::CoolerPolicyManual);
PROP_CONST(ThermConst, CoolerPolicyPerf,           Thermal::CoolerPolicyPerf);
PROP_CONST(ThermConst, CoolerPolicyDiscrete,       Thermal::CoolerPolicyDiscrete);
PROP_CONST(ThermConst, CoolerPolicyContinuous,     Thermal::CoolerPolicyContinuous);
PROP_CONST(ThermConst, CoolerPolicyContinuousSw,   Thermal::CoolerPolicyContinuousSw);
PROP_CONST(ThermConst, INVALID,                    JSTHERMAL_ILWALID_VALUE);

PROP_CONST(ThermConst, INTERNAL_TEMP, OverTempCounter::INTERNAL_TEMP);
PROP_CONST(ThermConst, MEMORY, Thermal::MEMORY);
PROP_CONST(ThermConst, SW_SLOWDOWN, Thermal::SW_SLOWDOWN);

// Thermal device classes
PROP_CONST(ThermConst, DEVICE_CLASS_GPU_GPC_TSOSC, LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_GPC_TSOSC);
PROP_CONST(ThermConst, DEVICE_CLASS_HBM2_SITE,     LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_HBM2_SITE);

// HW FS event types
PROP_CONST(ThermConst, HWFS_EXT_OVERT,       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EXT_OVERT);
PROP_CONST(ThermConst, HWFS_EXT_ALERT,       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EXT_ALERT);
PROP_CONST(ThermConst, HWFS_EXT_POWER,       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EXT_POWER);
PROP_CONST(ThermConst, HWFS_OVERT,           LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_OVERT);
PROP_CONST(ThermConst, HWFS_ALERT_0H,        LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_0H);
PROP_CONST(ThermConst, HWFS_ALERT_1H,        LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_1H);
PROP_CONST(ThermConst, HWFS_ALERT_2H,        LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_2H);
PROP_CONST(ThermConst, HWFS_ALERT_3H,        LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_3H);
PROP_CONST(ThermConst, HWFS_ALERT_4H,        LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_4H);
PROP_CONST(ThermConst, HWFS_ALERT_NEG1H,     LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_NEG1H);
PROP_CONST(ThermConst, HWFS_THERMAL_0,       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_0);
PROP_CONST(ThermConst, HWFS_THERMAL_1,       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_1);
PROP_CONST(ThermConst, HWFS_THERMAL_2,       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_2);
PROP_CONST(ThermConst, HWFS_THERMAL_3,       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_3);
PROP_CONST(ThermConst, HWFS_THERMAL_4,       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_4);
PROP_CONST(ThermConst, HWFS_THERMAL_5,       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_5);
PROP_CONST(ThermConst, HWFS_THERMAL_6,       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_6);
PROP_CONST(ThermConst, HWFS_THERMAL_7,       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_7);
PROP_CONST(ThermConst, HWFS_THERMAL_8,       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_8);
PROP_CONST(ThermConst, HWFS_THERMAL_9,       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_9);
PROP_CONST(ThermConst, HWFS_THERMAL_10,      LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_10);
PROP_CONST(ThermConst, HWFS_THERMAL_11,      LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_11);
PROP_CONST(ThermConst, HWFS_DEDICATED_OVERT, LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_DEDICATED_OVERT);
PROP_CONST(ThermConst, HWFS_SCI_FS_OVERT,    LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_SCI_FS_OVERT);
PROP_CONST(ThermConst, HWFS_EXT_ALERT_0,     LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EXT_ALERT_0);
PROP_CONST(ThermConst, HWFS_EXT_ALERT_1,     LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EXT_ALERT_1);
PROP_CONST(ThermConst, VOLTAGE_HW_ADC,       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_VOLTAGE_HW_ADC);
PROP_CONST(ThermConst, EDPP_VMIN,            LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EDPP_VMIN);
PROP_CONST(ThermConst, EDPP_FONLY,           LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EDPP_FONLY);
PROP_CONST(ThermConst, BA_BA_W2_T1H,         LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_BA_BA_W2_T1H);
