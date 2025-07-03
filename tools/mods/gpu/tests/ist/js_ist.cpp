/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "ist.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/utility.h"   // For Tokenizer

using std::string;
using std::vector;

//--------------------------------------------------------------------
// \param[in] paths Semicolon-separated list of paths
static JSBool C_IstFlow_constructor
(
    JSContext* pContext,
    JSObject*  pObject,
    uintN      NumArguments,
    jsval*     pArguments,
    jsval*     pReturlwalue
)
{
     const int minNumArgs = 1;
     const int maxNumArgs = 1;
     STEST_HEADER(minNumArgs, maxNumArgs, "Usage: IstFlow(testImageArgs)");

    // Retrieve input variables
    using IstClass = Ist::IstFlow;
    vector<IstClass::TestArgs> allTestArgs;
    {
        JavaScriptPtr pJs;

        // The macro above guarantees that there is exactly one arg,
        // so no need to double check. We also know it should be an array
        JsArray allArgs;
        if (RC::OK != pJs->FromJsval(pArguments[0], &allArgs))
        {
            pJs->Throw(pContext, RC::BAD_PARAMETER, usage);
            return JS_FALSE;
        }

        // For each element of the array passed in
        for (const auto& imageArgs : allArgs)
        {
            // Get the element (as an object)
            JSObject* args;
            if (RC::OK != pJs->FromJsval(imageArgs, &args))
            {
                pJs->Throw(pContext, RC::BAD_PARAMETER, usage);
                return JS_FALSE;
            }

            // There must be a path
            string path;
            if (RC::OK != pJs->GetProperty(args, "path", &path))
            {
                pJs->Throw(pContext, RC::BAD_PARAMETER, usage);
                return JS_FALSE;
            }
            allTestArgs.emplace_back(std::move(path));
            IstClass::TestArgs& lwrTestArgs = allTestArgs.back();

            // There may be a vector of test numbers (sequences)
            vector<UINT32> testNums;
            if (RC::OK == pJs->GetProperty(args, "sequences", &testNums))
            {
                lwrTestArgs.testNumbers = std::move(testNums);
            }

            // There may be a request to loop
            uint16_t numLoops;
            if (RC::OK == pJs->GetProperty(args, "loops", &numLoops))
            {
                lwrTestArgs.numLoops = numLoops;
            }
        }
    }

    auto pIst = make_unique<IstClass>(allTestArgs);

    // Add the .Help() to this JSObject
    JS_DefineFunction(pContext, pObject, "Help", &C_Global_Help, 1, 0);

    // Tie the Ist wrapper to the new object
    return JS_SetPrivate(pContext, pObject, pIst.release());
}

//--------------------------------------------------------------------
// Boilerplate
// (must be before exposed functions)

static void C_IstFlow_finalize
(
    JSContext* ctx,
    JSObject*  obj
)
{
    MASSERT(ctx != 0);
    MASSERT(obj != 0);

    Ist::IstFlow* const pIst = static_cast<Ist::IstFlow*>(
            JS_GetPrivate(ctx, obj));

    if (pIst)
    {
        delete pIst;
    }
};

static JSClass IstFlow_class =
{
    "IstFlow",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_IstFlow_finalize
};

static SObject IstFlow_Object
(
    "IstFlow",
    IstFlow_class,
    nullptr,
    0U,
    "IstFlow JS Object",
    C_IstFlow_constructor
);

//--------------------------------------------------------------------
// IST Exposed Functions

