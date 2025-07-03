/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2013,2017, 2019, 2021 by LWPU Corporation.  All rights 
 * reserved. All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"
#include "mdiag/tests/test_state_report.h"
#include "mdiag/utils/gr_random.h"

#include "t5d.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"

#include "mdiag/tests/gpu/2d/verif2d/verif2d.h"

#include "gpu/include/gpumgr.h"
#include "core/include/mgrmgr.h"

extern const ParamDecl trace_3d_params[]; \

static const ParamDecl trace_and_2d_params[] = {
  SIMPLE_PARAM("-forever", "Repeat the init-run-check outer sequence until error or timeout (default=disable)."),
  {"-nTimes", "u", 0, 0, 0, "Number of test loops"},
  UNSIGNED_PARAM("-x", "display width of separate 2d buffer"),
  UNSIGNED_PARAM("-y", "display height of separate 2d buffer"),
  SIMPLE_PARAM("-separate", "run 2d tests on a separate buffer"),
  SIMPLE_PARAM("-no_clear", "don't clear the 2d buffer"),
  SIMPLE_PARAM("-method_dump", "prints all methodwrites instead of sending them"),
  SIMPLE_PARAM("-burst_write", "writes to pushbuffer and then only updates put pointer once"),
  STRING_PARAM("-v2d", "run v2d as the 2d test portion of tc5d, using string arg as script file name"),
  STRING_PARAM("-v2dvars", "initialize v2d script variables, 'sym1=expr1,sym2=expr2,etc.'"),
  STRING_PARAM("-v2dCheckCrc", "set the CRC to compare against when calling checkCrc() in the v2d script"),
  SIMPLE_PARAM("-v2dGenCrc","generate CRC for the final trace_3d v2d image"),
  UNSIGNED_PARAM("-subchannel_id", "subchannel id used in the v2d"),
  SIMPLE_PARAM("-fermi_hack", "This argument is obsolete and should be removed from all levels."),
  SIMPLE_PARAM("-enable_color_compression",  "Enable 2D color compression."),
  STRING_PARAM("-c_render_to_z_2d", "Adds the 2D class method SetDstColorRenderToZetaSurface to the test channel."),
  MEMORY_SPACE_PARAMS("_dst", "destination surface"),
  MEMORY_SPACE_PARAMS("_src", "source surface"),
  STRING_PARAM("-interrupt_file", "Specify the interrupt file name used to verify expected interrupts."),

  LAST_PARAM
};

extern const ParamDecl t5d_params[] = {
    PARAM_SUBDECL(trace_3d_params),
    PARAM_SUBDECL(trace_and_2d_params),
    SIMPLE_PARAM("-doubleXor", "perform 2 xors that cancel eachother"),
    SIMPLE_PARAM("-singleXor", "do all 2d writes with an xor rop"),
    SIMPLE_PARAM("-2dBefore", "perform 2d operations before 3d trace and turn off interleaving."),
    UNSIGNED_PARAM("-2dBeforeNum", "number of 2dBefore methods to send down during the 3d traces"),
    SIMPLE_PARAM("-2dAfter", "perform 2d operations after 3d trace and turn off interleaving."),
    UNSIGNED_PARAM("-2dAfterNum", "number of 2dAfter methods to send down during the 3d traces"),
    UNSIGNED_PARAM("-2dHeight", "the maximum height of the 2d thing to be drawn. Default is 20"),
    UNSIGNED_PARAM("-2dWidth", "the maximum width of the 2d thing to be drawn. Default is 20"),
    UNSIGNED_PARAM("-2dHeightMax", "the maximum height of the 2d thing to be drawn. Default is 20"),
    UNSIGNED_PARAM("-2dWidthMax", "the maximum width of the 2d thing to be drawn. Default is 20"),
    SIMPLE_PARAM("-interRect", "interleave 3d trace with rect"),
    SIMPLE_PARAM("-interBlit", "interleave 3d trace with blit"),
    SIMPLE_PARAM("-interScaled", "interleave 3d trace with scaled"),
    UNSIGNED_PARAM("-subchswGran", "granularity of subchannel switches during the 3d trace"),
    // The following parameters are active only with mdiag option -single_method
    UNSIGNED_PARAM("-subchswPoint", "trace_cels method before which the frist subch switch happens"),
    UNSIGNED_PARAM("-subchswNum", "number of subchannel switches during the 3d trace"),
    UNSIGNED_PARAM("-subchswInterval", "if presents, subch switching will happen at every specified number of 3d methods"),
    SIMPLE_PARAM("-interScaled", "interleave 3d trace with drawScaled"),
    // The following parameters are for dvd10
    SIMPLE_PARAM("-interDvd10", "interleave 3d trace with dvd10"),
    SIMPLE_PARAM("-dumpDvd10", "Dump output image of dvd10 in bmp format"),
    SIMPLE_PARAM("-no_clear", "don't clear the 2d buffer"),
    // for v2d
    SIMPLE_PARAM("-noCtxState", "obsolete, remove me!"),
    SIMPLE_PARAM("-fermi_hack", "This argument is obsolete and should be removed from all levels."),
    UNSIGNED_PARAM("-block_height_a", "block height in gobs (surface type A)"),
    UNSIGNED_PARAM("-block_height_b", "block height in gobs (surface type B)"),
    LAST_PARAM
};

