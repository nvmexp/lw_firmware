/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "verif2d.h"
#include "v2d.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/mdiag_xml.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "core/include/lwrm.h"
#include "core/include/gpu.h"
#include "class/cl9097.h"
#include "lwmisc.h"
#include "gpu/include/gpusbdev.h"
#include "core/utility/errloggr.h"
#include "mdiag/tests/testdir.h"

extern const ParamDecl v2d_params[] =
{
    PARAM_SUBDECL(lwgpu_2dtest_params),
    PARAM_SUBDECL(lwgpu_single_params),
    UNSIGNED_PARAM("-x", "dst rect x coord"),
    UNSIGNED_PARAM("-y", "dst rect y coord"),
    UNSIGNED_PARAM("-sx", "src rect x coord"),
    UNSIGNED_PARAM("-sy", "src rect y coord"),
    UNSIGNED_PARAM("-w", "dst rect width"),
    UNSIGNED_PARAM("-h", "dst rect height"),
    UNSIGNED_PARAM("-ck", "color key (default disabled/0)"),
    STRING_PARAM("-ckformat", "color key color format (default disabled/A16R5G6B5)"),
    UNSIGNED_PARAM("-rop", "rop (default disabled/0)"),
    STRING_PARAM("-op", "set operation/patch config (default SRCCOPY)"),
    UNSIGNED_PARAM("-beta1", "set beta1d31 value (default disabled/0)"),
    UNSIGNED_PARAM("-beta4", "set beta4 value (default disabled/0)"),
    STRING_PARAM("-class", "rendering class to test (default lw4_image_blit)"),
    STRING_PARAM("-dstformat", "destination surface color format (default A8R8G8B8)"),
    STRING_PARAM("-dstclear", "clearn pattern (default XY_COORD_GRID)"),
    SIMPLE_PARAM("-dsttiled", "tiled destination surface"),
    SIMPLE_PARAM("-dumpTga", "dump TGA of color buffer (default no dump)"),
    SIMPLE_PARAM("-dumpImages", "alias of dumpTga: dump TGA of color buffer (default no dump)"),
    SIMPLE_PARAM("-pixdiff", "dump difference of real/shadow surfaces to stdout"),
    STRING_PARAM("-ctxsurf", "which context surfaces class to use (default lw10)"),
    SIMPLE_PARAM("-no_check", "do not check gpu results against expected values (default do check)"),
    SIMPLE_PARAM("-crcMissOK", "do not check gpu results against expected values (default do check)"),
    STRING_PARAM("-checkCrc", "verify test matches given CRC (in hex on the commandline)"),
    STRING_PARAM("-script", "filename of v2d script to execute (default no script)"),
    STRING_PARAM("-scriptDir", "base path for script and crcfile (optional)"),
    STRING_PARAM("-params", "string of params to set before running script"),
    STRING_PARAM("-params0", "several params strings, for perfgen"),
    STRING_PARAM("-params1", "several params strings, for perfgen"),
    STRING_PARAM("-params2", "several params strings, for perfgen"),
    STRING_PARAM("-params3", "several params strings, for perfgen"),
    STRING_PARAM("-params4", "several params strings, for perfgen"),
    STRING_PARAM("-params5", "several params strings, for perfgen"),
    STRING_PARAM("-params6", "several params strings, for perfgen"),
    STRING_PARAM("-params7", "several params strings, for perfgen"),
    STRING_PARAM("-params8", "several params strings, for perfgen"),
    STRING_PARAM("-params9", "several params strings, for perfgen"),
    STRING_PARAM("-s", "same as -script"),
    STRING_PARAM("-p", "same as -params"),
    STRING_PARAM("-crcfile", "filename of .crc file"),
    SIMPLE_PARAM("-gen", "generate CRCs (requires -crcfile)"),

    // below dmacheck methods are obsoleted
    // -dmacheck_m2m (-dmacheck 0)
    // -dmaCheck (-dmacheck 1)
    // -dmacheck_cipher (-dmacheck 2)
    // -dmacheckCipher (-dmacheck 3)
    // -dmacheck_msppp (-dmacheck 4)
    // -dmacheck_cepri (-dmacheck 7)
    { "-dma_check", "u", (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl:: GROUP_START), 5, 6, "dma image back to system memory" },
    { "-dmacheck_ce",      "5", ParamDecl::GROUP_MEMBER, 0, 0, "dma image back to sysmem using Copy Engine" },
    { "-crccheck_gpu",     "6", ParamDecl::GROUP_MEMBER, 0, 0, "compute CRC using FECS" },
    SIMPLE_PARAM("-disable_compressed_vidmem_surfaces_dma", "For Blackwell and above, by default, RTL targets use CE for reads from compressed surfaces in video memory. Use this option to disable it"),
    SIMPLE_PARAM("-crc_new_channel", "use a separate channel for dmaCheck"),
    STRING_PARAM("-pmperf", "specify performance monitor trigger file"),
    STRING_PARAM("-pm_file",   "Specifies the PM resource file" ),
    STRING_PARAM("-pm_option", "Specifies the PM triggers to use" ),
    SIMPLE_PARAM("-pm_ilwalidate_cache", "Ilwalidate the L2 cache before a pm trigger"),
    UNSIGNED_PARAM("-methodCount", "set number of methods (for ctx switching)"),
    UNSIGNED_PARAM("-subchannel_id", "subchannel id used in the v2d"),
    {"-cmd", "ttttttttttttttttt", ParamDecl::PARAM_FEWER_OK, 0, 0,
        "list of commands to execute in order: format -cmd cmd1 cmd2 ... cmdn"},
    STRING_PARAM("-l1_promotion", "set sector promotion (valid values: none, 2v, 2h, all) default is all, error if chip doesn't support it)"),
    STRING_PARAM("-o", "Specify base filename for state reporting"),
    SIMPLE_PARAM("-noCtxState", "obsolete, remove me!"),
    SIMPLE_PARAM("-optimal_block_size", "use optimal block size for buffers with blocklinear layout"),
    SIMPLE_PARAM("-fermi_hack", "This argument is obsolete and should be removed from all levels."),
    SIMPLE_PARAM("-enable_ecov_checking", "Enable ECover checking"),
    SIMPLE_PARAM("-enable_ecov_checking_only", "Enable ECover checking only if a valid ecov data file exists"),
    SIMPLE_PARAM("-nullEcover", "indicates meaningless ecoverage checking -- this happens sometimes when running test with certain priv registers"),
    SIMPLE_PARAM("-ignore_ecov_file_errors", "Ignore errors found in ECover file"),
    SIMPLE_PARAM("-allow_ecov_with_ctxswitch", "By default, the ecov checking will be skipped in \"-ctxswitch\" cases, and this argument will force the ecov checking even with \"-ctxswitch\""),
    { "-ecov_file", "t", ParamDecl::PARAM_MULTI_OK, 0, 0, "Specify the name of ecov checking file. Without this arg, mods will find test.ecov in test dir." },
    STRING_PARAM("-ignore_ecov_tags", "ignore these ecov tags or class names when doing ecov crc chain matching per ecov line. Format: -ignore_ecov_tags <className>:<className>:..."),
    { "-wfi_method", "u", (ParamDecl::PARAM_ENFORCE_RANGE |
                           ParamDecl::GROUP_START), WFI_POLL, WFI_HOST, "How to wait for idle" },
    { "-wfi_poll",   (const char*)WFI_POLL, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED,
      0, 0, "wait for idle by polling chip registers" },
    { "-wfi_notify", (const char*)WFI_NOTIFY, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED,
      0, 0, "wait for idle by polling a notifier" },
    { "-wfi_intr",   (const char*)WFI_INTR, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED,
      0, 0, "wait for idle by sleeping until a notifier interrupt arrives" },
    { "-wfi_sleep",  (const char*)WFI_SLEEP, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED,
      0, 0, "wait for idle by sleeping 10 seconds" },
    { "-wfi_host",  (const char*)WFI_HOST, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED,
      0, 0, "wait for idle by host semaphore" },
    SIMPLE_PARAM("-no_l2_flush",  "disables l2 cache flush before crc check"),
    SIMPLE_PARAM("-interrupt_check_ignore_order", "Check interrupt strings without considering the order."),
    SIMPLE_PARAM("-skip_intr_check", "Do NOT check the HW interrupts for a trace3d test.  Only use this option if you know what you're doing..."),
    SIMPLE_PARAM("-enable_color_compression",  "Enable 2D color compression."),
    { "-RawImageMode", "u", (ParamDecl::PARAM_ENFORCE_RANGE |
                         ParamDecl::GROUP_START | ParamDecl:: GROUP_OVERRIDE_OK), RAWSTART, RAWEND, "Read Raw (uncompressed) framebuffer images" },
    { "-RawImagesOff", (const char*)RAWOFF,    ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, 0, 0, "disable Raw (uncompressed) framebuffer images"},
    { "-RawImages",    (const char*)RAWON,     ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED, 0, 0, "Read Raw (uncompressed) framebuffer images"},
    SIMPLE_PARAM("-ignore_selfgild", "Ignore selfgild buffers"),
    SIMPLE_PARAM("-disable_bsp", "dummy argument, should not use"),
    STRING_PARAM("-dummy_ch_for_offload", "Allocate a dummy compute/graphic channel to enforce FECS ucode get loaded (bug 818386)"),

    SIMPLE_PARAM("-backdoor_load", "Dump raw memory contents of surface initialization and CRC checks"),
    SIMPLE_PARAM("-backdoor_crc", "Dump raw memory contents of surface initialization and CRC checks"),

    { "-backdoor_archive", "u", (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl::GROUP_START), 0, 1, "either create or read an archive of backdoor dump files" },
    { "-backdoor_archive_create", "0", ParamDecl::GROUP_MEMBER, 0, 0, "create an archive of all the raw memory dump files" },
    { "-backdoor_archive_read",   "1", ParamDecl::GROUP_MEMBER, 0, 0, "read an archive of all the raw memory dump files" },

    STRING_PARAM("-backdoor_archive_id", "Specifies the ID associated with the dump files in the backdoor archive.  Normally this is trep ID.  Required when running with -backdoor_archive_create or -backdoor_archive_read."),

    { "-raw_memory_dump_mode", "u", (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl::GROUP_START), 0, 2, "determine how to dump raw memory during chiplib trace capture" },
    { "-dump_raw_via_bar1_phys",  "0", ParamDecl::GROUP_MEMBER, 0, 0, "dump raw memory via mapping BAR1 as physical memory" },
    { "-dump_raw_via_pramin",     "1", ParamDecl::GROUP_MEMBER, 0, 0, "dump raw memory via PRAMIN" },
    { "-dump_raw_via_pte_change", "2", ParamDecl::GROUP_MEMBER, 0, 0, "dump raw memory via changing the PTE kind to non-swizzled pitch" },

    SIMPLE_PARAM("-c_render_to_z_2d", "Adds the 2D class method SetDstColorRenderToZetaSurface to the test channel."),

    STRING_PARAM("-interrupt_file", "Specify the interrupt file name used to verify expected interrupts."),
    STRING_PARAM("-smc_engine_label", "target SMC engine for this test, exclusive with -smc_eng_name. Example: v2d -smc_engine_label sys0 -i trace1/test.hdr"),
    STRING_PARAM("-smc_eng_name", "target SMC engine for this test, exclusive with -smc_engine_label. Example: v2d -smc_eng_name eng_0"),

    SIMPLE_PARAM("-disable_swizzle_between_naive_raw", "disable the swizzle logic on mods side between naive and raw"),

    UNSIGNED_PARAM("-dmacopy_ce_offset",  "Specify a CE engine from CE0-LAST_CE (value in range 0-last-CE#)."),

    { "-surface_init", "u", (ParamDecl::PARAM_ENFORCE_RANGE | ParamDecl::GROUP_START), 0, 1, "Specify which kinds of surface to be init with CE" },
    { "-surfaceinit_ce",     "0", ParamDecl::GROUP_MEMBER, 0, 0, "Initialize all sysmem surfaces with CE, not reflected writes" },
    { "-blocklinearinit_ce", "1", ParamDecl::GROUP_MEMBER, 0, 0, "Initialize blocklinear sysmem surfaces with CE, not reflected writes" },

    SIMPLE_PARAM("-set_cc_trusted_for_crc", "Temporarily set the CC trust level for the copy engine to TRUSTED during crc checks."),

    LAST_PARAM
};

