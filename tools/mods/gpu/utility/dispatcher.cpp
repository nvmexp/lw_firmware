/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "dispatcher.h"

#include "core/include/cirlwlarbuffer.h"
#include "core/include/jscript.h"
#include "core/include/jsonlog.h"
#include "core/include/mgrmgr.h"
#include "core/include/xp.h"
#include "core/include/utility.h"
#include "core/tests/modstest.h"
#include "device/cpu/utility/cpusensors.h"
#include "device/interface/pcie.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/clockthrottle.h"
#include "gpu/perf/perfsub.h"
#include "gpu/perf/perfutil.h"
#include "gpu/perf/thermsub.h"
#include "gpu/perf/voltsub.h"
#include <algorithm>
#include <functional>

UINT32 LwpActor::s_SequenceNum = 0;
// Arbitrary value to denote end of thermal event list
/* static */ const UINT32 ThermalLimitActor::s_SlowdownPwmEventId = 0xFFFFFFFE;

//-----------------------------------------------------------------------------
LwpActor::LwpActor(ActorTypes Type, Tee::Priority PrintPri) :
    m_PrintPri(PrintPri),
    m_IsThreadSafe(false),
    m_EnableJson(true),
    m_UseMsAsTime(false),
    m_Seed(0x12345678),
    m_NumSteps(10),
    m_TimeUsRemain(0),
    m_ActionIdx(0),
    m_PrevActionIdx(0),
    m_Type(Type),
    m_Verbose(false),
    m_pJsonItem(NULL),
    m_CirBufferSize(512)
{
}

LwpActor::~LwpActor()
{
    // don't destroy JsonItem here
    // The JavaScript garbage collector will call C_JsonItem_finalize when the
    // test's JSObject is freed, which will indirectly call the C++ destructor
    // for the JsonItem.
}

RC LwpActor::Cleanup()
{
    return OK;
}

void LwpActor::Reset()
{
    Printf(Tee::PriLow, "%s: Reset\n", GetActorName().c_str());
    m_ActionIdx = 0;
    m_PrevActionIdx = 0;
    m_TimeUsRemain = m_Actions[0].TimeUsToNext;
}

//* This is called by the LwpDispatcher. It'll execute the next action
RC LwpActor::ExelwteIfTime
(
    INT64 TimeUsSinceLastSleep,
    INT64 TimeUsSinceLastChange,
    bool *pExelwted
)
{
    RC rc;
    m_TimeUsRemain -= TimeUsSinceLastSleep;
    if (m_TimeUsRemain > 0)
    {
        return OK;
    }

    const UINT32 actiolwal = static_cast<UINT32>(GetActiolwalById(m_ActionIdx));
    if (m_pJsonItem)
    {
        m_pJsonItem->RemoveAllFields();
        m_pJsonItem->SetTag("LwDispatcher_Action");
        m_pJsonItem->SetField("ActorName", m_ActorName.c_str());
        m_pJsonItem->SetField("Actiolwal", actiolwal);
        m_pJsonItem->SetField("ActionId", m_ActionIdx);
        m_pJsonItem->SetField("SequenceNum", s_SequenceNum);
        m_pJsonItem->SetField("TimeUsToNext", m_Actions[m_ActionIdx].TimeUsToNext);
        m_pJsonItem->SetField("TimeUsSinceLastChange", TimeUsSinceLastChange);
        CHECK_RC(m_pJsonItem->WriteToLogfile());
    }
    m_Log.InsertEntry(this, actiolwal, m_ActionIdx);
    s_SequenceNum++;
    *pExelwted = true;

    // time to run something:
    rc = RunIndex(m_ActionIdx);
    return rc;
}

void LwpActor::AdvanceToNextAction()
{
    return AdvanceToNextAction(false);
}

void LwpActor::AdvanceToNextAction(bool IsSkipped)
{
    if (!IsSkipped)
        m_PrevActionIdx = m_ActionIdx;
    // advance the index and get next m_TimeUsRemain
    m_ActionIdx    = (m_ActionIdx + 1) % m_Actions.size();
    m_TimeUsRemain = m_Actions[m_ActionIdx].TimeUsToNext;
}
//-----------------------------------------------------------------------------
// LwpActor parameter format:
// 1) Fancy Picker for the set of values for each action
// 2) Fancy Picker for the timesMs between each action
// 3) string Params: "NumSteps:9|Seed:0x1234431"
RC LwpActor::ParseParams(const vector<jsval>& jsPickers, const string& Params)
{
    RC rc;
    // parse all the non fancy picker parameters
    CHECK_RC(ParseStrParams(Params));

    if (jsPickers.size() < 2)
    {
        Printf(Tee::PriError, "need fancy picker to specify the values & times\n");
        return RC::BAD_PARAMETER;
    }

    CHECK_RC(m_ValPicker.FromJsval(jsPickers[0]));
    CHECK_RC(m_TimePicker.FromJsval(jsPickers[1]));

    // initialize the action array: maybe pull this into Action class
    FancyPicker::FpContext Context;
    Context.LoopNum    = 0;
    Context.RestartNum = 0;
    Context.pJSObj     = 0;
    Context.Rand.SeedRandom(m_Seed);
    m_ValPicker.SetContext(&Context);
    m_TimePicker.SetContext(&Context);

    Printf(m_PrintPri, " %s Actions:\n", m_ActorName.c_str());
    // create a list of actions based on fancy pickers
    for (UINT32 i = 0; i < GetNumSteps(); i++)
    {
        Action NewAction;
        NewAction.Actiolwal    = m_ValPicker.Pick();
        NewAction.TimeUsToNext = m_TimePicker.Pick();
        if (m_UseMsAsTime)
        {
            NewAction.TimeUsToNext *= 1000;
        }

        Printf(m_PrintPri, " %02d: Value = %02d, NextTimeout = %dus\n",
               i, NewAction.Actiolwal, NewAction.TimeUsToNext);

        m_Actions.push_back(NewAction);
        Context.LoopNum++;
    }
    return rc;
}

void LwpActor::AddAction(Action action)
{
    m_Actions.push_back(action);
}

//-----------------------------------------------------------------------------
RC LwpActor::ExtraParamFromJs(jsval j)
{
    return OK;
}

//-----------------------------------------------------------------------------
RC LwpActor::CreateJsonItem(JSContext *pCtx, JSObject *pObj)
{
    MASSERT(pCtx && pObj);
    RC rc;
    m_pJsonItem = new JsonItem(pCtx, pObj, m_ActorName.c_str());

    m_pJsonItem->SetTag("LwDispatcher_NewActor");
    m_pJsonItem->SetField("ActorName", m_ActorName.c_str());
    m_pJsonItem->SetField("Type", static_cast<UINT32>(m_Type));
    m_pJsonItem->SetField("NumSteps", m_NumSteps);
    m_Actions.resize(m_NumSteps);
    // this sets the child class's JsonInfo
    AddJsonInfo();
    CHECK_RC(m_pJsonItem->WriteToLogfile());
    return rc;
}
//-----------------------------------------------------------------------------
// Protected
// Format to parse "NumSteps:9|Seed:0x1234431|Name:ActorName"
RC LwpActor::ParseStrParams(string InputStr)
{
    RC rc;
    bool Done = false;
    // break this down to tokens based on '|', then parse each parameter
    while (!Done)
    {
        string LwrrParam;
        size_t PipePos = InputStr.find("|");
        if (PipePos == string::npos)
        {
            Done = true;
            LwrrParam = InputStr;
        }
        else
        {
            LwrrParam = InputStr.substr(0, PipePos);
            InputStr = InputStr.substr(PipePos + 1);
        }

        size_t ColPos = LwrrParam.find(":");
        if (ColPos == string::npos)
        {
            Printf(Tee::PriError, "no value after %s. Bad parameter\n",
                   LwrrParam.c_str());
            return RC::BAD_PARAMETER;
        }

        string Name = LwrrParam.substr(0, ColPos);
        string Value = LwrrParam.substr(ColPos + 1);
        CHECK_RC(SetParameterByName(Name, Value));
    }
    m_Log.SetSize(m_CirBufferSize);
    return rc;
}
//-----------------------------------------------------------------------------
RC LwpActor::SetParameterByName(const string& ParamName, const string& Value)
{
    if (ParamName == "NumSteps")
        m_NumSteps = static_cast<UINT32>(strtoul(Value.c_str(), NULL, 10));
    else if (ParamName == "TimeUnits")
    {
        if (Value == "Ms")
        {
            m_UseMsAsTime = true;
        }
    }
    else if (ParamName == "Name")
        m_ActorName = Value;
    else if (ParamName == "Seed")
        m_Seed = static_cast<UINT32>(strtoul(Value.c_str(), NULL, 10));
    else if (ParamName == "PrintPriority")
        m_PrintPri = static_cast<Tee::Priority>(strtoul(Value.c_str(), NULL, 10));
    else if (ParamName == "CirBufferSize")
        m_CirBufferSize = static_cast<UINT32>(strtoul(Value.c_str(), NULL, 10));
    else if (ParamName == "EnableJson")
        m_EnableJson = (strcmp(Value.c_str(), "true") == 0);
    else
    {
        Printf(Tee::PriError, "unknown parameter: '%s'\n", ParamName.c_str());
        return RC::BAD_PARAMETER;
    }
    return OK;
}
//-----------------------------------------------------------------------------
RC LwpActor::SetActorPropertiesByJsObj(JSObject *pObj)
{
    RC rc;
    JavaScriptPtr pJs;
    CHECK_RC(pJs->GetProperty(pObj, "ActorName", &m_ActorName));
    CHECK_RC(pJs->GetProperty(pObj, "NumSteps", &m_NumSteps));
    return rc;
}
//-----------------------------------------------------------------------------
RC LwpActor::CreateNewActionFromJs(JSObject *pObj, UINT32 ActionId)
{
    if (0 != m_ActionSet.count(ActionId))
    {
        // action already created
        return OK;
    }

    RC rc;
    JavaScriptPtr pJs;
    Action NewAction;
    CHECK_RC(pJs->GetProperty(pObj, "Actiolwal", &NewAction.Actiolwal));
    CHECK_RC(pJs->GetProperty(pObj,
                              "TimeUsSinceLastChange",
                              &NewAction.TimeUsToNext));
    Printf(m_PrintPri,
           " Actor:%s, %02d: Value=%02d, TimeRemainingBefore=%dms\n",
           m_ActorName.c_str(),
           ActionId,
           NewAction.Actiolwal,
           NewAction.TimeUsToNext);

    m_ActionSet.insert(ActionId);
    m_Actions[ActionId] = NewAction;
    return rc;
}
//-----------------------------------------------------------------------------
void LwpActor::DumpLastActions()
{
    m_Log.DumpEntries();
}

//-----------------------------------------------------------------------------
// JsActor: RunIndex exelwtes a JS method
//-----------------------------------------------------------------------------
JsActor::JsActor(JSObject *pObj, Tee::Priority PrintPri)
 :
   LwpActor(JS_ACTOR, PrintPri),
   m_pObj(pObj),
   m_pJsFunc(NULL)
{
}
JsActor::~JsActor()
{
}
//-----------------------------------------------------------------------------
RC JsActor::SetParameterByName(const string& ParamName, const string& Value)
{
    RC rc;
    if (ParamName == "JsFunc")
    {
        m_JsFuncName = Value;
        JavaScriptPtr pJs;
        CHECK_RC(pJs->GetProperty(pJs->GetGlobalObject(),
                                  m_JsFuncName.c_str(), &m_pJsFunc));
    }
    else
        CHECK_RC(LwpActor::SetParameterByName(ParamName, Value));
    return rc;
}

RC JsActor::CheckParams()
{
    if (m_JsFuncName == "")
    {
        Printf(Tee::PriError, "'JsFunc' Needs to be defined\n");
        return RC::BAD_PARAMETER;
    }
    return OK;
}

