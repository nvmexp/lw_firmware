/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
/**
 * @file   glrandom.cpp
 * @brief  GLRandom -- an OpenGL based random 3D graphics test.
 */

#include "glrandom.h"
#include "opengl/glgpugoldensurfaces.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/platform.h"
#include "gpu/utility/chanwmgr.h"
#include "core/include/golddb.h"
#include "core/include/xp.h"
#include "core/include/utility.h"
#include "gpu/js/fpk_comm.h"
#include <math.h>
#include "gpu/include/gpudev.h"
#include "opengl/mglcoverage.h"
#include "core/include/version.h"
using namespace GLRandom;
using namespace TestCoverage;

INT32 GLRandomTest::s_TraceSetupRefCount = 0;

ProgProperty& GLRandom::operator++(ProgProperty& p, int)
{
    int temp = p;
    return p = static_cast<ProgProperty>(++temp);
}
ProgProperty& GLRandom::operator--(ProgProperty& p, int)
{
    int temp = p;
    return p = static_cast<ProgProperty>(--temp);
}

bool GLRandom::TxfReq::operator<(const TxfReq &rhs) const
{
    // Note: we don't consider IU.mmLevel in this operation
    if(Attrs != rhs.Attrs)
        return (Attrs < rhs.Attrs);
    if(IU != rhs.IU)
        return (IU < rhs.IU);
    return (Format < rhs.Format);
}
bool GLRandom::TxfReq::operator==(const TxfReq &rhs)const
{
    // Note: we don't consider IU.mmLevel in this operation
    return (Attrs == rhs.Attrs && IU == rhs.IU && Format == rhs.Format);
}

TxfReq & GLRandom::TxfReq::operator=(const TxfReq &rhs)
{
    Attrs = rhs.Attrs;
    IU = rhs.IU;
    Format = rhs.Format;
    return *this;
}

bool GLRandom::PgmRequirements::TxcRequires
(
    ProgProperty prop,  //!< which texture-coord property
    UINT32 txAttr,      //!< a texture attribute that might be required
    InOrOut io          //!< check read vs. write properties
)
{
    UINT32 txfMask = 0;
    if ((io == ioIn) && InTxc.count(prop))
    {
        // Mask of the texture fetchers addressed by this texture-coord property
        txfMask = InTxc[prop].TxfMask;
    }
    else if(OutTxc.count(prop))
    {
        txfMask = OutTxc[prop].TxfMask;
    }
    else
    {
        // no point in even checking just return false.
        return false;
    }
    for (int i=0; i<32; i++)
    {
        if ( txfMask & (1<<i))
        {
            // This texture-fetcher is addressed using this texture-coord
            // property.  Does this fetcher have the attribute in question?
            // Check both the fetcher requirements of this program and any
            // fetcher requirements from downstream programs we know about.
            if ( (UsedTxf.count(i) && UsedTxf[i].Attrs & txAttr) ||
                 (RsvdTxf.count(i) && RsvdTxf[i].Attrs & txAttr) )
                return true;
        }
    }

    return false;
}

//------------------------------------------------------------------------------
// Non-member functions for Goldelwalues callbacks:
static void GLRandom_Print(void * pvGLRandom)
{
    GLRandomTest * pGLRandom = (GLRandomTest *)pvGLRandom;
    pGLRandom->Print(Tee::PriNormal);
    pGLRandom->m_GpuPrograms.CreateBugJsFile();
}
static UINT32 GLRandom_GetZPassPixelCount(void * pvGLRandom)
{
    GLRandomTest * pGLRandom = (GLRandomTest *)pvGLRandom;
    return pGLRandom->GetZPassPixelCount();
}

static UINT32 GLRandom_GetImageUnitChecksum(void *pvGLRandom)
{
    GLRandomTest * pGLRandom = (GLRandomTest*)pvGLRandom;
    return pGLRandom->m_Texturing.CheckTextureImageLayers();
}

double LowPrecAtan2(double a, double b)
{
    const double halfPi = LOW_PREC_PI * 0.5;
    const double result = atan2(a, b);
    if (result > halfPi)
        return halfPi;
    if (result < -halfPi)
        return -halfPi;
    return result;
}

//------------------------------------------------------------------------------
// GLRandom class constructor:
//
// Tell MSVC7 to shut up about "'this' : used in base member initializer list"
// I promise the helpers won't dereference it until after GLRandom's ctor exits.
#pragma warning( disable : 4355 )

GLRandomTest::GLRandomTest() :
        m_Misc(this)
        ,m_GpuPrograms(this)
        ,m_Texturing(this)
        ,m_Geometry(this)
        ,m_Lighting(this)
        ,m_Fog(this)
        ,m_Fragment(this)
        ,m_PrimitiveModes(this)
        ,m_VertexFormat(this)
        ,m_Vertexes(this)
        ,m_Pixels(this)
{
    SetName("GLRandomTest");

    // The order in which the helpers are added controls the order of:
    //     Restart
    //     Pick
    m_PickHelpers.push_back(&m_Misc);
    m_PickHelpers.push_back(&m_GpuPrograms);  // must be before pixels and textures
    m_PickHelpers.push_back(&m_Fragment);     // must be before pixels
    m_PickHelpers.push_back(&m_Pixels);       // must be before textures
    m_PickHelpers.push_back(&m_Texturing);
    m_PickHelpers.push_back(&m_Geometry);
    m_PickHelpers.push_back(&m_Lighting);
    m_PickHelpers.push_back(&m_Fog);
    m_PickHelpers.push_back(&m_PrimitiveModes);
    m_PickHelpers.push_back(&m_VertexFormat);
    m_PickHelpers.push_back(&m_Vertexes);

    // The order in which the helpers are added controls the order of :
    //     Print
    //     Send
    //     Playback
    //     Unplayback
    m_SendHelpers.push_back(&m_Misc);
    m_SendHelpers.push_back(&m_GpuPrograms);
    m_SendHelpers.push_back(&m_Texturing); // must be before geometry
    m_SendHelpers.push_back(&m_Geometry);  // must be before most of the others
    m_SendHelpers.push_back(&m_Lighting);
    m_SendHelpers.push_back(&m_Fog);
    m_SendHelpers.push_back(&m_Fragment);
    m_SendHelpers.push_back(&m_PrimitiveModes);
    m_SendHelpers.push_back(&m_VertexFormat);
    m_SendHelpers.push_back(&m_Pixels);    // must be second to last
    m_SendHelpers.push_back(&m_Vertexes);  // must be last

    m_pFpCtx = GetFpContext();

    size_t i;
#ifdef DEBUG
    // Don't forget to add a line above, if you add a new GLRandomHelper!
    MASSERT(m_PickHelpers.size() == RND_NUM_RANDOMIZERS);
    MASSERT(m_SendHelpers.size() == RND_NUM_RANDOMIZERS);

    for (i = 0; i < m_PickHelpers.size(); i++)
        MASSERT(0 != m_PickHelpers[i]);
    for (i = 0; i < m_SendHelpers.size(); i++)
        MASSERT(0 != m_SendHelpers[i]);
#endif
    m_UseSanitizeRSQ        = true;
    m_DisableReverseFrames  = false;
    m_CoverageLevel         = 0;
    m_pGSurfs               = NULL;
    m_FullScreen            = GL_TRUE;
    m_DoubleBuffer          = false;
    m_PrintOnSwDispatch     = false;
    m_PrintOlwpipeDispatch  = false;
    m_DoTimeQuery           = false;
    m_RenderTimePrintThreshold = 1.0;
    m_CompressLights        = true;
    m_ClearLines            = true;
    m_ColorSurfaceFormats.push_back(0);
    m_ContinueOnFrameError  = false;
    m_TgaTexImageFileName   = "";
    m_MaxFilterableFloatTexDepth = 16;
    m_MaxBlendableFloatSize = 0;
    m_RenderToSysmem        = false;
    m_ForceFbo              = false;
    m_FboWidth              = 0;
    m_FboHeight             = 0;
    m_UseSbos               = false;
    m_PrintSbos             = 0;
    m_SboWidth              = 512;
    m_SboHeight             = 25;
    m_HasF32BorderColor     = false;
    m_VerboseGCx            = false;
    m_DoGCxCycles           = GCxPwrWait::GCxModeDisabled;
    m_ForceGCxPstate        = Perf::ILWALID_PSTATE;
    m_DoRppgCycles          = false;
    m_DoTpcPgCycles         = false;
    m_ForceRppgMask         = 0x0;
    m_LogShaderCombinations = false;
    m_LogShaderOnFailure    = false;
    m_ShaderReplacement     = false;
    m_DisplayWidth          = 0;
    m_DisplayHeight         = 0;
    m_DisplayDepth          = 0;
    m_ZDepth                = 0;
    m_LoopsPerFrame         = 0;
    m_LoopsPerGolden        = 0;
    m_FrameRetries          = 10;
    m_AllowedSoftErrors     = 0;
    m_FrameReplayMs         = 0;
    m_DumpOnAssert          = true;
    m_NumLayersInFBO        = 0;
    m_PrintXfbBuffer        = -1;
    m_InjectErrorAtFrame    = -1;   //do not inject error at any frame
    m_InjectErrorAtSurface  = 0;    //inject error at first surface by default
    m_PrintFeatures         = 0;
    m_VerboseCrcData        = false;
    m_CrcLoopNum            = -1;
    m_CrcItemNum            = -1;
    // Just a guess -- see bug 855091, we hit false RM watchdog timeouts
    // when drawing 300 vertexes with a 10:1 geometry program and tessrates
    // greater than 5.  Override this from JS as needed for slower chips.
    m_MaxOutputVxPerDraw = 300 * 10 * 5 * 5;
    m_MaxVxMultiplier = 10 * 5 * 5;

    m_ClearLinesDL       = 0;
    m_OcclusionQueryId   = 0;
    m_RunningZpassCount  = 0;
    m_pTimer = NULL;

    m_DispatchMode = mglUnknown;
    m_SWRenderTimeSecs = 0.0;
    m_HWRenderTimeSecs = 0.0;
    m_HWRenderTimeSecsFrameTotal = 0.0;
    m_SWRenderTimeSecsFrameTotal = 0.0;
    m_IsDrawing = false;

    memset(&m_FrameErrs, 0, sizeof(m_FrameErrs));

    m_LogMode = Skip;
    m_LogState = lsUnknown;
    //m_LogItemCrcs;

    memset(&m_Features, 0, sizeof(m_Features));

    // lwTracing options
    memset(&m_RegistryTraceInfo, -1, sizeof(m_RegistryTraceInfo));
    memset(&m_MyTraceInfo, -1, sizeof(m_MyTraceInfo));
    m_LwrrentTraceAction = RND_TSTCTRL_LWTRACE_registry;

    m_TraceLevel = GLRandom::trLevelUndefined;
    m_TraceOptions = GLRandom::trOption;
    m_TraceMask0 = GLRandom::trDfltMask0;
    m_TraceMask1 = GLRandom::trDfltMask1;

    // Tell all GLRandomHelpers to initialize their FancyPicker objects.
    SetDefaultPickers(0, RND_NUM_RANDOMIZERS * RND_BASE_OFFSET);

#ifdef DEBUG
    // Make sure all FancyPickers have sane default values (only need to check
    // pick helpers)
    for (i = 0; i < m_PickHelpers.size(); i++)
        m_PickHelpers[i]->CheckInitialized();
#endif

    // Tell all PickHelpers the address of the FancyPicker context.
    for (i = 0; i < m_PickHelpers.size(); i++)
        m_PickHelpers[i]->SetContext(m_pFpCtx);
}

//------------------------------------------------------------------------------
/* virtual */GLRandomTest::~GLRandomTest()
{
}

//------------------------------------------------------------------------------
/* virtual */
bool GLRandomTest::IsSupported()
{
    GpuSubdevice * pSubdev = GetBoundGpuSubdevice();

    if (pSubdev->HasBug(240978))
    {
        Printf(Tee::PriError,
                "Disable GLRandomTest on 512MB G70 to WAR 240978\n");
        return false;
    }

    if (ChannelWrapperMgr::Instance()->UsesRunlistWrapper())
    {
        return false;
    }

    InitFromJs();

    if (m_DoGCxCycles != GCxPwrWait::GCxModeDisabled)
    {
        m_GCxBubble.reset(new GCxBubble(pSubdev));
        if (!m_GCxBubble->IsGCxSupported(m_DoGCxCycles, m_ForceGCxPstate))
        {
            return false;
        }
        if (m_VerboseGCx)
        {
            GCxImpl *pGCx = pSubdev->GetGCxImpl();
            Xp::JTMgr::DebugOpts dbgOpts = {};
            if (pGCx && OK == pGCx->GetDebugOptions(&dbgOpts))
            {
                dbgOpts.verbosePrint = Tee::PriNormal;
                pGCx->SetDebugOptions(dbgOpts);
            }
        }
    }

    if (m_DoRppgCycles)
    {
        m_RppgBubble.reset(new RppgBubble(pSubdev));
        if (!m_RppgBubble->IsSupported())
        {
            return false;
        }
    }
    if (m_DoTpcPgCycles)
    {
        m_TpcPgBubble.reset(new TpcPgBubble(pSubdev));
        if (!m_TpcPgBubble->IsSupported())
        {
            return false;
        }
    }

    if (m_DoPgCycles != LowPowerPgWait::PgDisable)
    {
        m_LowPowerPgBubble = make_unique<LowPowerPgBubble>(pSubdev);
        if (!m_LowPowerPgBubble->IsSupported(m_DoPgCycles))
        {
            return false;
        }
    }

    return m_mglTestContext.IsSupported();
}

//------------------------------------------------------------------------------
RC GLRandomTest::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());


    // Just in case someone uses the RNG during InitOpenGL, seed it.
    // This way Setup won't behave differently on the 2nd run after
    // a previous run that used the RNG.
    m_pFpCtx->Rand.SeedRandom(GetSeed(0));

#ifdef DEBUG
    // Ensure that all texture formats are consistent with color.h
    CHECK_RC(mglVerifyTexFmts());
#endif
    // Take over the screen, initialize GL library.
    CHECK_RC(m_mglTestContext.Setup());

    // setup dynamic tracing.
    SetupTrace();

    // Hook up our Goldelwalues object to the GL library.
    ColorUtils::Format zfmt;
    switch (m_ZDepth)
    {
        default:    MASSERT(0);
                    /* fall through */

        case 16:    zfmt = ColorUtils::Z16; break;

        case 24:    zfmt = ColorUtils::X8Z24; break;

        case 32:    zfmt = ColorUtils::Z24S8; break;
    }

    m_pGSurfs = new GLGpuGoldenSurfaces(GetBoundGpuDevice());

    // If the Fbo width/height have not been overriden in JS, then use the display surface size.
    if (m_ForceFbo && m_FboWidth == 0 && m_FboHeight == 0)
    {
        m_FboWidth = m_DisplayWidth;
        m_FboHeight = m_DisplayHeight;
    }

    const UINT32 w = m_FboWidth ? m_FboWidth  : m_DisplayWidth;
    const UINT32 h = m_FboHeight ? m_FboHeight : m_DisplayHeight;

    // Speedup variable to see if we should be reporting hardware/software coverage records.
    if ((Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK) &&
        GetGoldelwalues()->GetAction() != Goldelwalues::Store)
    {
        CoverageDatabase *pDB = CoverageDatabase::Instance();
        if (pDB)
        {
            m_CoverageLevel = pDB->GetCoverageLevel();
        }
    }

    // Find out some runtime data about our GL driver & graphics hardware:
    CHECK_RC( GetFeatures() );

    m_pGSurfs->SetCalcAlgorithmHint(GetGoldelwalues()->GetCalcAlgorithmHint());
    m_pGSurfs->SetExpectedCheckMethod(
        GetGoldelwalues()->GetCodes() & Goldelwalues::Crc ?
            Goldelwalues::Code::Crc :
            Goldelwalues::Code::CheckSums);
    m_pGSurfs->SetGoldensBinSize(GetGoldelwalues()->GetSurfChkOnGpuBinSize());

    // support for Multiple-Viewports
    if (m_NumLayersInFBO > 0)
    {
        m_pGSurfs->SetNumLayeredSurfaces(m_NumLayersInFBO);
        UINT32 depthTextureID, colorSurfaceID;
        m_mglTestContext.GetSurfaceTextureIDs(&depthTextureID, &colorSurfaceID);
        m_pGSurfs->SetSurfaceTextureIDs(depthTextureID, colorSurfaceID);
    }

    m_pGSurfs->DescribeSurfaces(
            w,
            h,
            mglTexFmt2Color(ColorSurfaceFormat(0)),
            zfmt,
            m_mglTestContext.GetPrimaryDisplay());

    for (UINT32 mrtIdx = 1; mrtIdx < NumColorSurfaces(); mrtIdx++)
    {

        m_pGSurfs->AddMrtColorSurface(
                mglTexFmt2Color(ColorSurfaceFormat(mrtIdx)),
                GL_COLOR_ATTACHMENT0_EXT + mrtIdx,
                w,
                h);
    }

    // Create compute shader and surface for checksum on GPU
    CHECK_RC(m_pGSurfs->CreateSurfCheckShader());

    GetGoldelwalues()->AddExtraCodeCallback("zpass_pixel_count",
        &GLRandom_GetZPassPixelCount, this);

    GetGoldelwalues()->AddExtraCodeCallback("image_unit_checksum",
        &GLRandom_GetImageUnitChecksum, this);
    UINT32 FirstGoldenLoop = GetTestConfiguration()->StartLoop();
    if ((m_LoopsPerGolden > 1) ||
        ((FirstGoldenLoop % m_LoopsPerGolden) != (m_LoopsPerGolden - 1)))
    {
        FirstGoldenLoop +=
            m_LoopsPerGolden - (FirstGoldenLoop % m_LoopsPerGolden) - 1;
    }

    GetGoldelwalues()->SetPrintCallback( &GLRandom_Print, this );

    m_Fragment.SetFullScreelwiewport();

    if (m_NumLayersInFBO > 0 &&
    (!HasExt(ExtARB_fragment_layer_viewport) || !HasExt(ExtARB_viewport_array)))
    {
        Printf(Tee::PriError, "Layered Rendering needs appropriate layer/viewport extensions.\n");
        return RC::SOFTWARE_ERROR;
    }

    // Initialize the GLRandomHelpers.
    for (UINT32 i = 0; i < RND_NUM_RANDOMIZERS; i++)
    {
        rc = m_PickHelpers[i]->InitOpenGL();
        if (rc)
        {
            Printf(Tee::PriError, "Failed to Init %s\n",
                m_PickHelpers[i]->GetName());
            return rc;
        }
    }
    if (GetUseSbos() && !HasExt(ExtLW_gpu_program5))
    {
        Printf(Tee::PriNormal, "Warning! UseSbos is not supported on this GPU\n");
        m_UseSbos = false;
    }

    if (GetUseSbos())
    {
        GLRandom::SboState sbo;
        m_GpuPrograms.GetSboState(&sbo, RndGpuPrograms::sboWrite);
        // There is no color format that will support 256bit data. So fudge
        // the width to make total size correct.
        int w = sbo.Rect[sboWidth] * (32/ColorUtils::PixelBytes(sbo.colorFmt));
        m_pGSurfs->AddSurf("Sbo",
                sbo.colorFmt, // should be RU32
                0, // no display for this surface
                GL_ARRAY_BUFFER,
                sbo.Id,
                w,
                sbo.Rect[sboHeight]);
    }

    CHECK_RC(SetupGoldens(0,
        m_pGSurfs,
        GetSeed(FirstGoldenLoop),
        true,
        nullptr));

    m_Texturing.SetTgaTexImageFileName(m_TgaTexImageFileName);
    m_Texturing.SetMaxFilterableFloatDepth(m_MaxFilterableFloatTexDepth);
    m_Texturing.SetHasF32BorderColor(m_HasF32BorderColor);

    m_Fragment.SetMaxBlendableFloatSize(m_MaxBlendableFloatSize);

    m_Lighting.CompressLights(m_CompressLights);