static const ParamConstraints trace_file_constraints_v2d[] = {
    MUTUAL_EXCLUSIVE_PARAM("-block_height", "-optimal_block_size"), // Bug 387379
    MUTUAL_EXCLUSIVE_PARAM("-block_height_a", "-optimal_block_size"),
    MUTUAL_EXCLUSIVE_PARAM("-block_height_b", "-optimal_block_size"),
    MUTUAL_EXCLUSIVE_PARAM("-smc_engine_label", "-smc_eng_name"),
    LAST_CONSTRAINT
};

Verif2dTest::Verif2dTest(ArgReader *pParams):
    LWGpu2dTest(pParams)
{
    ProcessCommonArgs();

    m_v2d = 0;
    dumpTga = m_pParams->ParamPresent("-dumpTga") > 0 || m_pParams->ParamPresent("-dumpImages") > 0;
    doDmaCheck = m_pParams->ParamPresent( "-dma_check" ) > 0 &&
        m_pParams->ParamUnsigned( "-dma_check" ) < 2;
    doDmaCheckCE = m_pParams->ParamPresent( "-dma_check" ) > 0 &&
        m_pParams->ParamUnsigned( "-dma_check" ) == 5;
    doGpuCrcCalc = m_pParams->ParamPresent( "-dma_check" ) > 0 &&
        m_pParams->ParamUnsigned( "-dma_check" ) == 6;

    ctxSurfString = m_pParams->ParamStr( "-ctxsurf", "" );
    m_rop = m_pParams->ParamUnsigned( "-rop", 0 );
    m_doRop = m_pParams->ParamPresent( "-rop" ) > 0;
    m_operation = m_pParams->ParamStr( "-op", "SRCCOPY" );
    m_doOperation = m_pParams->ParamPresent( "-op" ) > 0;
    m_beta1d31 = m_pParams->ParamUnsigned( "-beta1", 0 );
    m_doBeta1 = m_pParams->ParamPresent( "-beta1" ) > 0;
    m_beta4 = m_pParams->ParamUnsigned( "-beta4", 0 );
    m_doBeta4 = m_pParams->ParamPresent( "-beta4" ) > 0;
    m_ctxswMethodCount = m_pParams->ParamUnsigned( "-methodCount", 0 );
    m_sectorPromotion = m_pParams->ParamStr( "-l1_promotion", "none" );

    if (m_pParams->ParamPresent("-pm_file") > 0)
    {
        m_pm_trigger_file = m_pParams->ParamStr("-pm_file");
        m_useNewPerfmon = true;
    }
    else if (m_pParams->ParamPresent("-pmperf") > 0)
    {
        m_pm_trigger_file = m_pParams->ParamStr("-pmperf");
        m_useNewPerfmon = false;
    }
    else
    {
        m_pm_trigger_file = "";
        m_useNewPerfmon = false;
    }

    // CRC-checking logic:
    if (m_pParams->ParamPresent( "-no_check" ) == 0 &&
        m_pParams->ParamPresent( "-crcMissOK" ) == 0)
    {
        // -crcfile enables CRC file checking (or generation, with -gen):
        if ( m_pParams->ParamPresent("-crcfile") )
        {
            strcpy(_crcFilename, m_pParams->ParamStr("-crcfile"));
            if ( m_pParams->ParamPresent( "-gen" ) )
                _crcMode = CRC_MODE_GEN;
            else
                _crcMode = CRC_MODE_CHECK;
        }
        else
        {
            if ( m_pParams->ParamPresent( "-gen" ) )
                WarnPrintf( "-gen specified without -crcfile, ignored\n" );
            _crcMode = CRC_MODE_NOTHING;
        }

        // compare final dst surface to CRC on command-line:
        doCheckCommandLineCrc = m_pParams->ParamPresent( "-checkCrc" ) > 0;
        commandLineCrc = m_pParams->ParamStr("-checkCrc", "0" );
    }
    else
    {
        _crcMode = CRC_MODE_NOTHING;
        doCheckCommandLineCrc = false;
    }

    imageDumpFormat = V2dSurface::NONE;
    if ( m_pParams->ParamPresent( "-dumpTga" ) > 0 || m_pParams->ParamPresent( "-dumpImages" ) > 0)
        imageDumpFormat = V2dSurface::TGA;

    if ( m_pParams->ParamPresent( "-p" ) > 0 )
        m_paramsString = m_pParams->ParamStr( "-p" );
    if ( m_pParams->ParamPresent( "-params" ) > 0 )
        m_paramsString = m_pParams->ParamStr( "-params" );

    for (int p=0; p<10; p++)
    {
        // concatenate all -params* arguments
        // this allows perfgen to cycle over multiple sets of parameters
        string arg = string("-params") + char('0' + p);
        if ( m_pParams->ParamPresent( arg.c_str() ) > 0 )
        {
            if ( m_paramsString.size() > 0 )
                m_paramsString += ",";
            m_paramsString += m_pParams->ParamStr( arg.c_str() );
        }
    }
    InfoPrintf("v2d script parameters: %s\n", m_paramsString.c_str());

    if ( m_pParams->ParamPresent( "-s" ) > 0 )
        m_scriptFileName = m_pParams->ParamStr( "-s" );
    if ( m_pParams->ParamPresent( "-script" ) > 0 )
        m_scriptFileName = m_pParams->ParamStr( "-script" );

    if ( m_pParams->ParamPresent( "-scriptDir" ) > 0 )
        m_scriptDir = m_pParams->ParamStr( "-scriptDir" );
    else
        m_scriptDir = "./";

    if (m_scriptDir[m_scriptDir.size()-1] != '/')
        m_scriptDir += '/';

    dstFlags = V2dSurface::LINEAR;
    if (m_pParams->ParamPresent("-dsttiled"))
        dstFlags = (V2dSurface::Flags) (dstFlags | V2dSurface::TILED);

    m_cmdList.clear();
    int nCmds = m_pParams->ParamNArgs( "-cmd" );
    int j;
    for ( j = 0; j < nCmds; j++ )
        m_cmdList.push_back( m_pParams->ParamNStr("-cmd", j) );
    m_IsCtxSwitchTest = !LWGpuResource::GetTestDirectory()->IsSerialExelwtion();
}