Trace5dTest::Trace5dTest(ArgReader *params) :
    TraceAnd2dTest<Trace3DTest>(params)
{
    m_pParams = params;
    do2dBefore = false;
    do2dAfter = false;

    height2d = params->ParamUnsigned("-2dHeight", 4);
    width2d = params->ParamUnsigned("-2dWidth", 4);
    max_height2d = params->ParamUnsigned("-2dHeightMax", height2d);
    max_width2d = params->ParamUnsigned("-2dWidthMax", width2d);

    if (params->ParamPresent("-doubleXor"))
    {
        // XXX seems like -doubleXor is an obsolete option that can be ripped out
    }
    if (params->ParamPresent("-singleXor"))
    {
        // XXX seems like -singleXor is an obsolete option that can be ripped out
    }

    do2dBefore = !!params->ParamPresent("-2dBefore");
    do2dAfter  = !!params->ParamPresent("-2dAfter");
    num2dBefore = params->ParamUnsigned("-2dBeforeNum",1);
    num2dAfter = params->ParamUnsigned("-2dAfterNum",1);

    // CPLU: Li-Yi wants subchswGran to be intact with 2dbefore
    // if (do2dBefore || do2dAfter)
    //    subchswGran = 0;

    interleaveWithRect = !!params->ParamPresent("-interRect");
    interleaveWithBlit = !!params->ParamPresent("-interBlit");
    interleaveWithScaled = !!params->ParamPresent("-interScaled");
    interleaveWithDvd10 = !!params->ParamPresent("-interDvd10");

    // These interleave tests won't run on new chips -- verify not being used
    // XXX completely rip these out
    MASSERT(!interleaveWithRect);
    MASSERT(!interleaveWithBlit);
    MASSERT(!interleaveWithScaled);
    MASSERT(!interleaveWithDvd10);

    subchswGran = params->ParamUnsigned("-subchswGran", 0);
    subchswNum = params->ParamUnsigned("-subchswNum", 500);
    subchswPoint = params->ParamUnsigned("-subchswPoint", 1);
    subchswInterval = params->ParamUnsigned("-subchswInterval", 1);
    singleMethod = params->ParamPresent("-single_method");

    tcLoopCount = 1;
    tcMethodCount = 1;
    tcSubchswCount = 0;
    tcSubchswPoint = subchswPoint;

    grRandom = new GrRandom;
    buf = NULL;
}

Trace5dTest::~Trace5dTest(void)
{
}

Test *Trace5dTest::Factory(ArgDatabase *args)
{
    ArgReader *reader;

    reader = new ArgReader(t5d_params);
    if(!reader || !reader->ParseArgs(args)) {
        ErrPrintf(" error reading parameters!\n");
        return(0);
    } else  {
        Trace5dTest *test = new Trace5dTest(reader);
        test->ParseConlwrrentArgs(reader);
        return test;
    }
}

// a little extra cleanup to be done
void Trace5dTest::CleanUp(void)
{
    TraceAnd2dTest<Trace3DTest>::CleanUp();
}

