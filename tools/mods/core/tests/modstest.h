/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2022 by LWPU Corporation. All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_MODSTEST_H
#define INCLUDED_MODSTEST_H

#include "core/include/cpu.h"
#include "core/include/rc.h"
#include "core/include/script.h"
#include "core/include/device.h"
#include "core/include/fpicker.h"
#include "core/include/setget.h"
#include <string>
#include "core/include/tee.h"
#include "core/include/callback.h"
#include "core/include/help.h"
#include <regex>

class JsonItem;

/**
 * All MODS tests should inherit from the ModsTest class.
 *
 * A test is an object -- it can be instantiated any number of times to construct
 * a complicated test scenario.  All tests are expected to provide, at minimum,
 * "Setup", "Run", and "Cleanup" phases.  Expensive test startup/shutdown tasks
 * should preferably live in Setup and Cleanup, so that the minimum amount of CPU
 * time is required to actually Run the test.  This makes it easier to achieve
 * conlwrrency between tests, since you can run each test's Setup function before
 * calling any test's Run function.
 *
 * This is optional, but many tests may choose to allow Run to be called any
 * number of times, not just once.  This provides a straightforward way for the
 * test to be run in a loop, if so desired.
 *
 * Test success vs. failure is indicated via the return codes from Setup and Run.
 * Cleanup is always expected to succeed.
 */
class ModsTest
{
public:
    ModsTest();
    virtual ~ModsTest();

    // Non-copyable
    ModsTest(const ModsTest&) = delete;
    ModsTest& operator=(const ModsTest&) = delete;

    SETGET_PROP(Name, string);

    // Colwenience function so we can use standard JS macros
    bool GetIsSupported() { return IsSupportedWrapper(); }
    //! All tests are expected to provide an IsSupported function to determine
    //! whether the test is supported on the current hardware, OS, etc.
    virtual bool IsSupported(){ return false; };

    // A test needs to explicitly whitelist itself to run on virtual functions.
    virtual bool IsSupportedVf() { return false; };

    virtual RC Setup() = 0;
    virtual RC Run() = 0;
    virtual RC Cleanup() = 0;

    virtual RC InitFromJs();
    RC TestHelpWrapper(JSContext *pContext);
    virtual void PrintJsProperties(Tee::Priority pri);
    virtual void TestHelp(JSContext *pCtx, regex *pRegex);
    virtual void PropertyHelp(JSContext *pCtx, regex *pRegex);

    virtual RC SetJavaScriptObject(JSContext * cx, JSObject *obj);
    virtual JSObject * GetJSObject();

    //! Set up an extra objects to be objects of an ModsTest.
    virtual RC AddExtraObjects(JSContext *cx, JSObject *obj) {return OK;}

    virtual UINT32 GetLoop() const;
    virtual UINT32 GetPick(UINT32 idx);
    virtual double FGetPick(UINT32 idx);
    virtual RC     GetPicker(UINT32 idx, jsval * v);
    virtual RC     SetPicker(UINT32 idx, jsval v);
    virtual RC     SetDefaultsForPicker(UINT32 idx);
    virtual UINT32 GetNumPickers() {return m_pPickers->GetNumPickers();}
    virtual RC     SetDefaultPickers(UINT32 first, UINT32 last);
    virtual RC     SetupJsonItems();
    bool IsSupportedWrapper();
    RC SetupWrapper();
    RC RunWrapper();
    RC CleanupWrapper();
    bool GetSetupDone()  { return m_SetupDone; }
    void * GetMainFrameId() { return &m_MainErrCounterDummy; }
    void * GetRunFrameId() { return &m_RunErrCounterDummy; }

    SETGET_PROP(SetupSyncRequired, bool);
    SETGET_PROP(DeviceSync, bool);
    GET_PROP(EdcTol, UINT64);
    SET_PROP_LWSTOM(EdcTol, UINT64);
    GET_PROP(EdcTotalTol, UINT64);
    SET_PROP_LWSTOM(EdcTotalTol, UINT64);
    GET_PROP(EdcRdTol, UINT64);
    SET_PROP_LWSTOM(EdcRdTol, UINT64);
    GET_PROP(EdcWrTol, UINT64);
    SET_PROP_LWSTOM(EdcWrTol, UINT64);
    GET_PROP(EdcTotalPerMinTol, UINT64);
    SET_PROP_LWSTOM(EdcTotalPerMinTol, UINT64);
    SETGET_PROP_VAR(HeartBeatPeriodSec, UINT32, s_HeartBeatPeriodSec);
    SETGET_PROP(IgnoreErrCountsPreRun, bool);

    enum CallbackIdx
    {
        PRE_SETUP,
        PRE_RUN,
        POST_RUN,
        POST_CLEANUP,
        IS_SUPPORTED,
        PRE_PEX_CHECK,
        PEX_ERROR,
        MISC_A,
        MISC_B,
        ISM_EXP,
        END_LOOP,

