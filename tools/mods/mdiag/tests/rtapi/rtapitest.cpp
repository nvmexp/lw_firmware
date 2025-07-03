/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "rtapitest.h"
#include "core/include/platform.h"
#include "core/include/xp.h"
#include "core/include/tasker.h"
#include "mdiag/testapicontext.h"
#include "hwref/g00x/g000/armiscreg.h"
#include "hwref/g00x/g000/soc_hwproject.h"

#ifdef ELF_LOADER
#include "mdiag/tests/rtapi/elfloader/elfloader.h"
#endif

static bool s_IsFbrInitDone = false;

void TestApiRelease();
const ParamDecl rtapi_params[] = 
{
    { "-timeout_ms", "f", (ParamDecl::ALIAS_START | ParamDecl::ALIAS_OVERRIDE_OK), 0, 0, "Amount of time in milliseconds to wait before declaring the test result" },
    STRING_PARAM("-path", "specifies path to a loadable library."),
    SIMPLE_PARAM("-cpu_mode", "specifies this is a top mode test."),
    SIMPLE_PARAM("-disable_sync_between_mods_fbr", "disable sync between mods and fake bootrom."),

    SIMPLE_PARAM("-RTPrint", "enable printing logs."),
    UNSIGNED_PARAM("-num_cores", "number of cpu cores that test requires"),
    SIMPLE_PARAM("-nocacheinit", "do not initialize cache."),
    SIMPLE_PARAM("-semihosting", "enable semihosting."),
    { "-sync_at_start_stage", "u", (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl::GROUP_START), 0, 1, "sync t3d and rtapi test at start stage" },
    { "-rtapi_start_first", "0", ParamDecl::GROUP_MEMBER, 0, 0, "rtapi tests start before t3d tests" },
    { "-t3d_start_first",   "1", ParamDecl::GROUP_MEMBER, 0, 0, "t3d tests start before rtapi test" },

    { "-sync_at_end_stage", "u", (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl::GROUP_START), 0, 1, "sync t3d and rtapi test at end stage" },
    { "-rtapi_end_first", "0", ParamDecl::GROUP_MEMBER, 0, 0, "rtapi tests end before t3d tests" },
    { "-t3d_end_first",   "1", ParamDecl::GROUP_MEMBER, 0, 0, "t3d tests end before rtapi test" },
    LAST_PARAM
};

RTApiTest::RTApiTest(ArgReader* reader):Test()
{
    MASSERT(reader);
    m_pParams = reader;
    if (m_pParams->ParamPresent("-timeout_ms"))
    {
        m_TimeoutMs = m_pParams->ParamFloat("-timeout_ms", m_TimeoutMs);
    }
    else
    {
        m_TimeoutMs = 1000 * 60 * 10;
    }
    m_testHandle = nullptr;
}

struct SocBrInitPollArgs
{
    UINT64 address;
    bool   isCpuMode;
    bool   result;
};

enum InitStatus
{
    FbrInitDone = 0x1,
    DevInitDone = 0x2,
    FbrInitDoneCpuMode = 0x10000,
    DevInitDoneCpuMode = 0x20000
};

static bool IsSocBRInitialized(void *args)
{
    SocBrInitPollArgs *pArgs = (SocBrInitPollArgs*)args;
    UINT64 address = pArgs->address;
    bool isCpuMode = pArgs->isCpuMode;
    UINT32 value = 0;

    if (!s_IsFbrInitDone)
    {
        value = Platform::PhysRd32(address);
        UINT32 initDone = value & (isCpuMode ? FbrInitDoneCpuMode : FbrInitDone);
        s_IsFbrInitDone = initDone ? true : false;
    }

    if (s_IsFbrInitDone)
    {
        pArgs->result = true;
        InfoPrintf("%s: SOC initialization is done. isCpuMode:%d, Addr:0x%llx, Val:%x\n",
                   __FUNCTION__, isCpuMode, address, value);
    }
    else
    {
        InfoPrintf("%s: SOC initialization is not done. isCpuMode:%d, Addr:0x%llx, Val:%x\n",
                   __FUNCTION__, isCpuMode, address, value);
        pArgs->result = false;

        if (Platform::GetSimulationMode() != Platform::Hardware )
        {
            Platform::ClockSimulator(100);
        }
        else
        {
            Platform::Delay(1000);
        }
    }

    return pArgs->result;
}
#define MISCREG_ADDR(regOffset) (LW_ADDRESS_MAP_MISC_BASE + regOffset)
void RTApiTest::WaitForSocBrInitDone()
{

    bool isCpuMode = dynamic_cast<CpuModeTest*>(this) != nullptr;

    if (m_pParams->ParamPresent("-disable_sync_between_mods_fbr"))
    {
        DebugPrintf("%s: there is no handshake between mods and fbr!\n", __FUNCTION__);
        return;
    }
    UINT32 address = MISCREG_ADDR(MISCREG_MISC_SPARE_REG_0);
    SocBrInitPollArgs pollArgs{address, isCpuMode, false};
    POLLWRAP_HW(IsSocBRInitialized, &pollArgs, LwU64_LO32(m_TimeoutMs));
}