Verif2dTest::~Verif2dTest( void )
{
}

STD_TEST_FACTORY(v2d, Verif2dTest)

// a little extra setup to be done
int Verif2dTest::Setup(void)
{
    DebugPrintf("v2d: Setup() starts\n");

    if (!m_pParams->ValidateConstraints(trace_file_constraints_v2d))
    {
        ErrPrintf("Test parameters are not valid- please check previous messages\n");
        return 0;
    }

    string smcEngineLabel;
    
    if (m_pParams->ParamPresent("-smc_engine_label"))
    {
        smcEngineLabel = m_pParams->ParamStr("-smc_engine_label", UNSPECIFIED_SMC_ENGINE);
    }
    else
    {
        smcEngineLabel = m_pParams->ParamStr("-smc_eng_name", UNSPECIFIED_SMC_ENGINE);
    }

    // call parent's Setup first
    if ( ! Setup2d( "v2d", smcEngineLabel ) )
    {
        ErrPrintf("Verif2dTest::Setup: failed LWGpu2dTest::Setup()\n");
        return 0;
    }

    //
    // 1. open the script file
    // 2. find the object name, for example $twod  = object( fermi_twod_a );
    // 3. identify classID
    //
    ifstream scriptfile( m_scriptFileName.c_str() );
    if ( scriptfile.good() == false )
    {
        ErrPrintf("Couldn't open script file %s. Failed reason: %s\n",
                  m_scriptFileName.c_str(), Utility::InterpretFileError().Message());
        return 0;
    }

    string line;
    string objname;
    while(getline(scriptfile, line))
    {
        size_t i;
        size_t found = line.find_first_not_of(" ");
        if((found == string::npos) || (line[found] == '#'))
        {
            continue;// skip blank or comment line
        }

        found=line.find("object", found); //$twod  = object( fermi_twod_a );
        if((found == string::npos) || (found == 0) ||
           (found+6 >= line.size()))
        {
            continue;
        }
        if((line[found - 1] != ' ') && (line[found - 1] != '='))
        {
            continue;
        }
        if((line[found+6] != '(') && (line[found+6] != ' '))
        {
            continue;
        }

        size_t begin = line.find("(", found+6);
        size_t end = line.find(")", found+7);
        if((begin == string::npos ) || (end == string::npos))
        {
            continue;
        }
        for(i = begin+1; i < end; i ++)
        {
            if(line[i] == ' ') continue;
            objname += line[i];
        }
        break;
    }

    //  set the CRC chain according object name
    m_CrcChain.clear();
    if(!objname.empty() && (objname.compare("fermi_twod_a") == 0))
    {
        if (Platform::GetSimulationMode() != Platform::Amodel)
        {
            m_CrcChain.push_back("gf100");
        }
        else
        {
            m_CrcChain.push_back("gf100a");
        }
    }
    else if(!objname.empty() && ((objname.compare("kepler_twod_a") == 0) ||
            (objname.compare("kepler_inline_to_memory_a") == 0)))
    {
        if (Platform::GetSimulationMode() != Platform::Amodel)
        {
            m_CrcChain.push_back("gk104");
        }
        else
        {
            m_CrcChain.push_back("gk104a");
        }
    }
    else
    {
        if (Platform::GetSimulationMode() != Platform::Amodel)
            m_CrcChain.push_back("lw50f");
        m_CrcChain.push_back("lw50a");
    }

    if (_crcMode != CRC_MODE_NOTHING)
        LoadCrcProfile();

    if (!FindCrcSection("", ""))
        return 0;

    if (gXML != nullptr)
    {
        XD->XMLStartStdInline("<MDiagTestDir");
        XD->outputAttribute("now", gXML->GetRuntimeTimeInMilliSeconds());
        XD->outputAttribute("SeqId", GetSeqId());
        XD->outputAttribute("TestDir", m_scriptDir.c_str());
        XD->XMLEndLwrrent();
    }

    if ( m_pm_trigger_file.size() > 0 )
    {
        bool pmInitialized = false;

        MASSERT(m_useNewPerfmon);
        lwgpu->CreatePerformanceMonitor();

        const char *option_type = "STANDARD";
        if (lwgpu->GetLwgpuArgs()->ParamPresent("-pm_option"))
        {
            option_type = lwgpu->GetLwgpuArgs()->ParamStr("-pm_option");
        }

        if (lwgpu->PerfMonInitialize(option_type,
                m_pm_trigger_file.c_str()))
        {
            pmInitialized = true;
        }

        if (!pmInitialized)
        {
            ErrPrintf("Initialize PM failed!\n");
            CleanUp();
            SetStatus(Test::TEST_NO_RESOURCES);
            getStateReport()->runFailed( "failure initializing PM\n");
            return 0;
        }
    }

    // we should really verify if fermi 2d class is supported
    // but that is not hooked up yet
    LwRmPtr pLwRm;
    if (pLwRm->IsClassSupported(FERMI_A, lwgpu->GetGpuDevice()))
    {
        RegHal& regHal = lwgpu->GetRegHal(lwgpu->GetGpuSubdevice(), GetLwRmPtr(), GetSmcEngine());
        regHal.Write32(MODS_PGRAPH_PRI_FE_RENDER_ENABLE_TWOD_TRUE);
    }

    // Also setup Ecov checker
    RC rc = SetupEcov(&m_EcovVerifier, m_pParams, m_IsCtxSwitchTest, m_scriptFileName, "");
    if (OK != rc)
    {
        ErrPrintf("v2d: Failed to set up ecover: %s\n", rc.Message());
        return 0;
    }

    if (m_EcovVerifier.get())
    {
        m_EcovVerifier->SetGpuResourceParams(lwgpu->GetLwgpuArgs());
    }

    if ((rc = ErrorLogger::StartingTest()) != OK)
    {
        ErrPrintf("v2d: Error initializing ErrorLogger: %s\n", rc.Message());
        CleanUp();
        return 0;
    }

    DebugPrintf("v2d: Setup() done succesfully\n");
    return( 1 );
}