        NUM_CALLBACK_TYPE // must be last
    };

    Callbacks *GetCallbacks(CallbackIdx cbId);
    static Callbacks *GetGlobalCallbacks(CallbackIdx cbId);

    JsonItem * GetJsonEnter ();
    JsonItem * GetJsonExit ();

    virtual RC EndLoop();
    enum TestType
    {
        LWDA_MEM_TEST,
        LWDA_STREAM_TEST,
        GPU_MEM_TEST,
        GPU_TEST,
        MEM_TEST,
        MODS_TEST,
        PEX_TEST,
        RM_TEST
    };
    virtual bool IsTestType(TestType tt);

    static size_t GetNumDoneSetup()
    {
        return static_cast<size_t>(Cpu::AtomicRead(&s_DoneSetup));
    }

    static size_t GetNumDoneRun()
    {
        return static_cast<size_t>(Cpu::AtomicRead(&s_DoneRun));
    }

    static RC PrintProgressInit(UINT64 maxIterations, const string& progressDesc = "");
    static RC PrintProgressUpdate(UINT64 lwrrentIteration);
    static RC PrintErrorUpdate(UINT64 lwrrentIteration, RC errorRc);
    static RC EndPrintProgress();

protected:
    RC                          CreateFancyPickerArray(UINT32 NumPickers);
    FancyPicker::FpContext *    GetFpContext();
    FancyPickerArray *          GetFancyPickerArray();

    RC FireCallbacks(CallbackIdx id, Callbacks::Flags flags,
                     const CallbackArguments &args, const char *displayName);
    bool FireCallbacksBool(CallbackIdx id, Callbacks::Flags flags,
                           const CallbackArguments &args,
                           const char *displayName);
    RC SetPerformanceMetric(const char* name, FLOAT64 value);
    RC SetCoverageMetric(const char* name, FLOAT64 values);
    RC SetMetric(const char* metric, const char* name, FLOAT64 value);

    struct RunWrapperContext
    {
        bool DidSetup;
        bool DidPreRun;
        bool DidLogEnter;
        bool DidMainErrCounterEnter;
        bool DidRunErrCounterEnter;
        StickyRC  FirstRc;

        RunWrapperContext()
        :   DidSetup(false)
           ,DidPreRun(false)
           ,DidLogEnter(false)
           ,DidMainErrCounterEnter(false)
           ,DidRunErrCounterEnter(false)
        {
        }
    };

    void RunWrapperFirstHalf (RunWrapperContext * pCtx);
    void RunWrapperSecondHalf (RunWrapperContext * pCtx);

private:
    RC CheckAndSetEdcArg(UINT64 *pEdcArg, UINT64 val, bool shouldOverrideDefaultTol);

    string                  m_Name;
    FancyPicker::FpContext  m_FpCtx;
    FancyPickerArray *      m_pPickers;

    //! Per-test callback list.
    Callbacks               m_CallbackList[NUM_CALLBACK_TYPE];

    //! Global shared callback list.
    static Callbacks        s_CallbackList[NUM_CALLBACK_TYPE];

    //! Set true when Setup returns OK.
    //! Set false when Cleanup returns.
    bool m_SetupDone;

    //! Used, in addition to m_SetupDone,
    //! to control the calling of Setup and Cleanup.
    bool m_SetupIlwoked;

    //! Set to true if the forground & background tests must have their
    //! Setup() functions complete before either test starts the Run() function.
    bool m_SetupSyncRequired;

    //! Set to true if all conlwrrent devices must have their tests synchronized
    //! Setup(), Run() and Cleanup() will execute simultaneously for all conlwrrent devices
    bool m_DeviceSync;

    JsonItem * m_pJsonEnter;
    JsonItem * m_pJsonExit;

    // Tolerance set by -testarg for each type of EDC CRC error
    // incluidng total, read, write
    UINT64 m_EdcTol;
    // Tolerance set by -testarg for number of overall EDC CRC errors of this test.
    UINT64 m_EdcTotalTol;
    // Tolerance set by -testarg for number of read EDC CRC errors of this test.
    UINT64 m_EdcRdTol;
    // Tolerance set by -testarg for number of write EDC CRC errors of this test.
    UINT64 m_EdcWrTol;
    // Tolerance set by -testarg for number of overall EDC CRC errors per minute of this test.
    UINT64 m_EdcTotalPerMinTol;
    static UINT32 s_HeartBeatPeriodSec;

    // If true, will ignore error counter errors before Run().
    bool m_IgnoreErrCountsPreRun;

    // Dummy variables for providing unique frame ids when initializing ErrCounters
    char m_MainErrCounterDummy;
    char m_RunErrCounterDummy;

    static INT32 s_DoneSetup;
    static INT32 s_DoneRun;
};

extern SObject ModsTest_Object;

#endif // INCLUDED_MODSTEST_H
