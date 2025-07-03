/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mdiag.h"

#include "core/include/chiplibtracecapture.h"
#include "core/include/cmdline.h"
#include "core/include/fileholder.h"
#include "core/include/gpu.h"
#include "core/include/jscript.h"
#include "core/include/mgrmgr.h"
#include "core/include/mrmtestinfra.h"
#include "core/include/lwrm.h"
#include "core/include/script.h"
#include "core/include/simclk.h"
#include "core/include/tasker.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "core/include/version.h"
#include "core/utility/trep.h"
#include "ctrl/ctrl0000.h"
#include "device/interface/pcie.h"
#include "gpu/include/floorsweepimpl.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/js_gpusb.h"
#include "gpu/utility/gpuutils.h"
#include "mdiag/advschd/policymn.h"
#include "mdiag/iommu/iommudrv.h"
#include "mdiag/tests/test.h"
#include "mdiag/tests/test_state_report.h"
#include "mdiag/tests/testdir.h"
#include "mdiag/thread.h"
#include "mdiag/utils/mdiag_xml.h"
#include "mdiag/utils/mmuutils.h"
#include "mdiag/utils/lwgpu_classes.h"
#include "mdiag/utils/randstrm.h"
#include "mdiag/utl/utl.h"
#include "mdiag/vgpu_migration/vgpu_migration.h"
#include "mdiag/vmiopmods/vmiopmdiagelwmgr.h"

#include <memory>
#include <sstream>

namespace Mdiag
{
    RC Run(ArgDatabase *pArgs);
    RC DescribeArgs(const string &TestName);
    RC DescribeTests();
    RC WriteEarlyRegFields(UINT32 devInst, UINT32 subdevInst, const string args);
    RC LoadTests(TestDirectory *pTestdir, const vector<string> *pTestCmdLines,
                 ArgDatabase *pArgs, UINT32 numLoops);
    void CleanUpAllTests(TestDirectory *testdir, bool quick_exit);
    RC DumpTrepGpuSubdeviceData(ArgReader *args, Trep *pTrep,
                                GpuSubdevice *pSubdev);
    RC DumpTrepAdditionalChipData(ArgReader *args, Trep *trep, GpuSubdevice *subdev);

    RC RTLSaveRestore(const ArgReader& global_args, const char* point="mdiag_init");
    void VerifyLwlink(const ArgReader& global_args);
    RC CheckUtlScripts(ArgDatabase *pArgs);
    RC PrintUtlHelp(ArgDatabase *pArgs);
}

#define TEST_SETUP_SECTION 0x80000001

extern Tasker::ThreadID s_GpuThreadThreadID;
static bool s_NoTrep = false;
static bool s_EnableSmmu = false;
static ArgReader* s_MdiagParam = nullptr;

// Declare this here rather than including utils/sharedfaultbuffers.h, in order
// to avoid transitively including containers/queue.h, which conflicts with
// other headers in mdiag.cpp.
extern void CleanupMMUFaultBuffers();

static const ParamDecl global_params[] = {
  { "-e",             "t",  ParamDecl::PARAM_MULTI_OK, 0, 0,"test_name [-testargs...]"},
  { "-f",             "t",  ParamDecl::PARAM_MULTI_OK, 0, 0,"test_file (contains list of tests)"},
  { "-output_level",  "u",  (ParamDecl::PARAM_ENFORCE_RANGE |
                             ParamDecl::GROUP_START), 0, 5, "set output level:" },
  { "-silent",        "0",  ParamDecl::GROUP_MEMBER,  0, 0, "NO output (not even errors)" },
  { "-quiet",         "1",  ParamDecl::GROUP_MEMBER,  0, 0, "only warnings, errors, critical output" },
  { "-info",          "2",  ParamDecl::GROUP_MEMBER,  0, 0, "errors, warnings, info messages (default)" },
  { "-debug",         "3",  ParamDecl::GROUP_MEMBER,  0, 0, "enable extra debug output" },
  STRING_PARAM("-outputfilename", "status output file base name"),

  UNSIGNED_PARAM("-seed0", "first (of three) system-wide random number seeds"),
  UNSIGNED_PARAM("-seed1", "second (of three) system-wide random number seeds"),
  UNSIGNED_PARAM("-seed2", "third (of three) system-wide random number seeds"),

  SIMPLE_PARAM("-check", "just set tests up - don't run them"),
  SIMPLE_PARAM("-disable_utl", "Do not initialize UTL instance\n"),

  STRING_PARAM("-trepfile", "collected Test REPort file name (WIP -KMW), default: no trep output"),

  // XML output control params
  SIMPLE_PARAM("-xall", "Output all defined XML element groups"),
  STRING_PARAM("-xexclude", "Full XML output except for the named XML element groups"),
  STRING_PARAM("-xf", "Send XML output to named file instead of mdiag.xml"),
  STRING_PARAM("-xinclude", "XML output for only the named XML element groups"),
  SIMPLE_PARAM("-xs", "XML output for the Standard XML element group only"),
  SIMPLE_PARAM("-xc", "compress XML output"),

  SIMPLE_PARAM("-debugWaitMain", "spin in a loop waiting for debugger"),

  FLOAT_PARAM("-hostclk",            "HOST clock in mhz"),

  // Advsch
  SIMPLE_PARAM("-pre_oceb",          "Alert policy manager that OCEB support isn't ready yet"),
  SIMPLE_PARAM("-strict_test_args",    "mdiag arguments will not be processed as test arguments. This requires *all*"
                                       " test arguments be placed inside .cfg file or after -e option"),

  SIMPLE_PARAM("-serial", "forces all tests to run serially"),
  SIMPLE_PARAM("-quick_exit", "Exit MODS as soon as the test results are recorded, without shutting down the GPUs"),
  SIMPLE_PARAM("-enable_simrtl_save", "Enable RTL save (call $save in verilog side)"),
  SIMPLE_PARAM("-enable_simrtl_restore", "Enable RTL restore (call $restart in verilog side)"),
  STRING_PARAM("-rtl_save_restore_point", "Change save/restore point, defaults to mdiag_init"),
  SIMPLE_PARAM("-rtl_save_quick_exit", "Quick and dirty exit after RTL save end"),
  STRING_PARAM("-smc_partitioning", "configure the gpu partition and smc information (deprecated from Hopper, use -smc_mem/-smc_eng)."),
  SIMPLE_PARAM("-smc_partitioning_sys0_only", "specified all non-floorsweeping GPC will be connected to the sys0.\n"),
  STRING_PARAM("-smc_mem", "SMC Partition specification Hopper onwards\n"),
  STRING_PARAM("-smc_eng", "SMC Engine specification Hopper onwards\n"),

#ifdef INCLUDE_MDIAGUTL
  { "-utl_script", "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "Run a UTL script. Optional format: -utl_script option1=value1:option2=value2:path/scripts/a.py --arg1 ..."
    ", supported option: archive_path=/your/path/to/hdr.zip.tgz" },
  { "-utl_library_path", "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "Specify a library path for UTL to add to sys.path when importing scripts" },
  UNSIGNED_PARAM("-utl_start_event_callback_count", "overrides the default number of Start event callbacks from 1 to the specified number"),
#endif

  { "-lwlink_set_verify_config_mode",  "u",  (ParamDecl::PARAM_ENFORCE_RANGE |
                            ParamDecl::GROUP_START), 0, 2, "set the LwLink verify config mode:" },
  { "-lwlink_set_verify_config_none",  "0",  ParamDecl::GROUP_MEMBER,  0, 0, "don't verify LwLink configs match" },
  { "-lwlink_set_verify_config_exact", "1",  ParamDecl::GROUP_MEMBER,  0, 0, "verify LwLink configs match exactly" },
  { "-lwlink_set_verify_config_superset", "2",  ParamDecl::GROUP_MEMBER,  0, 0, "verify actual config is superset of needed config" },

  STRING_PARAM("-cpu_model_test", "Allows the user to specify a CPU test and an optional set of arguments for the CPU test." ),
  SIMPLE_PARAM("-disable_location_override_for_mdiag", "Disable the surface location override (e.g. -force_fb) for all mdiag surfaces."),
  { "-RawImageMode", "u", (ParamDecl::PARAM_ENFORCE_RANGE |
                         ParamDecl::GROUP_START | ParamDecl::GROUP_OVERRIDE_OK), RAWSTART, RAWEND, "Read or disable raw (uncompressed) framebuffer images" },
  { "-RawImagesOff", (const char*)RAWOFF,    ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, 0, 0, "Disable raw (uncompressed) framebuffer images"},
  { "-RawImages",    (const char*)RAWON,     ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, 0, 0, "Read raw (uncompressed) framebuffer images"},
  SIMPLE_PARAM("-using_external_memory", "Use external memory"),
  UNSIGNED64_PARAM("-va_range_limit", "Specifies the limit for virtual address range."),

  LAST_PARAM
};

static const ParamDecl loop_params[] = {
  UNSIGNED_PARAM("-loops", "Loop the tests this many times."),
  UNSIGNED_PARAM("-loop_ms", "Loop the tests this many milliseconds."),
  LAST_PARAM
};

Mdiag_XML *gXML = NULL;

#include <sys/timeb.h>
static unsigned long getLwrrentRealTime() {
#if defined(_WIN32)
    struct _timeb timebuffer;
    _ftime(&timebuffer);
#else
    struct timeb timebuffer;
    ftime(&timebuffer);
#endif
    return (unsigned long)timebuffer.time;
}

//------------------------------------------------------------------------------
// JavaScript linkage

JS_CLASS(Mdiag);

SObject Mdiag_Object
(
    "Mdiag",
    MdiagClass,
    0,
    0,
    "Mdiag Tests."
);

static SProperty Mdiag_NoGpu
(
    Mdiag_Object,
    "NoGpu",
    0,
    false,
    0,
    0,
    0,
    "If true, do not look for, initialize or in any other way try to touch a gpu."
);

SProperty Mdiag_NoTrep
(
    Mdiag_Object,
    "NoTrep",
    0,
    false,
    0,
    0,
    0,
    "If true, do not create test trep file with status"
);

static SProperty Mdiag_EnableSmmu
(
    Mdiag_Object,
    "EnableSmmu",
    0,
    false,
    0,
    0,
    0,
    "Enable loading smmu driver"
);

//------------------------------------------------------------------------------
// Methods and Tests
//------------------------------------------------------------------------------
C_(Mdiag_Run);
static STest Mdiag_Run
(
    Mdiag_Object,
    "Run",
    C_Mdiag_Run,
    0,
    "Run mdiag tests."
);

C_(Mdiag_DescribeArgs);
static STest Mdiag_DescribeArgs
(
    Mdiag_Object,
    "DescribeArgs",
    C_Mdiag_DescribeArgs,
    0,
    "Print the arguments supported by a particular mdiag test."
);

C_(Mdiag_DescribeTests);
static STest Mdiag_DescribeTests
(
    Mdiag_Object,
    "DescribeTests",
    C_Mdiag_DescribeTests,
    0,
    "List all of the mdiag tests."
);

C_(Mdiag_CheckUtlScripts);
static STest Mdiag_CheckUtlScripts
(
    Mdiag_Object,
    "CheckUtlScripts",
    C_Mdiag_CheckUtlScripts,
    0,
    "Check UTL scripts."
);

C_(Mdiag_PrintUtlHelp);
static STest Mdiag_PrintUtlHelp
(
    Mdiag_Object,
    "PrintUtlHelp",
    C_Mdiag_PrintUtlHelp,
    0,
    "Print UTL help."
);

C_(Mdiag_WriteEarlyRegFields);
static STest Mdiag_WriteEarlyRegFields
(
    Mdiag_Object,
    "WriteEarlyRegFields",
    C_Mdiag_WriteEarlyRegFields,
    0,
    "Set register fields before RM initialization."
);

// STest
C_(Mdiag_Run)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    const char   *usage = "Usage: Mdiag.Run(ArgDatabase)\n";
    JavaScriptPtr pJavaScript;
    JSObject     *jsArgDb;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &jsArgDb)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    ArgDatabase *pArgDb = JS_GET_PRIVATE(ArgDatabase, pContext, jsArgDb, "ArgDatabase");
    if (pArgDb == NULL)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    RETURN_RC(Mdiag::Run(pArgDb));
}