//!return 1 if successful, 0 if a failure oclwrred
int RTApiTest::Setup()
{
    int ret = 1;
    if (m_pParams->ParamPresent("-path"))
    {
        const char* rtTest = m_pParams->ParamStr("-path");
        RC rc = Xp::LoadDLL(rtTest, &m_testHandle, Xp::UnloadDLLOnExit);
        if (rc != OK)
        {
            ErrPrintf("%s: Failed to load RTApiTest: %s, error:%s\n",
                      __FUNCTION__, rtTest,  rc.Message());
            ret = 0;
        }
    }
    else
    {
        ErrPrintf("%s: Please specify RTAPI test with -path!\n", __FUNCTION__);
        ret = 0;
    }

    WaitForSocBrInitDone();

    //Indicate bpmp bootrom that dev init is done.
    UINT32 address = MISCREG_ADDR(MISCREG_MISC_SPARE_REG_0);
    Platform::PhysWr32(address, DevInitDone);

    HandleSoArgs();

    return ret;
}

void RTApiTest::CleanUp()
{
    if (m_testHandle)
    {
        Xp::UnloadDLL(m_testHandle);
        m_testHandle = 0;
    }

    if (m_pParams != nullptr)
    {
        delete m_pParams;
        m_pParams = nullptr;
    }

    TestApiRelease();
}

void RTApiTest::CleanUp(UINT32 stage)
{
    InfoPrintf( "%s: stage %u\n", __FUNCTION__, stage);
    CleanUp();
}

extern void TestApiSetEvent(EventId);
extern bool TestApiWaitOnEvent(EventId, UINT64);

void RTApiTest::Run(void) 
{
    m_pExecPhase = (pFnExecPhase) Xp::GetDLLProc(m_testHandle, "nitro_c_exec_phase");

    //Nitro C RTAPI Test
    if (m_pExecPhase != nullptr) 
    {
        RunNitroCTest();
        SetStatus(TestEnums::TEST_SUCCEEDED);
        return ;
    }

    //General RTAPI test
    using pFnRunRTApiTest = int (*)( );
    pFnRunRTApiTest pRuntest = (pFnRunRTApiTest)Xp::GetDLLProc(m_testHandle, "_lwrt_cpu_main");
    MASSERT(pRuntest);

    using pFnAllocRTApiTest = int (*)( );
    pFnAllocRTApiTest pAlloctest = (pFnAllocRTApiTest)Xp::GetDLLProc(m_testHandle, "_lwrt_alloc_exec");
    MASSERT(pAlloctest);

    ISR pCpuIrq = reinterpret_cast<ISR>(Xp::GetDLLProc(m_testHandle, "_lwrt_cpu_irq"));
    MASSERT(pCpuIrq);
    Platform::HookIRQ(0, pCpuIrq, nullptr);

    if (m_pParams->ParamPresent("-sync_at_start_stage"))
    {
        UINT32 argValue = m_pParams->ParamUnsigned("-sync_at_start_stage");

        if (argValue == 0)
        {
            TestApiSetEvent(StartStageEvent);
        }
        else
        {
            // Start T3D tests first
            TestApiWaitOnEvent(StartStageEvent, m_TimeoutMs);
        }
    }

    InfoPrintf("%s: Start to run RTAPI test\n", __FUNCTION__);

    bool result = !pAlloctest() && !pRuntest();
    Platform::ProcessPendingInterrupts();
    Platform::UnhookIRQ(0, pCpuIrq, nullptr);

    if (m_pParams->ParamPresent("-sync_at_end_stage"))
    {
        UINT32 argValue = m_pParams->ParamUnsigned("-sync_at_end_stage");
        // End RTAPI test first
        if (argValue == 0)
        {
            TestApiSetEvent(EndStageEvent);
        }
        else
        {
            TestApiWaitOnEvent(EndStageEvent, m_TimeoutMs);
        }
    }

    if (result)
    {
        SetStatus(TestEnums::TEST_SUCCEEDED);
    }
    else
    {
        SetStatus(TestEnums::TEST_FAILED);
    }
    InfoPrintf("%s: End to run RTAPI test\n", __FUNCTION__);
}