bool Trace5dTest::BeforeEachMethod()
{
    if (singleMethod)
    {
        if ((tcMethodCount >= tcSubchswPoint) && (tcSubchswCount < subchswNum))
        {
            if ((subchswInterval && ((tcMethodCount-tcSubchswPoint)%subchswInterval == 0)))
            {
                tcSubchswCount++;
                InfoPrintf("subch switch count %d\n", tcSubchswCount);

                MASSERT(!interleaveWithRect);
                MASSERT(!interleaveWithBlit);
                MASSERT(!interleaveWithScaled);
                MASSERT(!interleaveWithDvd10);
                InfoPrintf("trace_cels method count %d\n", tcMethodCount);
            }
        }

        DebugPrintf("trace_cels method count %d\n", tcMethodCount);
        tcMethodCount++;
    }
    return true;
}

bool Trace5dTest::BeforeEachMethodGroup(unsigned num_methods)
{
    if ((tcMethodCount >= subchswPoint) && (subchswGran && (grRandom->RandomRoll(1,subchswGran) == 1)))
    {
        InfoPrintf("trace_cels loop count %d\n", tcLoopCount);

        // reset subch switch point and count
        tcSubchswPoint = tcMethodCount;
        tcSubchswCount = 0;
        InfoPrintf("current subchsw point: %d\n", tcSubchswPoint);

        // if not in single method mode, do the 2d stuff once
        if (!singleMethod)
        {
            MASSERT(!interleaveWithRect);
            MASSERT(!interleaveWithBlit);
            MASSERT(!interleaveWithScaled);
            MASSERT(!interleaveWithDvd10);
            InfoPrintf("trace_cels method count %d\n", tcMethodCount);
        }
    }
    DebugPrintf("trace_cels loop count %d\n", tcLoopCount);
    DebugPrintf("trace_cels method count %d\n", tcMethodCount);

    tcLoopCount++;
    if (!singleMethod) tcMethodCount += num_methods;
    return true;
}

bool Trace5dTest::BeforeExelwteMethods()
{
 // setup code usd to be in BeforeExelwteMethods()
    // moved here such that BeforeExelwteMethods contain no init code.

    testStatus2d = Test::TEST_SUCCEEDED;

    if (!RunSetup2d())
        return false;

    MASSERT(m_doV2d); // all t5d tests should be v2d-based now, not legacy 2D
    return true; // v2d has its own setup
}

bool Trace5dTest::RunBeforeExelwteMethods()
{
    if (do2dBefore)
    {
        MASSERT(!interleaveWithDvd10); // obsolete test
        MASSERT(!interleaveWithRect); // obsolete test
        MASSERT(!interleaveWithBlit); // obsolete test
        MASSERT(!interleaveWithScaled); // obsolete test
    }
    return true;
}

// break a string up unto a vector of substrings, separated by any
// character in the string delim:
// helper function for Rulw2dTest, for parsing variable initialization strings
//
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

//
// The -v2d option does this:
// + creates V2dSurface objects for the the existing trace_cels color and Z surfaces, copying
//       the GPU contents of these surfaces into the respective V2dSurface shadow buffers, thus
//       initializing the V2dSurface objects with whatever trace_cels left in those
//       surfaces
// + creates v2d symbol table entries for the C and Z surface handles, and the surface
//       parameters (width, height, etc.), so that v2d scripts can use them
//       for src/dst surfaces, control which buffers are checked for correctness,
//       vary command sizes based on buffer shape, etc.   The symbols are:
//
//       surf3d_C_obj
//       surf3d_C_width
//       surf3d_C_height
//       surf3d_Z_obj
//       surf3d_Z_width
//       surf3d_Z_height
//
// + runs the v2d script
// + the pass/fail of the entire subchsw test is solely based on the return value from the v2d
//       script,
//       because we're not testing whether or not the 3d rendered correctly, we're testing
//       whether or not the 2d worked using the C and Z surfaces as src/dst.
//