// a little extra cleanup to be done
void Verif2dTest::CleanUp(void)
{
    DebugPrintf("v2d: CleanUp() starts\n");

    delete m_v2d;

    // call parent's cleanup too
    LWGpu2dTest::CleanUp();

    m_EcovVerifier.reset(0);

    DebugPrintf("v2d: CleanUp() done\n");
}

// Override LWGpuSingleChannelTest::PostRun()
void Verif2dTest::PostRun()
{
    LWGpu2dTest::PostRun();

    if (CheckIntrPassFail() != TestEnums::TEST_SUCCEEDED)
    {
        ErrPrintf("v2d: Failed interrupt check\n");
        SetStatus(Test::TEST_FAILED);
    }
    ErrorLogger::TestCompleted();
}

// break a string up unto a vector of substrings, separated by any
// character in the string delim:
static vector<string> tokenize( const string &str, const string &delim ) {
    vector<string> out;
    unsigned int t=0, i=0, start=0;
    while ( i < str.size() ) {
        while ( i < str.size() &&
                ( ( delim.find(str[i]) == string::npos ) == t ) ) i++;
        t = !t;
        if ( t )
            start = i;
        else if ( i > start )
            out.push_back( str.substr( start, i-start ) );
    }
    return out;
}

// the real test
void Verif2dTest::Run(void)
{
    DebugPrintf("v2d: Run() starts\n");

    try
    {
        m_v2d = new Verif2d( this->ch, this->lwgpu, dstFlags, this, m_pParams );
        m_v2d->Init(V2dGpu::LW30);
        m_v2d->SetCtxSurfType( ctxSurfString );
        m_v2d->SetImageDumpFormat( imageDumpFormat );
        m_v2d->SetDoDmaCheck( doDmaCheck );
        m_v2d->SetDoDmaCheckCE( doDmaCheckCE );
        m_v2d->SetDoGpuCrcCalc( doGpuCrcCalc );
        m_v2d->SetCrcData( _crcMode, _crcProfilePtr, _crcSection );
        m_v2d->SetScriptDir( m_scriptDir );
        m_v2d->SetSectorPromotion( m_sectorPromotion );

        if ( m_ctxswMethodCount > 0 )
        {
            InfoPrintf(" setting method count: %d\n", m_ctxswMethodCount );
            this->ch->SetMethodCount( m_ctxswMethodCount );
        }

        if ( m_scriptFileName.size() > 0 )
        {
            Lex lex;
            //Token t;
            lex.Init( m_scriptFileName );
            EvalElw E( m_v2d, &lex );
            Parser *p = new Parser( E );
            ScriptNode *programNode = p->Parse();    // parse the program:

            if ( m_paramsString.size() > 0 )
            {
                // extract params from string "id1=expr1,id2=expr2,..."
                // allow '+' instead of ',' for perfgen
                vector<string> assignments = tokenize( m_paramsString, ",+" );
                for ( unsigned int i=0; i<assignments.size(); i++ )
                {
                    // allow ':' instead of '=' for perfgen
                    vector<string> a = tokenize( assignments[i], "=:" );
                    if ( a.size() != 2 )
                        V2D_THROW( "error in -params string: '" <<
                                   assignments[i] << "'" );
                    programNode->SetSymbolUntypedValue( E, a[0], a[1] ) ;

                }
            }

            programNode->Evaluate( E );                 // run the program
            programNode->DumpSymbolTable( E );
            m_v2d->WaitForIdle();
            SetStatus(Test::TEST_SUCCEEDED);

            if ( m_v2d->GetDstSurface() != 0 )
            {
                if ( doCheckCommandLineCrc )
                {
                    // CRC given on command line
                    if ( ! m_v2d->CompareDstSurfaceToCRC( "v2d_fail.tga",
                                                          commandLineCrc ) )
                    {
                        SetStatus( Test::TEST_FAILED_CRC );
                        getStateReport()->crcCheckFailed(
                            "command-line CRC check failed");
                    }
                }

                if ( dumpTga )
                    m_v2d->SaveDstSurfaceToTGAfile( "v2d.tga" );
            }
            else
                InfoPrintf("v2d: no dst surface specified so no crc or surface compare checks were run\n");

            // Checking Event-Coverage
            if (m_EcovVerifier.get())
            {
                // Keep original logic: crcChain comes from m_v2d, rather than m_CrcChain,
                // Even though it is confusing comparing with m_pParams for m_EcovVerifier.
                m_EcovVerifier->AddCrcChain(m_v2d->GetCrcChain());
                VerifyEcov(m_EcovVerifier.get(), m_pParams, lwgpu, m_IsCtxSwitchTest, &status);
                m_EcovVerifier.reset(0);
            }
        }
        else
        {
            WarnPrintf("no v2d script file specified%d\n");
            SetStatus(Test::TEST_SUCCEEDED);
        }

        m_v2d->WaitForIdle();
        InfoPrintf("number of methods sent: %d\n", m_v2d->GetMethodWriteCount());

        switch (status)
        {
        case Test::TEST_NOT_STARTED:
            InfoPrintf("v2d: Run() ends with no script exelwted! \n");
            break;
        case Test::TEST_SUCCEEDED:
            InfoPrintf("v2d: Run() ends with test PASS! \n");
            break;
        case Test::TEST_FAILED_ECOV:
            InfoPrintf("v2d: Run() ends with test FAILED: ECov matching!\n");
            break;
        case Test::TEST_ECOV_NON_EXISTANT:
            InfoPrintf("v2d: Run() ends with test FAILED: ECov file error or missing!\n");
            break;
        case Test::TEST_FAILED_CRC:
            InfoPrintf("v2d: Run() ends with test FAILED: CRC matching!\n");
        default:
            // Should not be here
            ErrPrintf("v2d: Test FAILED!\n");
        }

        if (_crcMode == CRC_MODE_CHECK)
        {
            InfoPrintf("reporting v2d test passed GOLD\n");
            getStateReport()->crcCheckPassedGold();
        }
        else
        {
            InfoPrintf("reporting v2d test passed LEAD\n");
            getStateReport()->runSucceeded();
        }

        if (_crcMode == CRC_MODE_GEN)
            SaveCrcProfile();

        return;
    }

    catch ( const V2DException& v )
    {
        ErrPrintf("Verif2d:Error:%s\n", v.Message.c_str());
        if (!v.StatusSet)
        {
            SetStatus( Test::TEST_FAILED );
            getStateReport()->runFailed();
        }
    }

    DebugPrintf("v2d: Run() ends (fail) \n");

    // only set status to FAILED if some other part of the test has
    // not already set the status (i.e. on a CRC failure)
    if ( GetStatus() == TEST_NOT_STARTED )
    {
        SetStatus( Test::TEST_FAILED );
        getStateReport()->runFailed();
    }
}