void RTApiTest::RunNitroCTest()
{
    InfoPrintf( "%s: Start to run NitroC RTAPI test\n", __FUNCTION__);
    using pFnLoadTest = int (*)(void *);
    pFnLoadTest pLoadTest = (pFnLoadTest) Xp::GetDLLProc(m_testHandle, "ncsf_load_test");

    //For nitro c test ISR, run exec_phase with phase_id=14, routine_idx=0, processor_id=1
    //Refer http://lwbugs/2450832/63
    auto nitrocIsr = [](void* pTest) ->long {return reinterpret_cast<RTApiTest*>(pTest)->m_pExecPhase(14, 0, 1);};
    Platform::HookIRQ(0, nitrocIsr, this);

    using pFnGetNumRoutines = int (*)(int phase_id, int processor_id);
    pFnGetNumRoutines pGetNumRoutines = (pFnGetNumRoutines) Xp::GetDLLProc(m_testHandle, "nitro_c_get_num_routines_for_phase");

    int testid = pLoadTest(m_testHandle);
    InfoPrintf( "%s: NitroC RTAPI test id:%d\n", __FUNCTION__, testid);

    const int processor = 1;
    const int max_num_phases = 12;
    for (int phase=0; phase<max_num_phases; phase++) 
    {
        int num_routines = pGetNumRoutines(phase,processor);
        InfoPrintf("%s: There are %d routines for phase %d\n", __FUNCTION__, num_routines, phase);
        if (num_routines > 1)
        { 
            for (int routine=0; routine<num_routines; routine++) 
            {
                m_pExecPhase(phase, routine, processor);
                InfoPrintf("%s: Execute routine %d of phase %d\n", __FUNCTION__, routine, phase);
            }
        }
        else
        {
            m_pExecPhase(phase,0,processor);
        }
    }

    Platform::ProcessPendingInterrupts();
    Platform::UnhookIRQ(0, nitrocIsr, this);
}


//Step1: Parse soargs from chipargs.
//Step2: Check whether "reserve_sysmem" is present in cmdline
//Step3: Write soargs contents&address to scratch space which is on sysmem.
void RTApiTest::HandleSoArgs()
{
    vector<const char*> args;
    ParseSoArgs(args);
    //RTAPI side requires at least 1 test arg.
    args.insert(args.begin(), "MODS");

    UINT32 argc = static_cast<UINT32>(args.size());
    UINT32 maxArgLen = LW_RT_SIM_MAIN_ARG_LEN_MAX;
    MASSERT((argc <= LW_RT_SIM_MAIN_ARGC_MAX) && ((argc * maxArgLen) <= LW_RT_SIM_MAIN_ARG_LIMIT));

    UINT32 baseAddr = LW_RT_SIM_SCRATCH_BASE_DFT;
    UINT32 argcOffset = LW_RT_SIM_MAIN_ARGC_OFFSET;
    Platform::PhysWr32(baseAddr + argcOffset, argc);

    //If initial scratch base address ptr is wrong, then set it.
    UINT32 basePtr = LW_RT_SIM_SCRATCH_BASE_PTR;
    UINT32 initBaseAddr = Platform::PhysRd32(basePtr);
    if (initBaseAddr != baseAddr)
    {
        DebugPrintf("%s: Initial scratch base address is 0x%llx. Now change it to 0x%llx!\n",
                     __FUNCTION__, initBaseAddr, baseAddr);
        Platform::PhysWr64(basePtr, baseAddr);
    }


    DebugPrintf("%s: There is no soargs in cmdline!\n", __FUNCTION__);
    if (Platform::GetReservedSysmemMB() == 0)
    {
        MASSERT(!"The soargs is stored on sysmem, so -reserve_sysmem is needed to avoid memory confliction!\n");
    }

    UINT32 argOffset = LW_RT_SIM_MAIN_ARG_OFFSET;
    UINT32 argvOffset = LW_RT_SIM_MAIN_ARGV_OFFSET;
    UINT32 argv64BOffset = LW_RT_SIM_MAIN_ARGV64_OFFSET;
    UINT32 address = baseAddr + argOffset;

    for (auto arg: args)
    {
        UINT32 len = static_cast<UINT32>(strlen(arg)+1);
        MASSERT(len < maxArgLen);
        //Write soargs content to scratch space
        Platform::PhysWr(address, arg, len);

        //Write soargs address to scratch space
        Platform::PhysWr32(baseAddr + argvOffset , address);
        Platform::PhysWr64(baseAddr + argv64BOffset, address);

        address += maxArgLen;
        argvOffset += 4;
        argv64BOffset += 8;
    }
}