void Trace5dTest::Rulw2dTest( void )
{
    try
    {
        GpuDevice* pGpuDev = GetBoundGpuDevice();
        MASSERT(pGpuDev);
        if (pGpuDev->GetNumSubdevices() > 1 && surfC[0]->GetLocation() != Memory::Fb)
        {
            V2D_THROW("t5d does not support render targets in sysmem\n");
        }

        UINT32 surf3d_C_pitch = surfC[0]->GetPitch();
        uintptr_t surf3d_C_baseAddr = 0;
        UINT32 surf3d_C_handle = surfC[0]->GetMemHandle();
        UINT32 surf3d_C_width = surfC[0]->GetWidth();
        UINT32 surf3d_C_height = surfC[0]->GetHeight();
        bool surf3d_C_tiled = surfC[0]->GetTiled();
        bool surf3d_C_swizzled = surfC[0]->GetLayout() == Surface::Swizzled;
        UINT32 surf3d_C_surface_format =
            GetSurfaceMgr()->DeviceFormatFromFormat(surfC[0]->GetColorFormat());
        UINT32 surf3d_C_LogBlockHeight = surfC[0]->GetLogBlockHeight();
        UINT32 surf3d_C_LogBlockDepth  = surfC[0]->GetLogBlockDepth();

        UINT32 surf3d_Z_pitch = surfZ->GetPitch();
        uintptr_t surf3d_Z_baseAddr = 0;
        UINT32 surf3d_Z_handle = surfZ->GetMemHandle();
        UINT32 surf3d_Z_width = surfZ->GetWidth();
        UINT32 surf3d_Z_height = surfZ->GetHeight();
        bool surf3d_Z_tiled = surfZ->GetTiled();
        bool surf3d_Z_swizzled = surfZ->GetLayout() == Surface::Swizzled;
        UINT32 surf3d_Z_surface_format =
            GetSurfaceMgr()->DeviceFormatFromFormat(surfZ->GetColorFormat());
        UINT32 surf3d_Z_LogBlockHeight = surfZ->GetLogBlockHeight();
        UINT32 surf3d_Z_LogBlockDepth  = surfZ->GetLogBlockDepth();

        if ( ! GetDefaultChannel()->GetCh()->WaitForIdle() )
        {
            ErrPrintf("t5d-v2d: initial WaitForIdle failed");
            testStatus2d = Test::TEST_FAILED;
            return;
        }

        LWGpuResource * lwgpu = GetGpuResource();
        Verif2d *m_v2d = new Verif2d( GetDefaultChannel()->GetCh(), lwgpu, this, m_pParams );
        m_v2d->Init(V2dGpu::LW30);
        m_v2d->SetCtxSurfType( "lw4" );
        m_v2d->SetImageDumpFormat( V2dSurface::TGA );

        if( m_pParams->ParamPresent("-v2dGenCrc") ) {
            m_v2d->SetCrcData( CRC_MODE_GEN, new CrcProfile(), "" );
        }
        if( m_pParams->ParamPresent("-v2dCheckCrc") ) {
            CrcProfile *crc_profile = new CrcProfile();
            char buffer[1024];
            snprintf( buffer, 1024, "[t5d_crc]\nKEY = %s\n", m_pParams->ParamStr("-v2dCheckCrc") );
            crc_profile->LoadFromBuffer(buffer);
            m_v2d->SetCrcData( CRC_MODE_CHECK, crc_profile, "t5d_crc" );
        }

        if ( m_v2dScriptFileName.size() > 0 )
        {
            Lex lex;
            lex.Init( m_v2dScriptFileName );
            EvalElw E( m_v2d, &lex );
            Parser *p = new Parser( E );
            ScriptNode *n = p->Parse();       // parse the program:

            if ( m_v2dVariableInitStr.size() > 0 )
            {
                // extract params from string "id1=expr1,id2=expr2,..."
                vector<string> assignments = tokenize( m_v2dVariableInitStr, "," );
                for ( unsigned int i=0; i<assignments.size(); i++ )
                {
                    vector<string> a = tokenize( assignments[i], "=" );
                    if ( a.size() != 2 )
                        V2D_THROW( "error in -params string: '" <<
                                   assignments[i] << "'" );
                    n->SetSymbolUntypedValue( E, a[0], a[1]) ;

                }
            }

            string str;
            str = n->ObjectToTypedValueString( E, surf3d_C_handle );
            n->SetSymbolValue( E, "surf3d_C_obj", str );

            str = n->IntToTypedValueString( E, surf3d_C_width );
            n->SetSymbolValue( E, "surf3d_C_width", str );

            str = n->IntToTypedValueString( E, surf3d_C_height );
            n->SetSymbolValue( E, "surf3d_C_height", str );

            str = n->ObjectToTypedValueString( E, surf3d_Z_handle );
            n->SetSymbolValue( E, "surf3d_Z_obj", str );

            str = n->IntToTypedValueString( E, surf3d_Z_width );
            n->SetSymbolValue( E, "surf3d_Z_width", str );

            str = n->IntToTypedValueString( E, surf3d_Z_height );
            n->SetSymbolValue( E, "surf3d_Z_height", str );

            // let any pending 3d make its way to the FB
            //
            m_v2d->WaitForIdle();

            // create the V2dSurface objects for the C and Z buffers
            // hardcode A8R8G8B8 color / Y32 Z for format for now
            V2dSurface::Flags flags = V2dSurface::LINEAR;
            flags = (V2dSurface::Flags) ((UINT32)flags |
                                         (surf3d_C_tiled ? V2dSurface::TILED : 0));
            flags = (V2dSurface::Flags) ((UINT32)flags |
                                         (surf3d_C_swizzled ? V2dSurface::SWIZZLED : 0));

            string formatC = m_v2d->get_Csurface_format_string( surf3d_C_surface_format );
            V2dSurface::ColorFormat c_bogo_fmt = m_v2d->GetColorFormatFromString( formatC );

            int bpp_c = V2dSurface::ColorFormatToSize( c_bogo_fmt );
            if( bpp_c == 16 ) {
                formatC = "RF32_GF32_BF32_AF32";
            }

            V2dSurface *surf3dC_obj;
//V2dSurface::V2dSurface( Verif2d *v2d, UINT32 width, UINT32 height, UINT32 depth,
//                        string format, Flags flags, UINT32 offset, const string &where,
//                        ArgReader *pParams, UINT32 log2blockHeight, UINT32 log2blockDepth )
            surf3dC_obj = new V2dSurface( m_v2d, surf3d_C_width, surf3d_C_height, 1,
                                          formatC.c_str(),
                                          flags,
                                          surfC[0]->GetCtxDmaOffsetGpu(),
                                          surf3d_C_pitch,
                                          surf3d_C_baseAddr,
                                          surf3d_C_handle,
                                          true, // preExisting
                                          "", // where (not used if preExisting)
                                          m_pParams,
                                          surf3d_C_LogBlockHeight /* 4 */,
                                          surf3d_C_LogBlockDepth  /* 0 */,
                                          0
                );
            surf3dC_obj->Init();

            flags = (V2dSurface::Flags) ((UINT32)flags |
                                         (surf3d_Z_tiled ? V2dSurface::TILED : 0));
            flags = (V2dSurface::Flags) ((UINT32)flags |
                                         (surf3d_Z_swizzled ? V2dSurface::SWIZZLED : 0));

            string formatZ = m_v2d->get_Zsurface_format_string( surf3d_Z_surface_format );
            V2dSurface::ColorFormat z_bogo_fmt = m_v2d->GetColorFormatFromString( formatZ );
            int bpp = V2dSurface::ColorFormatToSize( z_bogo_fmt );
            formatZ = "Y32";
            switch (bpp) {
            case 2: formatZ = "Y16"; break;
            case 4: formatZ = "Y32"; break;
            case 8: formatZ = "RF32_GF32"; break;
            default:
                V2D_THROW( "error unrecognized number of bpp for Z surface '" <<
                                   bpp << "'" );

            }

            V2dSurface *surf3dZ_obj;
            surf3dZ_obj = new V2dSurface( m_v2d, surf3d_Z_width, surf3d_Z_height, 1,
                                          formatZ.c_str(),
                                          flags,
                                          surfZ->GetCtxDmaOffsetGpu(),
                                          surf3d_Z_pitch,
                                          surf3d_Z_baseAddr,
                                          surf3d_Z_handle,
                                          true, // preExisting
                                          "", // where (not used if preExisting)
                                          m_pParams,
                                          surf3d_Z_LogBlockHeight /* 4 */,
                                          surf3d_Z_LogBlockDepth  /* 0 */,
                                          0
                );
            surf3dZ_obj->Init();

            n->Evaluate( E );                    // run the program
            n->DumpSymbolTable( E );

            // WFI before EvalElw destructor frees all allocated surfaces:
            m_v2d->WaitForIdle();
        }

        // wait for gpu to finish pending commands
        m_v2d->WaitForIdle();

        bool v2d_check_passed = true;

        if ( m_v2d->GetDstSurface() != 0 )
        {
            if ( m_v2dDoCheckCrc ) {
                v2d_check_passed = m_v2d->CompareDstSurfaceToCRC( "v2d.tga", m_v2dExpectedCrc );
            } else {
                InfoPrintf( "dst surface CRC: 0x%08x (nothing supplied to compare it to)\n",
                            m_v2d->GetDstSurface()->CalcCRC() );
            }
        }
        else
            InfoPrintf( "t5d: no dst surface found so no crc or surface compare checks were run\n" );

        delete m_v2d;
        InfoPrintf("t5d: Run() ends (pass) \n");
        //
        // figure out test status in the tc5d+v2d case
        //
        if (!v2d_check_passed) {
            testStatus2d = Test::TEST_FAILED;
        }
        return;
    }

    catch ( string s )
    {
        ErrPrintf( "t5d: Error:%s\n", s.c_str() );
        testStatus2d = Test::TEST_FAILED;
        return;
    }
}