// STest
C_(Mdiag_DescribeArgs)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    string TestName;
    JavaScriptPtr pJavaScript;

    // This is a void method.
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &TestName)))
    {
        JS_ReportError(pContext, "Usage: Mdiag.DescribeArgs(TestName)");
        return JS_FALSE;
    }

    RETURN_RC(Mdiag::DescribeArgs(TestName));
}

// STest
C_(Mdiag_DescribeTests)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // This is a void method.
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: Mdiag.DescribeTests()");
        return JS_FALSE;
    }

    RETURN_RC(Mdiag::DescribeTests());
}

C_(Mdiag_CheckUtlScripts)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    const char   *usage = "Usage: Mdiag.CheckUtlScripts(ArgDatabase)\n";
    JavaScriptPtr pJavaScript;
    JSObject     *jsArgDb;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &jsArgDb)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    ArgDatabase *pArgDb = JS_GET_PRIVATE(ArgDatabase, pContext, jsArgDb, "ArgDatabase");
    if (pArgDb == NULL)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    RETURN_RC(Mdiag::CheckUtlScripts(pArgDb));
}

C_(Mdiag_PrintUtlHelp)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    const char   *usage = "Usage: Mdiag.Run(ArgDatabase)\n";
    JavaScriptPtr pJavaScript;
    JSObject     *jsArgDb;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &jsArgDb)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    ArgDatabase *pArgDb = JS_GET_PRIVATE(ArgDatabase, pContext, jsArgDb, "ArgDatabase");
    if (pArgDb == NULL)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    RETURN_RC(Mdiag::PrintUtlHelp(pArgDb));
}

// STest
C_(Mdiag_WriteEarlyRegFields)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    UINT32 dev;
    UINT32 subdev;
    string args;

    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &dev)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &subdev)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &args)))
    {
        JS_ReportError(pContext, "Usage: Mdiag.WriteEarlyRegFields(devInst, subdevInst, args)\n");
        return JS_FALSE;
    }

    RETURN_RC(Mdiag::WriteEarlyRegFields(dev, subdev, args));
}

// save/restore RTL
// the point can be mdiag_init or test_init
RC Mdiag::RTLSaveRestore(const ArgReader& global_args, const char* point)
{
    const char* current = global_args.ParamStr("-rtl_save_restore_point", "mdiag_init");
    if (global_args.ParamPresent("-enable_simrtl_save") && global_args.ParamPresent("-enable_simrtl_restore"))
    {
        ErrPrintf("-enable_simrtl_save and -enable_simrtl_restore used together!\n");
        MASSERT(0);
    }
    else if(global_args.ParamPresent("-enable_simrtl_save") && (strcmp(current, point) == 0))
    {
        Platform::EscapeWrite("sim_save",0,0,0);
        Platform::ClockSimulator(1);
        DebugPrintf("MODS Fast-Forward: SAVE END @%s\n", point);

        if (global_args.ParamPresent("-rtl_save_quick_exit"))
        {
            DebugPrintf("MODS Fast-Forward: Quick and dirty exit...\n");
            RC rc = MDiagUtils::TagTrepFilePass();
            if (rc != OK)
            {
                ErrPrintf("MODS Fast-Forward: TagTrepFilePass failed due to rc = %s.\n", rc.Message());
            }

            Utility::ExitMods(rc, Utility::ExitQuickAndDirty);
        }
    }
    else if(global_args.ParamPresent("-enable_simrtl_restore") && strcmp(current, point) == 0)
    {
        Platform::EscapeWrite("sim_restore",0,0,0);
        Platform::ClockSimulator(1);
        DebugPrintf("MODS Fast-Forward: RESTORE END @%s\n", point);
    }
    return OK;
}