void RTApiTest::AssertSoArgs(bool bAssert)
{
    if (bAssert)
    {
        MASSERT(!"soargs_begin and soargs_end does not match. Please check the cmdline!\n");
    }
}

//Parse soargs from chipargs. Sample is as below.
//Sample: -chipargs "-soargs_begin -v csim -enable_xbar_raw -soargs_end"
//In above sample, there are 3 soargs: "-v", "csim", "-enable_xbar_raw"
//If "soargs_begin" and "soargs_end" does not match, assert failure.
void RTApiTest::ParseSoArgs(vector<const char*>& args)
{
    bool bSoArg = false;
    const auto& argv = Platform::GetChipLibArgV();
    for (auto arg: argv)
    {
        if (strcmp(arg, "-soargs_begin") == 0)
        {
            AssertSoArgs(bSoArg);
            bSoArg = true;
        }
        else if (strcmp(arg, "-soargs_end") == 0)
        {
            AssertSoArgs(!bSoArg);
            bSoArg = false;
        }
        else if (bSoArg)
        {
            args.push_back(arg);
        }
    }

    AssertSoArgs(bSoArg);
}

CpuModeTest::CpuModeTest(ArgReader* reader):RTApiTest(reader)
{
    MASSERT(reader);
    m_pParams = reader;

    if (m_pParams->ParamPresent("-timeout_ms"))
    {
        m_TimeoutMs = m_pParams->ParamFloat("-timeout_ms", m_TimeoutMs);
    }
    else
    {
        m_TimeoutMs = 1000 * 60 * 10;
    }
}

void CpuModeTest::LoadAxf(const char* path)
{

#ifdef ELF_LOADER
    InfoPrintf("Loading Axf: %s\n", path);
    ElfLoader elfLoader(path);
    RC rc = elfLoader.LoadElf(0);
    MASSERT(rc == OK);
#else
    MASSERT(!"Loading AXF is not supported");
#endif
}


//!return 1 if successful, 0 if a failure oclwrred
int CpuModeTest::Setup()
{
    UINT32 basePtr = LW_RT_SIM_SCRATCH_BASE_PTR;
    UINT32 baseDft = LW_RT_SIM_SCRATCH_BASE_DFT;
    UINT32 coreOffset = LW_RT_SIM_CPU_CORE_OFFSET;
    const int actCoreNum = 1;
    Platform::PhysWr32(basePtr, baseDft);
    Platform::PhysWr32(baseDft+coreOffset, actCoreNum);

    UINT32 data = Platform::PhysRd32(basePtr);
    if (data != baseDft)
    {
        DebugPrintf("%s: Setup failure. sysmem addr:0x%x, value:0x%x, expected:0x%x\n",
                    __FUNCTION__, basePtr, data, baseDft);
        return 0;
    }

    data = Platform::PhysRd32(baseDft+coreOffset);
    if (data != actCoreNum)
    {
        DebugPrintf("%s: Setup failure. sysmem addr:0x%x, value:0x%x, expected:0x%x\n",
                    __FUNCTION__, baseDft+coreOffset, data, actCoreNum);
        return 0;
    }

    WaitForSocBrInitDone();

    if (m_pParams->ParamPresent("-path"))
    {
        vector<string> paths;
        const string pathStr = m_pParams->ParamStr("-path");
        RC rc = Utility::Tokenizer(pathStr, ":", &paths);
        MASSERT(rc == OK);
        for (auto& path : paths)
        {
            LoadAxf(path.c_str());
        }

        //Indicate bpmp bootrom to boot CCPLEX
        UINT32 address = MISCREG_ADDR(MISCREG_MISC_SPARE_REG_0);
        Platform::PhysWr32(address, DevInitDoneCpuMode);
        InfoPrintf("Loading CCPLEX AXF done\n");
     }

    HandleTestArgs();
    HandleSoArgs();

    return 1;
}