bool IntrErrorFilter(UINT64 intr, const char *lwrErr, const char *expectedErr);
TestEnums::TEST_STATUS Verif2dTest::CheckIntrPassFail()
{
    TestEnums::TEST_STATUS result = TestEnums::TEST_SUCCEEDED;

    if(m_pParams->ParamPresent("-skip_intr_check") > 0)
    {
        InfoPrintf("Verif2dTest::CheckIntrPassFail: Skipping intr check per commandline override!\n");
        ErrorLogger::IgnoreErrorsForThisTest();
        return result;
    }

    string ifile(m_scriptFileName);

    if (m_pParams->ParamPresent("-interrupt_file"))
    {
        ifile = Utility::StripFilename(m_scriptFileName.c_str());
        if (ifile.length() == 0)
        {
            ifile = ".";
        }
        ifile += "/";
        ifile += m_pParams->ParamStr("-interrupt_file");
    }
    else
    {
        string::size_type dot_pos = ifile.rfind('.');
        if (dot_pos != string::npos)
        {
            ifile.erase(dot_pos, string::npos);
        }
        ifile.append(".int");
    }
    const char *intr_file = ifile.c_str();
    FILE *wasFilePresent = NULL;

    if(ErrorLogger::HasErrors())
    {
        ErrorLogger::InstallErrorFilter(IntrErrorFilter);
        ErrorLogger::CompareMode compMode = ErrorLogger::Exact;
        if (m_pParams->ParamPresent("-interrupt_check_ignore_order") > 0)
        {
            compMode = ErrorLogger::OneToOne;
        }
        RC rc = ErrorLogger::CompareErrorsWithFile(intr_file, compMode);

        if(rc != OK)
        {
            if(Utility::OpenFile(intr_file, &wasFilePresent, "r") != OK)
            {
                ErrPrintf("Verif2dTest::checkIntrPassFail: interrupts detected, but %s file not found.\n", intr_file);
                ErrorLogger::PrintErrors(Tee::PriNormal);
                ErrorLogger::TestUnableToHandleErrors();
            }
            result = TestEnums::TEST_FAILED_CRC;
        }
    }
    else
    {
        // Don't use Utility::OpenFile since it prints an error if it can't
        // find the file...confuses people
        wasFilePresent = fopen(intr_file, "r");
        if(wasFilePresent)
        {
            ErrPrintf("Verif2dTest::checkIntrPassFail: NO error interrupts detected, but %s file exists!\n", intr_file);
            result = TestEnums::TEST_FAILED_CRC;
        }
    }

    if(wasFilePresent) fclose(wasFilePresent);

    return result;
}
