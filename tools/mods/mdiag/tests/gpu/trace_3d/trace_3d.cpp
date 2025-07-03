/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "trace_3d.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_classes.h"
#include "mdiag/utils/lwgpu_channel.h"

#include "mdiag/utils/surfutil.h"
#include "mdiag/utils/cregctrl.h"
#include "mdiag/utils/utils.h"
#include "mdiag/utils/cpumodel.h"
#include "mdiag/tests/test_state_report.h"
#include "selfgild.h"
#include "mdiag/utils/tex.h"
#include "mdiag/tests/testdir.h"

#include "core/include/tasker.h"
#include "core/include/platform.h"
#include "core/include/pool.h"
#include "core/include/lwrm.h"
#include "core/utility/errloggr.h"
#include "core/include/channel.h"
#include "core/include/registry.h"
#include "gpu/utility/atomwrap.h"
#include "gpu/utility/surffmt.h"

#include "mdiag/tests/gpu/lwgpu_tsg.h"

#include "turing/tu102/dev_bus.h"      // for LW_BRIDGE_REG_PM_CONTROL
#include "turing/tu102/dev_bus_addendum.h" // for RTD3, see bug2037431
#include "lwmisc.h"
#include "lwos.h"
#include "lwRmReg.h"  // for LW_REG_STR_RM_SUBSCRIBE_TO_ARCH_EVENTS

#include "ctrl/ctrl0041.h"
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl0080/ctrl0080fb.h"
#include "ctrl/ctrl2080.h" // LW20_SUBDEVICE_XX
#include "ctrl/ctrl5070.h" // LW5070_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT

#include "class/cl007d.h" // LW04_SOFTWARE_TEST
#include "class/cl5080.h" // LW50_DEFERRED_API_CLASS
#include "class/cl86b6.h" // LW86B6_VIDEO_COMPOSITOR
#include "class/cl902d.h" // FERMI_TWOD_A
#include "class/cl90b8.h" // GF106_DMA_DECOMPRESS
#include "class/cl95a1.h" // LW95A1_TSEC
#include "class/cl95b1.h" // LW95B1_VIDEO_MSVLD
#include "class/cla140.h" // KEPLER_INLINE_TO_MEMORY_B
#include "class/clc06f.h" // PASCAL_CHANNEL_GPFIFO_A
#include "class/cla0c0.h" // KEPLER_COMPUTE_A
#include "class/clc0c0.h" // PASCAL_COMPUTE_A
#include "class/cl9097.h" // FERMI_A
#include "class/clb197.h" // MAXWELL_B
#include "class/clc097.h" // PASCAL_A

#include "mdiag/utils/crc.h"
#include "mdiag/utils/profile.h"
#include "gpu/display/dpc/dpc_configurator.h"
#include "core/include/display.h"

#include <string.h>
#include "mdiag/utils/mdiag_xml.h"

#include "mdiag/utils/LWFloat.h"

// adv schd includes
#include "mdiag/advschd/policymn.h"
#include "mdiag/pm_lwchn.h"
#include "mdiag/pm_vaspace.h"
#include "mdiag/advschd/pmvftest.h"
#include "pm_t3d.h"
#include "mdiag/pm_smcengine.h"

#include "gpu/display/evo_disp.h" // EvoRasterSettings

#include "mdiag/gpu/zlwll.h"
#include "tracetsg.h"

#include "gpu/perf/pmusub.h"
#include "teegroups.h"

#include "gpu/perf/perfsub.h"
#include "core/include/simclk.h"
#include "core/include/chiplibtracecapture.h"

#include "mdiag/vmiopmods/vmiopmdiagelwmgr.h"

#include "mdiag/smc/smcengine.h"

#include "mdiag/utils/sharedsurfacecontroller.h"
#include "mdiag/utl/utl.h"

#ifndef _WIN32
#include "compbits_intf.h"
#endif

#ifdef INCLUDE_MDIAGUTL 
#include "mdiag/utl/utlgpuverif.h"
#endif

#include "mdiag/testapicontext.h"

#define MSGID(Mod) T3D_MSGID(Mod)

#define FIRST_TEST_ID 0

#define CHIPLIB_CALLSCOPE_WITH_TESTNAME \
        ChiplibOpScope newScope(string(GetTestName()) + " " + __FUNCTION__, \
                                NON_IRQ, ChiplibOpScope::SCOPE_COMMON, NULL)


LwrrentTestsSetting Trace3DTest::s_LwrrentTestsSetting;

extern const ParamDecl trace_3d_params[];

extern const ParamConstraints trace_file_constraints[];

static const char *DisplayArgNames[MAX_RENDER_TARGETS] =
{
    "-displayCA",
    "-displayCB",
    "-displayCC",
    "-displayCD",
    "-displayCE",
    "-displayCF",
    "-displayCG",
    "-displayCH",
};

map <string, Trace3DTest *> Trace3DTest::s_Trace3DName2Test;
map <UINT32, Trace3DTest *> Trace3DTest::s_Trace3DId2Test;

UINT32 Trace3DTest::s_ThreadCount = 0;

extern void TestApiSetEvent(EventId);
extern bool TestApiWaitOnEvent(EventId, UINT64);

// Used by Trace3dTest::WaitForNotifiersAllChannels()
static const char *WfiMethodToString(WfiMethod wfiMethod)
{
    switch (wfiMethod)
    {
        case WFI_POLL:
            return "wfi_poll";
        case WFI_NOTIFY:
            return "wfi_notify";
        case WFI_INTR:
            return "wfi_intr";
        case WFI_SLEEP:
            return "wfi_sleep";
        case WFI_HOST:
            return "wfi_host";
        case WFI_INTR_NONSTALL:
            return "wfi_intr_nonstall";
    }
    MASSERT(!"Illegal case passed to WfiMethodToString");
    return "";
}

Trace3DTest::Trace3DTest(ArgReader *reader,
                         UINT32 deviceInst /* = Gpu::UNSPECIFIED_DEV */ ) :
    ConlwrrentTest(),
    m_Trace(reader, this),
    m_GpuInst(deviceInst),
    m_PEConfigureFile(reader, this),
    m_TestGpuVerif(0),
    m_CoordinatorSyncCtx(false),
    m_IsTuT(false)
{
    int i, ft;

    m_pLwRm = LwRmPtr().Get();
    m_bAborted = false;
    m_clientExited = false;
    m_clientStatus = Test::TEST_SUCCEEDED;

    // Sequence ID is one-based, but the test ID used for accessing
    // VA spaces and subcontexts is zero-based.
    m_TestId = static_cast<UINT32>(GetSeqId()) - 1;
    s_Trace3DId2Test[m_TestId] = this;

    // these are used in the crc check method
    for (i = 0; i < MAX_RENDER_TARGETS; i++)
        surfC[i] = 0;
    surfZ = 0;

    params = reader;
    gsm = 0;

    scissorEnable = false;
    scissorXmin = 0;
    scissorYmin = 0;
    scissorWidth = 0;
    scissorHeight = 0;
    m_EnableSLIScissor = false;

    m_streams = 0;

    traceDisplayWidth = 0;
    traceDisplayHeight = 0;

    m_beginSwitchCalled = false;

    // Set up the time meter
    meter.reset(new StopWatch);
    for (ft = GpuTrace::FT_FIRST_FILE_TYPE; ft < GpuTrace::FT_NUM_FILE_TYPE; ft++)
    {
        m_Padding[ft] = 0;
    }

    // always create chunks for vaspace handle 0
    // it may used for default vas or segment mode surfaces
    CreateChunkMemory(0);
    m_GSSpillRegion.SetLocation(Memory::Fb);
    ParseDmaBufferArgs(m_GSSpillRegion, reader, "spill_region", &m_GSSpillRegionPteKindName, 0);

    getStateReport()->enable();

    m_stream_buffer = 1; //streaming output always goes to a surface 1. A test with streaming
                         //output never contains more then one render target
                         //Hacky? Yes. Ideally we should not put a hex dump of vertices on a surface
                         //but draw something using these vertices. That goes nicely along with
                         //selfgilding tests. But that requires support of multipass in trace_3d+FakeGL.
                         //This hack allows to start writing tests with streaming output.
                         //Support of streaming output should be revisited.
    pmMode = PM_MODE_NONE;
    numPmTriggers = 0;

    m_DisplayActive = false;
    m_DisplayCtxDma = 0xbad;
    m_DisplayMemHandle = 0xbad;

    m_profile.reset(new CrcProfile(CrcProfile::NORMAL));
    m_goldProfile.reset(new CrcProfile(CrcProfile::GOLD));

    m_profile->Initialize(reader);
    m_goldProfile->Initialize(reader);


    // Set the name for the trace_3d test, if the test name is not explicitly
    // specified, then use the last directory from the "-i" option (which is
    // required) as the test name (i.e. .../<testname>/test.hdr).  If the
    // test name cannot be parsed from the "-i" parameter, then use
    // "Trace3DTest_<test num>" as the test name.
    if (reader->ParamPresent("-name"))
    {
        m_TestName = string(reader->ParamStr("-name"));
    }
    else
    {
        string traceFile;
        if (reader->ParamPresent("-i"))
        {
           traceFile = string(reader->ParamStr("-i"));
        }
        m_TestName = MDiagUtils::GetTestNameFromTraceFile(traceFile);
        if (m_TestName.empty())
        {
            m_TestName = Utility::StrPrintf("Trace3DTest_%d",
                                (UINT32)s_Trace3DName2Test.size());
            InfoPrintf("Default test name is %s\n", m_TestName.c_str());
        }
    }

    // If more than one test shares the same name, append the test number
    // to the name
    if (s_Trace3DName2Test.count(m_TestName) != 0)
    {
        string BaseName = m_TestName;
        for (UINT32 suffix = 1; s_Trace3DName2Test.count(m_TestName) != 0; ++suffix)
            m_TestName = Utility::StrPrintf("%s_%d", BaseName.c_str(), suffix);

        InfoPrintf("Test %s already present, adding test %s\n",
                   BaseName.c_str(), m_TestName.c_str());
    }
    s_Trace3DName2Test[m_TestName] = this;

    if (params->ParamPresent("-trace_profile") > 0)
    {
        if (SimClk::SimManager* pM = SimClk::SimManager::GetInstance())
        {
            pM->SkipTraceProfile(false);
        }
    }

    m_pPluginHost = 0;
    m_pluginLibraryHandle = 0;

    m_AllocSurfaceCommandPresent = false;
    m_BuffInfo.SetPrintBufferOnce(true);
    m_ChannelAllocInfo.SetPrintOnce(true);

    m_SLIScissorSpec.clear();
    m_EnableSLIScissor = false;
    m_EnableSLISurfClip = false;

    m_LwrrentChannel = 0;

    m_IsCtxSwitchTest = GpuChannelHelper::IsCtxswEnabledByCmdline(params);

    m_WfiMethod = WFI_POLL;
    m_NoPbMassage = false;
    m_NoAutoFlush = false;

    // If skip dev init.
    m_SkipDevInit = GpuPtr()->IsInitSkipped();

    m_IsTopPlugin = (reader->ParamPresent("-cpu_model"));

    m_CoreId = 0;

    m_ThreadId = s_ThreadCount;
    s_ThreadCount++;

    m_IsProcessingEvent = false;
    m_NeedBlockEvent = false;

    m_PluginMailBox = NULL;
    m_TraceMailBox = NULL;

    if (m_IsCtxSwitchTest && params->ParamPresent("-pm_sync_coordinator"))
    {
        SetCoordinatorSyncCtx(true);
    }

    m_SmcEngineName = ParseSmcEngArg();

    if (m_IsCtxSwitchTest)
    {
        RC rc;
        if ((rc = GetBoundResources()) != OK)
        {
            ErrPrintf("GetBoundResources failed: %s \n", rc.Message());
            MASSERT(0);
        }

        if ((rc = ParseOptionalRegisters()) != OK)
        {
            ErrPrintf("ParseOptionalRegisters failed: %s \n", rc.Message());
            MASSERT(0);
        }

        if ((rc =ParseOptionalPmFile()) != OK)
        {
            ErrPrintf("ParseOptionalPmFile failed: %s \n", rc.Message());
            MASSERT(0);
        }
    }
}

Trace3DTest::~Trace3DTest()
{
    s_Trace3DName2Test.erase(m_TestName);
    s_Trace3DId2Test.erase(m_TestId);

    meter.reset(0);

    for (UINT32 stage = 0; stage < NumCleanUpStages(); ++stage)
        CleanUp(stage);

    m_profile.reset(0);
    m_goldProfile.reset(0);
    for (auto& chunk : m_Chunks)
    {
        delete[](chunk.second);
    }
}

void Trace3DTest::CreateChunkMemory(LwRm::Handle hVASpace)
{
    if (m_Chunks[hVASpace] == 0) {
        InfoPrintf("creating chunk memory for vaspace handle %x\n", hVASpace);
        Chunk *pChunks = new Chunk[GpuTrace::FT_NUM_FILE_TYPE];
        m_Chunks[hVASpace] = pChunks;

        for (int ft = GpuTrace::FT_FIRST_FILE_TYPE; ft < GpuTrace::FT_NUM_FILE_TYPE; ft++)
        {
            pChunks[ft].m_Surface.SetGpuVASpace(hVASpace);
            pChunks[ft].m_NeedPeerMapping = false;
            pChunks[ft].m_Surface.SetLocation((Memory::Location)0xFFFFFFFF);
        }

        // we copy to push buffer, so keep this in local mem
        pChunks[GpuTrace::FT_PUSHBUFFER].m_Surface.SetLocation(Memory::Coherent);
        ParseArgsForType(GpuTrace::FT_VERTEX_BUFFER, params, "vtx", hVASpace);
        ParseArgsForType(GpuTrace::FT_INDEX_BUFFER, params, "idx", hVASpace);
        ParseArgsForType(GpuTrace::FT_TEXTURE, params, "tex", hVASpace);
        ParseArgsForType(GpuTrace::FT_SHADER_PROGRAM, params, "pgm", hVASpace);
        ParseArgsForType(GpuTrace::FT_CONSTANT_BUFFER, params, "const", hVASpace);
        ParseArgsForType(GpuTrace::FT_TEXTURE_HEADER, params, "header", hVASpace);
        ParseArgsForType(GpuTrace::FT_TEXTURE_SAMPLER, params, "sampler", hVASpace);
        ParseArgsForType(GpuTrace::FT_SHADER_THREAD_MEMORY, params, "thread_mem", hVASpace);
        ParseArgsForType(GpuTrace::FT_SHADER_THREAD_STACK, params, "thread_stack", hVASpace);
        ParseArgsForType(GpuTrace::FT_SEMAPHORE, params, "sem", hVASpace);
        ParseArgsForType(GpuTrace::FT_SEMAPHORE_16, params, "sem", hVASpace);
        ParseArgsForType(GpuTrace::FT_NOTIFIER, params, "notifier", hVASpace);
        ParseArgsForType(GpuTrace::FT_STREAM_OUTPUT, params, "stream", hVASpace);
        ParseArgsForType(GpuTrace::FT_SELFGILD, params, "sgd", hVASpace);
        ParseArgsForType(GpuTrace::FT_VP2_0, params, "vp2_0", hVASpace);
        ParseArgsForType(GpuTrace::FT_VP2_1, params, "vp2_1", hVASpace);
        ParseArgsForType(GpuTrace::FT_VP2_2, params, "vp2_2", hVASpace);
        ParseArgsForType(GpuTrace::FT_VP2_3, params, "vp2_3", hVASpace);
        ParseArgsForType(GpuTrace::FT_VP2_4, params, "vp2_4", hVASpace);
        ParseArgsForType(GpuTrace::FT_VP2_5, params, "vp2_5", hVASpace);
        ParseArgsForType(GpuTrace::FT_VP2_6, params, "vp2_6", hVASpace);
        ParseArgsForType(GpuTrace::FT_VP2_7, params, "vp2_7", hVASpace);
        ParseArgsForType(GpuTrace::FT_VP2_8, params, "vp2_8", hVASpace);
        ParseArgsForType(GpuTrace::FT_VP2_9, params, "vp2_9", hVASpace);
        ParseArgsForType(GpuTrace::FT_VP2_14, params, "vp2_14", hVASpace);
        ParseArgsForType(GpuTrace::FT_CIPHER_A, params, "cipher_a", hVASpace);
        ParseArgsForType(GpuTrace::FT_CIPHER_B, params, "cipher_b", hVASpace);
        ParseArgsForType(GpuTrace::FT_CIPHER_C, params, "cipher_c", hVASpace);
        ParseArgsForType(GpuTrace::FT_PMU_0, params, "pmu_0", hVASpace);
        ParseArgsForType(GpuTrace::FT_PMU_1, params, "pmu_1", hVASpace);
        ParseArgsForType(GpuTrace::FT_PMU_2, params, "pmu_2", hVASpace);
        ParseArgsForType(GpuTrace::FT_PMU_3, params, "pmu_3", hVASpace);
        ParseArgsForType(GpuTrace::FT_PMU_4, params, "pmu_4", hVASpace);
        ParseArgsForType(GpuTrace::FT_PMU_5, params, "pmu_5", hVASpace);
        ParseArgsForType(GpuTrace::FT_PMU_6, params, "pmu_6", hVASpace);
        ParseArgsForType(GpuTrace::FT_PMU_7, params, "pmu_7", hVASpace);
    }
}

void Trace3DTest::ParseArgsForType(GpuTrace::TraceFileType type,
    const ArgReader *params, const char *memTypeName, LwRm::Handle hVASpace)
{
    Chunk *pChunks = m_Chunks[hVASpace];
    ParseDmaBufferArgs(pChunks[type].m_Surface, params, memTypeName,
        &m_PteKindName[type], &pChunks[type].m_NeedPeerMapping);

    string argName = Utility::StrPrintf("-padding_%s",
        memTypeName);

    m_Padding[type] = params->ParamUnsigned(argName.c_str(), 0);

    pChunks[type].m_Surface.SetName(GpuTrace::GetFileTypeData(type).Description);
}

const char* Trace3DTest::GetTestName()
{
    return m_TestName.c_str();
}

Test *Trace3DTest::Factory(ArgDatabase *args)
{
    unique_ptr<ArgReader> reader(new ArgReader(trace_3d_params));

    if (!reader.get() || !reader->ParseArgs(args))
    {
        ErrPrintf("trace_3d : Factory() error reading device parameters!\n");
        return(0);
    }

    unique_ptr<Trace3DTest> test;
    if (reader->ParamPresent("-usedev") || reader->ParamPresent("-multi_gpu"))
    {
        UINT32 deviceInst = 0;
        string scope;

        deviceInst = reader->ParamUnsigned("-usedev", 0);
        if (!reader->ParamPresent("-usedev") && reader->ParamPresent("-multi_gpu"))
        {
            WarnPrintf("trace_3d : -multi_gpu specified without -usedev.  Testing device 0!\n");
        }

        scope = Utility::StrPrintf("dev%d", deviceInst);

        reader.reset(new ArgReader(trace_3d_params));

        if (!reader.get() || !reader->ParseArgs(args, scope.c_str()))
        {
            ErrPrintf("trace_3d : Factory() error reading device scoped parameters!\n");
            return(0);
        }

        test.reset(new Trace3DTest(reader.get(), deviceInst));
    }
    else
    {
        test.reset(new Trace3DTest(reader.get()));
    }

    test->ParseConlwrrentArgs(reader.release());

    SpecifyCoordinatorSyncCtx(test.get());

    return test.release();
}

void Trace3DTest::SpecifyCoordinatorSyncCtx(Trace3DTest* test)
{
    if (!test->IsTestCtxSwitching())
    {
        DebugPrintf("%s, skip to specify coordinator sync test\n", __FUNCTION__);
        return;
    }
    // Assume that we only need to sync trace_3d tests
    // so first test id is 0;
    if (test->GetTestId() == FIRST_TEST_ID)
    {
        // set it as coordinator for now.
        // later if others are real coordinator, revert it back
        test->SetCoordinatorSyncCtx(true);
    }
    else
    {
        if (test->IsCoordinatorSyncCtx())
        {
            Trace3DTest* firstTest = FindTest(FIRST_TEST_ID);
            MASSERT(firstTest && "first test is not trace_3d test!");
            firstTest->SetCoordinatorSyncCtx(false);
        }
    }
}

//------------------------------------------------------------------------------
//! The return value 1 means that this is a normal case that no need to notify policy
//! manager and do cleanup work before test failes later.
//! The return value 0 means that FailSetup() finishes notifying policy manager and
//! doing cleanup work.
//!
int Trace3DTest::FailSetup()
{
    // Give other threads a chance to run before we start cleaning up
    // Useful for cases when there is an expected fault and the test needs to
    // do some processing before cleanup
    Tasker::Yield();

    PolicyManager *pPM = PolicyManager::Instance();

    if (m_bAborted)
    {
        return 1;
    }

    // If setup fails and was not aborted, notify policy manager
    if (pPM->InTest(pPM->GetTestFromUniquePtr(this)))
    {
        pPM->AbortTest(pPM->GetTestFromUniquePtr(this));
    }

    LWGpuResource *lwgpu = GetGpuResource();
    if (lwgpu && lwgpu->GetContextScheduler())
    {
        RC rc = lwgpu->GetContextScheduler()->UnRegisterTestFromContext();
        if (OK != rc)
        {
            ErrPrintf("UnRegisterTestFromContext failed: %s\n", rc.Message());
            MASSERT(0);
        }
    }

    SetStatus(Test::TEST_NO_RESOURCES);
    for (UINT32 stage = 0; stage < NumCleanUpStages(); ++stage)
        CleanUp(stage);

    return 0;
}

//--------------------------------------------------------------------
//! Begin channel switching on all channels in the test.  Do nothing
//! if channel switching is already running.
//!
RC Trace3DTest::BeginChannelSwitch()
{
    RC rc;

    if (!m_beginSwitchCalled)
    {
        m_beginSwitchCalled = true;

        if ((params->ParamPresent("-ctxswTimeSlice") == 0) &&
            !Utl::ControlsTests())
        {
            // multiple trace3d instances run interleavedly
            CHECK_RC(GetGpuResource()->GetContextScheduler()->AcquireExelwtion());
        }

        for (auto ch : m_TraceCpuManagedChannels)
        {
            ch->BeginChannelSwitch();
        }
    }

    if (m_bAborted)
    {
        // Trace_3d was aborted, probably while waiting for other
        // channels to run
        return RC::USER_ABORTED_SCRIPT;
    }
    return OK;
}

//--------------------------------------------------------------------
//! End channel switching on all channels in the test.  Do nothing if
//! channel switching is not running.
//!
void Trace3DTest::EndChannelSwitch()
{
    if (m_beginSwitchCalled)
    {
        for (auto ch : m_TraceCpuManagedChannels)
        {
            ch->EndChannelSwitch();
        }

        LWGpuContexScheduler::Pointer lwgpuSked = GetGpuResource()->GetContextScheduler();
        if (lwgpuSked && (lwgpuSked->ReleaseExelwtion() != OK))
        {
            ErrPrintf("%s Release exelwtion failed\n", __FUNCTION__);
            MASSERT(false);
        }

        m_beginSwitchCalled = false;
    }
}