static const UINT32 s_TestPass = 0xff123456;
static const UINT32 s_TsimRun = 1;

void CpuModeTest::Run(void) 
{
    const UINT32 miscReg = MISCREG_ADDR(MISCREG_TRANSACTOR_SCRATCH_0);
    Platform::PhysWr32(miscReg, s_TsimRun);

    UINT32 data = Platform::PhysRd32(miscReg);
    if (data != s_TsimRun)
    {
        DebugPrintf("Test is not run by tsim. misc reg:0x%x, value:0x%x, expected:0x%x\n",
                    miscReg, data, s_TsimRun);
        return;
    }

    PollTestResult();

    InfoPrintf("End to run cpu mode test\n");
}

struct CpuModeTestResultPollArgs
{
    UINT64 address;
    bool   result;
};

static bool WaitForCpuModeTestResult(void *args)
{
    CpuModeTestResultPollArgs *pArgs = (CpuModeTestResultPollArgs*)args;
    UINT64 address = pArgs->address;
    if (Platform::PhysRd32(address) == s_TestPass)
    {
        pArgs->result = true;
    }
    else
    {
        pArgs->result = false;
        if (Platform::GetSimulationMode() != Platform::Hardware)
        {
            // Reading register to kick off FMODEL running
            UINT64 addr = MISCREG_ADDR(MISCREG_MISC_SPARE_REG_0);
            Platform::PhysRd32(addr);
            Platform::ClockSimulator(100);
        }
        else
        {
           Platform::Delay(1000);
        }
    }

    return pArgs->result;
}

void CpuModeTest::PollTestResult()
{
    UINT32 baseDft = LW_RT_SIM_SCRATCH_BASE_DFT;
    UINT32 termOffset = LW_RT_SIM_CPU_TERMINATION_OFFSET;
    CpuModeTestResultPollArgs pollArgs{baseDft+termOffset, false};
    POLLWRAP_HW(WaitForCpuModeTestResult, &pollArgs, LwU64_LO32(m_TimeoutMs));
    TestEnums::TEST_STATUS status = pollArgs.result ? TestEnums::TEST_SUCCEEDED : TestEnums::TEST_FAILED; 
    SetStatus(status);
}