bool Trace5dTest::RunAfterExelwteMethods()
{
    InfoPrintf("did 3d part\n");
    InfoPrintf("trace_cels loop count %d\n", tcLoopCount);
    InfoPrintf("trace_cels method count %d\n", tcMethodCount);

    // if -v2d is given, it operates automatically (and only) like -2dAfter, and all other -inter*
    // 2d command line args are ignored
    MASSERT(m_doV2d); // all t5d tests should be v2d-based now, not legacy 2D
    Rulw2dTest();
    return true;
}

bool Trace5dTest::AfterExelwteMethods()
{
    MASSERT(m_doV2d); // all t5d tests should be v2d-based now, not legacy 2D
    return true; // v2d has its own checking mechanism
}

void Trace5dTest::CheckValidationBuffer(void)
{
    // important. Without this, we will not be able to get correct value from FB
    if (!GetDefaultChannel()->GetCh()->WaitForIdle())
    {
        ErrPrintf("WaitForIdle failed");
        return;
    }

    MASSERT(!interleaveWithBlit); // obsolete test
    MASSERT(!interleaveWithRect); // obsolete test
    MASSERT(!interleaveWithScaled); // obsolete test
    MASSERT(!interleaveWithDvd10); // obsolete test
}

int Trace5dTest::WriteMethod(int subchannel, UINT32 address, UINT32 data)
{
    return GetDefaultChannel()->GetCh()->MethodWrite(subchannel, address, data);
}

