/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file jspmgrtst.cpp
 */

#include "gpu/tests/rmtest.h"
#include "stdio.h"
#include "core/include/xp.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/cmdline.h"
#include "core/include/platform.h"
#include "gpu/utility/surf2d.h"
#include "core/include/utility.h"
#include "core/utility/errloggr.h"

#include <limits>
#include <vector>
#include <map>

#define UDTA_2LWDLIB_IF_LWDLIB_VERSION 13
typedef UINT32 (*Udta2PmgrIfGetIfLwdLibVersionFunc_t)();
typedef void   (*Udta2PmgrIfRegWr32Func_t)(UINT32, UINT32);
typedef UINT32 (*Udta2PmgrIfRegRd32Func_t)(UINT32);

enum Udta2PmgrIfPlatformType
{
    UDTA_ON_FULLCHIP_FMOD
    ,UDTA_ON_FULLCHIP_RTL
    ,UDTA_ON_FPGA
    ,UDTA_ON_EMULATION
    ,UDTA_ON_SILICON
};

typedef UINT32 (*Udta2PmgrIfGetPlatformFunc_t)();

typedef void   (*Udta2PmgrIfRunForNClocksFunc_t)(UINT32);
typedef void   (*Udta2PmgrIfEscapeReadFunc_t)
(
    const char *,
    UINT32,
    UINT32,
    UINT32 *
);

typedef void   (*Udta2PmgrIfEscapeWriteFunc_t)
(
    const char *,
    UINT32,
    UINT32,
    UINT32
);

typedef void   (*Udta2PmgrIfRegisterModsArgFunc_t)
(
    const char*,
    UINT32
);

#define TYPEDEF_REGISTER_UDTA_2PMGR_IF(func_name) \
    typedef void (*RegisterUdta2PmgrIf_##func_name##If_t)(Udta2PmgrIf##func_name##Func_t);

TYPEDEF_REGISTER_UDTA_2PMGR_IF(GetIfLwdLibVersion)
TYPEDEF_REGISTER_UDTA_2PMGR_IF(RegWr32)
TYPEDEF_REGISTER_UDTA_2PMGR_IF(RegRd32)
TYPEDEF_REGISTER_UDTA_2PMGR_IF(GetPlatform)
TYPEDEF_REGISTER_UDTA_2PMGR_IF(RunForNClocks)
TYPEDEF_REGISTER_UDTA_2PMGR_IF(EscapeRead)
TYPEDEF_REGISTER_UDTA_2PMGR_IF(EscapeWrite)
TYPEDEF_REGISTER_UDTA_2PMGR_IF(RegisterModsArg)


typedef UINT32 (*GetIfUdtaVersionIf_t)();
typedef void   (*UdtaMainIf_t)(const int&, const char**);

class JsPmgrTest: public RmTest
{
public:
    // constructor
    JsPmgrTest();
    virtual ~JsPmgrTest();

    virtual bool GetTestsAllSubdevices() {return false;}

    virtual string IsTestSupported();

    virtual RC Setup() { return OK;}
    virtual RC Run();
    virtual RC Cleanup() {return OK;}


    RC LaunchUDTA(const std::string udtaPath, const std::vector<std::string> &argv);

};


namespace Rmt_JsPmgr
{

static JsPmgrTest *s_pPmgr = nullptr;

UINT32 GetIfLwdLibVersion()
{
    // return Udta2LwdLibIf LwdLib side version to UDTA
    return UDTA_2LWDLIB_IF_LWDLIB_VERSION;
}

UINT32 GetPlatform()
{
    MASSERT(s_pPmgr != nullptr);

    UINT32 unPlatform;
    if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        unPlatform = UDTA_ON_FULLCHIP_FMOD;
    }
    else if (Platform::GetSimulationMode() == Platform::RTL)
    {
        unPlatform = UDTA_ON_FULLCHIP_RTL;
    }
    else if (s_pPmgr->GetBoundGpuSubdevice()->IsDFPGA())
    {
        unPlatform = UDTA_ON_FPGA;
    }
    else if (s_pPmgr->GetBoundGpuSubdevice()->IsEmulation())
    {
        unPlatform = UDTA_ON_EMULATION;
    }
    else if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        // Platform::Hardware && !IsDFPGA() && !IsEmulation()
        unPlatform = UDTA_ON_SILICON;
    }
    else
    {
        // Set defualt platform as fmod
        unPlatform = UDTA_ON_FULLCHIP_FMOD;
    }

    return unPlatform;
}