RC JsActor::RunIndex(UINT32 Idx)
{
    RC rc;
    string      Name = GetActorName();
    const UINT32 actiolwal = static_cast<UINT32>(GetActiolwalById(Idx));
    Printf(Tee::PriLow, "JsActor %s ::RunIndex(%d) - val = %d\n",
           Name.c_str(),
           Idx,
           actiolwal);

    JavaScriptPtr pJs;
    JsArray Args(1);
    // colwert the value to JS Argument to be passed into JS method
    CHECK_RC(pJs->ToJsval(actiolwal, &Args[0]));
    jsval      RetValInJs = JSVAL_NULL;
    UINT32     JSRetVal   = RC::SOFTWARE_ERROR;
    CHECK_RC(pJs->CallMethod(m_pObj, m_pJsFunc, Args, &RetValInJs));

    CHECK_RC(pJs->FromJsval(RetValInJs, &JSRetVal));
    rc = JSRetVal;
    AdvanceToNextAction();
    return rc;
}
//-----------------------------------------------------------------------------
void JsActor::AddJsonInfo()
{
    if (!GetJsonItem())
        return;

    GetJsonItem()->SetField("JsFuncName", m_JsFuncName.c_str());
}
//-----------------------------------------------------------------------------
RC JsActor::SetActorPropertiesByJsObj(JSObject *pObj)
{
    RC rc;
    JavaScriptPtr pJs;
    CHECK_RC(pJs->GetProperty(pObj, "JsFuncName", &m_JsFuncName));
    CHECK_RC(pJs->GetProperty(pJs->GetGlobalObject(),
                              m_JsFuncName.c_str(),
                              &m_pJsFunc));

    CHECK_RC(LwpActor::SetActorPropertiesByJsObj(pObj));
    return rc;
}
//-----------------------------------------------------------------------------
// PState Actor: RunIndex exelwtes a PState switch
//
PStateActor::PStateActor(Tee::Priority PrintPri) :
    LwpActor(PSTATE_ACTOR, PrintPri)
   ,m_pPerf(NULL)
   ,m_PrintRmPerfLimits(false)
   ,m_PrintClockChanges(false)
   ,m_PrintDispClkTransitions(false)
   ,m_SendDramLimitStrict(false)
   ,m_PerfSweep(false)
   ,m_CoveragePct(100)
   ,m_GpuTestNum(-1)
   ,m_OrigCallbackUsage(false)
{
    m_ActorName = "pstate";
}
PStateActor::~PStateActor()
{
}
//-----------------------------------------------------------------------------
void PStateActor::Reset()
{
    Printf(Tee::PriLow, "%s: Reset\n", GetActorName().c_str());

    // Explicitly setting the test number (only 145 uses dispatcher as of now)
    // because dispatcher thread created by parent thread shares its context
    // including test number and since the parent thread is also running a
    // foreground test having its own progress, if not explicitly set, both
    // threads can eventually end up using the same test number and hence
    // corrupting test progress data for each other.
    ErrorMap::SetTest(m_GpuTestNum);

    // Init the total test progress
    ModsTest::PrintProgressInit(m_CoveragePct);

    LwpActor::Reset();
}