#if defined(GL_LW_occlusion_query)
    if (HasExt(ExtLW_occlusion_query))
        glGenOcclusionQueriesLW(1, &m_OcclusionQueryId);
#endif
    if (m_DoTimeQuery)
    {
        m_pTimer = new mglTimerQuery();
        if (!m_pTimer->IsSupported())
            m_DoTimeQuery = false;
    }

    // Compile the clear-screen display list.
    if (m_ClearLines)
    {
        m_ClearLinesDL = glGenLists(1);
        glNewList (m_ClearLinesDL, GL_COMPILE);
        DoClearLines(w, h);
        glEndList();
    }

    // If we're rendering offscreen, the user is looking at garbage right now.
    // Give them something prettier to look while they wait for frame 0.

    if (m_mglTestContext.UsingFbo())
    {
        glClear(GL_COLOR_BUFFER_BIT);
        rc = m_mglTestContext.CopyFboToFront();
    }
    if (m_DoubleBuffer)
    {
        glClear(GL_COLOR_BUFFER_BIT);
        m_mglTestContext.SwapBuffers();
    }

    // Range check the m_DoGCxCycles variable.
    if (m_DoGCxCycles > GCxPwrWait::GCxModeRTD3)
    {
        Printf(Tee::PriError,
                "DoGCxCycles=%d is invalid, only %d-%d are valid.\n",
                m_DoGCxCycles, GCxPwrWait::GCxModeDisabled, GCxPwrWait::GCxModeRTD3);
        rc = RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    if (m_DoGCxCycles != GCxPwrWait::GCxModeDisabled &&
        m_DoRppgCycles)
    {
        Printf(Tee::PriError, "Cannot run with both DoGCxCycles and DoRppgCycles set\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    if (m_DoRppgCycles && m_DoTpcPgCycles)
    {
        Printf(Tee::PriError, "Can't run both DoRppgCycles and DoTpcPgCycles\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    if (m_DoGCxCycles != GCxPwrWait::GCxModeDisabled && m_DoTpcPgCycles)
    {
        Printf(Tee::PriError, "Can't run both DoGCxCycles and DoTpcPgCycles\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    if (m_DoGCxCycles != GCxPwrWait::GCxModeDisabled)
    {
        m_GCxBubble.reset(new GCxBubble(GetBoundGpuSubdevice()));
        m_GCxBubble->SetGCxParams(
                GetBoundRmClient(),
                GetBoundGpuDevice()->GetDisplay(),
                m_mglTestContext.GetTestContext(),
                m_ForceGCxPstate,
                GetTestConfiguration()->Verbose(),
                GetTestConfiguration()->TimeoutMs(),
                m_DoGCxCycles);
        m_GCxBubble->SetPciCfgRestoreDelayMs(m_PciConfigSpaceRestoreDelayMs);
        CHECK_RC(m_GCxBubble->Setup());
    }

    if (m_DoRppgCycles)
    {
        m_RppgBubble.reset(new RppgBubble(GetBoundGpuSubdevice()));
        CHECK_RC(m_RppgBubble->PrintIsSupported(GetVerbosePrintPri()));
        CHECK_RC(m_RppgBubble->Setup(m_ForceRppgMask,
                                     m_mglTestContext.GetDisplay(),
                                     m_mglTestContext.GetPrimarySurface()));
    }

    if (m_DoTpcPgCycles)
    {
        m_TpcPgBubble.reset(new TpcPgBubble(GetBoundGpuSubdevice()));
        CHECK_RC(m_TpcPgBubble->PrintIsSupported(GetVerbosePrintPri()));
        CHECK_RC(m_TpcPgBubble->Setup(m_mglTestContext.GetDisplay(),
                                     m_mglTestContext.GetPrimarySurface()));
    }

    if (m_DoPgCycles != LowPowerPgWait::PgDisable)
    {
        m_LowPowerPgBubble.reset(new LowPowerPgBubble(GetBoundGpuSubdevice()));
        m_LowPowerPgBubble->SetFeatureId(m_DoPgCycles);
        CHECK_RC(m_LowPowerPgBubble->Setup(
            GetBoundGpuDevice()->GetDisplay(), m_mglTestContext.GetTestContext()));
        CHECK_RC(m_LowPowerPgBubble->PrintIsSupported(GetVerbosePrintPri()));
        m_LowPowerPgBubble->SetDelayAfterBubble(m_DelayAfterBubbleMs);
    }

    return rc;
}

//------------------------------------------------------------------------------
bool GLRandomTest::IsGLInitialized() const
{
    return m_mglTestContext.IsGLInitialized();
}

//--------------------------------------------------------------------------------------------------
// Activate a work pause bubble which basically waits for the GPU to go into GC5/GC6 reduced power
// state and then generates a wakeup event to get back to the proper pstate.
RC GLRandomTest::ActivateBubble()
{
    StickyRC rc;
    Tee::Priority pri = GetVerbosePrintPri();

    if ((m_DoGCxCycles != GCxPwrWait::GCxModeDisabled ||
         m_DoRppgCycles || m_DoTpcPgCycles ||
         m_DoPgCycles != LowPowerPgWait::PgDisable) &&
        GetGoldelwalues()->GetAction() != Goldelwalues::Store)
    {
        //flush the current workload
        glFinish();
        CHECK_RC(mglGetRC());

        if (m_DoGCxCycles != GCxPwrWait::GCxModeDisabled)
        {
            rc = m_GCxBubble->ActivateBubble(m_Misc.GetPwrWaitDelayMs());
        }

        if (m_DoRppgCycles)
        {
            rc = m_RppgBubble->ActivateBubble(GetTestConfiguration()->TimeoutMs());
        }

        if (m_DoTpcPgCycles)
        {
            rc =  m_TpcPgBubble->ActivateBubble(GetTestConfiguration()->TimeoutMs());
        }

        if (m_DoPgCycles != LowPowerPgWait::PgDisable)
        {
            Printf(pri, "Entering Pg bubble for loop: %d\n", m_pFpCtx->LoopNum);
            rc =  m_LowPowerPgBubble->ActivateBubble(GetTestConfiguration()->TimeoutMs());
        }

        if (RC::OK != rc)
        {
            pri = Tee::PriError;
            // Disable normal GLRandom prints because they just confuse the user and don't apply
            // to this error.
            m_PrintOnError = false;
            Printf(pri, "Loop:%d ActivateBubble() failed with %s\n",
                   m_pFpCtx->LoopNum, rc.Message());
        }
        else
        {
            Printf(pri, "Loop:%d ActivateBubble()\n", m_pFpCtx->LoopNum);
        }
        if (m_DoGCxCycles)
        {
            m_GCxBubble->PrintStats();
        }
        if (m_DoRppgCycles)
        {
            m_RppgBubble->PrintStats(pri);
        }
        if (m_DoTpcPgCycles)
        {
            m_TpcPgBubble->PrintStats(pri);
        }
        if (m_DoPgCycles != LowPowerPgWait::PgDisable)
        {
            m_LowPowerPgBubble->PrintStats(pri);
        }
    }
    return (rc);
}
//------------------------------------------------------------------------------
RC GLRandomTest::Run()
{
    RC rc = OK;

    m_FrameErrs.Good        = 0;
    m_FrameErrs.Soft        = 0;
    m_FrameErrs.HardMix     = 0;
    m_FrameErrs.HardAll     = 0;
    m_FrameErrs.Other       = 0;
    m_FrameErrs.TotalTries  = 0;
    m_FrameErrs.TotalFails  = 0;
    m_PlaybackCount         = 0;
    m_CaptureCount          = 0;
    m_MiscomparingLoops.clear();

    //------------------------------------------------------------------------
    // Apply WAR 870748
    GpuSubdevice * pSubdev           = GetBoundGpuSubdevice();
    const bool     overrideThreshold = pSubdev->HasBug(870748);
    const UINT32   threshold         = overrideThreshold ? pSubdev->GetCropWtThreshold() : 0;

    if (overrideThreshold)
    {   // disable the CROP Hybrid functionality
        pSubdev->SetCropWtThreshold(0);
    }

    DEFER
    {
        if (overrideThreshold)
        {
            pSubdev->SetCropWtThreshold(threshold);
        }
    };

    //------------------------------------------------------------------------
    // Main Loop

    UINT32 firstFrame   = 0;
    UINT32 numFrames    = 0;
    INT32 frameInc      = 1;
    if (GetGoldelwalues()->GetAction() == Goldelwalues::Store || m_DisableReverseFrames)
    {
        //Test incrementing frames
        firstFrame = GetTestConfiguration()->StartLoop() / m_LoopsPerFrame;
        UINT32 endFrame   =
                (GetTestConfiguration()->StartLoop() +
                GetTestConfiguration()->Loops() +
                m_LoopsPerFrame - 1) / m_LoopsPerFrame;
        numFrames = endFrame - firstFrame;
        frameInc = 1;
    }
    else
    {
        //Test decrementing frames
        UINT32 endFrame = GetTestConfiguration()->StartLoop() / m_LoopsPerFrame;
        firstFrame   =
                (GetTestConfiguration()->StartLoop() +
                GetTestConfiguration()->Loops() +
                m_LoopsPerFrame - 1) / m_LoopsPerFrame;
        numFrames = firstFrame - endFrame;
        frameInc = -1;
        firstFrame--;
    }

    PrintProgressInit(numFrames*m_LoopsPerFrame, "NumFrames*LoopsPerFrame\n");
    UINT32 frameIdx = 0;
    for (frameIdx = 0, m_pFpCtx->RestartNum = firstFrame;
         frameIdx < numFrames;
         frameIdx++, m_pFpCtx->RestartNum += frameInc)
    {
        UINT32 errorsThisFrame = 0;
        UINT32 triesThisFrame  = 0;
        bool   retryable       = true;
        RC     frameRc = OK;
        UINT32 triesLeft;
        for (triesLeft = m_FrameRetries+1; triesLeft; triesLeft--)
        {
            RC tmpRc = DoFrame(m_pFpCtx->RestartNum);
            triesThisFrame++;
            if (tmpRc)
            {
                if (OK == frameRc)
                {
                    frameRc = tmpRc; // remember first frame error.
                }
                errorsThisFrame++;

                switch (INT32(tmpRc))
                {
                    case RC::GOLDEN_VALUE_MISCOMPARE_Z:
                    case RC::GOLDEN_VALUE_MISCOMPARE_Z_SD2:
                    case RC::GOLDEN_VALUE_MISCOMPARE:
                    case RC::GOLDEN_VALUE_MISCOMPARE_SD2:
                    case RC::GOLDEN_EXTRA_MISCOMPARE:
                    case RC::GOLDEN_EXTRA_MISCOMPARE_SD2:
                    case RC::GOLDEN_BAD_PIXEL:
                    case RC::GOLDEN_ERROR_RATE:
                        break;

                    default:
                        // This is not a pixel error(ie, hang or ctrl-C).
                        frameRc = tmpRc;
                        retryable = false;
                }
            }
            if (0 == errorsThisFrame)
                break;   // OK on first try, no retries required.
            else if (!retryable)
                break;

        } // frame retry loop
        // We print progress updates here because the retry & regress algorithms would cause the
        // progress bar to move backwards if we tried to do it inside of DoFrame. Remember that
        // this Run() can get called multiple times when regressing a failed frame, and DoFrame()
        // can get called multiple times when miscompares are detected.
        // In addition we need to check for non-standard test runs where the user may not want to
        // run an entire frame's worth of loops. ie Loops=10 & RestartSkipCount = 20.
        PrintProgressUpdate((frameIdx+1) * m_LoopsPerFrame);

        m_FrameErrs.TotalTries += triesThisFrame;
        m_FrameErrs.TotalFails += errorsThisFrame;

        if (errorsThisFrame)
        {
            if (retryable)
            {
                Printf(Tee::PriLow, "\tframe %d failed %d of %d tries.\n",
                       m_pFpCtx->RestartNum, errorsThisFrame, triesThisFrame );

                if (errorsThisFrame == 1)
                    m_FrameErrs.Soft++;
                else if (errorsThisFrame < triesThisFrame)
                    m_FrameErrs.HardMix++;
                else
                    m_FrameErrs.HardAll++;
            }
            else
            {
                Printf(Tee::PriLow, "Non-retryable error %d at frame %d.\n",
                       INT32(frameRc), m_pFpCtx->RestartNum );
                m_FrameErrs.Other++;
            }

            if ((m_FrameErrs.Soft > m_AllowedSoftErrors) ||
                m_FrameErrs.HardMix ||
                m_FrameErrs.HardAll ||
                m_FrameErrs.Other)
            {
                if (OK == rc)
                    rc = frameRc;

                if ((!m_ContinueOnFrameError) ||
                    (INT32(frameRc) == RC::USER_ABORTED_SCRIPT) ||
                    m_FrameErrs.Other)
                    goto Cleanup;
            }
        }
        else
        {
            m_FrameErrs.Good++;
        }
    } // frame loop
    if (m_Misc.XfbEnabled(xfbModeCapture) )
        Printf(Tee::PriLow,
            "Number of loops utilizing XFB:Capture(%d)/Playback(%d) out of %d\n",
            m_CaptureCount,
            m_PlaybackCount,
            GetTestConfiguration()->Loops());

    Cleanup:

    // Report error and restart info, if necessary.

    // Check for failure due to aclwmulated bad pixels/screens drawn

    if ( OK == rc )
        rc = GetGoldelwalues()->ErrorRateTest(GetJSObject());
    else
        (void) GetGoldelwalues()->ErrorRateTest(GetJSObject());

    // Check if the opengl driver got robust-channels errors from the resman.
    // This error code is more important than the previous error code, as it
    // usually means a GPU hang.
    RC rcchan = mglGetChannelError();
    if (rcchan)
        rc = rcchan;

    if (rc != RC::OK && m_LogShaderOnFailure)
    {
        m_GpuPrograms.CreateBugJsFile();
    }

    if (m_DoGCxCycles != GCxPwrWait::GCxModeDisabled)
    {
        m_GCxBubble->PrintStats();

    }

    // Log coverage values to DVS regression
    if (GetCoverageLevel() != 0)
    {
        CoverageDatabase * pDB = CoverageDatabase::Instance();
        if (pDB)
        {
            CoverageObject * pCov = nullptr;
            string testName = GetName();
            pDB->GetCoverageObject(Hardware, hwTexture, &pCov);
            if (pCov)
            {
                SetCoverageMetric(pCov->GetName().c_str(), pCov->GetCoverage(testName));
            }

            pDB->GetCoverageObject(Hardware, hwIsa, &pCov);
            if (pCov)
            {
                SetCoverageMetric( pCov->GetName().c_str(), pCov->GetCoverage(testName));
            }

            pDB->GetCoverageObject(Software, swOpenGLExt, &pCov);
            if (pCov)
            {
                SetCoverageMetric( pCov->GetName().c_str(), pCov->GetCoverage(testName));
            }
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
RC GLRandomTest::MarkGoldenRecs()
{
    RC rc;
    string       sname = GetName();
    const char * pname = sname.c_str();

    const UINT32 startLoop  = GetTestConfiguration()->StartLoop();
    const UINT32 loops      = GetTestConfiguration()->Loops();
    const UINT32 firstFrame = startLoop / m_LoopsPerFrame;
    const UINT32 endFrame   = (startLoop + loops + m_LoopsPerFrame - 1) /
                                m_LoopsPerFrame;

    for (UINT32 frame = firstFrame; frame < endFrame; frame++)
    {
        const UINT32 endLoop = min((frame + 1) * m_LoopsPerFrame,
                                    startLoop + loops);
        UINT32       loop    = max(frame * m_LoopsPerFrame, startLoop);

        for (/**/; loop < endLoop; ++loop)
        {
            if ((m_LoopsPerGolden <= 1) ||
                (loop && (loop % m_LoopsPerGolden == m_LoopsPerGolden - 1)))
            {
                CHECK_RC(GoldenDb::MarkRecs (pname, GetSeed(loop)));
            }
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
/* virtual */
RC GLRandomTest::Cleanup ()
{
    StickyRC firstRc;

    if (m_DoPgCycles != LowPowerPgWait::PgDisable)
    {
        firstRc = m_LowPowerPgBubble->Cleanup();
    }

    if (m_DoGCxCycles != GCxPwrWait::GCxModeDisabled)
    {
        firstRc = m_GCxBubble->Cleanup();
        m_GCxBubble.reset();
    }

    if (IsGLInitialized())
    {
        // Free resources allocated by the picker modules:
        for (UINT32 i = 0; i < RND_NUM_RANDOMIZERS; i++)
        {
            firstRc = m_SendHelpers[i]->Cleanup();
        }

        // Free display lists.
        if (m_ClearLinesDL)
        {
            glDeleteLists(m_ClearLinesDL, 1);
            m_ClearLinesDL = 0;
        }

        if (m_pTimer)
        {
            delete m_pTimer;
            m_pTimer = 0;
        }
    #if defined(GL_LW_occlusion_query)
        if (HasExt(ExtLW_occlusion_query) && m_OcclusionQueryId)
        {
            GLint tmpId;
            glGetIntegerv(GL_LWRRENT_OCCLUSION_QUERY_ID_LW, &tmpId);
            if (tmpId)
                glEndOcclusionQueryLW();
            glDeleteOcclusionQueriesLW(1, &m_OcclusionQueryId);
        }
        m_OcclusionQueryId = 0;
    #endif

        m_pGSurfs->CleanupSurfCheckShader();

        CleanupTrace();
    }

    // Free memory.
    GetGoldelwalues()->ClearSurfaces();
    delete m_pGSurfs;
    m_pGSurfs = NULL;

    // Release the display, shut down the GL library.
    firstRc = m_mglTestContext.Cleanup();

    firstRc = GpuTest::Cleanup();

    return firstRc;
}

//------------------------------------------------------------------------------
RC GLRandomTest::DrawClearLines()
{
    if (!HasExt(GLRandomTest::ExtARB_viewport_array) || m_NumLayersInFBO == 0)
    {
        glCallList(m_ClearLinesDL);     //draw lines to the first viewport only.
        return OK;
    }

    //draw clear lines to all viewports
    GLfloat w = static_cast<float>(m_FboWidth);
    GLfloat h = static_cast<float>(m_FboHeight);
    GLfloat x = m_Fragment.GetVportXIntersect();
    GLfloat y = m_Fragment.GetVportYIntersect();
    GLfloat offset = m_Fragment.GetVportSeparation();

    glViewport(0,                            0,
        static_cast<GLsizei>(x-offset),   static_cast<GLsizei>(y-offset));
    glCallList(m_ClearLinesDL);

    glViewport(static_cast<GLint>(x+offset), 0,
        static_cast<GLsizei>(w-x-offset), static_cast<GLsizei>(y-offset));
    glCallList(m_ClearLinesDL);

    glViewport(static_cast<GLint>(x+offset), static_cast<GLint>(y+offset),
        static_cast<GLsizei>(w-x-offset), static_cast<GLsizei>(h-y-offset));
    glCallList(m_ClearLinesDL);

    glViewport(0,                            static_cast<GLint>(y+offset),
        static_cast<GLsizei>(x-offset),   static_cast<GLsizei>(h-y-offset));
    glCallList(m_ClearLinesDL);
    glFlush();

    //copy the depth and color surfaces of first layer to the remaining layers
    UINT32 textures[2];
    m_mglTestContext.GetSurfaceTextureIDs(&textures[0], &textures[1]);
    for (GLuint i = 0; i < 2; i++)
    {
        for (GLint slice = 1; slice < m_NumLayersInFBO; slice++)
        {
            glCopyImageSubDataEXT(textures[i],//srcName
                      GL_TEXTURE_2D_ARRAY_EXT,//srcTarget
                      0, 0, 0,                //srcLevel, srcX, srcY
                      0, /*src = 0th slice*/  //src slice
                      textures[i],            //dstName
                      GL_TEXTURE_2D_ARRAY_EXT,//dstTarget
                      0, 0, 0,                //dstLevel, dstX, dstY
                      slice,                  //dst slice
                      static_cast<GLsizei>(w),//srcWidth
                      static_cast<GLsizei>(h),//srcHeight
                      1);                     //srcDepth
        }
    }
    return mglGetRC();
}

//------------------------------------------------------------------------------
RC GLRandomTest::DoFrame (UINT32 frame)
{
    StickyRC rc;
    const UINT32 BeginLoop       = frame * m_LoopsPerFrame;
    const UINT32 EndingLoop      = min((frame+1) * m_LoopsPerFrame,
                                       GetTestConfiguration()->StartLoop() +
                                       GetTestConfiguration()->Loops());
    const UINT32 FirstLoopToDraw = max(BeginLoop,
                                       GetTestConfiguration()->StartLoop());

    // Load textures, generate shaders, etc.
    m_pFpCtx->LoopNum = BeginLoop;
    CHECK_RC(RandomizeOncePerRestart(GetSeed(BeginLoop)));

    if (m_FrameReplayMs)
    {
        return DoReplayedFrame(FirstLoopToDraw, EndingLoop);
    }

    // Clear color & Z buffers.
    if (! (m_Misc.RestartSkipMask() & RND_TSTCTRL_RESTART_SKIP_clear))
    {
        CHECK_RC(ClearSurfs());
    }

    if (m_DoTimeQuery)
    {
        m_SWRenderTimeSecsFrameTotal = 0.0;
        m_HWRenderTimeSecsFrameTotal = 0.0;
    }

    m_pFpCtx->LoopNum = FirstLoopToDraw;
    GetGoldelwalues()->SetLoopAndDbIndex (FirstLoopToDraw,
                                          GetSeed(FirstLoopToDraw));

    for (/**/; m_pFpCtx->LoopNum < EndingLoop; m_pFpCtx->LoopNum++)
    {
        // Randomly pick stuff and draw it.
        rc = RandomizeOncePerDraw();
        if ((OK != rc) || m_Misc.Stop())
            break;

        const bool skipLoop = (0 != (RND_TSTCTRL_SKIP_VX & m_Misc.SkipMask()));
        const bool lastLoop = (m_pFpCtx->LoopNum == EndingLoop-1);
        const bool needSwap = (lastLoop && m_DoubleBuffer);
        const bool needGolden = (!skipLoop) && NeedGoldenRun();
        const bool needFinish = needSwap || needGolden;

        if (needFinish)
        {
            if (m_mglTestContext.UsingFbo())
            {
                m_Fragment.SetFullScreelwiewport();
                rc = m_mglTestContext.CopyFboToFront();
            }
            glFinish();
        }
        if (needSwap)
        {
            m_mglTestContext.SwapBuffers();
        }
        if (needGolden && (OK == rc))
        {
            //Inject Error
            if (m_InjectErrorAtFrame == static_cast<INT32>(m_pFpCtx->RestartNum) &&
                    GetGoldelwalues()->GetAction() == Goldelwalues::Check)
            {
                m_pGSurfs->InjectErrorAtSurface(m_InjectErrorAtSurface);
            }
            if (m_PrintSbos > 0)
                m_GpuPrograms.PrintSbos(m_PrintSbos, 0, m_SboHeight-1);

            rc = GoldenRun(needSwap);
            if (OK != rc)
            {
                m_MiscomparingLoops.insert(m_pFpCtx->LoopNum);
                if (m_DoubleBuffer && !needSwap )
                {
                    // Show the user the "bad" image.
                    m_mglTestContext.SwapBuffers();
                }
            }
        }
        else
        {
            GetGoldelwalues()->SetLoopAndDbIndex (
                    m_pFpCtx->LoopNum+1,
                    GetSeed(m_pFpCtx->LoopNum+1));
        }

        if (OK != rc)
            break;

    } // for each loop
    CHECK_RC(CleanupGLState());

    if (m_DoTimeQuery)
    {
        Printf(Tee::PriError, "RenderTime: GL/HW:%8.6fs SW:%8.6f\n",
            m_HWRenderTimeSecsFrameTotal, m_SWRenderTimeSecsFrameTotal);
    }

    // We've already incremented loop, but haven't really started drawing
    // the first loop of the next frame, so -1.
    rc = EndLoop(m_pFpCtx->LoopNum-1);
    return rc;
}

//------------------------------------------------------------------------------
// Helper function to determine if we need to call GoldenRun or not for this loop
bool GLRandomTest::NeedGoldenRun()
{
    if ((m_LoopsPerGolden <= 1) ||
        ( m_pFpCtx->LoopNum &&
          (m_pFpCtx->LoopNum % m_LoopsPerGolden == m_LoopsPerGolden-1)))
    {
        return true;
    }
    return false;
}

//------------------------------------------------------------------------------
RC GLRandomTest::DoReplayedFrame(UINT32 BeginLoop, UINT32 EndingLoop)
{
    StickyRC rc;

    const GLuint clearDL = glGenLists(3);
    const GLuint drawDL  = clearDL+1;
    const GLuint comboDL = clearDL+2;

    // Call glDeleteLists(clearDL, 3) on any return from this function.
    DEFER
    {
        glDeleteLists(clearDL, 3);
    };
    DEFERNAME(cleanupGlState)
    {
        CleanupGLState();
    };

    // Print the loop we're sending, if GL asserts.
    Platform::BreakPointHooker hooker;
    if (m_DumpOnAssert)
        hooker.Hook(GLRandom_Print, this);

    // Record a list containing the clear.
    glNewList (clearDL, GL_COMPILE);
    CHECK_RC(ClearSurfs());
    glEndList();

    // We can't change texture bindings or properties inside a display list.
    // So do the first loop's texture setup before starting the DL,
    // which is out-of-order but works in this case.
    m_pFpCtx->LoopNum = BeginLoop;
    CHECK_RC(PickOneLoop());
    PushGLState();
    CHECK_RC(m_Texturing.Send());
    PopGLState();

    // Start recording the display list, send the rest of the first loop.
    glNewList(drawDL, GL_COMPILE);
    PushGLState();
    CHECK_RC(SendOneLoop(m_Misc.SkipMask() & ~(1<<RND_TX)));
    PopGLState();

    // Record the rest of the display list.
    // RndTexturing::Send is a no-op when a display-list is being recorded.
    for (m_pFpCtx->LoopNum = BeginLoop + 1;
         m_pFpCtx->LoopNum < EndingLoop; m_pFpCtx->LoopNum++)
    {
        CHECK_RC(PickOneLoop());
        PushGLState();
        CHECK_RC(SendOneLoop(m_Misc.SkipMask()));
        PopGLState();

        //Debug feature to allow printing each loop's GLstate if Golden.SkipCount = 1
        GetGoldelwalues()->SetLoopAndDbIndex (
                m_pFpCtx->LoopNum,
                GetSeed(m_pFpCtx->LoopNum));
        // We use GoldenRun here only for the Goldelwalues callback functions
        // to print the loop's verbose data. This is for debugging only and
        // the actual CRC values will be bogas because we haven't actually sent
        // any work down the graphics pipe yet.
        if (NeedGoldenRun())
        {
            GoldenRun(true);
        }

    }
    glEndList();

    // Leave m_pFpCtx->LoopNum at EndingLoop-1 so other parts of glrandom know
    // which frame we're in.
    m_pFpCtx->LoopNum = EndingLoop-1;

    // Combine the two lists.
    glNewList(comboDL, GL_COMPILE);
    glCallList(clearDL);
    glCallList(drawDL);
    glEndList();

    // First playback will involve a lot of SW overhead, don't count
    // it as "drawing time".
    glCallList(comboDL);
    glFinish();
    rc = mglGetRC();
    if (OK != rc)
        return rc;

    // We want to keep the HW continuously busy, but not get more than
    // a few frames ahead of HW in the pushbuffer.
    const int numTimers = 4; // == max frames ahead
    mglTimerQuery timers[numTimers];
    if (!timers[0].IsSupported())
        return RC::UNSUPPORTED_FUNCTION;

    double TicksPerMs = 0.001 * (double)(INT64) Xp::QueryPerformanceFrequency();
    const INT64 start = (INT64) Xp::QueryPerformanceCounter();
    const INT64 end   = start + INT64(m_FrameReplayMs * TicksPerMs);

    // Keep the HW busy for the requested duration.
    int timerIdx = 0;
    while (end > (INT64) Xp::QueryPerformanceCounter())
    {
        timers[timerIdx].Begin();
        glCallList(comboDL);

        m_IsDrawing = true;
        if (m_mglTestContext.UsingFbo())
        {
            rc = m_mglTestContext.CopyFboToFront();
        }
        else if (m_DoubleBuffer)
        {
            m_mglTestContext.SwapBuffers();
        }
        timers[timerIdx].End();
        glFlush();
        // Wait for frame N-numTimers-1 to finish.
        timerIdx = (timerIdx + 1) % numTimers;
        timers[timerIdx].GetSeconds();
    }
    glFinish();
    m_IsDrawing = false;

    rc = mglGetRC();
    if (OK != rc)
        return rc;

    //Make sure we set the golden loop count so GoldenRun exelwtes at the end of each frame!
    GetGoldelwalues()->SetLoopAndDbIndex (
            m_pFpCtx->LoopNum,
            GetSeed(m_pFpCtx->LoopNum));
    // Check results of the last run.
    rc = GoldenRun(true);

    if (OK != rc)
        m_MiscomparingLoops.insert(m_pFpCtx->LoopNum);

    rc = EndLoop(m_pFpCtx->LoopNum);

    cleanupGlState.Cancel();
    rc = CleanupGLState();

    return rc;
}

//------------------------------------------------------------------------------
RC GLRandomTest::GoldenRun(bool alreadySwapped)
{
    if (m_mglTestContext.UsingFbo())
    {
        m_pGSurfs->SetReadPixelsBuffer(GL_COLOR_ATTACHMENT0_EXT);
    }
    else if (m_DoubleBuffer && !alreadySwapped)
    {
        m_pGSurfs->SetReadPixelsBuffer(GL_BACK);
    }
    else
    {
        m_pGSurfs->SetReadPixelsBuffer(GL_FRONT);
    }

    return GetGoldelwalues()->Run();
}

//------------------------------------------------------------------------------
void GLRandomTest::SetupTrace()
{
    if (0 == GLRandomTest::s_TraceSetupRefCount)
    {
        dglTracePushInfo(0);
    }
    GLRandomTest::s_TraceSetupRefCount++;

    // Get the current lwTrace parameters set in the registry
    // If -gltrace was passed on the command line, then this will be 50
    // otherwise it will be 0-1.
    dglTraceGetInfo(&m_RegistryTraceInfo);

    m_MyTraceInfo = m_RegistryTraceInfo;
    m_MyTraceInfo.lwTraceOptions = m_TraceOptions;
    m_MyTraceInfo.lwTraceMask[0] = m_TraceMask0;
    m_MyTraceInfo.lwTraceMask[1] = m_TraceMask1;
    m_LwrrentTraceAction = RND_TSTCTRL_LWTRACE_registry;

}
void GLRandomTest::CleanupTrace()
{
    // restore original trace parameters
    GLRandomTest::s_TraceSetupRefCount--;
    if (0 == GLRandomTest::s_TraceSetupRefCount)
    {
        dglTracePopInfo();
    }
}

//------------------------------------------------------------------------------
void GLRandomTest::ConfigTrace(UINT32 action)
{
    if (m_LwrrentTraceAction == action)
        return;

    LW_TRACE_INFO * pTraceInfo = nullptr;
    switch (action)
    {
        case RND_TSTCTRL_LWTRACE_registry:
            pTraceInfo = &m_RegistryTraceInfo;
            break;
        case RND_TSTCTRL_LWTRACE_apply_off:
            m_MyTraceInfo.lwTraceLevel = trLevelOff;
            pTraceInfo = &m_MyTraceInfo;
            break;
        case RND_TSTCTRL_LWTRACE_apply_on:
            m_MyTraceInfo.lwTraceLevel = trLevelOn;
            pTraceInfo = &m_MyTraceInfo;
            break;
        default:
            Printf(Tee::PriError,
                   "Bad TSTCTRL_LWTRACE value %d, allowed: 0,1,2\n", action);
            return;
    }
    MASSERT(pTraceInfo != nullptr);

    dglTraceSetInfo(pTraceInfo);
    m_LwrrentTraceAction = action;
}

//------------------------------------------------------------------------------
RC GLRandomTest::ClearSurfs()
{
    // Clear the screen to a pretty blue color.
    // If we clear to black, then unlit or back-facing polygons are invisible.

    const GLfloat * pClearColor = m_Misc.ClearColor();
    // Set the clear depth value to 1.0 to initialze the ZLwll RAMS to a known value.
    glClearDepth(1.0);
    glClearColor(pClearColor[0],pClearColor[1],pClearColor[2],pClearColor[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glFlush();

    if (m_Misc.RestartSkipMask() & RND_TSTCTRL_RESTART_SKIP_rectclear)
    {
        // Debug feature, draw a rect over the whole screen.
        // Should have no visible effect, nor change the checksums, but it
        // may undo the color-compression done by the clear.

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(RND_GEOM_WorldLeft,   RND_GEOM_WorldRight,
        RND_GEOM_WorldBottom, RND_GEOM_WorldTop,
        RND_GEOM_WorldNear,   RND_GEOM_WorldFar  );

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glTranslated(0.0, 0.0, -(RND_GEOM_WorldFar+RND_GEOM_WorldNear)/2);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_ALWAYS);

        glColor4fv(m_Misc.ClearColor());

        glBegin(GL_TRIANGLES);
        glVertex3f(RND_GEOM_WorldLeft,RND_GEOM_WorldTop+RND_GEOM_WorldHeight,0);
        glVertex3f(RND_GEOM_WorldLeft,RND_GEOM_WorldBottom,0);
        glVertex3f(RND_GEOM_WorldRight+RND_GEOM_WorldWidth,
            RND_GEOM_WorldBottom,0);
        glEnd();

        glFlush();

        glDisable(GL_DEPTH_TEST);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
    }

    if (m_ClearLines)
        DrawClearLines();

    glFinish();
    return mglGetRC();
}
//------------------------------------------------------------------------------
RC GLRandomTest::CleanupGLState()
{
    StickyRC rc;
    for (int i = 0; i < RND_NUM_RANDOMIZERS; i++)
    {
        rc = m_SendHelpers[i]->CleanupGLState();
    }
    rc = mglGetRC();
    return rc;
}
//------------------------------------------------------------------------------
RC GLRandomTest::RandomizeOncePerRestart(UINT32 Seed)
{
    RC rc = OK;
    const char * RestartFailed = "";

    // Reset the random-number generator.
    m_pFpCtx->Rand.SeedRandom(Seed);

    //
    // We will reseed at the end of this function, so that what happens
    // within the Generate* functions doesn't change the draw sequence later.
    // This makes debugging easier...
    //
    UINT32 nextSeed = m_pFpCtx->Rand.GetRandom();

    m_Misc.Restart();

    if (m_Misc.Stop())
        return OK;

    SetLogMode((eLogMode) m_Misc.LogMode());

    Printf(m_Misc.SuperVerbose() ?
           Tee::PriNormal :
           Tee::PriLow,
           "\n\tLoop %d\tRestartPointReset: seed %x\n", m_pFpCtx->LoopNum,Seed);

    m_RunningZpassCount = 0;

    ConfigTrace(m_Misc.RestartTraceAction());

    for (int i = 1/*m_Misc done separately above*/; i < RND_NUM_RANDOMIZERS; i++)
    {
        if (OK != (rc = m_PickHelpers[i]->Restart()))
        {
            RestartFailed = m_PickHelpers[i]->GetName();
            goto ReportErrors;
        }
    }

    m_pFpCtx->Rand.SeedRandom(nextSeed);

    ReportErrors:
    if (rc)
    {
        Printf(Tee::PriError, "\nRestart%s Error: %s.\n",
               RestartFailed, rc.Message());
    }
    return rc;
}

string RndHelperToString(uint32 helper)
{
    #define RND_HELPER_STRING(x)   case x: return #x
    switch (helper)
    {
        RND_HELPER_STRING(RND_TSTCTRL);
        RND_HELPER_STRING(RND_GPU_PROG);
        RND_HELPER_STRING(RND_TX);
        RND_HELPER_STRING(RND_GEOM);
        RND_HELPER_STRING(RND_LIGHT);
        RND_HELPER_STRING(RND_FOG);
        RND_HELPER_STRING(RND_FRAG);
        RND_HELPER_STRING(RND_PRIM);
        RND_HELPER_STRING(RND_VXFMT);
        RND_HELPER_STRING(RND_PXL);
        RND_HELPER_STRING(RND_VX);
        default:
        {
            return Utility::StrPrintf("Unknown RndHelper:%d", helper);
        }
    }
}

//------------------------------------------------------------------------------
RC GLRandomTest::PickOneLoop()
{
    // Pick random state.
    // The order we pick in is important!
    m_Misc.PickFirst();

    // Set up for CRC'ing our pick results.
    SetLogMode((eLogMode)m_Misc.LogMode());
    LogBegin(RND_NUM_RANDOMIZERS, RndHelperToString);

    // If not logging golden values capture the vertex data but don't playback
    if (m_LogMode != Store)
        m_Misc.XfbDisablePlayback();

    for (int i = 1/*m_Misc done separately above*/; i < RND_NUM_RANDOMIZERS; i++)
    {
        m_PickHelpers[i]->Pick();
    }

    m_Misc.PickLast();

    // Store CRCs of picks or check CRCs of picks vs. database,
    // to verify glrandom is sending consistent commands to OpenGL.
    RC rc = GLRandomTest::LogFinish("Pick");

    // Enable/disable GL driver tracing.
    ConfigTrace(m_Misc.TraceAction());

    return rc;
}

//------------------------------------------------------------------------------
void GLRandomTest::PushGLState()
{
    // "Push" all GL state, so we can "pop" it after drawing.
    // This allows each loop to be fairly independent of the others,
    // which makes isolating failures easier.
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushClientAttrib(GL_ALL_ATTRIB_BITS);

    glMatrixMode(GL_PROJECTION);    // setup a simple world to match
    glPushMatrix();                 // the size of the texture object
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    for (int txc = 0; txc < NumTxCoords(); txc++)
    {
        glActiveTexture(GL_TEXTURE0_ARB + txc);
        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
    }
}

//------------------------------------------------------------------------------
void GLRandomTest::PopGLState()
{
    for (int txc = 0; txc < NumTxCoords(); txc++)
    {
        glActiveTexture(GL_TEXTURE0_ARB + txc);
        glMatrixMode(GL_TEXTURE);
        glPopMatrix();
    }

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glPopClientAttrib();
    glPopAttrib();
}

//------------------------------------------------------------------------------
RC GLRandomTest::SendOneLoop(UINT32 skipMask)
{
    StickyRC rc;

    // In SuperVerbose mode, we will print each section before sending.
    for (int i = 0; i < RND_NUM_RANDOMIZERS; i++)
    {
        if (skipMask & (1 << i))
            continue;

        if (m_Misc.SuperVerbose() )
            m_SendHelpers[i]->Print(Tee::PriNormal);

        rc = m_SendHelpers[i]->Send();
        rc = mglGetRC();
        if (rc)
        {
            Printf(Tee::PriError, "\n%s Send failed.\n",
                   m_SendHelpers[i]->GetName());
            return rc;
        }
    }

    if (m_Misc.XfbEnabled(xfbModeCapture) && m_Misc.XfbIsCapturePossible())
        m_CaptureCount++;

    if (m_Misc.XfbEnabled(xfbModeCaptureAndPlayback) &&
        m_Misc.XfbIsPlaybackPossible())
    {
        m_PlaybackCount++;

        for (int i = 0; i < RND_NUM_RANDOMIZERS; i++)
        {
            if (skipMask & (1 << i))
                continue;
            rc = m_SendHelpers[i]->Playback();
            if (rc)
            {
                Printf(Tee::PriError, "\n%s Playback failed.\n",
                       m_SendHelpers[i]->GetName());
                return rc;
            }
        }
        for (int i = 0; i < RND_NUM_RANDOMIZERS; i++)
        {
            if (skipMask & (1 << i))
                continue;
            rc = m_SendHelpers[i]->UnPlayback();
            if (rc)
            {
                Printf(Tee::PriError, "\n%s UnPlayback failed.\n",
                       m_SendHelpers[i]->GetName());
                return rc;
            }
        }
    }

    glFlush();  // Tell GL to write the PUT pointer.
    rc = mglGetRC();
    return rc;
}

//------------------------------------------------------------------------------
RC GLRandomTest::RandomizeOncePerDraw()
{
    INT64 start = m_DoTimeQuery ? (INT64) Xp::QueryPerformanceCounter() : 0;
    StickyRC rc;

    CHECK_RC(PickOneLoop());

    if (m_DoTimeQuery)
    {
        INT64 end = (INT64) Xp::QueryPerformanceCounter();
        m_SWRenderTimeSecs = (float)(end-start)/
            (float)Xp::QueryPerformanceFrequency();
        m_SWRenderTimeSecsFrameTotal += m_SWRenderTimeSecs;
    }

    if (m_Misc.Stop())
        return OK;

    if (m_Misc.SkipMask() == RND_TSTCTRL_SKIP_all)
        return OK;  // no drawing, but maintain the random sequence.

    if (m_Misc.SuperVerbose() )
        Printf(Tee::PriNormal, "RandomizeOncePerDraw  Loop %d\n",
            m_pFpCtx->LoopNum);

    // Print the loop we're sending, if GL asserts.
    Platform::BreakPointHooker hooker;
    if (m_DumpOnAssert)
        hooker.Hook(GLRandom_Print, this);

    // Maintaining the random order is critical when trying to isolate a failure down to
    // a specific loop. Therefore we absolutely can't make any GetRandom() calls to the
    // fancy picker context, so disable it during Send() and we will get an MASSERT if
    // there is an attempt to get another random value.
    m_pFpCtx->Rand.Enable(false);

    // Save all GL state, so we can restore it after drawing.
    PushGLState();

    if (m_DoTimeQuery)
        m_pTimer->Begin();

#if defined(GL_LW_occlusion_query)
    if (m_Misc.CountPixels())
        glBeginOcclusionQueryLW(m_OcclusionQueryId);
#endif

    // Send our picks to the hardware.
    rc = SendOneLoop(m_Misc.SkipMask());

    // Check whether GL used HW/software/hybrid rendering for this loop.
    m_DispatchMode = mglGetDispatchMode();

    if (OK == rc)
    {
        if (m_DoTimeQuery)
            m_pTimer->End();

        if (m_Misc.Finish())
            glFinish();  // Wait for gpu to finish.

#if defined(GL_LW_occlusion_query)
        if (m_Misc.CountPixels())
        {
            GLuint tmpCount;
            glEndOcclusionQueryLW();
            glGetOcclusionQueryuivLW(m_OcclusionQueryId, GL_PIXEL_COUNT_LW,
                                     &tmpCount);
            m_RunningZpassCount += tmpCount;
        }
#endif
        if (m_DoTimeQuery)
            m_HWRenderTimeSecsFrameTotal += m_pTimer->GetSeconds();

        PopGLState();
    }
    // Check if RM robust-channel stuff detected errors (i.e. a gpu hang).
    rc = mglGetChannelError();

    // (Debug feature) Log and check dispatch-mode.
    // The GL driver should consistently use HW/vpipe/sw mode based on what
    // commands we sent, on a given gpu type.
    LogBegin(1);
    LogData(0, &m_DispatchMode, sizeof(m_DispatchMode));
    RC dmCheckRc = LogFinish("DispatchMode");
    if (OK != dmCheckRc)
    {
        rc = dmCheckRc;
        Printf(Tee::PriError, "Dispatch mode mismatch at loop %d\n",
               m_pFpCtx->LoopNum);
    }
    if (m_Misc.SuperVerbose() && (m_DispatchMode != mglUnknown))
    {
        Printf(Tee::PriNormal, "DispatchMode: %s\n",
               StrDispatchMode(m_DispatchMode));
    }

    if (rc)
    {
        Printf(Tee::PriError, "\nDraw error in loop %d: %s.\n",
               m_pFpCtx->LoopNum, rc.Message());
    }
    if ((rc  && m_PrintOnError) ||
        (m_PrintOnSwDispatch && (m_DispatchMode == mglSoftware)) ||
        (m_PrintOlwpipeDispatch && (m_DispatchMode == mglVpipe)) ||
        (m_DoTimeQuery && (m_HWRenderTimeSecs >= m_RenderTimePrintThreshold))
       )
    {
        // don't use PriError, it messes up the output of the shaders.
        Print(Tee::PriNormal);
    }

    // allow access to the fancy picker context.
    m_pFpCtx->Rand.Enable(true);
    return rc;
}

//------------------------------------------------------------------------------
void GLRandomTest::Print(Tee::Priority pri)
{
    Printf(pri, "Loop %d\n", m_pFpCtx->LoopNum);

    int i;
    for (i = 0; i < RND_NUM_RANDOMIZERS; i++)
    {
        if (m_Misc.SkipMask() & (1 << i))
            continue;

        m_SendHelpers[i]->Print(pri);
    }

    if (m_DispatchMode != mglUnknown)
        Printf(pri, "DispatchMode: %s\n", StrDispatchMode(m_DispatchMode));
    if (m_DoTimeQuery)
        Printf(pri, "RenderTime: SW:%8.6fs HW:%8.6fs\n",
            m_SWRenderTimeSecs, m_HWRenderTimeSecs);

    Printf(pri, "(Loop %d)\n", m_pFpCtx->LoopNum);
}

//------------------------------------------------------------------------------
UINT32 GLRandomTest::GetZPassPixelCount()
{
    return m_RunningZpassCount;
}

//------------------------------------------------------------------------------
void GLRandomTest::GetSboSize(GLint * pW, GLint * pH) const
{
    *pW = (GLint)m_SboWidth;
    *pH = (GLint)m_SboHeight;
}

//------------------------------------------------------------------------------
bool GLRandomTest::IsFsaa() const
{
    return GetTestConfiguration()->GetFSAAMode() != FSAADisabled;
}

//------------------------------------------------------------------------------
const char * GLRandomTest::StrDispatchMode(MglDispatchMode m) const
{
    switch (m)
    {
        case mglSoftware: return "software";
        case mglVpipe:    return "vpipe";
        case mglHardware: return "hardware";
        case mglUnknown:  return "unknown";

        default:          MASSERT(0); // shouldn't get here
            return "IllegalDispatchMode!";
    }
}

//------------------------------------------------------------------------------
void GLRandomTest::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);
    // Print the options we are going with:

    const char * TF[2] = { "false", "true"};
    const char * gc6[4] = {"disabled", "GC5", "GC6", "RTD3"};
    const char * sbos[4] = {"none", "read", "write", "read & write"};
    Printf(pri, "GLRandom controls:\n");
    Printf(pri, "\t%-32s %s\n",  "FullScreen:",        TF [m_FullScreen]);
    Printf(pri, "\t%-32s %s\n",  "DoubleBuffer:",      TF [m_DoubleBuffer]);
    Printf(pri, "\t%-32s %s\n",  "PrintOnSwDispatch:", TF [m_PrintOnSwDispatch]);
    Printf(pri, "\t%-32s %s\n",  "PrintOlwpipeDispatch:", TF [m_PrintOlwpipeDispatch]);
    Printf(pri, "\t%-32s %s\n",  "DoTimeQuery:",      TF [m_DoTimeQuery]);
    Printf(pri, "\t%-32s %f sec\n","RenderTimePrintThreshold:", m_RenderTimePrintThreshold);
    Printf(pri, "\t%-32s %s\n",  "CompressLights:",    TF [m_CompressLights]);
    Printf(pri, "\t%-32s %s\n",  "ClearLines:",        TF [m_ClearLines]);

    for (UINT32 i = 0; i < NumColorSurfaces(); i++)
    {
        Printf(pri, "\tColorSurfaceFormat[%d]:\t\t0x%x (%s)\n",
                i,
                ColorSurfaceFormat(i),
                mglEnumToString(ColorSurfaceFormat(i)));
    }

    Printf(pri, "\t%-32s %s\n", "RenderToSysmem:",     TF [m_RenderToSysmem]);
    Printf(pri, "\t%-32s %s\n", "ForceFbo:",           TF [m_ForceFbo]);
    if (m_FboWidth)
        Printf(pri, "\t%-32s %d\n",  "FboWidth:",      m_FboWidth);
    if (m_FboHeight)
        Printf(pri, "\t%-32s %d\n", "FboHeight:",      m_FboHeight);
    Printf(pri, "\t%-32s %s\n",   "UseSbos:",          TF[m_UseSbos]);
    Printf(pri, "\t%-32s %s\n",   "PrintSbos:",        sbos[m_PrintSbos&0x3]);
    Printf(pri, "\t%-32s %d\n",   "SboWidth:",         m_SboWidth);
    Printf(pri, "\t%-32s %d\n",   "SboHeight:",        m_SboHeight);
    Printf(pri, "\t%-32s %s\n",   "ContinueOnFrameError:", TF [m_ContinueOnFrameError]);
    Printf(pri, "\t%-32s %s\n",   "DisableReverseFrames:", TF [m_DisableReverseFrames]);
    Printf(pri, "\t%-32s %d\n",   "FrameRetries:",     m_FrameRetries);
    Printf(pri, "\t%-32s %d\n",   "AllowedSoftErrors:",m_AllowedSoftErrors);
    Printf(pri, "\t%-32s %d\n",   "FrameReplayMs:",    m_FrameReplayMs);
    Printf(pri, "\t%-32s %s\n",   "VerboseGCx:",       TF[m_VerboseGCx]);
    Printf(pri, "\t%-32s %s\n",   "DoGCxCycles:",      gc6[m_DoGCxCycles]);
    Printf(pri, "\t%-32s %d\n",   "ForceGCxPstate:",   m_ForceGCxPstate);
    Printf(pri, "\t%-32s %s\n",   "LogShaderCombinations:", TF [m_LogShaderCombinations]);
    Printf(pri, "\t%-32s %s\n",   "LogShaderOnFailure:", TF [m_LogShaderOnFailure]);
    Printf(pri, "\t%-32s %s\n",   "ShaderReplacement:", TF [m_ShaderReplacement]);
    Printf(pri, "\t%-32s %d\n",   "PrintXfbBuffer on loop:", (INT32)m_PrintXfbBuffer);
    Printf(pri, "\t%-32s %d\n",   "TraceLevel:",       m_TraceLevel);
    Printf(pri, "\t%-32s 0x%x\n", "TraceOptions:",     m_TraceOptions);
    Printf(pri, "\t%-32s 0x%x\n", "TraceMask0:",       m_TraceMask0);
    Printf(pri, "\t%-32s 0x%x\n", "TraceMask1:",       m_TraceMask1);
    Printf(pri, "\t%-32s %s\n",   "PrintFeatures:",    TF [m_PrintFeatures]);
    Printf(pri, "\t%-32s %s\n",   "VerboseCrcData:",   TF [m_VerboseCrcData]);
    Printf(pri, "\t%-32s %d\n",   "CrcLoopNum:",       m_CrcLoopNum);
    Printf(pri, "\t%-32s %d\n",   "CrcItemNum:",       m_CrcItemNum);
    Printf(pri, "\t%-32s %d\n",   "LoopsPerProgram:",  m_LoopsPerProgram);
    if (m_NumLayersInFBO)
    {
        Printf(pri, "\t%-32s %d\n","NumLayersInFBO:",  m_NumLayersInFBO);
    }

    if (m_TgaTexImageFileName.size())
    {
        Printf(pri, "\t%-32s %s\n",  "TgaTexImageFileName:", m_TgaTexImageFileName.c_str());
    }
    Printf(pri, "\t%-32s %d\n", "MaxFilterableFloatTexDepth:", m_MaxFilterableFloatTexDepth);
    Printf(pri, "\t%-32s %d\n", "MaxBlendableFloatSize:", m_MaxBlendableFloatSize);
    Printf(pri, "\t%-32s %s\n", "HasF32BorderColor:",  TF [m_HasF32BorderColor]);
    Printf(pri, "\t%-32s %u\n", "MaxOutputVxPerDraw:", m_MaxOutputVxPerDraw);
    Printf(pri, "\t%-32s %u\n", "MaxVxMultiplier:",    m_MaxVxMultiplier);
    if (m_InjectErrorAtFrame != -1)
    {
        Printf(pri, "\t%-32s %d\n",  "InjectErrorAtFrame:", m_InjectErrorAtFrame);
        Printf(pri, "\t%-32s %d\n",  "InjectErrorAtSurface:", m_InjectErrorAtSurface);
    }
}

//------------------------------------------------------------------------------
// Get user options from JavaScript world into our file-globals.
//
RC GLRandomTest::InitFromJs()
{
    RC            rc = OK;

    // Tell TestConfiguration and Goldelwalues to get their user options
    // and print them at low priority.
    CHECK_RC( GpuTest::InitFromJs() );

    // Get GLRandom-specific properties from JavaScript:

    m_DisplayWidth    = GetTestConfiguration()->SurfaceWidth();
    m_DisplayHeight   = GetTestConfiguration()->SurfaceHeight();
    m_DisplayDepth    = GetTestConfiguration()->DisplayDepth();
    m_ZDepth          = GetTestConfiguration()->ZDepth();
    m_LoopsPerFrame   = GetTestConfiguration()->RestartSkipCount();
    m_LoopsPerGolden  = GetGoldelwalues()->GetSkipCount();

    // Validate the options, fix them up if necessary:

    if (!m_LoopsPerFrame)
        m_LoopsPerFrame = 1;
    if (!m_LoopsPerGolden)
        m_LoopsPerGolden = 1;
    if (m_LoopsPerFrame != m_LoopsPerGolden)
        m_FrameRetries = 0;

    CHECK_RC( m_mglTestContext.SetProperties(
            GetTestConfiguration(),
            m_DoubleBuffer,
            GetBoundGpuDevice(),
            GetDispMgrReqs(),
            ColorSurfaceFormat(0),
            m_ForceFbo,
            m_RenderToSysmem,
            m_NumLayersInFBO));

    m_ColorSurfaceFormats[0] = m_mglTestContext.ColorSurfaceFormat(0);

    for (UINT32 mrtIdx = 1; mrtIdx < NumColorSurfaces(); mrtIdx++)
    {
        m_mglTestContext.AddColorBuffer(ColorSurfaceFormat(mrtIdx));
    }
    if (m_FboWidth && m_FboHeight)
    {
        m_mglTestContext.SetSurfSize(m_FboWidth, m_FboHeight);
    }

    return rc;
}

//------------------------------------------------------------------------------
jsval GLRandomTest::ColorSurfaceFormatsToJsval() const
{
    JavaScriptPtr  pJavaScript;
    JsArray jsa;

    for (UINT32 mrtIdx = 0; mrtIdx < NumColorSurfaces(); mrtIdx++)
    {
        jsval jsv;
        if (OK == pJavaScript->ToJsval(ColorSurfaceFormat(mrtIdx), &jsv))
        {
            jsa.push_back(jsv);
        }
    }
    jsval retval;
    pJavaScript->ToJsval(&jsa, &retval);

    return retval;
}

//------------------------------------------------------------------------------
void GLRandomTest::ColorSurfaceFormatsFromJsval(jsval jsv)
{
    JavaScriptPtr  pJavaScript;
    JsArray jsa;

    m_ColorSurfaceFormats.clear();

    if (OK == pJavaScript->FromJsval(jsv, &jsa))
    {
        // Array case -- MRT if size > 1.
        for (UINT32 mrtIdx = 0; mrtIdx < jsa.size(); mrtIdx++)
        {
            UINT32 x;
            if (OK == pJavaScript->FromJsval(jsa[mrtIdx], &x))
            {
                m_ColorSurfaceFormats.push_back(x);
            }
        }
        if (m_ColorSurfaceFormats.size() == 0)
            m_ColorSurfaceFormats.push_back(0);
    }
    else
    {
        // Legacy scalar case -- not MRT.
        UINT32 x = 0;
        pJavaScript->FromJsval(jsv, &x);
        m_ColorSurfaceFormats.push_back(x);
    }
}
//------------------------------------------------------------------------------
UINT32 GLRandomTest::GetCoverageLevel()
{
    return m_CoverageLevel;
}

//------------------------------------------------------------------------------
// Expose protected function to the helper objects.
Goldelwalues *  GLRandomTest::GetGoldelwalues()
{
    return GpuTest::GetGoldelwalues();
}

//------------------------------------------------------------------------------
RC GLRandomTest::SetDefaultPickers(UINT32 first, UINT32 last)
{
    if (last >= RND_NUM_RANDOMIZERS * RND_BASE_OFFSET)
        last = RND_NUM_RANDOMIZERS * RND_BASE_OFFSET - 1;

    if (first > last)
        return RC::SOFTWARE_ERROR;

    // Colwert JS picker-indices, which encode the helper number
    // to C++ per-helper picker-indices for the correct helpers
    int firstPickIdx = first / RND_BASE_OFFSET;
    int lastPickIdx  = last / RND_BASE_OFFSET;
    int start        = first % RND_BASE_OFFSET;

    for(int pickIdx = firstPickIdx; pickIdx <= lastPickIdx; pickIdx++ )
    {
        // valid picker indices are 0-(RND_BASE_OFFSET-1)
        int end = last - pickIdx * RND_BASE_OFFSET;
        end = min(end,(int)RND_BASE_OFFSET-1);
        GLRandomHelper *pHelper;

        RC rc = OK;
        rc = HelperFromPickerIndex(pickIdx, &pHelper);
        if (OK == rc)
        {
            for ( int i=start; i <= end && rc == OK; i++)
                rc = pHelper->SetDefaultPicker(i);
        }

        start = 0;  // subsequent passes must start a zero
    }

    return OK;
}

//------------------------------------------------------------------------------
RC GLRandomTest::PickerFromJsval(int index, jsval value)
{
    if ((index < 0) || (index >= RND_NUM_RANDOMIZERS * RND_BASE_OFFSET))
        return RC::BAD_PICKER_INDEX;

    RC rc;
    GLRandomHelper *pHelper;
    CHECK_RC(HelperFromPickerIndex(index / RND_BASE_OFFSET, &pHelper));
    CHECK_RC(pHelper->PickerFromJsval(index % RND_BASE_OFFSET, value));
    return rc;
}

//------------------------------------------------------------------------------
RC GLRandomTest::PickerToJsval(int index, jsval * pvalue)
{
    if ((index < 0) || (index >= RND_NUM_RANDOMIZERS * RND_BASE_OFFSET))
        return RC::BAD_PICKER_INDEX;

    RC rc;
    GLRandomHelper *pHelper;
    CHECK_RC(HelperFromPickerIndex(index / RND_BASE_OFFSET, &pHelper));
    CHECK_RC(pHelper->PickerToJsval(index % RND_BASE_OFFSET, pvalue));
    return rc;
}

//------------------------------------------------------------------------------
RC GLRandomTest::PickToJsval(int index, jsval * pvalue)
{
    if ((index < 0) || (index >= RND_NUM_RANDOMIZERS * RND_BASE_OFFSET))
        return RC::BAD_PICKER_INDEX;

    RC rc;
    GLRandomHelper *pHelper;
    CHECK_RC(HelperFromPickerIndex(index / RND_BASE_OFFSET, &pHelper));
    CHECK_RC(pHelper->PickToJsval(index % RND_BASE_OFFSET, pvalue));
    return rc;
}

//------------------------------------------------------------------------------
UINT32 GLRandomTest::GetSeed(UINT32 loop) const
{
    MASSERT (m_LoopsPerFrame);  // Should have been set non-zero by InitFromJs.

    const UINT32 frame  = loop / m_LoopsPerFrame;

    if (m_FrameSeeds.size() > frame)
    {
        const UINT32 offset = loop % m_LoopsPerFrame;
        return m_FrameSeeds[frame] + offset;
    }
    else
    {
        return GetTestConfiguration()->Seed() + loop;
    }
}

//------------------------------------------------------------------------------
UINT32 GLRandomTest::GetFrame() const
{
    if (m_LoopsPerFrame)
    {
        return GetLoop() / m_LoopsPerFrame;
    }
    return 0;
}

//------------------------------------------------------------------------------
UINT32 GLRandomTest::GetFrameSeed() const
{
    // Since FrameSeed is a property of this test, GetFrameSeed() will be called
    // to obtain the 'default' FrameSeed value when the -testhelp parameter is
    // used, even though the test is not fully exelwted. In that case,
    // m_LoopPerFrame will not have been set from JS, and thus we need to check
    // for NULL here in order to avoid an assertion from the GetSeed() function.
    if (m_LoopsPerFrame)
    {
        return GetSeed(m_LoopsPerFrame * GetFrame());
    }
    return 0;
}

//------------------------------------------------------------------------------
bool GLRandomTest::GetIsDrawing() const
{
    return m_IsDrawing;
}

//------------------------------------------------------------------------------
void GLRandomTest::DoClearLines(UINT32 TestWidth, UINT32 TestHeight)
{
    // enable depth, stencil
    glShadeModel(GL_SMOOTH);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    if (m_DisplayDepth > 16)
    {
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_ALWAYS, GL_ALWAYS, GL_ALWAYS);
        glStencilOp(GL_INCR_WRAP_EXT, GL_ILWERT, GL_INCR_WRAP_EXT);
    }

    GLubyte Colors[4][4] =
    {
        {  0,  0,  0,255},   // opaque black
        {255,255,255,  0},   // transparent white
        {255,255,255,255},   // opaque white
        {  0,  0,  0,  0}    // transparent black
    };

    glBegin(GL_LINES);

    int      line;
    int      xNumSteps = TestWidth / 32;
    int      yNumSteps = TestHeight / 32;

    GLfloat  xStep    = RND_GEOM_WorldWidth  / (GLfloat)xNumSteps;
    GLfloat  yStep    = RND_GEOM_WorldHeight / (GLfloat)yNumSteps;
    int      zModulo  = 9;
    GLfloat  zStep    = RND_GEOM_WorldDepth / (zModulo - 1.0f);

    // A fan of lines across the whole screen, from the upper left corner.
    GLfloat startX = RND_GEOM_WorldLeft;
    GLfloat startY = RND_GEOM_WorldTop;
    for (line = 1; line < (xNumSteps + yNumSteps); line++)
    {
        GLfloat stopX;
        GLfloat stopY;

        if (line < xNumSteps)
        {
            stopX = RND_GEOM_WorldLeft + line * xStep;
            stopY = RND_GEOM_WorldBottom;
        }
        else
        {
            stopX = RND_GEOM_WorldRight;
            stopY = RND_GEOM_WorldBottom + (line - xNumSteps) * yStep;
        }

        GLfloat startZ = -(RND_GEOM_WorldDepth / 2.0f) +
                            (line % zModulo) * zStep;
        GLfloat stopZ  = -(RND_GEOM_WorldDepth / 2.0f) +
                            ((line + 4) % zModulo) * zStep;

        glColor4ubv(Colors[line & 3]);
        glVertex3f(startX, startY, startZ);
        glColor4ubv(Colors[(line+1) & 3]);
        glVertex3f(stopX, stopY, stopZ);
    }

    // A fan of lines across the whole screen, from the lower right corner.
    startX = RND_GEOM_WorldRight;
    startY = RND_GEOM_WorldBottom;
    for (line = 1; line < (xNumSteps + yNumSteps); line++)
    {
        GLfloat stopX;
        GLfloat stopY;

        if (line < yNumSteps)
        {
            stopX = RND_GEOM_WorldLeft;
            stopY = RND_GEOM_WorldBottom + line * yStep;
        }
        else
        {
            stopX = RND_GEOM_WorldLeft + (line - yNumSteps) * xStep;
            stopY = RND_GEOM_WorldTop;
        }

        GLfloat startZ = -(RND_GEOM_WorldDepth / 2.0f) +
                            (line % zModulo) * zStep;
        GLfloat stopZ  = -(RND_GEOM_WorldDepth / 2.0f) +
                            ((line + 5) % zModulo) * zStep;

        glColor4ubv(Colors[line & 3]);
        glVertex3f(startX, startY, startZ);
        glColor4ubv(Colors[(line+2) & 3]);
        glVertex3f(stopX, stopY, stopZ);
    }

    glEnd();
    glShadeModel(GL_FLAT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
}

#if defined(GLRANDOM_PICK_LOGGING)
//------------------------------------------------------------------------------
void GLRandomTest::SetLogMode(GLRandomTest::eLogMode m)
{
    switch (m)
    {
        case Skip:
        case Store:
        case Check:
            m_LogMode = m;
            break;

        default:
            m_LogMode = Skip;
            break;
    }
}

GLRandomTest::eLogMode GLRandomTest::GetLogMode() const
{
    return m_LogMode;
}

//------------------------------------------------------------------------------
void GLRandomTest::LogBegin(size_t numItems, string (*pItemToString)(uint32))
{
    MASSERT( m_LogState == lsUnknown || m_LogState == lsFinish);
    m_LogItemToString = pItemToString;
    m_LogState = lsBegin;

    m_LogItemCrcs.resize(numItems);

    size_t i;
    for (i = 0; i < numItems; i++)
        m_LogItemCrcs[i] = ~(UINT32)0;
}

//------------------------------------------------------------------------------
UINT32 GLRandomTest::LogData(int item, const void * data, size_t dataSize)
{
    MASSERT( m_LogState == lsBegin || m_LogState == lsLog);
    m_LogState = lsLog;
    size_t size = dataSize;

    if (m_LogMode != Skip)
    {
        UINT32 tmp = m_LogItemCrcs[item];

        const unsigned char * p = (const unsigned char *)data;
        while (dataSize--)
        {
            tmp = (tmp >> 3) | ((tmp & 7) << 28);
            tmp = tmp ^ *p++;
        }
        m_LogItemCrcs[item] = tmp;
    }
    if (m_VerboseCrcData)
    {
        string strItem = (m_LogItemToString) ? m_LogItemToString(item) :
                                               Utility::StrPrintf("%d", item);
        Printf(Tee::PriNormal, "Loop:%04d m_LogItemsCrc[%s] = 0x%x\n",
               m_pFpCtx->LoopNum, strItem.c_str(), m_LogItemCrcs[item]);
        const unsigned char * p = (const unsigned char *)data;
        if (m_pFpCtx->LoopNum == (UINT32)m_CrcLoopNum && item == m_CrcItemNum)
        {
            size_t i = 0;
            while ( i < size )
            {
                for (size_t j = 0; (j < 16) && (i < size); j++, i++)
                {
                    Printf(Tee::PriNormal, "0x%02x ", *(p + i));
                }
                Printf(Tee::PriNormal, "\n");
            }
        }
    }
    return (m_LogItemCrcs[item]);
}

//------------------------------------------------------------------------------
RC GLRandomTest::LogFinish(const char * label)
{
    MASSERT( m_LogState == lsBegin || m_LogState == lsLog);
    m_LogState = lsFinish;

    RC rc = OK;

    if ((m_LogMode == Skip) || (m_LogItemCrcs.size() == 0))
        return OK;

    // Get a handle (database index) into the golden db.

    string testName = "GLR_";
    testName.append(label);
    string recName = GetGoldelwalues()->GetName();
    const UINT32 DbIndex = GetSeed(m_pFpCtx->LoopNum);

    GoldenDb::RecHandle dbHandle;
    if (OK != (rc = GoldenDb::GetHandle(testName.c_str(),
                                        recName.c_str(),
                                        0,
                                        (UINT32)m_LogItemCrcs.size(),
                                        &dbHandle)))
    {
        m_LogMode = Skip;
        return rc;
    }

    if (m_LogMode == Store)
    {
        // Store the logged data to the golden db.

        if (OK != (rc = GoldenDb::PutRec(dbHandle, DbIndex, &(m_LogItemCrcs[0]),
            false)))
        {
            m_LogMode = Skip;
            return rc;
        }
    }
    else
    {
        // Check the logged data against the "correct" values from the golden db.

        MASSERT(m_LogMode == Check);
        const UINT32 * pStoredData;

        if (OK != (rc = GoldenDb::GetRec(dbHandle, DbIndex, &pStoredData)))
        {
            m_LogMode = Skip;
            return rc;
        }
        size_t i;
        for (i = 0; i < m_LogItemCrcs.size(); i++)
        {
            if (pStoredData[i] != m_LogItemCrcs[i])
            {
                string strItem = (m_LogItemToString) ? m_LogItemToString((unsigned int)i) : 
                                                   Utility::StrPrintf("%d", (unsigned int)i);
                Printf(Tee::PriError,
                   "!!! Loop %d (seed %d) %s %s inconsistent! (expected 0x%x, got 0x%x)\n",
                   m_pFpCtx->LoopNum,
                   DbIndex,
                   label,
                   strItem.c_str(),
                   pStoredData[i],
                   m_LogItemCrcs[i]
                  );
                if (OK == rc)
                {
                    rc = RC::SOFTWARE_ERROR;
                }
            }
        }
        return rc;
    }
    return OK;
}

#else //  defined(GLRANDOM_PICK_LOGGING)

void GLRandomTest::SetLogMode(GLRandomTest::eLogMode ignored)
{
    m_LogMode = Skip;
}
GLRandomTest::eLogMode GLRandomTest::GetLogMode() const
{
    return Skip;
}
void GLRandomTest::LogBegin(size_t numItems, string (*pItemToString)(uint32))
{
}
UINT32 GLRandomTest::LogData(int item, const void * data, size_t dataSize)
{
    return 0;
}
RC GLRandomTest::LogFinish(const char * label)
{
    return OK;
}

#endif // defined(GLRANDOM_PICK_LOGGING)

//------------------------------------------------------------------------------
void GLRandomTest::PickNormalfv(GLfloat * fv, bool PointsFront)
{
    // Generate an x,y,z direction vector with this length, making sure
    // that it points to the indicated side of the Z=0 plane.

    // Imagine the earth, north pole pointing in the +Z direction,
    // with the prime meridian (0 degrees East) to the right...

    const double d2r = 3.1415/180.0;
    double north = d2r * m_pFpCtx->Rand.GetRandom(0, 90);
    double east  = d2r * m_pFpCtx->Rand.GetRandom(0, 359);

    if ( ! PointsFront )
        north *= -1.0;      // away-facing normal, southern hemisphere

    fv[0] = (GLfloat) (cos(east)*cos(north));
    fv[1] = (GLfloat) (sin(east)*cos(north));
    fv[2] = (GLfloat) sin(north);
}

//------------------------------------------------------------------------------
RC GLRandomTest::GetFeatures()
{
    const Tee::Priority featPri = m_PrintFeatures ? Tee::PriNormal : Tee::PriLow;
    const char * ExtStrings[ ExtNUM_EXTENSIONS ] =
    {
        "GL_ARB_depth_texture"
        ,"GL_ARB_multisample"
        ,"GL_ARB_shadow"
        ,"GL_ARB_texture_border_clamp"
        ,"GL_ARB_texture_non_power_of_two"
        ,"GL_ATI_texture_float"
        ,"GL_EXT_blend_equation_separate"
        ,"GL_EXT_blend_func_separate"
        ,"GL_EXT_depth_bounds_test"
        ,"GL_EXT_fog_coord"
        ,"GL_EXT_packed_float"
        ,"GL_EXT_point_parameters"
        ,"GL_EXT_secondary_color"
        ,"GL_EXT_shadow_funcs"
        ,"GL_EXT_stencil_two_side"
        ,"GL_EXT_texture3D"
        ,"GL_EXT_texture_lwbe_map"
        ,"GL_EXT_texture_edge_clamp"
        ,"GL_EXT_texture_filter_anisotropic"
        ,"GL_EXT_texture_integer"
        ,"GL_EXT_texture_lod_bias"
        ,"GL_EXT_texture_mirror_clamp"
        ,"GL_EXT_texture_shared_exponent"
        ,"GL_IBM_texture_mirrored_repeat"
        ,"GL_LW_depth_clamp"
        ,"GL_LW_float_buffer"
        ,"GL_LW_fog_distance"
        ,"GL_LW_fragment_program"
        ,"GL_LW_fragment_program2"
        ,"GL_LW_fragment_program3"
        ,"GL_LW_half_float"
        ,"GL_LW_light_max_exponent"
        ,"GL_LW_occlusion_query"
        ,"GL_LW_point_sprite"
        ,"GL_LW_primitive_restart"
        ,"GL_LW_vertex_program"
        ,"GL_LW_vertex_program1_1"
        ,"GL_LW_vertex_program2"
        ,"GL_LW_vertex_program3"
        ,"GL_SGIS_generate_mipmap"
        ,"GL_LW_transform_feedback"
        ,"GL_LW_gpu_program4"
        ,"GL_ARB_vertex_buffer_object"
        ,"GL_LW_gpu_program4_0"
        ,"GL_LW_gpu_program4_1"
        ,"GL_LW_explicit_multisample"
        ,"GL_ARB_draw_buffers"
        ,"GL_EXT_draw_buffers2"
        ,"GL_ARB_draw_buffers_blend"
        ,"GL_EXT_texture_array"
        ,"GL_ARB_texture_lwbe_map_array"
        ,"GL_EXTX_framebuffer_mixed_formats"  // EXTX is not a typo!
        ,"GL_LW_gpu_program5"
        ,"GL_LW_gpu_program_fp64"
        ,"GL_LW_gpu_program5"  // In place of GL_LW_tessellation_program,
                               //see bug 719997
        ,"GL_LW_parameter_buffer_object2"
        ,"GL_LW_bindless_texture"
        ,"GL_AMD_seamless_lwbemap_per_texture"
        ,"GL_LW_blend_equation_advanced_coherent"
        ,"GL_ARB_draw_instanced"
        ,"GL_ARB_fragment_layer_viewport"
        ,"GL_ARB_viewport_array"
        ,"GL_LW_viewport_array2"
        ,"GL_LW_geometry_shader_passthrough"
        ,"GL_LW_shader_atomic_fp16_vector"
        ,"GL_EXT_texture_filter_minmax"
        ,"GL_ARB_texture_view"
        ,"GL_ARB_sparse_texture"
        ,"GL_ARB_compute_variable_group_size"
        ,"GL_LW_shader_atomic_float"
        ,"GL_LW_shader_atomic_float64"
        ,"GL_LW_shader_atomic_int64"
        ,"GL_KHR_texture_compression_astc_ldr"
        ,"GL_LW_fragment_shader_barycentric"
        ,"GL_LW_shading_rate_image"
        ,"GL_LW_conservative_raster"
        ,"GL_LW_conservative_raster_pre_snap"
        ,"GL_LW_conservative_raster_pre_snap_triangles"
        ,"GL_LW_conservative_raster_underestimation"
        ,"GL_LW_conservative_raster_dilate"
        ,"GL_LW_draw_texture"
    };

    const char * driverVer = (const char*) glGetString(GL_VERSION);
    Printf(featPri, "\tOpenGL driver version:\t%s\n", driverVer);

    // Too hard to view the extensions in a single line, so replace each
    // ' ' with a '\n' prior to printing. Also Printf() has an internal
    // 4K buffer, so make sure we don't overflow.
    const char * extensions = (const char*) glGetString(GL_EXTENSIONS);

    Printf(featPri, "GL extension string = \n");
    vector<string> unSupportedExt;

    if (NULL != extensions)
    {
        string s;

        do
        {
            const char * end = strchr(extensions, ' ');
            const char * next = end;
            if (NULL != end)
            {
                s.assign(extensions, end-extensions);
                next = end + 1;
            }
            else
            {
                s = extensions;
            }

            Printf(featPri, "\t%s\n", s.c_str());
            if (m_PrintFeatures)
            {
                // Keep track of the features we dont support.
                bool found = false;
                for (UINT32 i = 0; i < ExtNUM_EXTENSIONS; i++)
                {
                    if (s == ExtStrings[i])
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    unSupportedExt.push_back(s);
                }
            }
            extensions = next;
        }
        while (NULL != extensions && '\0' != *extensions);
    }
    if (m_PrintFeatures)
    {
        Printf(featPri, "GL extensions not known to GLRandom:\n");
        for (UINT32 i = 0; i < unSupportedExt.size(); i++)
        {
            Printf(featPri,"%s\n\t-",unSupportedExt[i].c_str());
        }
    }

    GLint ext;
    for ( ext = 0; ext < ExtNUM_EXTENSIONS; ext++ )
    {
        m_Features.HasExt[ ext ] = mglExtensionSupported(ExtStrings[ext]);
    }

    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &m_Features.NumTxUnits);

#if defined(GL_LW_fragment_program)
    if ( m_Features.HasExt[ ExtLW_fragment_program ] )
    {
        glGetIntegerv(GL_MAX_TEXTURE_COORDS_LW,      &m_Features.NumTxCoords);
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS_LW, &m_Features.NumTxFetchers);
    }
    else
#endif
    {
        m_Features.NumTxCoords     = m_Features.NumTxUnits;
        m_Features.NumTxFetchers   = m_Features.NumTxUnits;
    }
    if (m_Features.HasExt[ExtLW_vertex_program3])
    {
        glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS_ARB,
            &m_Features.NumVtxTexFetchers);
    }
    else
    {
        m_Features.NumVtxTexFetchers = 0;
    }

    if (MaxTxCoords < m_Features.NumTxCoords)
    {
        MASSERT(!"Too many tx-coords");
        Printf(Tee::PriError,
               "GL reports %d tx coords, GLRandom supports only %d!\n",
               m_Features.NumTxCoords, MaxTxCoords);
        m_Features.NumTxCoords = MaxTxCoords;
    }
    if (MaxTxFetchers < m_Features.NumTxFetchers)
    {
        MASSERT(!"Too many tx-fetchers");
        Printf(Tee::PriError,
               "GL reports %d tx fetchers, GLRandom supports only %d!\n",
               m_Features.NumTxFetchers, MaxTxFetchers);
        m_Features.NumTxFetchers = MaxTxFetchers;
    }
    if (MaxVxTxFetchers < m_Features.NumVtxTexFetchers)
    {
        MASSERT(!"Too many vtxtx-fetchers");
        Printf(Tee::PriError,
           "GL reports %d vtxtx fetchers, GLRandom supports only %d!\n",
           m_Features.NumVtxTexFetchers, MaxVxTxFetchers);
        m_Features.NumVtxTexFetchers = MaxVxTxFetchers;
    }

    if ( m_Features.HasExt[ ExtEXT_texture_filter_anisotropic ] )
    {
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,
            &m_Features.MaxMaxAnisotropy);
    }
    else
    {
        m_Features.MaxMaxAnisotropy = 1.0;
    }

    if ( m_Features.HasExt[ ExtLW_light_max_exponent ] )
    {
        glGetIntegerv(GL_MAX_SHININESS_LW, &m_Features.MaxShininess);
        glGetIntegerv(GL_MAX_SPOT_EXPONENT_LW, &m_Features.MaxSpotExponent);
    }
    else
    {
        m_Features.MaxShininess = 128;
        m_Features.MaxSpotExponent = 128;
    }

    if ( m_Features.HasExt[ ExtEXT_texture_array ])
    {
        glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS_EXT,
            (GLint *)&m_Features.MaxArrayTextureLayers);
    }
    else
    {
        m_Features.MaxArrayTextureLayers = 0;
    }

    glGetIntegerv(GL_MAX_TEXTURE_SIZE,(GLint *)&m_Features.Max2dTextureSize);

    if (m_Features.HasExt[ ExtEXT_texture_lwbe_map ])
    {
        glGetIntegerv(GL_MAX_LWBE_MAP_TEXTURE_SIZE_EXT,
            (GLint *)&m_Features.MaxLwbeTextureSize);
    }
    else
    {
        m_Features.MaxLwbeTextureSize = 0;
    }

    glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE,(GLint *)&m_Features.Max3dTextureSize);
    glGetIntegerv(GL_MAX_CLIP_PLANES, (GLint *)&m_Features.MaxUserClipPlanes);

    RC rc = mglGetRC();
    if ( OK != rc )
    {
        Printf(Tee::PriNormal, "Error reading HW/driver properties\n");
    }
    else
    {
        int nExt = 0;
        Printf(featPri,
                "GL extensions known to GLRandom and supported here:\n" );
        for ( ext = 0; ext < ExtNUM_EXTENSIONS; ext++ )
        {
            if ( m_Features.HasExt[ ext ] )
            {
                ReportExtCoverage(ExtStrings[ext]);
                Printf(featPri, " + %s\n", ExtStrings[ext] );
                nExt++;
            }
        }
        Printf(featPri,
                "GL extensions known to GLRandom but not supported here:\n" );
        for ( ext = 0; ext < ExtNUM_EXTENSIONS; ext++ )
        {
            if ( ! m_Features.HasExt[ ext ] )
            {
                Printf(featPri, "   (%s)\n", ExtStrings[ext] );
            }
        }

        if (nExt < ExtNUM_EXTENSIONS/3)
        {
            // We must never again screw up a major GPU bringup by
            // failing to detect any extensions and silently killing our
            // test coverage.
            Printf(Tee::PriError, "Only detected %d of %d GL extensions!\n",
                nExt,
                ExtNUM_EXTENSIONS);
            rc = RC::SOFTWARE_ERROR;
        }

        Printf(featPri,
            "Features: ext=%d/%d tex=%d,%d,%d,%d aniso=%.0f shin=%d clip=%d\n",
            nExt,
            ExtNUM_EXTENSIONS,
            NumTxUnits(),
            NumTxCoords(),
            NumTxFetchers(),
            NumVtxTexFetchers(),
            MaxMaxAnisotropy(),
            MaxShininess(),
            MaxUserClipPlanes() );
    }
    return rc;
}

//-------------------------------------------------------------------------------------------------
RC GLRandomTest::ReportExtCoverage(const char * pExt)
{
    if (GetCoverageLevel() != 0)
    {
        CoverageDatabase * pDB = CoverageDatabase::Instance();
        if (pDB)
        {
            RC rc;
            CoverageObject *pCov = nullptr;
            pDB->GetCoverageObject(Software,swOpenGLExt, &pCov);
            if (!pCov)
            {
                pCov = new SwOpenGLExtCoverageObject;
                pDB->RegisterObject(Software,swOpenGLExt, pCov);
            }
            pCov->AddCoverage(pExt, GetName());
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
GLboolean   GLRandomTest::HasExt(GLRandomTest::ExtensionId ex) const
{
    if (ex >= ExtNUM_EXTENSIONS)
    {
        if (ex == ExtNO_SUCH_EXTENSION)
            return GL_FALSE;

        MASSERT(0); // shouldn't get here
        return GL_FALSE;
    }

    return m_Features.HasExt[ ex ];
}

//------------------------------------------------------------------------------
GLint       GLRandomTest::NumTxUnits() const
{
    return m_Features.NumTxUnits;
}

//------------------------------------------------------------------------------
GLint       GLRandomTest::NumTxCoords() const
{
    return m_Features.NumTxCoords;
}

//------------------------------------------------------------------------------
GLint       GLRandomTest::NumVtxTexFetchers() const
{
    return m_Features.NumVtxTexFetchers;
}

//------------------------------------------------------------------------------
GLint       GLRandomTest::NumTxFetchers() const
{
    return m_Features.NumTxFetchers;
}

//------------------------------------------------------------------------------
GLfloat     GLRandomTest::MaxMaxAnisotropy() const
{
    return m_Features.MaxMaxAnisotropy;
}

//------------------------------------------------------------------------------
GLint       GLRandomTest::MaxShininess() const
{
    return m_Features.MaxShininess;
}

//------------------------------------------------------------------------------
GLint       GLRandomTest::MaxSpotExponent() const
{
    return m_Features.MaxSpotExponent;
}

//------------------------------------------------------------------------------
GLuint      GLRandomTest::Max2dTextureSize() const
{
    return m_Features.Max2dTextureSize;
}

//------------------------------------------------------------------------------
GLuint      GLRandomTest::MaxLwbeTextureSize() const
{
    return m_Features.MaxLwbeTextureSize;
}

//------------------------------------------------------------------------------
GLuint      GLRandomTest::Max3dTextureSize() const
{
    return m_Features.Max3dTextureSize;
}

//------------------------------------------------------------------------------
GLuint      GLRandomTest::MaxArrayTextureLayerSize() const
{
    return m_Features.MaxArrayTextureLayers;
}

//------------------------------------------------------------------------------
GLuint      GLRandomTest::MaxUserClipPlanes() const
{
    return m_Features.MaxUserClipPlanes;
}

//------------------------------------------------------------------------------
UINT32 GLRandomTest::ColorSurfaceFormat(UINT32 MrtIndex) const
{
    MASSERT(MrtIndex < NumColorSurfaces());

    return m_ColorSurfaceFormats[MrtIndex];
}

//------------------------------------------------------------------------------
UINT32 GLRandomTest::NumColorSurfaces() const
{
    return (UINT32) m_ColorSurfaceFormats.size();
}

UINT32 GLRandomTest::NumColorSamples() const
{
    return m_mglTestContext.NumColorSamples();
}

//------------------------------------------------------------------------------
UINT32 GLRandomTest::DispWidth() const
{
    return m_DisplayWidth;
}

//------------------------------------------------------------------------------
UINT32 GLRandomTest::DispHeight() const
{
    return m_DisplayHeight;
}

//------------------------------------------------------------------------------
void GLRandomTest::GetFrameErrCounts
        (
        UINT32 * pGood,
        UINT32 * pSoft,
        UINT32 * pHardMix,
        UINT32 * pHardAll,
        UINT32 * pOther,
        UINT32 * pTotalTries,
        UINT32 * pTotalFails
        ) const
{
    *pGood       = m_FrameErrs.Good;
    *pSoft       = m_FrameErrs.Soft;
    *pHardMix    = m_FrameErrs.HardMix;
    *pHardAll    = m_FrameErrs.HardAll;
    *pOther      = m_FrameErrs.Other;
    *pTotalTries = m_FrameErrs.TotalTries;
    *pTotalFails = m_FrameErrs.TotalFails;
}

RC GLRandomTest::FreeUserPrograms()
{
    m_GpuPrograms.FreeUserPrograms();
    return OK;
}

UINT32 GLRandomTest::StartLoop() const
{
    return const_cast<GLRandomTest*>(this)->GetTestConfiguration()->StartLoop();
}

UINT32 GLRandomTest::LoopsPerFrame() const
{
    return (UINT32)m_LoopsPerFrame;
}
bool GLRandomTest::LogShaderCombinations() const
{
    return m_LogShaderCombinations;
}
UINT32 GLRandomTest::PrintXfbBuffer() const
{
    return m_PrintXfbBuffer;
}

//-----------------------------------------------------------------------------
/* virtual */ RC GLRandomTest::SetPicker(UINT32 idx, jsval v)
{
    return PickerFromJsval(idx, v);
}

//-----------------------------------------------------------------------------
/* virtual */ UINT32 GLRandomTest::GetPick(UINT32 idx)
{
    jsval result;
    if (OK != PickToJsval(idx, &result))
        return 0;

    return JSVAL_TO_INT(result);
}

//-----------------------------------------------------------------------------
/* virtual */ RC GLRandomTest::GetPicker(UINT32 idx, jsval *v)
{
    return PickerToJsval(idx, v);
}

//-----------------------------------------------------------------------------
// Helper function for the LoadUserGPU???Program() functions
//
RC ParseJsPgmRqmts
(
    GLRandom::PgmRequirements *PgmRqmts,
    JSContext * pContext,
    jsval  arg
)
{
    UINT32 i;
    const char *syntax =
    "PgmRequirements must be array of:\n"
    "[\n"
    " [txf,flags,imageunit,mmlevel,access,elems], \n"
    " [ 0..76, flags], // InRegs\n"
    " [ 0..76, flags], // OutRegs\n"
    " [txAttr,prop,fetcher,components], //InTxCds\n"
    " NeedSbos,\n"
    " BindlessTxAttr, \n"
    " EndStrategy\n"
    "]\n";

    JavaScriptPtr pJs;
    JsArray jsTxObjAttRqd;
    JsArray jsInRegs;
    JsArray jsOutRegs;
    JsArray jsInTxCds;
    const size_t numElements = pJs->UnpackJsval(
        arg, "aaaaIII",
        &jsTxObjAttRqd,
        &jsInRegs,
        &jsOutRegs,
        &jsInTxCds,
        &PgmRqmts->NeedSbos,  // optional
        &PgmRqmts->BindlessTxAttr, //optional
        &PgmRqmts->EndStrategy); // optional
    if (numElements < 4)
    {
        JS_ReportWarning(pContext, syntax);
        return RC::BAD_PARAMETER;
    }

    for (i = 0; i < jsTxObjAttRqd.size(); i++)
    {
        UINT32 idx;
        GLRandom::TxfReq txfReq;
        const size_t numElements = pJs->UnpackJsval(
            jsTxObjAttRqd[i], "IIIIII",
            &idx,
            &txfReq.Attrs,
            &txfReq.IU.unit,
            &txfReq.IU.mmLevel,
            &txfReq.IU.access,
            &txfReq.IU.elems);

        if ((numElements != 2 && numElements != 6) ||
            (idx >= MaxVxTxFetchers))
        {
            JS_ReportWarning(pContext,
                "txfetchers must be array of pairs of [ txf, attr] "
                "or an array of [txf,attr,imageunit,mmlevel,access,elems]");
            return RC::BAD_PARAMETER;
        }
        PgmRqmts->UsedTxf[idx] = txfReq;
    }

    for (i = 0; i < jsInRegs.size(); i++)
    {
        INT32  pgmProp = 0;
        UINT32  compMask = compNone;
        const size_t numElements = pJs->UnpackJsval(jsInRegs[i], "II",
                                                    &pgmProp, &compMask);

        if ((numElements != 2) || (pgmProp >= ppNUM_ProgProperty))
        {
            JS_ReportWarning(pContext, "InRegs must be array of pairs of"
                " [ 0..76, flags]");
            return RC::BAD_PARAMETER;
        }

        PgmRqmts->Inputs[(ProgProperty)pgmProp] = compMask;
    }

    for (i = 0; i < jsOutRegs.size(); i++)
    {
        INT32   pgmProp = 0;
        UINT32  compMask = compNone;
        const size_t numElements = pJs->UnpackJsval(jsOutRegs[i], "II",
                                                    &pgmProp, &compMask);

        if ((numElements != 2) || (pgmProp >= ppNUM_ProgProperty))
        {
            JS_ReportWarning(pContext, "OutRegs must be array of pairs of"
                " [ 0..76, flags]");
            return RC::BAD_PARAMETER;
        }

        PgmRqmts->Outputs[(ProgProperty)pgmProp] = compMask;
    }

    for (i = 0; i < jsInTxCds.size(); i++)
    {
        JsArray txReq;
        UINT32   txAttr;
        INT32   pgmProp; // the pgm property that is carrying the tex coord data
        UINT32  txfMask; // A bit mask of the fetchers using this pgmProp as a
                         //texture coordinate
        if (OK == pJs->FromJsval(jsInTxCds[i], &txReq))
        {
            if (txReq.size() == 4)
            { // old style syntax, ignore last argument, colwert txfMask to a
              // bitmask
                const size_t numElems = pJs->UnpackJsArray(txReq, "III",
                                                           &txAttr,
                                                           &pgmProp,
                                                           &txfMask);
                if (numElems != 3)
                {
                    JS_ReportError(pContext, "InTxCds must be an array of 4"
                        " elements:[txAttr,prop,fetcher,components]");
                    return RC::BAD_PARAMETER;
                }
                // we should already have an entry in the UsedTxf[] for this
                // fetcher.
                if ( PgmRqmts->UsedTxf[txfMask].Attrs != txAttr)
                {
                    JS_ReportError(pContext, "txAttr in InTxCds[] & "
                        "txfetchers[] don't match");
                    return RC::BAD_PARAMETER;
                }

                // If entry doesn't exist a default one will be constructed.
                TxcReq txcReq = PgmRqmts->InTxc[(ProgProperty)pgmProp];
                txcReq.TxfMask |= 1<<txfMask;
                txcReq.Prop = (ProgProperty)pgmProp;
                PgmRqmts->InTxc[(ProgProperty)pgmProp] = txcReq;
            }
            else if (txReq.size() == 2)
            { // new style syntax
                const size_t numElems = pJs->UnpackJsArray(txReq, "II",
                                                           &pgmProp,
                                                           &txfMask);
                if (2 != numElems)
                {
                    JS_ReportError(pContext, "InTxCds must be an array of 2"
                        " elements:[prop,fetcherMask]");
                    return RC::BAD_PARAMETER;
                }
                PgmRqmts->InTxc[(ProgProperty)pgmProp] =
                    TxcReq((ProgProperty)pgmProp, txfMask);
            }
            else
            {
                JS_ReportError(pContext, "InTxCds must be an array of 2"
                    " elements:[prop,fetcherMask]");
                return RC::BAD_PARAMETER;
            }
        }
        else
        {
            JS_ReportError(pContext, "InTxCds must be an array of 3 elements:"
                "[prop,fetcher,components]");
            return RC::BAD_PARAMETER;
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
RC CheckPrimAndPatchSize
(
    JSContext * pContext,
    int prim,
    int vxPerPatch
)
{
    switch (prim)
    {
        default:
            JS_ReportError(pContext, "Invalid primitive type");
            return RC::BAD_PARAMETER;

        case GL_POINTS:
        case GL_LINES:
        case GL_LINE_STRIP:
        case GL_TRIANGLES:
        case GL_TRIANGLE_STRIP:
        case GL_LINES_ADJACENCY_EXT:
        case GL_TRIANGLES_ADJACENCY_EXT:
            return OK;

        case GL_PATCHES:
            break;
    }

    if ((vxPerPatch == -1) ||
        ((vxPerPatch >= 1) && (vxPerPatch <= GLRandom::MaxPatchVertices)))
    {
        return OK;
    }

    JS_ReportError(pContext,
            "VxPerPatch must be -1 (don't care) or 1 to 32 vertices");
    return RC::BAD_PARAMETER;
}

//------------------------------------------------------------------------------
RC ParseJsPrimRqmts
(
    GLRandom::PgmRequirements *PgmRqmts,
    JSContext * pContext,
    jsval  arg
)
{
    RC rc;
    JavaScriptPtr pJs;
    JsArray jsAr;
    const char * syntax =
            "PrimRqmts must be an integer primIn or an array of 5 elements:"
            " [primIn,primOut,vxPerPatchIn,vxPerPatchOut,vxMultiplier]";

    if (OK == pJs->FromJsval(arg, &jsAr))
    {
        // New style: [PrimitiveIn, PrimitiveOut, VxPerPatchIn, VxPerPatchOut, vxMultiplier]
        const size_t numElems = pJs->UnpackJsArray(
            jsAr, "IIIIf",
            &PgmRqmts->PrimitiveIn,
            &PgmRqmts->PrimitiveOut,
            &PgmRqmts->VxPerPatchIn,
            &PgmRqmts->VxPerPatchOut,
            &PgmRqmts->VxMultiplier);
        if (numElems < 5)
        {
            JS_ReportError(pContext, syntax);
            return RC::BAD_PARAMETER;
        }
        CHECK_RC(CheckPrimAndPatchSize(pContext,
                PgmRqmts->PrimitiveIn, PgmRqmts->VxPerPatchIn));
        CHECK_RC(CheckPrimAndPatchSize(pContext,
                PgmRqmts->PrimitiveOut, PgmRqmts->VxPerPatchOut));
    }
    else if (OK == pJs->FromJsval(arg, &PgmRqmts->PrimitiveIn))
    {
        // Old style: PrimitiveIn
        PgmRqmts->PrimitiveOut = -1;
        PgmRqmts->VxPerPatchIn = -1;
        PgmRqmts->VxPerPatchOut = -1;

        CHECK_RC(CheckPrimAndPatchSize(pContext,
                PgmRqmts->PrimitiveIn, PgmRqmts->VxPerPatchIn));
    }
    else
    {
        JS_ReportError(pContext, syntax);
        return RC::BAD_PARAMETER;
    }

    return OK;
}

//------------------------------------------------------------------------------
const vector<UINT32> & GLRandomTest::GetFrameSeeds() const
{
    return m_FrameSeeds;
}
void GLRandomTest::SetFrameSeeds(const vector<UINT32> &v)
{
    m_FrameSeeds = v;
}
const set<UINT32> & GLRandomTest::GetMiscomparingLoops() const
{
    return m_MiscomparingLoops;
}

//-----------------------------------------------------------------------------
string GLRandom::TxAttrToString(UINT32 TxAttr)
{
    string str = "unknown";
    // start with the exclusive bits:
    switch (TxAttr & SASMDimFlags)
    {
        case Is1d             : str = "Is1d";               break;
        case Is2d             : str = "Is2d";               break;
        case Is3d             : str = "Is3d";               break;
        case IsLwbe           : str = "IsLwbe";             break;
        case IsArray1d        : str = "IsArray1d";          break;
        case IsArray2d        : str = "IsArray2d";          break;
        case IsArrayLwbe      : str = "IsArrayLwbe";        break;
        case IsShadow1d       : str = "IsShadow1d";         break;
        case IsShadow2d       : str = "IsShadow2d";         break;
        case IsShadowLwbe     : str = "IsShadowLwbe";       break;
        case IsShadowArray1d  : str = "IsShadowArray1d";    break;
        case IsShadowArray2d  : str = "IsShadowArray2d";    break;
        case IsShadowArrayLwbe: str = "IsShadowArrayLwbe";  break;
        case Is2dMultisample  : str = "Is2dMultisample";    break;
    }
    // add the other flags
    if (TxAttr & IsDepth)       str += "+IsDepth";
    if (TxAttr & IsFloat)       str += "+IsFloat";
    if (TxAttr & IsSigned)      str += "+IsSigned";
    if (TxAttr & IsUnSigned)    str += "+IsUnSigned";
    if (TxAttr & IsLegacy)      str += "+IsLegacy";
    if (TxAttr & IsNoBorder)    str += "+IsNoBorder";

    return str;
}

//-----------------------------------------------------------------------------
INT32 GLRandom::MapTxAttrToTxDim(UINT32 Attr)
{
    switch (Attr & SASMDimFlags)
    {
        case IsShadow1d     :
        case Is1d           :  return GL_TEXTURE_1D;

        case IsShadow2d     :
        case Is2d           :  return GL_TEXTURE_2D;

        case Is3d           :  return GL_TEXTURE_3D;

        case IsShadowLwbe   :
        case IsLwbe         :  return GL_TEXTURE_LWBE_MAP_EXT;

        case IsShadowArray1d:
        case IsArray1d      :  return GL_TEXTURE_1D_ARRAY_EXT;

        case IsShadowArray2d:
        case IsArray2d      :  return GL_TEXTURE_2D_ARRAY_EXT;

        case IsShadowArrayLwbe:
        case IsArrayLwbe    :  return GL_TEXTURE_LWBE_MAP_ARRAY_ARB;

        case Is2dMultisample:  return GL_TEXTURE_RENDERBUFFER_LW;
        default:
            MASSERT(!"Unknown TexAttribBit");
            return GL_TEXTURE_2D;
    }
}

//------------------------------------------------------------------------------
RC GLRandomTest::HelperFromPickerIndex(int index, GLRandomHelper **ppHelper)
{
    switch(index)
    {
        case RND_TSTCTRL:
            *ppHelper = &m_Misc;
            break;
        case RND_GPU_PROG:
            *ppHelper = &m_GpuPrograms;
            break;
        case RND_TX:
            *ppHelper = &m_Texturing;
            break;
        case RND_GEOM:
            *ppHelper = &m_Geometry;
            break;
        case RND_LIGHT:
            *ppHelper = &m_Lighting;
            break;
        case RND_FOG:
            *ppHelper = &m_Fog;
            break;
        case RND_FRAG:
            *ppHelper = &m_Fragment;
            break;
        case RND_PRIM:
            *ppHelper = &m_PrimitiveModes;
            break;
        case RND_VXFMT:
            *ppHelper = &m_VertexFormat;
            break;
        case RND_PXL:
            *ppHelper = &m_Pixels;
            break;
        case RND_VX:
            *ppHelper = &m_Vertexes;
            break;
        default:
            MASSERT(!"Unknown picker index!");
            *ppHelper = NULL;
            return RC::SOFTWARE_ERROR;
    }
    return OK;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ GLRandomTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(GLRandomTest, GpuTest, "Random OpenGL test");

namespace {
void ErrorJsifcMismatch(INT32 actual)
{
    Printf(Tee::PriError, "Error: JSIFC_VERSION %d, expected %d.\n",
           actual, GLRandom::Program::JSIFC_VERSION);
}
} // anonymous namespace

JS_STEST_LWSTOM(GLRandomTest, LoadUserGpuVertexProgram, 4,
        "Load a user-defined GpuVertex program.")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    void *     pvGLRandomTest = JS_GetPrivate(pContext, pObject);
    JavaScriptPtr pJavaScript;

    if (pvGLRandomTest)
    {
        GLRandomTest * pGLRandom  = (GLRandomTest *)pvGLRandomTest;

        string   pgmString;
        UINT32   col0Alias  = 0;
        INT32    fmtVersion = 0;
        PgmRequirements pgmRqmts;
        StickyRC rc;

        if (OK != pJavaScript->FromJsval(pArguments[0], &fmtVersion))
            rc = RC::BAD_PARAMETER;

        if (fmtVersion == GLRandom::Program::JSIFC_VERSION)
        {
            if((NumArguments != 4) ||
               (OK != ParseJsPgmRqmts(&pgmRqmts,pContext,pArguments[1])) ||
               (OK != pJavaScript->FromJsval(pArguments[2], &col0Alias)) ||
               (OK != pJavaScript->FromJsval(pArguments[3], &pgmString)))
            {
                rc = RC::BAD_PARAMETER;
            }
        }
        else
        {
            ErrorJsifcMismatch(fmtVersion);
            rc = RC::BAD_PARAMETER;
        }

        if (rc != OK)
        {
            string usage = Utility::StrPrintf(
                "Usage: GLRandom.LoadUserGpuVertexProgram(\n"
                "%d,\n"
                "[[fetcher,attr,ImageUnit,MMLevel,Access,elems],\n"
                " [InRegs],[OutRegs],[InTxCds],NeedSbos,BindlessTxAttr],\n"
                "c0alias,pgmstring)\n",
                GLRandom::Program::JSIFC_VERSION);
            JS_ReportError(pContext, usage.c_str());
            return JS_FALSE;
        }

        // Note on all these casts:
        // GLuint and UINT32 are the same size and typedness, but on the Mac
        // compiler one ends up being a "long" and the other an "int", which is
        // an error.
        MASSERT(sizeof(UINT32) == sizeof(GLuint));
        rc = pGLRandom->m_GpuPrograms.AddUserVertexProgram(
                pgmRqmts,
                (VxAttribute) col0Alias,
                pgmString.c_str());

        RETURN_RC(rc);
    }
    RETURN_RC(OK);
}

JS_STEST_LWSTOM(GLRandomTest, LoadUserGpuTessCtrlProgram, 4,
        "Load a user-defined TessCtrl program.")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    void *     pvGLRandomTest = JS_GetPrivate(pContext, pObject);
    JavaScriptPtr pJavaScript;

    if (pvGLRandomTest)
    {
        GLRandomTest * pGLRandom  = (GLRandomTest *)pvGLRandomTest;
        PgmRequirements pgmRqmts;
        string   pgmString;
        INT32    fmtVersion = 0;
        StickyRC  rc;

        if (OK != pJavaScript->FromJsval(pArguments[0], &fmtVersion))
            rc = RC::BAD_PARAMETER;

        if (fmtVersion == GLRandom::Program::JSIFC_VERSION)
        {
            if((NumArguments != 4) ||
               (OK != ParseJsPgmRqmts(&pgmRqmts,pContext,pArguments[1])) ||
               (OK != ParseJsPrimRqmts(&pgmRqmts, pContext, pArguments[2])) ||
               (OK != pJavaScript->FromJsval(pArguments[3], &pgmString)))
            {
                rc = RC::BAD_PARAMETER;
            }
        }
        else
        {
            ErrorJsifcMismatch(fmtVersion);
            rc = RC::BAD_PARAMETER;
        }

        if (OK != rc)
        {
            string usage = Utility::StrPrintf(
                "Usage: GLRandomTest.LoadUserGpuTessCtrlProgram(\n"
                "%d,\n"
                "[[fetcher,attr,ImageUnit,MMLevel,Access,elems],\n"
                " [InRegs],[OutRegs],[InTxCds],NeedSbos,BindlessTxAttr],\n"
                "[primIn,primOut,vxPerPatchIn,vxPerPatchOut],\n"
                "pgmstring)\n",
                GLRandom::Program::JSIFC_VERSION);
            JS_ReportError(pContext, usage.c_str());
            return JS_FALSE;
        }

        rc = pGLRandom->m_GpuPrograms.AddUserTessCtrlProgram(
                pgmRqmts, pgmString.c_str());
        RETURN_RC(rc);
    }
    RETURN_RC(OK);
}

JS_STEST_LWSTOM(GLRandomTest, LoadUserGpuTessEvalProgram, 5,
        "Load a user-defined TessEval program.")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    void *     pvGLRandomTest = JS_GetPrivate(pContext, pObject);
    JavaScriptPtr pJavaScript;

    if (pvGLRandomTest)
    {
        GLRandomTest * pGLRandom  = (GLRandomTest *)pvGLRandomTest;
        PgmRequirements pgmRqmts;
        string   pgmString;
        INT32    fmtVersion = 0;
        StickyRC rc;
        UINT32   tessMode = 0;

        if (OK != pJavaScript->FromJsval(pArguments[0], &fmtVersion))
            rc = RC::BAD_PARAMETER;

        if (fmtVersion == GLRandom::Program::JSIFC_VERSION)
        {
            if((NumArguments != 5) ||
               (OK != ParseJsPgmRqmts(&pgmRqmts,pContext,pArguments[1])) ||
               (OK != ParseJsPrimRqmts(&pgmRqmts, pContext, pArguments[2])) ||
               (OK != pJavaScript->FromJsval(pArguments[3], &tessMode)) ||
               (OK != pJavaScript->FromJsval(pArguments[4], &pgmString))
                )
            {
                rc = RC::BAD_PARAMETER;
            }
        }
        else
        {
            ErrorJsifcMismatch(fmtVersion);
            rc = RC::BAD_PARAMETER;
        }

        if (OK != rc)
        {
            string usage = Utility::StrPrintf(
                "Usage: GLRandomTest.LoadUserGpuTessEvalProgram(\n"
                "%d,\n"
                "[[fetcher,attr,ImageUnit,MMLevel,Access,elems],\n"
                " [InRegs],[OutRegs],[InTxCd],NeedSbos,BindlessTxAttr],\n"
                "[primIn,primOut,vxPerPatchIn,vxPerPatchOut],\n"
                "tessMode,\n"
                "pgmstring)\n",
                GLRandom::Program::JSIFC_VERSION);
            JS_ReportError(pContext, usage.c_str());
            return JS_FALSE;
        }

        rc = pGLRandom->m_GpuPrograms.AddUserTessEvalProgram(
            pgmRqmts, tessMode, pgmString.c_str());
        RETURN_RC(rc);
    }
    RETURN_RC(OK);
}

JS_STEST_LWSTOM(GLRandomTest, LoadUserGpuGeometryProgram, 4,
        "Load a user-defined GpuGeometry program.")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    void *     pvGLRandomTest = JS_GetPrivate(pContext, pObject);
    JavaScriptPtr pJavaScript;

    if (pvGLRandomTest)
    {
        GLRandomTest * pGLRandom  = (GLRandomTest *)pvGLRandomTest;
        string   pgmString;
        INT32    fmtVersion = 0;
        PgmRequirements pgmRqmts;
        StickyRC rc;

        if (OK != pJavaScript->FromJsval(pArguments[0], &fmtVersion))
            rc = RC::BAD_PARAMETER;

        if (fmtVersion == GLRandom::Program::JSIFC_VERSION)
        {
            if((NumArguments != 4) ||
               (OK != ParseJsPgmRqmts(&pgmRqmts,pContext,pArguments[1])) ||
               (OK != ParseJsPrimRqmts(&pgmRqmts, pContext, pArguments[2])) ||
               (OK != pJavaScript->FromJsval(pArguments[3], &pgmString)))
            {
                rc = RC::BAD_PARAMETER;
            }
        }
        else
        {
            ErrorJsifcMismatch(fmtVersion);
            rc = RC::BAD_PARAMETER;
        }

        if (rc != OK)
        {
            string usage = Utility::StrPrintf(
                "Usage: GLRandomTest.LoadUserGpuGeometryProgram(\n"
                "%d,\n"
                "[[fetcher,attr,ImageUnit,MMLevel,Access,elems],\n"
                " [InRegs],[OutRegs],[InTxCd],NeedSbos,BindlessTxAttr],\n"
                "[primIn,primOut,vxPerPatchIn,vxPerPatchOut,vxMultiplier],\n"
                "pgmstring)\n",
                GLRandom::Program::JSIFC_VERSION);
            JS_ReportError(pContext, usage.c_str());
            return JS_FALSE;
        }

        MASSERT(sizeof(UINT32) == sizeof(GLuint));
        // Note on all these casts:
        // GLuint and UINT32 are the same size and typedness, but on the Mac
        // compiler one ends up being a "long" and the other an "int", which is
        // an error.
        rc = pGLRandom->m_GpuPrograms.AddUserGeometryProgram(
                pgmRqmts, pgmString.c_str());
        RETURN_RC(rc);
    }
    RETURN_RC(OK);
}

JS_STEST_LWSTOM(GLRandomTest, LoadUserGpuFragmentProgram, 3,
        "Load a user-defined GpuFragment program.")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    void *     pvGLRandomTest = JS_GetPrivate(pContext, pObject);
    JavaScriptPtr pJavaScript;

    if (pvGLRandomTest)
    {
        GLRandomTest * pGLRandom  = (GLRandomTest *)pvGLRandomTest;
        PgmRequirements pgmRqmts;
        string      pgmString;
        INT32       fmtVersion = 0;
        StickyRC    rc;
        if (OK != pJavaScript->FromJsval(pArguments[0], &fmtVersion))
        {
            rc = RC::BAD_PARAMETER;
        }

        if (fmtVersion == GLRandom::Program::JSIFC_VERSION)
        {
            if((NumArguments != 3) ||
               (OK != ParseJsPgmRqmts(&pgmRqmts,pContext,pArguments[1])) ||
               (OK != pJavaScript->FromJsval(pArguments[2], &pgmString)))
            {
                rc = RC::BAD_PARAMETER;
            }
        }
        else    // invalid version
        {
            ErrorJsifcMismatch(fmtVersion);
            rc = RC::BAD_PARAMETER;
        }

        if (OK != rc)
        {
            string usage = Utility::StrPrintf(
                "Usage: GLRandomTest.LoadUserGpuFragmentProgram(\n"
                "%d,\n"
                "[[fetcher,attr,ImageUnit,MMLevel,Access,elems],\n"
                " [InRegs],[OutRegs],[InTxCd],NeedSbos,BindlessTxAttr],\n"
                "pgmstring)\n",
                GLRandom::Program::JSIFC_VERSION);
            JS_ReportError(pContext, usage.c_str());
            return JS_FALSE;
        }

        MASSERT(sizeof(UINT32) == sizeof(GLuint));
        // Note on all these casts:
        // GLuint and UINT32 are the same size and typedness, but on the Mac
        // compiler one ends up being a "long" and the other an "int", which is
        // an error.
        rc = pGLRandom->m_GpuPrograms.AddUserFragmentProgram(
            pgmRqmts,pgmString.c_str());
        RETURN_RC(rc);
    }
    RETURN_RC(OK);
}

JS_STEST_LWSTOM(GLRandomTest, LoadUserDummyProgram, 1,
        "Load a user-defined Dummy program.")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    void *     pvGLRandomTest = JS_GetPrivate(pContext, pObject);
    JavaScriptPtr pJavaScript;
    StickyRC rc;

    if (pvGLRandomTest)
    {
        GLRandomTest * pGLRandom  = (GLRandomTest *)pvGLRandomTest;
        UINT32 value = 0;
        GLRandom::pgmKind pk = PgmKind_NUM;
        switch (NumArguments)
        {
            default:
                rc = RC::BAD_PARAMETER;
                break;
            case 1: // mandatory argument
                rc = pJavaScript->FromJsval(pArguments[0], &value);//fallthrough
                pk = static_cast<GLRandom::pgmKind>(value);
                break;
        }
        if (rc != OK)
        {
            JS_ReportError(pContext,
                "Usage: GLRandomTest.LoadUserDummyProgram(PgmKind)");
            return JS_FALSE;
        }

        MASSERT(sizeof(UINT32) == sizeof(GLuint));
        // Note on all these casts:
        // GLuint and UINT32 are the same size and typedness, but on the Mac
        // compiler one ends up being a "long" and the other an "int", which is
        // an error.
        rc = pGLRandom->m_GpuPrograms.AddUserDummyProgram(pk);
        RETURN_RC(rc);
    }
    RETURN_RC(OK);
}

JS_STEST_BIND_NO_ARGS(GLRandomTest,
        FreeUserPrograms,
        "Discard all user gpu programs.");

JS_SMETHOD_LWSTOM(GLRandomTest, GetFrameErrCounts, 0,
        "Get frame counts: [ Good, Soft, HardMix, HardAll, Other, TotalTries, TotalFails].")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    void *     pvGLRandomTest = JS_GetPrivate(pContext, pObject);
    JavaScriptPtr pJavaScript;

    if (pvGLRandomTest && NumArguments == 0)
    {
        GLRandomTest * pGLRandomTest  = (GLRandomTest *)pvGLRandomTest;

        UINT32 Good, Soft, HardMix, HardAll, Other, TotalTries, TotalFails;
        pGLRandomTest->GetFrameErrCounts(&Good, &Soft, &HardMix, &HardAll,
            &Other, &TotalTries, &TotalFails);

        JsArray jsa;
        RC rc = pJavaScript->PackJsArray(&jsa, "IIIIIII",
                                        Good,         // 0
                                        Soft,         // 1
                                        HardMix,      // 2
                                        HardAll,      // 3
                                        Other,        // 4
                                        TotalTries,   // 5
                                        TotalFails);  // 6

        if (OK != (rc = pJavaScript->ToJsval(&jsa, pReturlwalue)))
        {
            RETURN_RC(rc);
        }
        return JS_TRUE;
    }
    else
    {
        JS_ReportError(pContext, "Usage: GLRandom.GetFrameErrCounts()");
        return JS_FALSE;
    }
    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(GLRandomTest, GetZlwllInfo, 0,
        "Get Zlwll Info: [ widthAlignPixels, heightAlignPixels, pixelSquaresByAliquos, "
                           "aliquotTotal, zlwllRegionByteMultiplier, zlwllRegionHeaderSize, "
                           "zlwllSubregionHeaderSize, subregionCount, subregionWidthAlignPixels, "
                           "subregionHeightAlignPixels ].")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    void *     pvGLRandomTest = JS_GetPrivate(pContext, pObject);
    JavaScriptPtr pJavaScript;

    if (pvGLRandomTest && NumArguments == 0)
    {
        RC rc;
        GLRandomTest * pGLRandomTest  = (GLRandomTest *)pvGLRandomTest;
        GpuSubdevice * pSubdev = pGLRandomTest->GetBoundGpuSubdevice();
        LW2080_CTRL_GR_GET_ZLWLL_INFO_PARAMS Info = { 0 };
        LwRmPtr pLwRm;
        C_CHECK_RC(pLwRm->ControlBySubdevice(pSubdev, LW2080_CTRL_CMD_GR_GET_ZLWLL_INFO,
                                       &Info, sizeof(Info)));
        JsArray jsa;
        rc = pJavaScript->PackJsArray(&jsa, "IIIIIIIIII"
                                        ,Info.widthAlignPixels           // [0]
                                        ,Info.heightAlignPixels          // [1]
                                        ,Info.pixelSquaresByAliquots     // [2]
                                        ,Info.aliquotTotal               // [3]
                                        ,Info.zlwllRegionByteMultiplier  // [4]
                                        ,Info.zlwllRegionHeaderSize      // [5]
                                        ,Info.zlwllSubregionHeaderSize   // [6]
                                        ,Info.subregionCount             // [7]
                                        ,Info.subregionWidthAlignPixels  // [8]
                                        ,Info.subregionHeightAlignPixels // [9]
                                        );

        if ((rc = pJavaScript->ToJsval(&jsa, pReturlwalue)) != RC::OK)
        {
            RETURN_RC(rc);
        }
        return JS_TRUE;
    }
    else
    {
        JS_ReportError(pContext, "Usage: GLRandom.GetZlwllInfo()");
        return JS_FALSE;
    }
    return JS_TRUE;
}

CLASS_PROP_READONLY (GLRandomTest, Loop,  UINT32, "Current test loop");
CLASS_PROP_READONLY (GLRandomTest, Frame, UINT32, "Current test frame");
CLASS_PROP_READONLY (GLRandomTest, FrameSeed, UINT32, "Seed of current frame");
CLASS_PROP_READONLY (GLRandomTest, IsDrawing, bool, "True at moments when test is drawing");

CLASS_PROP_READWRITE(GLRandomTest, FullScreen, UINT32,
        "Full screen mode (otherwise, window)");
CLASS_PROP_READWRITE(GLRandomTest, DoubleBuffer, bool,
        "Render to back buffer & swap (otherwise, render to front buffer)");
CLASS_PROP_READWRITE(GLRandomTest, PrintOnSwDispatch, bool,
        "If true, print (like SuperVerbose) any draws that hit sw rasterizer (DOS/xbox, lw10+ only).");
CLASS_PROP_READWRITE(GLRandomTest, PrintOlwpipeDispatch, bool,
        "If true, print (like SuperVerbose) any draws that hit vpipe dispatch (DOS/xbox, lw10+ only).");
CLASS_PROP_READWRITE(GLRandomTest, DoTimeQuery, bool,
        "If true, use EXT_timer_query to measure & report render time.");
CLASS_PROP_READWRITE(GLRandomTest, RenderTimePrintThreshold, double,
        "If DoTimeQuery, and loop exceeds threshold in seconds, print.");
CLASS_PROP_READWRITE(GLRandomTest, CompressLights, bool,
        "If true, renumber GL lights to use GL_LIGHT_0 to n-1, with no gaps.");
CLASS_PROP_READWRITE(GLRandomTest, ClearLines, bool,
        "If true, draw lines after each clear, before the random draws.");
CLASS_PROP_READWRITE(GLRandomTest, ContinueOnFrameError, bool,
        "Continue running if an error oclwrred in a frame.");
CLASS_PROP_READWRITE(GLRandomTest, FrameRetries, UINT32,
        "How many times to retry a frame error to decide hard vs. soft.");
CLASS_PROP_READWRITE(GLRandomTest, AllowedSoftErrors, UINT32,
        "How many non-repeatable frame errors to allow.");
CLASS_PROP_READWRITE(GLRandomTest, FrameReplayMs, UINT32,
        "If >0, record frame as display list and play N milliseconds.");
CLASS_PROP_READWRITE(GLRandomTest, TgaTexImageFileName, string,
        "TGA file name for loading as a texture.");
CLASS_PROP_READWRITE(GLRandomTest, MaxFilterableFloatTexDepth, UINT32,
        "Max bit-depth of float texture that may use bi- or tri-linear filtering.");
CLASS_PROP_READWRITE(GLRandomTest, MaxBlendableFloatSize, UINT32,
        "Max size of float surface element that may be blended (0, 16, or 32).");
CLASS_PROP_READWRITE(GLRandomTest, RenderToSysmem, bool,
        "Render offscreen FrameBufferObjects to sysmem.");
CLASS_PROP_READWRITE(GLRandomTest, ForceFbo, bool,
        "Draw offscreen to FrameBufferObject rather than to front buffer.");
CLASS_PROP_READWRITE(GLRandomTest, FboWidth, UINT32,
        "Override width of offscreen FBO surface.");
CLASS_PROP_READWRITE(GLRandomTest, FboHeight, UINT32,
        "Override Height of offscreen FBO surface.");
CLASS_PROP_READWRITE(GLRandomTest, HasF32BorderColor, bool,
        "Gpu supports argbF32 border colors in HW.");
CLASS_PROP_READWRITE(GLRandomTest, UseSbos, bool,
        "Use offscreen ShaderBufferObjects.");
CLASS_PROP_READWRITE(GLRandomTest, PrintSbos, UINT32,
        "Print contents of offscreen ShaderBufferObjects.");
CLASS_PROP_READWRITE(GLRandomTest, SboWidth, UINT32,
        "Override width of offscreen SBO surface.");
CLASS_PROP_READWRITE(GLRandomTest, SboHeight, UINT32,
        "Override Height of offscreen SBO surface.");
CLASS_PROP_READWRITE_LWSTOM(GLRandomTest, ColorSurfaceFormat,
        "if 0, follow TestConfiguration.DisplayDepth, otherwise an array of GL tx-format enums (>1 for MRT).");
CLASS_PROP_READWRITE(GLRandomTest, VerboseGCx, bool,
        "If true show verbose GCx messages (default = false)");
CLASS_PROP_READWRITE(GLRandomTest, DoGCxCycles, UINT32,
        "Perform mixed GCx cycles between sending down vertices.");
CLASS_PROP_READWRITE(GLRandomTest, DoRppgCycles, bool,
        "Perform bubbles to wait for RPPG entry in between sending down veticies.");
CLASS_PROP_READWRITE(GLRandomTest, DoTpcPgCycles, bool,
        "Perform bubbles to wait for TPC-PG entry in between sending down veticies.");
CLASS_PROP_READWRITE(GLRandomTest, DoPgCycles, UINT32,
        "Perform PG idle bubbles like GPC-RG/GR-RPG in between sending down veticies.");
CLASS_PROP_READWRITE(GLRandomTest, DelayAfterBubbleMs, FLOAT64,
        "Delay after finishing Pg bubble");
CLASS_PROP_READWRITE(GLRandomTest, ForceRppgMask, UINT32,
         "Explicitly request which RPPG types to check. 0x1=GR, 0x2=MS, 0x4=DI");
CLASS_PROP_READWRITE(GLRandomTest, ForceGCxPstate, UINT32,
        "Force to Pstate prior to GCx entry.");
CLASS_PROP_READWRITE(GLRandomTest, PciConfigSpaceRestoreDelayMs, UINT32,
        "Delay before accessing PCI config space, in ms (default is 50)");
CLASS_PROP_READWRITE(GLRandomTest, LogShaderCombinations, bool,
        "Log each shader combination into a new *.js file");
CLASS_PROP_READWRITE(GLRandomTest, LogShaderOnFailure, bool,
        "Log shaders on failure when regressing frames");
CLASS_PROP_READWRITE(GLRandomTest, ShaderReplacement, bool,
        "Enable shader replacement with content read from files");
CLASS_PROP_READWRITE(GLRandomTest, DumpOnAssert, bool,
        "Dump render info and shader .js file on assertion failure.");
CLASS_PROP_READWRITE(GLRandomTest, MaxOutputVxPerDraw, UINT32,
        "Max vertexes per draw, after tessellation and instancing");
CLASS_PROP_READWRITE(GLRandomTest, MaxVxMultiplier, UINT32,
        "Max output vx per input vx, after tess, geom, instancing");
CLASS_PROP_READWRITE(GLRandomTest, TraceLevel, UINT32,
        "Override the lwTrace print level");
CLASS_PROP_READWRITE(GLRandomTest, TraceOptions, UINT32,
        "Override lwTrace output options .");
CLASS_PROP_READWRITE(GLRandomTest, TraceMask0, UINT32,
        "Override lwTrace mask bits 0-31.");
CLASS_PROP_READWRITE(GLRandomTest, TraceMask1, UINT32,
        "Override lwTrace mask bits 32-63");
CLASS_PROP_READWRITE(GLRandomTest, NumLayersInFBO, INT32,
        "Number of layers in the layered FBO.");
CLASS_PROP_READWRITE(GLRandomTest, PrintXfbBuffer, UINT32,
        "Print the XFB capture buffer on a specific loop.");
CLASS_PROP_READWRITE(GLRandomTest, InjectErrorAtFrame, INT32,
        "Insert error at this loop");
CLASS_PROP_READWRITE(GLRandomTest, InjectErrorAtSurface, INT32,
        "Insert error at this surface");
CLASS_PROP_READWRITE(GLRandomTest, PrintFeatures, bool,
        "Print OpenGL extensions supported and not supported");
CLASS_PROP_READWRITE(GLRandomTest, VerboseCrcData, bool,
        "Print each time crc entries are updated.");
CLASS_PROP_READWRITE(GLRandomTest, CrcLoopNum, INT32,
        "Loop number to dump raw crc data.");
CLASS_PROP_READWRITE(GLRandomTest, CrcItemNum, INT32,
        "Specific crc item to dump.");
CLASS_PROP_READWRITE(GLRandomTest, LoopsPerProgram, UINT32,
        "Number of loops each set of shaders use.");
CLASS_PROP_READWRITE(GLRandomTest, DisableReverseFrames, bool,
        "Disable running frames in reverse on gputest.js (default=false).");
//TODO remove before check-in
CLASS_PROP_READWRITE(GLRandomTest, UseSanitizeRSQ, bool,
        "Enable RSQ input sanitization for <= 0.0");

static JSBool GLRandomTest_Get_ColorSurfaceFormat
(
  JSContext *    pContext,
  JSObject  *    pObject,
  jsval          PropertyId,
  jsval     *    pValue
)
{
  MASSERT(pValue   != 0);

  GLRandomTest * pTest;
  if((pTest = JS_GET_PRIVATE(GLRandomTest, pContext, pObject, "GLRandomTest")) != 0)
  {
      *pValue = pTest->ColorSurfaceFormatsToJsval();
      return JS_TRUE;
  }
  return JS_FALSE;
}
static JSBool GLRandomTest_Set_ColorSurfaceFormat
(
  JSContext *    pContext,
  JSObject  *    pObject,
  jsval          PropertyId,
  jsval     *    pValue
)
{
  MASSERT(pValue   != 0);

  GLRandomTest * pTest;
  if((pTest = JS_GET_PRIVATE(GLRandomTest, pContext, pObject, "GLRandomTest")) != 0)
  {
      pTest->ColorSurfaceFormatsFromJsval(*pValue);

      return JS_TRUE;
  }
  return JS_FALSE;
}

CLASS_PROP_READWRITE_LWSTOM(GLRandomTest, FrameSeeds,
        "if empty, follow TestConfiguration.Seed, otherwise an array of starting random seeds.");

static JSBool GLRandomTest_Get_FrameSeeds
(
  JSContext *    pContext,
  JSObject  *    pObject,
  jsval          PropertyId,
  jsval     *    pValue
)
{
  MASSERT(pValue   != 0);

  GLRandomTest * pTest;
  if((pTest = JS_GET_PRIVATE(GLRandomTest, pContext, pObject, "GLRandomTest")) != 0)
  {
      JavaScriptPtr pJs;
      vector<UINT32> frameSeeds = pTest->GetFrameSeeds();
      JsArray jsa;
      for (size_t i = 0; i < frameSeeds.size(); ++i)
      {
         jsval tmp;
         pJs->ToJsval(frameSeeds[i], &tmp);
         jsa.push_back(tmp);
      }
      pJs->ToJsval(&jsa, pValue);
      return JS_TRUE;
  }
  return JS_FALSE;
}
static JSBool GLRandomTest_Set_FrameSeeds
(
  JSContext *    pContext,
  JSObject  *    pObject,
  jsval          PropertyId,
  jsval     *    pValue
)
{
  MASSERT(pValue   != 0);

  GLRandomTest * pTest;
  if((pTest = JS_GET_PRIVATE(GLRandomTest, pContext, pObject, "GLRandomTest")) != 0)
  {
      JavaScriptPtr pJs;
      vector<UINT32> frameSeeds;
      JsArray jsa;
      if (OK == pJs->FromJsval(*pValue, &jsa))
      {
          for (size_t i = 0; i < jsa.size(); ++i)
          {
              UINT32 tmp;
              (void) pJs->FromJsval(jsa[i], &tmp);
              frameSeeds.push_back(tmp);
         }
      }
      pTest->SetFrameSeeds(frameSeeds);

      return JS_TRUE;
  }
  return JS_FALSE;
}

JS_STEST_LWSTOM(GLRandomTest, SetScissorOverride, 4,
        "Set a nonrandom Scissor override, used by all loops.")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    void *     pvGLRandomTest = JS_GetPrivate(pContext, pObject);
    JavaScriptPtr pJavaScript;
    StickyRC firstRc;

    if (pvGLRandomTest)
    {
        GLRandomTest * pGLRandom  = (GLRandomTest *)pvGLRandomTest;

        INT32 x = 0;
        INT32 y = 0;
        INT32 w = 0;
        INT32 h = 0;

        if (NumArguments == 4)
        {
            firstRc = pJavaScript->FromJsval(pArguments[0], &x);
            firstRc = pJavaScript->FromJsval(pArguments[1], &y);
            firstRc = pJavaScript->FromJsval(pArguments[2], &w);
            firstRc = pJavaScript->FromJsval(pArguments[3], &h);
        }
        else
        {
            firstRc = RC::BAD_PARAMETER;
        }
        if (OK != firstRc)
        {
            JS_ReportError(pContext, "Usage: GLRandomTest.SetScissorOverride(x,y,w,h)");
            return JS_FALSE;
        }

        pGLRandom->m_Fragment.SetScissorOverride(x, y, w, h);
    }
    RETURN_RC(firstRc);
}

JS_STEST_LWSTOM(GLRandomTest, MarkGoldenRecs, 0,
        "Mark all golden records that Run would touch.")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    void *     pvGLRandomTest = JS_GetPrivate(pContext, pObject);

    RC rc;
    if (pvGLRandomTest)
    {
        GLRandomTest * pGLRandom  = (GLRandomTest *)pvGLRandomTest;
        rc = pGLRandom->MarkGoldenRecs();
    }
    RETURN_RC(rc);
}

P_(GLRandomTest_Get_MiscomparingLoops)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    GLRandomTest * pTest;
    if((pTest = JS_GET_PRIVATE(GLRandomTest, pContext, pObject, NULL)) != 0)
    {
        JavaScriptPtr pJs;
        JsArray jsa;

        const set<UINT32> & badLoops = pTest->GetMiscomparingLoops();
        set<UINT32>::const_iterator i;

        for (i = badLoops.begin(); i != badLoops.end(); ++i)
        {
            jsval jsv;
            pJs->ToJsval(*i, &jsv);
            jsa.push_back(jsv);
        }
        if (OK != pJs->ToJsval(&jsa, pValue))
        {
            JS_ReportError(pContext, "Failed in GetMiscomparingLoops");
            return JS_FALSE;
        }
        return JS_TRUE;
    }
    return JS_FALSE;
}
static SProperty GLRandomTest_MiscomparingLoops
(
    GLRandomTest_Object,
    "MiscomparingLoops",
    0,
    0,
    GLRandomTest_Get_MiscomparingLoops,
    0,
    JSPROP_READONLY,
    "Array of failing loop numbers"
);

P_(GLRandomTest_Get_MiscomparingSeeds)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    GLRandomTest * pTest;
    if((pTest = JS_GET_PRIVATE(GLRandomTest, pContext, pObject, NULL)) != 0)
    {
        JavaScriptPtr pJs;
        JsArray jsa;

        const set<UINT32> & badLoops = pTest->GetMiscomparingLoops();
        set<UINT32>::const_iterator i;

        // Note that GLRandomTest can be configured to check more often than
        // once per frame.  Here we want only the seeds for the beginning of
        // each frame that had a failure.
        set<UINT32> badFrames;
        for (i = badLoops.begin(); i != badLoops.end(); ++i)
        {
            const UINT32 badLoop = *i;
            badFrames.insert(badLoop / pTest->LoopsPerFrame());
        }
        for (i = badFrames.begin(); i != badFrames.end(); ++i)
        {
            const UINT32 startingLoop = pTest->LoopsPerFrame() * (*i);
            jsval jsv;
            pJs->ToJsval(pTest->GetSeed(startingLoop), &jsv);
            jsa.push_back(jsv);
        }
        if (OK != pJs->ToJsval(&jsa, pValue))
        {
            JS_ReportError(pContext, "Failed in GetMiscomparingSeeds");
            return JS_FALSE;
        }
        return JS_TRUE;
    }
    return JS_FALSE;
}
static SProperty GLRandomTest_MiscomparingSeeds
(
    GLRandomTest_Object,
    "MiscomparingSeeds",
    0,
    0,
    GLRandomTest_Get_MiscomparingSeeds,
    0,
    JSPROP_READONLY,
    "Array of random seeds for failing frames."
);