void RegWr32(UINT32 unAddress, UINT32 unData)
{
    MASSERT(s_pPmgr != nullptr);

    s_pPmgr->GetBoundGpuSubdevice()->RegWr32(unAddress, unData);
}

UINT32 RegRd32(UINT32 unAddress)
{
    MASSERT(s_pPmgr != nullptr);

    return s_pPmgr->GetBoundGpuSubdevice()->RegRd32(unAddress);
}

void RunForNClocks(UINT32 n)
{
    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        // Colwert clock cycles to delay.
        if (s_pPmgr->GetBoundGpuSubdevice()->IsEmulation())
        {
            // Assuming that the imaginative clock is in worst case as low as 1 kHz
            // 1 clock cycle = 1 millisecond
            Platform::Delay(n * 1000);
        }
        else
        {
            // Assuming that the clock is not slower than 1 MHz
            // 1 clock cycle = 1 microsecond
            Platform::Delay(n);
        }
    }
    else
    {
        Platform::ClockSimulator(n);
    }

    Tasker::Yield();
}

void EscapeRead(const char *Path, UINT32 Index, UINT32 Size, UINT32 *pValue)
{
    Platform::EscapeRead(Path, Index, Size, pValue);
}

void EscapeWrite(const char *Path, UINT32 Index, UINT32 Size, UINT32 Value)
{
    Platform::EscapeWrite(Path, Index, Size, Value);
}


void RegisterModsArg
(
     const char* argName,
     UINT32 numParams
)
{
     ArgDatabase *argDatabase = CommandLine::ArgDb();

     string fullArgName = "-";
     fullArgName += argName;

     string argTypes;
     for (UINT32 typeIdx = 0; typeIdx < numParams; typeIdx++)
     {
        argTypes += "t";
     }

     ParamDecl param[2] =
     {
         {fullArgName.c_str(), argTypes.c_str(), ParamDecl::PARAM_MULTI_OK, 0, 0, argName},
         {nullptr, nullptr, 0, 0, 0, nullptr}
     };

     ArgReader argReader(param);

     argReader.ParseArgs(argDatabase);
}

} // namespace


//! \brief IsTestSupported Function.
//!
//! The test is not supported on fmodel platform
//! \sa Setup
//-----------------------------------------------------------------------------
string JsPmgrTest::IsTestSupported()
{
    if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        return "Gpio Sanity Test is not supported for F-Model due to incomplete register modelling.";
    }

    return RUN_RMTEST_TRUE;
}