bool Trace5dTest::RunSetup2d()
{
    if (!Standard2DSetup())
    {
        ErrPrintf("Couldn't set up 2D class\n");
        return false;
    }

    MASSERT(m_doV2d); // all t5d tests should be v2d-based now, not legacy 2D
    return true; // v2d does all its own 2d setup
}

int Trace5dTest::Standard2DSetup()
{
    MASSERT(m_doV2d); // all t5d tests should be v2d-based now, not legacy 2D
    return 1;
}

Test::TestStatus Trace5dTest::CheckPassFail(void)
{
    GetDefaultChannel()->GetCh()->WaitForIdle();

    // changed -- now we let the 3d crc check go through even if v2d is in use.  It's up to the
    // testgen level writer to specify -no_check if the 3d buffer is not to be checked.   We want
    // to call CheckPassFail so that the -dumpImages code will work
    //

    if (testStatus2d == Test::TEST_SUCCEEDED)
    {
        InfoPrintf("2d tests did not report any failures.\n");
        status = Trace3DTest::CheckPassFail();
    }
    else
    {
        Trace3DTest::CheckPassFail();
        InfoPrintf("that was the 3d check, the 2d part did not pass\n");
        getStateReport()->crcCheckFailed();
        status = testStatus2d;
    }

    if (!params->ParamPresent("-doubleXor")) {
        WarnPrintf("The results of this test might not be very meaningful, as the 3d crc should fail\n");
    }
    return status;
}