//------------------------------------------------------------------------------
RC Trace3DTest::AllocChannels()
{
    RC rc = OK;
    SimClk::EventWrapper event( SimClk::Event::T3D_ALLOC_CH);

    // Alloc per-channel resources.

    if (params->ParamPresent("-fastclear") > 0)
    {
        ErrPrintf("-fastclear is deprecated; use -gpu_clear instead\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    GpuTrace::TraceChannelObjects* channelObjects = GetTrace()->GetChannels();
    for (auto& ch : *channelObjects)
    {
        // dynamic channel is not created in setup stage
        if (ch.IsDynamic())
        {
            continue;
        }

        {
            ChiplibOpScope newScope(string(GetTestName()) + " Alloc channel " + ch.GetName(),
                                    NON_IRQ, ChiplibOpScope::SCOPE_COMMON, NULL);

            // Create channel
            CHECK_RC(ch.AllocChannel(params));
        }

        // Create channel object
        CHECK_RC(ch.AllocObjects());

        // Add created channel to list
        CHECK_RC(AddChannel(&ch));

        // Other operations are for cpu managed channel
        if (ch.IsGpuManagedChannel())
        {
            continue;
        }

        LWGpuResource * lwgpu = GetGpuResource();
        if ((ch.GetWfiMethod() == WFI_INTR) && lwgpu->IsHandleIntrTest())
        {
            ErrPrintf("-wfi_intr is not compatible with -handleIntr\n");
            return RC::SOFTWARE_ERROR;
        }

        CHECK_RC(SetupCtxswZlwll(&ch));
        CHECK_RC(SetupCtxswPm(&ch));
        CHECK_RC(SetupCtxswSmpc(&ch));

        if (EngineClasses::IsGpuFamilyClassOrLater(
            TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_PASCAL_A))
        {
            CHECK_RC(SetupCtxswPreemption(&ch));
        }
    }

    LWGpuResource * lwgpu = GetGpuResource();
    TraceSubChannel* psubch = 0;
    if (GetGrChannel())
    {
        psubch = GetGrChannel()->GetGrSubChannel();
    }
    else if (GetDefaultChannel())
    {
        psubch = GetDefaultChannel()->GetSubChannel("");
    }

    LWGpuSubChannel* subChannel = 0;
    if (psubch)
        subChannel = psubch->GetSubCh();

    gsm = lwgpu->CreateGpuSurfaceMgr(this, params, subChannel);
    if (!gsm)
    {
        ErrPrintf("Surface Setup for class 0x%x failed\n", psubch->GetClass());
        return RC::SOFTWARE_ERROR;
    }

    // tell the surface manager to use the normal surface group
    gsm->UseTraceSurfaces();

    CHECK_RC(m_Trace.EnableColorSurfaces(gsm));

    // now retrieve the surfacepointers we want to track
    int i;
    for (i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        surfC[i] = gsm->GetSurface(ColorSurfaceTypes[i], 0);
    }
    surfZ = gsm->GetSurface(SURFACE_TYPE_Z, 0);

    // tell surfmgr if we have a clear_init, bug 394532
    gsm->SetClearInit(m_Trace.GetClearInit());

    return OK;
}

//----------------------------------------------------------------------------

RC Trace3DTest::MapSliSurfaces()
{
    RC rc = OK;

    if ((params->ParamPresent("-sli_local_surfaces") == 0 ||
         params->ParamPresent("-sli_remote_surfaces") == 0)/* &&
         _DMA_TARGET_P2P != (_DMA_TARGET)params->ParamUnsigned("-loc_tex")*/)
    {
        DebugPrintf(MSGID(SLI), "%s: User wants all the surfaces local, so no work to do\n",
                    __FUNCTION__);
        return rc;
    }

    // TODO_GPS: use new API when it's ready.
    InfoPrintf("%s: Looking for subdevices ...\n", __FUNCTION__);
    GpuDevice* pGpuDev = GetBoundGpuDevice();
    if (pGpuDev == NULL)
    {
        ErrPrintf("%s: Error in GetBoundGpuDevice.\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    InfoPrintf("%s: Bound Device has %d subdevices\n",
               __FUNCTION__, pGpuDev->GetNumSubdevices());

    if (pGpuDev->GetNumSubdevices() < 2)
    {
        InfoPrintf("%s: No SLI configuration: bailing out\n", __FUNCTION__);
        return rc;
    }

    // Initialize the mapper
    InfoPrintf("%s: Pushing surfaces in SliSurfaceManager ...\n", __FUNCTION__);
    GetSSM()->SetParams(params);

    if (GetSSM()->Setup() != OK)
    {
        ErrPrintf("%s: Error in SliSurfaceMapper setup.\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    // NOTE: Add all the surfaces that have to be peer-ed.
    // For major control, in future push also some info like "color surface,
    // ..."

    // Trace3dTest -> GpuTrace -> TraceModule
    GpuTrace* p_trace = GetTrace();
    if (p_trace)
    {
        for (ModuleIter modIt = p_trace->ModBegin();
             modIt != p_trace->ModEnd(); ++modIt)
        {
            TraceModule* trm = *modIt;

            if (trm == 0)
            {
                // Not sure if this is actually legal but ignore for now.
            }
            else if (trm->IsColorOrZ())
            {
                // SLI mapping for Color and Z surfaces is handled
                // by the surface manager.
            }
            else if (GpuTrace::FT_TEXTURE_HEADER != trm->GetFileType())
            {
                InfoPrintf("%s: adding TraceModule trm= %p, name= %s, size= %llx\n",
                           __FUNCTION__, trm, trm->GetName().c_str(), trm->GetSize());

                if (trm->CanBeSliRemote())
                {
                    MdiagSurf* surf = trm->GetDmaBufferNonConst();
                    // Also for debuging purpose update the surface name for printing
                    if (surf->GetName().empty())
                    {
                        surf->SetName(trm->GetName());
                    }

                    InfoPrintf("%s: adding GetDmaBuffer()= %p (%s)\n",
                               __FUNCTION__, surf, surf->GetName().c_str());

                    // Exclude RTT buffers from SLI allocation
                    // The reason to get the tracemodule relocation count
                    // is to avoid that surface being accessed partial remotely,
                    // otherwise we can't handle relocation in that case.
                    bool canBePartialRemote = ((GenericTraceModule*)trm)->GetReloc().empty();
                    GetSSM()->AddDmaBuffer(pGpuDev, surf, canBePartialRemote);
                }
            }
        }
    }

    // Clip ID surface
    IGpuSurfaceMgr* surfMgr = GetSurfaceMgr();
    if (surfMgr && surfMgr->GetNeedClipID())
    {
        MdiagSurf* surf = surfMgr->GetClipIDSurface();
        if (surf)
        {
            InfoPrintf("%s: adding cIdSurf= %p\n", __FUNCTION__, surf);
            if (surf->GetName().empty())
            {
                surf->SetName("ClipId");
            }
            GetSSM()->AddDmaBuffer(pGpuDev, surf, true);
        }
    }

    // Allocate the peer surfaces.
    InfoPrintf("%s: Pushed %d surfaces in SliSurfaceMapper\n",
               __FUNCTION__, GetSSM()->GetSurfaceNumber(pGpuDev));

    InfoPrintf("%s: Peering surfaces ...\n", __FUNCTION__);
    rc = GetSSM()->AllocateSurfaces(pGpuDev);
    if (OK != rc)
    {
        ErrPrintf("%s: Error in SliSurfaceMapper::AllocateSurfaces.\n", __FUNCTION__);
        return rc;
    }
    // Patch the local/remote info for each dma buffer
    typedef SliSurfaceMapper::Name2Buff_type Name2Buff_type;
    Name2Buff_type BuffLocMap = GetSSM()->GetBuffLocInfo();
    for (const auto& name2Buff: BuffLocMap)
    {
        const string name = name2Buff.first;
        const UINT32 access = name2Buff.second.access;
        const UINT32 host = name2Buff.second.host;
        const MdiagSurf *buff = name2Buff.second.buff;
        m_BuffInfo.SetTarget(name, access==host);
        if (access != host)
        {
            // For peer surfaces, set the peer VA
            m_BuffInfo.SetVA(name, buff->GetCtxDmaOffsetGpuPeer(host)
                + buff->GetExtraAllocSize());
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC Trace3DTest::ParseOptionalPmFile()
{
    if (params->ParamPresent("-pm_file"))
    {
        // -pm_file and -pm_sb_file should be exlwslive in PM test.
        if (s_LwrrentTestsSetting.hasPmSbFileArg)
        {
             ErrPrintf("'-pm_file' and '-pm_sb_file' should not be used together");
             return RC::BAD_PARAMETER;
        }

        // Multiple tests with pm_file args is not allowed.
        if (s_LwrrentTestsSetting.hasPmFileArg)
        {
            ErrPrintf("There should be only one test with '-pm_file'");
            return RC::BAD_PARAMETER;
        }

        s_LwrrentTestsSetting.hasPmFileArg = true;
        s_LwrrentTestsSetting.hasTuT = true;
        m_IsTuT = true;
    }

    if (params->ParamPresent("-pm_sb_file"))
    {
        // -pm_file and -pm_sb_file should be exlwslive in PM test.
        if (s_LwrrentTestsSetting.hasPmFileArg)
        {
             ErrPrintf("'-pm_file' and '-pm_sb_file' should not be used together");
             return RC::BAD_PARAMETER;
        }

        if (s_LwrrentTestsSetting.hasPmSbFileArg)
        {
            // Mutilple tests have pm file args. This is no longer TuT case.
            InfoPrintf("Mutilple tests have -pm_sb_file. Make sure all the tests have the same file name and"
                       "PMTRIGGER_SYNC_EVENT is used in the trace\n ");
            s_LwrrentTestsSetting.hasTuT = false;
        }
        else
        {
            m_IsTuT = true;
            s_LwrrentTestsSetting.hasTuT = true;
        }

        s_LwrrentTestsSetting.hasPmSbFileArg = true;
    }

    return OK;
}

//------------------------------------------------------------------------------
RC Trace3DTest::ParseOptionalRegisters()
{
    StickyRC src;
    vector<CRegisterControl> ctxRegCtrls;
    multimap<string, tuple<ArgDatabase*, ArgReader*>> privArgMap;
    unique_ptr<ArgReader>   privParam;

    vector<CRegisterControl>& mdiagRegCtrls = m_pBoundGpu->GetPrivRegs();

    src = m_pBoundGpu->ProcessRegBlock(params, "-ctx_reg", ctxRegCtrls);
    for (auto reg : ctxRegCtrls)
    {
        if (reg.m_domain != RegHalDomain::RAW)
        {
            ErrPrintf("-ctx_reg should not have domain regsiter. Correct usage is '-ctx_priv +begin <RegSpace> -ctx_reg ... +end'\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    // process the list of  -ctx_priv options
    if (OK !=  m_pBoundGpu->ParseRegBlock(params,
            "Correct usage is '-ctx_priv +begin gpu -ctx_reg ... +end'\n"
            "Or, just '-ctx_reg ...'\n"
            "Or '-ctx_priv +begin <RegSpace> -ctx_reg ... +end' for domain register",
            "-ctx_priv", &privArgMap, trace_3d_params))
    {
        ErrPrintf("Error in parsing -ctx_reg cmdline arg\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    for (auto arg : privArgMap)
    {
        ArgDatabase *argDB;
        ArgReader *privParams;

        string regSpace = arg.first;
        tie(argDB, privParams) = arg.second;
        vector<CRegisterControl> regCtrls;
        src = m_pBoundGpu->ProcessRegBlock(privParams, "-ctx_reg", regCtrls);

        for (auto& reg : regCtrls)
        {
            // RegSpace(LW_CONTEXT) will be used in RAW case too(such as PGRAPH), so assign the regSpace to reg
            reg.m_regSpace = regSpace;
            // Handle domain register which is zero based register address
            if (reg.m_domain != RegHalDomain::RAW)
            {
                // need to adjust the domain for multicast lwlink
                // Only affect single register regspace
                reg.m_domain = MDiagUtils::AdjustDomain(reg.m_domain, reg.m_regSpace);
            }
            ctxRegCtrls.push_back(reg);
        }

        delete argDB;
        delete privParams;
    }

    for (auto& mdiagRegCtrl : mdiagRegCtrls)
    {
        src = AddRegCtrl(mdiagRegCtrl);
    }

    for (auto& ctxRegCtrl : ctxRegCtrls)
    {
        ctxRegCtrl.m_isCtxReg = true;
        src = AddRegCtrl(ctxRegCtrl);
    }

    return src;
}
//------------------------------------------------------------------------------
RC Trace3DTest::CheckCtxRegCtrl(LwRm::Handle handle, const CRegisterControl& pendingRegCtrl)
{
    bool conflict = false;
    vector<CRegisterControl> & regCtrlsPerHandle = s_LwrrentTestsSetting.ctxRegCtrlMap[handle];
    for (auto& savedRegCtrl : regCtrlsPerHandle)
    {
        // allow different ops
        if (pendingRegCtrl.m_action != savedRegCtrl.m_action ||
            pendingRegCtrl.m_address != savedRegCtrl.m_address ||
            pendingRegCtrl.m_domain != savedRegCtrl.m_domain ||
            pendingRegCtrl.m_regSpace != savedRegCtrl.m_regSpace ||
            (pendingRegCtrl.m_mask & savedRegCtrl.m_mask) == 0)
            continue;
        else
        {
            conflict = true;
            // Assume it applies to all contexts, replace and give warning if conflicts
            // Note not allow traces using same context by -tsg_name with -ctx_reg LW_CONTEXT by now
            // Todo: Handle confliction if LW_CONTEXT_tsg/channel and -tsg_name is specified, error out by now
            WarnPrintf("Conflict register at 0x%08x, replaced with the following one \n", pendingRegCtrl.m_address);
            pendingRegCtrl.PrintRegInfo();
            savedRegCtrl = pendingRegCtrl;
        }
    }
    if (!conflict)
    {
        // No conflict, save the pendingRegCtrl
        // Note that global register will be overwrite without warning!
        regCtrlsPerHandle.push_back(pendingRegCtrl);
    }
    return OK;
}

//------------------------------------------------------------------------------
RC Trace3DTest::AddSmcRegCtrl(const CRegisterControl& pendingRegCtrl)
{
    bool conflict = false;
    // Default m_SmcEngineName in non-smc mode is "unspecified_smc_engine"
    vector<CRegisterControl>& regCtrlsPerSmc = s_LwrrentTestsSetting.regCtrlMap[m_SmcEngineName];
    for (auto& savedRegCtrl : regCtrlsPerSmc)
    {
        // Check conflict with already saved RegCtrl.
        if (pendingRegCtrl.m_address != savedRegCtrl.m_address ||
            pendingRegCtrl.m_domain != savedRegCtrl.m_domain ||
            pendingRegCtrl.m_regSpace != savedRegCtrl.m_regSpace ||
            (pendingRegCtrl.m_mask & savedRegCtrl.m_mask) == 0)
            continue;

        // Conflict on the same register address and field.
        if (!(savedRegCtrl == pendingRegCtrl) && savedRegCtrl.m_isCtxReg && pendingRegCtrl.m_isCtxReg)
        {
            // Case 1. the conlfict is from two trace3d tests with different operations
            // on the same register address and field.
            ErrPrintf("Conflict operation on the same register :\n");
            savedRegCtrl.PrintRegInfo();
            pendingRegCtrl.PrintRegInfo();
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        if (!savedRegCtrl.m_isCtxReg && pendingRegCtrl.m_isCtxReg )
        {
            // Case 2. The already saved register is from mdiag
            // the pengding register is from trace3d. Override the saved one.
                savedRegCtrl = pendingRegCtrl;
        }
        // For other cases, no change to the saved RegCtrl.
        conflict = true;
        break;
    }
    if (!conflict)
    {
        // No conflict. Save the pending RegCtrl.
        // Save it first.
        regCtrlsPerSmc.push_back(pendingRegCtrl);
    }
    return OK;
}
//------------------------------------------------------------------------------
// Add a RegCtrl to current setting, check conflict apart from LW_CONTEXT
RC Trace3DTest::AddRegCtrl(const CRegisterControl& pendingRegCtrl)
{
    StickyRC src;
    // per trace register
    bool isCtxRegSpace = pendingRegCtrl.m_regSpace.rfind("LW_CONTEXT", 0) == 0;
    if (!m_pBoundGpu->IsSMCEnabled() && !isCtxRegSpace)
    {
        // If not in SMC mode, there should be no '-ctx_reg' without LW_CONTEXT.
        if (pendingRegCtrl.m_isCtxReg)
        {
            ErrPrintf("SMC is not enabled but '-ctx_reg' without specifing context is used in Trace3d\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        // The register is already handled in mdiag level. Skip it;
        return OK;
    }

    // -ctx_reg only handles PGRAPH/RUNLIST/CHRAM registers.
    // Todo: IsCtxReg means PGRAPH|CHRAM|RUNLIST, should rename to isSmcReg to avoid confusion
    bool isCtxReg = m_pBoundGpu->IsCtxReg(pendingRegCtrl.m_address);
    if (!isCtxReg)
    {
        if (pendingRegCtrl.m_isCtxReg)
        {
            ErrPrintf("Register 0x%08x is not allowed for '-ctx_reg'. Make sure the register is PGRAPH/RUNLIST/CHRAM \n", pendingRegCtrl.m_address);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        DebugPrintf("For Non PGRAPH/RUNLIST/CHRAM register from mdiag. Ignore register:0x%08x in trace3d\n", pendingRegCtrl.m_address);
        return OK;
    }

    // Add first, handle ctxReg conflict after channel create stage
    if (isCtxRegSpace)
    {
        if (params->ParamPresent("-tsg_name"))
        {
            ErrPrintf("Not support '-tsg_name' with -ctx_priv LW_CONTEXT by now \n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        else
        {
            m_traceRegCtrls.push_back(pendingRegCtrl);
        }
    }
    // Should reslove the conflict on the same register in smc.
    else
    {
        src = AddSmcRegCtrl(pendingRegCtrl);
    }
    return src;
}

//! \brief Get rm handle list to operate by regSpace----------------------------
//!
//! return OK if parsed successfully
RC Trace3DTest::GetHandleByRegSpace
(
    string regSpace,
    LwRm::Handles& handles,
    TraceChannel* pTraceChannel /* = nullptr */
)
{
    RC rc;
    const string contextPrefix = "LW_CONTEXT_";
    // No specified channel, get them all
    if (pTraceChannel == nullptr)
    {
        if (regSpace.find(contextPrefix + "ALL") != string::npos)
        {
            // Append all channel
            const TraceChannels *traceChannels = GetChannels();
            for (auto& tc : *traceChannels)
            {
                handles.insert(tc->GetCh()->ChannelHandle());
            }
        }
        // Only support LW_CONTEXT_ALL by now
        else
        {
            ErrPrintf("Can't find the context name in regspace: %s\n", regSpace.c_str());
            return RC::BAD_PARAMETER;
        }
    }
    // Only return the intersection
    else
    {
        if (regSpace.find(contextPrefix + "ALL") != string::npos)
        {
            handles.insert(pTraceChannel->GetCh()->ChannelHandle());
        }
        else
        // Only support LW_CONTEXT_ALL by now, may need to get tsg handler in future
        {
            ErrPrintf("Can't find the context name in regspace: %s\n", regSpace.c_str());
            return RC::BAD_PARAMETER;
        }
    }
    return OK;
}
//------------------------------------------------------------------------------
RC Trace3DTest::HandleOptionalRegisters()
{
    // Handle per smc register
    vector<CRegisterControl>& regCtrlsPerSmc = s_LwrrentTestsSetting.regCtrlMap[m_SmcEngineName];
    // Todo: It should be an error if register is RUNLIST|CHRAM but without using LW_ENGINE regspace
    for (auto& regCtrl : regCtrlsPerSmc)
    {
        if (regCtrl.m_regSpace == "" && m_pBoundGpu->IsPerRunlistReg(regCtrl.m_address))
        {
            regCtrl.m_regSpace = "LW_ENGINE_GR0";
        }
    }

    RC rc = m_pBoundGpu->HandleOptionalRegisters(&regCtrlsPerSmc, Gpu::UNSPECIFIED_SUBDEV,
                                                m_pSmcEngine);

    s_LwrrentTestsSetting.regCtrlMap.erase(m_SmcEngineName);
    return rc;
}
//------------------------------------------------------------------------------
RC Trace3DTest::HandleOptionalCtxRegisters
(
    TraceChannel* pTraceChannel /* = nullptr */
)
{
    // Handle per trace register in trace_3d
    StickyRC src;
    GpuSubdevice *pSubdev = GetGpuResource()->GetGpuSubdevice(Gpu::UNSPECIFIED_SUBDEV);
    LwRm *pLwRm = GetLwRmPtr();
    UINT32 rd_data = 0;
    UINT32 wr_data = 0;
    for (auto& regc : m_traceRegCtrls)
    {
        regc.PrintRegInfo();
        LwRm::Handles handles;
        src = GetHandleByRegSpace(regc.m_regSpace, handles, pTraceChannel);
        for (auto& hdl : handles)
        {
            // Check if tests are using same channel by -use_channel or operating same register
            // Give warning if conflicts
            CheckCtxRegCtrl(hdl, regc);
            switch (regc.m_action)
            {
                case CRegisterControl::EWRITE:
                case CRegisterControl::EARRAY1WRITE:
                case CRegisterControl::EARRAY2WRITE:
                    pSubdev->CtxRegWr32(hdl, regc.m_address, regc.m_data, pLwRm);
                    InfoPrintf("results: wr_addr=0x%08x, wr_data=0x%08x\n", regc.m_address, regc.m_data);
                    break;
                case CRegisterControl::EREAD:
                case CRegisterControl::EARRAY1READ:
                case CRegisterControl::EARRAY2READ:
                    pSubdev->CtxRegRd32(hdl, regc.m_address, &regc.m_data, pLwRm);
                    InfoPrintf("results: rd_data=0x%08x\n", regc.m_data);
                    break;
                case CRegisterControl::EMODIFY:
                case CRegisterControl::EARRAY1MODIFY:
                case CRegisterControl::EARRAY2MODIFY:
                    pSubdev->CtxRegRd32(hdl, regc.m_address, &rd_data, pLwRm);
                    wr_data = (regc.m_data & regc.m_mask) | (rd_data & ~regc.m_mask);
                    pSubdev->CtxRegWr32(hdl, regc.m_address, wr_data, pLwRm);
                    InfoPrintf("results: rd_data=0x%08x  wr_data=0x%08x\n", rd_data, wr_data);
                    break;
                case CRegisterControl::ECOMPARE:
                    pSubdev->CtxRegRd32(hdl,regc.m_address, &rd_data, pLwRm);
                    InfoPrintf("results:  0x%08x=0x%08x\n", regc.m_data, rd_data);
                    if (rd_data != regc.m_data)
                    {
                        ErrPrintf("Register read/compare failed: addr=0x%08x expected=0x%08x actual=0x%08x\n",
                            regc.m_address, regc.m_data, rd_data);
                        src = RC::DATA_MISMATCH;
                    }
                    break;
                default:
                    DebugPrintf("Ignoring unknown register action\n");
                    break;
            }
        }
        if (src != OK)
        {
            ErrPrintf("Handling the following register and action failed:\n");
            regc.PrintRegInfo();
            return src;
        }
    }
    return src;
}
//------------------------------------------------------------------------------
int Trace3DTest::Setup()
{
    ErrPrintf("%s : Multistage setup required, use Setup(stage).\n", __FUNCTION__);
    return 0;
}

//------------------------------------------------------------------------------
int Trace3DTest::Setup(int stage)
{
    int rc = 0;
    switch (stage)
    {
        case 0:
            TestSynlwp(MDiagUtils::TestSetupStart);
            rc = SetupStage0();
            break;
        case 1:
            rc = SetupStage1();
            break;
        case 2:
            rc = SetupStage2();
            break;
        default:
            ErrPrintf("%s : Invalid setup stage %d.\n", __FUNCTION__, stage);
            break;
    }
    if (!rc || 2 == stage)
    {
        // Setup failed or finished last stage setup.
        TestSynlwp(MDiagUtils::TestSetupEnd);
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Perform stage 0 of test setup.
//!
//! This stage acquires any resources, reads the trace header file and allocates
//! any necessary channels.  Any surface allocation is delayed until stage 1
//! because it is required that any surfaces being peer'd from a different test
//! be flagged as a peer surface prior to surface allocation
//!
//! \return 1 if successful, 0 if a failure oclwrred
int Trace3DTest::SetupStage0()
{
    CHIPLIB_CALLSCOPE_WITH_TESTNAME;

    if (!params->ValidateConstraints(trace_file_constraints))
    {
        ErrPrintf("Test parameters are not valid- please check previous messages\n");
        return FailSetup();
    }

    RC rc;

    // Set up floating point chop mode
    init_lwfloat();

    output_filename = params->ParamStr("-o", "test");
    output_filename = UtilStrTranslate(output_filename, DIR_SEPARATOR_CHAR2, DIR_SEPARATOR_CHAR);

    m_CrcDumpMode = (CrcEnums::CRC_MODE)params->ParamUnsigned("-crcMode", CrcEnums::CRC_CHECK);

    if (params->ParamPresent("-fermi_gob") > 0)
        GetBoundGpuDevice()->SetGobHeight(8);

    // Important -- do this before anything else once you know your options (like -o)
    // Even if setup failed, initialize with the output filename so we can log errors to <tracename>.err
    getStateReport()->init(output_filename.c_str());

    // For serial mode, parse it in SetupStage0
    if (!m_IsCtxSwitchTest)
    {
        if (OK != GetBoundResources())
        {
            ErrPrintf("GetBoundResources failed: %s \n", rc.Message());
            return FailSetup();
        }

        if (OK != ParseOptionalRegisters())
        {
            ErrPrintf("ParseOptionalRegisters failed: %s \n", rc.Message());
            return FailSetup();
        }
    }

    // Get the SmcEngine* and LwRm* for this test
    m_pSmcEngine = GetGpuResource()->GetSmcEngine(m_SmcEngineName);

    if (m_pSmcEngine)
    {
        m_pLwRm = GetGpuResource()->GetLwRmPtr(m_pSmcEngine);
    }

    if (Utl::HasInstance())
    {
        UINT32 paramCount = params->ParamPresent("-utl_test_script");

        for (UINT32 i = 0; i < paramCount; ++i)
        {
            string scriptParam = params->ParamNStr("-utl_test_script", i, 0);
            if (OK != Utl::Instance()->ReadTestSpecificScript(this, scriptParam))
            {
                return FailSetup();
            }
        }

        Utl::Instance()->TriggerTestInitEvent(this);
    }

    if (HandleOptionalRegisters() != OK)
    {
        ErrPrintf("trace_3d: failed to process '-reg'\n");
        return FailSetup();
    }

    // if command line specifies a plugin, load and initialize it now
    //
    if ( params->ParamPresent( "-plugin" ) )
    {
        const char * pluginInfo = params->ParamStr( "-plugin" );
        rc = LoadPlugin( pluginInfo );
        if ( rc != OK )
        {
            ErrPrintf("trace_3d: unable to instantiate plugin: '%s'\n", pluginInfo );
            return FailSetup();
        }
    }

    //////////////////////////// Begin Trace Event ////////////////////////
    //
    rc = traceEvent(TraceEventType::BeginTrace3DSetup);
    if ( rc != OK )
    {
        ErrPrintf("trace_3d: Error in processing trace event BeginTrace3DSetup:\n" );
        return FailSetup();
    }
    //
    ///////////////////////////  End Trace Event /////////////////////////

    // Read header to figure out which classes we really need and try to get a
    // GPU for that one
    // read the requested trace file
    if (params->ParamPresent("-i"))
    {
        header_file = params->ParamStr("-i");
        header_file = UtilStrTranslate(header_file, DIR_SEPARATOR_CHAR2, DIR_SEPARATOR_CHAR);

        if (ChiplibTraceDumper::GetPtr()->DumpChiplibTrace())
        {
            string testName = LwDiagUtils::StripDirectory(output_filename.c_str());
            string tracePath = LwDiagUtils::StripFilename(header_file.c_str());

            ChiplibTraceDumper::GetPtr()->SetTestNamePath(testName, tracePath);

            if (params->ParamPresent("-backdoor_archive_id"))
            {
                ChiplibTraceDumper::GetPtr()->SetBackdoorArchiveId(params->ParamStr("-backdoor_archive_id"));
            }
        }
    }
    else
    {
        if (!params->ParamPresent("-no_trace"))
        {
            ErrPrintf("trace_3d: you must specify either -i or -no_trace.\n");
            return FailSetup();
        }
    }

    // Initialize the TraceFileMgr
    bool untarInMemory = false;
    if (params->ParamPresent("-untar_method"))
    {
        if (params->ParamUnsigned("-untar_method") == 1)
        {
            untarInMemory = true;
        }
    }
    else if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        untarInMemory = true;
    }

    if (params->ParamPresent("-i"))
    {
        rc = m_TraceFileMgr.Initialize(header_file, untarInMemory, this);

        if (rc != OK)
        {
            ErrPrintf("trace_3d: couldn't initialize trace file manager\n");
            return FailSetup();
        }

        // From here on out, prefer to find other files in the tar archive,
        // if one is present.
        m_TraceFileMgr.SearchArchiveFirst(true);

        if (params->ParamPresent("-file_search_order"))
        {
            if (params->ParamUnsigned("-file_search_order") == 0)
            {
                m_TraceFileMgr.SearchArchiveFirst(true);
            }
            else
            {
                m_TraceFileMgr.SearchArchiveFirst(false);
            }
        }
    }

    // Initialize wfi_method with a proper value (CMD/default)
    // Trace header can (re)define it if -wfi_method is absent
    UINT32 wfimethod = WFI_INTR;
    if (Platform::GetSimulationMode() == Platform::Amodel)
        wfimethod = WFI_NOTIFY;
    else if (Platform::IsVirtFunMode())
        wfimethod = WFI_INTR_NONSTALL;

    if (params->ParamPresent("-wfi_method"))
    {
        wfimethod = params->ParamUnsigned("-wfi_method", wfimethod);
    }
    SetWfiMethod((WfiMethod)wfimethod);

    if (params->ParamPresent("-partition_table"))
    {
        string filename = params->ParamStr("-partition_table");
        rc = m_PEConfigureFile.ReadHeader(filename.c_str(), LwU64_LO32(GetTimeoutMs()));
        if (OK != rc)
        {
            return FailSetup();
        }
    }
    if (params->ParamPresent("-i") &&
        OK != (rc = m_Trace.ReadHeader(header_file.c_str(), LwU64_LO32(GetTimeoutMs()))))
    {
        return FailSetup();
    }

    // number of PMTRIGGER_SYNC_EVENT pair must be same in all the conlwrrent running tests
    if (!CheckPMSyncNumInConlwrrentTests())
    {
        return FailSetup();
    }

    if (Utl::HasInstance())
    {
        Utl::Instance()->InitVaSpace();
    }

    //////////////////////////// Begin Trace Event ////////////////////////
    //
    // Trace3dPlugin event: "afterReadTraceHeader".
    //
    // TRACE_3D FLOW: At this point, trace_3d has read and partially processed
    // the header file of the trace ( by calling m_Trace.ReadHeader() ).   A
    // list of "TraceModule" objects has been created, one object for each FILE
    // statement in the trace.
    //
    // MEANING: A plugin that handles this event may create additional
    // TraceModules via the plugin BufferManager, and trace_3d will behave just
    // as if those plugin-created trace modules had come from FILE statements in
    // the trace header [*].
    //
    // NOTE: if the call of m_Trace.ReadHeader() is moved, or if the work it
    // does is changed, then the location of this event may also need to change
    // to preserve its meaning.
    //
    // [*] There is no data in the trace file for the plugi-created surface.
    // The plugin must supply the data for the surface it wants to add to the
    // trace.   Since trace_3d will soon (almost immediately) look for the file
    // data for each TraceModule object in the trace archive ( in
    // m_Trace.ReadTrace(), see below ), class TraceModule has been extended to
    // include a boolean "fromPlugin" data member.  Trace_3d skips the
    // archive file lookup for those TraceModules having "fromPlugin == true".
    //
    rc = traceEvent(TraceEventType::AfterReadTraceHeader);
    if ( rc != OK )
    {
        ErrPrintf("trace_3d: Error in processing trace event AfterReadTraceHeader:\n" );
        return FailSetup();
    }
    //
    ///////////////////////////  End Trace Event /////////////////////////

    // This command line argument is used to get around the problem that a trace
    // wants MODS to act as though ALLOC_SURFACE commands were used but doesn't
    // actually have any color/z surfaces to declare.  (bug 584583)
    if (params->ParamPresent("-use_alloc_surface_for_rt_alloc_only") > 0)
    {
        SetAllocSurfaceCommandPresent(true);
    }

    conlwrrent = (params->ParamPresent("-conlwrrent") > 0);

    if (!m_Trace.ReadTrace())
    {
        ErrPrintf("trace_3d: failed to read trace data files.\n");
        return FailSetup();
    }

    if (OK != m_Trace.AddVirtualAllocations())
    {
        ErrPrintf("trace_3d: failed adding extra virtual allocations.\n");
        return FailSetup();
    }

    if (OK != m_Trace.DoVP2TilingLayout())
    {
        ErrPrintf("trace_3d: failed to do vp2 tiling layout.\n");
        return FailSetup();
    }

    if (conlwrrent && (m_TraceCpuManagedChannels.size() > 1))
    {
        ErrPrintf("-conlwrrent is broken for traces with multiple channels.\n");
        return FailSetup();
    }

    if (OK != AllocChannels())
    {
        ErrPrintf("Failed to alloc channel/object resources.\n");
        return FailSetup();
    }

    // Handle context register control after channels are allocated
    if (OK != HandleOptionalCtxRegisters())
    {
        ErrPrintf("Failed to handle Context register.\n");
        return FailSetup();
    }

    getStateReport()->init(params->ParamStr("-o", ""));

    return 1;
}

//------------------------------------------------------------------------------
//! \brief Perform stage 1 of test setup.
//!
//! For peer tests and non-context switching tests, this stage allocates all
//! surfaces.  For context switching, non-peer tests allocation is delayed until
//! the test is exelwted from BeginIteration
//!
//! \return 1 if successful, 0 if a failure oclwrred
int Trace3DTest::SetupStage1()
{
    CHIPLIB_CALLSCOPE_WITH_TESTNAME;

    if (m_SkipDevInit)
    {
        return 1;
    }

    // A peer test must never be run in serial exelwtion mode
    if (IsPeer())
    {
        if (LWGpuResource::GetTestDirectory()->IsSerialExelwtion())
        {
            ErrPrintf("trace_3d : A peer test must never be run in serial exelwtion mode\n");
            return FailSetup();
        }

        if (GetBoundGpuDevice()->GetNumSubdevices() > 1)
        {
            ErrPrintf("trace_3d : peer tests are not supported on SLI devices.\n");
            return FailSetup();
        }
    }

    // If a different instance of a Trace3DTest is using a surface from
    // the current test as a "peer" surface, then the other test has
    // registered with this instance using the test and surface name and
    // providing the GpuDevice it is running on.  Loop through all the
    // modules from the current test that are being "peer'd" by other
    // tests and provide those modules with a set of GpuDevices that
    // need mappings to their surface
    for (auto& surface : m_PeerMappedSurfaces)
    {
        if (m_Trace.FindSurface(surface.first) == SURFACE_TYPE_UNKNOWN)
        {
            TraceModule * ptm = m_Trace.ModFind(surface.first.c_str());
            if (0 != ptm)
            {
                if (ptm->IsPeer())
                {
                    ErrPrintf("Unable to peer map %s since it is also a peer surface\n",
                              surface.first.c_str());
                    return FailSetup();
                }
                ptm->AddPeerGpuMappings(&surface.second);
            }
            else
            {
                ErrPrintf("Unknown surface being peer mapped : %s\n", surface.first.c_str());
                return FailSetup();
            }
        }
    }

    if (!SetupSurfaces())
    {
        // If hits some errors, FailSetup() is called by SetupSurfaces().
        ErrPrintf("Failed to allocate surfaces.\n");
        return 0;
    }

    return 1;
}

//------------------------------------------------------------------------------
//! \brief Perform stage 2 of test setup.
//!
//! This stage sets up any peer modules (modules in this test that reference
//! modules or surfaces from a different test).
//!
//! \return 1 if successful, 0 if a failure oclwrred
int Trace3DTest::SetupStage2()
{
    CHIPLIB_CALLSCOPE_WITH_TESTNAME;

    if (m_SkipDevInit)
    {
        return 1;
    }

    // Setup any peer modules (i.e. pull in pointers to the module/surface from
    // the test being peered).  Once they have been pulled in, verify that
    // the current test is configured to utilize those modules correctly
    for (ModuleIter modIt =  m_Trace.ModBegin(false, true);
         modIt !=  m_Trace.ModEnd(false, true); ++modIt)
    {
        // Color and Z surface SLI is handled by the surface manager.
        if ((*modIt)->IsColorOrZ())
        {
            continue;
        }

        if (OK != (*modIt)->SetupPeer())
        {
            ErrPrintf("Peer module setup failed\n");
            return FailSetup();
        }

        // Validate that necessaary VP2 Relocs are present for VP2 peer modules
        if ((*modIt)->IsVP2TilingSupported())
        {
            if (!(*modIt)->AreRequiredVP2TileModeRelocsPresent())
            {
                ErrPrintf("At least one RELOC_VP2_TILEMODE is needed for %s\n",
                          (*modIt)->GetName().c_str());
                return FailSetup();
            }
        }
    }

    if ((IsPeer() || !IsTestCtxSwitching()) && !Utl::ControlsTests())
    {
        if (!FinishSetup() && !m_bAborted)
        {
            //If hit some errors, FailSetup() is called by FinishSetup().
            ErrPrintf("Failed to complete setup.\n");
            return 0;
        }
        else if (m_bAborted)
        {
            return 1;
        }
    }

    return 1;
}

//------------------------------------------------------------------------------
//! \brief Setup the surfaces for use in the test.
//!
//! This includes allocating notifiers and surfaces, and mapping appropriate
//! surfaces
//!
//! \return 1 if successful, 0 if a failure oclwrred
int Trace3DTest::SetupSurfaces()
{
    CHIPLIB_CALLSCOPE_WITH_TESTNAME;
    SimClk::EventWrapper event(SimClk::Event::T3D_SETUP_SURF);

    RC rc;

    //////////////////////////// Begin Trace Event ////////////////////////
    //
    // Trace3dPlugin event: "BeforeTraceSurfacesSetup".
    //
    // TRACE_3D FLOW: At this point, trace_3d hasn't setup any trace surfaces.
    //
    // MEANING: A plugin that handles this event may setup the surface sharing
    // names like cheetah shared surface, or global shared surface.
    //
    // NOTE: if the call of Trace3DTest.SetupSurfaces() is moved, or if the work
    // it does is changed, then the location of this event may also need to
    // change to preserve its meaning.
    //
    rc = traceEvent(TraceEventType::BeforeTraceSurfacesSetup);
    if ( rc != OK )
    {
        ErrPrintf("trace_3d: Error in processing trace event BeforeTraceSurfacesSetup\n" );
        return FailSetup();
    }

    TraceChannel* pTraceChannel = GetDefaultChannel();
    LWGpuResource * lwgpu = GetGpuResource();

    const TraceChannels *tChannels = GetChannels();
    for (const auto ch : *tChannels)
    {
        LWGpuChannel * chan = ch->GetCh();
        UINT32 cHandle = chan->ChannelHandle();
        m_BuffInfo.specifyChannelHandle(cHandle, m_pLwRm);
    }

    lwgpu->GetBuffInfo(&m_BuffInfo);

    for (auto ch : m_TraceCpuManagedChannels)
    {
        ch->GetCh()->GetBuffInfo(&m_BuffInfo, ch->GetName());
    }

    // Disable all color surfaces initially
    if (!GetAllocSurfaceCommandPresent())
    {
        UINT32 i;
        for (i = 0; i < MAX_RENDER_TARGETS; i++)
        {
            gsm->SetSurfaceUsed(ColorSurfaceTypes[i], false, 0);
        }
        if (pTraceChannel && pTraceChannel->GetGrSubChannel() &&
            params->ParamPresent("-nullz") == 0)
        {
            gsm->SetSurfaceUsed(SURFACE_TYPE_Z, true, 0);
        }
        else
        {
            gsm->SetSurfaceUsed(SURFACE_TYPE_Z, false, 0);
        }
    }

    // The following can be overridden in the trace header, so set first
    gsm->SetTargetDisplay(m_Trace.GetWidth(), m_Trace.GetHeight(), 1, 1);

    rc = gsm->InitSurfaceParams(params, 0);
    if (rc != OK)
    {
        ErrPrintf("Error initializing surface parameters: %s\n", rc.Message());
        return FailSetup();
    }

    // Now read back the target display width and height in case the command line processing has over ridden them
    traceDisplayWidth = gsm->GetTargetDisplayWidth();
    traceDisplayHeight = gsm->GetTargetDisplayHeight();

    // allocation - start with everything invalid so on failure we can just
    //  nuke everything...

    // Need to create a notifier for either -conlwrrent or for some WFI modes

    for (auto ch : m_TraceCpuManagedChannels)
    {
        if (OK != (rc = ch->AllocNotifiers(params)))
        {
            return FailSetup();
        }
    }

    UINT32 width = gsm->GetWidth();
    UINT32 height = gsm->GetHeight();
    string scissorStr;

    // scissor setup
    if (params->ParamPresent("-scissor") > 0)
    {
        scissorStr = params->ParamStr("-scissor");
    }
    if (params->ParamPresent("-scissor_correct") > 0)
    {
        // Option -scissor provides wrong operation for fermi y-ilwersion.
        // However we cannot change the behavior now as it is too late to
        // update fermi traces/results. So new option -scissor_correct is
        // added to provide the right operations (bug 666003).
        scissorStr = params->ParamStr("-scissor_correct");
    }

    if (!scissorStr.empty())
    {
        sscanf(scissorStr.c_str(), "%dx%d+%d+%d", &scissorWidth, &scissorHeight, &scissorXmin, &scissorYmin);
        // validate scissor params before enable
        if ((scissorWidth != 0) && (scissorHeight != 0))
        {
            scissorEnable = true;

            if (params->ParamPresent("-scissor_correct") > 0)
            {
                // Adjustment for y-ilwersion
                if ((scissorYmin + scissorHeight) > height)
                {
                    // Scissor size bigger than the surface size, just force to 0
                    scissorYmin = 0;
                }
                else
                {
                    scissorYmin = height - scissorYmin - scissorHeight;
                }
            }
        }
    }

  // This load relies on color_format enums to match the SetSurfaceFormat method
  // in all classes!  Probably should do this a better way - SRW
  /* Load doesn't just read the files, it does the following:
     Read header file (test.hdr)
     Read push buffer file (test.dma)
     Read any texture files (test_?.lwt)
     Read any palette texture files
     Read any vertex buffer files (test_?.vtx)

     If it is a push buffer, we massage the data by parsing the methods
     in the file and hacking each one individually if needed.
     */

    // Massage each pushbuffer-methods module of each channel.
    for (auto ch : m_TraceCpuManagedChannels)
    {
        TraceModules::iterator iMod;
        for (iMod = ch->ModBegin(); iMod != ch->ModEnd(); ++iMod)
        {
            if (OK != (*iMod)->MassagePushBuffer(0, &m_Trace, params))
            {
                return FailSetup();
            }
        }
    }

    rc = ErrorLogger::StartingTest();
    if (rc != OK)
    {
        ErrPrintf("Error initializing ErrorLogger: %s\n", rc.Message());
        return FailSetup();
    }

    // figures out if we are using PerfMon
    bool needSetupPM = false;

    if (!IsTestCtxSwitching())
    {
        needSetupPM = true;
        // In serial mode. perf related members in s_LwrrentTestsSetting are not used.
    }
    else
    {
        needSetupPM = s_LwrrentTestsSetting.hasTuT ? m_IsTuT : IsCoordinatorSyncCtx();
    }

    // Setup PM before surface allocation.
    if (needSetupPM && !SetupPM(params))
    {
        return FailSetup();
    }

    // XXX This used to be before LoadFiles.  Could restore old alloc order if we
    // were to split LoadFiles into multiple steps.  This needs to be later so that
    // we can use SetCt/ZtSelect methods to decide which color and Z buffers to
    // allocate.
    if (params->ParamPresent("-stream_to_color"))
    {
        for (UINT32 i = 0; i < m_streams; ++i)
        {
            if (i >= MAX_RENDER_TARGETS)
            {
                ErrPrintf("Not enough render targets for all SEB buffers\n");
                return FailSetup();
            }

            //streaming buffer will get "rendered" on surface m_stream_buffer
            //we need the rest of them for multiple SEB buffers
            gsm->SetSurfaceNull(ColorSurfaceTypes[m_stream_buffer+i], false, 0);
            gsm->SetSurfaceUsed(ColorSurfaceTypes[m_stream_buffer+i], true, 0);
            gsm->GetSurface(ColorSurfaceTypes[m_stream_buffer+i], 0)->SetLayout(Surface2D::Pitch);
        }
    }

    if (params->ParamPresent("-prog_zt_as_ct0"))
    {
        // Don't allocate the first color buffer when this option is used
        gsm->SetSurfaceUsed(SURFACE_TYPE_COLORA, false, 0);
    }

    if (params->ParamPresent("-zt_count_0") > 0)
    {
        // Don't allocate z buffer
        gsm->SetSurfaceUsed(SURFACE_TYPE_Z, false, 0);
    }

    // The surface manager allocates a ClipID buffer by default,
    // so tell it not to do that if no graphics channel exists or
    // the user has specified there shouldn't be a ClipID buffer.
    if (!GetGrChannel() || (params->ParamPresent("-no_clip_id") > 0))
    {
        gsm->SetNeedClipID(false);
    }

    // Setup all CheetAh shared surfaces in surface manager.
    for (MdiagSurf* surf = gsm->GetSurfaceHead(0); surf;
        surf = gsm->GetSurfaceNext(0))
    {
        set<string>::iterator it = m_TegraSharedSurfNames.find(surf->GetName());
        if (it != m_TegraSharedSurfNames.end())
        {
            if (surf->IsAllocated())
            {
                ErrPrintf("trace_3d %s: Surface %s already allocated. Can't declare as CheetAh shared.\n",
                          __FUNCTION__, surf->GetName().c_str());
                return FailSetup();
            }
            surf->SetVASpace(Surface2D::GPUVASpace);
            InfoPrintf("trace_3d %s: Surface %s declared as CheetAh shared.\n",
                       __FUNCTION__, surf->GetName().c_str());
            m_TegraSharedSurfNames.erase(it);
        }
    }

    rc = gsm->AllocateSurfaces(params, &m_BuffInfo, 0);
    if (rc != OK)
    {
        ErrPrintf("trace_3d: AllocateSurfaces failed: %s\n", rc.Message());
        return FailSetup();
    }

    rc = MapPeerSurfaces();
    if (rc != OK)
    {
        ErrPrintf("trace_3d: MapPeerSurfaces failed: %s\n", rc.Message());
        return FailSetup();
    }

    rc = SetupTegraSharedModules();
    if (rc != OK)
    {
        ErrPrintf("trace_3d: SetupTegraSharedModules failed: %s\n", rc.Message());
        return FailSetup();
    }

    rc = InitAllocator();
    if (rc != OK)
    {
        ErrPrintf("trace_3d: InitAllocator failed: %s\n", rc.Message());
        return FailSetup();
    }

    rc = m_Trace.AllocateSurfaces(width, height);
    if (rc != OK)
    {
        ErrPrintf("trace_3d: AllocateSurfaces failed: %s\n", rc.Message());
        return FailSetup();
    }
    if (GetBoundGpuDevice()->GetNumSubdevices() > 1)
    {
        // In SLI system, print p2p mapping info
        InfoPrintf("%s: Printing SliSurfMapper's state:\n", __FUNCTION__);
        GetSSM()->Print();
    }

    rc = gsm->ProcessSurfaceViews(params, &m_BuffInfo, 0);
    if (rc != OK)
    {
        ErrPrintf("trace_3d: ProcessSurfaceViews failed: %s\n", rc.Message());
        return FailSetup();
    }

    rc = m_Trace.CreateBufferDumper();
    if (rc != OK)
    {
        ErrPrintf("trace_3d: CreateBufferDumper failed: %s\n", rc.Message());
        return FailSetup();
    }

    //////////////////////////// Begin Trace Event ////////////////////////
    //
    // Trace3dPlugin event: "TraceSurfacesAllocated".
    //
    // TRACE_3D FLOW: At this point, trace_3d has allocated space for all the
    // trace surfaces, but hasn't downloaded the initial contents yet. However,
    // any pushbuffers which are specified to have surfaces allocated HAVE NOT
    // yet had their surfaces allocated.  Pushbuffer surface allocation is done
    // during GpuTrace::DoRelocations()
    //
    // MEANING: A plugin that handles this event may inspect the surface
    // parameters like GPU virtual address.
    //
    // NOTE: if the call of m_Trace.AllocateSurfaces() is moved, or if the work
    // it does is changed, then the location of this event may also need to
    // change to preserve its meaning.
    //
    rc = traceEvent(TraceEventType::TraceSurfacesAllocated);
    if ( rc != OK )
    {
        ErrPrintf("trace_3d: Error in processing trace event TraceSurfacesAllocated:\n" );
        return FailSetup();
    }
    //
    ///////////////////////////  End Trace Event /////////////////////////

    return 1;
}

//------------------------------------------------------------------------------
//! \brief Map any surfaces that used by a different instance of a Trace3DTest.
//!
//! \return OK if successful, not OK if a failure oclwrred
RC Trace3DTest::MapPeerSurfaces()
{
    RC rc;

    for (auto & surface : m_PeerMappedSurfaces)
    {
        SurfaceType st = m_Trace.FindSurface(surface.first.c_str());
        MdiagSurf * pSurf = NULL;

        if (st == SURFACE_TYPE_CLIPID)
        {
            pSurf = GetSurfaceMgr()->GetClipIDSurface();
        }
        else if (st != SURFACE_TYPE_UNKNOWN)
        {
            pSurf = GetSurfaceMgr()->GetSurface(st, 0);
        }

        // Skip the surface if it is not a clip surface or color/z surface
        if (pSurf != NULL)
        {
            // For each GpuDevice that is requesting to use the surface
            // as a peer surface, validate that the surface can be peer'd
            // and map the surface as appropriate
            for (auto device : surface.second)
            {
                if (pSurf->GetLocation() == Memory::Fb)
                {
                    if (device == GetBoundGpuDevice())
                    {
                        // Does this case ever happen?  Two peered tests
                        // running on the same device?
                        // TBD: what to do about peerID?
                        CHECK_RC(pSurf->MapLoopback());
                    }
                    else
                    {
                        CHECK_RC(pSurf->MapPeer(device));
                    }
                }
                else if (device != GetBoundGpuDevice())
                {
                    CHECK_RC(pSurf->MapShared(device));
                }
            }
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Complete setup - setup any classes, perform relocations and download
//!        all cached surfaces to the actual surface.
//!
//! \return 1 if successful, 0 if a failure oclwrred
int Trace3DTest::FinishSetup()
{
    SimClk::EventWrapper event(SimClk::Event::T3D_FINISH_SETUP);

    RC rc;

    rc = DisplaySurface();
    if (rc != OK)
    {
        ErrPrintf("trace_3d: DisplaySurface failed: %s\n", rc.Message());
        return FailSetup();
    }

    rc = SetupChecker();
    if (OK != rc)
    {
        ErrPrintf("Could not create the GPU verification object\n");
        return FailSetup();
    }

    // ILW: Here all the surfaces should have been allocated.
    // MapSliSurfaces must be called before the relocations.
    rc = MapSliSurfaces();
    if (rc != OK)
    {
        ErrPrintf("trace_3d: MapSliSurfaces failed: %s\n", rc.Message());
        return FailSetup();
    }

    // Now that surfaces are allocated, we can perform relocations.
    rc = m_Trace.DoRelocations(GetBoundGpuDevice());
    if (rc != OK)
    {
        ErrPrintf("trace_3d: DoRelocations failed: %s\n", rc.Message());
        return FailSetup();
    }

    rc = m_Trace.UpdateTrapHandler();
    if (rc != OK)
    {
        ErrPrintf("trace_3d: UpdateTrapHandler failed: %s\n", rc.Message());
        return FailSetup();
    }

    // At this point most surfaces (including pushbuffer surfaces if any
    // were specified) have been allocated.  Print their information
    string buffInfoTitle = Utility::StrPrintf("Test: %s", GetTestName());
    m_BuffInfo.Print(buffInfoTitle.c_str(),GetBoundGpuDevice());

    // Apply SLI scissor to pushbuffer by massaging.
    // Note: configuration is read by SetupChecker which calls SetupScissor.
    ApplySLIScissor();

    if (!StartPolicyManagerAfterSetup())
    {
        if (OK != PolicyManagerStartTest())
        {
            return FailSetup();
        }
    }

    // Peer tests should not begin channel switching here since FinishSetup()
    // is called during the single threaded setup stage, it is not possible to
    // channel switch the setup methods for peer tests anyway.  Beginning
    // channel switching for peer tests is delayed until RealSetup() which is
    // called from BeginIteration().
    //
    // In addition, since all conlwrrent tests in a peer test run through all
    // setup stages, if channel switching is started here, then any subsequent
    // tests (in a group of peer tests) will not sucessfully begin channel
    // switching since the first test will hold the channel switching mutex
    if (!IsPeer() && !Utl::ControlsTests())
    {
        rc = BeginChannelSwitch();
        if (OK != rc)
        {
            // return success if test is aborted
            return rc == RC::USER_ABORTED_SCRIPT? 1 : FailSetup();
        }
    }

    // Send init methods for each channel.
    rc = SetupClasses();
    if (rc != OK)
    {
        ErrPrintf("trace_3d: failed to send init methods: %s\n", rc.Message());
        return FailSetup();
    }

    LWGpuResource * lwgpu = GetGpuResource();
    if (GetBoundGpuDevice()->GetFamily() >= GpuDevice::Hopper &&
        lwgpu->HasGlobalZbcArguments())
    {
        lwgpu->GetZbcTable(m_pLwRm);
    }

    // Turn on ELPG based on elpg_mask.
    rc = SwitchElpg(true);
    if (rc != OK)
    {
        ErrPrintf("trace_3d: failed to turn on ELPG: %s\n", rc.Message());
        return FailSetup();
    }

    // There are buffers allocated in SetupClasses that have not been
    // printed yet.  We could move the original print to this spot,
    // we then we wouldn't get the table if there was an error in SetupClasses.
    // Channel switching would also be a problem.  Instead, we'll print
    // the buffer table again.  The SetPrintBufferOnce function should've
    // already been called so that a given buffer will never be printed
    // more than once.
    buffInfoTitle = "after SetupClasses" + buffInfoTitle;
    m_BuffInfo.Print(buffInfoTitle.c_str(),GetBoundGpuDevice());

    const UINT32 defaultSubdevId = 0;

    gsm->GetZLwll()->PrintZLwllInfo(m_Trace.GetZLwllIds(), defaultSubdevId,
        GetGpuResource(), GetLwRmPtr(), GetSmcEngine());
    gsm->GetZLwll()->SetParams(params);

    if (GetGrChannel() != 0)
    {
        // Clear color/Z surfaces.
        RGBAFloat ColorClear = m_Trace.GetColorClear();

        GpuVerif * gpuVerif = GetGrChannel()->GetGpuVerif();
        rc = gsm->Clear(params, gpuVerif, ColorClear, m_Trace.GetZetaClear(), m_Trace.GetStencilClear());

        if (rc != OK)
        {
            ErrPrintf("trace_3d: clear failed: %s\n", rc.Message());
            return FailSetup();
        }
    }

    // Load other surface images with data previously read from disk.
    GpuDevice *pGpuDev = GetBoundGpuDevice();
    rc = m_Trace.LoadSurfaces();
    if (rc != OK)
    {
        ErrPrintf("trace_3d: LoadSurfaces failed: %s\n", rc.Message());
        return FailSetup();
    }

    // After LoadSurface, if there is no need to read any files later,
    // shut down the file manager.
    if (!m_Trace.HasFileUpdates() &&
        !m_Trace.HasDynamicModules() &&
        !m_Trace.HasRefCheckModules())
    {
        m_TraceFileMgr.ShutDown();
    }

    if (params->ParamPresent("-elf_name") > 0)
    {
        const char* elfName = params->ParamStr("-elf_name");

        std::string secElfName;
        if (params->ParamPresent("-sec_elf_name") > 0)
            secElfName.assign(params->ParamStr("-sec_elf_name"));

        // Get the optional backdoor memory library path
        const char* backdoorPath = 0;
        if (params->ParamPresent("-backdoorlib_path"))
            backdoorPath = params->ParamStr("-backdoorlib_path");

        const char* cosim_args = "";
        if (params->ParamPresent("-cosim_args") > 0) {
            cosim_args = params->ParamStr("-cosim_args");
        }

        rc = RunCosim(elfName, secElfName.empty() ? 0 : secElfName.c_str(),
                      backdoorPath, cosim_args);
        if ( rc != OK )
        {
            ErrPrintf("trace_3d: RunCosim failed\n");
            return FailSetup();
        }
    }

    //////////////////////////// Begin Trace Event ////////////////////////
    //
    // Trace3dPlugin event: "TraceSurfacesDownloaded".
    //
    // TRACE_3D FLOW: At this point, trace_3d has downloaded the initial
    // contents for all the trace surfaces
    //
    // MEANING: A plugin that handles this event may inspect / change the
    // initial contents of the trace surfaces
    //
    // NOTE: if the call of m_Trace.LoadSurfaces() is moved, or if the work it
    // does is changed, then the location of this event may also need to change
    // to preserve its meaning.
    //
    rc = traceEvent(TraceEventType::TraceSurfacesDownloaded);
    if ( rc != OK )
    {
        ErrPrintf("trace_3d: Error in processing trace event TraceSurfacesDownloaded:\n" );
        return FailSetup();
    }
    //
    ///////////////////////////  End Trace Event /////////////////////////

    if (params->ParamPresent("-pm_ilwalidate_cache") > 0)
    {
        for (UINT32 subdev = 0; subdev < pGpuDev->GetNumSubdevices(); ++subdev)
        {
            InfoPrintf("trace_3d: Ilwalidate L2 with L2_ILWALIDATE_CLEAN after TraceSurfacesDownloaded\n");

            rc.Clear();
            rc = pGpuDev->GetSubdevice(subdev)->IlwalidateL2Cache(GpuSubdevice::L2_ILWALIDATE_CLEAN);

            if (rc != OK)
            {
                ErrPrintf("trace_3d: Error (%s) in ilwalidating L2 in subdev(%u)\n", rc.Message(), subdev);
                return FailSetup();
            }
        }
    }

    return 1;
}

//!-----------------------------------------------------------------------------
//! \brief Gets the name of the chip in lowercase letters.
//!
//! \return the name of the chip under test, e.g. t124
std::string Trace3DTest::GetChipName() const
{
    // Get gpu resource according to class type specified in trace
    if (GetGpuResource() == 0)
    {
        ErrPrintf("trace_3d: GetChipName must be called after acquiring the GPU.\n");
        MASSERT(0);
    }

    // Get the chip name
    std::string chip = Gpu::DeviceIdToInternalString(GetGpuResource()->GetGpuSubdevice()->DeviceId());
    if (chip.length() == 0)
        MASSERT(0);

    // We get capital letters, so we want to drop to lowercase.
    for (size_t s = 0; s < chip.size(); s++)
    {
        chip[s] = tolower(chip[s]);
    }

    return chip;
}

//------------------------------------------------------------------------------
//! \brief Run the python version of the run_cosim script.
//!
//! \param elfName The kernel elf for cosim to load
//! \param secondaryElf A secondary elf for cosim to load, e.g. page tables file
//! \param backdoorlibPath Path to the chip tree's libtegra_mem_<chip>.so (optional)
//! \param cosim_args Extra args to pass to cosim.
//!
//! \return OK if successful, not OK if a failure oclwrred
//!
//! \note if we ever want to run the MODS+ASIM flow on Windows, we would need to write a cross-platform method to get
//!       the directory delimiter. ('/' versus '\')
RC Trace3DTest::RunCosim(const char* elfName, const char* secondaryElf,
                         const char* backdoorLibPath,
                         const char* cosim_args) const
{
    // Locate the complete simmgr installation path.
    std::string modsExe = Utility::DefaultFindFile("mods", true);
    if (modsExe.empty())
    {
        ErrPrintf("trace_3d: Unable to find the mods exelwtable!\n");
        MASSERT(0);
    }
    std::string modsDir = Utility::StripFilename(modsExe.c_str());
    std::string parentDir = Utility::StripFilename(modsDir.c_str());
    std::string simmgrDir = parentDir + "/simmgr";

    // Get the chip name
    std::string chip = GetChipName();

    std::string cosimCommand = simmgrDir;
    cosimCommand += "/scripts/gpu_cosim.py --nocheck --chip ";
    cosimCommand += chip;
    cosimCommand += " --timeout=0 --sim_dir ";
    cosimCommand += simmgrDir;
    cosimCommand += " --log_dir=. --mem 256 --mach t132multiengine ";
    if (cosim_args) {
        cosimCommand += cosim_args;
        cosimCommand += " ";
    }

    // Cosim args
    if (backdoorLibPath || secondaryElf)
    {
        cosimCommand += "--cosim_arg='";
        if (backdoorLibPath)
        {
            cosimCommand += "-backdoor_lib ";
            cosimCommand += backdoorLibPath;
        }

        if (secondaryElf)
        {
            cosimCommand += " --cosim_sec_melf ";
            cosimCommand += secondaryElf;
        }
        cosimCommand += "' ";
    }

    if (params->ParamPresent("-debug_elf") > 0)
    {
        cosimCommand += "--kgdb_symbols ";
        cosimCommand += elfName;
        cosimCommand += "--kgdb_window ";
    }

    cosimCommand += elfName;
    cosimCommand += " &";
    InfoPrintf("trace_3d: exelwting cosimCommand: %s\n", cosimCommand.c_str());
    int rc = system(cosimCommand.c_str());
    return (rc != -1 ? OK : RC::SOFTWARE_ERROR);
}

struct PollAllChannelsIdleArgs
{
    GpuDevice*     pGpuDevice;
    LwRm*          pLwRm;
};

static bool PollAllChannelsIdle(void *args)
{
    PollAllChannelsIdleArgs *pArgs = (PollAllChannelsIdleArgs*) args;
    GpuDevice *pGpuDev = pArgs->pGpuDevice;
    LwRm *pLwRm = pArgs->pLwRm;
    for (UINT32 i = 0; i < pGpuDev->GetNumSubdevices(); ++i)
    {
        if (!pGpuDev->GetSubdevice(i)->PollAllChannelsIdle(pLwRm))
        {
            return false;
        }
    }

    return true;
}

//------------------------------------------------------------------------------
//! The return value 1 means success in RealSetup().
//! The return value 0 means that there is a failure in RealSetup() and test has been
//! cleaned up through FailSetup().
//!
int Trace3DTest::RealSetup()
{
    CHIPLIB_CALLSCOPE_WITH_TESTNAME;

    if (m_SkipDevInit)
    {
        return 1;
    }

    RC rc;

    LWGpuChannel* pChDefault = 0;
    LWGpuSubChannel* pSubchDefault = 0;
    TraceChannel* pTraceChannel = GetDefaultChannel();
    if (pTraceChannel)
    {
        pChDefault = pTraceChannel->GetCh();
        TraceSubChannel* pTraceSubChannel = pTraceChannel->GetSubChannel("");
        pSubchDefault = pTraceSubChannel? pTraceSubChannel->GetSubCh() : 0;
    }

    LWGpuResource * lwgpu = GetGpuResource();

#ifndef _WIN32
    if (params->ParamPresent("-compbit_test_type") &&
        m_CompBitsTest.get() == 0)
    {
        if (OK != CreateCompbitTest())
        {
            return FailSetup();
        }
    }
#endif

    if ((!IsPeer() && IsTestCtxSwitching()) || Utl::ControlsTests())
    {
        if (!FinishSetup() && !m_bAborted)
        {
            //If hit some errors, FailSetup() is called by FinishSetup().
            ErrPrintf("Failed to complete setup.\n");
            return 0;
        }
        else if (m_bAborted)
        {
            return 1;
        }
    }

    // if the user asked for a crc report only, stop here and don't execute test or anymore setup
    if (m_CrcDumpMode == CrcEnums::CRC_REPORT)
    {
        return(1);
    }

    // Peer tests begin channel switching here.  (On the first iteration of
    // non-peer tests, channel switching began in Setup().  BeginChannelSwitch
    // is a no-op if it's run twice, so it safe to re-run it in that case.)
    rc = BeginChannelSwitch();
    if (OK != rc)
    {
        // return success if test is aborted
        return rc == RC::USER_ABORTED_SCRIPT? 1 : FailSetup();
    }

    //call to stub function for extra setup steps (used by classes inheriting from trace_3d)
    if (!BeforeExelwteMethods())
    {
        return FailSetup();
    }

    //call to stub function for sending extra primitives before trace_3d
    //(used by classes inheriting from trace_3d)
    if (!RunBeforeExelwteMethods())
    {
        return FailSetup();
    }

    if (conlwrrent && pChDefault)
    {
        start_offset = pChDefault->GetLwrrentDput();
    }

    if (lwgpu->PerfMon() && pChDefault && pSubchDefault &&
        (pmMode != PM_MODE_NONE) && (pmMode != PM_MODE_PM_SB_FILE))
    {
        // Add XML output to match -pmsb
        if ( gXML ) {
            XD->XMLStartElement("<PmTrigger");
            XD->outputAttribute("index",  0);
            XD->endAttributes();
        }

        lwgpu->PerfMonStart(pChDefault, pSubchDefault->subchannel_number(),
            pSubchDefault->get_classid());
    }

    if (StartPolicyManagerAfterSetup())
    {
        if (OK !=PolicyManagerStartTest())
        {
            return FailSetup();
        }
    }

    // If there is a CPU test that will be run along with this trace3d test,
    // let the CPU model know the CPU test name.
    if (params->ParamPresent("-cpu_model_test") > 0)
    {
        if (!CPUModel::Enabled())
        {
            ErrPrintf("trace_3d: CPU test specified but chiplib does not support CPU Model\n" );
            return FailSetup();
        }

        PollAllChannelsIdleArgs args = {GetBoundGpuDevice(), m_pLwRm};

        if (OK != POLLWRAP_HW(&PollAllChannelsIdle, &args,
                LwU64_LO32(GetTimeoutMs())))
        {
            return FailSetup();
        }

        string escapeString;

        escapeString = Utility::StrPrintf("CPU_MODEL|CM_SETUP|Test name:=%s",
            params->ParamStr("-cpu_model_test"));

        if (params->ParamPresent("-cpu_model_test_lib"))
        {
            escapeString += Utility::StrPrintf("|Test lib:=%s",
                params->ParamStr("-cpu_model_test_lib"));
        }

        Platform::EscapeWrite(escapeString.c_str(), 0, 4, 0);
    }

    PrintChannelAllocInfo();

    //////////////////////////// Begin Trace Event ////////////////////////
    //
    // Trace3dPlugin event: "BeforeRunTraceOps".
    //
    // TRACE_3D FLOW: At this point, trace_3d has finished initialization and
    // is about to begin running the "trace ops", or the individual commands
    // of the trace
    //
    // MEANING: A plugin that handles this event may inspect / change the
    // list of trace ops before any of them are exelwted, or may give
    // the GPU some work to do prior to exelwting any of the trace ops
    //
    // NOTE: if the call to m_Trace.RunTraceOps() is moved, or if the work it
    // does is changed, then the location of this event may also need to change
    // to preserve its meaning.
    //
    rc = traceEvent(TraceEventType::BeforeRunTraceOps);
    if ( rc != OK )
    {
        ErrPrintf("trace_3d: Error in processing trace event BeforeRunTraceOps:\n" );
        return FailSetup();
    }
    //
    ///////////////////////////  End Trace Event /////////////////////////

    if (params->ParamPresent("-ctxsw_scan_method") > 0)
    {
        // resume ctxsw before sending method segment
        for (auto ch : m_TraceCpuManagedChannels)
        {
            LWGpuChannel* pGpuChannel = ch->GetCh();
            pGpuChannel->SetSuspendCtxSw(false);
        }
    }

    LWGpuContexScheduler::Pointer lwgpuSked = lwgpu->GetContextScheduler();
    bool syncOnTraceMethods = lwgpu->IsSyncTracesMethodsEnabled() &&
                              (lwgpuSked != NULL);
    if (syncOnTraceMethods)
    {
        if (OK != lwgpuSked->SyncAllRegisteredCtx(SYNCPOINT("Before sending trace methods")))
        {
            return FailSetup();
        }
    }

    // Send all methods to all channels.
    {
        ChiplibOpScope newScope(string(GetTestName()) + " " + "RunTraceOps", NON_IRQ,
                                ChiplibOpScope::SCOPE_COMMON, NULL);
        SimClk::EventWrapper event(SimClk::Event::T3D_TRACEOP);
        if ((rc = m_Trace.RunTraceOps()) != OK)
        {
            ErrPrintf("Failed to play back trace methods: %s\n", rc.Message());
            return FailSetup();
        }
    }

    // Inject Nop method into each channel to trigger the write-event after the last trace method
    //
    // Method write trigger in policy manager is pre-write trigger.
    // Before method is written into pb, it will be triggered by the MethodWriteEvent.
    // If the method count could match, it will trigger the hooked actions. So the trigger event
    // happens before the method has been aclwrately written, which is called pre-write.
    // To support 100 perecent in triggers like Trigger.OnPercentmethodsWritten(), since it is pre-write
    // trigger, so need another method to trigger the write-event after the last method.
    // For this purpose, we inject an extra NOP method to help trigger the write-event after last trace
    // method.
    //
    // Since it's only need by Policymanager, limit the extra injection only when Policymanager is active.

    if(PolicyManager::Instance()->IsInitialized())
    {
        for (auto ch : m_TraceCpuManagedChannels)
        {
            LWGpuChannel* pGpuChannel = ch->GetCh();
            if (pGpuChannel)
            {
                rc = pGpuChannel->GetModsChannel()->WriteNop();
                if (OK != rc)
                {
                    ErrPrintf("Fail to insert Nop method into each channel: %s.\n", rc.Message());
                    return FailSetup();
                }
            }
        }
    }

    if ((params->ParamPresent("-decompress_in_place") > 0) ||
        (params->ParamPresent("-decompress_in_placeZ") > 0))
    {
        for (auto ch : m_TraceCpuManagedChannels)
        {
            rc = ch->SendPostTraceMethods();
            if (OK != rc)
            {
                ErrPrintf("Failed to play back post trace methods: %s\n", rc.Message());
                return FailSetup();
            }
        }
    }

    //////////////////////////// Begin Trace Event ////////////////////////
    //
    // Trace3dPlugin event: "AfterRunTraceOps".
    //
    // TRACE_3D FLOW: At this point, trace_3d has exelwted all the individual
    // "trace ops", or the individual commands, of the trace.  Methods may
    // not yet be "flushed" (the "put" pointer written).
    //
    // MEANING: A plugin that handles this event may examine the gpu,
    // force a method flush or WFI, inspect the state of the gpu, or submit
    // more work for the gpu to do before trace_3d enters the results
    // verification phase
    //
    // NOTE: if the call to m_Trace.RunTraceOps() is moved, or if the work it
    // does is changed, then the location of this event may also need to change
    // to preserve its meaning.
    //
    rc = traceEvent(TraceEventType::AfterRunTraceOps);
    if ( rc != OK )
    {
        ErrPrintf("trace_3d: Error in processing trace event AfterRunTraceOps:\n" );
        return FailSetup();
    }
    //
    ///////////////////////////  End Trace Event /////////////////////////

    if (_Trigger_LA)
    {
        if (_Trigger_Addr <= 0xffff)
        {
            UINT32 data = 0;
            InfoPrintf("Attention! trace_3d is triggering the logic analiser at IO READ(0x%x)\n",_Trigger_Addr);
            Platform::PioRead32(_Trigger_Addr, &data);
        }
        else
        {
            InfoPrintf("Attention! trace_3d is triggering the logic analiser at MEM READ(0x%x)\n",_Trigger_Addr);
            MEM_RD32(_Trigger_Addr);
        }
    }

    //call to stub function for sending extra primitives after trace_3d
    //(used by classes inheriting from trace_3d)
    if (!RunAfterExelwteMethods())
    {
        ErrPrintf("RunAfterExelwteMethods() failed.\n");
        return FailSetup();
    }

    //call to stub function for extra steps to end the tests (used by classes inheriting from trace_3d)
    if (!AfterExelwteMethods())
    {
        ErrPrintf("AfterExelwteMethods() failed.\n");
        return FailSetup();
    }

    // Sync trace thread
    // Note: Atomic methods may be queued in atomic wrapper until RunAfterExelwteMethods()
    if (syncOnTraceMethods)
    {
        if (OK != lwgpuSked->SyncAllRegisteredCtx(SYNCPOINT("After sending trace methods")))
        {
            return FailSetup();
        }
        if (OK != FlushMethodsAllChannels())
        {
            return FailSetup();
        }
        if (OK != WaitForDMAPushAllChannels())
        {
            return FailSetup();
        }
        if (OK != lwgpuSked->SyncAllRegisteredCtx(SYNCPOINT("After flush methods")))
        {
            return FailSetup();
        }
    }

    // Remove all cached surfaces
    m_Trace.ReleaseAllCachedSurfs();

    // Start the timer if we're using it
    if (params->ParamPresent("-timer"))
    {
        meter->Reset ();
        meter->Start ();
    }

    if (OK != SendNotifiers())
    {
        return FailSetup();
    }

    if (conlwrrent && pChDefault)
    {
        end_offset = pChDefault->GetLwrrentDput();
        InfoPrintf("end_offset = %x\n",end_offset);
        MASSERT(!"LWGpuChannel::StartMethodLoop is not supported");
    }

    // if context switching, wait for idle
    if (IsTestCtxSwitching() &&
        (params->ParamPresent("-ctxswTimeSlice") == 0) &&
        !Utl::ControlsTests())
    {
        if (OK != WaitForIdle())
        {
            ErrPrintf("When context switching, chip should be idle before RealSetup() returns.\n");
            return FailSetup();
        }
    }

    if (IsTestCtxSwitching() && params->ParamPresent("-single_kick"))
    {
        // yield to let other thread do Realsetup() before final wait chip idle
        InfoPrintf("%s: Yield() to let other thread to Realsetup() in case of ctxswitch + single_kick\n",
                   __FUNCTION__);
        Yield();
    }

    //
    // All trace methods are sent;
    // -block couldn't cowork with -ctxswNum due to the mutex deadlock. Releae channel
    // mutex after sending trace methods to avoid deadlock, but this behavior change
    // leads to existed test failures(bug 1451758). Limit the channel mutex
    // release to -block case to avoid breaking existed ctxsw tests.
    if (IsBlockEnabled())
    {
        EndChannelSwitch();
    }

    return 1;
}

//------------------------------------------------------------------------------
RC Trace3DTest::SendNotifiers()
{
    RC rc;
    for (auto ch : m_TraceCpuManagedChannels)
    {
        ch->ClearNotifiers();
        CHECK_RC(ch->GetCh()->BeginInsertedMethodsRC());
        rc = ch->SendNotifiers();
        if (OK != rc)
        {
            ch->GetCh()->CancelInsertedMethods();
            ErrPrintf("trace_3d: SendNotifiers failed on channel %s\n",
                      ch->GetName().c_str());
            return rc;
        }
        CHECK_RC(ch->GetCh()->EndInsertedMethodsRC());
    }
    return rc;
}

//------------------------------------------------------------------------------
RC Trace3DTest::WaitForIdle()
{
    RC rc;

    const string scopeName = Utility::StrPrintf("%s Trace3DTest::WaitForIdle %s",
                                                GetTestName(),
                                                GetDefaultChannel() ? WfiMethodToString(GetDefaultChannel()->GetWfiMethod()) : "");
    ChiplibOpScope newScope(scopeName, NON_IRQ, ChiplibOpScope::SCOPE_COMMON, NULL);

    //////////////////////////// Begin Trace Event ////////////////////////
    //
    rc = traceEvent(TraceEventType::BeforeFlushMethodsInWaitForIdle);
    if ( rc != OK )
    {
        ErrPrintf("trace_3d: Error in processing trace event BeforeFlushMethodsInWaitForIdle\n" );
        return rc;
    }
    //
    ///////////////////////////  End Trace Event /////////////////////////

    CHECK_RC( FlushMethodsAllChannels() );

    //////////////////////////// Begin Trace Event ////////////////////////
    //
    rc = traceEvent(TraceEventType::BeforeWaitForIdle);
    if ( rc != OK )
    {
        ErrPrintf("trace_3d: Error in processing trace event BeforeWaitForIdle\n" );
        return rc;
    }
    //
    ///////////////////////////  End Trace Event /////////////////////////

    TraceChannel* pTraceChannel = GetDefaultChannel();
    if (pTraceChannel)
    {
        if (pTraceChannel->GetWfiMethod() == WFI_POLL)
        {
            // For Pascal+ chip, the graphics unit will not report idle for a very long time, even though it really is idle
            // we have to explicitly send a WFI method
            if (EngineClasses::IsGpuFamilyClassOrLater(
                TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_PASCAL_A))
            {
                CHECK_RC( WriteWfiMethodToAllChannels() );
            }

            // Wait for get==put on all channels, so we spend less time in the RM.
            //
            CHECK_RC( WaitForDMAPushAllChannels() );

            // The RM's IdleChannels function fiddles with the context-switching
            // controls.  If this trace uses 2+ channels and they are synchronizing
            // with semaphores, we can deadlock if we try to idle one channel at a time.
            // So, in WFI_POLL mode, we will make one IdleChannels call for all channels
            // at once so the RM is aware of the issue.
            DebugPrintf(MSGID(WaitX), "Trace3DTest -wfi_poll mode, calling RmIdleChannels...\n");

            CHECK_RC( WaitForIdleRmIdleAllChannels() );
        }
        else if (pTraceChannel->GetWfiMethod() == WFI_SLEEP)
        {
            Tasker::Sleep(10000);
        }
        else
        {
            // For WFI_INTR and WFI_NOTIFY AND WFI_HOST, poll for the interrupt or notifier
            CHECK_RC( WaitForNotifiersAllChannels() );
        }
    }

    if (params->ParamPresent("-sync_at_end_stage"))
    {

        if (!m_IsCtxSwitchTest)
        {
            ErrPrintf("-sync_at_end_stage can be used in ctxswitch mode only");
            MASSERT(0);
        }

        UINT32 argValue = params->ParamUnsigned("-sync_at_end_stage");
        // End RTAPI test first
        if (argValue == 0)
        {
            TestApiWaitOnEvent(EndStageEvent, GetTimeoutMs());
        }
        else
        {
            TestApiSetEvent(EndStageEvent);
        }
    }

    //////////////////////////// Begin Trace Event ////////////////////////
    //
    rc = traceEvent(TraceEventType::AfterWaitForIdle);
    if ( rc != OK )
    {
        ErrPrintf("trace_3d: Error in processing trace event AfterWaitForIdle\n" );
        return rc;
    }
    //
    ///////////////////////////  End Trace Event /////////////////////////

    // If the test has been skipped, it not need to wfi the shared channel which
    // is created by the policy manager.
    // Else other tests has no idea the shared channel has been used, so it will
    // free the channel resource which cause the page fault.(see bug 200209043)
    if(params->ParamPresent("-skip_test_policy_registration"))
        return OK;

    // Do not pass a handle so policy manager knows that this is a chip wide
    // WFI
    if (PolicyManager::Instance()->IsInitialized())
    {
        CHECK_RC(PolicyManager::Instance()->WaitForIdle());
    }

    return OK;
}

RC Trace3DTest::SelfGildArgCheck()
{
    RC rc;

    if (params->ParamPresent("-crc_chunk_size") &&
        params->ParamUnsigned("-crc_chunk_size") > 0)
    {
        // Partial CRC checking is designed to save memory while mods trying to
        // read back some big surfaces, this is implemented by reading chunk by
        // chunk, so we don't reserve a big flat memory to hold the surface,
        // that conflicts with current self-gilding implemenation. bug 507849
        ErrPrintf("Option -crc_chunk_size isn't compatible with self-gilding\n");
        return RC::SOFTWARE_ERROR;
    }
    else if ((params->ParamPresent("-ignore_selfgild") == 0) &&
             (params->ParamUnsigned("-dma_check", 0) == 6))
    {
        // Option -crccheck_gpu is designed to avoid cpu access to surface,
        // but self-gilding requires to use cpu to check surface data.
        // Fail the test because these two requests conflict with each other.
        ErrPrintf("Option -crccheck_gpu(\"-dma_check 6\") isn't compatible with self-gilding\n");
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC Trace3DTest::SetupCrcChecker()
{
    RC rc;

    const SelfgildStates& selfGild = m_Trace.GetSelfgildStates();
    if (selfGild.empty())
    {
         m_TestGpuVerif = new GpuVerif(
             m_pLwRm, m_pSmcEngine,
             GetGpuResource(),
             gsm, params, LW04_SOFTWARE_TEST,
             m_profile.get(), m_goldProfile.get(),
             0, m_Trace.GetTraceFileMgr(), NeedDmaCheck());
    }
    else
    {
        CHECK_RC(SelfGildArgCheck());
        m_TestGpuVerif = new GpuVerif(
            selfGild.begin(), selfGild.end(),
            m_pLwRm, m_pSmcEngine, GetGpuResource(),
            gsm, params, LW04_SOFTWARE_TEST,
            m_profile.get(), m_goldProfile.get(),
            0, m_Trace.GetTraceFileMgr(), NeedDmaCheck());
    }

    if (!m_TestGpuVerif->Setup(gsm->GetBufferConfig()))
        return RC::SOFTWARE_ERROR;

    GpuSubdevice* pSubdev = GetBoundGpuDevice()->GetSubdevice(0);
    m_TestGpuVerif->SetCrcChainManager(params, pSubdev, LW04_SOFTWARE_TEST);

    return rc;
}

RC Trace3DTest::SetupChannelCrcChecker
(
    TraceChannel* channel,
    bool graphicsChannelFound
)
{
    RC rc;

    GpuSubdevice* pSubdev = GetBoundGpuDevice()->GetSubdevice(0);

    if (m_Trace.GetSelfgildStates().empty())
    {
        CHECK_RC(channel->SetupCRCChecker(gsm, params, m_profile.get(),
            m_goldProfile.get(), pSubdev));
    }
    else
    {
        CHECK_RC(SelfGildArgCheck());
        CHECK_RC(channel->SetupSelfgildChecker(m_Trace.GetSelfgildStates().begin(),
            m_Trace.GetSelfgildStates().end(), gsm, params, m_profile.get(),
            m_goldProfile.get(), pSubdev));
    }

    AddRenderTargetsToChannelVerif (channel, graphicsChannelFound);

    return rc;
}

RC Trace3DTest::SetupChecker()
{
    RC rc;
    bool graphicsChannelFound = false;

    // Obtain scissor and SLI scissor parameters from commandline
    CHECK_RC(SetupScissor());

    MASSERT(m_profile.get() != 0);
    MASSERT(m_goldProfile.get() != 0);

    // bug 3001916
    // -crccheck_gpu usually needs DMA copy for pitch colwertion
    // but we don't support DMA copy for inflated buffers
    if ((params->ParamUnsigned("-dma_check", 0) == 6) &&
        (params->ParamPresent("-inflate_rendertarget_and_offset_window") ||
        params->ParamPresent("-inflate_rendertarget_and_offset_viewport")))
    {
        ErrPrintf("Option -crccheck_gpu isn't compatible with -inflate_rendertarget_and_offset_window/-inflate_rendertarget_and_offset_viewport\n");
        return RC::SOFTWARE_ERROR;
    }

    if (m_TraceCpuManagedChannels.empty())
    {
        CHECK_RC(SetupCrcChecker());
    }
    else
    {
        for (auto ch : m_TraceCpuManagedChannels)
        {
            // There is no need to set up Crc checking on a host only channel
            if (!ch->IsHostOnlyChannel())
            {
                CHECK_RC(SetupChannelCrcChecker(ch, graphicsChannelFound));

                if (ch->GetGrSubChannel())
                {
                    graphicsChannelFound = true;
                }
            }
        }
    }

    // Also setup Ecov checker
    const string testName = params->ParamStr("-i", "test");
    rc = SetupEcov(&m_EcovVerifier, params, m_IsCtxSwitchTest, testName, GetChipName());
    if (OK != rc)
    {
        ErrPrintf("trace_3d: Failed to set up ecover.\n");
        return rc;
    }

    if (m_EcovVerifier.get())
    {
        for (auto ch : m_TraceCpuManagedChannels)
        {
            m_EcovVerifier->AddCrcChain(ch->GetCrcChain());
            m_EcovVerifier->SetGpuResourceParams(GetGpuResource()->GetLwgpuArgs());
            ArgReader *jsArgReader = CommandLine::GetJsArgReader();
            jsArgReader->ParseArgs(CommandLine::ArgDb());
            m_EcovVerifier->SetJsParams(jsArgReader);
        }
    }

    return rc;
}

//! \brief Add color/z surfaces to channel GpuVerif object.
//!
//! \param channel A trace channel from m_TraceCpuManagedChannels
//! \param graphicsChannelFound True if a graphics channel was previously found
void Trace3DTest::AddRenderTargetsToChannelVerif(TraceChannel *channel,
    bool graphicsChannelFound)
{
    const bool stitch = !params->ParamPresent("-sli_scissor_no_stitching");
    GpuVerif* verif = channel->GetGpuVerif();

    for (int i = 0; i < MAX_RENDER_TARGETS; ++i)
    {
        MdiagSurf* surf = gsm->GetSurface(ColorSurfaceTypes[i], 0);

        if ((surf != 0) &&
            gsm->GetValid(surf) &&
            RenderTargetCheckedByChannel(channel, ColorSurfaceTypes[i],
                graphicsChannelFound))
        {
            DebugPrintf(MSGID(Main), "Adding surface type: %s\n", surf->GetName().c_str());
            if (stitch)
            {
                gsm->SetSLIScissorSpec(surf, m_SLIScissorSpec);
            }

            CrcRect rect;
            GetSurfaceCrcRect(surf, ColorSurfaceTypes[i], i, &rect);
            verif->AddSurfC(surf, i, rect);
        }
    }

    MdiagSurf* surf = gsm->GetSurface(SURFACE_TYPE_Z, 0);

    if ((surf != 0) &&
        gsm->GetValid(surf) &&
        RenderTargetCheckedByChannel(channel, SURFACE_TYPE_Z,
            graphicsChannelFound))
    {
        DebugPrintf(MSGID(Main), "Adding surface type: %s\n", surf->GetName().c_str());
        if (stitch)
        {
            gsm->SetSLIScissorSpec(surf, m_SLIScissorSpec);
        }

        CrcRect rect;
        GetSurfaceCrcRect(surf, SURFACE_TYPE_Z, -1, &rect);
        verif->AddSurfZ(surf, rect);
    }
}

//!
//! \brief Determine if the render target is CRC checked by the channel
//!
//! \param channel A trace channel from m_TraceCpuManagedChannels
//! \param surfaceType The surface type of the render target
//! \param graphicsChannelFound True if a graphics channel was previously found
bool Trace3DTest::RenderTargetCheckedByChannel(TraceChannel *channel,
    SurfaceType surfaceType, bool graphicsChannelFound)
{
    // For newer traces (ones with ALLOC_SURFACE commands present),
    // find the corresponding TraceModule and compare it's channel
    // with the present channel.
    if (GetAllocSurfaceCommandPresent())
    {
        TraceModule *module = m_Trace.FindModuleBySurfaceType(surfaceType);

        if ((module != 0) && (channel == module->GetTraceChannel()))
        {
            return true;
        }
    }

    // For older traces (ones without ALLOC_SURFACE commands present),
    // use this channel if it is a graphics channel and if no previous
    // graphics channels were found.
    else
    {
        if (channel->GetGrSubChannel() && !graphicsChannelFound)
        {
            return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------------
// Determine the CRC rect
//
void Trace3DTest::GetSurfaceCrcRect(MdiagSurf *surface,
    SurfaceType surfaceType, int index, CrcRect* rect)
{
    if (surface->GetName().length() > 0)
    {
        TraceModule *module = m_Trace.ModFind(surface->GetName().c_str());

        if (module && module->GetFileType() == GpuTrace::FT_ALLOC_SURFACE)
        {
            SurfaceTraceModule *mod = (SurfaceTraceModule*) module;
            *rect = mod->GetCrcRect();
        }
    }

    string colorOptions[8] = {
        "-crc_rectCA", "-crc_rectCB", "-crc_rectCC", "-crc_rectCD",
        "-crc_rectCE", "-crc_rectCF", "-crc_rectCG", "-crc_rectCH"
    };

    if (params->ParamPresent("-use_min_dim") > 0)
    {
        rect->x = 0;
        rect->y = 0;
        rect->width = gsm->GetWidth();
        rect->height = gsm->GetHeight();
    }
    else
    {
        string option;

        if (IsSurfaceTypeColor3D(surfaceType))
        {
            if (params->ParamPresent(colorOptions[index].c_str()))
            {
                option = colorOptions[index];
            }
            else
            {
                option = "-crc_rectC";
            }
        }
        else
        {
            option = "-crc_rectZ";
        }

        if (params->ParamPresent(option.c_str()))
        {
            rect->x      = params->ParamNUnsigned(option.c_str(), 0);
            rect->y      = params->ParamNUnsigned(option.c_str(), 1);
            rect->width  = params->ParamNUnsigned(option.c_str(), 2);
            rect->height = params->ParamNUnsigned(option.c_str(), 3);
        }
    }

    if ((rect->width > 0 || rect->height > 0) &&
        params->ParamPresent("-crccheck_gpu") > 0)
    {
        WarnPrintf("-crc_rectX/CRC_RECT/-use_min_dim doesn't work with -crccheck_gpu\n");
    }
}

//------------------------------------------------------------------------------
// Adapted from BufferManager_GetSurfaceProperties in plgnsprt.cpp
// except that we search by name here.
//
MdiagSurf* Trace3DTest::GetSurfaceByName(const char* name)
{
    if (name == NULL)
        return NULL;

    IGpuSurfaceMgr* gsm = GetSurfaceMgr();
    MdiagSurf * pSurf = gsm->GetSurfaceHead(0);

    while (pSurf)
    {
        if (gsm->GetValid(pSurf) && pSurf->GetName() == name)
        {
            return pSurf;
        }

        pSurf = gsm->GetSurfaceNext(0);
    }

    GpuTrace * pGpuTrace = GetTrace();
    ModuleIter modIt = pGpuTrace->ModBegin();
    ModuleIter modIt_end = pGpuTrace->ModEnd();
    TraceModule * pTraceModule = 0;
    for (; modIt != modIt_end; ++modIt)
    {
        pTraceModule = *modIt;
        if (pTraceModule->IsColorOrZ())
        {
            // this buffer is already managed by the surface manager,
            // so we've already counted it above
            continue;
        }

        MdiagSurf * pS2d(pTraceModule->GetDmaBufferNonConst());
        if (pS2d != NULL)
        {
            if (pS2d->GetName() == name)
            {
                return pS2d;
            }
        }
    }

    // Surface created by policy manager is shared among trace3d tests
    // but not registed to gsm or GpuTrace; search m_SharedSurfaces
    return GetGpuResource()->GetSharedSurface(name);
}

//------------------------------------------------------------------------------
/* virtual */ void Trace3DTest::CleanUp()
{
    MASSERT(!"Multistage cleanup required, use CleanUp(stage)");
}

//------------------------------------------------------------------------------
/* virtual */ void Trace3DTest::CleanUp(UINT32 stage)
{
    // This CleanUp() may not be called when "-quick_exit" specified.
    switch (stage)
    {
        case 0:
            TestSynlwp(MDiagUtils::TestCleanupStart);
            CleanUpStage0();
            return;
        case 1:
            CleanUpStage1();
            TestSynlwp(MDiagUtils::TestCleanupEnd);
            return;
        default:
            MASSERT(!"Invalid CleanUp stage");
            break;
    }
}

//------------------------------------------------------------------------------
void Trace3DTest::CleanUpStage0()
{
    CHIPLIB_CALLSCOPE_WITH_TESTNAME;

    UINT32 ft;

    EndChannelSwitch();

    if (m_TestGpuVerif)
    {
        delete m_TestGpuVerif;
        m_TestGpuVerif = 0;
    }

    // If there was a specified gpudevice and the bound gpu is null then
    // acquiring the bound resource failed.  There is nothing to clean up
    // since this happens at the start of Setup()
    if ((m_GpuInst != Gpu::UNSPECIFIED_DEV) && (m_pBoundGpu == nullptr))
        return;

    if (GetGpuResource() && params->ParamPresent("-clear_intr_on_exit"))
    {
        GpuDevice *pGpuDevice = GetBoundGpuDevice();
        for (UINT32 subdev = 0;
             subdev < pGpuDevice->GetNumSubdevices();
             subdev++)
        {
            RegHal& regHal = GetGpuResource()->GetRegHal(subdev, GetLwRmPtr(), m_pSmcEngine);
            // clear LW_PGRAPH_INTR by writing the value it contains back to it
            regHal.Write32(MODS_PGRAPH_INTR, regHal.Read32(MODS_PGRAPH_INTR));
        }
    }

    StopDisplay();

    // If test was aborted, we may have skipped WaitForIdle.  Wait for
    // channels to go idle before freeing surfaces to make sure GPU
    // doesn't access deleted surfaces.
    //
    if (m_bAborted)
    {
        for (auto ch : m_TraceCpuManagedChannels)
        {
            LWGpuChannel *pGpuChannel = ch->GetCh();
            if (pGpuChannel)
            {
                pGpuChannel->WaitForIdleRC(); // ignore errors; this is cleanup
            }
        }
    }

    // Free surfaces before freeing channels and tsgs
    //    some surfaces like host reflected surface must be freed before channel
    //
    DebugPrintf(MSGID(Main), "Trace3DTest::CleanUp; Free surfaces and textures.\n");
    for (auto& chunk : m_Chunks)
    {
        Chunk *pChunks = chunk.second;
        for (ft = GpuTrace::FT_FIRST_FILE_TYPE; ft < GpuTrace::FT_NUM_FILE_TYPE; ft++)
        {
            pChunks[ft].m_Surface.Free();
        }
    }

    {
        SharedSurfaceController * pSurfController = nullptr;
        if (GetGpuResource())
        {
            pSurfController = GetGpuResource()->GetSharedSurfaceController();
        }

        // Free Maps first as they need to be handled before the corresponding
        // virtual and physical allocations.
        for (auto& tex : m_Textures)
        {
            if (m_GlobalSharedSurfNames.find(tex.first->GetName()) != m_GlobalSharedSurfNames.end())
            {
                const string &global_name = m_GlobalSharedSurfNames[tex.first->GetName()];
                if (GetGpuResource()->GetSharedSurface(global_name.c_str()) != &(tex.second))
                    continue;
            }
            if (tex.first->GetSharedByTegra())
                continue;
            if (tex.first->IsShared())
            {
                string name = tex.first->GetName();
                LwRm::Handle hVaSpace = tex.first->GetVASpaceHandle();
                LwRm * pLwRm = GetLwRmPtr();
                if (pSurfController)
                {
                    pSurfController->FreeSharedSurf(name, hVaSpace, pLwRm);
                    pSurfController->FreeVirtSurf(name, hVaSpace, pLwRm);
                }
            }
            else if (tex.second.IsMapOnly())
            {
                tex.second.Free();
            }
        }

        for (auto& tex : m_Textures)
        {
            if (m_GlobalSharedSurfNames.find(tex.first->GetName()) != m_GlobalSharedSurfNames.end())
            {
                const string &global_name = m_GlobalSharedSurfNames[tex.first->GetName()];
                if (GetGpuResource()->GetSharedSurface(global_name.c_str()) != &(tex.second))
                    continue;
            }
            if (tex.first->GetSharedByTegra())
                continue;
            if (tex.first->IsShared())
            {
                string name = tex.first->GetName();
                LwRm::Handle hVaSpace = tex.first->GetVASpaceHandle();
                LwRm * pLwRm = GetLwRmPtr();
                if (pSurfController)
                {
                    pSurfController->FreeSharedSurf(name, hVaSpace, pLwRm);
                    pSurfController->FreeVirtSurf(name, hVaSpace, pLwRm);
                }
            }
            else if (!tex.second.IsMapOnly())
            {
                tex.second.Free();
            }
        }
    }

    m_Trace.ClearDmaLoaders();

    DebugPrintf(MSGID(Main), "Trace3DTest::CleanUp; Free channel(s).\n");
    // All instance block share same context header until bug 1697531 got fixed, so we need to free all channels before vaspace free
    FreeChannels();

    // Free surf manager after freeing channels.
    if (gsm)
    {
        DebugPrintf(MSGID(Main), "Trace3DTest::CleanUp; free surface manager.\n");
        gsm->Release();
        gsm = 0;
    }

    m_DmaBuffers.clear();

    m_Trace.ClearHeader();
    m_TraceFileMgr.ShutDown();

    m_EcovVerifier.reset(0);

    m_GSSpillRegion.Free();
    m_LwrrentChannel = 0;

    // cleanup the plugin, if any
    //
    if ( m_pPluginHost )
    {
        ShutdownPlugins();
    }

    m_Textures.clear();
}

//------------------------------------------------------------------------------
void Trace3DTest::CleanUpStage1()
{
    CHIPLIB_CALLSCOPE_WITH_TESTNAME;

    GpuDevice *pGpuDevice = nullptr;

    FreeResource();

    if (m_pBoundGpu)
    {
        pGpuDevice = m_pBoundGpu->GetGpuDevice();
        if (OK != m_pBoundGpu->RemoveActiveT3DTest(this))
        {
            MASSERT(0);
        }
        m_pBoundGpu = NULL;
    }

    // Re-init the GPU if the test called GpuDevice::Reset()
    //
    if (pGpuDevice && GetStatus() == TEST_SUCCEEDED)
    {
        if (Utl::HasInstance())
        {
            Utl::Instance()->FreePreResetRecoveryObjects();
        }
        pGpuDevice->RecoverFromReset();
    }
}

//------------------------------------------------------------------------------
/* virtual */ void Trace3DTest::BeforeCleanup()
{
    // bug 2237126, do the rm check which does the comparison on total
    // number of GFXP/CILP ctxsw requested against the actual number of
    // GFXP/CILP happened in the test, even with '-quick_exit'.
    if(params->ParamPresent("-quick_exit") &&
       !m_bAborted)
    {
        ProcessPreemptionStats();
    }
}

//------------------------------------------------------------------------------
void Trace3DTest::ProcessPreemptionStats()
{
    auto f = [this](UINT32 h)
    {
        RC rc;
        LW2080_CTRL_GR_PROCESS_PREEMPTION_STATS_PARAMS params = {0};
        params.hChannel = h;
        rc = m_pLwRm->Control(m_pLwRm->GetSubdeviceHandle(GetGpuResource()->GetGpuSubdevice()),
                              LW2080_CTRL_CMD_GR_PROCESS_PREEMPTION_STATS,
                              &params,
                              sizeof(params));
        if (OK != rc)
        {
            if (rc == RC::LWRM_NOT_SUPPORTED)
            {
                // If LW2080_CTRL_CMD_GR_PROCESS_PREEMPTION_STATS is not supported,
                // should not assert here.
                rc.Clear();
                WarnPrintf("ProcessPreemptionStats is not supported.\n");
            }
            else
            {
                ErrPrintf("Error call ProcessPreemptionStats in RM: %s\n", rc.Message());
                MASSERT(0);
            }
        }
    };

    for (auto ch : m_TraceGpuManagedChannels)
    {
        f(ch->GetCh()->ChannelHandle());
    }

    for (auto ch : m_TraceCpuManagedChannels)
    {
        f(ch->GetCh()->ChannelHandle());
    }
}

//------------------------------------------------------------------------------
RC Trace3DTest::FreeChannels()
{
    for (auto ch : m_TraceGpuManagedChannels)
    {
        ch->Free();
    }
    m_TraceGpuManagedChannels.clear();

    for (auto ch : m_TraceCpuManagedChannels)
    {
        ch->Free();
    }
    m_TraceCpuManagedChannels.clear();

    return OK;
}

RC Trace3DTest::FreeTsgs()
{
    for (auto tsg : m_TraceTsgs)
    {
        tsg->Free();
    }
    m_TraceTsgs.clear();

    return OK;
}

// the real test
int Trace3DTest::BeginIteration()
{
    // set up pushbuffers and Policy Manager. execute TraceOps. send methods.
    if (!RealSetup())
    {
        getStateReport()->error("Test failed to initialize correctly.");
        CritPrintf("Trace3DTest test specified failed to initialize correctly\n");
        state = FAILED; // state is FAILED for first loop
        return FAILED; // if RealSetup fails, return here to say so...
    }

    state = BUSY; // state is BUSY for first loop
    return BUSY;
}

// Kick off the test by moving put pointer to end of the trace
// Gets called for every loop
int Trace3DTest::StimulateDevice(int deviceStatus)
{
    if (m_bAborted)
        return FINISHED;

    if (params->ParamPresent("-sync_scanout"))
    {
        if (GetDefaultChannel()->GetCh()->GetUpdateType() != LWGpuChannel::MANUAL)
        {
            WarnPrintf("-sync_scanout expects a channel in MANUAL mode, have you forgot \"-single_kick\"?\n");
        }

        GetGpuResource()->DispPipeConfiguratorSetScanout(&m_bAborted);
    }

    TraceChannel* pTraceChannel = GetDefaultChannel();
    if (!pTraceChannel)
    {
        return FINISHED;
    }

    if (pTraceChannel->GetWfiMethod() != WFI_POLL &&
        pTraceChannel->GetWfiMethod() != WFI_SLEEP)
    {
        if (conlwrrent)
        {
            GetDefaultChannel()->GetCh()->SetDput(end_offset); // First get it going
            state = WAIT1;
        }
        else
        {
            for (auto ch : m_TraceCpuManagedChannels)
                ch->GetCh()->MethodFlush();
        }
    }
    //@@@ debug code, delete this
    //MASSERT(0);
    return BUSY;
}

// Polling until we are done this current loop iteration
int Trace3DTest::GetDeviceStatus()
{
    RC rc;

    TraceChannel* pTraceChannel = GetDefaultChannel();
    if (!pTraceChannel)
    {
        return FINISHED;
    }

    LWGpuChannel* pChDefault = pTraceChannel->GetCh();

    // Do not abort directly here.  Directly aborting after methods have been
    // flushed in StimulateDevice can result in crashes likely due to resources
    // being cleaned up while still in use (since the final WaitForIdle is
    // skipped)

    for (auto ch : m_TraceCpuManagedChannels)
    {
        rc = ch->GetCh()->CheckForErrorsRC();
        if (rc != OK)
        {
            ErrPrintf("Channel %s, error detected: %s\n", ch->GetName().c_str(), rc.Message());
            if (params->ParamPresent("-ignore_channel_errors"))
            {
                InfoPrintf("Channel error is ignored due to option -ignore_channel_error\n");
                return (FINISHED);
            }
            else
            {
                return (m_bAborted ? FINISHED : FAILED);
            }
        }
    }

    int ret = BUSY;
    if (m_CrcDumpMode == CrcEnums::CRC_REPORT)
    {
        // Don't bother waiting
        ret = FINISHED;
    }
    else if (conlwrrent)
    {
        // Note that -conlwrrent is only used with single-channel traces.

        if (state == WAIT1) { // GET is not at end of trace yet, keep polling
            UINT16 content = GetDefaultChannel()->PollNotifierValue();

            if (content != LW_OK) {
                // Not finished or an error
                if (content != 0xffff) {
                    // If not original value then we have an error
                    ErrPrintf("Unexpected notifier status %x\n", content);
                    ret = FINISHED;
                }
            } else {
                // NOTIFY has fired, so PUT can be moved back to start
                // Set state machine to GET to jump back to start
                state = WAIT2;
                pChDefault->SetDput(start_offset);   // Now execute jump back to start
            }
        } else if (state == WAIT2) { // GET is not at start
            // Wait for GET == start_offset
            if (start_offset == pChDefault->GetMinDGet(pChDefault->ChannelNum(),0)) {
                state = FINISHED;
                ret = FINISHED;
            }
        }
    }
    else
    {
        rc = WaitForIdle();
        if (OK == rc)
        {
            InfoPrintf("Trace3DTest::GetDeviceStatus -- wait for idle succeeded\n");
            ret = FINISHED;
        }
        else if ((RC::NOTIFIER_TIMEOUT == rc) || (RC::TIMEOUT_ERROR == rc))
        {
            WarnPrintf("Trace3DTest::GetDeviceStatus -- wait for idle returned timeout\n");
            ret = BUSY;
        }
        else
        {
            ErrPrintf("Trace3DTest::GetDeviceStatus -- wait for idle failed\n");
            ret = FAILED;
        }

        Platform::ProcessPendingInterrupts();

        // When using WaitForIdle, GetDeviceStatus() will only be called once,
        // In order to catch RC errors that occur when waiting for idle, check here
        // as well as at the beginning
        for (auto ch : m_TraceCpuManagedChannels)
        {
            rc = ch->GetCh()->CheckForErrorsRC();
            if (rc != OK)
            {
                ErrPrintf("Error detected for channel '%s': %s\n", ch->GetName().c_str(), rc.Message());

                if (params->ParamPresent("-ignore_channel_errors"))
                {
                    InfoPrintf("Channel error is ignored due to option -ignore_channel_error\n");
                    ret = FINISHED;
                }
                else
                {
                    return (m_bAborted ? FINISHED : FAILED);
                }
            }
        }
    }

    return (m_bAborted ? FINISHED : ret);
}

void Trace3DTest::StopPM()
{
    LWGpuResource* lwgpu = GetGpuResource();

    if ( params->ParamPresent("-stop_vpm_capture") )
    {
        for (UINT32 subdev = 0;
             subdev < GetBoundGpuDevice()->GetNumSubdevices();
             subdev++)
        {
            GetBoundGpuDevice()->GetSubdevice(subdev)->StopVpmCapture();
        }
    }

    if ( !lwgpu->PerfMon() )
    {
        return;
    }

    if ((pmMode != PM_MODE_NONE) && (pmMode != PM_MODE_PM_SB_FILE))
    {
        LWGpuChannel * pChDefault = GetDefaultChannel()->GetCh();
        TraceSubChannel* pTraceSubCh = GetDefaultChannel()->GetSubChannel("");
        lwgpu->PerfMonEnd(pChDefault, pTraceSubCh->GetSubChNum(),
            pTraceSubCh->GetClass());

        // Match -pmsb xml output
        if ( gXML )
            XD->XMLEndLwrrent();
    }
}

RC Trace3DTest::PolicyManagerStartTest()
{
    RC rc;

    PolicyManager *pPM = PolicyManager::Instance();
    if (pPM->IsInitialized())
    {
        if (params->ParamPresent("-skip_test_policy_registration"))
        {
            InfoPrintf("Skip registering the test %s to policy manager due to -skip_test_policy_registration\n",
                       GetTestName());
            PmTest * pTest = new PmTest_Trace3D(pPM, this);
            // Note: For the channel shared the same tsg with the channel in another test
            //       If this tsg faulting while the report channel is the unregistered channel
            //       it will report the error. So add the skip test channel to the inactive
            //       channel. See Bug  200175567
            for (auto ch : m_TraceCpuManagedChannels)
            {
                CHECK_RC(RegisterChannelToPolicyManager(pPM, pTest, ch));
            }
            return rc;
        }

        rc = RegisterWithPolicyManager();
        if (OK != rc)
        {
            ErrPrintf("trace_3d: RegisterWithPolicyManager failed: %s\n", rc.Message());
            return rc;
        }

        rc = pPM->StartTest(pPM->GetTestFromUniquePtr(this));
        if (OK != rc)
        {
            ErrPrintf("trace_3d: PolicyManager::StartTest failed: %s\n", rc.Message());
            return rc;
        }
    }

    return rc;
}

RC Trace3DTest::PolicyManagerEndTest()
{
    RC rc;

    PolicyManager *pPM = PolicyManager::Instance();
    if (pPM->IsInitialized())
    {
        rc = pPM->EndTest(pPM->GetTestFromUniquePtr(this));

        if (rc != OK)
        {
            InfoPrintf("PolicyManager reported a failure: %s\n", rc.Message());
            InfoPrintf("Test %s FAILED!\n", header_file.c_str());
        }

        return rc;
    }

    return OK;
}

bool Trace3DTest::StartPolicyManagerAfterSetup() const
{
    // After Volta, PolicyManager will be started after test setup by default
    // Another argument to disable the behavior might be added here in the future.
    return (params->ParamPresent("-start_policymn_after_setup") > 0) ||
        EngineClasses::IsGpuFamilyClassOrLater(
            TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_VOLTA);
}

void Trace3DTest::EndIteration()
{
    PolicyManager *pPM = PolicyManager::Instance();

    // When -nocheck presents, CheckPassFail will not be ilwoked at all.
    // However we still want to stop vpm capture and end PM, as well as
    // notify policy manager that the current test has completed
    if ( params->ParamPresent("-nocheck") )
    {
        // Also turn off ELPG to avoid spurious ELPG events after the
        // test is complete: see bug 692164
        RC rc = SwitchElpg(false);
        if (rc != OK)
        {
            ErrPrintf("trace_3d: failed to turn off ELPG: %s\n", rc.Message());
        }

        StopPM();

        if (pPM->IsInitialized())
        {
            pPM->EndTest(pPM->GetTestFromUniquePtr(this));
        }
    }
    else if (pPM->InTest(pPM->GetTestFromUniquePtr(this)))
    {
        // If the test iteration is ending and -nocheck is not specified,
        // then PolicyManager::EndTest should have been called from
        // Trace3DTest::CheckPassFail, if the test is still going then an
        // error oclwrred, notify policy manager to abort the test
        pPM->AbortTest(pPM->GetTestFromUniquePtr(this));
    }

    //
    // Release Channel mutex here if it has not been released in RealSetup()
    // due to -block.
    if (!IsBlockEnabled())
    {
        EndChannelSwitch();
    }
}

// Filter Ptimer interrupt message called by Trace3DTest::CheckPassFail()
// Return true to log this error string, false to ignore it
// Please reference ErrorLogFilterProc@errloggr.h for details
static bool LogFilterPtimerIntrMsg(const char *errMsg)
{
    return !Utility::MatchWildCard(errMsg, "processing ptimer interrupt*");
}

// Used by Trace3DTest::CheckPassFail() to poll for CPU model test completion.
// This function returns true when the CPU model test is no longer idle.
// CPU model test status is zero when idle, non-zero otherwise.
//
static bool WaitForCpuModelTest(void *args)
{
    UINT32 *cpuModelTestStatus = (UINT32 *) args;
    Platform::EscapeRead("CPU_MODEL|CM_STATUS", 0, 4, cpuModelTestStatus);
    return (*cpuModelTestStatus != 0);
}

Test::TestStatus Trace3DTest::CheckPassFail()
{
    CHIPLIB_CALLSCOPE_WITH_TESTNAME;
    SimClk::EventWrapper event(SimClk::Event::T3D_PASSFAIL);

    RC rc;
    rc = traceEvent(TraceEventType::BeforeCheckPassFail);
    if (rc != OK)
    {
        ErrPrintf("Error sending trace event\n");
        return (m_bAborted ? status : Test::TEST_FAILED);
    }

    // We'll only check plugin status if -skip_dev_init is specified.
    if (m_SkipDevInit)
    {
        return m_clientStatus;
    }

    // Check if there is unrecognized or ineffective CheetAh shared surface.
    rc = CheckIfAnyTegraSharedSurfaceLeft();
    if (rc != OK)
    {
        ErrPrintf("trace_3d: CheckIfAnyTegraSharedSurfaceLeft failed. See messages above: .\n", rc.Message());
        return Test::TEST_FAILED;
    }

    // Trace has completed (all traces in multi-trace tests with -block), this
    // is a good time to stop the PerfMon stuff and dump the report.
    // We don't want to capture the -dmaCheck activity, I think.
    LWGpuResource * lwgpu = GetGpuResource();

    StopPM();

    if (params->ParamPresent("-timer"))
    {
        meter->Stop ();
        RawPrintf("Time in seconds for %s is %g\n", header_file.c_str(), meter->GetElapsedTimeInSeconds());
    }

    // Turn off ELPG to avoid spurious ELPG events after the
    // test is complete: see bug 692164
    rc = SwitchElpg(false);
    if (rc != OK)
    {
        ErrPrintf("trace_3d: failed to turn off ELPG: %s\n", rc.Message());
        return Test::TEST_FAILED;
    }

    // Dynamic CRC check may modify the test status, to avoid normal CRC
    // check overriding the result, we should get the current state
    // See detail at bug 709656
    status = Test::TEST_INCOMPLETE == GetStatus() ? Test::TEST_SUCCEEDED : GetStatus();

    // If a CPU test is also running, make sure if finishes and then check
    // the error status.
    // Bug 3140240, skip CM_STATUS query for vf since vf only leverages special string for
    // -cpu_model_test to send gmmu mapping.
    if ((params->ParamPresent("-cpu_model_test") > 0) && CPUModel::Enabled() &&
        status == Test::TEST_SUCCEEDED && !m_bAborted &&
        !Platform::IsVirtFunMode())
    {
        InfoPrintf("Waiting for CPU model test to finish.\n");
        UINT32 cpuModelTestStatus = 1;
        POLLWRAP_HW(WaitForCpuModelTest, &cpuModelTestStatus,
            LwU64_LO32(GetTimeoutMs()));

        if (cpuModelTestStatus == 2)
        {
            ErrPrintf("CPU model test failed its internal CRC check.\n");
            status = Test::TEST_FAILED_CRC;
        }
        else if (cpuModelTestStatus >= 3)
        {
            ErrPrintf("CPU model test failed with an unknown error.\n");
            status = Test::TEST_FAILED;
        }
        else
        {
            InfoPrintf("CPU model test finished successfully.\n");
        }
    }

    bool deferPmEndTest = params->ParamPresent("-end_pmtest_after_crccheck") > 0;
    if (!deferPmEndTest)
    {
        if (PolicyManagerEndTest() != OK)
        {
            if (m_bAborted)
            {
                InfoPrintf("The failure reported from PolicyManager is ignored because the test is aborted!\n");
            }
            else
                status = Test::TEST_FAILED;
        }
    }

    CheckStopTestSyncOnFail("Ahead of CRC check");
    TestSynlwp(MDiagUtils::TestBeforeCrcCheck);

    LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS param;
    memset(&param, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));
    param.flags = FLD_SET_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FB_FLUSH, _YES, param.flags);
    if (!params->ParamPresent("-no_l2_flush"))
    {
        param.flags = FLD_SET_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _FULL_CACHE, param.flags);
        param.flags = FLD_SET_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES, param.flags);
        param.flags = FLD_SET_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES, param.flags);

        DebugPrintf(MSGID(Main), "Flushing L2...\n");
    }

    DebugPrintf(MSGID(Main), "Sending UFLUSH...\n");
    GpuDevice *pGpuDev = GetBoundGpuDevice();
    for (UINT32 subdev = 0; subdev < pGpuDev->GetNumSubdevices(); subdev++)
    {
        rc = m_pLwRm->Control(
            m_pLwRm->GetSubdeviceHandle(pGpuDev->GetSubdevice(subdev)),
            LW2080_CTRL_CMD_FB_FLUSH_GPU_CACHE,
            &param,
            sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

        if (rc != OK)
        {
            ErrPrintf("Error flushing l2 cache, message: %s\n", rc.Message());
            return (m_bAborted ? status : Test::TEST_FAILED);
        }
    }
    param.flags = FLD_SET_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _SYSTEM_MEMORY, param.flags);
    for (UINT32 subdev = 0; subdev < pGpuDev->GetNumSubdevices(); subdev++)
    {
        rc = m_pLwRm->Control(
            m_pLwRm->GetSubdeviceHandle(pGpuDev->GetSubdevice(subdev)),
            LW2080_CTRL_CMD_FB_FLUSH_GPU_CACHE,
            &param,
            sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

        if (rc != OK)
        {
            ErrPrintf("Error flushing l2 cache, message: %s\n", rc.Message());
            return (m_bAborted ? status : Test::TEST_FAILED);
        }
    }

    if (status == Test::TEST_SUCCEEDED)
    {
        status = m_clientStatus;
    }

#ifndef _WIN32
    if (m_CompBitsTest.get() != 0)
    {
        rc = m_CompBitsTest->PostRender();

        if (rc != OK)
        {
            ErrPrintf("m_CompBitsTest->PostRender() failed: %s\n", rc.Message());

            if (status == Test::TEST_SUCCEEDED)
            {
                if (rc == RC::GOLDEN_VALUE_MISCOMPARE)
                {
                    status = Test::TEST_FAILED_CRC;
                }
                else
                {
                    status = Test::TEST_FAILED;
                }
            }
        }
    }
#endif

    Platform::ProcessPendingInterrupts();

    if (!m_bAborted)
    {
        TestStatus surfaceStatus = Test::TEST_SUCCEEDED;
        TestStatus lwrStatus;
        for (UINT32 loop = 0; loop < params->ParamUnsigned("-loop_crc", 1); ++loop)
        {
            lwrStatus = (Test::TestStatus)VerifySurfaces(getStateReport());
            if (surfaceStatus == Test::TEST_SUCCEEDED)
                surfaceStatus = lwrStatus;
        }
        if (status == Test::TEST_SUCCEEDED)
            status = surfaceStatus;
    }
    else if (GetDefaultChannel())
    {
        TestStatus intrStatus;
        intrStatus = (Test::TestStatus)GetDefaultChannel()->GetGpuVerif()->DoIntrCheck(getStateReport());
        if (status == Test::TEST_SUCCEEDED)
            status = intrStatus;
    }

    if (deferPmEndTest)
    {
        if (PolicyManagerEndTest() != OK)
        {
            if (m_bAborted)
            {
                InfoPrintf("The failure reported from PolicyManager is ignored because the test is aborted!\n");
            }
            else
                status = Test::TEST_FAILED;
        }
    }

    // Checking Event-Coverage
    if (m_EcovVerifier.get())
    {
        VerifyEcov(m_EcovVerifier.get(), params, lwgpu, m_IsCtxSwitchTest, &status);
        m_EcovVerifier.reset(0);
    }

    // If PTIMER interrupt logging is enabled, ignore
    // the interrupt reported from RM right before ErrorLogger::TestCompleted()
    //
    // ARCH_EVENTS is defined in resman\arch\lwalloc\common\inc\oscommon.h,
    //
    // LW_ARCH_EVENT_PTIMER                        = 23,
    //
    UINT32 subscribeArchEvents{0};
    const UINT32 PtimerEventBitShift{23}; // LwArchEvents::LW_ARCH_EVENT_PTIMER
    rc = Registry::Read("ResourceManager",
                        LW_REG_STR_RM_SUBSCRIBE_TO_ARCH_EVENTS,
                        &subscribeArchEvents);
    if (rc != OK)
    {
        DebugPrintf("Reading registry %s.%s failed\n", "ResourceManager", LW_REG_STR_RM_SUBSCRIBE_TO_ARCH_EVENTS);
        rc.Clear();
    }
    else if (subscribeArchEvents & (1 << PtimerEventBitShift))
    {
        ErrorLogger::InstallErrorLogFilterForThisTest(LogFilterPtimerIntrMsg);
        InfoPrintf("Install a filter to ignore PTIMER message after interrupt check\n");
    }

    // VerifySurfaces calls into DoCrcCheck, which eventually calls the function
    // that checks for HW engine errors
    rc = ErrorLogger::TestCompleted();
    if (rc != OK)
    {
        InfoPrintf("Unhandled error(s) detected in ErrorLogger: %s\n", rc.Message());
        InfoPrintf("Test %s FAILED!\n", header_file.c_str());

        if (status == Test::TEST_SUCCEEDED)
            status = Test::TEST_FAILED;
    }

    /////////////////////////////////////////////////////////////////////////////////
    DispPipeConfig::DispPipeConfigurator* pDpcConfigurator = lwgpu->GetDispPipeConfigurator();
    if ((pDpcConfigurator != 0) && !m_bAborted)
    {
        MdiagSurf* pLumaSurface = nullptr;
        MdiagSurf* pChromaSurface = nullptr;
        if (pDpcConfigurator->GetExelwteAtEnd())
        {
            if (params->ParamPresent("-shared_luma_surface") &&
                params->ParamPresent("-shared_chroma_surface"))
            {
                const char* lumaName = params->ParamStr("-shared_luma_surface");
                InfoPrintf("%s: shared luma surface %s\n", __FUNCTION__, lumaName);
                pLumaSurface = GetSurfaceByName(lumaName);

                const char* chromaName = params->ParamStr("-shared_chroma_surface");
                InfoPrintf("%s: shared chroma surface %s\n", __FUNCTION__, chromaName);
                pChromaSurface = GetSurfaceByName(chromaName);
            }
        }
        RC dpcRc = GetGpuResource()->DispPipeConfiguratorRunTest(&m_bAborted, pLumaSurface, pChromaSurface, params->ParamPresent("-sync_scanout_end"));

        if (dpcRc != OK)
        {
            InfoPrintf("Errors detected in DPC test: %s\n", rc.Message());
            InfoPrintf("Test %s FAILED!\n", header_file.c_str());

            if (status == Test::TEST_SUCCEEDED)
                status = Test::TEST_FAILED_CRC;
        }

        if (params->ParamPresent("-quick_exit"))
        {
            InfoPrintf("trace_3d: Skipped Cleanup of DPC test\n");
        }
        else
        {
            if (GetGpuResource()->DispPipeConfiguratorCleanup() != RC::OK)
            {
                ErrPrintf("trace_3d: Error cleaning up DPC test\n");
                return Test::TEST_FAILED;
            }
        }
    }

    TestStatus nStatus;
    if (m_TraceCpuManagedChannels.empty())
    {
        nStatus = TestStatus(m_TestGpuVerif->GetCrcChecker(CT_LwlCounters)->Check(0));
    }
    else
    {
        nStatus = TestStatus(GetDefaultChannel()->GetGpuVerif()->GetCrcChecker(CT_LwlCounters)->Check(0));
    }
    if (Test::TEST_SUCCEEDED == status && Test::TEST_SUCCEEDED != nStatus)
    {
        status = nStatus;
    }

    // dump crc files
    if (m_profile->GetDump())
    {
        if (!m_profile->SaveToFile())
        {
            ErrPrintf("Fail to dump crc files.\n");
        }
    }

    // bug 480482: dump all buffers after test.
    // here dump all writable non color/z and non CRC checked surfaces and buffers.
    // all color/z and CRC checked surfaces and buffers are dumped by setting m_forceImages to true.
    if (params->ParamPresent("-dump_all_buffer_after_test"))
    {
        if (DumpAllBuffers() != OK)
        {
            ErrPrintf("Error oclwrred when dumping all buffers after test.\n");
        }
    }

    //////////////////////////// Begin Trace Event ////////////////////////
    //
    rc = traceEvent(TraceEventType::AfterCheckPassFail);
    if ( rc != OK )
    {
        ErrPrintf("trace_3d: Error in processing trace event AfterCheckPassFail:\n" );
        return (m_bAborted ? status : Test::TEST_FAILED);
    }
    //
    ///////////////////////////  End Trace Event /////////////////////////

    // Check client status again since plugin might still fail
    // the test in the AfterCheckPassFail handler.
    if (status == Test::TEST_SUCCEEDED)
    {
        status = m_clientStatus;
    }

    if (m_bAborted)
        InfoPrintf("Test %s ABORTED!\n", header_file.c_str());
    else if (status == Test::TEST_SUCCEEDED)
        InfoPrintf("Test %s PASSED!\n", header_file.c_str());
    else if (status == Test::TEST_FAILED)
        InfoPrintf("Test %s FAILED!\n", header_file.c_str());
    else if (status == Test::TEST_FAILED_CRC)
        InfoPrintf("Test %s FAILED CRC MATCH!\n", header_file.c_str());
    else if (status == Test::TEST_CRC_NON_EXISTANT)
        InfoPrintf("Test %s FAILED: MISSING CRC in profile.\n", header_file.c_str());
    else if (status == Test::TEST_FAILED_ECOV)
        InfoPrintf("Test %s FAILED ECOV matching!\n", header_file.c_str());
    else if (status == Test::TEST_ECOV_NON_EXISTANT)
        InfoPrintf("Test %s FAILED: ECov file error or missing!\n", header_file.c_str());
    else
        InfoPrintf("Test %s FAILED - UNKNOWN REASON.\n", header_file.c_str());

    return status;
}

//
// dump all writable non color/z and non CRC checked buffers
//
RC Trace3DTest::DumpAllBuffers()
{
    RC rc = OK;

    // dump all non-color/z and non-CRC checked buffers by traversing all modules
    for (ModuleIter mit = m_Trace.ModBegin(); mit != m_Trace.ModEnd(); ++mit)
    {
        if ((*mit)->IsColorOrZ())
        {
            continue;
        }

        TraceModule::ModCheckInfo checkInfo;
        (*mit)->GetCheckInfo(checkInfo);

        if (NO_CHECK != checkInfo.CheckMethod)
        {
            continue;
        }

        GpuVerif *gpuverif = (checkInfo.pTraceCh)->GetGpuVerif();
        MASSERT(gpuverif != 0);

        MdiagSurf *dma = (*mit)->GetDmaBufferNonConst();
        if (dma && // valid DMA buffer
            dma->GetMemHandle() && // not freed yet
            (dma->GetProtect() & Memory::Writeable)) // writable
        {
            string fname = (*mit)->GetName();

            for (UINT32 subdev = 0; subdev < GetBoundGpuDevice()->GetNumSubdevices(); ++subdev)
            {
                CHECK_RC(gpuverif->DumpSurfOrBuf(dma, fname, subdev));
            }
        }
    }

    return rc;
}

TestEnums::TEST_STATUS Trace3DTest::VerifySurfaces2SubDev(ITestStateReport *report, UINT32 subdev)
{
    list<TestEnums::TEST_STATUS> status;

    // If any channels are disabled, then get a list of the surfaces
    // associated with those channels.  We won't CRC those surfaces.
    //
    SurfaceSet skipSurfaces; // Surfaces that we won't CRC because
    for (auto ch : m_TraceCpuManagedChannels)
    {
        if (!ch->GetCh()->GetEnabled())
        {
            SurfaceSet relocSurfaces;
            ch->GetRelocSurfaces(&relocSurfaces);
            skipSurfaces.insert(relocSurfaces.begin(), relocSurfaces.end());
        }
    }

    // Run CRC checks
    //
    FILE* fp = 0;
    string fname = params->ParamStr("-o", "test");
    fname += ".inf";
    RC rc = Utility::OpenFile(fname.c_str(), &fp, "w");
    if (OK != rc)
        return TestEnums::TEST_FAILED;
    fclose(fp);

    string dummyChName;
    if (params->ParamPresent("-dummy_ch_for_offload"))
    {
        dummyChName = params->ParamNStr("-dummy_ch_for_offload", 0);
    }
    else if (params->ParamPresent("-dummy_gr_channel"))
    {
        dummyChName = params->ParamNStr("-dummy_gr_channel", 0);
    }

    if (m_TraceCpuManagedChannels.empty())
    {
        status.push_back(m_TestGpuVerif->DoCrcCheck(report, subdev, &skipSurfaces));
    }
    else
    {
        bool bAllChannelsDummy = true;
        for (auto ch : m_TraceCpuManagedChannels)
        {
            // Hack for bug 818386.
            // Escape CRC check for the dummy graphic channel.
            if (dummyChName == ch->GetName())
            {
                continue;
            }

            bAllChannelsDummy = false;

            // Skip Crc check on host only channels since they cannot have Crc checkers
            if (!ch->IsHostOnlyChannel())
            {
                status.push_back(ch->VerifySurfaces(report, subdev, &skipSurfaces));
            }
        }

        if (bAllChannelsDummy)
        {
            // same behavior as the case m_TraceCpuManagedChannels.empty()
            //
            status.push_back(m_TraceCpuManagedChannels.front()->VerifySurfaces(
                report, subdev, &skipSurfaces));
        }
    }
    // combining status from each channel into one; this may need to get revisited
    list<TestEnums::TEST_STATUS>::const_iterator iter = status.begin();
    list<TestEnums::TEST_STATUS>::const_iterator stat_end = status.end();
    for ( ; iter != stat_end; ++iter)
    {
        if (*iter != TestEnums::TEST_SUCCEEDED)
            break;
    }

    return iter == stat_end ? TestEnums::TEST_SUCCEEDED : *iter;
}

TestEnums::TEST_STATUS Trace3DTest::VerifySurfaces(ITestStateReport *report)
{
    UINT32 subdev;
    list<TestEnums::TEST_STATUS> status;
    for (subdev = 0; subdev < GetBoundGpuDevice()->GetNumSubdevices(); ++subdev)
    {
        status.push_back(VerifySurfaces2SubDev(report, subdev));
    }

    // If the trace has self-gilding but the the command-line specifies
    // that self-gilding should be ignored, make sure that at least
    // one CRC or reference check has been performed; otherwise issue
    // an error message.
    if (!m_Trace.GetSelfgildStates().empty() &&
        params->ParamPresent("-ignore_selfgild"))
    {
        UINT32 checksPerformed = 0;

        for (auto ch : m_TraceCpuManagedChannels)
        {
            if (!ch->IsHostOnlyChannel())
            {
                GpuVerif* verif = ch->GetGpuVerif();
                checksPerformed += verif->ChecksPerformed();
            }
        }

        if (checksPerformed == 0)
        {
            ErrPrintf("-ignore_selfgild was specified but no other CRC or reference checks were performed\n");
            return TestEnums::TEST_FAILED;
        }
    }

    // combining status from each gpu into one; this may need to get revisited
    list<TestEnums::TEST_STATUS>::const_iterator stat_end = status.end();
    list<TestEnums::TEST_STATUS>::const_iterator iter = status.begin();
    for ( ; iter != stat_end; ++iter)
    {
        if (*iter != TestEnums::TEST_SUCCEEDED)
            break;
    }

    return iter == stat_end ? TestEnums::TEST_SUCCEEDED : *iter;
}

int Trace3DTest::SetupPM(const ArgReader* params)
{
    const char *trgFilename = nullptr;
    LWGpuResource * lwgpu = GetGpuResource();

    // Never setup PM for amodel
    if ( Platform::GetSimulationMode() == Platform::Amodel )
        return 1;

    if (params->ParamPresent("-pm_sb_file"))
    {
        trgFilename = params->ParamStr("-pm_sb_file");
        pmMode = PM_MODE_PM_SB_FILE;
        InfoPrintf("Trace3DTest::SetupPM -pm_sb_file specified trigger %s state buckets enabled\n", trgFilename);
    }
    else if (params->ParamPresent("-pm_file"))
    {
        trgFilename = params->ParamStr("-pm_file");
        pmMode = PM_MODE_PM_FILE;
        InfoPrintf("Trace3DTest::SetupPM -pm_file PM resource files %s\n", trgFilename);
    }
    else
    {
        // nothing to do if we don't have a perf file
        InfoPrintf("Trace3DTest::SetupPM -pm_file or -pm_sb_file is not specified!\n");
        return 1;
    }

    lwgpu->CreatePerformanceMonitor();

    if ( pmMode == PM_MODE_NONE )
        return 1;

    const char *option_type = "STANDARD";

    if (lwgpu->GetLwgpuArgs()->ParamPresent("-pm_option"))
    {
        option_type = lwgpu->GetLwgpuArgs()->ParamStr("-pm_option");
    }

    if (!lwgpu->PerfMonInitialize(option_type, trgFilename))
    {
        ErrPrintf("Initialize PM failed!\n");
        return 0;
    }

    InfoPrintf("SetupPM DONE\n");

    return 1;
}

// Used for sorting buffers to allocate.  Buffers which fixed
// addresses should come first, then buffers with address range
// constraints, then all other buffers.
//
bool Trace3DTest::AllocInfo::operator<(const AllocInfo &rhs) const
{
    if (HasLockedAddress() && !rhs.HasLockedAddress())
    {
        return true;
    }
    else if (!HasLockedAddress() && rhs.HasLockedAddress())
    {
        return false;
    }
    else if (HasAddressRange() && !rhs.HasAddressRange())
    {
        return true;
    }

    return false;
}

// Is this buffer constrained to a specific address?
//
bool Trace3DTest::AllocInfo::HasLockedAddress() const
{
    if (m_Surface->HasFixedVirtAddr())
    {
        return true;
    }
    else if (m_Surface->HasFixedPhysAddr())
    {
        return true;
    }

    return false;
}

// Is this buffer constrained to an address range?
//
bool Trace3DTest::AllocInfo::HasAddressRange() const
{
    if (m_Surface->HasVirtAddrRange())
    {
        return true;
    }
    else if (m_Surface->HasPhysAddrRange())
    {
        return true;
    }

    return false;
}

RC Trace3DTest::InitAllocator()
{
    RC rc;
    vector<AllocInfo> buffersToAlloc;

    // imply that individual surfaces are moved to m_Textures
    // then m_Textures will be handled by InitIndividualBuffer
    CHECK_RC(InitChunkBuffer(&buffersToAlloc));

    CHECK_RC(InitIndividualBuffer(&buffersToAlloc));

    // If -buffer_alloc_order is set to 1, sort the buffers to be allocated
    // such that those with address constraints come first.  Otherwise,
    // use the default ordering.
    if (params->ParamUnsigned("-buffer_alloc_order", 1) == 1)
    {
        stable_sort(buffersToAlloc.begin(), buffersToAlloc.end());
    }

    //Do setup, if required, for p2p accesses
    GetP2pSM()->SetParams(params);
    CHECK_RC(GetP2pSM()->Setup());

    CHECK_RC(AllocateBuffer(&buffersToAlloc));

    // loop back mode individual surfaces have already mapped in AllocateIndividualBuffer
    CHECK_RC(MapLoopbackChunkBuffer());

    return OK;
}

RC Trace3DTest::InitChunkBuffer(vector<AllocInfo>* pBuffersToAlloc)
{
    RC rc;

    for (auto& chunk: m_Chunks)
    {
        CHECK_RC(InitChunkBuffer(chunk.first, chunk.second, pBuffersToAlloc));
    }

    return rc;
}

// init chunk buffer for each VA space
RC Trace3DTest::InitChunkBuffer(LwRm::Handle hVASpace, Chunk* pChunks, vector<AllocInfo>* pBuffersToAlloc)
{
    RC rc;

    for (int ft = GpuTrace::FT_FIRST_FILE_TYPE; ft < GpuTrace::FT_NUM_FILE_TYPE; ft++)
    {
        pChunks[ft].m_Surface.SetArrayPitch(m_Trace.GetUsageByFileType((GpuTrace::TraceFileType)ft, true, hVASpace));
        pChunks[ft].m_Surface.SetColorFormat(ColorUtils::Y8);
        pChunks[ft].m_Surface.SetForceSizeAlloc(true);
        pChunks[ft].m_Surface.SetProtect(m_Trace.GetProtectByFileType((GpuTrace::TraceFileType)ft));

        string argname = Utility::StrPrintf("-loc_%s", GpuTrace::GetFileTypeData(ft).ArgName);
        if (params->ParamPresent("-sli_p2ploopback") > 0 &&
                ft != GpuTrace::FT_PUSHBUFFER &&  //pb never goes to peer memory
                ft != GpuTrace::FT_SELFGILD &&
                (_DMA_TARGET)params->ParamUnsigned(argname.c_str(), _DMA_TARGET_VIDEO) == _DMA_TARGET_P2P)
        {
            pChunks[ft].m_Surface.SetLocation(Memory::Fb);
            pChunks[ft].m_Surface.SetLoopBack(true);
        }
    }

    // Some surfaces need to be writeable.  This is only done in trace version 0,
    // since version 1 allows the trace to specify the correct permissions instead.
    if (m_Trace.GetVersion() == 0)
    {
        pChunks[GpuTrace::FT_SHADER_PROGRAM].m_Surface.SetProtect(Memory::ReadWrite);
        pChunks[GpuTrace::FT_CONSTANT_BUFFER].m_Surface.SetProtect(Memory::ReadWrite);
        pChunks[GpuTrace::FT_TEXTURE].m_Surface.SetProtect(Memory::ReadWrite);
        pChunks[GpuTrace::FT_SHADER_THREAD_MEMORY].m_Surface.SetProtect(Memory::ReadWrite);
        pChunks[GpuTrace::FT_SHADER_THREAD_STACK].m_Surface.SetProtect(Memory::ReadWrite);
        pChunks[GpuTrace::FT_SEMAPHORE].m_Surface.SetProtect(Memory::ReadWrite);
        pChunks[GpuTrace::FT_SEMAPHORE_16].m_Surface.SetProtect(Memory::ReadWrite);
        pChunks[GpuTrace::FT_NOTIFIER].m_Surface.SetProtect(Memory::ReadWrite);
        pChunks[GpuTrace::FT_STREAM_OUTPUT].m_Surface.SetProtect(Memory::ReadWrite);
        pChunks[GpuTrace::FT_LOD_STAT].m_Surface.SetProtect(Memory::ReadWrite);
        pChunks[GpuTrace::FT_ZLWLL_RAM].m_Surface.SetProtect(Memory::ReadWrite);

        for (int gmem = GpuTrace::FT_GMEM_A; gmem <= GpuTrace::FT_GMEM_P; ++gmem)
        {
            pChunks[gmem].m_Surface.SetProtect(Memory::ReadWrite);
        }
        for (int pmu = GpuTrace::FT_PMU_0; pmu <= GpuTrace::FT_PMU_7; ++pmu)
        {
            pChunks[pmu].m_Surface.SetProtect(Memory::ReadWrite);
        }
    }

    for (int gmem = GpuTrace::FT_GMEM_A; gmem <= GpuTrace::FT_GMEM_P; ++gmem)
    {
        ModuleIter end = m_Trace.ModEnd();
        for (ModuleIter iter = m_Trace.ModBegin(); iter != end; ++iter)
        {
            if (((*iter)->GetFileType() == gmem) && ((*iter)->GetVASpaceHandle() == hVASpace))
            {
                if (!(*iter)->HasAttrOverride())
                {
                    if ((*iter)->GetBlockLinear())
                    {
                        pChunks[gmem].m_Surface.SetLayout(Surface2D::BlockLinear);
                        pChunks[gmem].m_Surface.SetColorFormat(ColorUtils::A8R8G8B8);
                    }
                    else
                    {
                        pChunks[gmem].m_Surface.SetLayout(Surface2D::Pitch);
                    }
                }
            }

        }

        if (params->ParamPresent("-pte_kind") > 0 &&
                Platform::GetSimulationMode() != Platform::Amodel)
        {
            UINT32 PteKind;
            const char *PteKindName = params->ParamStr("-pte_kind");
            if (!GetBoundGpuDevice()->GetSubdevice(0)->
                    GetPteKindFromName(PteKindName, &PteKind))
            {
                ErrPrintf("Invalid PTE kind name: %s\n", PteKindName);
                MASSERT(0);
            }
            pChunks[gmem].m_Surface.SetPteKind(PteKind);
        }

        string gmem_name = Utility::StrPrintf("gmem_%c", 'A' + gmem - GpuTrace::FT_GMEM_A);

        // This is called here rather than in the constructor so that the
        // code above that sets defaults for gmem can run first.
        ParseArgsForType((GpuTrace::TraceFileType)gmem, params,
                gmem_name.c_str(), hVASpace);

        // Need to process gmem options after ParseArgsForType() called.
        // This is redundant -- For now only process loc_gmem.
        _DMA_TARGET target = (_DMA_TARGET)params->ParamUnsigned("-loc_gmem", _DMA_TARGET_VIDEO);
        gmem_name = Utility::StrPrintf("-loc_gmem_%c", 'A' + gmem - GpuTrace::FT_GMEM_A);
        if (params->ParamPresent(gmem_name.c_str()) > 0)
            target = (_DMA_TARGET)params->ParamUnsigned(gmem_name.c_str(), target);
        if (target == _DMA_TARGET_P2P)
        {
            target = _DMA_TARGET_VIDEO;
        }
        pChunks[gmem].m_Surface.SetLocation(TargetToLocation(target));
    }

    // Scale down the size of the shader thread memory and stack based on thread
    // ID throttling
    // This should not happen when new option -thread_stack_throttle_method_hw
    // presents. See bug 209737
    if ( !params->ParamPresent("-thread_stack_throttle_method_hw") ) {
        int shift = 0;
        switch (params->ParamUnsigned("-thread_stack_throttle", 24))
        {
            case 1:
                shift = 5;
                break;
            case 2:
                shift = 4;
                break;
            case 4:
                shift = 3;
                break;
            case 8:
                shift = 2;
                break;
            case 16:
                shift = 1;
                break;
            case 24:
                break;
            default:
                return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        pChunks[GpuTrace::FT_SHADER_THREAD_STACK].m_Surface.SetArrayPitch(
                pChunks[GpuTrace::FT_SHADER_THREAD_STACK].m_Surface.GetArrayPitch() >> shift);
    }

    // Scale down the size of the shader thread memory and stack based on thread
    // colwoys
    if (params->ParamPresent("-no_colwoys"))
    {
        pChunks[GpuTrace::FT_SHADER_THREAD_STACK].m_Surface.SetArrayPitch(
                pChunks[GpuTrace::FT_SHADER_THREAD_STACK].m_Surface.GetArrayPitch() / 2);
    }

    // Scale up the size of the shader stack memory based on # of SMs
    UINT64 shaderStackSize = pChunks[GpuTrace::FT_SHADER_THREAD_STACK].m_Surface.GetArrayPitch();
    if (shaderStackSize > 0)
    {
        UINT32 scale =
            GetTPCCount(0) * GetGrInfo(LW2080_CTRL_GR_INFO_INDEX_LITTER_NUM_SM_PER_TPC, 0);
        pChunks[GpuTrace::FT_SHADER_THREAD_STACK].m_Surface.SetArrayPitch(shaderStackSize * scale);
    }

    // Thread memory has already been scaled up
    UINT64 ShaderMemSize = pChunks[GpuTrace::FT_SHADER_THREAD_MEMORY].m_Surface.GetArrayPitch();
    if (ShaderMemSize > 0)
    {
        // Increase the size since we just cannot do the correct alignment for all models
        pChunks[GpuTrace::FT_SHADER_THREAD_MEMORY].m_Surface.SetArrayPitch(ShaderMemSize +
                m_Trace.GetFileTypeAlign(GpuTrace::FT_SHADER_THREAD_MEMORY));
    }

    // We don't use this pushbuffer, so don't waste the space
    pChunks[GpuTrace::FT_PUSHBUFFER].m_Surface.SetArrayPitch(0);

    // Buffers created with the ALLOC_SURFACE trace command are
    // always allocated individually.
    pChunks[GpuTrace::FT_ALLOC_SURFACE].m_Surface.SetArrayPitch(0);

    // We don't really have enough info to do this right
    pChunks[GpuTrace::FT_TEXTURE].m_Surface.SetLayout(Surface2D::BlockLinear);
    pChunks[GpuTrace::FT_TEXTURE].m_Surface.SetColorFormat(ColorUtils::R5G6B5);

    // textures should be allocated separately in paging memory model
    if (pChunks[GpuTrace::FT_TEXTURE].m_Surface.GetAddressModel() == Memory::Paging)
    {
        pChunks[GpuTrace::FT_TEXTURE].m_Surface.SetArrayPitch(0);
        ModuleIter modIt;
        UINT64 hintIncrement = 0;
        const UINT64 bigPageSize = GetBoundGpuDevice()->GetBigPageSize();

        for (modIt = m_Trace.ModBegin(); modIt != m_Trace.ModEnd(); ++modIt)
        {
            TraceModule * mod = *modIt;
            if ((mod->GetFileType() == GpuTrace::FT_TEXTURE) && (mod->GetVASpaceHandle() == hVASpace))
            {
                MdiagSurf buf(pChunks[GpuTrace::FT_TEXTURE].m_Surface);
                buf.SetArrayPitch((mod->GetTxParams()->Size() + 255) & ~255);
                buf.SetProtect(Memory::Protect(mod->GetProtect() |
                            pChunks[GpuTrace::FT_TEXTURE].m_Surface.GetProtect()));
                buf.SetType(mod->GetHeapType());
                // Override the buf's color format if texture header returns a valid one,
                // otherwise keep the default format.
                if (mod->GetTxParams()->GetColorFormat() != ColorUtils::LWFMT_NONE)
                {
                    buf.SetColorFormat(mod->GetTxParams()->GetColorFormat());
                }

                if ((mod->GetGpuCacheMode() != Surface2D::GpuCacheDefault) &&
                        (buf.GetGpuCacheMode() == Surface2D::GpuCacheDefault))
                {
                    buf.SetGpuCacheMode(mod->GetGpuCacheMode());
                }

                if ((mod->GetP2PGpuCacheMode() != Surface2D::GpuCacheDefault) &&
                        (buf.GetP2PGpuCacheMode() == Surface2D::GpuCacheDefault))
                {
                    buf.SetP2PGpuCacheMode(mod->GetP2PGpuCacheMode());
                }

                UINT32 attr = mod->GetHeapAttr();
                if (m_Trace.BufferLwlled(mod->GetName()))
                {
                    switch((ZLwllEnum)params->ParamUnsigned("-zlwll_mode", ZLWLL_REQUIRED))
                    {
                        case ZLWLL_REQUIRED:
                            attr |= DRF_DEF(OS32, _ATTR, _ZLWLL, _REQUIRED);
                            break;
                        case ZLWLL_ANY:
                            attr |= DRF_DEF(OS32, _ATTR, _ZLWLL, _ANY);
                            break;
                        case ZLWLL_NONE:
                            attr |= DRF_DEF(OS32, _ATTR, _ZLWLL, _NONE);
                            break;
                        default:
                            return RC::BAD_COMMAND_LINE_ARGUMENT;
                    }
                }

                if (mod->HasAttrSet())
                {
                    buf.ConfigFromAttr(attr);
                }

                if (mod->HasAttr2Set())
                {
                    buf.ConfigFromAttr2(mod->GetHeapAttr2());
                }

                if (mod->HasVprSet())
                {
                    buf.SetVideoProtected(mod->GetVpr());
                }

                if (buf.GetLocation() == (Memory::Location)0xFFFFFFFF)
                    buf.SetLocation(TargetToLocation(mod->GetLocation()));

                if (buf.HasFixedVirtAddr())
                {
                    buf.SetFixedVirtAddr(buf.GetFixedVirtAddr() +
                            hintIncrement);
                }
                else
                {
                    if (mod->HasAddressRange())
                    {
                        buf.SetVirtAddrRange(mod->GetAddressRange().first,
                            mod->GetAddressRange().second);
                    }
                }

                if (buf.HasFixedPhysAddr())
                {
                    buf.SetFixedPhysAddr(buf.GetFixedPhysAddr() +
                            hintIncrement);
                }
                hintIncrement += ALIGN_UP(
                        buf.EstimateSize(GetBoundGpuDevice()) +
                        buf.GetHiddenAllocSize() +
                        buf.GetExtraAllocSize(),
                        bigPageSize);

                if (mod->GetCompressed())
                {
                    switch (params->ParamUnsigned("-compress_mode", COMPR_REQUIRED))
                    {
                        case COMPR_REQUIRED:
                            buf.SetCompressed(true);
                            buf.SetCompressedFlag(LWOS32_ATTR_COMPR_REQUIRED);
                            break;

                        case COMPR_ANY:
                            buf.SetCompressed(true);
                            buf.SetCompressedFlag(LWOS32_ATTR_COMPR_ANY);
                            break;

                        default:
                            buf.SetCompressed(false);
                    }
                    buf.SetPteKind(-1);
                    buf.SetComptagCovMin(0);
                    buf.SetComptagCovMax(params->ParamUnsigned("-comptag_covg_tex", 100));
                    buf.SetComptagStart(0);
                } // if (mod->GetCompressed())

                if (mod->GetMapToBackingStore()) // special buffer mapp'ed to backing store
                {
                    LW0080_CTRL_FB_GET_COMPBIT_STORE_INFO_PARAMS param = {0};

                    CHECK_RC(m_pLwRm->Control(
                                m_pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                                LW0080_CTRL_CMD_FB_GET_COMPBIT_STORE_INFO,
                                &param, sizeof(param)));

                    if (param.AddressSpace == LW0080_CTRL_CMD_FB_GET_COMPBIT_STORE_INFO_ADDRESS_SPACE_SYSMEM)
                    {
                        InfoPrintf("Backing store is allocated in sysmem \n");
                    }
                    else if (param.AddressSpace != LW0080_CTRL_CMD_FB_GET_COMPBIT_STORE_INFO_ADDRESS_SPACE_FBMEM)
                    {
                        MASSERT(!"MODS cannot support unknown backing store aperture!");
                    }

                    buf.SetFixedPhysAddr(param.Address);

                    buf.SetLocation(Memory::Optimal);
                }

                UINT32 paramCount = params->ParamPresent("-vpr_texture");
                for (UINT32 i = 0; i < paramCount; ++i)
                {
                    if (params->ParamNStr("-vpr_texture", i, 0) == mod->GetName())
                    {
                        const string &value = params->ParamNStr("-vpr_texture", i, 1);

                        if (value == "ON")
                        {
                            buf.SetVideoProtected(true);
                        }
                        else if (value == "OFF")
                        {
                            buf.SetVideoProtected(false);
                        }
                        else
                        {
                            ErrPrintf("Unrecognized VPR value %s for -vpr_texture command.\n", value.c_str());

                            return RC::BAD_COMMAND_LINE_ARGUMENT;
                        }
                    }
                }

                // Individual buffer should has its name set to the module's name.
                buf.SetName(mod->GetName());

                m_Textures.push_back(make_pair(mod, buf));
            }
        }
    }

    // VP2 surfaces should be allocated separately in paging memory model.
    for (int ft = GpuTrace::FT_VP2_0; ft <= GpuTrace::FT_VP2_9; ft++)
    {
        // The shared MdiagSurf is not used.
        pChunks[ft].m_Surface.SetArrayPitch(0);

        ModuleIter modIt;
        UINT64     size = 0;

        for (modIt = m_Trace.ModBegin(); modIt != m_Trace.ModEnd(); ++modIt)
        {
            TraceModule * mod = *modIt;
            if ((mod->GetFileType() == ft) && (mod->GetVASpaceHandle() == hVASpace))
            {
                // Give this module a private DmaBuffer.
                MdiagSurf buf(pChunks[ft].m_Surface);
                buf.SetArrayPitch((mod->GetSize() + 255) & ~255);
                buf.SetProtect(mod->GetProtect());
                buf.SetType(mod->GetHeapType());
                buf.SetGpuVASpace(mod->GetVASpaceHandle());

                if (mod->HasAttrSet())
                {
                    buf.ConfigFromAttr(mod->GetHeapAttr());
                }

                if (mod->HasAttr2Set())
                {
                    buf.ConfigFromAttr2(mod->GetHeapAttr2());
                }

                if (mod->HasVprSet())
                {
                    buf.SetVideoProtected(mod->GetVpr());
                }

                UINT32 width, height;
                if (OK == mod->GetVP2Width(&width))
                {
                    buf.SetWidth(width);
                }
                else
                {
                    MASSERT(!"GetVP2Width failed.\n");
                }
                if (OK == mod->GetVP2Height(&height))
                {
                    buf.SetHeight(height);
                }
                else
                {
                    MASSERT(!"GetVP2Height failed.\n");
                }

                if (buf.HasFixedVirtAddr())
                {
                    buf.SetFixedVirtAddr(buf.GetFixedVirtAddr()+size);
                }
                else
                {
                    if (mod->HasAddressRange())
                    {
                        buf.SetVirtAddrRange(mod->GetAddressRange().first,
                            mod->GetAddressRange().second);
                    }
                }

                const UINT64 bigPageSize = GetBoundGpuDevice()->GetBigPageSize();
                if (buf.HasFixedPhysAddr())
                {
                    buf.SetFixedPhysAddr(buf.GetFixedPhysAddr() + size);
                }

                size += ALIGN_UP(buf.EstimateSize(GetBoundGpuDevice()) +
                        buf.GetHiddenAllocSize() +
                        buf.GetExtraAllocSize(),
                        bigPageSize);

                if (buf.GetLocation() == (Memory::Location)0xFFFFFFFF)
                    buf.SetLocation(TargetToLocation(mod->GetLocation()));

                if ((mod->GetGpuCacheMode() != Surface2D::GpuCacheDefault) &&
                        (buf.GetGpuCacheMode() == Surface2D::GpuCacheDefault))
                {
                    buf.SetGpuCacheMode(mod->GetGpuCacheMode());
                }

                if ((mod->GetP2PGpuCacheMode() != Surface2D::GpuCacheDefault) &&
                        (buf.GetP2PGpuCacheMode() == Surface2D::GpuCacheDefault))
                {
                    buf.SetP2PGpuCacheMode(mod->GetP2PGpuCacheMode());
                }

                buf.SetName(mod->GetName());

                // Add to the map of private MdiagSurfs (@@@ m_Textures is a bad name).
                m_Textures.push_back(make_pair(mod, buf));
            }
        }
    }

    ModuleIter modIt;
    for (modIt = m_Trace.ModBegin(); modIt != m_Trace.ModEnd(); ++modIt)
    {
        TraceModule * mod = *modIt;
        if (mod->GetVASpaceHandle() != hVASpace) continue;

        // Buffers created by the ALLOC_SURFACE trace command
        // always get a private buffer.
        if (mod->GetFileType() == GpuTrace::FT_ALLOC_SURFACE)
        {
            // Color and Z surfaces are owned by the surface manager,
            // so they shouldn't go in the m_Textures array.
            if (mod->IsColorOrZ())
            {
                continue;
            }

            MdiagSurf *surface = mod->GetParameterSurface();
            SurfaceFormat* formatHelper = mod->GetFormatHelper();

            if (formatHelper != 0)
            {
                surface->SetArrayPitch(formatHelper->GetSize() /
                        surface->GetArraySize());
            }
            else if (mod->GetSize() > 0)
            {
                surface->SetArrayPitch(mod->GetSize());
            }
            else
            {
                // The ArrayPitch needs to be non-zero so that this surface
                // will be allocated later.  However, the ComputeParams()
                // function hasn't been called yet, so the real size isn't
                // known.  This will guess the size and ComputeParams() will
                // fix it later.
                surface->SetArrayPitch(surface->EstimateSize(GetBoundGpuDevice()));
            }

            if (m_Trace.BufferLwlled(mod->GetName()))
            {
                switch((ZLwllEnum)params->ParamUnsigned("-zlwll_mode", ZLWLL_REQUIRED))
                {
                    case ZLWLL_REQUIRED:
                        surface->SetZLwllFlag(LWOS32_ATTR_ZLWLL_REQUIRED);
                        break;
                    case ZLWLL_ANY:
                        surface->SetZLwllFlag(LWOS32_ATTR_ZLWLL_ANY);
                        break;
                    case ZLWLL_NONE:
                        surface->SetZLwllFlag(LWOS32_ATTR_ZLWLL_NONE);
                        break;
                    case ZLWLL_CTXSW_SEPERATE_BUFFER:
                    case ZLWLL_CTXSW_NOCTXSW:
                        // These modes are handled elsewhere.
                        break;
                    default:
                        ErrPrintf("Unrecognized value for command-line argument -zlwll_mode.\n");
                        return RC::BAD_COMMAND_LINE_ARGUMENT;
                }
            }

            // Surface views are not allocated so they shouldn't be put
            // in the m_Textures array.
            if (!(*modIt)->IsSurfaceView())
            {
                m_Textures.push_back(make_pair(mod, *surface));

                mod->SaveTrace3DSurface(&(m_Textures.back().second));
            }
        }

        // Move surfaces whose default memory space is not video memory out into
        // a private MdiagSurf, unless they already have one.
        else if (((mod->GetLocation() != _DMA_TARGET_VIDEO) ||
                    mod->AllocIndividualBuffer()) &&
                find_if(m_Textures.begin(), m_Textures.end(), TraceModuleCmp(mod)) == m_Textures.end())
        {
            MdiagSurf buf(pChunks[mod->GetFileType()].m_Surface);
            buf.SetArrayPitch(mod->GetSize());
            Memory::Protect prot = (Memory::Protect)(mod->GetProtect() | buf.GetProtect());
            buf.SetProtect(prot);

            if (mod->HasAttrSet())
            {
                buf.ConfigFromAttr(mod->GetHeapAttr());
            }

            if (mod->HasAttr2Set())
            {
                buf.ConfigFromAttr2(mod->GetHeapAttr2());
            }

            if (mod->HasVprSet())
            {
                buf.SetVideoProtected(mod->GetVpr());
            }

            buf.SetType(mod->GetHeapType());

            if (buf.GetLocation() == (Memory::Location)0xFFFFFFFF)
                buf.SetLocation(TargetToLocation(mod->GetLocation()));
            if (!buf.HasFixedVirtAddr() && mod->HasAddressRange())
            {
                buf.SetVirtAddrRange(mod->GetAddressRange().first,
                    mod->GetAddressRange().second);
            }
            string name = mod->GetName();
            // Individual buffer should has its name set to the module's name.
            buf.SetName(name);

            m_Textures.push_back(make_pair(mod, buf));
        }

        if ((mod->GetGpuCacheMode() != Surface2D::GpuCacheDefault) &&
                (pChunks[mod->GetFileType()].m_Surface.GetGpuCacheMode() == Surface2D::GpuCacheDefault))
        {
            pChunks[mod->GetFileType()].m_Surface.SetGpuCacheMode(mod->GetGpuCacheMode());
        }

        if ((mod->GetP2PGpuCacheMode() != Surface2D::GpuCacheDefault) &&
                (pChunks[mod->GetFileType()].m_Surface.GetP2PGpuCacheMode() == Surface2D::GpuCacheDefault))
        {
            pChunks[mod->GetFileType()].m_Surface.SetP2PGpuCacheMode(mod->GetP2PGpuCacheMode());
        }

        if (mod->GetSharedByTegra())
        {
            Textures::iterator it = find_if(m_Textures.begin(), m_Textures.end(), TraceModuleCmp(mod));
            if (it != m_Textures.end())
            {
                MdiagSurf *surf = &it->second;
                if (surf->IsAllocated())
                {
                    ErrPrintf("trace_3d %s: Surface %s already allocated. Can't declare as CheetAh shared.\n",
                              __FUNCTION__, surf->GetName().c_str());
                    return RC::SOFTWARE_ERROR;
                }
                surf->SetVASpace(Surface2D::GPUVASpace);
                InfoPrintf("trace_3d %s: Surface %s declared as CheetAh shared.\n",
                           __FUNCTION__, surf->GetName().c_str());
            }
            else
            {
                ErrPrintf("trace_3d %s: Can't find MdiagSurf object for CheetAh shared surface %s.\n",
                          __FUNCTION__, mod->GetName().c_str());
                return RC::SOFTWARE_ERROR;
            }
        }
    }

    // Any surface that was left with a "default" memory space needs to be fixed up
    // to use video memory.
    for (int ft = GpuTrace::FT_FIRST_FILE_TYPE; ft < GpuTrace::FT_NUM_FILE_TYPE; ft++)
    {
        if (pChunks[ft].m_Surface.GetLocation() == (Memory::Location)0xFFFFFFFF)
            pChunks[ft].m_Surface.SetLocation(Memory::Fb);
    }

    if (params->ParamPresent("-pgm_alloc_high"))
    {
        // Force the memory allocation to be 16MB.  Waste a bunch of space at
        // the bottom of the surface.
        pChunks[GpuTrace::FT_SHADER_PROGRAM].m_LwrOffset = 1 << 24;
        pChunks[GpuTrace::FT_SHADER_PROGRAM].m_Surface.SetArrayPitch(1 << 24);
    }

    // Shader program surface must be a multiple of 256B, since the fetches are
    // in 256B chunks.
    pChunks[GpuTrace::FT_SHADER_PROGRAM].m_Surface.SetArrayPitch(
            (pChunks[GpuTrace::FT_SHADER_PROGRAM].m_Surface.GetArrayPitch() + 255) & ~255);
    pChunks[GpuTrace::FT_CONSTANT_BUFFER].m_Surface.SetArrayPitch(
            (pChunks[GpuTrace::FT_CONSTANT_BUFFER].m_Surface.GetArrayPitch() + 255) & ~255);
    // Shader program address has to be 256B aligned as well
    pChunks[GpuTrace::FT_SHADER_PROGRAM].m_Surface.SetAlignment( 256 );
    pChunks[GpuTrace::FT_SHADER_PROGRAM].m_Surface.SetVirtAlignment( 256 );

    // Shader program needs padding area due to instruction prefectching
    if (EngineClasses::IsGpuFamilyClassOrLater(
        TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_KEPLER))
    {
        // Kepler needs 1024B (bug 684593).
        m_Padding[GpuTrace::FT_SHADER_PROGRAM] = 1024;
    }
    else
    {
        // For Fermi, the shader program should by default have 256 bytes
        // of padding.  (See bug 538230)
        m_Padding[GpuTrace::FT_SHADER_PROGRAM] = 256;
    }

    if (params->ParamPresent("-stream_to_color"))
    {
        if (m_streams != 0 && pChunks[GpuTrace::FT_VERTEX_BUFFER].m_Surface.GetArrayPitch() == 0)
            pChunks[GpuTrace::FT_VERTEX_BUFFER].m_Surface.SetArrayPitch(1);
    }

    for (int ft = GpuTrace::FT_FIRST_FILE_TYPE;
            ft < GpuTrace::FT_NUM_FILE_TYPE;
            ++ft)
    {
        if (pChunks[ft].m_Surface.GetArrayPitch() > 0)
        {
            // Add possible padding from a commmand line argument.
            pChunks[ft].m_Surface.SetArrayPitch(pChunks[ft].m_Surface.GetArrayPitch() +
                    m_Padding[ft]);

            pBuffersToAlloc->emplace_back(&pChunks[ft].m_Surface,
                                         static_cast<GpuTrace::TraceFileType>(ft),
                                         nullptr /*indicate it is a chunk buffer*/);
        }
    }

    return rc;
}

RC Trace3DTest::InitIndividualBuffer(vector<AllocInfo>* pBuffersToAlloc)
{
    RC rc;

    // Some modules (i.e. textures) may need to be allocated separately
    for ( auto& tex : m_Textures)
    {
        if (tex.first->IsDynamic())
        {
            // Dynamic surfaces are allocated via trace-op.
        }
        else if (tex.second.GetArrayPitch() > 0)
        {
            pBuffersToAlloc->emplace_back(&tex.second, tex.first->GetFileType(), tex.first);
        }
    }

    return rc;
}

RC Trace3DTest::SetIndividualBufferAttr(MdiagSurf *surface)
{
    // For both static & dynamic allocated buffers, the function should be
    // called before the suface get allocated
    MASSERT(surface);
    RC rc = OK;
    if (surface->GetSkedReflected())
    {
        // Pass the compute object handle to surface2d
        // Reflected attribute is only for Kepler and later chips
        vector<TraceChannel*> compute_channels;
        GetComputeChannels(compute_channels);
        if (compute_channels.empty())
        {
            ErrPrintf("No compute channel was found\n");
            return RC::SOFTWARE_ERROR;
        }
        // Since SKED reflected buffer is shared for all compute channels,
        // so we just map to the first one.
        TraceChannel *ch = compute_channels[0];
        MASSERT(ch);
        TraceSubChannel *subch = ch->GetComputeSubChannel();
        MASSERT(subch->GetClass() >= KEPLER_COMPUTE_A);
        UINT32 objHandle = subch->GetSubCh()->object_handle();

        if (0 == objHandle)
        {
            ErrPrintf("Compute object not found, failed to set SKED_REFLECTED for the buffer\n");
            return RC::SOFTWARE_ERROR;
        }
        else
        {
            surface->SetMappingObj(objHandle);
            surface->SetGpuManagedChId(ch->GetCh()->ChannelNum());
            // Add surfaces to lwgpuchannel class, thus we can free
            // the SKED reflected buffers before object get freed
            ch->GetCh()->SetSkedRefBuf(surface);
        }
    }

    if (surface->IsHostReflectedSurf())
    {
        TraceChannel *ch = GetChannel(surface->GetSurf2D()->GetGpuManagedChName(), GpuTrace::GPU_MANAGED);
        MASSERT(ch);
        UINT32 objHandle = (UINT32)(ch->GetCh()->GetModsChannel()->GetHandle());

        if (0 == objHandle)
        {
            ErrPrintf("Invalid channel handle to map HOST_REFLECTED buffer\n");
            return RC::SOFTWARE_ERROR;
        }
        else
        {
            surface->SetMappingObj(objHandle);
            surface->SetGpuManagedChId(ch->GetCh()->ChannelNum());
            // Add surfaces to lwgpuchannel class, thus we can free
            // the HOST reflected buffers before object get freed
            ch->GetCh()->SetHostRefBuf(surface);
        }
    }

    return rc;
}

/// FailTest is intended for use by trace_3d plugins. Primarily, it sets the client (plugin)'s status as failed. If the
/// test has already completedly successfully at call time, then that status is overridden and the test is failed.
void Trace3DTest::FailTest(Test::TestStatus status)
{
    m_clientExited = true;;
    m_clientStatus = status ;

    if (status != Test::TEST_SUCCEEDED)
    {
        if (GetStatus() == Test::TEST_SUCCEEDED || GetStatus() == Test::TEST_INCOMPLETE)
            SetStatus(m_clientStatus);
    }
}

RC Trace3DTest::AllocateBuffer(vector<AllocInfo>* pBuffersToAlloc)
{
    RC rc;

    for (auto& info : *pBuffersToAlloc)
    {
        CHECK_RC(SetPteKind(*(info.m_Surface), m_PteKindName[info.m_FileType],
            GetBoundGpuDevice()));

        if (info.m_Module)
        {
            LwRm::Handle hVASpace = info.m_Module->GetVASpaceHandle();
            info.m_Surface->SetGpuVASpace(hVASpace);
        }

        if (params->ParamPresent("-shared_luma_surface") &&
            params->ParamPresent("-shared_chroma_surface"))
        {
            const char* lumaName = params->ParamStr("-shared_luma_surface");
            if (info.m_Surface->GetName().compare(lumaName) == 0)
            {
                info.m_Surface->SetDisplayable(true);
                InfoPrintf("%s: shared luma surface %s is displayable.\n", __FUNCTION__, lumaName);
            }

            const char* chromaName = params->ParamStr("-shared_chroma_surface");
            if (info.m_Surface->GetName().compare(chromaName) == 0)
            {
                info.m_Surface->SetDisplayable(true);
                InfoPrintf("%s: shared chroma surface %s is displayable.\n", __FUNCTION__, chromaName);
            }
        }

        if (Utl::HasInstance())
        {
            Utl::Instance()->AddTestSurface(this, info.m_Surface);
        }

        if (info.IsChunkBuffer())
        {
            rc = AllocateChunkBuffer(&info);
        }
        else
        {
            rc = AllocateIndividualBuffer(&info);
        }

        if (rc != OK)
        {
            m_BuffInfo.Print("after allocation error",GetBoundGpuDevice());
            return rc;
        }
    }

    return rc;
}

RC Trace3DTest::AllocateChunkBuffer(AllocInfo *info)
{
    RC rc;
    MdiagSurf *surface = info->m_Surface;

    if (info->m_Module && info->m_Module->IsShared())
    {
        ErrPrintf("%s: Not support shared surface at chunk memory module.\n");
        return RC::SOFTWARE_ERROR;
    }

    //check if the surface is a global surface
    const string surface_name =  surface->GetName();
    if (m_GlobalSharedSurfNames.find(surface_name) != m_GlobalSharedSurfNames.end())
    {
        //the shared surface shouldn't be downloaded
        if (info->m_Module)
            info->m_Module->GetCachedSurface()->SetWasDownloaded();

        string & global_name = m_GlobalSharedSurfNames[surface_name];
        MdiagSurf *global_surf = GetGpuResource()->GetSharedSurface(global_name.c_str());
        if (global_surf != NULL)
        {
            if (global_surf->GetGpuDev() != GetBoundGpuDevice())
            {
                ErrPrintf("global shared surfaces should be on the same gpu device.\n");
                return RC::SOFTWARE_ERROR;
            }
            *surface = *global_surf;
            surface->SetDmaBufferAlloc(true); // no need to call Surface3D::Free
            return OK;
        }
    }

    bool disabledLocationOverride = false;
    INT32 oldLocationOverride = Surface2D::NO_LOCATION_OVERRIDE;

    if (params->ParamPresent("-disable_location_override_for_trace"))
    {
        oldLocationOverride = Surface2D::GetLocationOverride();
        Surface2D::SetLocationOverride(Surface2D::NO_LOCATION_OVERRIDE);
        disabledLocationOverride = true;
    }

    const string chunkBufferType = GpuTrace::GetFileTypeData(info->m_FileType).Description;
    InfoPrintf("AllocateChunkBuffer: Need 0x%llx bytes of %s for %s, hVASpace %x\n",
               surface->GetArrayPitch(),
        surface->GetLoopBack() ? "loopback" :
        GetMemoryLocationName(surface->GetLocation()),
        chunkBufferType.c_str(),
        surface->GetGpuVASpace()
        );

    MDiagUtils::SetMemoryTag tag(GetBoundGpuDevice(), chunkBufferType.c_str());

    GetP2pSM()->GetP2pDeviceMapping(GetBoundGpuDevice(), info->m_FileType, surface);

    if (Utl::HasInstance())
    {
        Utl::Instance()->TriggerSurfaceAllocationEvent(surface, this);
    }

    rc = surface->Alloc(GetBoundGpuDevice(), m_pLwRm);

    // This is a hack to get around a problem with using explicit virtual
    // allocation to satisfy virtual address ranges.  (Bug 545822)
    // If an allocation fails due to an address range constraint,
    // try again with implicit virtual allocation.  The explicit virtual
    // allocation is more efficient, but it has some holes such that for
    // some older traces, only the implicit virtual allocation will work.
    if ((rc == RC::LWRM_INSUFFICIENT_RESOURCES) &&
        (surface->GetMemHandle() != 0) &&
        surface->HasVirtAddrRange() &&
        surface->GetGpuVirtAddrHintUseVirtAlloc())
    {
        surface->GetSurf2D()->SetGpuVirtAddrHintUseVirtAlloc(false);
        rc = surface->ReMapPhysMemory();
    }

    if (rc == OK)
    {
        PrintDmaBufferParams(*surface);
    }

    if (disabledLocationOverride)
    {
        Surface2D::SetLocationOverride(oldLocationOverride);
    }

    if (rc != OK)
    {
        ErrPrintf("mem region alloc failed for %s, needed %lld bytes of %s: %s\n",
                  GpuTrace::GetFileTypeData(info->m_FileType).Description,
                  surface->GetAllocSize(),
                  GetMemoryLocationName(surface->GetLocation()),
                  rc.Message());
    }

    if (m_GlobalSharedSurfNames.find(surface_name) != m_GlobalSharedSurfNames.end() )
    {
        const string &global_name = m_GlobalSharedSurfNames[surface_name];
        CHECK_RC(GetGpuResource()->AddSharedSurface(global_name.c_str(), surface));
    }
    return rc;
}

RC Trace3DTest::AllocateIndividualBuffer(AllocInfo *info)
{
    RC rc;
    MdiagSurf *surface = info->m_Surface;
    TraceModule *module = info->m_Module;
    const string chunkBufferType = GpuTrace::GetFileTypeData(info->m_FileType).Description;

    //check if the surface is a global surface
    const string surface_name =  surface->GetName();
    if (module && module->IsShared())
    {
        LWGpuResource * pGpuResource = GetGpuResource();
        SharedSurfaceController * pSurfController = pGpuResource->GetSharedSurfaceController();
        LwRm::Handle hVaSpace = surface->GetGpuVASpace();
        LwRm * pLwRm = surface->GetLwRmPtr();
        MdiagSurf * sharedSurf = pSurfController->GetSharedSurf(surface->GetName(), hVaSpace, pLwRm);
        if (sharedSurf)
        {
            // Prevent ALLOC_SURFACE and ALLOC_PHYSICAL has same name and same lwrm client
            if ((sharedSurf->HasMap() && surface->HasMap()) ||
                    (sharedSurf->HasPhysical() && surface->HasPhysical()))
            {
                // ToDo:: Whether need to check the gpudevice is same
                *surface = *sharedSurf;
                module->GetCachedSurface()->SetWasDownloaded();
                surface->SetDmaBufferAlloc(true); // no need to call Surface2D::Free
                return OK;
            }
        }
    }
    else
    {
        if (m_GlobalSharedSurfNames.find(surface_name) != m_GlobalSharedSurfNames.end() )
        {
            //the shared surface shouldn't be downloaded
            if (module)
                module->GetCachedSurface()->SetWasDownloaded();

            string & global_name = m_GlobalSharedSurfNames[surface_name];
            MdiagSurf *global_surf = GetGpuResource()->GetSharedSurface(global_name.c_str());
            if (global_surf != NULL)
            {
                if (global_surf->GetGpuDev() != GetBoundGpuDevice())
                {
                    ErrPrintf("global shared surfaces should be on the same gpu device.\n");
                    return RC::SOFTWARE_ERROR;
                }
                *surface = *global_surf;
                surface->SetDmaBufferAlloc(true); // no need to call Surface3D::Free
                return OK;
            }
        }
    }
    bool disabledLocationOverride = false;
    INT32 oldLocationOverride = Surface2D::NO_LOCATION_OVERRIDE;

    if (params->ParamPresent("-disable_location_override_for_trace"))
    {
        oldLocationOverride = Surface2D::GetLocationOverride();
        Surface2D::SetLocationOverride(Surface2D::NO_LOCATION_OVERRIDE);
        disabledLocationOverride = true;
    }

    if (surface->IsVirtualOnly())
    {
        InfoPrintf("AllocateIndividualBuffer: Need 0x%llx bytes of virtual space for virtual-only %s\n",
            surface->GetArrayPitch(), info->m_Module->GetName().c_str());
    }
    else
    {
        InfoPrintf("AllocateIndividualBuffer: Need 0x%llx bytes of %s for %s\n",
            surface->GetArrayPitch(),
            surface->GetLoopBack() ? "loopback" : GetMemoryLocationName(surface->GetLocation()),
            info->m_Module->GetName().c_str());
    }

    MDiagUtils::SetMemoryTag tag(GetBoundGpuDevice(), chunkBufferType.c_str());

    // matches AAmode for the texture buffers
    if ((info->m_FileType == GpuTrace::FT_TEXTURE) &&
        (params->ParamPresent("-pte_kind_on_aamode_tex") > 0))
    {
        const char* aamode = params->ParamNStr("-pte_kind_on_aamode_tex", 0);
        const char* ptekind = params->ParamNStr("-pte_kind_on_aamode_tex", 1);
        Surface2D::AAMode aaval;

        if (!GetAAModeFromName(aamode, &aaval))
        {
            return RC::SOFTWARE_ERROR;
        }

        if (aaval == surface->GetAAMode())
        {
            CHECK_RC(SetPteKind(*surface, ptekind, GetBoundGpuDevice()));
        }
    }

    CHECK_RC(SetIndividualBufferAttr(surface));

    if (Utl::HasInstance())
    {
        Utl::Instance()->TriggerSurfaceAllocationEvent(surface, this);
    }

    rc = module->AllocateSurface(surface, GetBoundGpuDevice());
    if (module->IsShared())
    {
        surface->SetDmaBufferAlloc(true); // no need to call Surface3D::Free
    }

    if ((rc == OK) && module->GetLoopback() &&
        (surface->GetLocation() == Memory::Fb))
    {
        vector<UINT32> peerIDs = module->GetPeerIDs();
        for (auto id : peerIDs)
        {
            rc = surface->MapLoopback(id);
        }

    }

    if (rc == OK)
    {
        PrintDmaBufferParams(*surface);
    }

    if (disabledLocationOverride)
    {
        Surface2D::SetLocationOverride(oldLocationOverride);
    }

    // video memory is full, trying system memory ...
    bool tex2sys = params->ParamPresent("-spill_tex2sys") > 0;

    if (tex2sys && rc == RC::LWRM_INSUFFICIENT_RESOURCES &&
        surface->GetLocation() == Memory::Fb &&
        (module->GetProtect() == Memory::Readable ||
            params->ParamPresent("-force_tex_spill") > 0))
    {
        int sys_type = params->ParamUnsigned("-spill_tex2sys");
        MASSERT(sys_type == 1 || sys_type == 2);

        WarnPrintf("Could not fit buffer %s to video memory, trying sytem memory...\n",
                   module->GetName().c_str());

        module->SetLocation(sys_type == 1 ? _DMA_TARGET_COHERENT : _DMA_TARGET_NONCOHERENT);
        surface->SetLocation(sys_type == 1 ? Memory::Coherent : Memory::NonCoherent);
        surface->SetCompressed(false);
        surface->SetPteKind(-1);

        MDiagUtils::SetMemoryTag tag(GetBoundGpuDevice(), chunkBufferType.c_str());

        rc = module->AllocateSurface(surface, GetBoundGpuDevice());

        if (rc == OK)
        {
            PrintDmaBufferParams(*surface);
        }
    }

    // we really can not allocate this buffer, bailing out ...
    if (rc != OK)
    {
        ErrPrintf("mem region alloc failed for %s(%s), needed %lld bytes of %s: %s\n",
                  chunkBufferType.c_str(),
                  module->GetName().c_str(),
                  surface->GetAllocSize(),
                  GetMemoryLocationName(surface->GetLocation()),
                  rc.Message());

        return rc;
    }

    const UINT64 offset = surface->GetCtxDmaOffsetGpu()
        + surface->GetExtraAllocSize();

    // If the buffer has a fixed virtual address constraint, make sure
    // that it was allocated at the requested address.
    if (surface->HasFixedVirtAddr())
    {
        if (offset != surface->GetFixedVirtAddr())
        {
            ErrPrintf("%s allocation failed: requested address is 0x%llx, obtained address is 0x%llx\n",
                module->GetName().c_str(), surface->GetFixedVirtAddr(), offset);

            return RC::SOFTWARE_ERROR;
        }
    }

    // If there is a virtual address range constraint, verify that the surface
    // was allocated within the specified virtual address range.
    else if (surface->HasVirtAddrRange() > 0)
    {
        CHECK_RC(CheckAddressRange(module->GetName().c_str(),
                offset, module->GetSize(),
                surface->GetVirtAddrRangeMin(),
                surface->GetVirtAddrRangeMax()));
    }

#ifdef LW_VERIF_FEATURES
    // once bug 223036 is fixed we should do printout reargdless of the texture location
    // if (module->GetProtect() == Memory::ReadWrite)
    if (surface->GetLocation() == Memory::Fb &&
        module->GetProtect() == Memory::ReadWrite &&
        surface->GetExternalPhysMem() == 0)
    {
        LW0041_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS par = {0};
        par.memOffset = 0;

        RC rc = m_pLwRm->Control(
            surface->GetMemHandle(), LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR,
            &par, sizeof(par));

        if (rc != OK)
        {
            ErrPrintf("%s\n", rc.Message());
            MASSERT(0);
        }

        InfoPrintf("RTT surface %s is %s compressed\n", module->GetName().c_str(), par.comprFormat != 0 ? "" : "not");
    }
#endif

    if (!surface->IsPhysicalOnly() && !surface->IsMapOnly())
    {
        // All separately-allocated modules should be in the
        // same CTX_DMA, i.e. paging memory model.

        //CtxDma is per vaspace?
        Chunk *pChunks = m_Chunks[surface->GetGpuVASpace()];
        if (!pChunks[info->m_FileType].m_SurfaceCtxDma)
        {
            if (pChunks[info->m_FileType].m_Surface.GetCtxDmaHandle())
            {
                pChunks[info->m_FileType].m_SurfaceCtxDma =
                    pChunks[info->m_FileType].m_Surface.GetCtxDmaHandle();
            }
            else
            {
                pChunks[info->m_FileType].m_SurfaceCtxDma = surface->GetCtxDmaHandle();
            }
        }

        /*
        if (pChunks[info->m_FileType].m_SurfaceCtxDma != surface->GetCtxDmaHandle())
        {
            ErrPrintf("All textures in vaspace %d are expected to be in the same context dma (0x%08x), but %s is in 0x%08x ctxdma\n",
                surface->GetGpuVASpace(),
                pChunks[info->m_FileType].m_SurfaceCtxDma,
                module->GetName().c_str(),
                surface->GetCtxDmaHandle());

            return RC::SOFTWARE_ERROR;
        }
        */
    }

    if (m_GlobalSharedSurfNames.find(surface_name) != m_GlobalSharedSurfNames.end() )
    {
        const string &global_name = m_GlobalSharedSurfNames[surface_name];
        CHECK_RC(GetGpuResource()->AddSharedSurface(global_name.c_str(), surface));
    }

    return rc;
}

RC Trace3DTest::MapLoopbackChunkBuffer()
{
    RC rc;

    for (ModuleIter modIt = m_Trace.ModBegin(); modIt != m_Trace.ModEnd(); ++modIt)
    {
        TraceModule * mod = *modIt;
        MdiagSurf *pSurf =  GetSurface(mod->GetFileType(), mod->GetVASpaceHandle());
        if ((pSurf->GetArrayPitch() > 0) &&
            (pSurf->GetLocation() == Memory::Fb) &&
            mod->GetLoopback())
        {
            vector<UINT32> peerIDs = mod->GetPeerIDs();
            for (auto id : peerIDs)
            {
                rc = pSurf->MapLoopback(id);
            }
        }
        if (rc != OK)
        {
            ErrPrintf("loopback mapping failure\n");
            return rc;
        }
    }

    return rc;
}

// allocates memory on behalf of a GpuTrace -
//  returns base of buffer if successful, MemoryResource::NO_ADDRESS if not
//  fills in handle and offset if the trace provides non-null pointers
bool Trace3DTest::TraceAllocate(TraceModule* mod,
                                GpuTrace::TraceFileType filetype,
                                UINT32 bytes_needed,
                                MdiagSurf **ppDmaBuf,
                                UINT64 *pOffset,
                                bool  *alloc_separate)
{
    Chunk *pChunks = m_Chunks[mod->GetVASpaceHandle()];
    if (alloc_separate)
    {
        *alloc_separate = false;
    }

    if (find_if(m_Textures.begin(), m_Textures.end(), TraceModuleCmp(mod)) != m_Textures.end())
    {
        // This module has a private MdiagSurf, and will use all of it.
        *ppDmaBuf = &find_if(m_Textures.begin(), m_Textures.end(), TraceModuleCmp(mod))->second;
        *pOffset = 0;
        if (alloc_separate)
        {
            *alloc_separate = true;
        }
    }
    else if (filetype == GpuTrace::FT_PUSHBUFFER)
    {
        RC rc;

        // Each pushbuffer gets a private DmaBuffer.
        MdiagSurf buf(pChunks[GpuTrace::FT_PUSHBUFFER].m_Surface);

        // Allocate the pushbuffer
        buf.SetArrayPitch((mod->GetSize() + 255) & ~255);
        buf.SetLocation(Memory::Coherent);
        buf.SetProtect(Memory::Readable);

        // The push buffer should not be GPU cached by default.
        if (buf.GetGpuCacheMode() == Surface2D::GpuCacheDefault)
        {
            buf.SetGpuCacheMode(Surface2D::GpuCacheOff);
        }

        if (mod->HasVprSet())
        {
            buf.SetVideoProtected(mod->GetVpr());
        }

        buf.SetGpuVASpace(mod->GetVASpaceHandle());
        // Add to the map of private DmaBuffers (@@@ m_Textures is a bad name).
        m_Textures.push_back(make_pair(mod, buf));

        InfoPrintf("TraceAllocate: Need 0x%llx bytes of %s for %s (Pushbuffer)\n",
                   buf.GetArrayPitch(),
                   GetMemoryLocationName(buf.GetLocation()),
                   mod->GetName().c_str());

        MDiagUtils::SetMemoryTag tag(GetBoundGpuDevice(), GpuTrace::GetFileTypeData(mod->GetFileType()).Description);
        rc = m_Textures.back().second.Alloc(GetBoundGpuDevice(), m_pLwRm);

        // we really can not allocate this buffer, bailing out ...
        if (rc != OK)
        {
            ErrPrintf("mem region alloc failed for %s(%s), needed %lld bytes of %s: %s\n",
                      GpuTrace::GetFileTypeData(mod->GetFileType()).Description,
                      mod->GetName().c_str(),
                      m_Textures.back().second.GetAllocSize(),
                      GetMemoryLocationName(buf.GetLocation()), rc.Message());
            return false;
        }

        // Pushbuffers have private MdiagSurfs and use all of them.
        *ppDmaBuf = &(m_Textures.back().second);
        *pOffset = 0;
        if (alloc_separate)
        {
            *alloc_separate = true;
        }
    }
    else
    {
        // This module shares a MdiagSurf.  Assign it part of the shared buf.
        // First, align the current offset up to the necessary alignment boundary
        // for this file type.
        UINT64 Align = m_Trace.GetFileTypeAlign(filetype);
        const UINT64 offset = pChunks[filetype].m_Surface.GetCtxDmaOffsetGpu()
            + pChunks[filetype].m_Surface.GetExtraAllocSize();

        if (filetype == GpuTrace::FT_SHADER_PROGRAM &&
            params->ParamPresent("-pgm_alloc_high") > 0)
        {
            if (pChunks[filetype].m_LwrOffset < bytes_needed)
            {
                ErrPrintf("TraceAllocate: not enough memory allocated\n");
                return false;
            }
            else
            {
                // Allocate from high addr to low addr
                pChunks[filetype].m_LwrOffset =
                    ((pChunks[filetype].m_LwrOffset - offset - bytes_needed) & ~(Align-1))
                    + offset;

                *ppDmaBuf = &pChunks[filetype].m_Surface;
                *pOffset = pChunks[filetype].m_LwrOffset;
            }
        }
        else
        {
            pChunks[filetype].m_LwrOffset =
                ((pChunks[filetype].m_LwrOffset + offset + Align-1) & ~(Align-1))
                - offset;

            *ppDmaBuf = &pChunks[filetype].m_Surface;
            *pOffset = pChunks[filetype].m_LwrOffset;

            // See bug 138284.
            if (filetype == GpuTrace::FT_SHADER_THREAD_STACK)
                return true;

            pChunks[filetype].m_LwrOffset += bytes_needed;
            if (pChunks[filetype].m_LwrOffset > pChunks[filetype].m_Surface.GetArrayPitch())
            {
                ErrPrintf("TraceAllocate: not enough memory allocated\n");
                return false;
            }
        }

        if (mod->HasAddressRange())
        {
            UINT64 lwrrentAddress = offset + *pOffset;

            // check if all buffers are allocated where requested
            if (OK != CheckAddressRange(mod->GetName(), lwrrentAddress,
                    bytes_needed, mod->GetAddressRange().first,
                    mod->GetAddressRange().second))
            {
                return false;
            }
        }
    }
    return true;
}

RC Trace3DTest::CheckAddressRange(const string &name, UINT64 lwrrentAddress,
    UINT64 size, UINT64 addressMin, UINT64 addressMax) const
{
    // Check to see if the entire buffer could even fit within the
    // requested address range.  The address range is inclusive,
    // so one must be subtracted from the size.  (E.g., an address range
    // of 0x100 to 0x1FF would be legal for a 256-byte buffer.)
    if (size > addressMax - addressMin + 1)
    {
        ErrPrintf("%s allocation failed: buffer size 0x%llx exceeds size of requested address range 0x%llx to 0x%llx\n",
                  name.c_str(), size, addressMin, addressMax);

        return RC::SOFTWARE_ERROR;
    }

    // Based on the address the buffer was actually assigned to,
    // Check to see if the entire buffer resides within the
    // requested address range and therefore satisfies the constraint.
    else if ((lwrrentAddress < addressMin) ||
        (lwrrentAddress + size > addressMax + 1))
    {
        ErrPrintf("%s allocation failed: allocated address range 0x%llx to 0x%llx is not within requested address range 0x%llx to 0x%llx\n",
                  name.c_str(), lwrrentAddress, lwrrentAddress + size, addressMin, addressMax);

        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

// Determine if the specified surface needs peer mapping
//
bool Trace3DTest::NeedPeerMapping(MdiagSurf* Surf) const
{
    // First check if the specified surface is one of the chunk allocation
    // buffers.  If so, use the peer mapping setting of the buffer type.
    map<LwRm::Handle, Chunk*>::const_iterator it = m_Chunks.find(Surf->GetGpuVASpace());
    MASSERT(it != m_Chunks.end());
    const Chunk *pChunks = it->second;
    for (int ft = GpuTrace::FT_FIRST_FILE_TYPE; ft < GpuTrace::FT_NUM_FILE_TYPE; ft++)
    {
        if (&pChunks[ft].m_Surface == Surf)
        {
            return pChunks[ft].m_NeedPeerMapping;
        }
    }

    // Next check if the surface is one of the surfaces that is allocated
    // individually (i.e., not chunk allocation).  Though these surfaces
    // are allocated individually, the command-line argument to specify
    // peer mapping is specified by type.
    //
    // However, ignore any surfaces that were created via the ALLOC_SURFACE
    // trace header command because the command-line argument to specify
    // peer mapping is different for those surfaces.
    for (auto& tex: m_Textures)
    {
        if ((&(tex.second) == Surf) &&
            (tex.first->GetFileType() != GpuTrace::FT_ALLOC_SURFACE))
        {
            return pChunks[tex.first->GetFileType()].m_NeedPeerMapping;
        }
    }

    // If this point of the function is reached, the surface must be one
    // that was created via the ALLOC_SURFACE trace header command.
    // The peer mapping setting for these surfaces is stored directly
    // on the MdiagSurf object.
    return Surf->GetNeedsPeerMapping();
}

bool Trace3DTest::NeedDmaCheck() const
{
    bool dmaCheck = params->ParamPresent("-dma_check") > 0 ||
                    m_Trace.DmaCheckCeRequested();

    return dmaCheck;
}

bool Trace3DTest::RunAfterExelwteMethods()
{
    for (auto ch : m_TraceCpuManagedChannels)
    {
        AtomChannelWrapper *pAtomWrap =
            ch->GetCh()->GetModsChannel()->GetAtomChannelWrapper();
        if ((pAtomWrap != NULL) && (OK != pAtomWrap->CancelAtom()))
            return false;
    }

    for (auto ch : m_TraceCpuManagedChannels)
    {
        auto sub_end = ch->SubChEnd();
        for (auto isubch = ch->SubChBegin(); isubch != sub_end; ++isubch)
        {
            // Looks like none-gr subchannel is not happy with this method
            if ( (*isubch)->GrSubChannel() )
            {
                LWGpuSubChannel* pSubch = (*isubch)->GetSubCh();
                if (OK != pSubch->MethodWriteRC(LW9097_PIPE_NOP, 0))
                    return false;
            }
        }
    }
    return true;
}

LwRm::Handle Trace3DTest::GetSurfaceCtxDma(int filetype, LwRm::Handle hVASpace) const
{
    map<LwRm::Handle, Chunk*>::const_iterator it = m_Chunks.find(hVASpace);
    MASSERT(it != m_Chunks.end());
    const Chunk *pChunks = it->second;
    LwRm::Handle ctxDmaHandle = pChunks[filetype].m_Surface.GetCtxDmaHandle();
    if (ctxDmaHandle == 0)
    {
        ctxDmaHandle = pChunks[filetype].m_SurfaceCtxDma;
    }
    return ctxDmaHandle;
}

UINT32 Trace3DTest::GetMethodResetValue(UINT32 HwClass, UINT32 Method) const
{
    if (EngineClasses::IsClassType("Gr", HwClass))
    {
        if (HwClass >= MAXWELL_B)
        {
            return GetMaxwellBMethodResetValue(Method);
        }
        else
        {
            return GetFermiMethodResetValue(Method);
        }
    }
    else
    {
        ErrPrintf("Don't know how to reset method 0x%x for class 0x%x\n", Method, HwClass);
        MASSERT(0);
    }
    return 0;
}

UINT32 Trace3DTest::ClassStr2Class(const char *ClassStr) const
{
    // We support not just Tesla, but a bunch of other classes, too
    if (!strcmp(ClassStr, "lw01_null"))
        return LWGpuClasses::GPU_LW_NULL_CLASS;
    if (!strcmp(ClassStr, "lw50_deferred_api_class"))
        return LW50_DEFERRED_API_CLASS;
    if (!strcmp(ClassStr, "lw86b6_video_compositor"))
        return LW86B6_VIDEO_COMPOSITOR;
    if (!strcmp(ClassStr, "gf106_dma_decompress"))
        return GF106_DMA_DECOMPRESS;
    if (!strcmp(ClassStr, "lw95a1_tsec"))
        return LW95A1_TSEC;
    if (!strcmp(ClassStr, "fermi_twod_a"))
        return FERMI_TWOD_A;
    if (!strcmp(ClassStr, "lw04_software_test"))
        return LW04_SOFTWARE_TEST;
    if (!strcmp(ClassStr, "kepler_inline_to_memory_b"))
        return KEPLER_INLINE_TO_MEMORY_B;

    UINT32 classNum = 0;
    if (EngineClasses::GetClassNum(ClassStr, &classNum))
    {
        return classNum;
    }
    return 0;
}

RC Trace3DTest::SetupClasses()
{
    RC rc;

    // Setup gpu managed channel
    for (auto ch : m_TraceGpuManagedChannels)
    {
        LWGpuChannel* lwgpuCh = ch->GetCh();
        if (lwgpuCh)
        {
            CHECK_RC(lwgpuCh->ScheduleChannel(true));
        }
    }

    // Setup cpu managed channel
    for (auto ch : m_TraceCpuManagedChannels)
    {
        CHECK_RC(ch->SetMethodCount());
        rc = ch->SetupClass();
        if (OK != rc)
        {
            ErrPrintf("Error detected for channel '%s': %s\n", ch->GetName().c_str(), rc.Message());
            if (rc == ch->GetCh()->CheckForErrorsRC() && params->ParamPresent("-ignore_channel_errors_for_init_methods"))
            {
                InfoPrintf("Channel error is ignored due to option -ignore_channel_errors_for_init_methods\n");
                rc.Clear();
            }
            else
            {
                return rc;
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
TraceChannel * Trace3DTest::GetChannel(const string &chname, ChannelManagedMode mode)
{
    if (GpuTrace::CPU_MANAGED & mode)
    {
        for (auto ch : m_TraceCpuManagedChannels)
        {
            if (0 == ch->GetName().compare(chname))
                return ch;
        }
    }

    if (GpuTrace::GPU_MANAGED & mode)
    {
        for (auto ch : m_TraceGpuManagedChannels)
        {
            if (0 == ch->GetName().compare(chname))
                return ch;
        }
    }

    return 0;
}

//------------------------------------------------------------------------------
TraceChannel * Trace3DTest::GetDefaultChannel()
{
    if (m_TraceCpuManagedChannels.empty())
        return 0;

    TraceChannel *grChannel = GetGrChannel();

    return grChannel ? grChannel : m_TraceCpuManagedChannels.front();
}

TraceChannel * Trace3DTest::GetDefaultChannelByVAS(LwRm::Handle hVASpace) const
{
    TraceChannel *result = 0;
    for (auto ch : m_TraceCpuManagedChannels)
    {
        if (ch->GetVASpaceHandle() == hVASpace)
        {
            if (result ==0)
            {
                 result = ch;
            }
            if (ch->GetGrSubChannel())
            {
                result = ch;
                break;
            }
        }
    }
    return result;
}

//------------------------------------------------------------------------------
TraceChannel * Trace3DTest::GetLwrrentChannel(ChannelManagedMode mode)
{
    switch (mode)
    {
    case GpuTrace::CPU_MANAGED:
        if (!m_TraceCpuManagedChannels.empty())
            return m_TraceCpuManagedChannels.back();
        break;
    case GpuTrace::GPU_MANAGED:
        if (!m_TraceGpuManagedChannels.empty())
            return m_TraceGpuManagedChannels.back();
        break;
    case GpuTrace::CPU_GPU_MANAGED:
        return m_LwrrentChannel;
    default:
        return 0;
    }
    return 0;
}

//------------------------------------------------------------------------------
TraceChannel * Trace3DTest::GetGrChannel()
{
    TraceChannels::iterator iter = find_if(m_TraceCpuManagedChannels.begin(),
                                           m_TraceCpuManagedChannels.end(),
                                           [](TraceChannel* pChannel) -> bool
                                           { return pChannel->GetGrSubChannel(); });

    return iter == m_TraceCpuManagedChannels.end() ? 0 : (*iter);
}

//------------------------------------------------------------------------------
void Trace3DTest::GetComputeChannels(vector<TraceChannel*>& compute_channels)
{
    compute_channels.clear();
    for (auto ch : m_TraceCpuManagedChannels)
    {
        if (ch->GetComputeSubChannel())
        {
            compute_channels.push_back(ch);
        }
    }
}

//------------------------------------------------------------------------------
RC Trace3DTest::AddChannel(TraceChannel * traceChannel)
{
    ChannelManagedMode mode = traceChannel->IsGpuManagedChannel()?
        GpuTrace::GPU_MANAGED : GpuTrace::CPU_MANAGED;

    if (0 != GetChannel(traceChannel->GetName(), mode))
    {
        // duplicate channel name
        return RC::SOFTWARE_ERROR;
    }

    if (GpuTrace::CPU_MANAGED == mode)
    {
        m_TraceCpuManagedChannels.push_back(traceChannel);
        m_LwrrentChannel = m_TraceCpuManagedChannels.back();

        if (Utl::HasInstance())
        {
            vector<UINT32> subChCEInsts;
            if (GetBoundGpuDevice()->GetSubdevice(0)->HasFeature(Device::GPUSUB_HOST_REQUIRES_ENGINE_ID))
            {
                for (auto iSubCh = traceChannel->SubChBegin(); iSubCh != traceChannel->SubChEnd(); ++iSubCh)
                {
                    if ((*iSubCh)->CopySubChannel())
                    {
                        subChCEInsts.push_back((*iSubCh)->GetCeType());
                    }
                }
            }
            Utl::Instance()->AddTestChannel(this, traceChannel->GetCh(),
                traceChannel->GetName(), &subChCEInsts);
        }
    }

    if (GpuTrace::GPU_MANAGED == mode)
    {
        m_TraceGpuManagedChannels.push_back(traceChannel);
        m_LwrrentChannel = m_TraceGpuManagedChannels.back();
    }

    m_ChannelAllocInfo.AddChannel(traceChannel->GetCh());

    return OK;
}

// Remove freed channel from trace channel list of Trace3DTest class
RC Trace3DTest::RemoveChannel(TraceChannel* traceChannel)
{
    ChannelManagedMode mode = traceChannel->IsGpuManagedChannel()?
        GpuTrace::GPU_MANAGED : GpuTrace::CPU_MANAGED;

    TraceChannels* pChannelList = 0;
    if (GpuTrace::CPU_MANAGED == mode)
    {
        pChannelList = &m_TraceCpuManagedChannels;
    }
    else if (GpuTrace::GPU_MANAGED == mode)
    {
        pChannelList = &m_TraceGpuManagedChannels;
    }

    if (pChannelList)
    {
        TraceChannels::iterator it;
        it = find(pChannelList->begin(), pChannelList->end(), traceChannel);
        if (it != pChannelList->end())
        {
            pChannelList->erase(it);
        }
    }

    return OK;
}

TraceTsg * Trace3DTest::GetTraceTsg(const string& tsgName)
{
    for (auto tsg : m_TraceTsgs)
    {
        if (tsg->GetName() == tsgName)
            return tsg;
    }
    return 0;
}

RC Trace3DTest::AddTraceTsg(TraceTsg* pTraceTsg)
{
    if (0 != GetTraceTsg(pTraceTsg->GetName()))
    {
        return RC::SOFTWARE_ERROR;
    }

    m_TraceTsgs.push_back(pTraceTsg);
    return OK;
}

//------------------------------------------------------------------------------
RC Trace3DTest::RegisterWithPolicyManager()
{
    PolicyManager *pPM = PolicyManager::Instance();
    RC rc;

    MASSERT(pPM->IsInitialized());

    // Add the current test to policy manager
    CHECK_RC(pPM->AddTest(new PmTest_Trace3D(pPM, this)));
    PmTest *pTest = pPM->GetTestFromUniquePtr(this);

    if (!params->ParamPresent("-no_policy_registration"))
    {
        // There are 2 kinds of Trace3d surfaces that should be referred to:
        // 1) a "filename" (referred in the header file with the file name
        // and the fileType), e.g.,
        //
        // FILE chunk_0.fp PixelProgram SWAP_SIZE 4
        //
        // The name can be whatever and the fileType are defined as enumerant
        // (in GpuTrace::TraceFileType).
        //
        // To retrieve a filename I can direct the iterators to access m_Modules
        // (GpuTrace in gputrace.h).
        // Once I have *TraceModule, I can get the FileType with GetFileType() and
        // the name with GetName() and or access to the MdiagSurf.
        //
        // 2) a "surface" (eg, COLOR0...7, Z, CLIPID) which are implictly
        // allocated.
        //
        // I guess they are allocated in Trace3DTest (trace_3d.h)
        //
        // Surface *surfZ;
        // Surface *surfC[MAX_RENDER_TARGETS];
        //
        // I can access this data to retrieve GetTypeName(),
        // GetType(), ...

        // 1) filename
        //
        GpuTrace* p_trace = GetTrace();

        if (p_trace)
        {
            for (ModuleIter modIt = p_trace->ModBegin();
                modIt != p_trace->ModEnd(); ++modIt)
            {
                TraceModule * trm = *modIt;

                // FT_SHADER_THREAD_STACK and FT_SHADER_THREAD_MEMORY will always
                // overlap with modules of the same type if multiples are declared
                // Policy Manager cannot handle overlapping subsurfaces so do not
                // add subsurfaces of those types
                if (trm &&
                    trm->GetDmaBuffer() &&
                    !trm->IsColorOrZ() &&
                    (trm->GetFileType() != GpuTrace::FT_SHADER_THREAD_STACK) &&
                    (trm->GetFileType() != GpuTrace::FT_SHADER_THREAD_MEMORY))
                {
                    MdiagSurf *pSurf = trm->GetDmaBufferNonConst();

                    if (pSurf->HasMap() || pSurf->IsAtsMapped())
                    {
                        string TypeName =
                            GpuTrace::GetFileTypeData(trm->GetFileType()).Description;
                        UINT64 Offset =
                            pSurf->GetExtraAllocSize() + trm->GetOffsetWithinDmaBuf();
                        CHECK_RC(pPM->AddSubsurface(pTest, pSurf,
                                Offset, trm->GetSize(),
                                trm->GetName(), TypeName));
                    }
                }
            }
        }

        // 2) surface
        //
        if (surfZ && (surfZ->HasMap() || surfZ->IsAtsMapped()))
        {
            CHECK_RC(pPM->AddSurface(pTest, surfZ, true));
        }

        for (size_t i = 0; i < MAX_RENDER_TARGETS; ++i)
        {
            if (surfC[i] && (surfC[i]->HasMap() || surfC[i]->IsAtsMapped()))
            {
                CHECK_RC(pPM->AddSurface(pTest, surfC[i], true));
            }
        }

        // 3) Clip ID surface
        //
        IGpuSurfaceMgr *surfMgr = GetSurfaceMgr();
        if (surfMgr && surfMgr->GetNeedClipID())
        {
            MdiagSurf *cIDSurface = surfMgr->GetClipIDSurface();
            if (cIDSurface)
            {
                CHECK_RC(pPM->AddSurface(pTest, cIDSurface, true));
            }
        }

        // 4) ChannelHandles
        //
        for (auto ch : m_TraceCpuManagedChannels)
        {
            CHECK_RC(RegisterChannelToPolicyManager(pPM, pTest, ch));
        }

        // 5) CE engines
        //
        // lots of place assume only one subdev
        vector<UINT32> supportedCEs;
        GpuSubdevice *pSubDev = GetBoundGpuDevice()->GetSubdevice(0);
        CHECK_RC(m_pBoundGpu->GetSupportedCeNum(pSubDev, &supportedCEs, m_pLwRm));

        vector<UINT32> grCEEngineType;
        CHECK_RC(m_pBoundGpu->GetGrCopyEngineType(grCEEngineType, m_pLwRm));

        for (size_t i = 0; i < supportedCEs.size(); ++i)
        {
            UINT32 ceEngineType =
                CEObjCreateParam::GetEngineTypeByCeType(supportedCEs[i]);

            CHECK_RC(pPM->AddAvailableCEEngineType(ceEngineType,
                    find(grCEEngineType.begin(), grCEEngineType.end(), ceEngineType) != grCEEngineType.end()));
        }

        // 6) VaSpace
        LWGpuResource::VaSpaceManager * pVasManager = m_pBoundGpu->GetVaSpaceManager(m_pLwRm);
        for (Trace3DResourceContainer<VaSpace>::iterator it = pVasManager->begin();
            it != pVasManager->end(); ++it)
        {
            // just registe the vaspace which is in this trace3d or it is global
            if (it->first == pTest->GetTestId() ||
                it->first == LWGpuResource::TEST_ID_GLOBAL)
            {
                if (pPM->GetVaSpace(it->second->GetHandle(), pTest) != NULL)
                {
                    WarnPrintf("Already added vaspace 0x%x to PolicyManager. Discarding duplicate.\n",
                               it->second->GetHandle());
                }
                else
                {
                    const bool isGlobalVas = (it->first == LWGpuResource::TEST_ID_GLOBAL);
                    PmVaSpace * pVaSpace = new PmVaSpace_Trace3D(pPM,
                            isGlobalVas ? NULL : pTest,
                            GetBoundGpuDevice(),
                            it->second,
                            isGlobalVas, // shared vas or not
                            m_pLwRm);
                    CHECK_RC(pPM->AddVaSpace(pVaSpace));
                }
            }
        }
        // 7) SmcEngine
        if (m_pSmcEngine)
        {
            PmSmcEngine* pPmSmcEngine = new PmSmcEngine_Trace3D(pPM, nullptr, m_pSmcEngine);
            if (pPM->IsSmcEngineAdded(pPmSmcEngine))
            {
                delete pPmSmcEngine;
                pPmSmcEngine = nullptr;
            }
            else
                CHECK_RC(pPM->AddSmcEngine(pPmSmcEngine));
        }
        // 8) LWGpuResource
        CHECK_RC(pPM->AddLWGpuResource(GetGpuResource()));

        // 9) FLA Surfaces
        auto flaExportSurfaces = LWGpuResource::GetFlaExportSurfaces();
        for (const auto pFlaExportSurface : flaExportSurfaces)
        {
            // FLA export surfaces are specified through mdiag GPU args which
            // aren't tied to a specific test, so register these with the test
            // set to nullptr.
            CHECK_RC(pPM->AddSurface(nullptr, pFlaExportSurface, true));
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC Trace3DTest::RegisterChannelToPolicyManager
(
 PolicyManager* pPM,
 PmTest* pTest,
 TraceChannel* pTraceChannel
 )
{
    RC rc;

    // This need to update with multi-subchannel support
    PmChannel *pChannel =
        new PmChannel_LwGpu(pPM, pTest,
                            GetBoundGpuDevice(),
                            pTraceChannel->GetName(),
                            pTraceChannel->GetCh(),
                            pTraceChannel->GetTraceTsg() ?
                            pTraceChannel->GetTraceTsg()->GetLWGpuTsg() : NULL,
                            pTraceChannel->GetLwRmPtr());

    // Note that if a channel with the same handle has already been
    // added to policy manager, then the channel that was just created
    // will be deleted after the call to AddChannel.  However, its
    // handle will still be valid (since even if it was deleted a
    // channel with the same handle has already been added to policy
    // manager).  Save the channel handle for use in adding subchannel
    // names to policy manager
    LwRm::Handle hCh = pChannel->GetHandle();

    CHECK_RC(pPM->AddChannel(pChannel));

    // Get a pointer to the channel that was actually added, which may
    // not be the one created above if a channel with its handle was
    // already present in policy manager
    pChannel = pPM->GetChannel(hCh);
    MASSERT(pChannel);

    for (TraceSubChannelList::iterator itSubCh = pTraceChannel->SubChBegin();
         itSubCh != pTraceChannel->SubChEnd(); itSubCh++)
    {
        CHECK_RC(pChannel->SetSubchannelName((*itSubCh)->GetSubChNum(),
                                             (*itSubCh)->GetName()));
        CHECK_RC(pChannel->SetSubchannelClass((*itSubCh)->GetSubChNum(),
                                              (*itSubCh)->GetClass()));
    }

    return rc;
}

//------------------------------------------------------------------------------
RC Trace3DTest::DisplaySurface()
{
    RC         rc;
    Display *  pDisplay = GetBoundGpuDevice()->GetDisplay();

    UINT32 surf_idx = 0;
    bool cmd_present = false;
    for (UINT32 surf_idx_cmd = 0; surf_idx_cmd < MAX_RENDER_TARGETS; surf_idx_cmd++)
    {
        if (params->ParamPresent(DisplayArgNames[surf_idx_cmd]))
        {
            if (!cmd_present)
            {
                cmd_present = true;
                surf_idx = surf_idx_cmd;
            }
            else
            {
                ErrPrintf("trace_3d: Display of more then one surface is not supported\n");
                return RC::SOFTWARE_ERROR;
            }
        }
    }

    if (!cmd_present)
        return OK;

    if ( (surfC[surf_idx] == 0) ||
        (!GetSurfaceMgr()->GetValid(surfC[surf_idx])) )
    {
        ErrPrintf("Trace3DTest::DisplaySurface(): surface is not valid!\n");
        return RC::SOFTWARE_ERROR;
    }

    if (surfC[surf_idx]->GetAAMode() != Surface2D::AA_1)
    {
        ErrPrintf("Trace3DTest::DisplaySurface(): only AAMODE_1X1 is supported\n");
        // KJD: The only other mode lwrrently supported in Display is "SUPER_SAMPLE_X4_AA",
        // but I don't know which is the match for it in GetAAMode
        return RC::SOFTWARE_ERROR;
    }

    UINT32 DisplayWidth = surfC[surf_idx]->GetWidth() /
        surfC[surf_idx]->GetAAWidthScale();

    UINT32 DisplayHeight = surfC[surf_idx]->GetHeight() /
        surfC[surf_idx]->GetAAHeightScale();

    UINT32 DisplayDepth = surfC[surf_idx]->GetBytesPerPixel();

    UINT32 Connectors = 0;
    CHECK_RC(pDisplay->GetConnectors(&Connectors));
    if (Connectors & 1)
        CHECK_RC(pDisplay->Select(1));

    pDisplay->SetUpdateMode(Display::ManualUpdate);
    CHECK_RC(pDisplay->SetDefaultTimeoutMs(LwU64_LO32(GetTimeoutMs())));
    if (pDisplay->SetMode(DisplayWidth, DisplayHeight, DisplayDepth*8, 60) != OK)
    {
        // In case the resolution is not supported try to create custom timings:
        WarnPrintf("Trace3DTest::DisplaySurface(): Trying custom timings - may not work on real displays\n");

        // The custom timings will create a minimal raster that the requested
        // resolution can be fit into. width/height + "2"s are the minimal sizes
        // supported in hardware.
        // [Not fully true (sometimes they can be even smaller), but close enough]
        EvoRasterSettings ers(DisplayWidth+2,  0, DisplayWidth,  0,
                              DisplayHeight+2, 0, DisplayHeight, 0, 1, 0,
                              355752000, // Pixel clock is a value that just happens to work,
                                         // it can be anything else that the hardware supports.
                              0, 0, false);

        CHECK_RC(pDisplay->SetTimings(&ers));
        CHECK_RC(pDisplay->SetMode(DisplayWidth, DisplayHeight, DisplayDepth*8, 60));
    }

    GpuUtility::DisplayImageDescription desc;

    desc.Height         = DisplayHeight;
    desc.Width          = DisplayWidth;
    desc.AASamples      = 1;
    desc.Layout         = surfC[surf_idx]->GetLayout();
    desc.Pitch          = surfC[surf_idx]->GetPitch();
    desc.LogBlockHeight = surfC[surf_idx]->GetLogBlockHeight();
    desc.ColorFormat    = surfC[surf_idx]->GetColorFormat();

    UINT32 LogBlockWidth = surfC[surf_idx]->GetLogBlockWidth();
    UINT32 AllocWidth    = desc.Pitch / DisplayDepth;
    UINT32 GobWidth      = AllocWidth * DisplayDepth / 64;
    UINT32 BlockWidth    = GobWidth >> LogBlockWidth;

    desc.NumBlocksWidth = BlockWidth;

    m_DisplayMemHandle = surfC[surf_idx]->GetMemHandle();

    // Create an ISO ctx dma for this surface:
    CHECK_RC(m_pLwRm->AllocContextDma(
        &m_DisplayCtxDma,
        DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_ONLY) | DRF_DEF(OS03, _FLAGS, _CACHE_SNOOP, _DISABLE),
        m_DisplayMemHandle,
        0,
        surfC[surf_idx]->GetSize()-1));

    desc.CtxDMAHandle   = m_DisplayCtxDma;
    desc.Offset         = 0;

    m_DisplayActive = true; // Mark it active now so that cleanup function
                            // will free the new ctx dma

    LwRm::Handle hBaseChanHeadA;
    for (UINT32 DisplayIdx = 0; DisplayIdx < 24; DisplayIdx++)
    {
        UINT32 DisplayId = 1 << DisplayIdx;
        if (pDisplay->Selected() & DisplayId)
        {
            if (pDisplay->GetEvoDisplay()->GetBaseChannelHandle(DisplayId, &hBaseChanHeadA) == OK)
            {
                CHECK_RC(m_pLwRm->BindContextDma(hBaseChanHeadA, desc.CtxDMAHandle));
            }
        }
    }

    CHECK_RC(pDisplay->SetImage(&desc));
    CHECK_RC(pDisplay->SendUpdate
    (
        true,       // Core
        0xFFFFFFFF, // All bases
        0xFFFFFFFF, // All lwrsors
        0xFFFFFFFF, // All overlays
        0xFFFFFFFF, // All overlaysIMM
        true,       // Interlocked
        false       // Don't wait for notifier
    ));

    return OK;
}

//------------------------------------------------------------------------------
RC Trace3DTest::StopDisplay()
{
    if (!m_DisplayActive)
        return OK;

    Display *pDisplay = GetBoundGpuDevice()->GetDisplay();

    pDisplay->SetImage((GpuUtility::DisplayImageDescription *)0);
    pDisplay->SendUpdate
    (
        true,       // Core
        0xFFFFFFFF, // All bases
        0xFFFFFFFF, // All lwrsors
        0xFFFFFFFF, // All overlays
        0xFFFFFFFF, // All overlaysIMM
        true,       // Interlocked
        false       // Don't wait for notifier
    );

    m_pLwRm->UnmapMemoryDma(m_DisplayCtxDma, m_DisplayMemHandle, 0, 0, GetBoundGpuDevice());
    m_pLwRm->Free(m_DisplayCtxDma);

    m_DisplayActive = false;

    return OK;
}

MdiagSurf* Trace3DTest::LocateTextureBufByOffset( UINT64 offset )
{
    for (auto& tex : m_Textures)
    {
        const UINT64 texOffset = tex.second.GetCtxDmaOffsetGpu()
            + tex.second.GetExtraAllocSize();
        if ( (texOffset <= offset) &&
            (texOffset + tex.second.GetSize() > offset) )
        {
            return &(tex.second);
        }
    }
    return 0;
}

/* virtual */ LWGpuResource * Trace3DTest::GetGpuResource() const
{
    return m_pBoundGpu;
}

GpuDevice * Trace3DTest::GetBoundGpuDevice()
{
    return GetGpuResource()->GetGpuDevice();
}

RC Trace3DTest::GetBoundResources()
{
    m_pBoundGpu = nullptr;

    if ((m_GpuInst != Gpu::UNSPECIFIED_DEV) && (GetGpuResourcesByInst() != OK))
    {
        InfoPrintf("Unable to acquire bound resources.\n");
        return RC::DEVICE_NOT_FOUND;
    }

    // Try to acquire a GPU. If we fail, abort right away. If we pass, deacquire.
    // Try to acquire any GPU, we try just memfmt since it is the most basic class
    // IMPORTANT note: This is necessary because Setup is called for each test
    // before the test even runs (thus n^2 times). We don't want to proceed further
    // if we can't setup a GPU at all.  If a specific GPU was specified, then it has
    // already been acquired, no need to do this check here.

    if ((m_GpuInst == Gpu::UNSPECIFIED_DEV) &&
            !LWGpuResource::FindFirstResource())
    {
        InfoPrintf("A GPU with required capability isn't available.\n");
        return RC::DEVICE_NOT_FOUND;
    }

    // Get gpu resource according to class type specified in trace
    if (GetGpuResource() == 0 && GetGpuResourcesByClasses() != OK)
    {
        ErrPrintf("trace_3d: failed to acquire gpu resource.\n");
        return RC::DEVICE_NOT_FOUND;
    }

    return OK;
}

RC Trace3DTest::GetGpuResourcesByInst()
{
    m_pBoundGpu = LWGpuResource::GetGpuByDeviceInstance(m_GpuInst);

    if (m_pBoundGpu == NULL)
        return RC::DEVICE_NOT_FOUND;

    InfoPrintf("Running test on GPU Device %d\n", m_GpuInst);

    return m_pBoundGpu->AddActiveT3DTest(this);
}

RC Trace3DTest::GetGpuResourcesByClasses()
{
    if (m_pBoundGpu)
    {
        return OK;
    }

    // classes needed in trace
    set<UINT32> classes;
    for (auto ch : m_TraceCpuManagedChannels)
    {
        for (auto isubch = ch->SubChBegin(); isubch != ch->SubChEnd(); ++isubch)
        {
            classes.insert((*isubch)->GetClass());
        }
    }

    for (auto ch : m_TraceGpuManagedChannels)
    {
        for (auto isubch = ch->SubChBegin(); isubch != ch->SubChEnd(); ++isubch)
        {
            classes.insert((*isubch)->GetClass());
        }
    }

    UINT32* classIds = 0;
    UINT32 numClasses = classes.size();
    if (numClasses > 0)
    {
        UINT32 idx = 0;
        classIds = new UINT32[numClasses];
        for (auto it = classes.begin(); it != classes.end(); it++, idx++)
        {
            classIds[idx] = *it;
        }
    }

    //
    // acquire gpu resource
    m_pBoundGpu = LWGpuResource::GetGpuByClassSupported(
                    numClasses,
                    classIds);

    if (classIds)
    {
        delete[] classIds;
    }
    numClasses = 0;

    if (!m_pBoundGpu)
    {
        return RC::LWRM_INSUFFICIENT_RESOURCES;
    }

    return m_pBoundGpu->AddActiveT3DTest(this);
}

RC Trace3DTest::FlushMethodsAllChannels()
{
    RC rc;

    // Flush all channels.
    for (auto ch : m_TraceCpuManagedChannels)
    {
        rc = ch->GetCh()->MethodFlushRC();
        if (OK != rc)
        {
            ErrPrintf("Trace3DTest::%s: flush failed on channel %s.\n",
                      __FUNCTION__, ch->GetName().c_str());
            return rc;
        }
    }
    return OK;
}

RC Trace3DTest::WaitForDMAPushAllChannels()
{
    RC rc;

    for (auto ch : m_TraceCpuManagedChannels)
    {
        DebugPrintf(MSGID(WaitX), "Trace3DTest::%s wait for get==put on on channel %s.\n",
                    __FUNCTION__, ch->GetName().c_str());
        CHECK_RC(ch->GetCh()->WaitForDmaPushRC());
    }
    return OK;
}

RC Trace3DTest::WriteWfiMethodToAllChannels()
{
    RC rc;

    for (auto ch : m_TraceCpuManagedChannels)
    {
        DebugPrintf(MSGID(WaitX), "Trace3DTest::%s send down WFI method to channel %s "
                    "if chip is pascal or later and -wfi_poll is enbaled.\n",
                    __FUNCTION__, ch->GetName().c_str());

        for (auto iSubCh = ch->SubChBegin(); iSubCh != ch->SubChEnd(); ++iSubCh)
        {
            if ((*iSubCh)->GrSubChannel())
            {
                CHECK_RC((*iSubCh)->GetSubCh()->MethodWriteRC(LWC097_WAIT_FOR_IDLE, 0));
            }
            else if ((*iSubCh)->ComputeSubChannel())
            {
                CHECK_RC((*iSubCh)->GetSubCh()->MethodWriteRC(LWC0C0_WAIT_FOR_IDLE, 0));
            }
            else
            {
                CHECK_RC(ch->GetCh()->MethodWriteRC(0, LWC06F_WFI, DRF_DEF(C06F, _WFI, _SCOPE, _ALL)));
            }
        }
    }

    return OK;
}

//
// Call LWGpuChannel::WaitForIdleRC() to wait channels in trace3d test one by one.
//
// WaitForIdleRmIdleAllChannels calls RM API to wait for idle which will lock RM
// interface before returning from the API. Thus, other mods threads can't get
// the RM service due to RM API lock. This might lead to deadlock because channel
// might not reach idle state without the input of other threads.
// e.g. uvm test + plugin.
//
// Unlike WaitForIdleRmIdleAllChannels, WaitForIdleRC() will use host semaphore
// to wait channel idle by default other than RM API.
//
RC Trace3DTest::WaitForIdleHostSemaphoreIdleAllChannels()
{
    RC rc;

    // Flush all channels before waiting for idle one by one
    CHECK_RC(FlushMethodsAllChannels());

    // Wfi on channels one by one
    for (auto ch : m_TraceCpuManagedChannels)
    {
        DebugPrintf(MSGID(WaitX), "Trace3DTest::%s wait for idle on channel %s.\n",
                    __FUNCTION__, ch->GetName().c_str());

        // skip waiting others if one channel hits failure
        CHECK_RC(ch->GetCh()->WaitForIdleRC());
    }

    return rc;
}

RC Trace3DTest::WaitForIdleRmIdleAllChannels()
{
    UINT32 maxIdleRetries = 0;
    UINT32 idleRetries;
    vector<UINT32> phChannels(m_TraceCpuManagedChannels.size());
    vector<UINT32> phClients(m_TraceCpuManagedChannels.size());
    vector<UINT32> phDevices(m_TraceCpuManagedChannels.size());

    for (auto ch : m_TraceCpuManagedChannels)
    {
        idleRetries = ch->GetCh()->GetModsChannel()->GetIdleChannelsRetries();
        if (idleRetries > maxIdleRetries)
        {
            maxIdleRetries = idleRetries;
        }
    }

    RC rc;
    idleRetries = 0;

    do
    {
        // Recreate the phChannels et al arrays each iteration, in
        // case one of the channels became disabled
        //
        phChannels.clear();
        phDevices.clear();
        phClients.clear();
        for (auto ch : m_TraceCpuManagedChannels)
        {
            if (ch->GetCh()->GetEnabled())
            {
                phChannels.push_back(ch->GetCh()->GetModsChannel()->GetHandle());
                phDevices.push_back(m_pLwRm->GetDeviceHandle(GetBoundGpuDevice()));
                phClients.push_back(m_pLwRm->GetClientHandle());
            }
        }

        rc.Clear();
        rc = m_pLwRm->IdleChannels(
            phChannels[0],
            phChannels.size(),
            &phClients[0],
            &phDevices[0],
            &phChannels[0],
            DRF_DEF(OS30, _FLAGS, _BEHAVIOR, _SLEEP) |
            DRF_DEF(OS30, _FLAGS, _IDLE, _CACHE1) |
            DRF_DEF(OS30, _FLAGS, _IDLE, _ALL_ENGINES) |
            DRF_DEF(OS30, _FLAGS, _CHANNEL, _LIST),
            LwU64_LO32(GetTimeoutMs()) * 1000,
            GetBoundGpuDevice()->GetDeviceInst());                  // Colwert to us for RM

        //
        // Yield() once even channel is idle
        // to let interrupt polling thread(-poll_interrupts)
        // have a chance to detect/report interrupt triggered
        Tasker::Yield();

        // If any of the channels were disabled, then make sure that
        // we retry at least once per channel
        //
        if (maxIdleRetries < phChannels.size())
        {
            for (auto ch : m_TraceCpuManagedChannels)
            {
                if (!ch->GetCh()->GetEnabled())
                {
                    maxIdleRetries = phChannels.size();
                    break;
                }
            }
        }

    } while ((++idleRetries < maxIdleRetries) &&
             (rc == RC::LWRM_MORE_PROCESSING_REQUIRED));

    return rc;
}

RC Trace3DTest::WaitForIdleRmIdleOneChannel(UINT32 chNum)
{
    TraceChannels::iterator iCh;

    for (iCh = m_TraceCpuManagedChannels.begin(); iCh != m_TraceCpuManagedChannels.end(); ++iCh)
    {
        if ((*iCh)->GetCh()->ChannelNum() == chNum)
        {
            break;
        }
    }

    if (iCh == m_TraceCpuManagedChannels.end())
    {
        return OK;
    }

    if (!(*iCh)->GetCh()->GetEnabled())
    {
        return OK;
    }

    return (*iCh)->GetCh()->WaitForIdleRC();
}

UINT32 Trace3DTest::GetMethodCountOneChannel(UINT32 chNum)
{
    TraceChannels::iterator iCh;

    for (iCh = m_TraceCpuManagedChannels.begin(); iCh != m_TraceCpuManagedChannels.end(); ++iCh)
    {
        if ((*iCh)->GetCh()->ChannelNum() == chNum)
        {
            break;
        }
    }
    if (iCh == m_TraceCpuManagedChannels.end())
    {
        return 0;
    }
    else
    {
        return (*iCh)->GetCh()->GetMethodCount();
    }
}

UINT32 Trace3DTest::GetMethodWriteCountOneChannel(UINT32 chNum)
{
    TraceChannels::iterator iCh;

    for (iCh = m_TraceCpuManagedChannels.begin(); iCh != m_TraceCpuManagedChannels.end(); ++iCh)
    {
        if ((*iCh)->GetCh()->ChannelNum() == chNum)
        {
            break;
        }
    }
    if (iCh == m_TraceCpuManagedChannels.end())
    {
        return 0;
    }
    else
    {
        return (*iCh)->GetCh()->GetMethodWriteCount();
    }
}

// Used by Trace3DTest::WaitForNotifiersAllChannels()
struct WaitForNotifiersAllChannelsPollArgs
{
    Trace3DTest::TraceChannels *pChannels;
    set<TraceChannel*> DoneChannels;
    string testName;
    RC rc;
};

// Used by Trace3DTest::WaitForNotifiersAllChannels() to poll for test
// completion.  This function returns true when either all channels
// are done, or when CheckForErrorsRC() returns non-OK on any channel.
//
static bool WaitForNotifiersAllChannelsPollFunc(void *pArgs)
{
    WaitForNotifiersAllChannelsPollArgs *pPollArgs;
    Trace3DTest::TraceChannels *pChannels;
    bool AllNotifiersDone;
    RC rc;

    pPollArgs = (WaitForNotifiersAllChannelsPollArgs*)pArgs;
    pChannels = pPollArgs->pChannels;
    AllNotifiersDone = true;
    ChiplibOpScope newScope(pPollArgs->testName + " " + __FUNCTION__,
                            NON_IRQ, ChiplibOpScope::SCOPE_POLL, NULL);

    for (auto ch : *pChannels)
    {
        TraceChannel *pTraceChannel = ch;

        if (pTraceChannel->UseHostSemaphore())
        {
            CHECK_RC_CLEANUP(pTraceChannel->GetCh()->WaitForIdleRC());
            pPollArgs->DoneChannels.insert(pTraceChannel);
            DebugPrintf(MSGID(WaitX), "TraceChannel %s -%s, done.\n",
                        pTraceChannel->GetName().c_str(),
                        WfiMethodToString(pTraceChannel->GetWfiMethod()));
        }
        else
        {

            // Check for error on the channel, even if the channel is done
            // (some tests reset the channel).
            //
            CHECK_RC_CLEANUP(pTraceChannel->GetCh()->CheckForErrorsRC());

            // Check all undone channels for completion
            //
            if (!pPollArgs->DoneChannels.count(pTraceChannel))
            {
                if (pTraceChannel->PollNotifiersDone() ||
                    !pTraceChannel->GetCh()->GetEnabled())
                {
                    pPollArgs->DoneChannels.insert(pTraceChannel);
                    DebugPrintf(MSGID(WaitX), "TraceChannel %s -%s, done.\n",
                                pTraceChannel->GetName().c_str(),
                                WfiMethodToString(pTraceChannel->GetWfiMethod()));
                }
                else
                {
                    AllNotifiersDone = false;
                }
            }
        }
    }

Cleanup:
    if (pPollArgs->rc == OK)
    {
        pPollArgs->rc = rc;
    }
    return (AllNotifiersDone || (pPollArgs->rc != OK));
}

// Wait until the test is done in WFI_INTR and WFI_NOTIFY and WFI_HOST mode.
//
// While this method waits, it continually calls CheckForErrorsRC() on
// all channels so that we can respond to errors when they happen.
//
RC Trace3DTest::WaitForNotifiersAllChannels()
{
    WaitForNotifiersAllChannelsPollArgs PollArgs = { 0 };
    RC rc;

    PollArgs.pChannels = &m_TraceCpuManagedChannels;
    PollArgs.rc = OK;
    PollArgs.testName = string(GetTestName());
    rc = POLLWRAP(WaitForNotifiersAllChannelsPollFunc,
                  &PollArgs,
                  LwU64_LO32(GetTimeoutMs()));

    InfoPrintf("Trace3DTest %s polled on WaitForNotifiersAllChannelsPollFunc and returned %d.\n",
                __FUNCTION__, UINT32(rc));

    if (rc == RC::TIMEOUT_ERROR)
    {
        for (auto ch : m_TraceCpuManagedChannels)
        {
            TraceChannel *pTraceChannel = ch;
            if (!PollArgs.DoneChannels.count(pTraceChannel))
            {
                DebugPrintf(MSGID(WaitX), "TraceChannel %s -%s, timed out.\n",
                            pTraceChannel->GetName().c_str(),
                            WfiMethodToString(pTraceChannel->GetWfiMethod()));
            }
        }
    }

    if (PollArgs.rc != OK)
    {
        rc = PollArgs.rc;
    }

    //
    // Yield() once even channel is idle
    // to let interrupt polling thread(-poll_interrupts)
    // have a chance to detect/report interrupt triggered
    Tasker::Yield();

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Find a test in the test list by name.
//!
//! \param testName : Name of the test to find
//!
//! \return pointer to the test if the test was found, NULL if not found
Trace3DTest *Trace3DTest::FindTest(string testName)
{
    if (s_Trace3DName2Test.count(testName))
    {
        return s_Trace3DName2Test[testName];
    }

    return NULL;
}

//------------------------------------------------------------------------------
//! \brief Find a test in the test list by id.
//!
//! \param testId : Id of the test to find
//!
//! \return pointer to the test if the test was found, NULL if not found
Trace3DTest *Trace3DTest::FindTest(UINT32 testId)
{
    if (s_Trace3DId2Test.count(testId))
    {
        return s_Trace3DId2Test[testId];
    }

    return NULL;
}

//------------------------------------------------------------------------------
//! \brief Get conlwrrent run test number
//!
//!
//! \return number of conlwrrent running trace3d_test
UINT32 Trace3DTest::ConlwrrentRunTestNum()
{
    MASSERT(!s_Trace3DId2Test.empty());

    const Trace3DTest* test = s_Trace3DId2Test.begin()->second;
    if (!test->m_IsCtxSwitchTest)
    {
        return 1;
    }
    else
    {
        return static_cast<UINT32>(s_Trace3DId2Test.size());
    }
}

//------------------------------------------------------------------------------
//! \brief Add a surface to the list of surfaces that need to be peer mapped by
//!        this test.  The surface name can either be a valid FILE specified in
//!        the .hdr file, or it can be "Z", "COLOR[0-7]", or "CLIPID".
//!
//! \param pPeerDevice : Device that the requested surface needs to be peer'd
//!                      onto
//! \param surfaceName : Name of the surface to be peer'd
//!
void Trace3DTest::AddPeerMappedSurface(GpuDevice *pPeerDevice, string surfaceName)
{
    if (m_PeerMappedSurfaces.count(surfaceName) == 0)
    {
        set<GpuDevice *> newSet;
        m_PeerMappedSurfaces[surfaceName] = newSet;
    }
    m_PeerMappedSurfaces[surfaceName].insert(pPeerDevice);
}

//------------------------------------------------------------------------------
//! \brief Returns whether this test is a peer test.  A test is considered to be
//!        a peer test if any of its surfaces are used by another test, or it
//!        uses surfaces from another test.
//!
//! \return true if the test is a peer test, false otherwise
//!
bool Trace3DTest::IsPeer()
{
    // If another test is using surfaces within this test, then this test is
    // a peer test of that test
    if (m_PeerMappedSurfaces.size() != 0)
        return true;

    // If this test uses surfaces from another test, then this test is a peer
    // of that test
    return (m_Trace.ModBegin(false, true) != m_Trace.ModEnd(false, true));
}

//------------------------------------------------------------------------------
//! \brief send a control call to RM setting up the zlwll context switch mode
//!        after we read the mods command line options and determine if we want
//!        to context witch the zlwll surface
//!
//! \return OK if successful, not OK otherwise
//!
RC Trace3DTest::SetupCtxswZlwll(TraceChannel* channel)
{
    RC rc = OK;

    // Never setup ctxsw zlwll for amodel
    if ( Platform::GetSimulationMode() == Platform::Amodel )
        return rc;

    // Nothing to do for a non graphic channel
    if (channel->GetGrSubChannel() == 0)
    {
        return rc;
    }

    ZLwllEnum zlwllMode = (ZLwllEnum)params->ParamUnsigned("-zlwll_mode", ZLWLL_REQUIRED);
    switch(zlwllMode)
    {
    case ZLWLL_CTXSW_SEPERATE_BUFFER:
    case ZLWLL_CTXSW_NOCTXSW:
        {
            LwRm::Handle hSubdevice = m_pLwRm->GetSubdeviceHandle(GetGpuResource()->GetGpuSubdevice());
            LwRm::Handle hClient = m_pLwRm->GetClientHandle();
            LW2080_CTRL_GR_CTXSW_ZLWLL_MODE_PARAMS grCtxswZlwllModeParams = {0};
            UINT32 zlwllChid;
            UINT32 zlwllShareChid;
            UINT32 zlwllChH;
            UINT32 zlwllShareChH;

            zlwllChH = zlwllShareChH = channel->GetCh()->GetModsChannel()->GetHandle();
            zlwllChid = channel->GetCh()->ChannelNum();
            zlwllShareChid = params->ParamUnsigned("-zlwll_ctxsw_share_ch", zlwllChid);

            // lookup the channel handle for the shared channel
            TraceChannels::iterator iCh;
            for (iCh = m_TraceCpuManagedChannels.begin(); iCh != m_TraceCpuManagedChannels.end(); ++iCh)
            {
                if ( zlwllShareChid == (*iCh)->GetCh()->ChannelNum() )
                {
                    zlwllShareChH = (*iCh)->GetCh()->GetModsChannel()->GetHandle();
                    break;
                }
            }
            if (iCh == m_TraceCpuManagedChannels.end())
            {
                ErrPrintf("Error can't find the channel specified by -zlwll_ctxsw_share_ch 0x%x\n", zlwllShareChid);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }

            grCtxswZlwllModeParams.hChannel = zlwllChH;
            grCtxswZlwllModeParams.hShareClient = hClient;
            grCtxswZlwllModeParams.hShareChannel = zlwllShareChH;

            if (zlwllMode == ZLWLL_CTXSW_SEPERATE_BUFFER)
                grCtxswZlwllModeParams.zlwllMode = LW2080_CTRL_CTXSW_ZLWLL_MODE_SEPARATE_BUFFER;
            else
                grCtxswZlwllModeParams.zlwllMode = LW2080_CTRL_CTXSW_ZLWLL_MODE_NO_CTXSW;

            rc = m_pLwRm->Control(hSubdevice,
                LW2080_CTRL_CMD_GR_CTXSW_ZLWLL_MODE,
                (void*)&grCtxswZlwllModeParams,
                sizeof(grCtxswZlwllModeParams));

            if (rc != OK)
            {
                ErrPrintf("Error setting zlwll ctxsw mode in RM: %s\n", rc.Message());
                return rc;
            }
        }

        if (channel->GetTraceTsg() && channel->GetTraceTsg()->GetLWGpuTsg())
        {
            channel->GetTraceTsg()->GetLWGpuTsg()->SetZlwllModeSet();
        }

        break;
    default:
        break;
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief send a control call to RM setting up the pm context switch mode
//!        after we read the mods command line options and determine if we want
//!        to context switch the pm registers
//!
//! \return OK if successful, not OK otherwise
//!
RC Trace3DTest::SetupCtxswPm(TraceChannel* channel)
{
    RC rc = OK;

    int pm_ctxsw = params->ParamPresent("-pm_ctxsw");

    if (pm_ctxsw)
    {
        CHECK_RC(channel->GetCh()->SetupCtxswPm());
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief send a control call to RM setting up the SMPC context switch mode
//!        after we read the mods command line options and determine if we want
//!        to context switch the SMPC registers
//!
//! \return OK if successful, not OK otherwise
//!
RC Trace3DTest::SetupCtxswSmpc(TraceChannel* channel)
{
    RC rc = OK;

    if (params->ParamPresent("-smpc_ctxsw_mode"))
    {
        CHECK_RC(channel->GetCh()->SetupCtxswSmpc(
                static_cast<LWGpuChannel::SmpcCtxswMode>(
                    params->ParamUnsigned("-smpc_ctxsw_mode",
                        LWGpuChannel::SMPC_CTXSW_MODE_NO_CTXSW))));
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief send a control call to RM setting up the preemption context switch mode
//!        after we read the mods command line options and determine if we want
//!        to enable either gr or compute preemption
//!
//! \return OK if successful, not OK otherwise
//!
RC Trace3DTest::SetupCtxswPreemption(TraceChannel* channel)
{
    RC rc = OK;

    // Never setup ctxsw preemption for amodel
    if ( Platform::GetSimulationMode() == Platform::Amodel )
        return rc;

    // Nothing to do for a non graphics/compute channel
    if ((channel->GetGrSubChannel() == 0) && (channel->GetComputeSubChannel() == 0))
    {
        return rc;
    }

    GRPreemptionEnum grPreemptMode = (GRPreemptionEnum)params->ParamUnsigned("-gfx_preemption_mode", GR_PREEMPTION_WFI);
    ComputePreemptionEnum compPreemptMode = (ComputePreemptionEnum)params->ParamUnsigned("-compute_preemption_mode", COMPUTE_PREEMPTION_WFI);

    // if both modes are being set to the default, no sense sending control call to RM
    if ((grPreemptMode != GR_PREEMPTION_WFI) || (compPreemptMode != COMPUTE_PREEMPTION_WFI))
    {
        LwRm::Handle hSubdevice = m_pLwRm->GetSubdeviceHandle(GetGpuResource()->GetGpuSubdevice());
        LW2080_CTRL_GR_SET_CTXSW_PREEMPTION_MODE_PARAMS grSetCtxswPreemptionModeParams = {0};
        UINT32 preemptChH;
        TraceTsg *tsg = channel->GetTraceTsg();

        if (tsg != 0)
        {
            preemptChH = tsg->GetLWGpuTsg()->GetHandle();
        }
        else
        {
            preemptChH = channel->GetCh()->GetModsChannel()->GetHandle();
        }

        grSetCtxswPreemptionModeParams.hChannel = preemptChH;

        grSetCtxswPreemptionModeParams.flags = 0;

        switch (grPreemptMode)
        {
        case GR_PREEMPTION_WFI:
            grSetCtxswPreemptionModeParams.gfxpPreemptMode = LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_GFX_WFI;
            break;
        case GR_PREEMPTION_GFXP:
            grSetCtxswPreemptionModeParams.flags |= (LW2080_CTRL_GR_SET_CTXSW_PREEMPTION_MODE_FLAGS_GFXP_SET << 1);
            grSetCtxswPreemptionModeParams.gfxpPreemptMode = LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_GFX_GFXP;
            break;
        }

        switch (compPreemptMode)
        {
        case COMPUTE_PREEMPTION_WFI:
            grSetCtxswPreemptionModeParams.cilpPreemptMode = LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_WFI;
            break;
        case COMPUTE_PREEMPTION_CTA:
            grSetCtxswPreemptionModeParams.flags |= (LW2080_CTRL_GR_SET_CTXSW_PREEMPTION_MODE_FLAGS_CILP_SET);
            grSetCtxswPreemptionModeParams.cilpPreemptMode = LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_CTA;
            break;
        case COMPUTE_PREEMPTION_CILP:
            grSetCtxswPreemptionModeParams.flags |= (LW2080_CTRL_GR_SET_CTXSW_PREEMPTION_MODE_FLAGS_CILP_SET);
            grSetCtxswPreemptionModeParams.cilpPreemptMode = LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_CILP;
            break;
        }

        rc = m_pLwRm->Control(hSubdevice,
            LW2080_CTRL_CMD_GR_SET_CTXSW_PREEMPTION_MODE,
            (void*)&grSetCtxswPreemptionModeParams,
            sizeof(grSetCtxswPreemptionModeParams));

        if (rc != OK)
        {
            ErrPrintf("Error setting preemption ctxsw mode in RM: %s\n", rc.Message());
            return rc;
        }
    }

    return rc;
}

bool Trace3DTest::GetDidNullifyBeginEnd(UINT32 hwClass, UINT32 beginMethod)
{
    pair<UINT32,UINT32> p = make_pair(hwClass, beginMethod);
    bool didNullify = m_nullifyBeginEndSet.count(p) > 0;
    return didNullify;
}

void Trace3DTest::SetDidNullifyBeginEnd(UINT32 hwClass, UINT32 beginMethod)
{
    pair<UINT32,UINT32> p = make_pair(hwClass, beginMethod);
    m_nullifyBeginEndSet.insert(p);
}

void Trace3DTest::DumpStateAfterHang()
{
    // Normally timeouts are set to infinity on simulation, so likely this
    // function is only reached on silicon or emulation.
    //
    // Checking IsEmOrSim instead of IsEmulation to match the behavior
    // of the badly-named Gpu::IsEmulation.
    if (GetBoundGpuDevice()->GetSubdevice(0)->IsEmOrSim())
    {
        // Do a dummy read so emulator can get a trigger to dump the waves.
        Regs().Read32(MODS_PBUS_DEBUG_0);

        MASSERT(!"MODS timeout on emulator or simulator, abort to exit quickly\n");
    }
}

RC Trace3DTest::SwitchElpg(bool onOff)
{
    RC rc = OK;
    UINT32 elpgMask = params->ParamUnsigned("-elpg_mask", 0x0);

    if (params->ParamPresent("-disable_elpg_on_init") > 0)
    {

        GpuSubdevice *pSubDev = GetBoundGpuDevice()->GetSubdevice(0);
        PMU* pPmu;
        CHECK_RC(pSubDev->GetPmu(&pPmu));

        if (elpgMask > 0 && onOff)
        {
            // Since we're switching elpg on, then we need to check if
            // elpg has already been turned on for any particular engine.
            // It is illegal to powergate an engine that has already been
            // powergated, so we'll prevent that here.

            // GetPowerGatingParameters example:
            // pgParams[0].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED;
            // pgParams[0].parameterExtended = PMU::ELPG_GRAPHICS_ENGINE;
            // pgParams[0].parameterValue = 1; (0 for disable), return value
            vector<LW2080_CTRL_POWERGATING_PARAMETER> pgParams;
            LW2080_CTRL_POWERGATING_PARAMETER oneParam = {0};
            oneParam.parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED;

            for (int index = 0; index < 32; index++)
            {
                if (elpgMask & (0x1 << index))
                {
                    oneParam.parameterExtended = index;
                    pgParams.push_back(oneParam);
                }
            }

            CHECK_RC(pPmu->GetPowerGatingParameters(&pgParams));

            UINT32 pgParamIndex = 0;
            for (int index = 0; index < 32; index++)
            {
                if (elpgMask & (0x1 << index))
                {
                    //if the engine is powergated already, clear the bit in the elpg mask
                    elpgMask ^= (pgParams[pgParamIndex].parameterValue << index);
                    pgParamIndex++;
                }
            }
        }

        //we might have updated the elpg mask so check that it's non-zero
        if (elpgMask > 0)
        {
            if (onOff)
            {
                //MSCG needs to set a particular pstate before being enabled,
                //take care of that here.  Make sure this only happens if we're
                //enabling elpg with a valid mask (non-zero)
                CHECK_RC(SetPStateForMSCG(elpgMask));
            }

            // Now use the elpgMask to set the new power gating options.
            // SetPowerGatingParameters example:
            // pgParams[0].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED;
            // pgParams[0].parameterExtended = PMU::ELPG_GRAPHICS_ENGINE;
            // pgParams[0].parameterValue = 1; (0 for disable)
            vector<LW2080_CTRL_POWERGATING_PARAMETER> pgParams;
            LW2080_CTRL_POWERGATING_PARAMETER oneParam = {0};
            oneParam.parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED;
            oneParam.parameterValue = onOff;
            for (int index = 0; index < 32; index++)
            {
                if (elpgMask & (0x1 << index))
                {
                    oneParam.parameterExtended = index;
                    pgParams.push_back(oneParam);
                }
            }
            UINT32 Flags = 0;
            Flags = FLD_SET_DRF(2080, _CTRL_MC_SET_POWERGATING_THRESHOLD,
                                _FLAGS_BLOCKING, _TRUE, Flags);
            CHECK_RC(pPmu->SetPowerGatingParameters(&pgParams, Flags));
        }
    }

    return rc;
}

RC Trace3DTest::SetPStateForMSCG(UINT32 elpgMask)
{
    RC rc = OK;
    GpuSubdevice *pSubDev = GetBoundGpuDevice()->GetSubdevice(0);

    //check to see if we're turning on MSCG
    UINT32 mscgEnableMask = ((1 << PMU::ELPG_GRAPHICS_ENGINE) | (1 << PMU::ELPG_MS_ENGINE));
    if ((elpgMask & mscgEnableMask) != mscgEnableMask)
    {
        return OK; //mscg isn't enabled, don't modify the pstate
    }

    //bug 849131 exposes a coverage hole for mscg.  In order for mscg to activate,
    //we must first set the pstate to 8 (or 12). P8 is slightly more colwenient since
    //it is a bit quicker on simulation and P12 is no longer POR for gk10x.
    UINT32 mscgPState = 8;
    bool bValidPState = false;
    Perf* pPerf = pSubDev->GetPerf();

    CHECK_RC(pPerf->DoesPStateExist(mscgPState, &bValidPState));
    if (bValidPState)
    {
        UINT32 actualPState;
        CHECK_RC(pPerf->GetLwrrentPState(&actualPState));

        if (actualPState == mscgPState)
        {
            InfoPrintf("No Pstate changes necessary for MSCG on subdevice %d:%d: Desired=%d, Actual=%d\n",
                       pSubDev->GetParentDevice()->GetDeviceInst(),
                       pSubDev->GetSubdeviceInst(),
                       mscgPState, actualPState);
        }
        else
        {
            CHECK_RC(pPerf->ForcePState(mscgPState, LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR));
            CHECK_RC(pPerf->GetLwrrentPState(&actualPState));

            InfoPrintf("Set PState on subdevice %d:%d for MSCG: Desired=%d, Actual=%d\n",
                       pSubDev->GetParentDevice()->GetDeviceInst(),
                       pSubDev->GetSubdeviceInst(),
                       mscgPState, actualPState);
        }
    }
    else
    {
        WarnPrintf("Invalid PState %d on GPU %d:%d, cannot switch pstates for MSCG. This is non-fatal, RM will still enable MSCG.\n",
                   mscgPState,
                   pSubDev->GetParentDevice()->GetDeviceInst(),
                   pSubDev->GetSubdeviceInst());
    }

    return OK;
}

//--------------------------------------------------------------------------
// Add verification information for a surface to all graphic channels.
// This is called for dynamically allocated color/z surfaces.
//
void Trace3DTest::AddRenderTargetVerif(SurfaceType surfaceType)
{
    const bool stitch = !params->ParamPresent("-sli_scissor_no_stitching");
    int index = GetSurfaceTypeIndex(surfaceType);

    for (auto ch : m_TraceCpuManagedChannels)
    {
        if (ch->GetGrSubChannel())
        {
            GpuVerif* verif = ch->GetGpuVerif();
            MdiagSurf* surface = gsm->GetSurface(surfaceType, 0);

            MASSERT(surface != 0);

            if (gsm->GetValid(surface))
            {
                if (stitch)
                {
                    gsm->SetSLIScissorSpec(surface, m_SLIScissorSpec);
                }

                CrcRect rect;
                GetSurfaceCrcRect(surface, surfaceType, index, &rect);

                if (IsSurfaceTypeColor3D(surfaceType))
                {
                    verif->AddSurfC(surface, index, rect);
                }
                else
                {
                    verif->AddSurfZ(surface, rect);
                }
            }
        }
    }
}

void Trace3DTest::PreRun()
{
    LWGpuResource *lwgpu = GetGpuResource();
    MASSERT(lwgpu != NULL);

    if (lwgpu->GetContextScheduler())
    {
        lwgpu->GetContextScheduler()->BindThreadToTest(this);
    }

    ConlwrrentTest::PreRun();

    if (params->ParamPresent("-sync_at_start_stage"))
    {
        if (!m_IsCtxSwitchTest)
        {
            ErrPrintf("-sync_at_start_stage can be used in ctxswitch mode only");
            MASSERT(0);
        }

        UINT32 argValue = params->ParamUnsigned("-sync_at_start_stage");
        if (argValue == 0)
        {
            TestApiWaitOnEvent(StartStageEvent, GetTimeoutMs());
        }
        else
        {
            TestApiSetEvent(StartStageEvent);
        }
    }
    TestSynlwp(MDiagUtils::TestRunStart);
}

void Trace3DTest::PostRun()
{
    CheckStopTestSyncOnFail("Ahead of TestRunEnd");
    TestSynlwp(MDiagUtils::TestRunEnd);

    LWGpuResource *lwgpu = GetGpuResource();

    // Return early if FailSetup has been called before
    if (lwgpu == nullptr && GetStatus() == Test::TEST_NO_RESOURCES)
    {
        return;
    }

    MASSERT(lwgpu);

    // If this test was run in serial mode, clear the ZBC table
    // for the next test.
    if (LWGpuResource::GetTestDirectory()->IsSerialExelwtion())
    {
        DebugPrintf(MSGID(Main), "Clearing the ZBC tables.\n");
        lwgpu->ClearZbcTables(m_pLwRm);
    }

    if (lwgpu->GetContextScheduler())
    {
        RC rc = lwgpu->GetContextScheduler()->UnRegisterTestFromContext();
        if (OK != rc)
        {
            ErrPrintf("UnRegisterTestFromContext failed: %s\n", rc.Message());
            MASSERT(0);
        }
    }
}

void Trace3DTest::DeclareTegraSharedSurface(const char *name)
{
    m_TegraSharedSurfNames.insert(name);
}

RC Trace3DTest::SetupTegraSharedModules()
{
    RC rc;

    ModuleIter end = m_Trace.ModEnd();
    for (ModuleIter modIt = m_Trace.ModBegin(); modIt != end; ++modIt)
    {
        TraceModule *mod = *modIt;
        set<string>::iterator nameIt = m_TegraSharedSurfNames.find(mod->GetName());
        if (nameIt != m_TegraSharedSurfNames.end())
        {
            mod->SetSharedByTegra(true);
            m_TegraSharedSurfNames.erase(nameIt);
        }
    }

    return rc;
}

RC Trace3DTest::CheckIfAnyTegraSharedSurfaceLeft()
{
    // m_TegraSharedSurfNames should be empty now.
    // If not, then there must be unrecognized or ineffective surface.
    if (!m_TegraSharedSurfNames.empty())
    {
        for (auto& name: m_TegraSharedSurfNames)
        {
            ErrPrintf("trace_3d %s: Unrecognized or ineffective CheetAh shared surface: %s.\n",
                      __FUNCTION__, name.c_str());
        }
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

static void GpuReadyNotifyHandler(void* Args)
{
    MASSERT(Args);
    LWGpuResource *pLWGpuResource = (LWGpuResource *)Args;

    InfoPrintf("%s: GpuReady notifier is detected.\n", __FUNCTION__);

    GpuDevice *pGpuDevice = pLWGpuResource->GetGpuDevice();
    for (UINT32 ii = 0; ii < pGpuDevice->GetNumSubdevices(); ++ ii)
    {
        GpuSubdevice *pGpuSubdevice = pGpuDevice->GetSubdevice(ii);
        if (pGpuSubdevice->IsResmanEventHooked(LW2080_NOTIFIERS_GC5_GPU_READY))
        {
            LwNotification notifyData;
            RC rc = pGpuSubdevice->GetResmanEventData(
                                      LW2080_NOTIFIERS_GC5_GPU_READY,
                                      &notifyData);
            if (rc == OK)
            {
                Trace3DTest::TraceEventType plgnEvntType = Trace3DTest::TraceEventType::PowerGpuReady;
                const char *plgnEvntDataName = "GpuReadyEventType";

                // This event is global. Broadcast it to all active trace3d tests
                //
                set<Trace3DTest*> activeTests = pLWGpuResource->GetActiveT3DTests();
                for (auto test: activeTests)
                {
                    InfoPrintf("%s: Trace3DTest(%p) Sending \"%s\" trace event with data \"%s\" = 0x%x to plugin.\n",
                               __FUNCTION__, test, Trace3DTest::s_EventTypeMap.at(plgnEvntType).c_str(),
                               plgnEvntDataName, notifyData.info16);

                    test->traceEvent(plgnEvntType, plgnEvntDataName,
                                          notifyData.info16);
                }
            }
            else
            {
                MASSERT(!"There are errors in GpuReadyNotifyHandler.\n");
            }
        }
    }

    return;
}

RC Trace3DTest::SetDiGpuReady
(
    GpuSubdevice * gpuSubdev
)
{
    RC rc;

    // Parameter check
    if (!gpuSubdev)
    {
        ErrPrintf("%s: Invalid gpu subdev parameter.\n", __FUNCTION__);
        return RC::ILWALID_INPUT;
    }

    if (!gpuSubdev->IsResmanEventHooked(LW2080_NOTIFIERS_GC5_GPU_READY))
    {
        MASSERT(m_pBoundGpu);
        CHECK_RC(gpuSubdev->HookResmanEvent(
            LW2080_NOTIFIERS_GC5_GPU_READY,
            GpuReadyNotifyHandler, m_pBoundGpu,
            LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT,
            GpuSubdevice::NOTIFIER_MEMORY_ENABLED));
    }

    return rc;
}

static void GpuCEPrefetchHandler(void* Args)
{
    MASSERT(Args);
    LWGpuResource *pLWGpuResource = (LWGpuResource *)Args;

    DispPipeConfig::DispPipeConfigurator* pDpcConfigurator =
        pLWGpuResource->GetDispPipeConfigurator();

    InfoPrintf("Gpu prefetch notifier is detected.\n");

    GpuDevice *pGpuDevice = pLWGpuResource->GetGpuDevice();
    for (UINT32 ii = 0; ii < pGpuDevice->GetNumSubdevices(); ++ ii)
    {
        GpuSubdevice *pGpuSubdevice = pGpuDevice->GetSubdevice(ii);
        if (pGpuSubdevice->IsResmanEventHooked(LW2080_NOTIFIERS_LPWR_DIFR_PREFETCH_REQUEST))
        {
            pDpcConfigurator->RunPrefetchSequence();
        }
    }
}

RC Trace3DTest::SetEnableCEPrefetch
(
    GpuSubdevice *gpuSubdev,
    bool bEnable
)
{
    RC rc;

    InfoPrintf("Trace3DTest::SetEnablePrefetch: %d\n", bEnable);

    if (!gpuSubdev)
    {
        ErrPrintf("Trace3DTest::SetEnablePrefetch: Invalid gpu subdev parameter.\n");
        return RC::ILWALID_INPUT;
    }

    DispPipeConfig::DispPipeConfigurator* pDpcConfigurator =
        GetGpuResource()->GetDispPipeConfigurator();

    if (!pDpcConfigurator)
    {
        ErrPrintf("Trace3DTest::SetEnablePrefetch: DPC is required but no configured. \n");
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    CHECK_RC(pDpcConfigurator->SetEnablePrefetch(bEnable));

    if (bEnable)
    {
        pDpcConfigurator->SetLwRmClient(LwRmPtr().Get());

        if (!gpuSubdev->IsResmanEventHooked(LW2080_NOTIFIERS_LPWR_DIFR_PREFETCH_REQUEST))
        {
            MASSERT(m_pBoundGpu);

            CHECK_RC(gpuSubdev->HookResmanEvent(
                LW2080_NOTIFIERS_LPWR_DIFR_PREFETCH_REQUEST,
                GpuCEPrefetchHandler, m_pBoundGpu,
                LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT,
                GpuSubdevice::NOTIFIER_MEMORY_ENABLED));

        }
    }
    else
    {
        CHECK_RC(gpuSubdev->UnhookResmanEvent(LW2080_NOTIFIERS_LPWR_DIFR_PREFETCH_REQUEST));
    }

    return rc;
}

RC Trace3DTest::SetGpuPowerOnOff
(
    GpuSubdevice *gpuSubdev,
    UINT32 action,
    bool bUseHwTimer,
    UINT32 timeToWakeUs,
    bool bIsRTD3Transition,
    bool bIsD3Hot
)
{
    // Parameter check
    if (!gpuSubdev)
    {
        ErrPrintf("GPU on off test: Invalid gpu subdev parameter.\n");
        return RC::ILWALID_INPUT;
    }

    InfoPrintf("GPU on off test: action=0x%x, bUseHwTimer=%s, "
               "timeToWakeUs=0x%x, bIsRTD3Transition=%s, bIsD3Hot=%s\n",
               action, bUseHwTimer ? "true":"false",
               timeToWakeUs, bIsRTD3Transition ? "true":"false",
               bIsD3Hot ? "true":"false");

    //
    // Below still use LW2080_CTRL_CMD_GPU_POWER_ON_OFF
    // LW2080_CTRL_GPU_POWER_ON_OFF_RG_SAVE
    // LW2080_CTRL_GPU_POWER_ON_OFF_RG_RESTORE
    // LW2080_CTRL_GPU_POWER_ON_OFF_GC5_ENTER
    // LW2080_CTRL_GPU_POWER_ON_OFF_GC5_EXIT
    //
    // Below still use LW2080_CTRL_CMD_GC6_ENTER and LW2080_CTRL_CMD_GC6_EXIT
    // LW2080_CTRL_GPU_POWER_ON_OFF_GC6_ENTER
    // LW2080_CTRL_GPU_POWER_ON_OFF_GC6_EXIT
    //
    // Below change to use LW2080_CTRL_CMD_GC6_ENTER and LW2080_CTRL_CMD_GC6_EXIT
    // LW2080_CTRL_GPU_POWER_ON_OFF_MSHYBRID_GC6_ENTER
    // LW2080_CTRL_GPU_POWER_ON_OFF_MSHYBRID_GC6_EXIT
    //

    RC rc;

    if (LW2080_CTRL_GPU_POWER_ON_OFF_RG_SAVE == action ||
        LW2080_CTRL_GPU_POWER_ON_OFF_GC5_ENTER == action ||
        LW2080_CTRL_GPU_POWER_ON_OFF_RG_RESTORE == action ||
        LW2080_CTRL_GPU_POWER_ON_OFF_GC5_EXIT == action)
    {
        // Power on up function
        LW2080_CTRL_GPU_POWER_ON_OFF_PARAMS powerParams = {0};
        powerParams.action = action;

        if (LW2080_CTRL_GPU_POWER_ON_OFF_RG_SAVE == action ||
            LW2080_CTRL_GPU_POWER_ON_OFF_GC5_ENTER == action)
        {
            
            powerParams.params.bUseHwTimer = bUseHwTimer;
            if (bUseHwTimer)
            {
                powerParams.params.timeToWakeUs = timeToWakeUs;
            }
            else
            {
                if (timeToWakeUs != 0)
                {
                    WarnPrintf("GPU on off test: timeToWakeUs(0x%x) sent down is ignored because bUseHwTimer is false.\n",
                               timeToWakeUs);
                }
            }

            //
            // Hook the event handler if it has not been hooked.
            // The RM event has to be hooked before power on/off
            // action to avoid missing it.
            if (!gpuSubdev->IsResmanEventHooked(LW2080_NOTIFIERS_GC5_GPU_READY))
            {
                MASSERT(m_pBoundGpu);
                CHECK_RC(gpuSubdev->HookResmanEvent(
                    LW2080_NOTIFIERS_GC5_GPU_READY,
                    GpuReadyNotifyHandler, m_pBoundGpu,
                    LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT,
                    GpuSubdevice::NOTIFIER_MEMORY_ENABLED));
            }
        }

        rc = m_pLwRm->Control(m_pLwRm->GetSubdeviceHandle(gpuSubdev),
                                LW2080_CTRL_CMD_GPU_POWER_ON_OFF,
                                (void*)&powerParams,
                                sizeof(powerParams));

        if (OK != rc)
        {
            ErrPrintf("GPU on off test: LW2080_CTRL_CMD_GPU_POWER_ON_OFF failed. rc: %s.\n",
                      rc.Message());
            return rc;
        }
    }

    // RTD3 is reusing GC6_ENTER action with more flags
    // EnterGc6/ExitGc6 doesn't support RTD3 flags
    // So RTD3 entry/exit need a direct RM control call
    if (!bIsRTD3Transition)
    {
        if (action == LW2080_CTRL_GPU_POWER_ON_OFF_GC6_ENTER)
        {
            GC6EntryParams entry;
            memset(&entry, 0, sizeof(GC6EntryParams));
            entry.stepMask = BIT(LW2080_CTRL_GC6_STEP_ID_GPU_OFF);
            entry.flavorId = LW2080_CTRL_GC6_FLAVOR_ID_OPTIMUS;
            entry.params.bUseHwTimer = bUseHwTimer;
            entry.params.timeToWakeUs = timeToWakeUs;
            rc = gpuSubdev->EnterGc6(entry);

            if (OK != rc)
            {
                ErrPrintf("GPU on off test: LW2080_CTRL_CMD_GC6_ENTRY failed. rc: %s.\n",
                          rc.Message());
                return rc;
            }
        }
        else if (action == LW2080_CTRL_GPU_POWER_ON_OFF_GC6_EXIT)
        {
            GC6ExitParams exit;
            memset(&exit, 0, sizeof(GC6ExitParams));
            exit.flavorId = LW2080_CTRL_GC6_FLAVOR_ID_OPTIMUS;
            rc = gpuSubdev->ExitGc6(exit);

            if (OK != rc)
            {
                ErrPrintf("GPU on off test: LW2080_CTRL_CMD_GC6_EXIT failed. rc: %s.\n",
                          rc.Message());
                return rc;
            }
        }
    }
    else // RTD3 transition
    {
        LWGpuResource * lwgpu = GetGpuResource();
        UINT32 subdev = gpuSubdev->GetSubdeviceInst();

        // RTD3 entry
        if (action == LW2080_CTRL_GPU_POWER_ON_OFF_MSHYBRID_GC6_ENTER)
        {
            GC6EntryParams entry;
            memset(&entry, 0, sizeof(GC6EntryParams));
            entry.stepMask = BIT(LW2080_CTRL_GC6_STEP_ID_GPU_OFF);
            entry.flavorId = LW2080_CTRL_GC6_FLAVOR_ID_MSHYBRID;
            entry.params.bUseHwTimer = bUseHwTimer;
            entry.params.timeToWakeUs = timeToWakeUs;
            entry.params.bIsRTD3Transition = bIsRTD3Transition;

            rc = m_pLwRm->Control(m_pLwRm->GetSubdeviceHandle(gpuSubdev),
                                LW2080_CTRL_CMD_GC6_ENTRY,
                                (void*)&entry,
                                sizeof(entry));

            if (OK != rc)
            {
                ErrPrintf("GPU on off test: RTD3 LW2080_CTRL_CMD_GC6_ENTRY failed. rc: %s.\n",
                          rc.Message());
                return rc;
            }

            // RTD3 entry for hardware
            if (Platform::GetSimulationMode() == Platform::Hardware)
            {
                UINT32 pmCtlRegVal = 0;
                pmCtlRegVal = lwgpu->RegRd32(LW_SW_BRIDGE_REG_PM_CONTROL, subdev, m_pSmcEngine);
                pmCtlRegVal = FLD_SET_DRF(_SW_BRIDGE_REG, _PM_CONTROL, _DSP_FORCE_L2, _ENABLE, pmCtlRegVal);
                lwgpu->RegWr32(LW_SW_BRIDGE_REG_PM_CONTROL, pmCtlRegVal, subdev, m_pSmcEngine); // enable l2

                Tasker::PollHw(
                    Tasker::GetDefaultTimeoutMs(),
                    [lwgpu, subdev, this]
                    {
                        UINT32 ltssmRegVal = lwgpu->RegRd32(LW_SW_BRIDGE_GPU_BACKDOOR_LTSSM_STATUS0,
                                                 subdev, m_pSmcEngine);
                        return (DRF_VAL(_SW_BRIDGE_GPU_BACKDOOR, _LTSSM_STATUS0, _SWITCH_DSP, ltssmRegVal) ==
                                LW_SW_BRIDGE_GPU_BACKDOOR_LTSSM_STATUS0_SWITCH_DSP_L2);
                    },
                    __FUNCTION__);
            }
        }
        // RTD3 exit
        else if (action == LW2080_CTRL_GPU_POWER_ON_OFF_MSHYBRID_GC6_EXIT)
        {
            // RTD3 exit for hardware, bring up link must happeded before RM control call
            if (Platform::GetSimulationMode() == Platform::Hardware)
            {
                UINT32 pmCtlRegVal = 0;
                pmCtlRegVal = lwgpu->RegRd32(LW_SW_BRIDGE_REG_PM_CONTROL, subdev, m_pSmcEngine);
                pmCtlRegVal = FLD_SET_DRF(_SW_BRIDGE_REG, _PM_CONTROL, _DSP_FORCE_L2, _DISABLE, pmCtlRegVal);
                lwgpu->RegWr32(LW_SW_BRIDGE_REG_PM_CONTROL, pmCtlRegVal, subdev, m_pSmcEngine); // clear l2

                Tasker::PollHw(
                    Tasker::GetDefaultTimeoutMs(),
                    [lwgpu, subdev, this]
                    {
                        UINT32 ltssmRegVal = lwgpu->RegRd32(LW_SW_BRIDGE_GPU_BACKDOOR_LTSSM_STATUS0,
                                                 subdev, m_pSmcEngine);
                        return (DRF_VAL(_SW_BRIDGE_GPU_BACKDOOR, _LTSSM_STATUS0, _SWITCH_DSP, ltssmRegVal) ==
                                LW_SW_BRIDGE_GPU_BACKDOOR_LTSSM_STATUS0_SWITCH_DSP_L0);
                    },
                    __FUNCTION__);
            }

            GC6ExitParams exit;
            memset(&exit, 0, sizeof(GC6ExitParams));
            exit.flavorId = LW2080_CTRL_GC6_FLAVOR_ID_MSHYBRID;
            exit.params.bIsRTD3Transition = bIsRTD3Transition;

            rc = m_pLwRm->Control(m_pLwRm->GetSubdeviceHandle(gpuSubdev),
                                LW2080_CTRL_CMD_GC6_EXIT,
                                (void*)&exit,
                                sizeof(exit));

            if (OK != rc)
            {
                ErrPrintf("GPU on off test: RTD3 LW2080_CTRL_CMD_GC6_EXIT failed. rc: %s.\n",
                          rc.Message());
                return rc;
            }
        }
    }

    if (bIsD3Hot && (Platform::GetSimulationMode() == Platform::RTL))
    {
        InfoPrintf("bIsD3Hot is true, skip L2 entry/exit\n");
        return rc;
    }
    // Trigger pcie l2 entry/exit for RTD3. In production, this is triggered by
    // system software. Only supported by RTL chiplib. No plan for HW yet.
    if (bIsRTD3Transition && (Platform::GetSimulationMode() == Platform::RTL))
    {
        // Bug 200343895 has the details about the steps
        //
        const char * escapeKey = nullptr;
        if (LW2080_CTRL_GPU_POWER_ON_OFF_MSHYBRID_GC6_ENTER == action)
        {
            // Trigger L2 entry and wait it's done
            Platform::EscapeWrite("initiate_pcie_l2_entry", 0, 4, 1);
            escapeKey = "pcie_l2_entry_done";
        }
        else if (LW2080_CTRL_GPU_POWER_ON_OFF_MSHYBRID_GC6_EXIT == action)
        {
            // Poll on L2 exit done
            // No need to trigger exit according to bug 200343895
            escapeKey = "pcie_l2_exit_done";
        }
        else
        {
            MASSERT(!"Unkown action id in case of bIsRTD3Transition==true");
        }

        Tasker::PollHw(
            Tasker::GetDefaultTimeoutMs(),
            [escapeKey]
            {
                UINT32 done = 0;
                Platform::EscapeRead(escapeKey, 0, sizeof(done), &done);
                return done;
            },
            __FUNCTION__);
    }

    return rc;
}

void Trace3DTest::DeclareGlobalSharedSurface(const char *name, const char *global_name)
{
    m_GlobalSharedSurfNames[name] = global_name;
}

UINT32 Trace3DTest::GetTPCCount(UINT32 subdev)
{
    return GetGrInfo(LW2080_CTRL_GR_INFO_INDEX_SHADER_PIPE_SUB_COUNT, subdev);
}

UINT32 Trace3DTest::GetComputeWarpsPerTPC(UINT32 subdev)
{
    return GetGraphicsWarpsPerTPC(subdev);
}

UINT32 Trace3DTest::GetGraphicsWarpsPerTPC(UINT32 subdev)
{
    UINT32 smPerTpc = GetGrInfo(LW2080_CTRL_GR_INFO_INDEX_LITTER_NUM_SM_PER_TPC, subdev);
    return GetGrInfo(LW2080_CTRL_GR_INFO_INDEX_MAX_WARPS_PER_SM, subdev) * smPerTpc;
}

UINT32 Trace3DTest::GetGrInfo(UINT32 index, UINT32 subdevIndex)
{
    UINT32 data = 0;

    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        switch (index)
        {
        // Actually this RM API returns warps_per_TPC
        case LW2080_CTRL_GR_INFO_INDEX_MAX_WARPS_PER_SM:
            if (EngineClasses::IsGpuFamilyClassOrLater(
                TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_VOLTA))
            {
                data = 64;
            }
            else if (EngineClasses::IsGpuFamilyClassOrLater(
                TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_PASCAL_A))
            {
                data = 128;
            }
            else
            {
                data = 48; // Bug 335369
            }
            break;
        case LW2080_CTRL_GR_INFO_INDEX_SHADER_PIPE_SUB_COUNT:
            if (EngineClasses::IsGpuFamilyClassOrLater(
                TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_ADA))
            {
                data = 72;
            }
            else if (EngineClasses::IsGpuFamilyClassOrLater(
                TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_PASCAL_A))
            {
                data = 64;
            }
            else
            {
                data = 32; // Bug 335369, set #TPC to 32
            }
            break;
        // SM_per_TPC number
        case LW2080_CTRL_GR_INFO_INDEX_LITTER_NUM_SM_PER_TPC:
            if(EngineClasses::IsGpuFamilyClassOrLater(
                TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_VOLTA))
            {
                data = 2;
            }
            else
            {
                data = 1;
            }
            break;
        default:
            ErrPrintf("TraceModule::GetGrInfo(): %d is a invalid index running on Amodel\n", index);
        }
    }
    else
    {
        GpuSubdevice *pSubdev = GetBoundGpuDevice()->GetSubdevice(subdevIndex);

        LW2080_CTRL_GR_INFO info;
        info.index = index;
        LW2080_CTRL_GR_GET_INFO_PARAMS params = {0};
        params.grInfoListSize = 1;
        params.grInfoList = (LwP64)&info;

        m_pLwRm->Control(m_pLwRm->GetSubdeviceHandle(pSubdev),
            LW2080_CTRL_CMD_GR_GET_INFO,
            &params,
            sizeof(params));

        data = info.data;
    }

    return data;
}

#ifndef _WIN32
RC Trace3DTest::CreateCompbitTest()
{
    UINT32 testType;
    testType = params->ParamUnsigned("-compbit_test_type");
    if (testType > 5)
    {
        ErrPrintf("illegal -compbit_test_type value %u\n", testType);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    RC rc;
    TraceChannel *channel = GetGrChannel();
    MASSERT(channel != 0);
    TraceSubChannel *pTraceSubch = channel->GetGrSubChannel();

    m_CompBitsTest.reset(CompBitsTestIF::CreateInstance(pTraceSubch, testType));

    GpuDevice *pGpuDevice = GetBoundGpuDevice();
    FLOAT64 timeoutMs = GetTimeoutMs();
    CHECK_RC(m_CompBitsTest->SetArgs(surfC[0], pGpuDevice, channel, params, timeoutMs));
    CHECK_RC(m_CompBitsTest->Init());

    return rc;
}
#endif

RC Trace3DTest::GetUniqueVAS(PmVaSpace ** pVaSpace)
{
    RC rc;
    PolicyManager * pPM = PolicyManager::Instance();
    PmVaSpaces vaspaces = pPM->GetActiveVaSpaces();
    UINT32 uniqueVASCount = 0;

    for (auto vas : vaspaces)
    {
        if (vas->IsGlobal())
        {
            // All global VASs share the same test ID
            // VAS name check is needed to find the correct VAS in test
            if (vas->GetName() ==
                params->ParamStr("-address_space_name", ""))
            {
                // just support one trace which use the command line to specify the vaspace
                *pVaSpace = vas;
                uniqueVASCount++;
            }
        }
        else if(vas->GetTest()->GetTestId() == this->GetTestId())
        {
            *pVaSpace = vas;
            uniqueVASCount ++;
        }
    }

    if(uniqueVASCount != 1)
    {
        ErrPrintf("%s: Event can't deduce the vaspace. Please specify the vas handle explicity.\n",
                  __FUNCTION__);
        return RC::ILWALID_INPUT;
    }
    else
        return rc;
}

void Trace3DTest::FreeResource()
{
    if (m_pBoundGpu)
    {
        FreeSubCtxs(m_TestId);
        FreeVaSpace(m_TestId);
    }
    FreeTsgs();
}

RC Trace3DTest::FreeSubCtxs(UINT32 testId)
{
    RC rc;

    LWGpuResource::SubCtxManager * subCtxManager = m_pBoundGpu->GetSubCtxManager(m_pLwRm);
    if (Utl::HasInstance())
    {
        Utl::Instance()->FreeSubCtx(testId);
        Utl::Instance()->FreeSubCtx(LWGpuResource::TEST_ID_GLOBAL);
    }

    subCtxManager->FreeObjectInTest(testId);

    // RM will automatically free child handle  when free parent handle,
    // that means free tsg handle will free child subctx handles, so we need to free subctxs before tsg
    subCtxManager->FreeObjectInTest(LWGpuResource::TEST_ID_GLOBAL);

    return rc;
}

RC Trace3DTest::FreeVaSpace(UINT32 testId)
{
    RC rc;

    LWGpuResource::VaSpaceManager * vasManager = m_pBoundGpu->GetVaSpaceManager(m_pLwRm);
    if (Utl::HasInstance())
    {
        Utl::Instance()->FreeVaSpace(testId, m_pLwRm);
        Utl::Instance()->FreeVaSpace(LWGpuResource::TEST_ID_GLOBAL, m_pLwRm);
    }
    vasManager->FreeObjectInTest(testId);

    vasManager->FreeObjectInTest(LWGpuResource::TEST_ID_GLOBAL);

    return rc;
}

bool Trace3DTest::IsCoordinatorSyncCtx() const
{
    return m_CoordinatorSyncCtx;
}

void Trace3DTest::SetCoordinatorSyncCtx(bool isCoordinator)
{
    DebugPrintf("%s, set coordinator sync bit %d in test %s\n",
                __FUNCTION__, isCoordinator, m_TestName.c_str());
    m_CoordinatorSyncCtx = isCoordinator;
}

bool Trace3DTest::CheckPMSyncNumInConlwrrentTests()
{
    if (IsTestCtxSwitching())
    {
        DebugPrintf("%s, skip check oclwrrences of PMTRIGGER_SYNC_EVENT in test %s\n",
                    __FUNCTION__, m_TestName.c_str());
        return true;
    }

    if (s_Trace3DId2Test.size() == 1)
    {
        return true;
    }

    auto iter = s_Trace3DId2Test.begin();
    Trace3DTest* pTest = iter->second;

    if (pTest != this)
    {
        if (pTest->m_Trace.GetSyncPmTriggerPairNum() !=
            m_Trace.GetSyncPmTriggerPairNum())
        {
            ErrPrintf("test %s have different %d pairs of PMTRIGGER_SYNC_EVENT than others!\n",
                      m_TestName.c_str(), m_Trace.GetSyncPmTriggerPairNum());
            return false;
        }
    }

    return true;
}

void Trace3DTest::PrintChannelAllocInfo()
{
    m_ChannelAllocInfo.Print(Utility::StrPrintf("Test: %s", GetTestName()).c_str());
}

//
// Possible TestSync flows at least fall into the following three:
// 1. Normal:
// TestSetupStart -> TestSetupEnd -> TestRunStart -> TestBeforeCrcCheck
//      ->TestRunEnd -> TestCleanupStart -> TestCleanupEnd
//
// 2. Trace3DTest::Setup() failed:
// TestSetupStart -> Trace3DTest::Setup() failed -> TestCleanupStart -> TestCleanupEnd
// Trace3DTest::CleanUp() may not be called when "-quick_exit" specified.
// In this case TestCleanupStart/End will not be covered.
//
// 3. Trace3DTest::BeginIteration() and RealSetup() failed:
// TestSetupStart -> TestSetupEnd -> TestRunStart -> Trace3DTest::BeginIteration()
//      -> Trace3DTest::RealSetup() failed -> Trace3DTest::CleanUp() ->
//      -> TestCleanupStart -> TestCleanupEnd, Trace3DTest::BeginIteration()
//      -> Trace3DTest::PostRun() -> TestRunEnd
// In this case TestBeforeCrcCheck will not be covered.
//

void Trace3DTest::CheckStopTestSyncOnFail(const char* pCheckPointName)
{
    // Return early for non-SRIOV mode
    if (Platform::IsDefaultMode())
        return;

    if (FAILED == state ||
        // state would be set to FAILED if First loop failed in BeginIteration().
        status != Test::TEST_SUCCEEDED || m_bAborted)
    {
        // If Trace3D test can't be a success, SRIOV test sycnup need be disabled.
        InfoPrintf(SRIOVTestSyncTag "Trace3DTest::%s: test can't be a success (at point %s), disable test sync-up.\n",
                   __FUNCTION__, pCheckPointName ? pCheckPointName : "NotSpecified");
        VmiopMdiagElwManager::Instance()->StopTestSync();
    }
}

void Trace3DTest::TestSynlwp(const MDiagUtils::TestStage stage)
{
    // Return early for non-SRIOV mode
    if (Platform::IsDefaultMode())
        return;

    if ((MDiagUtils::TestCleanupStart == stage ||
        MDiagUtils::TestCleanupEnd == stage) &&
        m_CleanupDone)
    {
        DebugPrintf(SRIOVTestSyncTag "%s: Ignore test ID %d duplicated stage %d (%s).\n",
                    __FUNCTION__, GetTestId(), stage, MDiagUtils::GetTestStageName(stage));

        return;
    }

    DebugPrintf(SRIOVTestSyncTag "%s: test ID %d stage %d (%s). Trace3DTest aborted flag (%s).\n",
                __FUNCTION__, GetTestId(), stage, MDiagUtils::GetTestStageName(stage), m_bAborted ? "Yes" : "No");
    VmiopMdiagElwManager::Instance()->TestSynlwp(GetTestId(), stage);
    if (MDiagUtils::TestCleanupEnd == stage)
    {
        m_CleanupDone = true;
    }
}

// The function only can be called by UTL function
void Trace3DTest::SetSmcEngine(SmcEngine * pSmcEngine)
{
    m_pSmcEngine = pSmcEngine;
    if (GetGpuResource()->GetSmcResourceController()->IsSmcMemApi())
        m_SmcEngineName = pSmcEngine->GetName();
    else
        m_SmcEngineName = SmcResourceController::ColwertSysPipe2Str(pSmcEngine->GetSysPipe());
}

RegHal& Trace3DTest::Regs(UINT32 subdev)
{
    return GetGpuResource()->GetRegHal(GetBoundGpuDevice()->GetSubdevice(subdev),m_pLwRm, m_pSmcEngine);
}

// This function only be called by UTL function
UtlGpuVerif *  Trace3DTest::CreateGpuVerif
(
    LwRm::Handle hVaSpace,
    LWGpuChannel * pCh
)
{
#ifdef INCLUDE_MDIAGUTL
    unique_ptr<UtlGpuVerif> utlGpuVerif =
        std::make_unique<UtlT3dGpuVerif>(UtlT3dGpuVerif(this, hVaSpace, pCh));
    return utlGpuVerif.release();
#else
    return nullptr;
#endif
}