namespace Ist
{
    JS_STEST_BIND_NO_ARGS(IstFlow, Initialize,  "Enumerate and map GPU");
    JS_STEST_BIND_NO_ARGS(IstFlow, CheckHwCompatibility, "Check if test can run on this HW");
    JS_STEST_BIND_NO_ARGS(IstFlow, Setup,       "Initialize the MATHS-ACCESS library");
    JS_STEST_BIND_NO_ARGS(IstFlow, Run,         "Run all test images");
    JS_STEST_BIND_NO_ARGS(IstFlow, Cleanup,     "Deallocate memory allocated in/since Setup()");
    // Getters/Setters
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetTestTimeoutMs, timeoutMs, FLOAT64,
                                 "Timeout in ms for each test sequence");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetPreIstScript, path, string,
                                 "Script to init IST mode");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetUserScript, path, string,
                                 "User provided script path");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetUserScriptArgs, args, string,
                                 "Arguments of user provided script");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetRebootWithZpi, reboot, bool,
                                 "Try to use ZPI to reboot the chip");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetImageDumpFile, filename, string,
                                 "File to dump result image");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetCtrlStsDumpFile, filename, string,
                                 "File to dump controller status bits");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetTestRstDumpFile, filename, string,
                                 "File to dump controller test result");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetIstType, istype, string,
                                 "Type of IST test to be run");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetFrequencyMhz, frequencyMhz, UINT32,
                                 "GPC clk frequency in Mhz");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetRepeat, rep, UINT32,
                                 "The number of time to repeat each predictive IST");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetVoltageOverrideMvAry, voltageMv, std::vector<FLOAT32>,
                                 "Multiple Target voltages for predictive IST input as a string splited by ,");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetVoltageOffsetMv, offsetMv, FLOAT32,
                                 "Voltage offset added to base voltage for predictive IST");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetFbBroken, fbBroken, bool,
                                 "Flag to indicate a broken framebuffer");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetZpiScript, path, string,
                                 "Path to external ZPI script");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetOffsetShmooDown, offsetShmooDown, bool,
                                 "Flag to start from high voltage to low for Predictive IST");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetSeleneSystem, systemType, bool,
                                 "Indicates a selene system under test");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetDeepL1State, deepL1State, bool,
                                 "Flag to indicate whether to enter DeepL1 state");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetFsArgs, fsArgs, string,
                                 "Floorsweep arguments input from command line");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetCheckDefectiveFuse, checkDefectiveFuse, bool,
                                 "Check the errors reported by MAL against the DEFECTIVE fuses (default case is DISABLE fuses)");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetPrintPerSeqFsTagErrCnt, printPerSeqFsTagErrCnt, bool,
                                "Print the per Sequence per FStag error count table (deault case does not print it)");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetIgnoreMbist, ignoreMbist, bool,
                                "MODS to ingore the MBIST sequences test results");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetFilterMbist, filterMbist, bool,
                                "MODS to filter the MBIST sequences test results");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetNumRetryReadTemp, numRetryReadTemp, UINT32,
                                "Number of Retries allowed when talking with MLW to read temperature.");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetPrintTimes, printTimes, bool,
                                 "Flag to print the elapsed time in the test.");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetTestSeqRCSTimeoutMs, timeoutMs, UINT32,
                                 "Timeout in ms to obtain a test sequence result");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetInitTestSeqRCSTimeoutMs, timeoutMs, UINT32,
                                 "Timeout in ms to obtain the init test sequence result");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetSaveTestProgramResult, testResult, bool,
                                 "Flag to save test result into a file to be processed by FA offline tool.");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetIOMMUEnabled, iommuEnabled, bool,
                                 "Performs the proper mapping on IOMM enabled systems");

    // ZPI delays 
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetDelayAfterPwrDisableMs, delayAfterPwrDisableMs, UINT32,
                                 "Delay after power disabled");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetDelayBeforeOOBEntryMs, delayBeforeOOBEntryMs, UINT32,
                                 "Delay before OOB entry");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetDelayAfterOOBExitMs, delayAfterOOBExitMs, UINT32,
                                 "Delay after OOB exit");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetLinkPollTimeoutMs, linkPollTimeoutMs, UINT32,
                                 "Timeout to allow link to change states");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetDelayBeforeRescanMs, delayBeforeRescanMs, UINT32,
                                 "Delay before rescanning PCI");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetRescanRetryDelayMs, rescanRetryDelayMs, UINT32,
                                 "Delay retrying rescanning PCI");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetDelayAfterPexRstAssertMs, delayAfterPexRstAssertMs,
                                 UINT32, "Delay after PEX_RESET is asserted");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetDelayBeforePexDeassertMs, delayBeforePexDeAssertMs,
                                 UINT32, "Delay before PEX_RESET is de-asserted");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetDelayAfterExitZpiMs, delayAfterExitZpiMs,
                                 UINT32, "Delay after exiting ZPI routine");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetSkipJTAGCommonInit, skipcommon, bool,
                                 "Flag to skip the exelwtion of JTAG common init image.");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetSkipJTAGShutdown, skipshutdown, bool,
                                 "Flag to skip the exelwtion of JTAG shutdown image.");
    JS_SMETHOD_VOID_BIND_ONE_ARG(IstFlow, SetExtraMemoryBlocks, blocks, UINT32,
                                 "Extra memory added to the ones allocated by MODS.");


    JS_SMETHOD_LWSTOM(IstFlow, SetJtagProgInfos, 1, "Set the Jtag programming info")
    {
        STEST_HEADER(1, 1, "Usage: SetJtagProgInfos(g_jtagProgInfos)");
        std::vector<IstFlow::JtagProgArgs>jtagProgInfos;
        RC rc;
        JavaScriptPtr pJs;
        JsArray jtagProgInfoList;

        if (pJs->FromJsval(pArguments[0], &jtagProgInfoList) != OK)
        {
            JS_ReportError(pContext, "Invalid value for Jtag arguments %s\n", usage);
            return JS_FALSE;
        }

        for (UINT32 i = 0; i < jtagProgInfoList.size(); i++)
        {
            IstFlow::JtagProgArgs jtagProgInfo;
            JSObject *jtagProgInfoObj;
            CHECK_RC(pJs->FromJsval(jtagProgInfoList[i], &jtagProgInfoObj));
            CHECK_RC(pJs->GetProperty(jtagProgInfoObj, "jtagType", &jtagProgInfo.jtagType));
            CHECK_RC(pJs->GetProperty(jtagProgInfoObj, "jtagName", &jtagProgInfo.jtagName));
            CHECK_RC(pJs->GetProperty(jtagProgInfoObj, "width", &jtagProgInfo.width));
            CHECK_RC(pJs->GetProperty(jtagProgInfoObj, "value", &jtagProgInfo.value));
            jtagProgInfos.push_back(jtagProgInfo);
        }
        IstFlow *pObj = (IstFlow *)JS_GetPrivate(pContext, pObject);
        pObj->SetJtagProgInfos(jtagProgInfos);

        return JS_TRUE;
    }

    JS_SMETHOD_LWSTOM(IstFlow, SetCvbInfo, 1, "Set the CVB info")
    {
        STEST_HEADER(1, 1, "Usage: SetCvbInfo(g_CvbInfo)");
        IstFlow::CvbInfoArgs cvbInfo;
        RC rc;
        JavaScriptPtr pJs;
        JSObject *cvbInfoObj;

        if (pJs->FromJsval(pArguments[0], &cvbInfoObj) != OK)
        {
            JS_ReportError(pContext, "Invalid value for Extract CVB info Arguments %s\n", usage);
            return JS_FALSE;
        }

        CHECK_RC(pJs->GetProperty(cvbInfoObj, "testPattern", &cvbInfo.testPattern));
        CHECK_RC(pJs->GetProperty(cvbInfoObj, "voltageV", &cvbInfo.voltageV));
        IstFlow *pObj = (IstFlow *)JS_GetPrivate(pContext, pObject);
        pObj->SetCvbInfo(cvbInfo);
        return JS_TRUE;
    }

    JS_SMETHOD_LWSTOM(IstFlow, SetVoltageSchmooInfos, 1, "Set voltage schmoo parameters")
    {
        STEST_HEADER(1, 1, "Usage: SetVoltageSchmooInfo,(g_VoltageSchmooInfo)");
        std::vector<IstFlow::VoltageSchmoo> voltageSchmooInfos;
        RC rc;
        JavaScriptPtr pJs;
        JsArray voltageSchmooInfoList;

        if (pJs->FromJsval(pArguments[0], &voltageSchmooInfoList) != OK)
        {
            JS_ReportError(pContext, "Invalid value for voltage schmoo %s\n", usage);
            return JS_FALSE;
        }
        for (UINT32 i = 0; i < voltageSchmooInfoList.size(); i++)
        {
            IstFlow::VoltageSchmoo voltageSchmooInfo;
            JSObject *voltageSchmooObj;
            CHECK_RC(pJs->FromJsval(voltageSchmooInfoList[i], &voltageSchmooObj));
            CHECK_RC(pJs->GetProperty(voltageSchmooObj, "domain", &voltageSchmooInfo.domain));
            CHECK_RC(pJs->GetProperty(voltageSchmooObj, "startValue",
                        &voltageSchmooInfo.startValue));
            CHECK_RC(pJs->GetProperty(voltageSchmooObj, "endValue",
                        &voltageSchmooInfo.endValue));
            CHECK_RC(pJs->GetProperty(voltageSchmooObj, "step", &voltageSchmooInfo.step));
            voltageSchmooInfos.push_back(voltageSchmooInfo);
        }
        IstFlow *pObj = (IstFlow *)JS_GetPrivate(pContext, pObject);
        pObj->SetVoltageSchmooInfos(voltageSchmooInfos);

        return JS_TRUE;
    }

    JS_SMETHOD_LWSTOM(IstFlow, SetFrequencySchmooInfos, 1, "Set frequency schmoo parameters")
    {
        STEST_HEADER(1, 1, "Usage: SetFrequencySchmooInfo,(g_FrequencySchmooInfo)");
        std::vector<IstFlow::FrequencySchmoo> frequencySchmooInfos;
        RC rc;
        JavaScriptPtr pJs;
        JsArray frequencySchmooInfoList;

        if (pJs->FromJsval(pArguments[0], &frequencySchmooInfoList) != OK)
        {
            JS_ReportError(pContext, "Invalid value for frequency schmoo %s\n", usage);
            return JS_FALSE;
        }
        for (UINT32 i = 0; i < frequencySchmooInfoList.size(); i++)
        {
            IstFlow::FrequencySchmoo frequencySchmooInfo;
            JSObject *frequencySchmooObj;
            CHECK_RC(pJs->FromJsval(frequencySchmooInfoList[i], &frequencySchmooObj));
            CHECK_RC(pJs->GetProperty(frequencySchmooObj, "pllName",
                        &frequencySchmooInfo.pllName));
            CHECK_RC(pJs->GetProperty(frequencySchmooObj, "startValue",
                        &frequencySchmooInfo.startValue));
            CHECK_RC(pJs->GetProperty(frequencySchmooObj, "endValue",
                        &frequencySchmooInfo.endValue));
            CHECK_RC(pJs->GetProperty(frequencySchmooObj, "step", &frequencySchmooInfo.step));
            frequencySchmooInfos.push_back(frequencySchmooInfo);
        }
        IstFlow *pObj = (IstFlow *)JS_GetPrivate(pContext, pObject);
        pObj->SetFrequencySchmooInfos(frequencySchmooInfos);

        return JS_TRUE;
    }

    JS_SMETHOD_LWSTOM(IstFlow, SetVoltageProgInfos, 1, "Set voltage value")
    {
        STEST_HEADER(1, 1, "Usage: SetVoltageProgInfos,(g_VoltageProgInfo)");
        std::vector<IstFlow::VoltageProgInfo> voltageProgInfos;
        RC rc;
        JavaScriptPtr pJs;
        JsArray voltageProgInfoList;

        if (pJs->FromJsval(pArguments[0], &voltageProgInfoList) != OK)
        {
            JS_ReportError(pContext, "Invalid value for voltage %s\n", usage);
            return JS_FALSE;
        }
        for (UINT32 i = 0; i < voltageProgInfoList.size(); i++)
        {
            IstFlow::VoltageProgInfo voltageProgInfo;
            JSObject *voltageProgObj;
            CHECK_RC(pJs->FromJsval(voltageProgInfoList[i], &voltageProgObj));
            CHECK_RC(pJs->GetProperty(voltageProgObj, "domain",
                        &voltageProgInfo.domain));
            CHECK_RC(pJs->GetProperty(voltageProgObj, "valueMv",
                        &voltageProgInfo.valueMv));
            voltageProgInfos.push_back(voltageProgInfo);
        }
        IstFlow *pObj = (IstFlow *)JS_GetPrivate(pContext, pObject);
        pObj->SetVoltageProgInfos(voltageProgInfos);

        return JS_TRUE;
    }
}