RC JsPmgrTest::LaunchUDTA(const std::string udtaPath, const std::vector<std::string> &argv)
{
    RC rc;

    Rmt_JsPmgr::s_pPmgr = this;

    // RTL sim save/restore
    //TODO
    //RtlSaveRestore(argv);

    // Load csl lib
    void *pCslHandle;
    // if we load debug version UDTA lib, we need load debug csl lib too
    CHECK_RC(Xp::LoadDLL(strstr(udtaPath.c_str(), "debug") ? "libdebug_csl_64.so" : "libcsl_64.so",
        &pCslHandle, Xp::UnloadDLLOnExit|Xp::GlobalDLL));
    if (nullptr == pCslHandle)
    {
        MASSERT(!"Error: couldn't open libcsl.\n");
    }

    Printf(Tee::PriHigh, "Udta2LwdLibIf: UDTA Lib Path [%s]!", udtaPath.c_str());
    void *pUDTAHandle;
    CHECK_RC(Xp::LoadDLL(udtaPath.c_str(), &pUDTAHandle, Xp::UnloadDLLOnExit|Xp::GlobalDLL));
    if (nullptr == pUDTAHandle)
    {
        MASSERT(!"Error: couldn't open libudta.\n");
    }

    // Check UDTA side version first
    GetIfUdtaVersionIf_t pGetIfUdtaVersion
        = (GetIfUdtaVersionIf_t) Xp::GetDLLProc(pUDTAHandle, "GetIfUdtaVersion");
    if (!pGetIfUdtaVersion)
    {
        MASSERT(!"Error: couldn't get GetIfUdtaVersion func from libudta.so.\n");
    }
    if ((*pGetIfUdtaVersion)() < 2)
    {
        Printf(Tee::PriHigh, "Error: Udta2LwdLibIf UDTA side version "
                "(UDTA_2LWDLIB_IF_UDTA_VERSION) is (%d), less than "
                "Interface Requirement (2)!\n", (*pGetIfUdtaVersion)());
        MASSERT(0);
    }

#define CALL_REGISTER_UDTA_2LWDLIB_IF(func_name) \
    RegisterUdta2PmgrIf_##func_name##If_t pRegister##func_name = \
        (RegisterUdta2PmgrIf_##func_name##If_t) Xp::GetDLLProc( \
            pUDTAHandle, \
            "RegisterUdta2LwdLibIf_"#func_name); \
    if (!pRegister##func_name) \
    { \
        MASSERT(!"Error: couldn't get RegisterUdta2LwdLibIf_"#func_name" func from libudta.so.\n"); \
    } \
    (*pRegister##func_name)(Rmt_JsPmgr::func_name);

    CALL_REGISTER_UDTA_2LWDLIB_IF(GetIfLwdLibVersion)
    CALL_REGISTER_UDTA_2LWDLIB_IF(RegWr32)
    CALL_REGISTER_UDTA_2LWDLIB_IF(RegRd32)
    CALL_REGISTER_UDTA_2LWDLIB_IF(GetPlatform)
    CALL_REGISTER_UDTA_2LWDLIB_IF(RunForNClocks)
    CALL_REGISTER_UDTA_2LWDLIB_IF(EscapeRead)
    CALL_REGISTER_UDTA_2LWDLIB_IF(EscapeWrite)
    CALL_REGISTER_UDTA_2LWDLIB_IF(RegisterModsArg)

    UdtaMainIf_t pUdtaMain = (UdtaMainIf_t) Xp::GetDLLProc(pUDTAHandle, "UDTA_MAIN");
    if (!pUdtaMain)
    {
        MASSERT(!"Error: couldn't get UDTA_MAIN func from libudta.so.\n");
    }
    // Prepare char** style UDTA args
    std::vector<const char *> udtaArgv(argv.size(), nullptr);
    for (UINT32 i=0; i<argv.size(); i++)
    {
        udtaArgv[i] = argv[i].c_str();
    }
    // Pass args to UDTA
    (*pUdtaMain)((int)(argv.size()), &udtaArgv[0]);

    return rc;
}

JsPmgrTest::JsPmgrTest()
{
    SetName("JsPmgrTest");
}

JsPmgrTest::~JsPmgrTest()
{
}

RC JsPmgrTest::Run()
{
    RC rc;
    UINT32 val;
    jsval jsRc;
    JsArray args;
    JavaScriptPtr pJs;

    // Call the JS function that will do the real test
    CHECK_RC(pJs->CallMethod(GetJSObject(), "JSRunFunction", args, &jsRc));

    // Colwert the jsRc into a C++ RC
    CHECK_RC(pJs->FromJsval(jsRc, &val));
    rc = val;

    return rc;
}

JS_CLASS_INHERIT(JsPmgrTest, RmTest, "A JavaScript Test for Pmgr");

JS_STEST_BIND_ONE_ARG(JsPmgrTest, PrintProgressInit, maxIterations, UINT64,
    "Initialize the maximum number of iterations the test is expected to make");

JS_STEST_BIND_ONE_ARG(JsPmgrTest, PrintProgressUpdate, lwrrentIteration, UINT64,
    "Update the current progress of the test");

JS_STEST_LWSTOM(JsPmgrTest,
                LaunchUDTA,
                2,
                "Init Udta2LwdLibIf, Load UDTA and then Execute UDTA" )
{
    JavaScriptPtr pJs;

    // Check the arguments.
    std::string udtaPath;
    std::vector<std::string> argv;

    if ((NumArguments != 2) ||
        (OK != pJs->FromJsval(pArguments[0], &udtaPath)) ||
        (OK != pJs->FromJsval(pArguments[1], &argv)))
    {
        JS_ReportError(pContext, "Usage: JsPmgrTest.LaunchUDTA(udtaPath, udtaArgs)");
        return JS_FALSE;
    }

    JsPmgrTest *pJsPmgrTest;
    if ((pJsPmgrTest = JS_GET_PRIVATE(JsPmgrTest, pContext, pObject, "JsPmgrTest")) != 0)
    {
        RETURN_RC(pJsPmgrTest->LaunchUDTA(udtaPath, argv));
    }

    return JS_FALSE;
}