// Parse test args and store them into scratch space
void CpuModeTest::HandleTestArgs()
{

    UINT32 enablePrint = 0;
    if (m_pParams->ParamPresent("-RTPrint"))
    {
        enablePrint = 1;
    }

    UINT32 scratchBase = LW_RT_SIM_SCRATCH_BASE_DFT;

    UINT32 offset = LW_RT_SIM_CPU_RTPRINT_BUFFER_OFFSET;
    for (int i = 0; i < LW_RT_SIM_RTPRINT_BUFFER_COUNT; i++)
    {
        Platform::PhysWr32(scratchBase + offset, 0);
        offset += LW_RT_SIM_RTPRINT_BUFFER_SIZE;
    }

    Platform::PhysWr32(scratchBase + LW_RT_SIM_RTPRINT_ENABLE_OFFSET, enablePrint);

    vector<UINT08> initPrtBufVal(LW_RT_SIM_RTPRINT_BUFFER_COUNT);
    Platform::PhysWr(scratchBase + LW_RT_SIM_CPU_RTPRINT_OFFSET, &initPrtBufVal[0], LW_RT_SIM_RTPRINT_BUFFER_COUNT);
    Platform::PhysWr(scratchBase + LW_RT_SIM_COP_RTPRINT_OFFSET, &initPrtBufVal[0], LW_RT_SIM_RTPRINT_BUFFER_COUNT);
    Platform::PhysWr32(scratchBase + LW_RT_SIM_CPU_TERMINATION_OFFSET, LW_RT_SIM_TERMINATION_INIT);
    Platform::PhysWr32(scratchBase + LW_RT_SIM_COP_TERMINATION_OFFSET, LW_RT_SIM_TERMINATION_INIT);

    Platform::PhysWr32(scratchBase + LW_RT_SIM_CPU_DEBUG_OFFSET, LW_RT_SIM_DEBUG_PASS);
    Platform::PhysWr32(scratchBase + LW_RT_SIM_COP_DEBUG_OFFSET, LW_RT_SIM_DEBUG_PASS);

    UINT32 initCores = 1;
    if (m_pParams->ParamPresent("-num_cores") > 0)
    {
        initCores = m_pParams->ParamUnsigned("-num_cores");
    }
    Platform::PhysWr32(scratchBase + LW_RT_SIM_CPU_CORE_OFFSET, initCores);

    // Init here rather than doing in start_main_64gcc.S, to avoid each core clearing it
    Platform::PhysWr32(scratchBase + LW_RT_CPU_BOOT_SCRATCH0_OFFSET, 0);
    Platform::PhysWr32(scratchBase + LW_RT_SIM_CPU_RTPRINT_BID_OFFSET, 0);
    Platform::PhysWr32(scratchBase + LW_RT_SIM_COP_RTPRINT_BID_OFFSET, 0);

    INT32 cpuSync[4]{~0, ~0, ~0, ~0};
    Platform::PhysWr(scratchBase + LW_RT_SIM_CPU_SYNC_OFFSET, cpuSync, sizeof(cpuSync));

    UINT32 noCacheInit = 0;
    if (m_pParams->ParamPresent("-nocacheinit"))
    {
        noCacheInit = 1;
    }
    Platform::PhysWr32(scratchBase + LW_RT_SIM_CACHE_INIT_OFFSET, noCacheInit);

    UINT32 semiBase[LW_RT_CPU_COUNT + LW_RT_COP_COUNT] {
                    LW_RT_SIM_CPU0_SEMIHOSTED_BASE,
                    LW_RT_SIM_CPU1_SEMIHOSTED_BASE,
                    LW_RT_SIM_CPU2_SEMIHOSTED_BASE,
                    LW_RT_SIM_CPU3_SEMIHOSTED_BASE,
                    LW_RT_SIM_CPU4_SEMIHOSTED_BASE,
                    LW_RT_SIM_CPU5_SEMIHOSTED_BASE,
                    LW_RT_SIM_CPU6_SEMIHOSTED_BASE,
                    LW_RT_SIM_CPU7_SEMIHOSTED_BASE,
                    LW_RT_SIM_COP0_SEMIHOSTED_BASE,
                    LW_RT_SIM_COP1_SEMIHOSTED_BASE,
                    LW_RT_SIM_COP2_SEMIHOSTED_BASE,
                    LW_RT_SIM_COP3_SEMIHOSTED_BASE,
                    LW_RT_SIM_COP4_SEMIHOSTED_BASE,
                    LW_RT_SIM_COP5_SEMIHOSTED_BASE };

    bool bSemihosting = false;
    UINT32 semiSemEnable = 0;
    if (m_pParams->ParamPresent("-semihosting"))
    {
        bSemihosting  = true;
        semiSemEnable = 1;
    }

    for (int i=0; i<(LW_RT_CPU_COUNT + LW_RT_COP_COUNT); i++)
    {
        semiBase[i] += scratchBase;
        DebugPrintf("setting semihosted: semibase 0x%x,scratchbase 0x%x and id 0x%x {\n", semiBase[i], scratchBase, i);
        if (bSemihosting)
        {
            Platform::PhysWr32(semiBase[i]+ LW_RT_SIM_SEMIHOSTED_SEMAPHORE_OFS, 0);
        }
    }

    // Enable semihosting service flag if bSemihosting is true
    Platform::PhysWr32(scratchBase + LW_RT_SIM_SEMIHOSTED_SEMAPHORE_ENABLE, semiSemEnable);

    // Initializing PMC related SIM_SCRATCH space to zeros
    // uint32_t pmcAddr;
    for(int i=LW_RT_SIM_PMC_SCRATCH_MIN_OFFSET; i<LW_RT_SIM_PMC_SCRATCH_MAX_OFFSET; i=i+4)
    {
        DebugPrintf("Initializing PMC Scratch: Address 0x%x to zero\n", scratchBase+i);
        Platform::PhysWr32(scratchBase+i, 0);
    }
}

Test* RTApiTest::Factory(ArgDatabase *args)
{
    ArgReader *reader = new ArgReader(rtapi_params);
    if (!reader || !reader->ParseArgs(args))
    {
        ErrPrintf("%s: error reading parameters!\n", __FUNCTION__);
        return(0);
    }

    if (reader->ParamPresent("-cpu_mode"))
    {
        return new CpuModeTest(reader);
    }
    else
    {
        return new RTApiTest(reader);
    }
}