RC PStateActor::SetParameterByName(const string& ParamName, const string& Value)
{
    RC rc;
    if (ParamName == "Name")
    {
        if (Value == "PerfSweep" || Value == "LwvddStress")
        {
            m_PerfSweep = true;
        }
    }
    else if (ParamName == "GpuInst")
    {
        UINT32 GpuInst = (UINT32)strtoul(Value.c_str(), NULL, 10);
        GpuDevMgr* pGpuDevMgr = (GpuDevMgr*)(DevMgrMgr::d_GraphDevMgr);
        if (!pGpuDevMgr)
        {
            Printf(Tee::PriError, "Gpu subdevice manager not ready yet.\n");
            return RC::SOFTWARE_ERROR;
        }
        GpuSubdevice *pSubdev = pGpuDevMgr->GetSubdeviceByGpuInst(GpuInst);
        if (!pSubdev)
        {
            Printf(Tee::PriError, "GpuInst %d unknown\n", GpuInst);
            return RC::BAD_PARAMETER;
        }
        m_pPerf = pSubdev->GetPerf();
        MASSERT(m_pPerf);
    }
    else if (ParamName == "PrePStateCallback")
        m_PrePStateCallback = ParamName;
    else if (ParamName == "PostPStateCallback")
        m_PostPStateCallback = ParamName;
    else if (ParamName == "PreCleanupCallback")
        m_PreCleanupCallback = ParamName;
    else if (ParamName == "PrintRmPerfLimits")
        m_PrintRmPerfLimits = (Value == "true");
    else if (ParamName == "PrintClockChanges")
        m_PrintClockChanges = (Value == "true");
    else if (ParamName == "PrintDispClkTransitions")
        m_PrintDispClkTransitions = (Value == "true");
    else if (ParamName == "SendDramLimitStrict")
        m_SendDramLimitStrict = (Value == "true");
    else if (ParamName == "SkipDispClkTransition")
    {
        // Fetch the to/from dispclk values to skip on a transition
        vector<string> tokens;
        CHECK_RC(Utility::Tokenizer(Value, ",", &tokens));
        if (tokens.size() != 2)
        {
            Printf(Tee::PriError,
                   "Invalid no. of parameters in SkipDispClkTransition: \n"
                   "Expected SkipDispClkTransition:<From Clock>, <To Clock>\n"
                   "Found incorrect parameter = %s\n", Value.c_str());
            return RC::BAD_PARAMETER;
        }

        DispClkTransition newSkipTransition;
        newSkipTransition.FromClockValMHz = ((UINT64)strtoul(tokens[0].c_str(), NULL, 10));
        newSkipTransition.ToClockValMHz = ((UINT64)strtoul(tokens[1].c_str(), NULL, 10));
        m_DispClkSwitchSkipList.push_back(newSkipTransition);
    }
    else if (ParamName == "CovPct")
    {
        m_CoveragePct = static_cast<UINT32>(strtoul(Value.c_str(), NULL, 10));
    }
    else if (ParamName == "TestNum")
    {
        m_GpuTestNum = static_cast<UINT32>(strtoul(Value.c_str(), NULL, 10));
    }
    else
    {
        CHECK_RC(LwpActor::SetParameterByName(ParamName, Value));
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC PStateActor::ExtraParamFromJs(jsval jsvPerfPoints)
{
    JavaScriptPtr pJs;
    JsArray jsa;
    RC rc;
    CHECK_RC(pJs->FromJsval(jsvPerfPoints, &jsa));

    m_PerfPoints.clear();
    for (unsigned ii = 0; ii < jsa.size(); ++ii)
    {
        JSObject *pObj;
        CHECK_RC(pJs->FromJsval(jsa[ii], &pObj));
        Perf::PerfPoint NewPt;
        CHECK_RC(m_pPerf->JsObjToPerfPoint(pObj, &NewPt));
        m_PerfPoints.push_back(NewPt);
    }
    if (m_PerfPoints.size() > 1)
    {
        // Initialize a track list of every perf point being tested
        m_WasPerfPointUsed.clear();
        m_WasPerfPointUsed.resize(m_PerfPoints.size(), false);
    }
    else
    {
        Printf(Tee::PriError, "Need to give at least 2 PerfPoints to PStateActor\n");
        return RC::BAD_PARAMETER;
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC PStateActor::CheckParams()
{
    RC rc;
    // make sure we actually have PStates!
    UINT32 NumPStates = 0;
    if (!m_pPerf)
    {
        Printf(Tee::PriError, "GpuInst must be specified\n");
        return RC::BAD_PARAMETER;
    }
    CHECK_RC(m_pPerf->GetNumPStates(&NumPStates));
    if (NumPStates < 1)
    {
        Printf(Tee::PriError, "cannot use PState actor if there is no pstates\n");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(SetupActor());
    return rc;
}
//-----------------------------------------------------------------------------
RC PStateActor::SetupActor()
{
    RC rc;

    // Set PState JS callbacks. If there are no callbacks, we can run in a
    // detached thread.

    bool anyCallbacks = false;
    if (m_PrePStateCallback != "")
    {
        CHECK_RC(m_pPerf->SetUseCallbacks(Perf::PRE_PSTATE,
                                          m_PrePStateCallback));
        anyCallbacks = true;
    }
    if (m_PostPStateCallback != "")
    {
        CHECK_RC(m_pPerf->SetUseCallbacks(Perf::POST_PSTATE,
                                          m_PostPStateCallback));
        anyCallbacks = true;
    }
    if (m_PreCleanupCallback != "")
    {
        CHECK_RC(m_pPerf->SetUseCallbacks(Perf::PRE_CLEANUP,
                                          m_PreCleanupCallback));
        anyCallbacks = true;
    }

    m_OrigCallbackUsage = m_pPerf->GetUsePStateCallbacks();
    if (!anyCallbacks && !m_EnableJson)
    {
        // Forcibly disable PState callbacks to avoid accidentally calling into
        // JS code (not thread safe)
        CHECK_RC(m_pPerf->SetUsePStateCallbacks(false));
        m_IsThreadSafe = true;
    }

    CHECK_RC(m_PerfProfiler.Setup(m_pPerf->GetGpuSubdevice()));

    CHECK_RC(m_pPerf->GetLwrrentPerfPoint(&m_InitialPerfPoint));

    return rc;
}

//-----------------------------------------------------------------------------
RC PStateActor::RunIndex(UINT32 Idx)
{
    RC rc;
    if ((!m_DispClkSwitchSkipList.empty()) ||
        m_PrintDispClkTransitions)
    {
        const UINT32 prevIdx = GetPreviousActionIndex();
        const UINT32 prevActiolwal = static_cast<UINT32>(GetActiolwalById(prevIdx));
        const UINT32 nextActiolwal = static_cast<UINT32>(GetActiolwalById(Idx));

        if (nextActiolwal >= m_PerfPoints.size())
        {
            Printf(Tee::PriError, "Next Actiolwal (%d) out of range\n", nextActiolwal);
            return RC::SOFTWARE_ERROR;
        }

        Perf::PerfPoint & ppPrev = m_PerfPoints[prevActiolwal];
        Perf::PerfPoint & ppNext = m_PerfPoints[nextActiolwal];
        UINT64 NextDispClkKHz = 0;
        if (OK == m_pPerf->GetClockAtPerfPoint(ppNext, Gpu::ClkDisp, &NextDispClkKHz))
            NextDispClkKHz /= 1000;

        UINT64 PrevDispClkKHz = 0;
        if (OK == m_pPerf->GetClockAtPerfPoint(ppPrev, Gpu::ClkDisp, &PrevDispClkKHz))
            PrevDispClkKHz /= 1000;

        if (IsDispClkTransitionSkipped(PrevDispClkKHz, NextDispClkKHz))
        {
            if (m_PrintDispClkTransitions)
            {
                Printf(m_PrintPri, "PStateActor::RunIndex: "
                       "Skipping transition from %lldKHz to %lldKHz\n",
                       PrevDispClkKHz, NextDispClkKHz);
            }
            AdvanceToNextAction(true); // skip next action
            return OK;
        }
        else if (m_PrintDispClkTransitions)
        {
            Printf(m_PrintPri,
                   "PStateActor: Detected dispclk transition from %lldKHz to %lldKHz\n",
                   PrevDispClkKHz, NextDispClkKHz);
        }
    }

    GpuSubdevice * pGpuSubdevice = m_pPerf->GetGpuSubdevice();
    const UINT32 devInst = pGpuSubdevice->GetParentDevice()->GetDeviceInst();
    string Name = GetActorName();
    const UINT32 prevIdx = GetPreviousActionIndex();
    const UINT32 prevActiolwal = static_cast<UINT32>(GetActiolwalById(prevIdx));
    const UINT32 actiolwal = static_cast<UINT32>(GetActiolwalById(Idx));
    Printf(m_PrintPri, "dev%d PStateActor %s ::RunIndex(%d) - val = %d\n",
           devInst,
           Name.c_str(),
           Idx,
           actiolwal);

    if (actiolwal >= m_PerfPoints.size())
    {
        Printf(Tee::PriError, "Actiolwal out of range\n");
        return RC::SOFTWARE_ERROR;
    }
    Perf::PerfPoint & prevPt = m_PerfPoints[prevActiolwal];
    Perf::PerfPoint & pt = m_PerfPoints[actiolwal];
    m_WasPerfPointUsed[actiolwal] = true;

    if (prevPt.PStateNum != pt.PStateNum)
    {
        pair<UINT32, UINT32> lwrrPStateSwitch(prevPt.PStateNum, pt.PStateNum);
        m_PStateSwitchesCount[lwrrPStateSwitch]++;
    }

    Printf(m_PrintPri, "dev%d PerfPoint %s",
           devInst,
           pt.name(m_pPerf->IsPState30Supported()).c_str());

    if (pt.SpecialPt != Perf::GpcPerf_INTERSECT)
    {
        UINT64 perfPointGpcPerHz = 0;
        if (pt.GetFreqHz(Gpu::ClkGpc, &perfPointGpcPerHz) == RC::OK)
        {
            Printf(m_PrintPri, " target gpcclk frequency: %llu MHz",
               perfPointGpcPerHz / 1000000);
        }
    }

    if (m_pPerf->IsPState40Supported() && m_SendDramLimitStrict)
    {
        Perf::ClkRange lwrDramRange;
        CHECK_RC(m_pPerf->GetClockRange(pt.PStateNum, Gpu::ClkM, &lwrDramRange));
        pt.Clks[Gpu::ClkM] = Perf::ClkSetting(Gpu::ClkM,
                                              lwrDramRange.MinKHz * 1000ULL,
                                              Perf::FORCE_PERF_LIMIT_MIN);
    }

    Printf(m_PrintPri, "\n");

    if (m_PrintClockChanges)
    {
        if (0 == pt.Clks.size())
        {
            Printf(Tee::PriNormal,
                   "Dispather not overriding any clocks for pstate %u\n",
                   pt.PStateNum);
        }
        else
        {
            Printf(Tee::PriNormal,
                   "Dispatcher overriding clocks for pstate %u:\n",
                   pt.PStateNum);
            Perf::PrintClockInfo(&pt);
        }
    }

    RC setPerfRc = m_pPerf->SetPerfPoint(pt);

    CHECK_RC(m_PerfProfiler.Sample());
    m_PerfProfiler.PrintSample(m_PrintPri);

    // Callwlate the test progress data
    UINT32 cov = CallwlateCoverage();

    // Cap it, PrintProgressUpdate asserts if we pass a value greater
    // than what we specified initially
    UINT32 cappedCov = (cov <= m_CoveragePct) ? cov : m_CoveragePct;

    ModsTest::PrintProgressUpdate(cappedCov);

    if (m_PrintClockChanges)
    {
        Perf::PerfPoint ppSet;
        CHECK_RC(m_pPerf->GetLwrrentPerfPoint(&ppSet));
        Printf(Tee::PriNormal, "Clocks are now:\n");
        Perf::PrintClockInfo(&ppSet);
    }

    AdvanceToNextAction();

    if (m_PrintRmPerfLimits)
        CHECK_RC(m_pPerf->PrintRmPerfLimits(m_PrintPri));

    return setPerfRc ? setPerfRc : rc;
}

//-----------------------------------------------------------------------------
void PStateActor::AddJsonInfo()
{
    if (!GetJsonItem())
        return;

    if (m_PrePStateCallback != "")
    {
        GetJsonItem()->SetField("PrePStateCallback", m_PrePStateCallback.c_str());
    }
    if (m_PostPStateCallback != "")
    {
        GetJsonItem()->SetField("PostPStateCallback", m_PostPStateCallback.c_str());
    }
    if (m_PreCleanupCallback != "")
    {
        GetJsonItem()->SetField("PreCleanupCallback", m_PreCleanupCallback.c_str());
    }
}
//-----------------------------------------------------------------------------
RC PStateActor::SetActorPropertiesByJsObj(JSObject *pObj)
{
    RC rc;
    JavaScriptPtr pJs;
    CHECK_RC(LwpActor::SetActorPropertiesByJsObj(pObj));
    // the below properties are optional
    pJs->GetProperty(pObj, "PrePStateCallback", &m_PrePStateCallback);
    pJs->GetProperty(pObj, "PostPStateCallback", &m_PostPStateCallback);
    pJs->GetProperty(pObj, "PreCleanupCallback", &m_PreCleanupCallback);
    return rc;
}
//-----------------------------------------------------------------------------
RC PStateActor::PrintCoverage(Tee::Priority PrintPri)
{
    Printf(PrintPri,
           "Total perf points created: %u\n",
           static_cast<UINT32>(m_PerfPoints.size()));

    UINT32 GpcClkCoverage = CallwlateCoverage();

    if (!m_PerfSweep)
    {
        Printf(PrintPri,
               "Total unique gpcclk values created: %u\n",
               static_cast<UINT32>(m_UniqueGpcClkList.size()));
    }

    if (!m_UniqueGpcClkList.empty())
    {
        Printf(PrintPri,
               "%u%% of %s gpcclk values were tested\n", GpcClkCoverage,
               m_PerfSweep ? "" : "unique");
    }

    m_PerfProfiler.PrintSummary(PrintPri);

    return OK;
}
//-----------------------------------------------------------------------------
UINT32 PStateActor::CallwlateCoverage()
{
    UINT32 NumPointsCreated = static_cast<UINT32>(m_PerfPoints.size());
    UINT32 NumPointsTested = 0;

    for (UINT32 idx = 0; idx < m_PerfPoints.size(); idx++)
    {
        if (m_WasPerfPointUsed[idx])
            NumPointsTested++;
    }

    if (!m_PerfSweep)
    {
        UINT32 gpcclkFreqsTested = 0;

        // Sort the perf points into a unique list of gpcclk values
        UINT64 gpcPerfHz;
        RC gpcclk_rc;
        for (UINT32 idx = 0; idx < m_PerfPoints.size(); idx++)
        {
            // Record any new gpcclk frequencies that were covered
            if (!m_WasPerfPointUsed[idx] ||
                m_PerfPoints[idx].SpecialPt == Perf::GpcPerf_INTERSECT)
            {
                continue;
            }

            gpcPerfHz = 0;
            gpcclk_rc.Clear();
            gpcclk_rc = m_pPerf->GetGpcFreqOfPerfPoint(m_PerfPoints[idx],
                &gpcPerfHz);
            if ((gpcclk_rc == OK) && (gpcPerfHz != 0))
            {
                pair<map<UINT64, bool>::iterator, bool> element =
                    m_UniqueGpcClkList.insert(pair<UINT64, bool>(gpcPerfHz,
                        m_WasPerfPointUsed[idx]));

                // Even if the "inserted" element already exists,
                // mark if it was tested
                if (!element.second && m_WasPerfPointUsed[idx])
                    element.first->second = true;
            }
        }

        for (map<UINT64, bool>::iterator it = m_UniqueGpcClkList.begin();
        it != m_UniqueGpcClkList.end(); it++)
        {
            if (it->second)
                gpcclkFreqsTested++;
        }
        Printf(Tee::PriLow,
            "Actual gpcclk values tested: %u\n", gpcclkFreqsTested);
    }

    UINT32 Coverage = static_cast<UINT32>(static_cast<FLOAT64>(NumPointsTested) /
                                          NumPointsCreated * 100.0);

    return (Coverage > 100 ? 100 : Coverage);
}

bool PStateActor::IsDispClkTransitionSkipped
(
    UINT64 PrevDispClkKHz,
    UINT64 NextDispClkKHz
)
{
    PrevDispClkKHz /= 1000;
    NextDispClkKHz /= 1000;
    UINT32 transIdx = 0;
    for (; transIdx < m_DispClkSwitchSkipList.size(); transIdx++)
    {
        if ((PrevDispClkKHz == m_DispClkSwitchSkipList[transIdx].FromClockValMHz) &&
            (NextDispClkKHz == m_DispClkSwitchSkipList[transIdx].ToClockValMHz))
            break;
    }
    return ((transIdx != m_DispClkSwitchSkipList.size()) ? true : false);
}

RC PStateActor::Cleanup()
{
    StickyRC rc;

    if (m_pPerf)
    {
        rc = m_pPerf->SetUsePStateCallbacks(m_OrigCallbackUsage);
        if (m_InitialPerfPoint.PStateNum != Perf::ILWALID_PSTATE)
        {
            rc = m_pPerf->SetPerfPoint(m_InitialPerfPoint);
        }
    }

    rc = m_PerfProfiler.Cleanup();

    return rc;
}

//-----------------------------------------------------------------------------
// SimTempActor: RunIndex sets a simulated temperature
//-----------------------------------------------------------------------------
SimTempActor::SimTempActor(Tee::Priority printPri)
    : LwpActor(SIMTEMP_ACTOR, printPri)
    , m_pThermal(NULL)
{
}

SimTempActor::~SimTempActor()
{
}

RC SimTempActor::Cleanup()
{
    RC rc;

    if (m_pThermal)
    {
        CHECK_RC(m_pThermal->ClearSimulatedTemperature(OverTempCounter::INTERNAL_TEMP));
    }

    return rc;
}

RC SimTempActor::SetParameterByName(const string& paramName, const string& value)
{
    RC rc;
    if (paramName == "GpuInst")
    {
        UINT32 gpuInst = static_cast<UINT32>(strtoul(value.c_str(), NULL, 10));
        GpuDevMgr* pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
        if (!pGpuDevMgr)
        {
            Printf(Tee::PriError, "Gpu subdevice manager not ready yet.\n");
            return RC::SOFTWARE_ERROR;
        }
        GpuSubdevice *pSubdev = pGpuDevMgr->GetSubdeviceByGpuInst(gpuInst);
        if (!pSubdev)
        {
            Printf(Tee::PriError, "GpuInst %d unknown\n", gpuInst);
            return RC::BAD_PARAMETER;
        }
        m_pThermal = pSubdev->GetThermal();
        MASSERT(m_pThermal);
    }
    else
    {
        CHECK_RC(LwpActor::SetParameterByName(paramName, value));
    }
    return rc;
}

RC SimTempActor::CheckParams()
{
    RC rc;
    if (!m_pThermal)
    {
        Printf(Tee::PriError, "GpuInst must be specified\n");
        return RC::BAD_PARAMETER;
    }
    return rc;
}

RC SimTempActor::RunIndex(UINT32 index)
{
    RC rc;
    string name = GetActorName();
    const UINT32 actiolwal = static_cast<UINT32>(GetActiolwalById(index));
    Printf(Tee::PriLow, "SimTempActor %s ::RunIndex(%d) - val = %d\n",
           name.c_str(), index, actiolwal);

    CHECK_RC(m_pThermal->SetSimulatedTemperature(OverTempCounter::INTERNAL_TEMP,
                                                 actiolwal));
    AdvanceToNextAction();
    return rc;
}

void SimTempActor::AddJsonInfo()
{
}

//-----------------------------------------------------------------------------
// PcieSpeedChangeActor: RunIndex changes the PcieSpeed
//-----------------------------------------------------------------------------
PcieSpeedChangeActor::PcieSpeedChangeActor(Tee::Priority printPri)
    : LwpActor(PCIECHANGE_ACTOR, printPri)
    , m_pGpuSubdev(nullptr)
    , m_pGpuPcie(nullptr)
    , m_pPerf(nullptr)
    , m_InitialSpeed(Pci::SpeedUnknown)
{
}

PcieSpeedChangeActor::~PcieSpeedChangeActor()
{
}

RC PcieSpeedChangeActor::Cleanup()
{
    StickyRC rc;
    JavaScriptPtr pJavaScript;

    // Restore initial PCIE link speed
    if (m_pPerf && m_pPerf->IsPState30Supported())
    {
        if (m_InitialPerfPoint.PStateNum != Perf::ILWALID_PSTATE)
        {
            rc = m_pPerf->SetPerfPoint(m_InitialPerfPoint);
        }
    }
    else
    {
        rc = m_pGpuPcie->SetLinkSpeed(m_InitialSpeed);
    }

    // Stop skipping the PCIE link speed check
    JSObject* pGlobalObj = pJavaScript->GetGlobalObject();
    jsval skipCheck;
    RC jsrc;
    if (OK == (jsrc = pJavaScript->ToJsval(false, &skipCheck)))
    {
        rc = pJavaScript->SetProperty(pGlobalObj, "g_SkipPcieSpeedCheck", skipCheck);
    }
    rc = jsrc;

    return rc;
}

RC PcieSpeedChangeActor::SetParameterByName(const string& paramName, const string& value)
{
    RC rc;
    if (paramName == "GpuInst")
    {
        UINT32 gpuInst = static_cast<UINT32>(strtoul(value.c_str(), NULL, 10));
        GpuDevMgr* pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
        if (!pGpuDevMgr)
        {
            Printf(Tee::PriNormal, "Gpu subdevice manager not ready yet.\n");
            return RC::SOFTWARE_ERROR;
        }
        m_pGpuSubdev = pGpuDevMgr->GetSubdeviceByGpuInst(gpuInst);
        if (!m_pGpuSubdev)
        {
            Printf(Tee::PriNormal, "GpuInst %d unknown\n", gpuInst);
            return RC::BAD_PARAMETER;
        }
        m_pGpuPcie = m_pGpuSubdev->GetInterface<Pcie>();
        if (!m_pGpuPcie)
        {
            Printf(Tee::PriNormal, "Cannot get GpuPcie for GpuInst %d\n", gpuInst);
            return RC::SOFTWARE_ERROR;
        }
        m_pPerf = m_pGpuSubdev->GetPerf();
        if (!m_pPerf)
        {
            Printf(Tee::PriError, "Cannot get Perf for GpuInst\n");
            return RC::SOFTWARE_ERROR;
        }
        m_InitialSpeed = m_pGpuPcie->GetLinkSpeed(Pci::SpeedUnknown);
        CHECK_RC(m_pPerf->GetLwrrentPerfPoint(&m_InitialPerfPoint));
    }
    else
    {
        CHECK_RC(LwpActor::SetParameterByName(paramName, value));
    }
    return rc;
}

RC PcieSpeedChangeActor::CheckParams()
{
    RC rc;
    if (!m_pGpuPcie)
    {
        Printf(Tee::PriNormal, "GpuInst must be specified\n");
        return RC::BAD_PARAMETER;
    }
    return rc;
}

RC PcieSpeedChangeActor::RunIndex(UINT32 index)
{
    RC rc;
    string name = GetActorName();
    const Pci::PcieLinkSpeed newSpeed = static_cast<Pci::PcieLinkSpeed>(GetActiolwalById(index));
    Printf(m_PrintPri, "PcieSpeedChangeActor %s ::RunIndex(%d) - new speed = %dMB/s\n",
           name.c_str(), index, newSpeed);

    if (m_pPerf->IsPState30Supported())
    {
        UINT32 rmPexSpeed = PerfUtil::PciGenToCtrl2080Bit(newSpeed);
        CHECK_RC(m_pGpuSubdev->SetClock(Gpu::ClkPexGen, rmPexSpeed));
    }
    else
    {
        CHECK_RC(m_pGpuPcie->SetLinkSpeed(newSpeed));
    }

    AdvanceToNextAction();
    return rc;
}

void PcieSpeedChangeActor::AddJsonInfo()
{
}

//-----------------------------------------------------------------------------
// ThermalLimitActor: RunIndex sets a simulated temperature
//-----------------------------------------------------------------------------
ThermalLimitActor::ThermalLimitActor(Tee::Priority printPri)
    : LwpActor(THERMALLIMIT_ACTOR, printPri)
    , m_pGpuSubdev(NULL)
    , m_pThermal(NULL)
{
}

ThermalLimitActor::~ThermalLimitActor()
{
}

/*static*/ map<string, UINT32> ThermalLimitActor::s_ThermalEventStringMap =
{
    {"extovert",       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EXT_OVERT},
    {"extalert",       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EXT_ALERT},
    {"extpower",       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_EXT_POWER},
    {"overt",          LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_OVERT},
    {"alert0h",        LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_0H},
    {"alert1h",        LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_1H},
    {"alert2h",        LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_2H},
    {"alert3h",        LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_3H},
    {"alert4h",        LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_4H},
    {"alertneg1h",     LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_ALERT_NEG1H},
    {"thermal0",       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_0},
    {"thermal1",       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_1},
    {"thermal2",       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_2},
    {"thermal3",       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_3},
    {"thermal4",       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_4},
    {"thermal5",       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_5},
    {"thermal6",       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_6},
    {"thermal7",       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_7},
    {"thermal8",       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_8},
    {"thermal9",       LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_9},
    {"thermal10",      LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_10},
    {"thermal11",      LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_THERMAL_11},
    {"dedicatedovert", LW2080_CTRL_CMD_THERMAL_HWFS_EVENT_ID_DEDICATED_OVERT},
    {"slowdownpwm",    s_SlowdownPwmEventId}
};

/*static*/ UINT32 ThermalLimitActor::TranslateThermalEventString(string str)
{
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    str.erase(std::remove(str.begin(), str.end(), ' '), str.end());

    if (s_ThermalEventStringMap.find(str) != s_ThermalEventStringMap.end())
    {
        return s_ThermalEventStringMap[str];
    }
    else
    {
        return s_IlwalidEventId;
    }
};

RC ThermalLimitActor::Cleanup()
{
    RC rc;

    if (m_pThermal)
    {
        for (map<UINT32, INT32>::iterator it = m_SavedThermalLimitValues.begin();
             it != m_SavedThermalLimitValues.end(); it++)
        {
            CHECK_RC(RestoreThermalLimit(it->first));
        }
    }

    return rc;
}

RC ThermalLimitActor::SetParameterByName(const string& paramName, const string& value)
{
    RC rc;
    if (paramName == "GpuInst")
    {
        UINT32 gpuInst = static_cast<UINT32>(strtoul(value.c_str(), NULL, 10));
        GpuDevMgr* pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
        if (!pGpuDevMgr)
        {
            Printf(Tee::PriError, "Gpu subdevice manager not ready yet.\n");
            return RC::SOFTWARE_ERROR;
        }
        m_pGpuSubdev = pGpuDevMgr->GetSubdeviceByGpuInst(gpuInst);
        if (!m_pGpuSubdev)
        {
            Printf(Tee::PriError, "GpuInst %d unknown\n", gpuInst);
            return RC::BAD_PARAMETER;
        }
        m_pThermal = m_pGpuSubdev->GetThermal();
        MASSERT(m_pThermal);
    }
    else if (paramName == "PWMPeriod")
    {
        if (!m_pGpuSubdev)
        {
            Printf(Tee::PriError, "Must specify GpuInst before PWMPeriod\n");
            return RC::BAD_PARAMETER;
        }

        RegHal &regs = m_pGpuSubdev->Regs();
        UINT32 periodValue = static_cast<UINT32>(strtoul(value.c_str(), NULL, 10));
        UINT32 periodMax = regs.LookupValue(MODS_THERM_SLOWDOWN_PWM_0_PERIOD_MAX);
        if (periodValue > periodMax)
        {
            Printf(Tee::PriError, "Period value %u greater than allowed max %u\n",
                   periodValue, periodMax);
            return RC::BAD_PARAMETER;
        }

        UINT32 pwm0 = regs.Read32(MODS_THERM_SLOWDOWN_PWM_0);
        regs.SetField(&pwm0, MODS_THERM_SLOWDOWN_PWM_0_PERIOD, periodValue);
        regs.Write32(MODS_THERM_SLOWDOWN_PWM_0, pwm0);
    }
    else if (paramName == "PWMDutyCycle")
    {
        if (!m_pGpuSubdev)
        {
            Printf(Tee::PriError, "Must specify GpuInst before PWMDutyCycle\n");
            return RC::BAD_PARAMETER;
        }

        RegHal &regs = m_pGpuSubdev->Regs();
        UINT32 dutyCycleValue = static_cast<UINT32>(strtoul(value.c_str(), NULL, 10));
        UINT32 dutyCycleMax = regs.LookupValue(MODS_THERM_SLOWDOWN_PWM_1_HI_MAX);
        if (dutyCycleValue > dutyCycleMax)
        {
            Printf(Tee::PriError, "DutyCycle value %u greater than allowed max %u\n",
                   dutyCycleValue, dutyCycleMax);
            return RC::BAD_PARAMETER;
        }

        UINT32 pwm1 = regs.Read32(MODS_THERM_SLOWDOWN_PWM_1);
        regs.SetField(&pwm1, MODS_THERM_SLOWDOWN_PWM_1_HI, dutyCycleValue);
        regs.Write32(MODS_THERM_SLOWDOWN_PWM_1, pwm1);
    }
    else
    {
        CHECK_RC(LwpActor::SetParameterByName(paramName, value));
    }
    return rc;
}

RC ThermalLimitActor::ParseParams(const vector<jsval>& jsPickers, const string& Params)
{
    RC rc;
    JavaScriptPtr pJS;
    // parse all the non fancy picker parameters
    CHECK_RC(ParseStrParams(Params));

    if (jsPickers.size() < 4)
    {
        Printf(Tee::PriError, "need fancy picker to specify the indexes, times, events,"
                              "and values\n");
        return RC::BAD_PARAMETER;
    }

    JsArray events;
    CHECK_RC(pJS->FromJsval(jsPickers[2], &events));
    JsArray values;
    CHECK_RC(pJS->FromJsval(jsPickers[3], &values));
    if (events.size() != values.size())
    {
        Printf(Tee::PriError, "ThermalLimitActor: every event must also specify a value");
        return RC::BAD_PARAMETER;
    }

    for (UINT32 i = 0; i < events.size(); i++)
    {
        string eventStr;
        CHECK_RC(pJS->FromJsval(events[i], &eventStr));

        INT32 value = 0;
        CHECK_RC(pJS->FromJsval(values[i], &value));

        ThermalLimit newLimit = {};
        newLimit.eventStr = eventStr;
        newLimit.eventId  = TranslateThermalEventString(eventStr);
        newLimit.value    = value;

        if (newLimit.eventId == s_IlwalidEventId)
        {
            Printf(Tee::PriError, "ThermalLimitActor: invalid event name: %s\n", eventStr.c_str());
            Printf(Tee::PriHigh, "Valid event names are {");
            for (map<string, UINT32>::iterator it = s_ThermalEventStringMap.begin();
                 it != s_ThermalEventStringMap.end(); it++)
            {
                Printf(Tee::PriHigh, "%s, ", it->first.c_str());
            }
            Printf(Tee::PriHigh, "}\n");
            return RC::BAD_PARAMETER;
        }

        m_ThermalLimitList.push_back(newLimit);
    }

    FancyPicker valPicker, timePicker;
    CHECK_RC(valPicker.FromJsval(jsPickers[0]));
    CHECK_RC(timePicker.FromJsval(jsPickers[1]));

    // initialize the action array: maybe pull this into Action class
    FancyPicker::FpContext context;
    context.LoopNum    = 0;
    context.RestartNum = 0;
    context.pJSObj     = 0;
    context.Rand.SeedRandom(GetSeed());
    valPicker.SetContext(&context);
    timePicker.SetContext(&context);

    Printf(m_PrintPri, " %s Actions:\n", GetActorName().c_str());
    // create a list of actions based on fancy pickers
    for (UINT32 i = 0; i < GetNumSteps(); i++)
    {
        Action NewAction;
        NewAction.Actiolwal    = valPicker.Pick();
        NewAction.TimeUsToNext = timePicker.Pick();
        if (GetUseMsAsTime())
        {
            NewAction.TimeUsToNext *= 1000;
        }

        ThermalLimit& limit = m_ThermalLimitList[NewAction.Actiolwal];

        string valStr = "";
        if (limit.eventId == s_SlowdownPwmEventId)
            valStr = (limit.value == 0) ? "Disable" : "Enable";
        else if (limit.value == -1)
            valStr = "Temp Reset";
        else
            valStr = Utility::StrPrintf("Temp = %dC", limit.value);

        Printf(m_PrintPri, " %02d: Event = %s, %s, NextTimeout = %dus\n",
               i, limit.eventStr.c_str(), valStr.c_str(), NewAction.TimeUsToNext);

        AddAction(NewAction);
        context.LoopNum++;
    }
    return rc;
}

RC ThermalLimitActor::CheckParams()
{
    RC rc;
    if (!m_pGpuSubdev)
    {
        Printf(Tee::PriError, "GpuInst must be specified\n");
        return RC::BAD_PARAMETER;
    }
    return rc;
}

RC ThermalLimitActor::RunIndex(UINT32 index)
{
    RC rc;
    string name = GetActorName();
    const ThermalLimit& limit = m_ThermalLimitList[GetActiolwalById(index)];
    Printf(Tee::PriLow, "ThermalLimitActor %s ::RunIndex(%d) - event = %s, temp = %dC\n",
           name.c_str(), index, limit.eventStr.c_str(), limit.value);

    if (limit.eventId == s_SlowdownPwmEventId)
    {
        RegHal &regs = m_pGpuSubdev->Regs();
        UINT32 pwm0 = regs.Read32(MODS_THERM_SLOWDOWN_PWM_0);
        regs.SetField(&pwm0, (limit.value == 0)
                                                 ? MODS_THERM_SLOWDOWN_PWM_0_EN_DISABLE
                                                 : MODS_THERM_SLOWDOWN_PWM_0_EN_ENABLE);
        regs.Write32(MODS_THERM_SLOWDOWN_PWM_0, pwm0);
    }
    else if (limit.value == s_RestoreLimitValue)
    {
        CHECK_RC(RestoreThermalLimit(limit.eventId));
    }
    else
    {
        CHECK_RC(SaveThermalLimit(limit.eventId));
        // value*256 because the RM spec defines the temperature in units of (1/256)C
        CHECK_RC(m_pThermal->SetThermalLimit(limit.eventId, limit.value * 256));
    }
    AdvanceToNextAction();
    return rc;
}

RC ThermalLimitActor::SaveThermalLimit(UINT32 eventId)
{
    RC rc;
    if (m_SavedThermalLimitValues.find(eventId) == m_SavedThermalLimitValues.end())
    {
        INT32 thermalLimit = 0;
        CHECK_RC(m_pThermal->GetThermalLimit(eventId, &thermalLimit));
        m_SavedThermalLimitValues[eventId] = thermalLimit;
    }
    return rc;
}

RC ThermalLimitActor::RestoreThermalLimit(UINT32 eventId)
{
    RC rc;
    if (m_SavedThermalLimitValues.find(eventId) == m_SavedThermalLimitValues.end())
    {
        Printf(Tee::PriHigh, "ThermalLimitActor: cannot restore limit for event %d."
                              " It has not yet been changed.\n", eventId);
        return RC::BAD_PARAMETER;
    }
    CHECK_RC(m_pThermal->SetThermalLimit(eventId, m_SavedThermalLimitValues[eventId]));
    return rc;
}

void ThermalLimitActor::AddJsonInfo()
{
}

DroopyActor::DroopyActor(Tee::Priority pri) :
    LwpActor(DROOPY_ACTOR, pri)
{
    m_EnableJson = false;
    m_IsThreadSafe = true;
    m_ActorName = "droopy";
}

RC DroopyActor::Cleanup()
{
    RC rc;

    if (m_pPower && m_OrigLimitmA)
    {
        CHECK_RC(m_pPower->SetPowerCap(Power::DROOPY, m_OrigLimitmA));
    }

    return rc;
}

RC DroopyActor::SetParameterByName(const string& paramName, const string& value)
{
    RC rc;
    if (paramName == "GpuInst")
    {
        if (m_pPower)
        {
            Printf(Tee::PriError, "GpuInst specified multiple times\n");
            return RC::BAD_PARAMETER;
        }
        UINT32 gpuInst = static_cast<UINT32>(strtoul(value.c_str(), NULL, 10));
        GpuDevMgr* pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
        if (!pGpuDevMgr)
        {
            Printf(Tee::PriError, "Gpu subdevice manager not ready yet.\n");
            return RC::SOFTWARE_ERROR;
        }
        GpuSubdevice *pSubdev = pGpuDevMgr->GetSubdeviceByGpuInst(gpuInst);
        if (!pSubdev)
        {
            Printf(Tee::PriError, "GpuInst %d unknown\n", gpuInst);
            return RC::BAD_PARAMETER;
        }
        m_pPower = pSubdev->GetPower();
        if (!m_pPower)
        {
            Printf(Tee::PriError, "Cannot get power object for GPU %d\n", gpuInst);
            return RC::SOFTWARE_ERROR;
        }
        rc = m_pPower->GetPowerCap(Power::DROOPY, &m_OrigLimitmA);
        if (rc != OK)
        {
            m_OrigLimitmA = 0;
            return rc;
        }
    }
    else
    {
        CHECK_RC(LwpActor::SetParameterByName(paramName, value));
    }
    return rc;
}

RC DroopyActor::CheckParams()
{
    RC rc;
    if (!m_pPower)
    {
        Printf(Tee::PriError, "GpuInst must be specified\n");
        return RC::BAD_PARAMETER;
    }
    return rc;
}

RC DroopyActor::RunIndex(UINT32 index)
{
    RC rc;

    string name = GetActorName();
    const UINT32 actiolwal = static_cast<UINT32>(GetActiolwalById(index));
    Printf(Tee::PriLow, "DroopyActor::RunIndex(%d) - val = %d\n", index, actiolwal);

    CHECK_RC(m_pPower->SetPowerCap(Power::DROOPY, actiolwal));

    AdvanceToNextAction();
    return rc;
}

CpuPowerActor::CpuPowerActor(Tee::Priority pri)
    : LwpActor(CPUPOWER_ACTOR, pri)
{
    m_LastAction = { 0, Action::THREAD, -1.0 };
}

/*static*/ volatile UINT32 CpuPowerActor::s_DevInstOwner = ~0U;

RC CpuPowerActor::Cleanup()
{
    StickyRC rc;

    while (m_Threads.size() > 0)
    {
        rc = RemoveThread();
    }

    Cpu::AtomicWrite(&s_DevInstOwner, ~0U);

    return rc;
}

RC CpuPowerActor::SetParameterByName(const string& paramName, const string& value)
{
    RC rc;

    if (paramName == "DevInst")
    {
        m_DevInst = static_cast<UINT32>(strtoul(value.c_str(), NULL, 10));
    }
    else if (paramName == "TargetPower")
    {
        m_TargetPower = (value == "true");
    }
    else if (paramName == "SleepUs")
    {
        m_SleepUs = static_cast<UINT32>(strtoul(value.c_str(), NULL, 10));
    }
    else if (paramName == "LoopsToSleep")
    {
        m_LoopsToSleep = static_cast<UINT32>(strtoul(value.c_str(), NULL, 10));
    }
    else
    {
        CHECK_RC(LwpActor::SetParameterByName(paramName, value));
    }
    return rc;
}

RC CpuPowerActor::CheckParams()
{
    RC rc;

    if (m_DevInst == ~0U)
    {
        Printf(Tee::PriError, "CpuPowerActor: DevInst must be specified\n");
        return RC::BAD_PARAMETER;
    }
    else if (!Cpu::AtomicCmpXchg32(&s_DevInstOwner, ~0U, m_DevInst))
    {
        Printf(Tee::PriError, "CpuPowerActor: Already owned by DevInst=%u."
                              " Only one instance can exist at a time.\n", s_DevInstOwner);
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC CpuPowerActor::RunIndex(UINT32 index)
{
    RC rc;

    // Check for any failing threads
    for (const auto& pThread: m_Threads)
    {
        CHECK_RC(pThread->threadRC);
    }

    if (m_TargetPower)
    {
        const FLOAT64 targetmw = static_cast<FLOAT64>(GetActiolwalById(index));

        UINT64 val = 0;
        CHECK_RC(CpuSensors::GetPkgPowermW(0, &val, 100));
        const FLOAT64 lwrrentmw = static_cast<FLOAT64>(val);

        Printf(m_PrintPri, "CpuPowerActor: Target Power = %.0fumW, Current Power = %.0fmW, NumThreads = %u, SlowdownFactor = %f\n",
               targetmw, lwrrentmw, Cpu::AtomicRead(&m_NumThreads), m_SlowdownFactor);

        // Don't bother adjusting if we're really close to the target.
        if (fabs(targetmw - lwrrentmw) >= (s_MinMargin / 2))
        {
            if (targetmw > lwrrentmw)
            {
                CHECK_RC(IncreasePower(lwrrentmw, targetmw));
            }
            else if (targetmw < lwrrentmw)
            {
                CHECK_RC(DecreasePower(lwrrentmw, targetmw));
            }
        }
    }
    else
    {
        const UINT64 targetVal = static_cast<UINT64>(GetActiolwalById(index));
        const UINT64 lwrrentVal = static_cast<UINT64>(Cpu::AtomicRead(&m_NumThreads));

        Printf(m_PrintPri, "CpuPowerActor: Target NumThreads = %llu, Current NumThreads = %llu\n", targetVal, lwrrentVal);

        if (targetVal > lwrrentVal)
        {
            while (Cpu::AtomicRead(&m_NumThreads) < targetVal)
            {
                CHECK_RC(AddThread());
            }
        }
        else if (targetVal < lwrrentVal)
        {
            while (Cpu::AtomicRead(&m_NumThreads) > targetVal)
            {
                CHECK_RC(RemoveThread());
            }
        }
    }

    AdvanceToNextAction();
    return rc;
}

RC CpuPowerActor::AddThread()
{
    StickyRC rc;

    UINT32 idx = Cpu::AtomicRead(&m_NumThreads);
    string threadName = Utility::StrPrintf("cpustress_%u", idx);
    m_Threads.push_back(make_unique<ThreadInfo>(idx));
    Cpu::AtomicAdd(&m_NumThreads, 1);

    ThreadInfo* threadInfo = m_Threads.back().get();
    if (!threadInfo->test->IsSupported())
    {
        Printf(Tee::PriError, "CpuPowerStress not supported on this system!\n");
        return RC::UNSUPPORTED_DEVICE;
    }
    CHECK_RC(threadInfo->test->Setup());

    threadInfo->id = Tasker::CreateThread(threadName.c_str(),
    [this, threadInfo]()
    {
        Tasker::DetachThread detach;

        for (UINT64 loopIdx = 1; Cpu::AtomicRead(&m_NumThreads) > threadInfo->idx; loopIdx++)
        {
            UINT64 startTime = Xp::GetWallTimeUS();
            threadInfo->threadRC = threadInfo->test->Run();
            UINT64 endTime = Xp::GetWallTimeUS();

            FLOAT64 slowdown = m_SlowdownFactor;
            if (slowdown > 0.0)
            {
                UINT64 runTime = endTime - startTime;
                FLOAT64 delayus = static_cast<FLOAT64>(runTime) * slowdown;
                Utility::Delay(static_cast<UINT32>(delayus));
            }

            SleepLoop(loopIdx);
        }
    });

    if (threadInfo->id == Tasker::NULL_THREAD)
    {
        rc = m_Threads.back()->test->Cleanup();
        m_Threads.pop_back();
        Cpu::AtomicAdd(&m_NumThreads, -1);
        rc = RC::SOFTWARE_ERROR;
        return rc;
    }

    return rc;
}

RC CpuPowerActor::RemoveThread()
{
    MASSERT(!m_Threads.empty());
    StickyRC rc;

    auto pThreadInfo = m_Threads.back().get();
    Cpu::AtomicAdd(&m_NumThreads, -1);
    rc = Tasker::Join(pThreadInfo->id);
    rc = pThreadInfo->threadRC;
    rc = pThreadInfo->test->Cleanup();
    m_Threads.pop_back();

    return rc;
}

RC CpuPowerActor::IncreasePower(const FLOAT64 lwrrentmw, const FLOAT64 targetmw)
{
    MASSERT(m_TargetPower);
    RC rc;

    if (m_LastAction.type == Action::THREAD)
    {
        if (m_LastAction.delta > 0.0)
        {
            // Added a thread and still below target val.
            if (fabs(lwrrentmw - m_LastAction.val) <= s_MinMargin || lwrrentmw < m_LastAction.val)
            {
                // Despite adding another thread, power draw didn't increase at all.
                Printf(m_PrintPri, "CpuPowerActor: Power draw has stopped increasing\n");
            }
            else
            {
                CHECK_RC(AddThread());
                m_LastAction = { lwrrentmw, Action::THREAD, 1.0 };
            }
        }
        else
        {
            // Removed a thread, but now target is above current.
            if (m_LastAction.val == 0.0 || // Initialization
                (fabs(targetmw - lwrrentmw) > fabs(targetmw - m_LastAction.val) &&
                 fabs(targetmw - lwrrentmw) >= s_MinMargin))
            {
                // If now we're further away from the target than we were before,
                // and that difference is significant, re-add the thread.
                CHECK_RC(AddThread());
                m_LastAction = { lwrrentmw, Action::THREAD, 1.0 };
            }
        }
    }
    else // SLOWDOWN
    {
        if (m_LastAction.delta > 0.0)
        {
            // Increased the slowdown but by too much, and now below the target value.
            // Roll back the most recent change a bit.
            FLOAT64 factor = -1.0 * m_LastAction.delta / 2.0;
            m_SlowdownFactor += factor;
            m_LastAction = { lwrrentmw, Action::SLOWDOWN, factor };
        }
        else
        {
            // Decreased the slowdown, but still below target.
            if (m_SlowdownFactor == 0.0)
            {
                CHECK_RC(AddThread());
                m_LastAction = { lwrrentmw, Action::THREAD, 1.0 };
            }
            else
            {
                // Target is still far away despite the last change.
                // Increase the delta to increase the power faster.
                if (fabs(targetmw - lwrrentmw) > 2.0 * fabs(lwrrentmw - m_LastAction.val))
                {
                    m_LastAction.delta *= 2.0;
                }
                m_SlowdownFactor += m_LastAction.delta;
                if (m_SlowdownFactor < 0.0)
                    m_SlowdownFactor = 0.0;
            }
        }
    }

    return rc;
}

RC CpuPowerActor::DecreasePower(const FLOAT64 lwrrentmw, const FLOAT64 targetmw)
{
    MASSERT(m_TargetPower);
    RC rc;

    if (m_LastAction.type == Action::THREAD)
    {
        if (m_LastAction.delta > 0.0)
        {
            // Added a thread and now we're above the target.
            // Start increasing the slowdown factor.
            m_SlowdownFactor = 0.1;
            m_LastAction = { lwrrentmw, Action::SLOWDOWN, 0.1 };
        }
        else
        {
            // Removed a thread and still above the target.
            // Remove another, if possible.
            if (Cpu::AtomicRead(&m_NumThreads) == 0)
            {
                Printf(m_PrintPri, "CpuPowerActor: NumThreads=0, cannot reduce stress any further\n");
            }
            else
            {
                CHECK_RC(RemoveThread());
                m_LastAction = { lwrrentmw, Action::THREAD, -1.0 };
            }
        }
    }
    else // SLOWDOWN
    {
        if (m_LastAction.delta > 0.0)
        {
            // Increased the slowdown, but still above the target. Increase more if possible.
            if (fabs(lwrrentmw - m_LastAction.val) <= s_MinMargin &&
                m_LastAction.delta >= 1000.0)
            {
                // Cannot decrease power any more with slowdown alone.
                CHECK_RC(RemoveThread());
                m_LastAction = { lwrrentmw, Action::THREAD, -1.0 };
                m_SlowdownFactor = 0.0;
            }
            else
            {
                FLOAT64 factor = m_LastAction.delta;
                if (fabs(targetmw - lwrrentmw) > 2.0 * fabs(lwrrentmw - m_LastAction.val))
                {
                    // Target is still far away despite the last change.
                    // Increase the delta to decrease the power faster.
                    factor *= 2.0;
                }
                m_SlowdownFactor += factor;
                m_LastAction = { lwrrentmw, Action::SLOWDOWN, factor };
            }
        }
        else
        {
            // Decreased the slowdown but by too much, and now above the target value.
            // Roll back the most recent change a bit.
            FLOAT64 factor = -1.0 * m_LastAction.delta / 2.0;
            m_SlowdownFactor += factor;
            m_LastAction = { lwrrentmw, Action::SLOWDOWN, factor };
        }
    }

    return rc;
}

void CpuPowerActor::SleepLoop(UINT64 loopIdx)
{
    // If enabled, sleep after every N loops.  This can create
    // a stutter in power usage, resulting in an increased power draw.
    if (m_LoopsToSleep > 0 && loopIdx % m_LoopsToSleep == 0)
    {
        const UINT32 old = Cpu::AtomicAdd(&m_NumSleeping, 1U);
        if (old + 1 >= Cpu::AtomicRead(&m_NumThreads))
        {
            Tasker::Sleep(m_SleepUs / 1000.0);
            Cpu::AtomicWrite(&m_NumSleeping, 0U);
        }
        else
        {
            while (true)
            {
                const auto numSleeping = Cpu::AtomicRead(&m_NumSleeping);
                if (numSleeping == 0)
                    break;
                if (Cpu::AtomicRead(&m_NumThreads) < numSleeping)
                {
                    Cpu::AtomicWrite(&m_NumSleeping, 0U);
                    break;
                }
            }
        }
    }
}

LwpDispatcher::LwpDispatcher(JSContext *cx, JSObject *obj)
:
 m_UseTrace(false),
 m_ThreadId(0),
 m_IsRunning(0),
 m_StartIdx(0),
 m_StopIdx(0),
 m_RunThreadMutex(NULL),
 m_StopAtFirstActorError(false),
 m_pCtx(cx),
 m_pObj(obj),
 m_PrintPri(Tee::PriLow)
{
    m_RunThreadMutex = Tasker::AllocMutex("LwpDispatcher::m_RunThreadMutex",
                                          Tasker::mtxUnchecked);
    MASSERT(m_RunThreadMutex);
}
LwpDispatcher::~LwpDispatcher()
{
    ClearActors();
    Tasker::FreeMutex(m_RunThreadMutex);
}
//-----------------------------------------------------------------------------
RC LwpDispatcher::AddActor
(
    ActorTypes Type,
    const vector<jsval> &jsPickers,
    const string &Params,
    jsval jsvPerfPoints
)
{
    RC rc;
    if (m_UseTrace)
    {
        Printf(Tee::PriError, "Cannot intermix new actor with existing trace\n");
        return RC::SOFTWARE_ERROR;
    }

    LwpActor *pActor = NULL;
    switch (Type)
    {
        case JS_ACTOR:
            pActor = new JsActor(m_pObj, m_PrintPri);
            break;
        case PSTATE_ACTOR:
            pActor = new PStateActor(m_PrintPri);
            break;
        case SIMTEMP_ACTOR:
            pActor = new SimTempActor(m_PrintPri);
            break;
        case PCIECHANGE_ACTOR:
            pActor = new PcieSpeedChangeActor(m_PrintPri);
            break;
        case THERMALLIMIT_ACTOR:
            pActor = new ThermalLimitActor(m_PrintPri);
            break;
        case DROOPY_ACTOR:
            pActor = new DroopyActor(m_PrintPri);
            break;
        case CPUPOWER_ACTOR:
            pActor = new CpuPowerActor(m_PrintPri);
            break;
        default:
            Printf(Tee::PriError, "unknown Actor type\n");
            return RC::BAD_PARAMETER;
    }
    if ((OK != (rc = pActor->ParseParams(jsPickers, Params))) ||
        (OK != (rc = pActor->ExtraParamFromJs(jsvPerfPoints))) ||
        (OK != (rc = pActor->CheckParams())))
    {
        delete pActor;
        Printf(Tee::PriError, "Cannot create dispatcher actor\n");
        return rc;
    }
    if (DoesActorExist(pActor->GetActorName()))
    {
        Printf(Tee::PriError, " cannot add actor - name already exists\n");
        delete pActor;
        return RC::SOFTWARE_ERROR;
    }
    m_ActorNameIdxMap[pActor->GetActorName()] = m_Actors.size();
    m_Actors.push_back(pActor);
    if (pActor->GetEnableJson())
    {
        CHECK_RC(pActor->CreateJsonItem(m_pCtx, m_pObj));
    }
    if (IsRunning())
    {
        Printf(m_PrintPri, "starting LwpDispatcher Actor: %s\n", pActor->GetActorName().c_str());
    }
    return OK;
}
//-----------------------------------------------------------------------------
void LwpDispatcher::ClearActors()
{
    if (IsRunning())
    {
        Stop();
    }

    for (size_t i = 0; i < m_Actors.size(); i++)
    {
        delete m_Actors[i];
    }
    m_Actors.clear();
    m_ActorNameIdxMap.clear();
    m_UseTrace = false;
}
//-----------------------------------------------------------------------------
RC LwpDispatcher::PrintCoverage(Tee::Priority PrintPri)
{
    RC rc;
    for (size_t i = 0; i < m_Actors.size(); i++)
    {
        CHECK_RC(m_Actors[i]->PrintCoverage(PrintPri));
    }
    return rc;
}
bool LwpDispatcher::HasAtLeastCoverage(UINT32 Threshold)
{
    size_t i;
    for (i = 0; i < m_Actors.size(); i++)
    {
        if (m_Actors[i]->CallwlateCoverage() < Threshold)
            break;
    }
    return (i == m_Actors.size());
}
//-----------------------------------------------------------------------------
void LwpDispatcher::DumpLastActions()
{
    for (size_t i = 0; i < m_Actors.size(); i++)
    {
        m_Actors[i]->DumpLastActions();
    }
}
//-----------------------------------------------------------------------------

static void RunDispatcherThread(void *Arg)
{
    LwpDispatcher *pDispatch = (LwpDispatcher*)Arg;
    pDispatch->RunThread();
}

RC LwpDispatcher::Run(UINT32 StartSeq, UINT32 StopSeq)
{
    if (StopSeq < StartSeq)
    {
        Printf(Tee::PriError, "StopSeq must be >= StartSeq\n");
        return RC::BAD_PARAMETER;
    }

    if (!HasActors())
    {
        Printf(Tee::PriError, "No LwpDispatcher actors to Run\n");
        return RC::SOFTWARE_ERROR;
    }

    if (IsRunning())
    {
        Printf(Tee::PriError, "LwpDispatcher already running\n");
        return RC::SOFTWARE_ERROR;
    }

    // set the state and kick off
    Cpu::AtomicWrite(&m_IsRunning, 1);
    m_StartIdx  = StartSeq;
    m_StopIdx   = StopSeq;
    m_ThreadId = Tasker::CreateThread(RunDispatcherThread, this, 0, "LwpDispatcher");

    if (m_UseTrace && (m_ActionTrace.size() <= m_StopIdx))
    {
        Printf(Tee::PriNormal, "WARNING: StopId too large. Readjusting\n");
        m_StopIdx = static_cast<UINT32>(m_ActionTrace.size()) - 1;
    }

    Printf(m_PrintPri, "starting LwpDispatcher\n");
    for (UINT32 i = 0; i < m_Actors.size(); i++)
    {
        Printf(m_PrintPri, "\tstarting Actor: %s\n", m_Actors[i]->GetActorName().c_str());
    }
    return OK;
}
//-----------------------------------------------------------------------------
RC LwpDispatcher::Stop()
{
    if (!IsRunning())
    {
        Printf(Tee::PriDebug, "Dispatcher already stopped. No-op\n");
        return OK;
    }

    Printf(m_PrintPri, "stopping LwpDispatcher\n");
    for (UINT32 i = 0; i < m_Actors.size(); i++)
    {
        Printf(m_PrintPri, "\tstopping Actor: %s\n", m_Actors[i]->GetActorName().c_str());
    }
    Cpu::AtomicWrite(&m_IsRunning, 0);
    StickyRC rc = Tasker::Join(m_ThreadId, 2000.0);
    rc = m_RunRc;
    return rc;
}
//-----------------------------------------------------------------------------
RC LwpDispatcher::RunThread()
{
    Tasker::MutexHolder mh(m_RunThreadMutex);
    m_RunRc.Clear();

    bool canDetach = true;
    for (const auto pActor : m_Actors)
    {
        if (!pActor->IsThreadSafe())
        {
            canDetach = false;
            break;
        }
        else if (pActor->GetEnableJson())
        {
            // JSON is not thread safe, but this actor is marked as thread safe
            Printf(Tee::PriError, "Cannot detach %s actor because JSON is enabled",
                   pActor->GetActorName().c_str());
            return RC::SOFTWARE_ERROR;
        }
    }

    unique_ptr<Tasker::DetachThread> pDetach;
    if (canDetach)
    {
        pDetach = make_unique<Tasker::DetachThread>();
        Printf(m_PrintPri, "Running dispatcher in a detached thread\n");
    }

    // restart
    for (size_t i = 0; i < m_Actors.size(); i++)
    {
        m_Actors[i]->Reset();
    }

    UINT64 LastTick = Xp::QueryPerformanceCounter();
    UINT64 PerfFreq = Xp::QueryPerformanceFrequency();
    UINT64 LwrTick = 0;
    UINT32 LwrSeqNumber = m_StartIdx;
    INT64 TimeUsSinceLastChange = 0;
    INT64 TimeUsSinceLastSleep = 0;

    StickyRC rc;
    while (IsRunning())
    {
        FLOAT64 SleepMs;
        if (m_UseTrace)
        {
            SleepMs = m_ActionTrace[LwrSeqNumber].TimeUsBeforeChange / 1000.0;
            Tasker::Sleep(SleepMs);
            LwpActor *pActor = m_ActionTrace[LwrSeqNumber].pActor;
            rc = pActor->RunIndex(m_ActionTrace[LwrSeqNumber].ActionId);
            LwrSeqNumber++;
            if (LwrSeqNumber > m_StopIdx)
            {
                // wrap around when the lwr sequence number > end sequence number
                LwrSeqNumber = m_StartIdx;
                break;
            }
        }
        else
        {
            INT64 NextSleepUs = 0;
            bool Exelwted = false;
            for (size_t i = 0; i < m_Actors.size(); i++)
            {
                LwrTick = Xp::QueryPerformanceCounter();
                TimeUsSinceLastSleep = static_cast<INT64>((LwrTick - LastTick)* 1000000/PerfFreq);
                rc = m_Actors[i]->ExelwteIfTime(TimeUsSinceLastSleep,
                                                TimeUsSinceLastChange,
                                                &Exelwted);
                if (rc != OK)
                    SetStopAtFirstActorError(true);
                if (Exelwted)
                    TimeUsSinceLastChange = 0;
            }

            NextSleepUs = m_Actors[0]->GetTimeUsToNextChange();
            for (size_t i = 0; i < m_Actors.size(); i++)
            {
                // find the earliest one to execute next
                if (NextSleepUs > m_Actors[i]->GetTimeUsToNextChange())
                {
                    NextSleepUs = m_Actors[i]->GetTimeUsToNextChange();
                }
            }
            TimeUsSinceLastChange = NextSleepUs;
            LastTick = Xp::QueryPerformanceCounter();
            SleepMs = NextSleepUs / 1000.0;
            if (OK == rc)
                Tasker::Sleep(SleepMs);
        }

        if (m_StopAtFirstActorError && (OK != rc))
        {
            Printf(Tee::PriLow, "something broke in setting action\n");
            // Allow the test to run until time lapsed.
            // The error will be captured in sticky RC and show up in ::Stop()
            // Bug 3135591
            //break;
        }
    }

    //Cleanup
    for (size_t i = 0; i < m_Actors.size(); i++)
    {
        rc = m_Actors[i]->Cleanup();
    }

    m_RunRc = rc;
    return rc;
}
//-----------------------------------------------------------------------------
RC LwpDispatcher::SetStopAtFirstActorError(bool ToStop)
{
    m_StopAtFirstActorError = ToStop;
    return OK;
}
//-----------------------------------------------------------------------------
void LwpDispatcher::PrintLwlpritAction()
{
    vector<ErrorCausePair> Lwlprits;
    GetLwlpritActions(&Lwlprits);
    for (size_t i = 0; i < Lwlprits.size(); i++)
    {
        Printf(m_PrintPri,
               "Error: %8d  - might be caused by Action %s with Val %d\n",
               Lwlprits[i].Error.RcVal,
               Lwlprits[i].pCause->pActor->GetActorName().c_str(),
               Lwlprits[i].pCause->Actiolwal);
    }
}

// go through a the current list of errors and find the lwlprit actions
void LwpDispatcher::GetLwlpritActions(vector<ErrorCausePair> *pLwlprits)
{
    MASSERT(pLwlprits);
    // 1) Get a temporary copy of errors
    const CirBuffer<Log::ErrorEntry> &ErrorHistory = Log::GetErrorHistory();
    CirBuffer<Log::ErrorEntry>::const_iterator citer = ErrorHistory.begin();

    // 2) for each error in log history:
    for (; citer != ErrorHistory.end(); ++citer)
    {
        ActionLogs::LogEntry *pEntry = NULL;
        for (size_t i = 0; i < m_Actors.size(); i++)
        {
            ActionLogs::LogEntry *pActorEntry;
            pActorEntry = const_cast<ActionLogs::LogEntry*>(
                              m_Actors[i]->GetLog()->SearchErrors((*citer).TimeOfError));
            if (NULL != pActorEntry)
            {
                if (NULL == pEntry || pActorEntry->TimeStamp > pEntry->TimeStamp)
                {
                    pEntry = pActorEntry;
                }
            }
        }
        if (NULL == pEntry)
            continue;

        ErrorCausePair NewPair;
        NewPair.Error  = *citer;
        NewPair.pCause = pEntry;
        pLwlprits->push_back(NewPair);
    }
}
//-----------------------------------------------------------------------------
RC LwpDispatcher::ParseTrace(const JsArray *pJson)
{
    MASSERT(pJson);

    RC rc;
    JavaScriptPtr pJs;

    m_ActionTrace.clear();
    ClearActors();

    for (size_t i = 0; i < pJson->size(); i++)
    {
        JSObject * OneTag;
        CHECK_RC(pJs->FromJsval((*pJson)[i], &OneTag));

        string TagStr;
        CHECK_RC(pJs->GetProperty(OneTag, "__tag__", &TagStr));

        if ("LwDispatcher_NewActor" == TagStr)
        {
            CHECK_RC(ParseNewActor(OneTag));
        }
        else if ("LwDispatcher_Action" == TagStr)
        {
            CHECK_RC(ParseNewAction(OneTag));
        }
    }
    m_UseTrace = true;
    return rc;
}
//-----------------------------------------------------------------------------
RC LwpDispatcher::ParseNewActor(JSObject *pObj)
{
    MASSERT(pObj);
    RC rc;
    LwpActor *pActor = NULL;
    JavaScriptPtr pJs;
    UINT32 Type;
    CHECK_RC(pJs->GetProperty(pObj, "Type", &Type));
    switch (Type)
    {
        case JS_ACTOR:
            pActor = new JsActor(m_pObj, m_PrintPri);
            break;
        case PSTATE_ACTOR:
            pActor = new PStateActor(m_PrintPri);
            break;
        case PCIECHANGE_ACTOR:
            pActor = new PcieSpeedChangeActor(m_PrintPri);
            break;
        case SIMTEMP_ACTOR:
            pActor = new SimTempActor(m_PrintPri);
            break;
        case THERMALLIMIT_ACTOR:
            pActor = new ThermalLimitActor(m_PrintPri);
            break;
        case DROOPY_ACTOR:
            pActor = new DroopyActor(m_PrintPri);
            break;
        case CPUPOWER_ACTOR:
            pActor = new CpuPowerActor(m_PrintPri);
            break;
        default:
            Printf(Tee::PriError, "unknown Actor type\n");
            return RC::BAD_PARAMETER;
    }

    if (OK != (rc = pActor->SetActorPropertiesByJsObj(pObj)))
    {
        delete pActor;
        return rc;
    }

    if (DoesActorExist(pActor->GetActorName()))
    {
        Printf(Tee::PriError, " cannot add actor - name already exists\n");
        delete pActor;
        return RC::SOFTWARE_ERROR;
    }

    m_ActorNameIdxMap[pActor->GetActorName()] = m_Actors.size();
    m_Actors.push_back(pActor);
    CHECK_RC(pActor->CreateJsonItem(m_pCtx, m_pObj));
    if (IsRunning())
    {
        Printf(m_PrintPri, "starting LwpDispatcher Actor: %s\n", pActor->GetActorName().c_str());
    }
    return rc;
}
//-----------------------------------------------------------------------------
RC LwpDispatcher::ParseNewAction(JSObject *pObj)
{
    MASSERT(pObj);
    RC rc;
    JavaScriptPtr pJs;
    string ActorName;
    CHECK_RC(pJs->GetProperty(pObj, "ActorName", &ActorName));
    if (!DoesActorExist(ActorName))
    {
        Printf(Tee::PriError, "Uknown actor: %s\n", ActorName.c_str());
        return RC::SOFTWARE_ERROR;
    }

    const size_t actorIdx = m_ActorNameIdxMap[ActorName];

    TraceData NewTraceAction;
    NewTraceAction.pActor = m_Actors[actorIdx];
    CHECK_RC(pJs->GetProperty(pObj, "ActionId",
                              &NewTraceAction.ActionId));
    CHECK_RC(pJs->GetProperty(pObj, "TimeUsSinceLastChange",
                              &NewTraceAction.TimeUsBeforeChange));
    m_ActionTrace.push_back(NewTraceAction);

    CHECK_RC(m_Actors[actorIdx]->CreateNewActionFromJs(pObj,
                                                       NewTraceAction.ActionId));
    return rc;
}
//-----------------------------------------------------------------------------
bool LwpDispatcher::DoesActorExist(const string &ActorName) const
{
    return (m_ActorNameIdxMap.find(ActorName) != m_ActorNameIdxMap.end());
}
//-----------------------------------------------------------------------------
bool LwpDispatcher::IsRunning()
{
    return Cpu::AtomicRead(&m_IsRunning) == 1;
}
//-----------------------------------------------------------------------------
ActionLogs::ActionLogs()
:
 m_Mutex(NULL)
{
    m_Mutex = Tasker::AllocMutex("ActionLogs::m_Mutex", Tasker::mtxUnchecked);
    MASSERT(m_Mutex);
}

ActionLogs::~ActionLogs()
{
    Tasker::FreeMutex(m_Mutex);
}

void ActionLogs::SetSize(UINT32 Size)
{
    m_CirBuffer.reserve(Size);
}

void ActionLogs::InsertEntry
(
    LwpActor *pActor,
    UINT32 Actiolwal,
    UINT32 ActionId
)
{
    LogEntry NewEntry;
    NewEntry.pActor    = pActor;
    NewEntry.Actiolwal = Actiolwal;
    NewEntry.ActionIdx = ActionId;
    NewEntry.TimeStamp = Xp::QueryPerformanceCounter();

    Tasker::MutexHolder mh(m_Mutex);
    m_CirBuffer.push_back(NewEntry);
}

void ActionLogs::DumpEntries()
{
    Printf(Tee::PriNormal,
           "Name:              Value:      Id:      Time:\n"
           "--------------------------------------------------------------\n");

    Tasker::MutexHolder mh(m_Mutex);
    CirBuffer<LogEntry>::const_iterator citer = m_CirBuffer.begin();
    for (; citer != m_CirBuffer.end(); ++citer)
    {
        Printf(Tee::PriNormal, "%18s 0x%08x  %4d     %lld\n",
               (*citer).pActor->GetActorName().c_str(),
               (*citer).Actiolwal,
               (*citer).ActionIdx,
               (*citer).TimeStamp);
    }
}

const ActionLogs::LogEntry *ActionLogs::SearchErrors(UINT64 TimeOfError)
{
    Tasker::MutexHolder mh(m_Mutex);
    CirBuffer<LogEntry>::const_iterator iter = m_CirBuffer.begin();
    // move iterator until we find an action that happens after the TimeOfError
    // then decrement the iterator :
    // *If end happens first, then that means the TimeOfError happened after the
    // last action in the buffer
    // *If a larger TimeStamp were found first, then the time of error happened
    // in between two Actions
    while (iter != m_CirBuffer.end() && (TimeOfError >= (*iter).TimeStamp))
        ++iter;

    if (iter == m_CirBuffer.begin())
    {
        // no actions in buffer, or no action happened before the error
        return NULL;
    }
    else if (iter == m_CirBuffer.end())
    {
        return &m_CirBuffer.back();
    }

    --iter;
    return &(*iter);
}

//-----------------------------------------------------------------------------
//  JS API
//
static JSBool C_Dispatcher_constructor
(
    JSContext *cx,
    JSObject *obj,
    uintN argc,
    jsval *argv,
    jsval *rval
)
{
    LwpDispatcher *pDispatcher = new LwpDispatcher(cx, obj);
    MASSERT(pDispatcher);

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, obj, "Help", &C_Global_Help, 1, 0);
    return JS_SetPrivate(cx, obj, pDispatcher);
};

static void C_Dispatcher_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    LwpDispatcher *pDispatcher = (LwpDispatcher *)JS_GetPrivate(cx, obj);
    if (pDispatcher)
    {
        delete pDispatcher;
    }
    JS_SetPrivate(cx, obj, nullptr);
};
//-----------------------------------------------------------------------------
static JSClass Dispatcher_class =
{
    "Dispatcher",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_Dispatcher_finalize
};
//-----------------------------------------------------------------------------
static SObject Dispatcher_Object
(
    "Dispatcher",
    Dispatcher_class,
    0,
    0,
    "Dispatcher JS Object",
    C_Dispatcher_constructor
);

JS_SMETHOD_LWSTOM(Dispatcher, ClearActors, 0, "delete the list of existing actors")
{
    LwpDispatcher *pDispatch;
    if ((pDispatch = JS_GET_PRIVATE(LwpDispatcher, pContext, pObject, "Dispatcher")) != 0)
    {
        pDispatch->ClearActors();
        return JS_TRUE;
    }
    return JS_FALSE;
}

JS_SMETHOD_LWSTOM(Dispatcher, DumpLastActions, 0, "print out the last couple of exelwted actions")
{
    LwpDispatcher *pDispatch;
    if ((pDispatch = JS_GET_PRIVATE(LwpDispatcher, pContext, pObject, "Dispatcher")) != 0)
    {
        pDispatch->DumpLastActions();
        return JS_TRUE;
    }
    return JS_FALSE;
}

JS_SMETHOD_LWSTOM(Dispatcher, HasActors, 0, "return whether this dispatcher has actors")
{
    MASSERT(pReturlwalue !=0);
    JavaScriptPtr pJavaScript;
    LwpDispatcher *pDispatch;
    if ((pDispatch = JS_GET_PRIVATE(LwpDispatcher, pContext, pObject, "Dispatcher")) != 0)
    {
        RC rc;
        bool result = pDispatch->HasActors();
        C_CHECK_RC(pJavaScript->ToJsval(result, pReturlwalue));
        return JS_TRUE;
    }
    return JS_FALSE;
}

JS_SMETHOD_LWSTOM(Dispatcher, IsRunning, 0, "return whether this dispatcher is lwrrently running")
{
    MASSERT(pReturlwalue !=0);
    JavaScriptPtr pJavaScript;
    LwpDispatcher *pDispatch;
    if ((pDispatch = JS_GET_PRIVATE(LwpDispatcher, pContext, pObject, "Dispatcher")) != 0)
    {
        RC rc;
        bool result = pDispatch->IsRunning();
        C_CHECK_RC(pJavaScript->ToJsval(result, pReturlwalue));
        return JS_TRUE;
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(Dispatcher, AddActor, 4,
                "Add a new Actor - for more info, run Dispatcher.Manual()")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    JavaScriptPtr pJS;
    UINT32        Type;
    JsArray       jsFpickers;
    string        Params;
    jsval         jsvPerfPoints = JSVAL_NULL;
    if ((NumArguments < 3) || (NumArguments > 4) ||
        (OK != pJS->FromJsval(pArguments[0], &Type)) ||
        (OK != pJS->FromJsval(pArguments[1], &jsFpickers)) ||
        (OK != pJS->FromJsval(pArguments[2], &Params)))
    {
        JS_ReportError(pContext, "Usage: Dispatcher.AddActor(Type, [FPicker Arrays], "
                       "Param, PerfPoints)\n");
        return JS_FALSE;
    }
    if (NumArguments == 4)
        jsvPerfPoints = pArguments[3];
    LwpDispatcher *pDispatch;
    if ((pDispatch = JS_GET_PRIVATE(LwpDispatcher, pContext, pObject, "Dispatcher")) != 0)
    {
        RETURN_RC(pDispatch->AddActor((ActorTypes)Type, jsFpickers, Params, jsvPerfPoints));
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(Dispatcher, Run, 2,
                "Run the dispatcher. This kicks off a new thread. Parameters: \n"
                "StartIdx, EndIdx. Entering StartIdx & EndIdx ONLY if a trace"
                " has been created.")
{
    MASSERT(pArguments && pReturlwalue);

    JavaScriptPtr pJS;
    UINT32        StartSeq = 0;
    UINT32        StopSeq = 0;
    if ((NumArguments == 2) &&
        ((OK != pJS->FromJsval(pArguments[0], &StartSeq)) ||
         (OK != pJS->FromJsval(pArguments[1], &StopSeq))))
    {
        JS_ReportError(pContext, "Usage: Dispatcher.Run(StartIdx(optional), StopIdx(Optional))\n");
        return JS_FALSE;
    }

    LwpDispatcher *pDispatch;
    if ((pDispatch = JS_GET_PRIVATE(LwpDispatcher, pContext, pObject, "Dispatcher")) != 0)
    {
        RETURN_RC(pDispatch->Run(StartSeq, StopSeq));
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(Dispatcher, Stop, 0,
                "Stop the dispatcher. This waits for the dispatcher thread to complete")
{
    MASSERT(pReturlwalue !=0);

    LwpDispatcher *pDispatch;
    if ((pDispatch = JS_GET_PRIVATE(LwpDispatcher, pContext, pObject, "Dispatcher")) != 0)
    {
        RETURN_RC(pDispatch->Stop());
    }
    return JS_FALSE;
}

JS_SMETHOD_LWSTOM(Dispatcher, PrintLwlpritAction, 0,
                  "Print out the list of errors and the actions that could caused the problem")
{
    LwpDispatcher *pDispatch;
    if ((pDispatch = JS_GET_PRIVATE(LwpDispatcher, pContext, pObject, "Dispatcher")) != 0)
    {
        pDispatch->PrintLwlpritAction();
        return JS_TRUE;
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(Dispatcher, GetLwlpritActions, 1,
                "Get an array of error and the corresponding error time and cause")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    JavaScriptPtr pJS;
    JSObject     *pOutArray;

    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &pOutArray)))
    {
        JS_ReportError(pContext, "Usage: Dispatcher.GetLwlpritAction(OutArray)\n");
        return JS_FALSE;
    }

    LwpDispatcher *pDispatch;
    if ((pDispatch = JS_GET_PRIVATE(LwpDispatcher, pContext, pObject, "Dispatcher")) != 0)
    {
        RC rc;
        vector<LwpDispatcher::ErrorCausePair> Lwlprits;
        pDispatch->GetLwlpritActions(&Lwlprits);
        for (size_t i = 0; i < Lwlprits.size(); i++)
        {
            JsArray JsErrorEntry;
            jsval jsvTemp;
            C_CHECK_RC(pJS->ToJsval(Lwlprits[i].Error.RcVal, &jsvTemp));
            JsErrorEntry.push_back(jsvTemp);
            C_CHECK_RC(pJS->ToJsval(Lwlprits[i].pCause->pActor->GetActorName(), &jsvTemp));
            JsErrorEntry.push_back(jsvTemp);
            C_CHECK_RC(pJS->ToJsval(Lwlprits[i].pCause->Actiolwal, &jsvTemp));
            JsErrorEntry.push_back(jsvTemp);
            C_CHECK_RC(pJS->ToJsval(Lwlprits[i].pCause->ActionIdx, &jsvTemp));
            JsErrorEntry.push_back(jsvTemp);
            C_CHECK_RC(pJS->ToJsval(Lwlprits[i].pCause->TimeStamp, &jsvTemp));
            JsErrorEntry.push_back(jsvTemp);
            C_CHECK_RC(pJS->SetElement(pOutArray, static_cast<INT32>(i), &JsErrorEntry));
        }
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(Dispatcher, ParseTrace, 1,
               "Parse a trace of actors and actions - JSON format")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    JavaScriptPtr pJS;
    JsArray       InputArray;

    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &InputArray)))
    {
        JS_ReportError(pContext, "Usage: Dispatcher.ParseTrace(JsonObject)\n");
        return JS_FALSE;
    }
    RC rc;
    LwpDispatcher *pDispatch;
    if ((pDispatch = JS_GET_PRIVATE(LwpDispatcher, pContext, pObject, "Dispatcher")) != 0)
    {
        C_CHECK_RC(pDispatch->ParseTrace(&InputArray));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(Dispatcher, PrintCoverage, 1,
                  "Print out the coverage statistics from every actor")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    JavaScriptPtr pJS;
    UINT32 pri = static_cast<UINT32>(Tee::PriNormal);

    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &pri)))
    {
        JS_ReportError(pContext,
                       "Usage: Dispatcher.PrintCoverage(PrintPriority)\n");
        return JS_FALSE;
    }

    RC rc;
    LwpDispatcher *pDispatch;
    if ((pDispatch = JS_GET_PRIVATE(LwpDispatcher,
                                    pContext, pObject, "Dispatcher")) != 0)
    {
        C_CHECK_RC(pDispatch->PrintCoverage(static_cast<Tee::Priority>(pri)));
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(Dispatcher,
                  HasAtLeastCoverage,
                  1,
                  "Has specified percentage of gpcclk values been tested")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    const char usage[] = "Usage: Dispatch.HasAtLeastCoverage(Threshold)";
    if (NumArguments != 1)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    JavaScriptPtr pJavaScript;
    *pReturlwalue = JSVAL_NULL;
    UINT32 Threshold = 0;
    if (OK != pJavaScript->FromJsval(pArguments[0], &Threshold))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    RC rc;
    bool IsCovered = false;
    LwpDispatcher *pDispatch;
    if ((pDispatch = JS_GET_PRIVATE(LwpDispatcher,
                                    pContext, pObject, "Dispatcher")) != 0)
    {
        IsCovered = pDispatch->HasAtLeastCoverage(Threshold);
        C_CHECK_RC(pJavaScript->ToJsval(IsCovered, pReturlwalue));

        return JS_TRUE;
    }
    return JS_FALSE;
}

P_(Dispatcher_SetStopAtFirstActorError);
static SProperty Dispatcher_StopAtFirstError
(
    Dispatcher_Object,
    "StopAtFirstError",
    0,
    0,
    0,
    Dispatcher_SetStopAtFirstActorError,
    0,
    "When set to true, Dispatcher will Stop when an Actor reports an error"
);
P_(Dispatcher_SetStopAtFirstActorError)
{
    MASSERT(pContext && pValue);
    JavaScriptPtr pJS;
    LwpDispatcher *pDispatch;
    if ((pDispatch = JS_GET_PRIVATE(LwpDispatcher, pContext, pObject, "Dispatcher")) != 0)
    {
        bool RetVal;
        if (OK != pJS->FromJsval(*pValue, &RetVal))
        {
            JS_ReportError(pContext, "Failed to colwert SetStopAtFirstActorError");
            return JS_FALSE;
        }
        pDispatch->SetStopAtFirstActorError(RetVal);
        return JS_TRUE;
    }
    return JS_FALSE;
}

// if there's another one of these, move this to a macro
P_(Dispatcher_SetPrintPriority);
static SProperty Dispatcher_PrintPriority
(
    Dispatcher_Object,
    "PrintPriority",
    0,
    0,
    0,
    Dispatcher_SetPrintPriority,
    0,
    "Set the print priority for dispatcher and actions"
);
P_(Dispatcher_SetPrintPriority)
{
    MASSERT(pContext && pValue);
    JavaScriptPtr pJS;
    LwpDispatcher *pDispatch;
    if ((pDispatch = JS_GET_PRIVATE(LwpDispatcher, pContext, pObject, "Dispatcher")) != 0)
    {
        UINT32 Pri;
        if ((OK != pJS->FromJsval(*pValue, &Pri)) ||
            (Pri > Tee::PriAlways))
        {
            JS_ReportError(pContext, "Failed to colwert PrintPriority");
            return JS_FALSE;
        }
        pDispatch->SetPrintPriority(static_cast<Tee::Priority>(Pri));
        return JS_TRUE;
    }
    return JS_FALSE;
}

JS_SMETHOD_LWSTOM(Dispatcher,
                  Manual,
                  0,
                  "More Info help for Dispatcher and each actor.")
{
    Printf(Tee::PriNormal,
           "---------------Dispatcher Manual-----------------\n"
           "What is Dispatcher: This is MODS's version of LwPunish. It allows users to\n"
           "do background bashing while running tests in the foreground.\n"
           "\n\nDefinitions:\n"
           " Actor: an actor is an object that can change the state of the GPU or its surrounding."
           "\n"
           "       For example: PState, clock(s), voltage, ELPG setting, PEX swing level, ASPM, \n"
           "       memory settings. \n"
           " Action: User can specify (using fancy pickers) what value the actor should have, as\n"
           "         well as how often the values change. Each exelwtion of the actor is an \n"
           "         'Action'.\n"
           "\n"
           "How to create an dispatcher & actor:\n"
           " Example:\n"
           " g_Dispatcher = new Dispatcher();\n"
           " Disp.AddActor(Dispatcher.JS_ACTOR, \n"
           "               [[\"rand\", [2, 1000, 2000], [8, 10, 20]], // fancy picker for "
           "values to set\n"
           "                [\"rand\", [3, 1000, 2000], [7, 3000, 3500]]], // fancy picker for "
           "when the actions should kick in\n"
           "               \"NumSteps:20|JsFunc:MyJsFunc1|Name:ActorNum1\"\n"
           "\n"
           "* Each dispatcher can have multiple actors"
           "* User can then call g_Dispatcher.Run() to kick off the Actors\n"
           "  and call g_Dispatcher.Stop() to halt.\n"
           "* When 'Run' is exelwted, a thread is kicked off in the background. It circles"
           " through\n"
           "  the list of Actors and exelwtes Actions\n"
           "* For details on fancy picker syntax, please see: \n"
           "https://wiki.lwpu.com/engwiki/index.php/MODS/Projects/MODS_LwPunish#Fancy_Picker \n"
           "* For details on each types of actor, please go to \n"
           " https://wiki.lwpu.com/engwiki/index.php/MODS/Projects/MODS_LwPunish \n"
           "\n"
           "* JS_ACTOR is the most generic type of actor since it allows user to\n"
           "  define the actor arbitraily; however, there could be performance hits.\n"
           "\n\n"
           "Error Detection:\n"
           "The dispatcher attempts to keep track of when errors are detected. Each time an"
           " action\n"
           "is exelwted, a time stamp is recorded. When a test detects an error, MODS can log"
           " the\n"
           "time of the failure. By search through a cirlwlar buffer of the past exelwted"
           " actions, \n"
           "the dispatcher can 'guess' which was the sequence of 'actions' that caused the"
           " error\n"
           " To ilwoke Error Detection: \n"
           " Dispatcher.PrintLwlpritAction()\n"
           "\n\n"
           "Please type Help('Dispatcher') To get more details for each functions");
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
//! \brief: constants related to dispatcher
//!
JS_CLASS(DispatcherConst);
static SObject DispatcherConst_Object
(
    "DispatcherConst",
    DispatcherConstClass,
    0,
    0,
    "DispatcherConst JS Object"
);

PROP_CONST(DispatcherConst, JS_ACTOR,     JS_ACTOR);
PROP_CONST(DispatcherConst, PSTATE_ACTOR, PSTATE_ACTOR);
PROP_CONST(DispatcherConst, SIMTEMP_ACTOR, SIMTEMP_ACTOR);
PROP_CONST(DispatcherConst, PCIECHANGE_ACTOR, PCIECHANGE_ACTOR);
PROP_CONST(DispatcherConst, THERMALLIMIT_ACTOR, THERMALLIMIT_ACTOR);
PROP_CONST(DispatcherConst, DROOPY_ACTOR, DROOPY_ACTOR);
PROP_CONST(DispatcherConst, CPUPOWER_ACTOR, CPUPOWER_ACTOR);
// this is for ThermalLimitCycle
PROP_CONST(DispatcherConst, RESTORE_LIMIT_VALUE, ThermalLimitActor::s_RestoreLimitValue);
// this is for GetLwlpritActions
PROP_CONST(DispatcherConst, ERROR_RC_IDX,    0);
PROP_CONST(DispatcherConst, ACTOR_NAME_IDX,  1);
PROP_CONST(DispatcherConst, ACTION_VAL_IDX,  2);
PROP_CONST(DispatcherConst, ACTION_NUM_IDX,  3);
PROP_CONST(DispatcherConst, ACTION_TIME_IDX, 4);

// PStateActor variants
PROP_CONST(DispatcherConst, PerfJumps,    0);
PROP_CONST(DispatcherConst, PerfSweep,    1);
PROP_CONST(DispatcherConst, LwvddStress,  2);