RC Mdiag::Run(ArgDatabase *pArgs)
{
    CHIPLIB_CALLSCOPE;

    JavaScriptPtr pJs;
    StickyRC rc;
    int num_tests, tests_set_up;
    unique_ptr<Mdiag_XML> XmlWriter;
    DEFER
    {
        CleanupMMUFaultBuffers();
        LWGpuResource::FreeResources();
    };
    Trep trep;
    TestDirectory testdir(pArgs, &trep);
    VmiopMdiagElwManager* pMdiagElwMgr = VmiopMdiagElwManager::Instance();
    bool quick_exit = false;

    // Get test properties
    CHECK_RC(pJs->GetProperty(Mdiag_Object, Mdiag_NoTrep, &s_NoTrep));
    CHECK_RC(pJs->GetProperty(Mdiag_Object, Mdiag_EnableSmmu, &s_EnableSmmu));

    // now re-parse with global flag
    ArgReader global_args(global_params);
    if (!global_args.ParseArgs(pArgs, ""))
    {
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    s_MdiagParam = &global_args;
    Utility::CleanupValue<ArgReader*> globalParamCleanup(s_MdiagParam, nullptr);

    DebugPrintf("Mdiag::Run global args have been parsed\n");

    EngineClasses::CreateAllEngineClasses();

#ifdef INCLUDE_MDIAGUTL
    if (!global_args.ParamPresent("-disable_utl") && Utl::IsSupported())
    {
        Utl::CreateInstance(pArgs, &global_args, &trep, false);
    }
#endif

    if (global_args.ParamPresent("-debugWaitMain"))
    {
        Printf(Tee::PriAlways, "Waiting for debug connnect...\n\n\n");
        bool wait = 1;
        while (wait)
        {
            // break here
        }
    }

    if (Platform::GetSimulationMode() == Platform::RTL)
    {
        CHECK_RC(RTLSaveRestore(global_args));
    }

    //bug 1302801: Add brace here to gurantee simclk event destroyed before dump simclk profile,
    //             when run with 'quick_exit'.
    //And move some variable declarations out of brace, in case that they will be used in later condition block.
    {
		SimClk::EventWrapper event(SimClk::Event::MDIAG_RUN);
        if (SimClk::SimManager *pM = SimClk::SimManager::GetInstance())
        {
            Gpu::LwDeviceId devId = static_cast<Gpu::LwDeviceId>(0);
            GpuSubdevice* pSubdev = nullptr;
            GpuDevMgr* pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
            if (pGpuDevMgr)
                pSubdev = pGpuDevMgr->GetFirstGpu();
            if (pSubdev)
                devId = pSubdev->DeviceId();
            pM->SetDevID(devId);
        }

        auto mdiagInitScope = make_unique<ChiplibOpScope>("Mdiag Init", NON_IRQ,
                                                          ChiplibOpScope::SCOPE_COMMON, nullptr);

        Tee::SaveLevels saveLevels;
        if (global_args.ParamPresent("-output_level"))
        {
            UINT32 level = global_args.ParamUnsigned("-output_level");
            switch(level)
            {
                case 0:
                    Tee::SetScreenLevel(Tee::LevNone);
                    Tee::SetFileLevel(Tee::LevNone);
                    break;
                case 1:
                    Tee::SetScreenLevel(Tee::LevHigh);
                    Tee::SetFileLevel(Tee::LevHigh);
                    break;
                case 2:
                    Tee::SetScreenLevel(Tee::LevNormal);
                    Tee::SetFileLevel(Tee::LevNormal);
                    break;
                case 3:
                    Tee::SetScreenLevel(Tee::LevDebug);
                    Tee::SetFileLevel(Tee::LevDebug);
                    break;
                default:
                    Tee::SetScreenLevel(Tee::LevNormal);
                    Tee::SetFileLevel(Tee::LevNormal);
                    break;
            }
        }

        if (Tee::WillPrint(Tee::PriDebug))
        {
            Sys::SetThreadIdPrintEnable( true );
        }

        if (global_args.ParamPresent("-outputfilename"))
        {
            TestStateReport::setGlobalOutputName(global_args.ParamStr("-outputfilename", "test"));
        }

        // Process the XML specification arguments now, and open up any XML output file
        //  needed.  This is done very early in the process so that XML output is
        //  available.

        int                       ninc;
        bool                      xml_compress_requested = false;
        string                    xmlFileName = string(global_args.ParamStr("-outputfilename", "mdiag"))+".xml";
        Mdiag_XML::stringList     xmlGroups;
        bool                      xmlGroupsAreIncludes = false;
        bool                      xmlMultipleFileSpecifierSeen = false;
        bool                      xmlOutputNeeded = false;

        if (global_args.ParamPresent("-xc")) {
            xml_compress_requested = true;
        }
        if (global_args.ParamPresent("-xs")) {
            xmlOutputNeeded = true;
            xmlGroupsAreIncludes = true;
            xmlGroups.push_back("Standard");
        }
        if (global_args.ParamPresent("-xinclude")) {
            if (xmlOutputNeeded)
                xmlMultipleFileSpecifierSeen = true;
            xmlOutputNeeded = true;
            ninc = global_args.ParamNArgs("-xinclude");
            for (int i=0; i<ninc; i++)
                xmlGroups.push_back(global_args.ParamNStr("-xinclude", i));
            xmlGroupsAreIncludes = true;
        }
        if (global_args.ParamPresent("-xall")) {
            if (xmlOutputNeeded)
                xmlMultipleFileSpecifierSeen = true;
            xmlOutputNeeded = true;
            xmlGroups.clear();
            xmlGroupsAreIncludes = false;
        }
        if (global_args.ParamPresent("-xexclude")) {
            if (xmlOutputNeeded)
                xmlMultipleFileSpecifierSeen = true;
            xmlOutputNeeded = true;
            ninc = global_args.ParamNArgs("-xexclude");
            for (int i=0; i<ninc; i++)
                xmlGroups.push_back(global_args.ParamNStr("-xexclude", i));
        }
        if (global_args.ParamPresent("-xf")) {
            if (!xmlOutputNeeded)
                xmlGroups.push_back("FBMI");
            xmlOutputNeeded = true;
            xmlFileName = global_args.ParamStr("-xf");
        }
        if (xmlMultipleFileSpecifierSeen) {
            fprintf(stderr, "ERROR: only one of '-xall', '-xs', '-xexclude' or '-xinclude' may be specified\n");
            return(1);

        }

        Utility::CleanupValue<Mdiag_XML*> globalXMLCleanup(gXML, nullptr);

        if (xmlOutputNeeded) {
            if (xml_compress_requested) {
                xmlFileName += ".gz";
            }
            XmlWriter.reset(new Mdiag_XML(xmlFileName, xmlGroupsAreIncludes,
                        xmlGroups,
                        xml_compress_requested));
            if (!XmlWriter->creation_succeeded) {
                fprintf(stderr, "ERROR: Unable to initialize XML logging facility\n");
                return(1);
            }

            gXML = XmlWriter.get();
            XD->XMLStartElement("<MDIAGLog");
            XD->endAttributes();
        }

        // vGpu Migration parse cmdline args.
        // This need be done before SMC init and vGpu Migration init.
        GetMDiagvGpuMigration()->ParseCmdLineArgs();

        // init stream generator
        RandomStream::CreateStreamGenerator(global_args.ParamUnsigned("-seed0", 0x1234),
                                            global_args.ParamUnsigned("-seed1", 0x5678),
                                            global_args.ParamUnsigned("-seed2", 0x9abc));

        int numResFound = LWGpuResource::ScanSystem(pArgs);
        if (numResFound < 0)
        {
            // write a "no resources" result to the trep file and return.
            if (global_args.ParamPresent("-trepfile") && !s_NoTrep)
            {
                Trep tempTrep;
                TestDirectory testdir(pArgs, &tempTrep);
                tempTrep.SetTrepFileName(global_args.ParamStr("-trepfile"));
                CHECK_RC(tempTrep.AppendTrepString("test #0: TEST_NO_RESOURCES\n"));
                CHECK_RC(tempTrep.AppendTrepString("summary = some tests failed\n"));
            }

            // Need to return OK here even though there's an error.  Otherwise
            // mods will return with a non-zero status which will cause testgen
            // to report ERROR_SIM no matter what the trep file says.
            return OK;
        }
        InfoPrintf("%d resources found.\n", numResFound);

        // write trep file
        unsigned long startTime = getLwrrentRealTime();
        if (global_args.ParamPresent("-trepfile") && !s_NoTrep)
        {
            trep.SetTrepFileName(global_args.ParamStr("-trepfile"));

            string buf = Utility::StrPrintf("start time = %ld\n", startTime);
            CHECK_RC(trep.AppendTrepString(buf.c_str()));

            GpuDevMgr    * pGpuDevMgr = (GpuDevMgr *)(DevMgrMgr::d_GraphDevMgr);
            GpuSubdevice * pSubdev;

            if (pGpuDevMgr != NULL)
            {
               for (GpuDevice * pGpuDev = pGpuDevMgr->GetFirstGpuDevice();
                       pGpuDev != NULL; pGpuDev = pGpuDevMgr->GetNextGpuDevice(pGpuDev))
               {
                   for (UINT32 subdevInst = 0;
                           subdevInst < pGpuDev->GetNumSubdevices(); subdevInst++)
                   {
                       pSubdev = pGpuDev->GetSubdevice(subdevInst);

                       if (pGpuDevMgr->NumGpus() > 1)
                       {
                           string s = Utility::StrPrintf(
                                                     "DB_INFO: dev.subdev  = %d.%d\n",
                                                     pGpuDev->GetDeviceInst(),
                                                     subdevInst);
                           CHECK_RC(trep.AppendTrepString(s.c_str()));
                       }

                       CHECK_RC(DumpTrepGpuSubdeviceData(&global_args, &trep, pSubdev));
                       CHECK_RC(trep.AppendTrepString("\n"));
                       CHECK_RC(DumpTrepAdditionalChipData(&global_args, &trep, pSubdev));

                       if (pGpuDevMgr->NumGpus() > 1)
                           CHECK_RC(trep.AppendTrepString("\n"));
                   }

               }
            }
        }

        // Alloc iommu vaspace
        GpuDevMgr    * pGpuDevMgr = (GpuDevMgr *)(DevMgrMgr::d_GraphDevMgr);
        GpuSubdevice * pSubdev;
        if (pGpuDevMgr != NULL)
        {
            for (GpuDevice * pGpuDev = pGpuDevMgr->GetFirstGpuDevice();
                pGpuDev != NULL; pGpuDev = pGpuDevMgr->GetNextGpuDevice(pGpuDev))
            {
                for (UINT32 subdevInst = 0;
                    subdevInst < pGpuDev->GetNumSubdevices(); subdevInst++)
                {
                    pSubdev = pGpuDev->GetSubdevice(subdevInst);
                    auto pGpuPcie = pSubdev->GetInterface<Pcie>();

                    // Mdiag cannot detect whether this is an ats test
                    // since as_ats_map is handled in Mdiagsurf layer.
                    // Create Iommu object for all cases
                    // because Iommu initialization should be done here.
                    IommuDrv * iommu = IommuDrv::GetIommuDrvPtr();
                    if (iommu)
                    {
                        CHECK_RC(iommu->AllocDevVaSpace(pGpuPcie));
                    }
                }
            }
        }

        // The mdiag Thread object is a wrapper around Tasker threads.
        // These Thread objects form a hierarchy that allows for parent and
        // child threads.  There must be a single root thread that is allocated
        // by using the default Thread constructor.  All other threads are
        // created as a child of another thread.  Both the TestDirectory
        // object and the Utl object can create child threads by calling
        // the Thread::Fork function.
        unique_ptr<Thread> rootThread(new Thread());

        // Initialize UTL if a Utl object exists.
        if (Utl::HasInstance())
        {
            mdiagInitScope.reset();

            rc = Utl::Instance()->Init();

            if (OK != rc)
            {
                ErrPrintf("Utl::Init failed.\n");
                Utl::Shutdown();
                return rc;
            }
        }
#ifdef INCLUDE_MDIAGUTL
        else if (global_args.ParamPresent("-utl_script"))
        {
            ErrPrintf("-utl_script was specified but this platform does not support UTL.\n");
            return RC::UNSUPPORTED_FUNCTION;
        }
#endif

        //Init SMC
        CHECK_RC(LWGpuResource::InitSmcResource());

        if (GetMDiagvGpuMigration()->IsEnabled())
        {
            // Init vGpu Migration
            CHECK_RC(GetMDiagvGpuMigration()->Init());
        }

        LWGpuResource::PrintSmcInfo();

        if (global_args.ParamPresent("-quick_exit"))
        {
            // Since amodel is supposed to be quick, it was requested that we have at least one environment which always
            // checks the shutdown codepaths.
            if ((Platform::GetSimulationMode() == Platform::Hardware && !Platform::IsTegra()) ||
                (Platform::GetSimulationMode() == Platform::Amodel && !Platform::IsSelfHosted()))
            {
                InfoPrintf("\"-quick_exit\" argument is not supported on amodel or dGPU hardware/emulation - ignoring\n");
            }
            else
            {
                quick_exit = true;
            }
        }

        // If UTL is responsible for running the tests, call its Run function
        // here and skip the normal mdiag test running flow.
        if (Utl::ControlsTests())
        {
            if (Platform::GetSimulationMode() == Platform::RTL)
            {
                Platform::EscapeWrite("leaving_section", 0, 4, TEST_SETUP_SECTION);
            }

            rc = Utl::Instance()->Run();

            if (OK != rc)
            {
                ErrPrintf("Utl::Run failed.\n");
                Utl::Shutdown();
                return rc;
            }
        }

        // Otherwise do the normal mdiag flow.
        else
        {
            // figure out list of tests to run
            // expect '-e test_string' or '-f test_file' - other args go into the db
            num_tests = 0;
            ArgReader loop_args(loop_params);
            loop_args.ParseArgs(pArgs, "");
            bool    strict_test_args = global_args.ParamPresent("-strict_test_args") > 0;

            // Verify lwlink chipargs are consistent with RM on silicon
            if (global_args.ParamPresent("-lwlink_set_verify_config_mode"))
            {
                VerifyLwlink(global_args);
            }

            const vector<string>* testCmdLines = global_args.ParamStrList("-e", NULL);
            bool TestsRunOnMRMI = MrmCppInfra::TestsRunningOnMRMI();
            if (testCmdLines && !testCmdLines->empty())
            {
                UINT32 numLoops;
                if (TestsRunOnMRMI)
                {
                    numLoops = MrmCppInfra::GetTestLoopsArg();
                }
                else
                {
                    numLoops = loop_args.ParamUnsigned("-loops");
                }
                if (numLoops == 0) numLoops = 1;
                CHECK_RC(LoadTests(&testdir, testCmdLines,
                        strict_test_args ? 0 : pArgs, numLoops));
                num_tests += numLoops * (UINT32)testCmdLines->size();
            }

            pMdiagElwMgr->SetNumT3dTests(num_tests);

            UINT64 endLoopMs = 0;
            if (loop_args.ParamPresent("-loop_ms"))
            {
                endLoopMs = (Platform::GetTimeMS() +
                    loop_args.ParamUnsigned("-loop_ms") + 1);
            }

            if (Platform::IsPhysFunMode())
            {
                // Start all Vf tests before running Pf test
                PolicyManager * pPolicyMgr = PolicyManager::Instance();
                CHECK_RC(pMdiagElwMgr->SetupAllVfTests(pArgs));
                if (!pPolicyMgr->GetStartVfTestInPolicyManager())
                {
                    CHECK_RC(pMdiagElwMgr->RulwfTests());
                }
            }

            if (pArgs->CheckForUnusedArgs(true))
            {
                ErrPrintf("Exiting due to bad cmdline arguments\n");
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }

            mdiagInitScope.reset();

            rc.Clear();
            if (num_tests == 0)
            {
                Printf(Tee::PriAlways, "No tests!\n");
            }
            else
            {
                if (global_args.ParamPresent("-check"))
                {
                    // just do one setup pass, and see which tests can set up ok
                    tests_set_up = testdir.SetupAllTests();
                    if (TestsRunOnMRMI)
                    {
                        CHECK_RC(MrmCppInfra::SetupDoneWaitingForOthers("MDIAG"));
                    }

                    InfoPrintf("%d test(s) (out of %d) set up successfully.\n", tests_set_up, num_tests);

                    CleanUpAllTests(&testdir, quick_exit);
                }
                else
                {
                    // As confirmed with RTl, most of performance tests are single trace tests.
                    if (Platform::GetSimulationMode() == Platform::RTL)
                        Platform::EscapeWrite("entering_section", 0, 4, TEST_SETUP_SECTION);

                    // For serial run, only set up one test
                    // For conlwrrent run, set up as many tests as possible.
                    InfoPrintf("%s: MDIAG: SetupAllTests( ) & get num of tests.\n", __FUNCTION__);
                    num_tests = testdir.SetupAllTests();
                    InfoPrintf("%s: MDIAG: SetupAllTests( ) done. num_tests = [%d].\n", __FUNCTION__, (UINT32)num_tests);

                    if (TestsRunOnMRMI)
                    {
                        InfoPrintf("%s: MDIAG: Setup Done for MDIAG; Waiting for others.\n", __FUNCTION__);
                        CHECK_RC(MrmCppInfra::SetupDoneWaitingForOthers("MDIAG"));
                    }

                    if (Platform::GetSimulationMode() == Platform::RTL)
                        Platform::EscapeWrite("leaving_section", 0, 4, TEST_SETUP_SECTION);

                    while (num_tests > 0)
                    {
                        string rmTestName = "";
                        if (TestsRunOnMRMI)
                        {
                            rmTestName = MrmCppInfra::GetRmTestName();
                            InfoPrintf("%s: MDIAG: rmTestName = '%s'.\n", __FUNCTION__, rmTestName.c_str());

                            if (rmTestName.compare("Modeset") == 0)
                            {
                                InfoPrintf("%s: MDIAG: WaitOnEvent(Modeset).\n", __FUNCTION__);
                                CHECK_RC(MrmCppInfra::WaitOnEvent("Modeset"));

                                InfoPrintf("%s: MDIAG: ResetEvent(Modeset).\n", __FUNCTION__);
                                CHECK_RC(MrmCppInfra::ResetEvent("Modeset"));

                                // before waiting for any event ensure that the Modeset test isnt ended
                                // else we might wait infinitely for the event.
                                if (MrmCppInfra::IsEventSet("ModesetTestDone"))
                                {
                                    InfoPrintf("%s: MDIAG: ModesetTestDone event is set.Freeing all events.\n", __FUNCTION__);
                                    MrmCppInfra::FreeAllEvents();
                                    break;
                                }

                                InfoPrintf("%s: MDIAG: WaitOnEvent(ClockSwitch).\n", __FUNCTION__);
                                CHECK_RC(MrmCppInfra::WaitOnEvent("ClockSwitch"));

                                InfoPrintf("%s: MDIAG: ResetEvent(ClockSwitch).\n", __FUNCTION__);
                                CHECK_RC(MrmCppInfra::ResetEvent("ClockSwitch"));
                            }

                            // before waiting for any event ensure that the Modeset test isnt ended
                            // else we might wait infinitely for the event.
                            if (MrmCppInfra::IsEventSet("ModesetTestDone"))
                            {
                                InfoPrintf("%s: MDIAG: ModesetTestDone event is set.Freeing all events.\n", __FUNCTION__);
                                MrmCppInfra::FreeAllEvents();
                                break;
                            }
                        }

                        do
                        {
                            InfoPrintf("INFO:%s: MDIAG: %d tests set up...\n", __FUNCTION__, num_tests);
                            PostRunSemaphore();

                            // run them (all at the same time)
                            InfoPrintf("%s: MDIAG: Running all tests.\n", __FUNCTION__);
                            testdir.RunAllTests();

                            // Clean up all tests that ran
                            InfoPrintf("%s: MDIAG: CleanUp all tests.\n", __FUNCTION__);
                            CleanUpAllTests(&testdir, quick_exit);

                            InfoPrintf("%s: MDIAG: SetupAllTests( ) & get num of tests.\n", __FUNCTION__);
                            num_tests = testdir.SetupAllTests();
                            InfoPrintf("%s: MDIAG: num_tests = [%d].\n", __FUNCTION__, (UINT32)num_tests);
                        } while (num_tests > 0);

                        if (TestsRunOnMRMI && (rmTestName.compare("Modeset") == 0))
                        {
                            InfoPrintf("%s: MDIAG: ReportResultsAndClean().\n", __FUNCTION__);
                            testdir.ReportResultsAndClean();
                            if (MrmCppInfra::IsEventSet("ModesetTestDone"))
                            {
                                InfoPrintf("%s: MDIAG: ModesetTestDone event is set.Freeing all events.\n", __FUNCTION__);
                                MrmCppInfra::FreeAllEvents();
                                break;
                            }

                            InfoPrintf("%s: MDIAG: WaitOnEvent(ClockSwitchDone).\n", __FUNCTION__);
                            CHECK_RC(MrmCppInfra::WaitOnEvent("ClockSwitchDone"));

                            InfoPrintf("%s: MDIAG: ResetEvent(ClockSwitchDone).\n", __FUNCTION__);
                            CHECK_RC(MrmCppInfra::ResetEvent("ClockSwitchDone"));

                            InfoPrintf("%s: MDIAG: SetEvent(TraceDone).\n", __FUNCTION__);
                            CHECK_RC(MrmCppInfra::SetEvent("TraceDone"));

                            InfoPrintf("%s: MDIAG: LoadTests().\n", __FUNCTION__);
                            CHECK_RC(LoadTests(&testdir, testCmdLines,
                                    strict_test_args ? 0 : pArgs,
                                    loop_args.ParamUnsigned("-loops")));

                            InfoPrintf("%s: MDIAG: SetupAllTests( ) & get num of tests.\n", __FUNCTION__);
                            num_tests = testdir.SetupAllTests();
                            InfoPrintf("%s: MDIAG: num_tests-2 = [%d].\n", __FUNCTION__, (UINT32)num_tests);
                        }

                        if ((num_tests == 0) && (Platform::GetTimeMS() < endLoopMs))
                        {
                            CHECK_RC(LoadTests(&testdir, testCmdLines,
                                    strict_test_args ? 0 : pArgs,
                                    loop_args.ParamUnsigned("-loops")));
                            num_tests = testdir.SetupAllTests();
                        }
                    }
                }

                // Here all tests are done. Process all the pending interrupts to workaround the known wfi_intr issue
                // For wfi_intr, it treats next to last interrupt as the expected interrupt by mistake.
                // It causes no RM ISR to process the last interrupt during shut down gpu.
                Platform::ProcessPendingInterrupts();

                InfoPrintf("%s: MDIAG: ReportTestResults().\n", __FUNCTION__);

                // If UTL is active, trigger an EndEvent.
                if (Utl::HasInstance())
                {
                    rc = Utl::Instance()->TriggerEndEvent();

                    if (OK != rc)
                    {
                        Utl::Shutdown();
                        return rc;
                    }
                }

                rc = testdir.ReportTestResults();

                if (TestsRunOnMRMI)
                {
                    InfoPrintf("%s: MDIAG: TestDoneWaitingForOthers(MDIAG).\n", __FUNCTION__);
                    CHECK_RC(MrmCppInfra::TestDoneWaitingForOthers("MDIAG"));
                }
            }
        }

        // All child threads should be finished at this point, so delete
        // the root thread.
        rootThread.reset(nullptr);

        DebugPrintf("%s: MDIAG: RandomStream::DestroyStreamGenerator().\n", __FUNCTION__);
        RandomStream::DestroyStreamGenerator();

        if (global_args.ParamPresent("-trepfile") && !s_NoTrep)
        {
            DebugPrintf("%s: MDIAG: trepfile specified. Adding exelwtion time to trep file.\n", __FUNCTION__);
            unsigned long endTime = getLwrrentRealTime();
            string buf = Utility::StrPrintf("exelwtion time = %ld\n", endTime - startTime);
            trep.AppendTrepString(buf.c_str());
        }

        if (s_GpuThreadThreadID != Tasker::NULL_THREAD)
        {
            RC rc;
            GpuPtr pGpu;
            InfoPrintf("%s: MDIAG: ThreadShutdown.\n", __FUNCTION__);
            CHECK_RC(pJs->CallMethod(pJs->GetGlobalObject(), pGpu->ThreadShutdown()));
        }
    }

    RC oldRc = rc;
    rc.Clear();
    rc = LWGpuResource::RegisterOperationsResources();
    if (rc != OK)
    {
        ErrPrintf("%s: MDIAG: reg_eos check mismatch.\n", __FUNCTION__);
        int num_tests = (UINT32)testdir.GetNumTests();
        for (int testNum = 0; testNum < num_tests; ++testNum)
        {
            string buf = Utility::StrPrintf("test #%d: TEST_FAILED (reg_eos check mismatch)\n", testNum);
            CHECK_RC(trep.AppendTrepString(buf.c_str()));
        }
    }
    else
    {
        InfoPrintf("%s: MDIAG: reg_eos check match.\n", __FUNCTION__);
    }


    if (Platform::IsPhysFunMode())
    {
        // Wait all Vf tests done before exiting
        CHECK_RC(pMdiagElwMgr->WaitAllPluginThreadsDone());
    }

    if (!s_NoTrep)
        rc.Clear(); // caller doesn't want to report results
    else
        rc = (rc != OK) ? rc : oldRc; // return one of error rc value 

    if (quick_exit)
    {
        InfoPrintf("%s: MDIAG: quick_exit specified. Exit Mods.\n", __FUNCTION__);
        //Add support to data store/dump when run with 'quick_exit'
        if (SimClk::SimManager *pM = SimClk::SimManager::GetInstance())
        {
            pM->StoreAndVerify();
        }
        XmlWriter.reset(0);
        Utility::ExitMods(rc, Utility::ExitQuickAndDirty);
    }

    Utl::Shutdown();
    IommuDrv::Destroy();
    EngineClasses::FreeEngineClassesObjs();
    PolicyManager::ShutDown();
    MmuLevelTreeManager::ShutDown();

    return rc;
}

RC Mdiag::DescribeArgs(const string &TestName)
{

    // As of 7/6/2007 - If user does not supply arg after --args, TestName
    //    is set to undefined when this exelwtes.
    bool isMissingArg = ( TestName == "undefined" );

    char usage[] =
     "Usage: mods mdiag.js --args [ all | mdiag | <resource_name> | <test_name> ]\n";

    bool isAll = ( 0 == stricmp(TestName.c_str(), "all") );

    // For "--args all" or "--args mdiag", print the parameter description
    //   for mdiag
    bool doMdiag = isAll ||
                      ( 0 == stricmp(TestName.c_str(), "mdiag") );

    // Add a separator between the three sections when reporting all
    char separator[] = "---------------------------------------------------\n";

    if ( isMissingArg ) {
        RawPrintf("Invocation Error: Missing last argument.\n");
        RawPrintf(usage);
        return OK;
    }

    if (isAll) {
        RawPrintf(separator);
    }

    if ( doMdiag ) {
        RawPrintf(" mdiag     (global parameters):\n");
        ArgReader::DescribeParams(global_params);
    }

    if (isAll) {
        RawPrintf(separator);
    }

    // These will print either one actual parameter description or
    // will print all resource and all test parameter if "--args all"
    LWGpuResource::DescribeResourceParams(TestName.c_str());

    if (isAll) {
        RawPrintf(separator);
    }

    TestDirectory::DescribeTestParams(TestName.c_str());

    return OK;
}

RC Mdiag::DescribeTests()
{
    ArgDatabase Args;
    Trep trep;
    TestDirectory TestDir(&Args, &trep);

    InfoPrintf("Available tests (* = supports -forever):\n");
    TestDir.ListTests(0);

    return OK;
}

RC Mdiag::WriteEarlyRegFields(UINT32 devInst, UINT32 subdevInst, const string args)
{
    GpuDevMgr *pGpuDevMgr = (GpuDevMgr *)(DevMgrMgr::d_GraphDevMgr);
    Device *pGpudev = nullptr;
    RC rc = pGpuDevMgr->GetDevice(devInst, &pGpudev);
    MASSERT(rc == OK);
    MASSERT(subdevInst < dynamic_cast<GpuDevice*>(pGpudev)->GetNumSubdevices());
    GpuSubdevice* subdev = dynamic_cast<GpuDevice*>(pGpudev)->GetSubdevice(subdevInst);

    vector<string> argArray;
    stringstream ss(args);
    string regspace, regname, fieldname, valname;
    UINT32 valnum;

    RefManual* pManual = subdev->GetRefManual();
    unique_ptr<IRegisterMap> regmap = CreateRegisterMap(pManual);

    LwRm* pLwRm = LwRmPtr().Get();
    while (ss >> regspace)
    {
        ss >> regname;
        ss >> fieldname;
        ss >> valname;

        unique_ptr<IRegisterClass> ireg = regmap->FindRegister(regname.c_str());
        if (!ireg)
        {
            ErrPrintf("WriteEarlyRegFields: Register name not found: %s\n", regname.c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        string absFieldname = string(ireg->GetName()) + fieldname;
        unique_ptr<IRegisterField> ifield = ireg->FindField(absFieldname.c_str());
        if (!ifield)
        {
            ErrPrintf("WriteEarlyRegFields: Field name not found: %s\n", fieldname.c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        if (isdigit(valname[0]))
        {
            valnum = strtoul(valname.c_str(), nullptr, 16);
        }
        else
        {
            string absValname = absFieldname + valname;
            unique_ptr<IRegisterValue> ivalue = ifield->GetValueHead();
            if (!ivalue)
            {
                ErrPrintf("WriteEarlyRegFields: Value name not found: %s\n", valname.c_str());
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
            valnum = ivalue->GetValue();
        }

        UINT32 mask = ifield->GetWriteMask();
        UINT32 regaddr =
            MDiagUtils::GetDomainBase(regname.c_str(), regspace.c_str(), subdev, pLwRm) +
            ireg->GetAddress();
        UINT32 regval = subdev->RegRd32(regaddr);
        regval = (regval & ~mask) | ((valnum << ifield->GetStartBit()) & mask);
        InfoPrintf("WriteEarlyRegFields: Writing value 0x%x to register field %s\n", valnum, absFieldname.c_str());
        subdev->RegWr32(regaddr, regval);
    }
    return OK;
}

// Used by Mdiag::Run() to load tests
RC Mdiag::LoadTests
(
    TestDirectory *pTestdir,
    const vector<string> *pTestCmdLines,
    ArgDatabase *pArgs,
    UINT32 numLoops
)
{
    if (pTestCmdLines == NULL || pTestCmdLines->empty())
        return OK;

    if (numLoops == 0)
        numLoops = 1;

    for (UINT32 index = 0; index < numLoops; index++)
    {
        for (auto testCmdLwr = pTestCmdLines->begin();
             testCmdLwr != pTestCmdLines->end();
             ++testCmdLwr)
        {
            string newStr = testCmdLwr->c_str();

            Test *new_test = pTestdir->ParseTestLine(newStr.c_str(), pArgs);
            if (!new_test)
            {
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
        }
    }
    return OK;
}

struct clkInfo
{
    Gpu::ClkDomain   ModsDomain;
    const char *     TrepDomainName;
    bool             DisplayAsDiv2;
};
RC Mdiag::DumpTrepGpuSubdeviceData(ArgReader *args, Trep* pTrep,
                                   GpuSubdevice *pSubdev)
{
    RC rc;

    string lwrString;

    if (Platform::IsPhysFunMode() || Platform::IsDefaultMode())
    {
        const clkInfo clkInfos[] =
        {
            { Gpu::ClkM,       "dramclk",  false }
            ,{ Gpu::ClkHost,    "hostclk",  false }
            ,{ Gpu::ClkDisp,    "dispclk",  false }
            ,{ Gpu::ClkPA,      "paclk",    false }
            ,{ Gpu::ClkPB,      "pbclk",    false }
            ,{ Gpu::ClkX,       "xclk",     false }
            ,{ Gpu::ClkGpc,     "gpcclk",   true  }
            ,{ Gpu::ClkLtc,     "ltcclk",   true  }
            ,{ Gpu::ClkXbar,    "xbarclk",  true  }
            ,{ Gpu::ClkSys,     "sysclk",   true  }
            ,{ Gpu::ClkHub,     "hubclk",   true  }
            ,{ Gpu::ClkLeg,     "legclk",   false }
            ,{ Gpu::ClkUtilS,   "utlSclk",  false }
            ,{ Gpu::ClkPwr,     "pwrclk",   false }
            ,{ Gpu::ClkMSD,     "MSDclk",   false }
        };

        for (UINT32 i = 0; i < NUMELEMS(clkInfos); i++)
        {
            UINT64 actualClk;
            if (!GpuPtr()->IsInitSkipped() &&
                (pSubdev->HasDomain(clkInfos[i].ModsDomain)) &&
                (OK == pSubdev->GetClock(clkInfos[i].ModsDomain, &actualClk,
                                        NULL, NULL, NULL)))
            {
                // Arch doesn't want to see gpc2clk, they want gpcclk.
                if (!clkInfos[i].DisplayAsDiv2)
                {
                    actualClk = actualClk * 2;
                }

                lwrString = Utility::StrPrintf("DB_INFO: %-7s = %llu\n",
                        clkInfos[i].TrepDomainName, actualClk);
            }
            else
            {
                lwrString = Utility::StrPrintf("DB_INFO: %-7s = unknown\n",
                        clkInfos[i].TrepDomainName);
            }
            CHECK_RC(pTrep->AppendTrepString(lwrString.c_str()));
        }
    }

    lwrString = Utility::StrPrintf("DB_INFO: swcl    = %d\n", g_Changelist);
    CHECK_RC(pTrep->AppendTrepString(lwrString.c_str()));

    if (!GpuPtr()->IsInitSkipped())
    {
        lwrString = "DB_INFO: vbios   = " + pSubdev->GetRomVersion() + "\n";
        CHECK_RC(pTrep->AppendTrepString(lwrString.c_str()));

        LwRmPtr pLwRm;
        LW0000_CTRL_SYSTEM_GET_BUILD_VERSION_PARAMS params;
        memset(&params, 0, sizeof(params));

        // First learn how much memory to allocate for the strings.
        CHECK_RC(pLwRm->Control(
                    pLwRm->GetClientHandle(),
                    LW0000_CTRL_CMD_SYSTEM_GET_BUILD_VERSION,
                    &params,
                    sizeof(params)));

        // Alloc memory for the strings and colwert to LwP64 to make the char *
        // compatible with 64 bit systems.
        vector<char> VersionBuffer(params.sizeOfStrings, 0);
        vector<char> TitleBuffer(params.sizeOfStrings, 0);
        vector<char> DriverVersionBuffer(params.sizeOfStrings, 0);
        params.pVersionBuffer = LW_PTR_TO_LwP64(&VersionBuffer[0]);
        params.pTitleBuffer   = LW_PTR_TO_LwP64(&TitleBuffer[0]);
        params.pDriverVersionBuffer = LW_PTR_TO_LwP64(&DriverVersionBuffer[0]);

        // Call again to get the strings.
        CHECK_RC(pLwRm->Control(
                    pLwRm->GetClientHandle(),
                    LW0000_CTRL_CMD_SYSTEM_GET_BUILD_VERSION,
                    &params,
                    sizeof(params)));

        MASSERT(TitleBuffer[0] != 0);

        lwrString = Utility::StrPrintf("DB_INFO: rm      = %s\n",
                                 (const char*) LwP64_VALUE(params.pTitleBuffer));
        CHECK_RC(pTrep->AppendTrepString(lwrString.c_str()));

        vector<UINT32> Ecids;
        pSubdev->ChipId(&Ecids);
        lwrString = "DB_INFO: ECID    = 0x";
        for (UINT32 i = 0; i < Ecids.size(); i++)
        {
            char EcidWord[10];
            sprintf(EcidWord, "%08x", Ecids[i]);
            lwrString += EcidWord;
        }
        lwrString += "\n";
        CHECK_RC(pTrep->AppendTrepString(lwrString.c_str()));
    }
    else
    {
        CHECK_RC(pTrep->AppendTrepString("DB_INFO: vbios   = unknown\n"));
        CHECK_RC(pTrep->AppendTrepString("DB_INFO: rm      = unknown\n"));
        CHECK_RC(pTrep->AppendTrepString("DB_INFO: ECID    = unknown\n"));
    }

    UINT32 gpuLockdownStatus = 0; // 0 = GPU not locked down
    if (GpuPtr()->IsInitSkipped() && 
        Platform::GetSimulationMode() == Platform::RTL)
    {
        Platform::EscapeRead("gpu_lockdown_status", 0, sizeof(gpuLockdownStatus), &gpuLockdownStatus);
    }

    if (gpuLockdownStatus == 0)
    {
        map<string, UINT32> fsMasks;
        if (pSubdev->GetFsImpl()->GetFloorsweepingMasks(&fsMasks) == OK)
        {
            map<string, UINT32>::const_iterator cit;
            for (cit = fsMasks.begin(); cit != fsMasks.end(); cit++)
            {
                string maskinfo = Utility::StrPrintf("DB_INFO: %-16s = 0x%x\n",
                    cit->first.c_str(), cit->second);
                CHECK_RC(pTrep->AppendTrepString(maskinfo.c_str()));
            }
        }
    }

    return OK;
}

RC Mdiag::DumpTrepAdditionalChipData(ArgReader *args, Trep *trep, GpuSubdevice *subdev)
{
    RC rc;

    if (GpuPtr()->IsInitSkipped())
    {
        CHECK_RC(trep->AppendTrepString("Chip not initialized. Won't dump additional chip info.\n"));
        return OK;
    }
    if (!subdev)
    {
        CHECK_RC(trep->AppendTrepString("Invalid GPU subdevice. Won't dump additional chip info.\n"));
        return OK;
    }
    FrameBuffer *fb = subdev->GetFB();
    if (!fb)
    {
        CHECK_RC(trep->AppendTrepString("Invalid GPU framebuffer. Won't dump additional chip info.\n"));
        return OK;
    }

    string line;
    line = "Full Name: " + subdev->DeviceName() + "\n";
    CHECK_RC(trep->AppendTrepString(line));

    line = "Short Name: " + subdev->DeviceShortName() + "\n";
    CHECK_RC(trep->AppendTrepString(line));

    UINT32 revision = 0;
    CHECK_RC(subdev->GetChipRevision(&revision));
    line = Utility::StrPrintf("Chip Arch/Impl/Rev: %02x / %02x / %02x"
        "    MinorRev: ",
        subdev->Architecture(), subdev->Implementation(),
        revision);
    if (subdev->IsExtMinorRevisiolwalid())
    {
        line += Utility::StrPrintf("%d\n", subdev->ExtMinorRevision());
    }
    else
    {
        line += "<ERROR>\n";
    }
    CHECK_RC(trep->AppendTrepString(line));

    if (subdev->IsNetlistRevision1Valid())
    {
        line = Utility::StrPrintf("Netlist Metadata ID: %d\n",
            subdev->NetlistRevision1());
    }
    else
    {
        line = "Netlist Metadata ID: <ERROR>\n";
    }
    CHECK_RC(trep->AppendTrepString(line));

    UINT32 vbiosVersion = subdev->VbiosRevision();
    UINT32 vbiosOEMVersion = subdev->VbiosOEMRevision();
    line = Utility::StrPrintf("Vbios: %x.%02x.%02x.%02x.%02x\n",
        ((vbiosVersion >> 24) & 0xff),
        ((vbiosVersion >> 16) & 0xff),
        ((vbiosVersion >> 8) & 0xff),
        ((vbiosVersion) & 0xff),
        (vbiosOEMVersion & 0xff));
    CHECK_RC(trep->AppendTrepString(line));

    line = Utility::StrPrintf("Lwdqro State: %s\n",
        subdev->IsQuadroEnabled() ? "GL" : "Non-GL");
    CHECK_RC(trep->AppendTrepString(line));

    line = Utility::StrPrintf("Physical VRAM: %llu\n",
        (fb->GetGraphicsRamAmount() + (1 << 19)) >> 20);
    CHECK_RC(trep->AppendTrepString(line));

    FrameBuffer::RamProtocol protocol = fb->GetRamProtocol();
    string protocolStr;
    switch (protocol)
    {
        case FrameBuffer::RamSdram:
            protocolStr = "SDRAM";
            break;
        case FrameBuffer::RamDDR1:
            protocolStr = "DDR1";
            break;
        case FrameBuffer::RamDDR2:
            protocolStr = "DDR2";
            break;
        case FrameBuffer::RamDDR3:
            protocolStr = "DDR3";
            break;
        case FrameBuffer::RamSDDR4:
            protocolStr = "SDDR4";
            break;
        case FrameBuffer::RamGDDR2:
            protocolStr = "GDDR2";
            break;
        case FrameBuffer::RamGDDR3:
            protocolStr = "GDDR3";
            break;
        case FrameBuffer::RamGDDR4:
            protocolStr = "GDDR4";
            break;
        case FrameBuffer::RamGDDR5:
            protocolStr = "GDDR5";
            break;
        case FrameBuffer::RamGDDR5X:
            protocolStr = "GDDR5X";
            break;
        case FrameBuffer::RamGDDR6:
            protocolStr = "GDDR6";
            break;
        case FrameBuffer::RamGDDR6X:
            protocolStr = "GDDR6X";
            break;
        case FrameBuffer::RamLPDDR2:
            protocolStr = "LPDDR2";
            break;
        case FrameBuffer::RamLPDDR3:
            protocolStr = "LPDDR3";
            break;
        case FrameBuffer::RamLPDDR4:
            protocolStr = "LPDDR4";
            break;
        case FrameBuffer::RamHBM1:
            protocolStr = "HBM1";
            break;
        case FrameBuffer::RamHBM2:
            protocolStr = "HBM2";
            break;
        case FrameBuffer::RamUnknown:
            protocolStr = Utility::StrPrintf("unknown(%d)", protocol);
            break;
        default:
            protocolStr = Utility::StrPrintf("unrecognized(%d)", protocol);
            break;
    }
    line = "VRAM Type: " + protocolStr + "\n";
    CHECK_RC(trep->AppendTrepString(line));

    FrameBuffer::RamVendorId vendorId = fb->GetVendorId();
    string vendorIdStr;
    switch (vendorId)
    {
        case FrameBuffer::RamSAMSUNG:
            vendorIdStr = "SAMSUNG";
            break;
        case FrameBuffer::RamQIMONDA:
            vendorIdStr = "QIMONDA";
            break;
        case FrameBuffer::RamELPIDA:
            vendorIdStr = "ELPIDA";
            break;
        case FrameBuffer::RamETRON:
            vendorIdStr = "ETRON";
            break;
        case FrameBuffer::RamNANYA:
            vendorIdStr = "NANYA";
            break;
        case FrameBuffer::RamHYNIX:
            vendorIdStr = "HYNIX";
            break;
        case FrameBuffer::RamMOSEL:
            vendorIdStr = "MOSEL";
            break;
        case FrameBuffer::RamWINBOND:
            vendorIdStr = "WINBOND";
            break;
        case FrameBuffer::RamESMT:
            vendorIdStr = "ESMT";
            break;
        case FrameBuffer::RamMICRON:
            vendorIdStr = "MICRON";
            break;
        case FrameBuffer::RamVendorUnknown:
            vendorIdStr = Utility::StrPrintf("UNKNOWN(%d)", vendorId);
            break;
        default:
            vendorIdStr = Utility::StrPrintf("unrecognized(%d)", vendorId);
            break;
    }
    line = "VRAM Vendor: " + vendorIdStr + "\n";
    CHECK_RC(trep->AppendTrepString(line));

    line = Utility::StrPrintf("VRAM Bus Width: %d\n", fb->GetBusWidth());
    CHECK_RC(trep->AppendTrepString(line));

    line = Utility::StrPrintf("GPU IRQ: %d\n", subdev->GetIrq());
    CHECK_RC(trep->AppendTrepString(line));

    UINT32 busType = subdev->BusType();
    string busTypeStr;
    switch(busType)
    {
        case LW2080_CTRL_BUS_INFO_TYPE_PCI:
            busTypeStr = "PCI";
            break;
        case LW2080_CTRL_BUS_INFO_TYPE_PCI_EXPRESS:
            busTypeStr = "PCIE";
            break;
        case LW2080_CTRL_BUS_INFO_TYPE_FPCI:
            busTypeStr = "FPCI";
            break;
        case LW2080_CTRL_BUS_INFO_TYPE_AXI:
            busTypeStr = "AXI";
            break;
        default:
            busTypeStr = "PCI";
            break;
    }
    line = "GPU Bus Type: " + busTypeStr + "\n";
    CHECK_RC(trep->AppendTrepString(line));

    if (subdev->IsEcidLo32Valid() &&
        subdev->IsEcidHi32Valid())
    {
        vector<UINT32> chipIdVec;
        CHECK_RC(subdev->ChipId(&chipIdVec));
        UINT64 serial = (UINT64(chipIdVec[1]) << 32) | chipIdVec[2];
        line = Utility::StrPrintf("GPU Serial: %016llx\n", serial);
    }
    else
    {
        line = "GPU Serial: <ERROR>\n";
    }
    CHECK_RC(trep->AppendTrepString(line));

    UINT32 foundry = subdev->Foundry();
    string foundryStr;
    switch(foundry)
    {
        case LW2080_CTRL_GPU_INFO_FOUNDRY_TSMC:
            foundryStr = "TSMC";
            break;
        case LW2080_CTRL_GPU_INFO_FOUNDRY_UMC:
            foundryStr = "UMC";
            break;
        case LW2080_CTRL_GPU_INFO_FOUNDRY_IBM:
            foundryStr = "IBM";
            break;
        case LW2080_CTRL_GPU_INFO_FOUNDRY_SMIC:
            foundryStr = "SMIC";
            break;
        case LW2080_CTRL_GPU_INFO_FOUNDRY_CHARTERED:
            foundryStr = "CHARTERED";
            break;
        case LW2080_CTRL_GPU_INFO_FOUNDRY_TOSHIBA:
            foundryStr = "TOSHIBA";
            break;
        default:
            foundryStr = "<unknown>";
            break;
    }
    line = "GPU Foundry: " + foundryStr + "\n";
    CHECK_RC(trep->AppendTrepString(line));

    string sampleTypeStr;
    if (subdev->IsSampleTypeValid())
    {
        UINT32 sampleType = subdev->SampleType();
        switch(sampleType)
        {
            case LW2080_CTRL_GPU_INFO_SAMPLE_NONE:
                sampleTypeStr = "NONE";
                break;
            case LW2080_CTRL_GPU_INFO_SAMPLE_ES:
                sampleTypeStr = "ES";
                break;
            case LW2080_CTRL_GPU_INFO_SAMPLE_QS:
                sampleTypeStr = "QS";
                break;
            case LW2080_CTRL_GPU_INFO_SAMPLE_PS:
                sampleTypeStr = "PS";
                break;
            case LW2080_CTRL_GPU_INFO_SAMPLE_QS_PS_PROD:
                sampleTypeStr = "QS_PS_PROD";
                break;
            default:
                sampleTypeStr = "<unknown>";
                break;
        }
    }
    else
    {
        sampleTypeStr = "<ERROR>";
    }
    line = "Sample Type: " + sampleTypeStr + "\n";
    CHECK_RC(trep->AppendTrepString(line));

    string hwQualTypeStr;
    if (subdev->IsHwQualTypeValid())
    {
        UINT32 hwQualType = subdev->HQualType();
        switch(hwQualType)
        {
            case LW2080_CTRL_GPU_INFO_HW_QUAL_TYPE_NONE:
                hwQualTypeStr = "NONE";
                break;
            case LW2080_CTRL_GPU_INFO_HW_QUAL_TYPE_NOMINAL:
                hwQualTypeStr = "NOMINAL";
                break;
            case LW2080_CTRL_GPU_INFO_HW_QUAL_TYPE_SLOW:
                hwQualTypeStr = "SLOW";
                break;
            case LW2080_CTRL_GPU_INFO_HW_QUAL_TYPE_FAST:
                hwQualTypeStr = "FAST";
                break;
            case LW2080_CTRL_GPU_INFO_HW_QUAL_TYPE_HIGH_LEAKAGE:
                hwQualTypeStr = "HIGH_LEAKAGE";
                break;
            default:
                hwQualTypeStr = "<unknown>";
                break;
        }
    }
    else
    {
        hwQualTypeStr = "<ERROR>";
    }
    line = "HW Qual Type: " + hwQualTypeStr + "\n";
    CHECK_RC(trep->AppendTrepString(line));

    line = Utility::StrPrintf("RAM Strap: 0x%08x\n", fb->GetConfigStrap());
    CHECK_RC(trep->AppendTrepString(line));

    line = Utility::StrPrintf("GPU Cores: %d\n", subdev->GetCoreCount());
    CHECK_RC(trep->AppendTrepString(line));

    return rc;
}

void Mdiag::PostRunSemaphore()
{
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        GpuDevMgr * pGpuDevMgr = (GpuDevMgr *)(DevMgrMgr::d_GraphDevMgr);

        // Do the socketWait on all GpuDevices
        if (pGpuDevMgr != NULL)
        {
           for (GpuDevice * pGpuDev = pGpuDevMgr->GetFirstGpuDevice();
                   pGpuDev != NULL; pGpuDev = pGpuDevMgr->GetNextGpuDevice(pGpuDev))
           {
               DebugPrintf("PostRunSemaphore: doing EscapeWrite to socketWait on GpuDevice %d\n",
                           pGpuDev->GetDeviceInst());
               const UINT32 value = 1;
               pGpuDev->EscapeWriteBuffer("socketWait", 0, sizeof(UINT32), &value);
               DebugPrintf("PostRunSemaphore: done doing EscapeWrite on GpuDevice %d\n",
                           pGpuDev->GetDeviceInst());
           }
        }
    }
}

void Mdiag::CleanUpAllTests(TestDirectory *testdir, bool quick_exit)
{
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        // Tell the simulation to not capture the cleanup phase of the run
        // for all devices.
        GpuDevMgr * pGpuDevMgr = (GpuDevMgr *)(DevMgrMgr::d_GraphDevMgr);

        if (pGpuDevMgr != NULL)
        {
           for (GpuDevice * pGpuDev = pGpuDevMgr->GetFirstGpuDevice();
                   pGpuDev != NULL; pGpuDev = pGpuDevMgr->GetNextGpuDevice(pGpuDev))
           {
               const UINT32 value = 0;
               pGpuDev->EscapeWriteBuffer("SetBackDoorCapture", 0, 4, &value);
           }
        }
    }

    testdir->CleanUpAllTests(quick_exit);
}

void Mdiag::VerifyLwlink(const ArgReader& global_args)
{
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        InfoPrintf("%s: LWLink testing in non-silicon mode. Skipping LWLink argument check\n", __FUNCTION__);
    }
    else
    {
        GpuDevMgr * pGpuDevMgr = (GpuDevMgr *)(DevMgrMgr::d_GraphDevMgr);
        MASSERT(pGpuDevMgr != NULL);
        GpuDevice * pGpuDev = pGpuDevMgr->GetFirstGpuDevice();
        MASSERT(pGpuDev != NULL);
        MASSERT(pGpuDev->GetNumSubdevices() > 0);
        GpuSubdevice * pSubdev = pGpuDev->GetSubdevice(0);
        MASSERT(pSubdev != NULL);
        LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS statusParams = {};
        RC rc = LwRmPtr()->ControlBySubdevice(pSubdev,
                                    LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS,
                                    &statusParams,
                                    sizeof(statusParams));
        if (rc != OK)
        {
            ErrPrintf("%s: Failed to get LWLink config from RM!\n", __FUNCTION__);
            MASSERT(0);
        }

        UINT32 mode = global_args.ParamUnsigned("-lwlink_set_verify_config_mode");
        int rv = 0;
        switch(mode)
        {
        case 0: // verify disabled
            break;
        case 1: // verify expected matches actual
            rv = Platform::VerifyLWLinkConfig(sizeof(statusParams), &statusParams, false);
            break;
        case 2: // verify expected is subset of actual
            rv = Platform::VerifyLWLinkConfig(sizeof(statusParams), &statusParams, true);
            break;
        default:
            // should never get here
            MASSERT(0);
            break;
        }

        if (rv)
        {
            ErrPrintf("%s: Validate LwLinkConfig failed!\n", __FUNCTION__);
            MASSERT(0);
        }
    }
}

// This function is called from mdiag.js when the -check_utl_scripts
// argument is specified.  It's used to skip the rest of MODS and run
// only UTL so that UTL scripts can be quickly checked for syntax errors.
//
RC Mdiag::CheckUtlScripts(ArgDatabase *pArgs)
{
#ifndef INCLUDE_MDIAGUTL
    ErrPrintf("-check_utl_scripts is not supported on Windows.\n");
    return RC::SOFTWARE_ERROR;
#endif

    ArgReader global_args(global_params);
    if (!global_args.ParseArgs(pArgs, ""))
    {
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    Utl::CreateInstance(pArgs, &global_args, nullptr, true);
    MASSERT(Utl::HasInstance());

    RC rc = Utl::Instance()->CheckScripts();

    if (OK != rc)
    {
        ErrPrintf("Utl::CheckScripts failed.\n");
        return rc;
    }

    Utl::Shutdown();
    return rc;
}

// This function is called from mdiag.js when the -print_utl_help argument is
// specified.  It's used to print the documentation of all UTL objects.
// No tests are run.
//
RC Mdiag::PrintUtlHelp(ArgDatabase *pArgs)
{
#ifndef INCLUDE_MDIAGUTL
    ErrPrintf("-print_utl_help is not supported on Windows.\n");
    return RC::SOFTWARE_ERROR;
#endif

    ArgReader global_args(global_params);
    if (!global_args.ParseArgs(pArgs, ""))
    {
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    Utl::CreateInstance(pArgs, &global_args, nullptr, true);
    MASSERT(Utl::HasInstance());

    RC rc = Utl::Instance()->PrintHelp();

    if (OK != rc)
    {
        ErrPrintf("Utl::PrintHelp failed.\n");
        return rc;
    }

    Utl::Shutdown();
    return rc;
}

bool Mdiag::IsSmmuEnabled()
{
    return s_EnableSmmu;
}

const ArgReader* Mdiag::GetParam() 
{
    return s_MdiagParam;
}
