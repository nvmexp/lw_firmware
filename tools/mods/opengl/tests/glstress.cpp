/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2002-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "glstress.h"
#include "core/include/massert.h"
#include "core/tests/testconf.h"       // TestConfiguration
#include "random.h"
#include "core/include/xp.h"
#include "core/include/gpu.h"
#include "core/include/framebuf.h"
#include "core/include/jscript.h"
#include "core/include/memerror.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "core/include/imagefil.h"
#include "core/include/utility.h"
#include "core/include/version.h"
#include "gpu/utility/surf2d.h"
#include <math.h>
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gpuwm.h"
#include "core/include/golden.h"
#include "gpu/perf/pmusub.h"
#include "gpu/perf/pwrwait.h"
#include "gpu/js/fpk_comm.h"
#include "gpu/utility/chanwmgr.h"
#include "core/include/jsonlog.h"
#include <algorithm>
#include "kepler/gk104/dev_pwr_pri.h"  //Bug 1042805
#include "core/include/cmdline.h"

// Ideas for making this test tougher:
//  - automated tuner vs. LW, Hot, and M clock domains
//  - more simultaneous textures
//  - more texture features
//  - Try float16, see if it works!
// Ideas for new error-aclwmulating methods other than XOR in rop:
//  - blend ADD/SUBTRACT
//    problem is that errors can saturate at 1.0 or 0.0 and be lost
//    but, this is probably ok in FLOAT16 surface?
//

//------------------------------------------------------------------------------
//! Helper class for managing the work/pause times with varying
//! target frequencies, etc.
class PulseController
{
public:
    PulseController
    (
        UINT32 MaxPulseSize,                //!< max quad-strips to draw at once
        double FreqSweepOctavesPerSecond
    );
    ~PulseController();

    void RecordPulseTime(double seconds);
    void SetTargets (UINT32 DutyPct, double Hz);

    UINT32  GetPulseSize() { return UINT32(m_LwrPulseSize + 0.5); }
    double  GetCycleTime() { return m_TargetCycleTime; }
    double  GetMinFreq() const { return m_MinFreq; }
    double  GetMaxFreq() const { return m_MaxFreq; }

private:
    UINT32 m_MaxPulseSize;
    double m_FreqSweepOctavesPerSecond;
    UINT32 m_DutyPct;
    double m_WorkFraction;
    double m_SweepDir;

    double m_LwrFreq;
    double m_TargetPulseTime;
    double m_TargetCycleTime;
    double m_LwrPulseSize;
    double m_MaxFreq;
    double m_MinFreq;

    double m_PulseTimeSum;
    UINT32 m_PulseCount;
    enum { PulseCountLimit = 200 };
};

// Default shader programs.

// In encrypted EUD MODS GLStressTest::GpuProg::Load for simplicity assumes
// that all shader programs are represented by encrypted strings, because they
// were read from JavaScript. Therefore we encrypt the default ones too.
// For the details how encrypted JavaScript functions in the EUD see the
// following wiki: https://wiki.lwpu.com/engwiki/index.php?title=MODS/EUD

const char * GLStressTest::s_DefaultTessCtrlProg = ENCJSENT(
    "!!LWtcp5.0\n"
    "# Default glstress TessCtl: passthru plus set tessellation level.\n"
    "VERTICES_OUT 4;\n"
    "PARAM pgmElw[256] = { program.elw[0..255] };\n"
    "ATTRIB pos   = vertex.position;\n"
    "ATTRIB tc[2] = { vertex.texcoord[0..1] };\n"
    "TEMP i;\n"
    "TEMP t;\n"
    "main:\n"
    " MOV.U i.x, primitive.invocation;\n"
    " MOV result.position, pos[i.x];\n"
    " MOV result.texcoord[0], tc[i.x][0];\n"
    " MOV result.texcoord[1], tc[i.x][1];\n"
    " SUB.CC0 t, i, 0;\n"
    " IF EQ0.x;\n"
    "  MOV result.patch.tessouter[0], pgmElw[22].y;\n" // Y TessRate
    "  MOV result.patch.tessouter[1], pgmElw[22].x;\n" // m_XTessRate
    "  MOV result.patch.tessouter[2], pgmElw[22].y;\n"
    "  MOV result.patch.tessouter[3], pgmElw[22].x;\n"
    "  MOV result.patch.tessinner[0], pgmElw[22].x;\n"
    "  MOV result.patch.tessinner[1], pgmElw[22].y;\n"
    " ENDIF;\n"
    "END\n");

const char * GLStressTest::s_DefaultTessEvalProg = ENCJSENT(
    "!!LWtep5.0\n"
    "# Default glstress TessEval: increase Z towards center.\n"
    "TESS_MODE QUADS;\n"
    "TESS_SPACING EQUAL;\n"
    "TESS_VERTEX_ORDER CCW;\n"
    "ATTRIB pos   = vertex.position;\n"
    "ATTRIB tc[2] = { vertex.texcoord[0..1] };\n"
    "TEMP LL;\n"
    "TEMP UR;\n"
    "TEMP tessc;\n"
    "TEMP a;\n"
    "TEMP b;\n"
    "main:\n"
    // The input patch has 4 coords defining a horizontal rect the width of
    // the primary surface (minus some perspective shrink for the far plane).
    // Here our result vertex is somewhere within that rect.
    // We really only need to look at the lower-left and upper-right vertices.
    " MOV.U LL, 0;\n"
    " MOV.U UR, 2;\n"
    " MOV tessc.xy, vertex.tesscoord;\n"
    // Our clip-coord x,y range from -100 to 100.
    // Z will be -100 at edges for near plane, -80 for far plane.
    // W will be  100 at edges for near plane, 110 for far plane.
    // For interior vertices, we want z,w to increase by 100 at x,y=0,0.
    " LRP a, tessc.xyxx, pos[LL.x], pos[UR.x];\n"
    " MAX b.x, |a.x|, |a.y|;\n"
    " ADD b.x, 100, -b.x;\n"
    " ADD a.z, a.z, b.x;\n"
    " MAD a.w, b.x, 0.5, a.w;\n"
    " MOV result.position, a;\n"
    " LRP result.texcoord[0].xy, tessc, tc[LL.x][0], tc[UR.x][0];\n"
    " LRP result.texcoord[1].xy, tessc, tc[LL.x][1], tc[UR.x][1];\n"
    " MOV result.fogcoord.x, a.w;\n" // just a guess
    // Our normals will be 0,0,1 (to eye) at center.
    // At edge-centers, will have x or y of -+0.5 (45 deg from eye).
    " MUL b.xy, a, 0.005;\n"
    " MOV b.zw, 1;\n"
    " NRM b.xyz, b;\n"
    " MOV result.attrib[1].xyz, b;\n"
    " MOV result.color.primary, b.yzxw;\n"
    " MOV result.color.secondary, b.zyxw;\n"
    "END\n");

//------------------------------------------------------------------------------
// Various GL interfaces expect us to pass in a void* to some application data.
//
// For index and vertex data stored in a GL buffer object, we instead pass in
// an offset from the beginning of the buffer, per the
// GL_ARB_vertex_buffer_object extension spec.
//
// The actual address of the buffer is off in FB somewhere and is not normally
// mapped to the CPU's virtual address space.
//
// Since the GL interfaces are declard to take a pointer type, we colwert our
// offsets to bogus pointers by adding them to NullPtr below.
// This is prettier and more "correct" than casting.
//------------------------------------------------------------------------------
static char * NullPtr = 0;

//------------------------------------------------------------------------------
GLStressTest::GLStressTest()
{
    SetName("GLStressTest");
    m_LoopMs = 4000;
    m_BufferCheckMs = ~0;
    m_RuntimeMs = 0;
    m_FullScreen = GL_TRUE;
    m_DoubleBuffer = false;
    m_PatternClass.SelectPatternSet(PatternClass::PATTERNSET_ALL);
    m_KeepRunning = false;
    m_Width = 0;
    m_Height = 0;
    m_Depth = 0;
    m_ColorSurfaceFormat = 0;
    m_ZDepth = 0;
    m_TxSize = 16;
    m_NumTextures = m_PatternClass.GetLwrrentNumOfSelectedPatterns();
    m_TxD = 0.0;
    m_PpV = 100e3;
    m_Rotate = false;
    m_UseMM = true;
    m_TxGenQ = false;
    m_Use3dTextures = false;
    m_NumLights = 3;
    m_SpotLights = true;
    m_Normals = false;
    m_BigVertex = false;
    m_Stencil = true;
    m_Ztest = true;
    m_TwoTex = true;
    m_Fog = true;
    m_LinearFog = true;
    m_BoringXform = false;
    m_DrawLines = false;
    m_DumpPngOnError = false;
    m_SendW = false;
    m_DoGpuCompare = false;
    m_AlphaTest = true;
    m_OffsetSecondTexture = false;
    m_ViewportXOffset = 0;
    m_ViewportYOffset = 0;
    m_TxImages.clear();
    m_ClearColor = 0x00000000;
    m_GpioToDriveInDelay = -1;
    m_StartFreq = -1.0;
    m_DutyPct = 50;
    m_FreqSweepOctavesPerSecond = 2.0;
    m_BeQuiet = false;
    m_ForceErrorCount = 0;
    m_ContinueOnError = false;
    m_EnablePowerWait = false;
    m_TegraPowerWait = false;
    m_EnableDeepIdle = false;
    m_ToggleELPGOlwideo = false;
    m_ToggleELPGGatedTimeMs = 100;
    m_PowerWaitTimeoutMs = 5000;
    m_DeepIdleTimeoutMs = 5000;
    m_ShowDeepIdleStats = false;
    m_PrintPowerEvents = false;
    m_EndLoopPeriodMs = 10000;
    m_FramesPerFlush = 100;
    m_UseFbo = false;
    m_UseTessellation = false;
    m_DumpFrames = 0;
    m_Blend = false;
    m_Torus = false;
    m_TorusMajor = true;
    m_LwllBack = false;
    m_TorusHoleDiameter = 2.0;
    m_TorusDiameter = 10.0;
    m_DrawPct = 100.0;
    m_Watts = 0;
    m_AllowedMiscompares = 0;
    m_SmoothLines = true;
    m_StippleLines = true;
    m_PulseMode = PULSE_DISABLED;
    m_TileMask = 1;
    m_NumActiveGPCs = 0;
    m_SkipDrawChecks = false;

    m_ActualColorSurfaceFormat = 0;
    m_TicksPerSec = (double)(INT64) Xp::QueryPerformanceFrequency();

    m_GpuProgs[Vtx] = GpuProg("Vtx", "GL_LW_vertex_program",
            GL_VERTEX_PROGRAM_LW, "");

    m_GpuProgs[TessCtrl] = GpuProg("TessCtrl", "GL_LW_gpu_program5",
            GL_TESS_CONTROL_PROGRAM_LW, s_DefaultTessCtrlProg);

    m_GpuProgs[TessEval] = GpuProg("TessEval", "GL_LW_gpu_program5",
            GL_TESS_EVALUATION_PROGRAM_LW, s_DefaultTessEvalProg);

    m_GpuProgs[Frag] = GpuProg("Frag", "GL_LW_fragment_program",
            GL_FRAGMENT_PROGRAM_LW, "");

    m_Rows = 0;
    m_Cols = 0;
    m_XTessRate = 0;
    m_CenterLeftCol = 0;
    m_CenterRightCol = 0;
    m_CenterBottomRow = 0;
    m_CenterTopRow = 0;
    m_VxPerPlane = 0;
    m_NumVertexes = 0;
    m_IdxPerPlane = 0;
    m_NumIndexes = 0;
    m_TextureObjectIds = NULL;
    memset(m_FboIds, 0, sizeof(m_FboIds));
    memset(m_FboTexIds, 0, sizeof(m_FboTexIds));
    m_GpuCompareFragProgId = 0;
    m_GpuCopyFragProgId = 0;
    m_GpuCompareOcclusionQueryId = 0;
    m_WindowRect[0] = m_WindowRect[1] =
        m_WindowRect[2] = m_WindowRect[3] = 0;
    m_ScissorRect[0] = m_ScissorRect[1] =
        m_ScissorRect[2] = m_ScissorRect[3] = 0;
    m_Timers = NULL;
    memset(m_Vbos, 0, sizeof(m_Vbos));
    memset(m_ConstIds, 0, sizeof(m_ConstIds));
    m_FrameCount = 0;
    m_StripCount = 0;
    m_Plane = 0;
    m_PowerWaitTrigger = 0;
    m_PowerWaitDelayMs = 0;
    m_FramesPerCheck = 0;
    m_HighestGPCIdx = 0;
    m_ElapsedGpu = 0.0;
    m_ElapsedRun = 0.0;
    m_ElapsedGpuIdle = 0.0;
    m_RememberedFps = -1.0;
    m_RememberedPps = -1.0;
    m_RememberedFrames = 0.0;
    m_RememberedPixels = 0.0;
    m_PrintedHelpfulMessage = false;
    m_MemLocsAreMisleading = true;
    m_ReportMemLocs = false;
    m_CantReportMemLocsReason = nullptr;
    m_pRawPrimarySurface = NULL;
    m_Xbloat = 1;
    m_Ybloat = 1;
    m_pGolden = GetGoldelwalues();
    m_pElpgToggle = NULL;

    m_pFpCtx = GetFpContext();
    CreateFancyPickerArray(FPK_GLSTRESSTEST_NUM_PICKERS);
    m_pPickers = GetFancyPickerArray();
}

//------------------------------------------------------------------------------
/* virtual */
GLStressTest::~GLStressTest()
{
    Free();
}

//------------------------------------------------------------------------------
/* virtual */
bool GLStressTest::IsSupported()
{
    GpuSubdevice * pGpuSub = GetBoundGpuSubdevice();

    if (ChannelWrapperMgr::Instance()->UsesRunlistWrapper())
    {
        return false;
    }

    if (!mglHwIsSupported(pGpuSub))
    {
        // This part is so old, the OpenGL driver has dropped support.
        return false;
    }

    if (m_EnableDeepIdle && m_ToggleELPGOlwideo)
    {
        Printf(Tee::PriNormal,
               "Deep Idle and background video toggling are incompatible\n");
        return false;
    }

    // Use sysfs with GPU driver in the kernel
    if (m_EnablePowerWait && GetBoundGpuSubdevice()->IsSOC() && !Platform::HasClientSideResman())
    {
        m_TegraPowerWait = true;
        return true;
    }

    if (m_EnablePowerWait || m_ToggleELPGOlwideo || m_EnableDeepIdle)
    {
        // Power waiting required, but this part does not have ELPG.
        if (!GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_HAS_ELPG))
            return false;

        PMU *pPmu;
        if (OK != GetBoundGpuSubdevice()->GetPmu(&pPmu))
        {
            Printf(Tee::PriNormal, "Warning : Error getting PMU\n");
            return false;
        }

        if (pPmu->IsElpgOwned())
        {
            Printf(Tee::PriNormal, "ELPG is lwrrently in use\n");
            return false;
        }

        if (m_EnableDeepIdle)
        {
            Perf* const pPerf = GetBoundGpuSubdevice()->GetPerf();
            if (!pPerf)
            {
                Printf(Tee::PriNormal, "Perf object not found!\n");
                return false;
            }

            if (pPerf->IsOwned())
            {
                return false;
            }
        }

        vector<LW2080_CTRL_POWERGATING_PARAMETER> pgParams(2);
        pgParams[0].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED;
        pgParams[0].parameterExtended = PMU::ELPG_GRAPHICS_ENGINE;
        pgParams[1].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED;
        pgParams[1].parameterExtended = PMU::ELPG_VIDEO_ENGINE;
        if (OK != pPmu->GetPowerGatingParameters(&pgParams))
        {
            Printf(Tee::PriNormal,
                   "Warning : Error querying power gating parameters\n");
            return false;
        }

        // Deep idle doesn't care whether ELPG is actually enabled or not
        if (m_EnablePowerWait && !m_EnableDeepIdle && !pgParams[0].parameterValue)
        {
            Printf(Tee::PriLow,
                   "Warning: ELPG on graphics is supported but not enabled\n");
            return false;
        }

        if (m_ToggleELPGOlwideo && !pgParams[1].parameterValue)
        {
            Printf(Tee::PriLow,
                   "Warning: ELPG on video is supported but not enabled\n");
            return false;
        }

        if (m_EnableDeepIdle && !pPmu->IsDeepIdleSupported())
        {
            return false;
        }
    }

    return true;
}
//------------------------------------------------------------------------------
// Print other non-JS test properties that influence the test behavior.
void GLStressTest::PrintNonJsProperties()
{
    if (!m_BeQuiet)
    {
        const char * ftStrings[2] = { "false", "true" };
        VerbosePrintf("\t%-32s: %s\n", "UsingFbo",      ftStrings[m_mglTestContext.UsingFbo()]);
        VerbosePrintf("\t%-32s: %d\n", "Cols",          m_Cols);
        VerbosePrintf("\t%-32s: %d\n", "Rows",          m_Rows);
    }
}

//------------------------------------------------------------------------------
void GLStressTest::PrintJsProperties(Tee::Priority pri)
{
    if (!m_BeQuiet)
    {
        GpuMemTest::PrintJsProperties(pri);

        const char * ftStrings[2] = { "false", "true" };

        Printf(pri, "%s controls:\n", GetName().c_str());
        Printf(pri, "\t%-32s: %u\n", "AllowedMiscompares", m_AllowedMiscompares);
        Printf(pri, "\t%-32s: %d\n", "LoopMs",     m_LoopMs);
        Printf(pri, "\t%-32s: %d\n", "BufferCheckMs", m_BufferCheckMs);
        Printf(pri, "\t%-32s: %d\n", "RuntimeMs",  m_RuntimeMs);
        Printf(pri, "\t%-32s: 0x%x\n", "ColorSurfaceFormat",  m_ColorSurfaceFormat);
        Printf(pri, "\t%-32s: %s\n", "FullScreen",   ftStrings[ m_FullScreen ]);
        Printf(pri, "\t%-32s: %s\n", "DoubleBuffer", ftStrings[ m_DoubleBuffer ]);
        Printf(pri, "\t%-32s: %d\n", "TxSize",     m_TxSize);
        Printf(pri, "\t%-32s: %d\n", "NumTextures",  m_NumTextures);
        Printf(pri, "\t%-32s: %f (%f texels per pixel)\n", "TxD", m_TxD, pow(2.0, m_TxD));
        Printf(pri, "\t%-32s: %f\n", "PpV", m_PpV);
        Printf(pri, "\t%-32s: %s\n", "Rotate",     ftStrings[ m_Rotate ]);
        Printf(pri, "\t%-32s: %s\n", "UseMM",      ftStrings[ m_UseMM ]);
        Printf(pri, "\t%-32s: %s\n", "TxGenQ",     ftStrings[ m_TxGenQ ]);
        Printf(pri, "\t%-32s: %s\n", "Use3dTextures", ftStrings[ m_Use3dTextures ]);
        Printf(pri, "\t%-32s: %d\n", "NumLights",    m_NumLights);
        Printf(pri, "\t%-32s: %s\n", "SpotLights",   ftStrings[ m_SpotLights ]);
        Printf(pri, "\t%-32s: %s\n", "Normals",      ftStrings[ m_Normals ]);
        Printf(pri, "\t%-32s: %s\n", "BigVertex",    ftStrings[ m_BigVertex ]);
        Printf(pri, "\t%-32s: %s\n", "Stencil",      ftStrings[ m_Stencil ]);
        Printf(pri, "\t%-32s: %s\n", "Ztest",      ftStrings[ m_Ztest ]);
        Printf(pri, "\t%-32s: %s\n", "TwoTex",     ftStrings[ m_TwoTex ]);
        Printf(pri, "\t%-32s: %s\n", "Fog",        ftStrings[ m_Fog ]);
        Printf(pri, "\t%-32s: %s\n", "LinearFog",    ftStrings[ m_LinearFog ]);
        Printf(pri, "\t%-32s: %s\n", "BoringXform",  ftStrings[ m_BoringXform ]);
        Printf(pri, "\t%-32s: %s\n", "DrawLines",    ftStrings[ m_DrawLines ]);
        Printf(pri, "\t%-32s: %s\n", "DumpPngOnError", ftStrings[ m_DumpPngOnError ]);
        Printf(pri, "\t%-32s: %s\n", "SendW",        ftStrings[ m_SendW ]);
        Printf(pri, "\t%-32s: %s\n", "DoGpuCompare", ftStrings[ m_DoGpuCompare ]);
        Printf(pri, "\t%-32s: %s\n", "AlphaTest",    ftStrings[ m_AlphaTest ]);
        Printf(pri, "\t%-32s: %s\n", "OffsetSecondTexture",   ftStrings[ m_OffsetSecondTexture ]);
        Printf(pri, "\t%-32s: %d\n", "ViewportXOffset", m_ViewportXOffset);
        Printf(pri, "\t%-32s: %d\n", "ViewportYOffset", m_ViewportYOffset);
        Printf(pri, "\t%-32s: %08x (LE_A8R8G8B8)\n", "ClearColor",   m_ClearColor);
        Printf(pri, "\t%-32s: %d\n", "GpioToDriveInDelay",       m_GpioToDriveInDelay);
        Printf(pri, "\t%-32s: %.1f Hz\n", "StartFreq", m_StartFreq);
        Printf(pri, "\t%-32s: %d\n", "DutyPct", m_DutyPct);
        Printf(pri, "\t%-32s: %.1f\n", "FreqSweepOctavesPerSecond", m_FreqSweepOctavesPerSecond);
        Printf(pri, "\t%-32s: %d\n", "ForceErrorCount",   m_ForceErrorCount);
        Printf(pri, "\t%-32s: %s\n", "UseTessellation",   ftStrings[ m_UseTessellation ]);
        Printf(pri, "\t%-32s: %s\n", "Blend",         ftStrings[ m_Blend ]);
        Printf(pri, "\t%-32s: %s\n", "Torus",         ftStrings[ m_Torus ]);
        Printf(pri, "\t%-32s: %s\n", "TorusMajor",    ftStrings[ m_TorusMajor ]);
        Printf(pri, "\t%-32s: %s\n", "LwllBack",      ftStrings[ m_LwllBack ]);
        Printf(pri, "\t%-32s: %.1f\n", "DrawPct",     m_DrawPct);
        Printf(pri, "\t%-32s: %s\n", "SmoothLines",    ftStrings[ m_SmoothLines ]);
        Printf(pri, "\t%-32s: %s\n", "StippleLines",    ftStrings[ m_StippleLines ]);
        Printf(pri, "\t%-32s: %d\n", "PulseMode",     m_PulseMode);
        Printf(pri, "\t%-32s: %08x\n", "TileMask",    m_TileMask);

        UINT32 i;
        for (i = 0; i < m_TxImages.size(); i++)
            Printf(pri, "\t%-32s[%d]: \"%s\"\n", "TxImages",  i, m_TxImages[i].c_str());
    }
}

//------------------------------------------------------------------------------
/* virtual */
RC GLStressTest::InitFromJs()
{
    RC rc;

    // Call base-class function (for TestConfiguration and Golden properties).
    CHECK_RC( GpuMemTest::InitFromJs() );

    m_FramesPerCheck = GetGoldelwalues()->GetSkipCount();
    m_PatternClass.FillRandomPattern(GetTestConfiguration()->Seed());

    m_Width   = GetTestConfiguration()->SurfaceWidth();
    m_Height  = GetTestConfiguration()->SurfaceHeight();
    m_Depth   = GetTestConfiguration()->DisplayDepth();
    m_ZDepth  = GetTestConfiguration()->ZDepth();

    if (m_Stencil && (m_ZDepth != 32))
    {
        Printf(Tee::PriNormal, "Disabling stencil with non-32-bit Z depth.\n");
        m_Stencil = false;
    }
    if ((m_Depth < 32) && (m_Depth != 16))
    {
        Printf(Tee::PriNormal, "Depth %d not supported, forcing to 16.\n", m_Depth);
        m_Depth = 16;
    }
    if (!m_NumTextures)
    {
        m_TxSize = 0;
        m_Use3dTextures = false;
        m_UseMM = false;
        m_TxGenQ = false;
        m_TwoTex = false;
    }

    if (m_BigVertex)
    {
        m_SendW = false;
    }
    if (m_Torus)
    {
        m_UseTessellation = false;
    }

    // Using a non-default color format may force offscreen rendering.
    CHECK_RC( m_mglTestContext.SetProperties(
            GetTestConfiguration(),
            m_DoubleBuffer,
            GetBoundGpuDevice(),
            GetDispMgrReqs(),
            m_ColorSurfaceFormat, // possibly override color format
            m_UseFbo,   //useFbo
            false,      //don't render to sysmem
            0));        //do not use layered FBO

    if (m_ColorSurfaceFormat != 0)
    {
        Printf(Tee::PriLow, "Detected ColorSurfaceFormat override!\n");
        if ((m_ColorSurfaceFormat >= GL_RGBA32I_EXT) &&
            (m_ColorSurfaceFormat <= GL_LUMINANCE_ALPHA_INTEGER_EXT) &&
            (!m_GpuProgs[Frag].IsSet()) )
        {
            Printf(Tee::PriError,
                   "ColorSurfaceFormat is INTEGER_INTERNAL_FORMAT and can't run with fixed function pipeline\n");
            Printf(Tee::PriError, "User must supply a fragment program!\n");
            rc = RC::ILWALID_ARGUMENT;
        }
        else
            Printf(Tee::PriLow, "Forcing offscreen rendering mode\n");
    }

    m_ActualColorSurfaceFormat = m_mglTestContext.ColorSurfaceFormat();

    if (m_NumActiveGPCs)
    {
        if (m_PulseMode == PULSE_DISABLED)
        {
            // For an easier UI, non-zero NumActiveGPCs should activate the mode:
            m_PulseMode = PULSE_TILE_MASK_GPC_SHUFFLE;
        }
        else if (m_PulseMode != PULSE_TILE_MASK_GPC_SHUFFLE)
        {
            Printf(Tee::PriError, "PulseMode can't be set to %d when NumActiveGPCs=%d\n",
                m_PulseMode, m_NumActiveGPCs);
            return RC::ILWALID_ARGUMENT;
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */
RC GLStressTest::Setup()
{
    RC rc;

    CHECK_RC(GpuMemTest::Setup());


    // Take over the screen, initialize GL library.
    CHECK_RC(m_mglTestContext.Setup());

    if ((m_EnablePowerWait && ! m_TegraPowerWait)
        || m_ToggleELPGOlwideo || m_EnableDeepIdle)
    {
        PMU *pPmu;
        CHECK_RC(GetBoundGpuSubdevice()->GetPmu(&pPmu));
        CHECK_RC(m_ElpgOwner.ClaimElpg(pPmu));
        if (m_EnableDeepIdle)
            CHECK_RC(m_PStateOwner.ClaimPStates(GetBoundGpuSubdevice()));
    }

    m_Xbloat = 1;
    m_Ybloat = 1;
    m_MemLocsAreMisleading = false;
    m_ReportMemLocs = false;
    m_CantReportMemLocsReason = "is at an undisclosed location";
    m_pRawPrimarySurface = NULL;

    if (! mglExtensionSupported("GL_EXT_framebuffer_object"))
        m_DoGpuCompare = false;

    m_pRawPrimarySurface = m_mglTestContext.GetPrimarySurface();

    if (m_pRawPrimarySurface)
    {
        m_pRawPrimarySurface->Print(Tee::PriLow, "C surface ");

        if (m_pRawPrimarySurface->GetLocation() != Memory::Fb
            && ! GetBoundGpuSubdevice()->IsSOC())
        {
            m_CantReportMemLocsReason = "is in sysmem";

            // In http://lwbugs/606198 we find that glFinish returns before
            // ROP has written all pixels to the sysmem output surface.
            //
            // Force the glReadPixels surface-checking path.
            m_pRawPrimarySurface = NULL;
        }
    }

    // Some platforms (DOS, sim) allow raw access to the front color buffer.
    if (!m_DoGpuCompare && !m_DoubleBuffer && nullptr != m_pRawPrimarySurface)
    {
        if (m_pRawPrimarySurface->GetLayout() == Surface2D::Pitch &&
            m_pRawPrimarySurface->GetTiled())
        {
            m_CantReportMemLocsReason = "is tiled";
        }
        else if (m_pRawPrimarySurface->GetCompressed())
        {
            m_CantReportMemLocsReason = "is compressed";
        }
        else
        {
            // Do MemError FB decoding, we have an untiled pitch primary surface.
            m_ReportMemLocs = true;
        }
    }
    if (m_pRawPrimarySurface)
    {
        // We will attempt to report miscomparing pixels as memory errors at
        // particular known addresses.

        switch (GetTestConfiguration()->GetFSAAMode())
        {
            case FSAA2xDac:
            case FSAA2xQuinlwnxDac:
                m_Xbloat = 2;
                break;
            case FSAA4xDac:
                m_Xbloat = 2;
                m_Ybloat = 2;
                break;
            default:
                break;
        }
        if (m_NumTextures || m_Stencil || m_Ztest)
        {
            // We are reading & writing more than just the C buffer, so
            // rendering errors could be caused by memory problems at addresses
            // other than the one that miscompared.  Caveat emptor.
            m_MemLocsAreMisleading = true;
        }
    }

    if (! mglExtensionSupported("GL_LW_gpu_program5"))
        m_UseTessellation = false;

    if (m_SkipDrawChecks)
    {
        Printf(Tee::PriWarn, "%s: SkipDrawChecks is active!\n", GetName().c_str());
    }

    // Initialize the OpenGL stuff.
    CHECK_RC( Allocate() );

    // One time initialization of the WindowRect.
    glGetIntegerv(GL_SCISSOR_BOX, m_WindowRect);
    PrintNonJsProperties();
    CHECK_RC( SetupDrawState() );

    if (m_EnablePowerWait || m_EnableDeepIdle)
    {
        /* Enabling Deep L1 interrupt in the following code. This is a workaround.
           We can delete the code after RM updates her 'bifenabledeepL1' function
           **Note: This may break in future.
         */
        GpuSubdevice * pSubdev = GetBoundGpuSubdevice();
        if (pSubdev->HasBug(1042805))
        {
            UINT32 RegVal = pSubdev->RegRd32(LW_PPWR_PMU_GPIO_INTR_RISING_EN);
            RegVal = FLD_SET_DRF(_PPWR_PMU, _GPIO_INTR_RISING_EN,
                                 _XVXP_DEEP_IDLE, _ENABLED, RegVal);
            pSubdev->RegWr32(LW_PPWR_PMU_GPIO_INTR_RISING_EN, RegVal);
        }

        // Get the pstates needed for DI. Override them if applicable.
        //CHECK_RC(SetupDeepIdlePStatesPicker());

        if (((*m_pPickers)[FPK_GLSTRESSTEST_POWER_WAIT].Mode() == FancyPicker::RANDOM) ||
            ((*m_pPickers)[FPK_GLSTRESSTEST_DELAYMS_AFTER_PG].Mode() == FancyPicker::RANDOM) ||
            ((*m_pPickers)[FPK_GLSTRESSTEST_DEEPIDLE_PSTATE].Mode() == FancyPicker::RANDOM))

        {
            m_pFpCtx->Rand.SeedRandom(GetTestConfiguration()->Seed());
        }

        m_pFpCtx->LoopNum = 0;
        m_PowerWaitTrigger = (*m_pPickers)[FPK_GLSTRESSTEST_POWER_WAIT].Pick() * 4;
        m_PowerWaitDelayMs = (*m_pPickers)[FPK_GLSTRESSTEST_DELAYMS_AFTER_PG].Pick();
        m_pFpCtx->LoopNum++;
    }

    if (m_ToggleELPGOlwideo)
    {
        m_pElpgToggle = new ElpgBackgroundToggle(GetBoundGpuSubdevice(),
                                                 PMU::ELPG_VIDEO_ENGINE,
                                                 m_PowerWaitTimeoutMs,
                                                 m_ToggleELPGGatedTimeMs);
        if (m_pElpgToggle == NULL)
        {
            return RC::CANNOT_ALLOCATE_MEMORY;
        }
        CHECK_RC(m_pElpgToggle->Start());
    }

    return rc;
}

//------------------------------------------------------------------------------
void GLStressTest::UpdateTestProgress() const
{
    if (m_RuntimeMs)
    {
        PrintProgressUpdate(min(static_cast<UINT32>(m_ElapsedRun * 1000.0), m_RuntimeMs));
    }
    else
    {
        PrintProgressUpdate(min(static_cast<UINT32>(m_ElapsedGpu * 1000.0), m_LoopMs));
    }
}

//------------------------------------------------------------------------------
/* virtual */
RC GLStressTest::Run()
{
    #define CHECK_BOTH_BUFFERS true
    #define CHECK_ONE_BUFFER false
    StickyRC rc;
    GpuDevice * pGpuDev = GetBoundGpuDevice();
    GpuSubdevice * pGpuSub = GetBoundGpuSubdevice();
    UINT64 startPGCount = 0;
    PMU   *pPmu = NULL;
    vector<LW2080_CTRL_POWERGATING_PARAMETER> pgParams(1);
    UINT32 nextEndLoopMs = m_EndLoopPeriodMs;
    double bufferCheckRuntime = m_BufferCheckMs / 1000.0;

    if (m_EnablePowerWait && m_TegraPowerWait)
    {
        startPGCount = TegraElpgWait::ReadTransitions();
    }
    else if (m_EnablePowerWait || m_EnableDeepIdle)
    {
        CHECK_RC(pGpuSub->GetPmu(&pPmu));
        MASSERT(pPmu != nullptr);

        if (m_PrintPowerEvents)
        {
            pgParams[0].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_GATINGCOUNT;
            pgParams[0].parameterExtended = PMU::ELPG_GRAPHICS_ENGINE;

            // Get the starting power gate count for the test
            CHECK_RC(pPmu->GetPowerGatingParameters(&pgParams));
            startPGCount = pgParams[0].parameterValue;
        }

        // Reset deep idle statistics when starting a deep idle test
        if (m_EnableDeepIdle)
        {
            CHECK_RC(pPmu->ResetDeepIdleStatistics(PMU::DEEP_IDLE_NO_HEADS));
        }
    }

    // Delete saved error/benchmark data from previous run, if any.
    for (UINT32 sd = 0; sd < pGpuDev->GetNumSubdevices(); sd++)
    {
        if (m_BeQuiet)
            GetMemError(sd).SuppressHelpMessage();
    }

    if (m_PulseMode == PULSE_TILE_MASK_GPC_SHUFFLE)
    {
        m_GPCShuffleStats.Reset();
    }
    m_RememberedFps = 0.0;
    m_RememberedPps = 0.0;
    m_RememberedPixels = 0.0;
    m_ElapsedRun = 0.0;
    m_ElapsedGpuIdle = 0.0;
    m_ElapsedGpu = 0.0;
    m_FrameCount = 0;
    m_StripCount = 0;

    glFinish();
    CallbackArguments args;
    CHECK_RC(FireCallbacks(ModsTest::MISC_A, Callbacks::STOP_ON_ERROR,
                           args, "Pre-Clear"));

    // Clear the screen.
    SetupScissor(100.0);
    CHECK_RC( Clear() );
    if (m_DoubleBuffer)
    {
        m_mglTestContext.SwapBuffers();
        CHECK_RC( Clear() );
    }

    const UINT64 minFrames = GetTestConfiguration()->Loops();
    INT64 start = 0;
    INT64 end = 0;

    const bool recordProgress = !m_KeepRunning;
    if (recordProgress)
    {
        if (m_RuntimeMs)
        {
            PrintProgressInit(m_RuntimeMs);
        }
        else
        {
            PrintProgressInit(m_LoopMs);
        }
    }

    if (m_StartFreq > 0.0)
    {
        // Draw in little bursts of quad-strips, with delays in between, to
        // produce a pulsing workload.
        PulseController ctrl(
                4 * m_Rows,  // max pulse -- 4 full frames
                m_FreqSweepOctavesPerSecond);

        const bool sweeping = m_FreqSweepOctavesPerSecond > 0.0;
        if (sweeping)
            ctrl.SetTargets(m_DutyPct, m_StartFreq);

        while (m_KeepRunning || (m_RuntimeMs ?
               (m_ElapsedRun * 1000.0 < m_RuntimeMs) :
               (m_FrameCount < minFrames || m_ElapsedGpu * 1000.0 < m_LoopMs)))
        {
            if (!sweeping)
                ctrl.SetTargets(m_DutyPct, m_StartFreq);

            // Callwlate when this "pulse" should finish: now + 1 cycle.
            double cycleTime  = ctrl.GetCycleTime();
            start = (INT64) Xp::QueryPerformanceCounter();
            end   = start + INT64(cycleTime * m_TicksPerSec);

            // Draw next pulse of work.

            if (m_GpioToDriveInDelay >= 0)
                pGpuSub->GpioSetOutput(m_GpioToDriveInDelay, false);

            if (OK != (rc = Draw(ctrl.GetPulseSize(), NULL)))
                break;

            glFinish();

            if (m_GpioToDriveInDelay >= 0)
                pGpuSub->GpioSetOutput(m_GpioToDriveInDelay, true);

            const INT64 finish = (INT64) Xp::QueryPerformanceCounter();
            ctrl.RecordPulseTime((finish - start) / m_TicksPerSec);

            // Pause before sending the next draw.
            do
            {
                Tasker::Yield();
            }
            while (end > (INT64) Xp::QueryPerformanceCounter());

            m_ElapsedGpu += cycleTime;

            m_ElapsedRun +=
                (Xp::QueryPerformanceCounter() - start)/m_TicksPerSec;

            if (OK != (rc = DoEndLoop(&nextEndLoopMs)))
                break;

            // Set progress update
            if (recordProgress)
                UpdateTestProgress();
        }
        if (m_GpioToDriveInDelay >= 0)
            pGpuSub->GpioSetOutput(m_GpioToDriveInDelay, false);

        if (m_FreqSweepOctavesPerSecond)
        {
            Printf(Tee::PriLow, "GLStress freqs %.1f to %.1f khz\n",
                ctrl.GetMinFreq() / 1000.0,
                ctrl.GetMaxFreq() / 1000.0);
        }
    }
    else
    {
        // Draw as fast as possible, with no pauses in the workload.

        bool runPowerThread =
            !(m_EnablePowerWait || m_ToggleELPGOlwideo || m_EnableDeepIdle);
        if (runPowerThread)
        {
            runPowerThread = StartPowerThread() == OK;
        }
        DEFER
        {
            if (runPowerThread)
            {
                this->StopPowerThread();
            }
        };

        if (!m_FramesPerFlush)
            m_FramesPerFlush = 1;

        UINT32 timerIdx = 0;

        while (1)
        {
            start = Xp::QueryPerformanceCounter();
            const bool timeDone = (!m_KeepRunning &&
                                    (m_RuntimeMs ?
                                    (m_ElapsedRun * 1000.0 >= m_RuntimeMs):
                                    (m_ElapsedGpu * 1000.0 >= m_LoopMs)));

            const bool loopsDone = (m_FrameCount >= minFrames);

            if (timeDone && loopsDone)
                break;

            UINT32 framesToDraw = m_FramesPerFlush;

            if (timeDone)
            {
                // The test will exit when minFrames are complete.
                // Don't draw further than necessary.
                // Avoids timeout on fmodel where minFrames is 1.
                UINT32 fLeft = static_cast<UINT32>(minFrames - m_FrameCount);
                framesToDraw = min(framesToDraw, fLeft);
            }

            // GetSeconds returns 0 for a timer that hasn't been used before,
            // i.e. this is a no-op in the first two loops.
            //
            // After that, it waits for the two semaphore-writes previously queued
            // up by the Begin and End methods in previous loops to be written and
            // reports the timestamp differences.
            //
            // So in addition to tracking total render-time, this keeps us from
            // getting too far ahead of the GPU (always 4 to 8 frames ahead).

            m_ElapsedGpu += m_Timers[timerIdx].GetSeconds();
            if (OK != (rc = Draw(framesToDraw * m_Rows, &m_Timers[timerIdx])))
                break;

            timerIdx = timerIdx ^ 1;

            glFlush();

            m_ElapsedRun += (Xp::QueryPerformanceCounter()-start)/m_TicksPerSec;
            if (m_ElapsedRun > bufferCheckRuntime)
            {
                CompleteQuadFrame();
                start = Xp::QueryPerformanceCounter();
                if (OK != (rc = CheckFinalSurface(CHECK_ONE_BUFFER)))
                    break;
                //update times and callwlate the next checkpoint
                m_ElapsedRun += (Xp::QueryPerformanceCounter()-start)/m_TicksPerSec;
                bufferCheckRuntime = m_ElapsedRun  + (m_BufferCheckMs / 1000.0);
            }

            if (m_DoubleBuffer)
                m_mglTestContext.SwapBuffers();

            if (OK != (rc = DoEndLoop(&nextEndLoopMs)))
                break;

            // Set progress update
            if (recordProgress)
                UpdateTestProgress();
        }
        if (runPowerThread)
        {
            // Run this explicitly to be able to check the RC.
            runPowerThread = false;
            rc = StopPowerThread();
        }

        // Finish the last draw(s).
        for (UINT32 t = 0; t < TIMER_NUM; t++)
        {
            if (m_Timers[t].GetState() != mglTimerQuery::Idle)
                m_ElapsedGpu += m_Timers[t].GetSeconds();
        }
    }

    rc = CompleteQuadFrame();

    rc = FireCallbacks(ModsTest::MISC_B, Callbacks::RUN_ON_ERROR,
                       args, "Pre-CheckBuffer");

    // Update benchmarking data that is readable from JS between runs.
    m_RememberedFrames = static_cast<double>(m_FrameCount);
    m_RememberedPixels = m_RememberedFrames * m_Width * m_Height;

    if (m_ElapsedGpu > 0.0 || m_ElapsedRun > 0.0)
    {
        if (m_ElapsedGpu > 0.0)
        {
            m_RememberedFps = m_RememberedFrames / m_ElapsedGpu;
        }
        else // m_ElapsedRun > 0.0
        {
            m_RememberedFps = m_RememberedFrames / m_ElapsedRun;
        }
        m_RememberedPps = m_RememberedFps * m_Width * m_Height;
        // Callwlate bytes per second (counting read and write separately).
        // In Torus mode, forget it -- too hard to callwlate aclwrately.
        // Otherwise, we read & write all of C & Z (if enabled) each frame
        // except that half of frames are "far" and cover only about 80%.
        if (!m_Torus)
        {
            const Tee::Priority pri = GetMemError(0).GetAutoStatus() ?
                                      Tee::PriNormal : Tee::PriLow;

            double factor = 2.0 * 0.9; // r+w, 80% on odd frames.
            if (m_Ztest)
                factor *= 2.0;
            GpuTest::RecordBps(m_RememberedFrames * m_Width * m_Height * factor,
                               m_ElapsedGpu > 0.0 ? m_ElapsedGpu : m_ElapsedRun,
                               pri);
        }
    }
    else
    {
        m_RememberedFps = 0;
        m_RememberedPps = 0;
    }

    // Check final surface.
    if (OK == rc)
        rc = CheckFinalSurface(CHECK_BOTH_BUFFERS);

    UINT64 errCount = 0;

    for (UINT32 sd = 0; sd < pGpuDev->GetNumSubdevices() && !m_BeQuiet; sd++)
    {
        errCount += GetMemError(sd).GetErrorCount();
    }

    // Unlike most memory tests, GLStressTest has its
    // own favorite error code.

    if (rc == RC::BAD_MEMORY || (rc == RC::OK && errCount > m_AllowedMiscompares))
    {
        rc.Clear();
        rc = RC::GPU_STRESS_TEST_FAILED;
    }

    // Warn memqual guys about how unreliable GLStress is for mem-locations.

    if (errCount && !m_BeQuiet)
    {
        if (!m_ReportMemLocs)
        {
            MASSERT(m_CantReportMemLocsReason != NULL);
            Printf(Tee::PriWarn,
                   "Errors found, but %s can't decode FB offsets because "
                   "the primary surface %s.\n",
                   GetName().c_str(),
                   m_CantReportMemLocsReason);
        }
        else if (m_MemLocsAreMisleading)
        {
            Printf(Tee::PriWarn,
                   "Error not reporting FB offsets because they may be misleading for the"
                   " following reasons:\n");
            if (m_NumTextures)
                Printf(Tee::PriWarn, " - %s.NumTextures is > 0\n", GetName().c_str());
            if (m_Stencil)
                Printf(Tee::PriWarn, " - %s.Stencil is true\n", GetName().c_str());
            if (m_Ztest)
                Printf(Tee::PriWarn, " - %s.Ztest is true\n", GetName().c_str());
        }
    }

    if ((rc == OK) && m_EnablePowerWait && m_TegraPowerWait && m_PrintPowerEvents)
    {
        const UINT64 pgCount = TegraElpgWait::ReadTransitions();
        Printf(Tee::PriNormal, "%s : %llu graphics power gating events oclwrred\n",
               GetName().c_str(), pgCount - startPGCount);
    }
    else if ((rc == OK) && (m_EnablePowerWait || m_EnableDeepIdle) && m_PrintPowerEvents)
    {
        // Get the ending power gating count
        rc = pPmu->GetPowerGatingParameters(&pgParams);
        Printf(Tee::PriLow, "%s : %llu graphics power gating events oclwrred\n",
               GetName().c_str(), (UINT64)(pgParams[0].parameterValue - startPGCount));

        if (m_EnableDeepIdle)
        {
            LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS endStats = { 0 };
            rc = pPmu->GetDeepIdleStatistics(PMU::DEEP_IDLE_NO_HEADS, &endStats);
            Printf(Tee::PriLow,
                   "%s : Deep Idle Stats - Attempts (%d) Entries (%d), Exits(%d)\n",
                   GetName().c_str(),
                   (UINT32)endStats.attempts, (UINT32)endStats.entries,
                   (UINT32)endStats.exits);
        }
    }

    if (m_PulseMode == PULSE_TILE_MASK_GPC_SHUFFLE)
    {
        m_GPCShuffleStats.PrintReport(GetVerbosePrintPri(), "GPC shuffle");
    }

    return rc;
}

//------------------------------------------------------------------------------
RC GLStressTest::CheckFinalSurface(bool bCheckBothBuffers)
{
    StickyRC rc;
    INT64 start = Xp::QueryPerformanceCounter();
    if (m_DoGpuCompare)
    {
        if (CopyBadPixelsToFront() > m_AllowedMiscompares)
        {
            rc = RC::GPU_STRESS_TEST_FAILED;
            CopyDrawFboToFront();   // get rid of the running 'dots'
            m_ElapsedGpuIdle += (Xp::QueryPerformanceCounter()-start)/m_TicksPerSec;
            CheckBuffer();          // and possibly dump the png.
            return rc;
        }
    }
    else
    {
        if (m_mglTestContext.UsingFbo())
        {
            rc = m_mglTestContext.CopyFboToFront();
        }
        rc = CheckBuffer();

        if (m_DoubleBuffer)
        {
            m_mglTestContext.SwapBuffers();
            if (bCheckBothBuffers)
            {
                rc = CheckBuffer();
            }
        }
    }
    m_ElapsedGpuIdle += (Xp::QueryPerformanceCounter()-start)/m_TicksPerSec;
    return rc;
}

//------------------------------------------------------------------------------
// Make sure we return to the "cleared" surface which is every 4 complete frames.
RC GLStressTest::CompleteQuadFrame()
{
    StickyRC rc;
    const UINT32 boundary = 4 * m_Rows;
    const UINT32 stripsToNextBoundary = boundary - (m_StripCount % boundary);

    if (stripsToNextBoundary != boundary)
    {
        Printf(Tee::PriLow, "CompleteQuadFrame():Drawing to cleared point.\n");
        // Draw ahead to "cleared" point  before checking the color buffer.
        INT64 start = Xp::QueryPerformanceCounter();
        rc = Draw(stripsToNextBoundary, &m_Timers[TIMER_CHKBUF]);

        // Make sure the pushbuffers are empty before checking the surface.
        glFlush();
        m_ElapsedGpu += m_Timers[TIMER_CHKBUF].GetSeconds();
        m_ElapsedRun += (Xp::QueryPerformanceCounter()-start)/m_TicksPerSec;
        rc = mglGetRC();
    }
    return rc;
}

//------------------------------------------------------------------------------
/* virtual */
RC GLStressTest::Cleanup()
{
    StickyRC firstRc;

    firstRc = Free();

    // Release the display, shut down the GL library.

    firstRc = m_mglTestContext.Cleanup();
    if ((m_EnablePowerWait && ! m_TegraPowerWait)
        || m_ToggleELPGOlwideo || m_EnableDeepIdle)
    {
        firstRc = m_ElpgOwner.ReleaseElpg();
        if (m_EnableDeepIdle)
            m_PStateOwner.ReleasePStates();
    }

    firstRc = GpuMemTest::Cleanup();

    return firstRc;
}

RC GLStressTest::SetupTileMasking()
{
    RC rc;

    if ( (m_PulseMode != PULSE_TILE_MASK_USER) &&
         (m_PulseMode != PULSE_TILE_MASK_GPC) &&
         (m_PulseMode != PULSE_TILE_MASK_GPC_SHUFFLE) )
    {
        return rc;
    }

    GpuSubdevice *subdev = GetBoundGpuSubdevice();
    LW2080_CTRL_GR_GET_GLOBAL_SM_ORDER_PARAMS smOrder = {};
    CHECK_RC(LwRmPtr()->ControlBySubdevice(subdev,
        LW2080_CTRL_CMD_GR_GET_GLOBAL_SM_ORDER,
        &smOrder, sizeof(smOrder)));

    float smIdMap[LW2080_CTRL_CMD_GR_GET_GLOBAL_SM_ORDER_MAX_SM_COUNT] = { };
    m_HighestGPCIdx = 0;
    for (UINT32 idx = 0; idx < smOrder.numSm; idx++)
    {
        if (smOrder.globalSmId[idx].gpcId > m_HighestGPCIdx)
        {
            m_HighestGPCIdx = smOrder.globalSmId[idx].gpcId;
        }
        smIdMap[idx] = 1.0f * smOrder.globalSmId[idx].gpcId;
    }

    glGenBuffers(CONST_NUM, m_ConstIds);

    glBindBuffer(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, m_ConstIds[CONST_SMIDMAP]);
    glBufferData(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, sizeof(smIdMap),
        smIdMap, GL_STATIC_DRAW);
    glBindBuffer(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, 0);
    glBindBufferBaseLW(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, 0, m_ConstIds[CONST_SMIDMAP]);


    if (m_PulseMode == PULSE_TILE_MASK_GPC_SHUFFLE)
    {
        if (m_NumActiveGPCs > (m_HighestGPCIdx + 1))
        {
            m_NumActiveGPCs = m_HighestGPCIdx + 1;
        }
        UpdateTileMask();
    }
    else
    {
        UINT32 tileMask;
        switch (m_PulseMode)
        {
            case PULSE_TILE_MASK_USER:
                tileMask = m_TileMask;
                break;
            case PULSE_TILE_MASK_GPC:
                CHECK_RC(subdev->HwGpcToVirtualGpcMask(m_TileMask, &tileMask));
                break;
            default:
                tileMask = 1;
        }
        float enabledGpcs[8*sizeof(tileMask)] = { };
        for (INT32 bitIdx = Utility::BitScanForward(tileMask);
             bitIdx != -1;
             bitIdx = Utility::BitScanForward(tileMask, bitIdx + 1))
        {
            enabledGpcs[bitIdx] = 1.0f;
        }

        glBindBuffer(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, m_ConstIds[CONST_ENABLEDGPCS]);
        glBufferData(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, sizeof(enabledGpcs),
            enabledGpcs, GL_STATIC_DRAW);
        glBindBuffer(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, 0);
    }
    glBindBufferBaseLW(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, 1, m_ConstIds[CONST_ENABLEDGPCS]);

    // Equivalent of driver internally created shaders when none
    // is explicitly set - this is needed to inject tile masking:
    if (m_GpuProgs[Frag].m_Text.empty())
    {
        string &fixedShader = m_GpuProgs[Frag].m_Text;
        fixedShader = "!!LWfp5.0\n";
        if (m_Fog)
        {
            if (m_LinearFog)
            {
                fixedShader += "OPTION ARB_fog_linear;\n";
            }
            if (m_NumTextures)
            {
                if (m_TwoTex)
                {
                    fixedShader +=
R"lwfp(TEMP r0;
TEMP r1;
TXP r0, fragment.texcoord[0], texture[0], 2D;
MUL r0, fragment.color, r0;
TXP r1, fragment.texcoord[1], texture[1], 2D;
LRP r1.xyz, r1.w, r1, r0;
MOV r1.w, r0;
MAD.SAT r0, -state.fog.params.x, |fragment.fogcoord.x|, state.fog.params.y;
MOV.SAT r0, r0;
LRP.SAT r0.xyz, r0, r1, state.fog.color;
MOV r0.w, r1;
MOV result.color, r0;
END
)lwfp";
                }
                else
                {
                    fixedShader +=
R"lwfp(TEMP r0;
FLOAT TEMP r1;
TXP r0, fragment.texcoord[0], texture[0], 2D;
LRP r0.xyz, r0.w, r0, fragment.color;
MOV r0.w, fragment.color;
MAD.SAT r1, -state.fog.params.x, |fragment.fogcoord.x|, state.fog.params.y;
MOV.SAT r1, r1;
LRP.SAT r1.xyz, r1, r0, state.fog.color;
MOV r1.w, r0;
MOV.SAT result.color, r1;
END
)lwfp";
                }
            }
            else
            {
                    fixedShader +=
R"lwfp(TEMP r0;
MAD.SAT r0, -state.fog.params.x, |fragment.fogcoord.x|, state.fog.params.y;
MOV.SAT r0, r0;
LRP.SAT result.color.xyz, r0, fragment.color, state.fog.color;
MOV result.color.w, fragment.color;
END
)lwfp";
            }
        }
        else
        {
            if (m_NumTextures)
            {
                if (m_TwoTex)
                {
                    fixedShader +=
R"lwfp(TEMP r0;
TEMP r1;
TXP r0, fragment.texcoord[0], texture[0], 2D;
MUL r0, fragment.color, r0;
TXP r1, fragment.texcoord[1], texture[1], 2D;
LRP r1.xyz, r1.w, r1, r0;
MOV r1.w, r0;
MOV.SAT result.color, r1;
END
)lwfp";
                }
                else
                {
                    fixedShader +=
R"lwfp(TEMP r0;
TXP r0, fragment.texcoord[0], texture[0], 2D;
MUL r0, fragment.color, r0;
MOV result.color, r0;
END
)lwfp";
                }
            }
            else
            {
                fixedShader +=
R"lwfp(MOV result.color, fragment.color;
END
)lwfp";
            }
        }
        m_GpuProgs[Frag].m_Text = fixedShader;
    }

    vector<string> shaderLines;

    CHECK_RC(Utility::Tokenizer(m_GpuProgs[Frag].m_Text, "\n", &shaderLines));

    const size_t numLines = shaderLines.size();
    if (numLines < 2)
    {
        Printf(Tee::PriError, "Shader source too small for tile masking injection\n");
        return RC::ILWALID_TEST_INPUT;
    }
    if ( (shaderLines[0].find("!!LWfp") == string::npos) &&
         (shaderLines[0].find("!!ARBfp") == string::npos) )
    {
        Printf(Tee::PriError, "Unsupported shader type for tile masking injection\n");
        return RC::ILWALID_TEST_INPUT;
    }

    size_t lineIdx;
    for (lineIdx = numLines - 1; lineIdx > 0; lineIdx--)
    {
        string &shaderLine = shaderLines[lineIdx];

        if (shaderLine.find("END") != string::npos)
        {
            switch (m_PulseMode)
            {
                case PULSE_TILE_MASK_GPC:
                case PULSE_TILE_MASK_GPC_SHUFFLE:
                    shaderLine =
R"lwfp( RET;
tileMasking:
 BUFFER smidmap[] = { program.buffer[0] };
 BUFFER enabledgpc[] = { program.buffer[1] };
 TEMP kiltemp;
 MOV kiltemp.x, fragment.smid.x;
 MOV kiltemp.x, smidmap[kiltemp.x];
 ROUND.U kiltemp.x, kiltemp.x;
 MOV.U.CC kiltemp.x, enabledgpc[kiltemp.x];
 KIL EQ.x;
 RET;
END)lwfp";
                    break;
                case PULSE_TILE_MASK_USER:
                    shaderLine =
R"lwfp( RET;
tileMasking:
 BUFFER enabledtile[] = { program.buffer[1] };
 TEMP kiltemp;
 DIV kiltemp.x, fragment.position.x, 16;
 FLR.U kiltemp.x, kiltemp.x;
 MOD.U kiltemp.x, kiltemp.x, 32;
 MOV.U.CC kiltemp.x, enabledtile[kiltemp.x];
 KIL EQ.x;
 RET;
END)lwfp";
                    break;
                default:
                    MASSERT(!"Missing tileMasking implementation");
                    break;
            }
        }
        else if (shaderLine.find("main:") != string::npos)
        {
            shaderLine = "main:\n CAL tileMasking;";
            break;
        }
        else if (shaderLine.find("OPTION") != string::npos)
        {
            shaderLine += "\nCAL tileMasking;";
            break;
        }
    }
    shaderLines[0] = "!!LWfp5.0";
    if (lineIdx == 0)
    {
        shaderLines[0] += "\nCAL tileMasking;";
    }

    string &fragText = m_GpuProgs[Frag].m_Text;
    fragText.clear();
    for (lineIdx = 0; lineIdx < numLines; lineIdx++)
    {
        fragText += shaderLines[lineIdx];
        fragText += "\n";
    }

    return rc;
}

//------------------------------------------------------------------------------
RC GLStressTest::Allocate ()
{
    RC rc = OK;

    if (m_DoGpuCompare)
    {
        // Allocate id numbers for frambuffers and the attached textures.
        glGenFramebuffersEXT(FBO_NUM, m_FboIds);
        glGenTextures(FBO_TEX_NUM, m_FboTexIds);

        // Set up the textures that we render to.

        glActiveTexture(GL_TEXTURE0_ARB);
        glBindTexture(GL_TEXTURE_2D, m_FboTexIds[FBO_TEX_DRAW_COLOR]);
        glTexImage2D(
                GL_TEXTURE_2D,
                0/*mm level*/,
                GL_RGBA8,
                m_Width,
                m_Height,
                0/*border*/,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                NULL);  // set up mm level but don't load image data
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glBindTexture(GL_TEXTURE_2D, m_FboTexIds[FBO_TEX_REF_COLOR]);
        glTexImage2D(
                GL_TEXTURE_2D,
                0, //mm level
                GL_RGBA8,
                m_Width,
                m_Height,
                0, // border
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                NULL);  // set up mm level but don't load image data
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glBindTexture(GL_TEXTURE_2D, m_FboTexIds[FBO_TEX_DEPTH_STENCIL]);

        //T30 doesn't support depth & stencil in the same buffer. So only work
        //with the depth buffer. Stencil is not worth the extra effort.
        const GLenum fmt =
            (m_ZDepth == 32) && mglExtensionSupported("GL_EXT_packed_depth_stencil")
            ? GL_DEPTH_STENCIL_LW : GL_DEPTH_COMPONENT;
        const GLenum type= m_ZDepth != 32 ?
            GL_UNSIGNED_INT : GL_UNSIGNED_INT_24_8_LW;
        glTexImage2D(
                GL_TEXTURE_2D,
                0,              // mm level
                fmt,
                m_Width,
                m_Height,
                0,              // border
                fmt,
                type,
                NULL);  // set up mm level but don't load image data
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glBindTexture(GL_TEXTURE_2D, 0);

        // Set up the reference surface.
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FboIds[FBO_REFERENCE]);
        glFramebufferTexture2DEXT(
                GL_FRAMEBUFFER_EXT,
                GL_COLOR_ATTACHMENT0_EXT,
                GL_TEXTURE_2D,
                m_FboTexIds[FBO_TEX_REF_COLOR],
                0);
        glFramebufferTexture2DEXT(
                GL_FRAMEBUFFER_EXT,
                GL_DEPTH_ATTACHMENT_EXT,
                GL_TEXTURE_2D,
                m_FboTexIds[FBO_TEX_DEPTH_STENCIL],
                0);
        if (m_ZDepth >= 32)
        {
            glFramebufferTexture2DEXT(
                    GL_FRAMEBUFFER_EXT,
                    GL_STENCIL_ATTACHMENT_EXT,
                    GL_TEXTURE_2D,
                    m_FboTexIds[FBO_TEX_DEPTH_STENCIL],
                    0);
        }
        CHECK_RC(mglCheckFbo());

        // Set up the drawing surface.
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FboIds[FBO_DRAW]);
        glFramebufferTexture2DEXT(
                GL_FRAMEBUFFER_EXT,
                GL_COLOR_ATTACHMENT0_EXT,
                GL_TEXTURE_2D,
                m_FboTexIds[FBO_TEX_DRAW_COLOR],
                0);
        glFramebufferTexture2DEXT(
                GL_FRAMEBUFFER_EXT,
                GL_DEPTH_ATTACHMENT_EXT,
                GL_TEXTURE_2D,
                m_FboTexIds[FBO_TEX_DEPTH_STENCIL],
                0);

        if (m_ZDepth >= 32)
        {
            glFramebufferTexture2DEXT(
                    GL_FRAMEBUFFER_EXT,
                    GL_STENCIL_ATTACHMENT_EXT,
                    GL_TEXTURE_2D,
                    m_FboTexIds[FBO_TEX_DEPTH_STENCIL],
                    0);
        }
        CHECK_RC(mglCheckFbo());

        MASSERT(0 == (rc = mglGetRC()));

        const char * gpuCompareFragProg =
                "!!ARBfp1.0\n"
                "OPTION LW_fragment_program2;\n"
                "TEMP R0;\n"
                "TEMP R1;\n"
                // Read the pixel from the actual and expected surfaces.
                "TEX R1, fragment.texcoord[2], texture[2], 2D;\n"
                "TEX R0, fragment.texcoord[2], texture[3], 2D;\n"
                "ADD R0, R0, -R1;\n"
                "DP4C R0.x,R0,R0;\n"
                "KIL EQ.x;\n"  // Exit w/o drawing.
                // Draw the pixel (will be counted as error).
                "MOV result.color, R1;\n"
                "END\n";

        glGenProgramsLW(1, &m_GpuCompareFragProgId);
        CHECK_RC(mglLoadProgram(
                GL_FRAGMENT_PROGRAM_LW,
                m_GpuCompareFragProgId,
                (GLsizei) strlen(gpuCompareFragProg),
                (const GLubyte *)gpuCompareFragProg));

        const char * gpuCopyFragProg =
            "!!ARBfp1.0\n"
            "OPTION LW_fragment_program2;\n"
            "TEX result.color, fragment.texcoord[2], texture[2], 2D;\n"
            "END\n";

        glGenProgramsLW(1, &m_GpuCopyFragProgId);
        CHECK_RC(mglLoadProgram(
                GL_FRAGMENT_PROGRAM_LW,
                m_GpuCopyFragProgId,
                (GLsizei) strlen(gpuCopyFragProg),
                (const GLubyte *)gpuCopyFragProg));

        glGenOcclusionQueriesLW(1, &m_GpuCompareOcclusionQueryId);
    }

    if (m_UseTessellation)
    {
        CHECK_RC(m_GpuProgs[TessCtrl].Load());
        CHECK_RC(m_GpuProgs[TessEval].Load());
    }

    if (m_Torus)
        CalcGeometrySizeTorus();
    else
        CalcGeometrySizePlanes();

    glGenBuffers(VBO_NUM, m_Vbos);
    glBindBuffer(GL_ARRAY_BUFFER, m_Vbos[VBO_VTX]);

    const size_t vxSize = m_BigVertex ? sizeof(vtx_f3_f3_f2) : sizeof(vtx);

    glBufferData(GL_ARRAY_BUFFER,
            m_NumVertexes * vxSize,
            NULL,
            GL_STATIC_DRAW);

    if (!m_GpuProgs[TessEval].IsLoaded())
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_Vbos[VBO_IDX]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                m_NumIndexes * sizeof (GLuint),
                NULL,
                GL_STATIC_DRAW);
    }

    CHECK_RC(SetupTileMasking());

    CHECK_RC(m_GpuProgs[Frag].Load());
    CHECK_RC(m_GpuProgs[Vtx].Load());

    if (m_Torus)
        GenGeometryTorus();
    else
        GenGeometryPlanes();

    // Load some textures.
    if (m_NumTextures)
    {
        RC rc = LoadTextures();
        if (OK != rc)
          return rc;
    }

    const mglTexFmtInfo * pFmtInfo =
            mglGetTexFmtInfo(mglTexFmt2Color(m_ActualColorSurfaceFormat));
    const UINT32 bpp = ColorUtils::PixelBytes(pFmtInfo->ExtColorFormat);

    m_ScreenBuf.resize(m_Width * m_Height * bpp);

    m_ClearColorPixel.resize(bpp);

    m_Timers = new mglTimerQuery[TIMER_NUM];
    if (!m_Timers[0].IsSupported())
    {
        // WAR for missing GL_EXT_timer_query on CheetAh
        if (GetBoundGpuSubdevice()->IsSOC())
        {
            if (!m_RuntimeMs)
            {
                m_RuntimeMs = m_LoopMs;
            }
        }
        else
        {
            Printf(Tee::PriError, "GL_EXT_timer_query is required\n");
            return RC::UNSUPPORTED_FUNCTION;
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
RC GLStressTest::LoadTextures ()
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glActiveTexture(GL_TEXTURE0_ARB);

    RC rc = mglGetRC();
    if (OK != rc)
      return rc;

    GLint numTex = m_NumTextures;

    m_TextureObjectIds = new GLuint [numTex];
    glGenTextures(numTex, m_TextureObjectIds);

    GLuint numTexels = m_TxSize * m_TxSize;
    GLenum txDim     = GL_TEXTURE_2D;

    if (m_Use3dTextures)
    {
        numTexels *= m_TxSize;
        txDim      = GL_TEXTURE_3D;
    }

    GLuint * buf = new GLuint [ numTexels ];
    if (!buf)
      return RC::CANNOT_ALLOCATE_MEMORY;

    m_PatternClass.ResetFillIndex();

    GLint txIdx;
    for (txIdx = 0; txIdx < numTex; txIdx++)
    {
        // Fill the image.
        if (txIdx == m_NumTextures - 1)
        {
            // Last texture has random data.
            Memory::FillRandom(buf, 37, numTexels*4);
        }
        else
        {
            if (m_TxImages.size())
            {
                size_t i = txIdx % m_TxImages.size();
                CHECK_RC(ImageFile::Read(
                        m_TxImages[i].c_str(),
                        true, // doTile
                        ColorUtils::A8B8G8R8,
                        buf,
                        m_TxSize,
                        m_TxSize,
                        m_TxSize * 4));
                // @@@ broken for Use3dTextures
            }
            else
            {
                GLuint * pFill    = buf;
                m_PatternClass.FillMemoryIncrementing( (void *)pFill,
                                                        numTexels * 4, //pitch in bytes
                                                        1, // lines
                                                        numTexels * 4, // width in bytes
                                                        GetTestConfiguration()->DisplayDepth()); // depth
            }
        }
        glBindTexture(txDim, m_TextureObjectIds[txIdx]);

        if (m_UseMM)
        {
            glTexParameteri(txDim, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(txDim, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        else
        {
            glTexParameteri(txDim, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(txDim, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }

        GLuint level;
        GLuint sz;
        for (level = 0, sz = m_TxSize;  sz;  level++, sz >>= 1)
        {
            if ((level > 0) && (!m_UseMM))
                break;

            if (m_Use3dTextures)
            {
                glTexImage3D(
                    GL_TEXTURE_3D,
                    level,
                    GL_RGBA8,
                    sz,
                    sz,
                    sz,
                    0,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    buf );
            }
            else
            {
                glTexImage2D(
                    GL_TEXTURE_2D,
                    level,
                    GL_RGBA8,
                    sz,
                    sz,
                    0,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    buf );
            }
            RC rc = mglGetRC();
            if (OK != rc)
            {
                delete [] buf;
                return rc;
            }
        }
    }
    glBindTexture(txDim, 0);

    delete [] buf;
    return OK;
}

//------------------------------------------------------------------------------
RC GLStressTest::Free ()
{
    StickyRC firstRc;

    if (m_pElpgToggle)
    {
        firstRc = m_pElpgToggle->Stop();
        delete m_pElpgToggle;
        m_pElpgToggle = NULL;
    }

    if (m_NumTextures)
    {
        if (m_TextureObjectIds)
        {
            glDeleteTextures(m_NumTextures, m_TextureObjectIds);
            delete [] m_TextureObjectIds;
        }
        m_TextureObjectIds = NULL;
    }
    for (int i = 0; i < ProgType_NUM; i++)
    {
        m_GpuProgs[i].Unload();
    }
    if (m_GpuCompareFragProgId)
    {
        glDeleteProgramsLW(1, &m_GpuCompareFragProgId);
        m_GpuCompareFragProgId = 0;
    }
    if (m_GpuCopyFragProgId)
    {
        glDeleteProgramsLW(1, &m_GpuCopyFragProgId);
        m_GpuCopyFragProgId = 0;
    }

    if (m_Timers)
    {
        delete [] m_Timers;
        m_Timers = 0;
    }
    if (m_Vbos[0])
    {
        glDeleteBuffers(VBO_NUM, m_Vbos);
        memset(m_Vbos, 0, sizeof(m_Vbos));
    }
    if (m_ConstIds[0])
    {
        glDeleteBuffers(CONST_NUM, m_ConstIds);
        memset(m_ConstIds, 0, sizeof(m_ConstIds));
    }

    if (m_DoGpuCompare)
    {
        if (m_FboIds[0])
            glDeleteFramebuffersEXT(FBO_NUM, m_FboIds);
        if (m_FboTexIds[0])
            glDeleteTextures(FBO_TEX_NUM, m_FboTexIds);
        if (m_GpuCompareOcclusionQueryId)
            glDeleteOcclusionQueriesLW(1, &m_GpuCompareOcclusionQueryId);

        UINT32 i;
        for (i = 0; i < FBO_NUM; i++)
            m_FboIds[i] = 0;
        for (i = 0; i < FBO_TEX_NUM; i++)
            m_FboTexIds[i] = 0;
        m_GpuCompareOcclusionQueryId = 0;
    }

    return firstRc;
}

//------------------------------------------------------------------------------
RC GLStressTest::Clear ()
{
    GLfloat clearColor[4];
    clearColor[0] = ((m_ClearColor >> 16) & 0xff) * (1.0F/255.0F); // red
    clearColor[1] = ((m_ClearColor >>  8) & 0xff) * (1.0F/255.0F); // green
    clearColor[2] = ((m_ClearColor >>  0) & 0xff) * (1.0F/255.0F); // blue
    clearColor[3] = ((m_ClearColor >> 24) & 0xff) * (1.0F/255.0F); // alpha

    // Clear the screen/FBO.
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearDepth(0.0);
    glClearStencil(0);

    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // If we are using a FBO then we are clearing the FBO buffer not the primary surface.
    if (m_DoGpuCompare)
    {
        // Clear the "reference" FBO.
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FboIds[FBO_REFERENCE]);
        glClear(GL_COLOR_BUFFER_BIT);

        // Switch back to the "draw" FBO.
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FboIds[FBO_DRAW]);
        glClear(GL_COLOR_BUFFER_BIT);
        CopyDrawFboToFront();
    }
    else if (m_mglTestContext.UsingFbo())
    {
        glClear(GL_COLOR_BUFFER_BIT);
        m_mglTestContext.CopyFboToFront();
        glFinish();
    }
    else    // we are not bound to any framebuffer and are simply writing
    {       // to the desktop surface.
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // read 1 pixel for reference comparison later.
    const mglTexFmtInfo * pFmtInfo = mglGetTexFmtInfo(mglTexFmt2Color(m_ActualColorSurfaceFormat));
    glReadPixels(
        0,
        0,
        1, //Width,
        1, //Height,
        pFmtInfo->ExtFormat,
        pFmtInfo->Type,
        &m_ClearColorPixel[0]
    );

    glFinish();
    return mglGetRC();
}

//------------------------------------------------------------------------------
void GLStressTest::SetupViewport()
{
    GLint x = 0;
    GLint y = 0;
    GLsizei w = m_Width;
    GLsizei h = m_Height;

    if (m_ViewportXOffset > 0)
    {
        x = m_ViewportXOffset;
        w -= m_ViewportXOffset;
    }
    else
    {
        w += m_ViewportXOffset;
    }

    if (m_ViewportYOffset > 0)
    {
        y = m_ViewportYOffset;
        h -= m_ViewportYOffset;
    }
    else
    {
        h += m_ViewportYOffset;
    }
    glViewport (x, y, w, h);
}

//------------------------------------------------------------------------------
RC GLStressTest::SetupDrawState ()
{
    RC rc;

    if (m_DoGpuCompare)
    {
        // Switch to the "draw" FBO.
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FboIds[FBO_DRAW]);
    }

    for (int i = 0; i < ProgType_NUM; i++)
    {
        m_GpuProgs[i].Setup();
    }

    if (m_GpuProgs[Vtx].IsLoaded())
    {
        glTrackMatrixLW(GL_VERTEX_PROGRAM_LW, 0, GL_MODELVIEW_PROJECTION_LW, GL_IDENTITY_LW);
        glTrackMatrixLW(GL_VERTEX_PROGRAM_LW, 4, GL_MODELVIEW,               GL_ILWERSE_TRANSPOSE_LW);
    }

    if (m_GpuProgs[TessCtrl].IsLoaded())
    {
        // In tessellation mode, we draw "patches" that are horizontal
        // strips the width of the screen.
        glPatchParameteriLW(GL_PATCH_VERTICES, 4);
    }

    GLfloat x_to_s[4] = {0.0, 0.0, 0.0, 0.0};
    GLfloat y_to_t[4] = {0.0, 0.0, 0.0, 0.0};
    if (m_TxSize)
    {
        x_to_s[0] = (GLfloat)(pow(2.0, m_TxD) / m_TxSize);
        y_to_t[1] = (GLfloat)(pow(2.0, m_TxD) / m_TxSize);
    }

    GLfloat z_to_r[4] = {0.0, 0.0, 0.0, 0.0};
    z_to_r[2] = 1.0f / 200.0f;

    GLfloat xy_to_q[4] = {0.0, 0.0, 0.0, 1.0};
    xy_to_q[0] =  0.9f/m_Width;
    xy_to_q[1] =  0.9f/m_Height;

    if (m_BoringXform)
    {
        x_to_s[0] = 1.0;
        y_to_t[1] = 1.0;
        z_to_r[2] = 1.0;
        xy_to_q[0] = xy_to_q[1] = 0.0;
    }

    if (m_DrawLines)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    if (m_SmoothLines)
    {
        glEnable(GL_LINE_SMOOTH);
    }

    if (m_StippleLines)
    {
        glEnable(GL_LINE_STIPPLE);

        // Pass a 16-bit integer to set the stipple pattern.
        // It should look like: " - -- -  - -- - "
        glLineStipple(1, 0x5A5A);
    }

    // set up texturing
    if (m_NumTextures)
    {
        GLenum txDim = GL_TEXTURE_2D;
        if (m_Use3dTextures)
          txDim     = GL_TEXTURE_3D;

        if (m_TwoTex)
        {
            glActiveTexture(GL_TEXTURE1);
            glEnable(txDim);
            glBindTexture(txDim, m_TextureObjectIds[0]);
            glEnable(GL_TEXTURE_GEN_S);
            glEnable(GL_TEXTURE_GEN_T);
            if (m_Use3dTextures)
              glEnable(GL_TEXTURE_GEN_R);
            if (m_TxGenQ)
              glEnable(GL_TEXTURE_GEN_Q);
            glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
            glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
            glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
            glTexGeni(GL_Q, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);

            GLfloat x_to_s2[4] = { x_to_s[0], x_to_s[1], x_to_s[2], x_to_s[3] };
            GLfloat y_to_t2[4] = { y_to_t[0], y_to_t[1], y_to_t[2], y_to_t[3] };
            GLfloat z_to_r2[4] = { z_to_r[0], z_to_r[1], z_to_r[2], z_to_r[3] };

            if (m_OffsetSecondTexture)
            {
                x_to_s2[3] = 0.5;   // offset S by 0.5*W
                y_to_t2[3] = 0.5;   // offset T by 0.5*W
                z_to_r2[3] = 0.5;   // offset R by 0.5*W
            }
            glTexGenfv(GL_S, GL_OBJECT_PLANE, x_to_s2);
            glTexGenfv(GL_T, GL_OBJECT_PLANE, y_to_t2);
            glTexGenfv(GL_R, GL_OBJECT_PLANE, z_to_r2);
            glTexGenfv(GL_Q, GL_OBJECT_PLANE, xy_to_q);

            glTexElwi(GL_TEXTURE_ELW, GL_TEXTURE_ELW_MODE, GL_DECAL);

            CHECK_RC(mglGetRC());
        }
        glActiveTexture(GL_TEXTURE0);
        glEnable(txDim);
        glEnable(GL_TEXTURE_GEN_S);
        glEnable(GL_TEXTURE_GEN_T);
        if (m_Use3dTextures)
          glEnable(GL_TEXTURE_GEN_R);
        if (m_TxGenQ)
          glEnable(GL_TEXTURE_GEN_Q);
        glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
        glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
        glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
        glTexGeni(GL_Q, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
        glTexGenfv(GL_S, GL_OBJECT_PLANE, x_to_s);
        glTexGenfv(GL_T, GL_OBJECT_PLANE, y_to_t);
        glTexGenfv(GL_R, GL_OBJECT_PLANE, z_to_r);
        glTexGenfv(GL_Q, GL_OBJECT_PLANE, xy_to_q);
        CHECK_RC(mglGetRC());
    }
    // Set up projection
    if (m_BoringXform)
    {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }
    else
    {
        if (m_Torus)
        {
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();

            // Torus diameter is 10.0.
            // Scale our world so that the lesser of w,h is 80% of torus diameter.
            // World depth is always 80% of torus diameter.

            const double smallDim = m_TorusDiameter * 0.4;
            if (m_Width < m_Height)
            {
                // Tall, narrow window.
                const double bigDim   = smallDim * m_Height / m_Width;

                glOrtho (-smallDim, smallDim,   // left, right
                         -bigDim, bigDim,       // lower, upper
                         smallDim, 3*smallDim); // near, far
            }
            else
            {
                // Wide, short window.
                const double bigDim   = smallDim * m_Width / m_Height;

                glOrtho (-bigDim, bigDim,       // left, right
                         -smallDim, smallDim,   // lower, upper
                         smallDim, 3*smallDim); // near, far
            }

            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            glTranslated(0.0, 0.0, -2.0 * smallDim);
        }
        else
        {
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glFrustum( m_Width/-2.0,  m_Width/2.0,
                       m_Height/-2.0, m_Height/2.0,
                       100, 300 );

            if (m_Rotate)
              glRotatef(-180.0, 0.0, 0.0, 1.0);

            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            glTranslated(m_Width/-2.0, m_Height/-2.0, -200);
        }
    }

    CHECK_RC(mglGetRC());

    SetupViewport();

    if (m_NumLights)
    {
        // Lighting:
        glEnable(GL_LIGHTING);

        GLfloat white[4] = { 1.0, 1.0, 1.0, 1.0};
        GLfloat gray[4]  = { 0.8F, 0.8F, 0.8F, 0.8F};
        GLfloat cols[8*4] =
          { 1.0F, 1.0F, 1.0F, 1.0F,
            1.0F, 0.3F, 0.3F, 1.0F,
            0.3F, 1.0F, 0.3F, 1.0F,
            0.3F, 0.3F, 1.0F, 1.0F,
            1.0F, 1.0f, 0.3F, 1.0F,
            1.0F, 0.3F, 1.0F, 1.0F,
            0.3F, 1.0F, 1.0F, 1.0F,
            1.0F, 0.3F, 0.3F, 1.0F
          };
        GLfloat locs[8*4] =
          { m_Width*3/6.0f, m_Height*5/6.0f,  95.0f, 1.0f,
            m_Width*1/6.0f, m_Height*1/6.0f,  95.0f, 1.0f,
            m_Width*5/6.0f, m_Height*1/6.0f,  95.0f, 1.0f,
            m_Width*3/6.0f, m_Height*3/6.0f,  95.0f, 1.0f,
            m_Width*5/6.0f, m_Height*5/6.0f,  95.0f, 1.0f,
            m_Width*1/6.0f, m_Height*5/6.0f,  95.0f, 1.0f,
            m_Width*3/6.0f, m_Height*1/6.0f,  95.0f, 1.0f,
            m_Width*2/6.0f, m_Height*3/6.0f,  95.0f, 1.0f
          };

        GLuint L;
        for (L = 0; L < m_NumLights; L++)
        {
            glEnable(GL_LIGHT0+L);
            glLightfv(GL_LIGHT0+L, GL_POSITION, locs + L*4);
            glLightf (GL_LIGHT0+L, GL_LINEAR_ATTENUATION, 0.001F);
            glLightfv(GL_LIGHT0+L, GL_DIFFUSE, cols + L*4);

            if (m_SpotLights)
              glLightf (GL_LIGHT0+L, GL_SPOT_EXPONENT, 25);

            if (L == 0)
              glLightfv(GL_LIGHT0, GL_AMBIENT, gray);
        }

        glMaterialfv(GL_FRONT, GL_AMBIENT, white );
        CHECK_RC(mglGetRC());
    }

    // Set up the vertex array pointers.
    glEnableClientState(GL_VERTEX_ARRAY);

    if (m_BigVertex)
    {
        const GLvoid * pVtxData = NullPtr + offsetof(vtx_f3_f3_f2, pos);
        glVertexPointer(3, GL_FLOAT, sizeof(vtx_f3_f3_f2), pVtxData);

        if (m_Normals)
        {
            const GLvoid * pNormData = NullPtr + offsetof(vtx_f3_f3_f2, norm);

            glEnableClientState(GL_NORMAL_ARRAY);
            glNormalPointer(GL_FLOAT, sizeof(vtx_f3_f3_f2), pNormData);
        }

        const GLvoid * pTex0Data = NullPtr + offsetof(vtx_f3_f3_f2, tex0);

        glClientActiveTexture(GL_TEXTURE0_ARB + 0);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, sizeof(vtx_f3_f3_f2), pTex0Data);
    }
    else
    {
        if (m_BoringXform)
        {
            const GLvoid * pVtxData = NullPtr + offsetof(vtx, f4.pos);

            if (m_SendW)
              glVertexPointer( 4, GL_FLOAT, sizeof(vtx), pVtxData);
            else
              glVertexPointer( 3, GL_FLOAT, sizeof(vtx), pVtxData);
        }
        else
        {
            const GLvoid * pVtxData = NullPtr + offsetof(vtx, s4s3.pos);

            if (m_SendW)
              glVertexPointer( 4, GL_SHORT, sizeof(vtx), pVtxData);
            else
              glVertexPointer( 3, GL_SHORT, sizeof(vtx), pVtxData);

            if (m_Normals)
            {
                const GLvoid * pNormData = NullPtr + offsetof(vtx, s4s3.norm);

                glEnableClientState(GL_NORMAL_ARRAY);
                glNormalPointer( GL_SHORT, sizeof(vtx), pNormData);
            }
        }
    }
    CHECK_RC(mglGetRC());

    // Set up fragment operations:
    if (m_Ztest)
        glEnable(GL_DEPTH_TEST);
    if (m_Blend)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else
    {
        glEnable(GL_COLOR_LOGIC_OP);
        glLogicOp(GL_XOR);
    }
    if (m_Stencil)
        glEnable(GL_STENCIL_TEST);
    if (m_AlphaTest)
    {
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 0.0);
    }

    if (m_Fog)
    {
        GLfloat fogcolor[4] = { 1.0f, 1.0f, 0.8f, 0.8f };

        glEnable(GL_FOG);
        glFogfv(GL_FOG_COLOR, fogcolor);

        if (m_LinearFog)
        {
            glFogi(GL_FOG_MODE, GL_LINEAR);
            glFogf(GL_FOG_DENSITY, 0.625f);
            glFogf(GL_FOG_START, 100.0f);  // @@@ check these distances
            glFogf(GL_FOG_END, 150.0f);
        }
        else
        {
            glFogi(GL_FOG_MODE, GL_EXP2);
            glFogf(GL_FOG_DENSITY, 1.0f/(2.0f*200));
            glFogi(GL_FOG_DISTANCE_MODE_LW, GL_EYE_RADIAL_LW);
        }
    }

    if (m_LwllBack)
    {
        glEnable(GL_LWLL_FACE);
        glLwllFace(GL_BACK);
        glFrontFace(GL_CW);
    }

    glGetIntegerv(GL_SCISSOR_BOX, m_ScissorRect);

    SetupScissor(m_DrawPct);

    // Clear any old "software rendering oclwrred" state in the DGL code.
    dglGetDispatchMode();

    return mglGetRC();
}

//------------------------------------------------------------------------------
void GLStressTest::SetupScissor(double drawPct)
{
    // Choose new scissor boundaries for drawPct as many pixels
    // as the full screen holds (min 0.5% of pixels, max full screen).

    const GLint fullW = m_WindowRect[2];
    const GLint fullH = m_WindowRect[3];
    const GLint fullPixels = fullW * fullH;

    // Colwert DrawPct to a draw ratio, limit to safe range for log().
    const double dr = max(0.005, min(1.0, drawPct / 100.0));

    // Callwlate new height to retain original aspect ratio and yield the
    // right approximate target pixel count.
    const GLint newH = static_cast<GLint>
        (exp(log((double)fullH) + log(dr)/2.0) + 0.5);

    // Find the integer width that gets us closest to the target pixel count.
    const GLint tgtPixels  = static_cast<GLint>(fullPixels * dr + 0.5);
    const GLint newW = (tgtPixels + newH/2) / newH;

    GLint newRect[4];
    newRect[0] = m_WindowRect[0] + (fullW - newW) / 2;
    newRect[1] = m_WindowRect[1] + (fullH - newH) / 2;
    newRect[2] = newW;
    newRect[3] = newH;

    if (memcmp(newRect, m_ScissorRect, sizeof(m_ScissorRect)))
    {
        Printf(Tee::PriDebug, "New Scissor = %dx%d\n", newW, newH);
        memcpy(m_ScissorRect, newRect, sizeof(m_ScissorRect));

        if (drawPct < 100.0)
        {
            glScissor(m_ScissorRect[0], m_ScissorRect[1],
                      m_ScissorRect[2], m_ScissorRect[3]);
            glEnable(GL_SCISSOR_TEST);
        }
        else
        {
            glDisable(GL_SCISSOR_TEST);
        }
    }
}

//------------------------------------------------------------------------------
RC GLStressTest::SuspendDrawState ()
{
    //unbind the current FBO and revert back to the default front buffer.
    if (m_DoGpuCompare)
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    for (int i = 0; i < ProgType_NUM; i++)
        m_GpuProgs[i].Suspend();

    // Texture
    if (m_NumTextures)
    {
        GLenum txDim = GL_TEXTURE_2D;
        if (m_Use3dTextures)
          txDim     = GL_TEXTURE_3D;

        for (int i = 0; i < 2; i++)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            glDisable(txDim);
        }
    }

    glViewport (0, 0, m_Width, m_Height);

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_COLOR_LOGIC_OP);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_POLYGON_STIPPLE);
    glDisable(GL_FOG);
    glDisable(GL_LWLL_FACE);
    glDisable(GL_DITHER);
    return mglGetRC();
}

//------------------------------------------------------------------------------
void GLStressTest::RestoreDrawState ()
{
    if (m_DoGpuCompare)
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FboIds[FBO_DRAW]);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    for (int i = 0; i < ProgType_NUM; i++)
        m_GpuProgs[i].Resume();

    // Texture
    if (m_NumTextures)
    {
        GLenum txDim = GL_TEXTURE_2D;
        if (m_Use3dTextures)
          txDim     = GL_TEXTURE_3D;

        if (m_TwoTex)
        {
            glActiveTexture(GL_TEXTURE1_ARB);
            glEnable(txDim);
        }
        glActiveTexture(GL_TEXTURE0_ARB);
        glEnable(txDim);
    }

    SetupViewport();

    if (m_NumLights)
        glEnable(GL_LIGHTING);

    if (m_Ztest)
        glEnable(GL_DEPTH_TEST);

    if (m_Blend)
        glEnable(GL_BLEND);
    else
        glEnable(GL_COLOR_LOGIC_OP);

    if (m_Stencil)
        glEnable(GL_STENCIL_TEST);

    if (m_AlphaTest)
        glEnable(GL_ALPHA_TEST);

    if (m_Fog)
        glEnable(GL_FOG);

    if (m_LwllBack)
        glEnable(GL_LWLL_FACE);

    SetupDrawState();
}

//------------------------------------------------------------------------------
GLuint GLStressTest::CopyBadPixelsToFront ()
{
    GLuint badPixelCount = 0;

    SuspendDrawState();

    // Bind the "draw" color buffer as texture 2.
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_FboTexIds[FBO_TEX_DRAW_COLOR]);
    glEnable(GL_TEXTURE_2D);

    // Bind the "reference" color buffer as texture 3.
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_FboTexIds[FBO_TEX_REF_COLOR]);
    glEnable(GL_TEXTURE_2D);

    glEnable(GL_FRAGMENT_PROGRAM_LW);
    glBindProgramLW(GL_FRAGMENT_PROGRAM_LW, m_GpuCompareFragProgId);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);

    glBeginOcclusionQueryLW(m_GpuCompareOcclusionQueryId);

    // Draw one textured quad that covers the screen.
    // This copies the offscreen draw surface to the user surface.
    glBegin(GL_TRIANGLES);

    glMultiTexCoord2f(GL_TEXTURE2, 0.0, 2.0); // upper left + 1 screen height
    glVertex2f(-1.0, 3.0);

    glMultiTexCoord2f(GL_TEXTURE2, 0.0, 0.0); // lower left
    glVertex2f(-1.0, -1.0);

    glMultiTexCoord2f(GL_TEXTURE2, 2.0, 0.0);  // lower right + 1 screen width
    glVertex2f( 3.0,  -1.0);

    glEnd();

    glEndOcclusionQueryLW();
    glFlush();
    glGetOcclusionQueryuivLW(m_GpuCompareOcclusionQueryId, GL_PIXEL_COUNT_LW, &badPixelCount);
    //Normally m_ForceErrorCount will be zero unless we are confirming the error paths of the
    //software.
    badPixelCount += m_ForceErrorCount;

    glDisable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE3);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    glDisable(GL_FRAGMENT_PROGRAM_LW);

    glActiveTexture(GL_TEXTURE2);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (!badPixelCount || !m_pGolden->GetStopOnError())
    {
        // Draw dots on the screen to indicate loop count.
        glPointSize(2.0);
        glBegin(GL_POINTS);
        for (UINT32 b = 0; (UINT64(1)<<b) <= m_FrameCount; b++)
        {
            if (m_FrameCount & (UINT64(1)<<b))
                glColor4ub(0xff, 0xff, 0xff, 0xff);
            else
                glColor4ub(0x55, 0x55, 0x55, 0x55);

            glVertex2f(GLfloat(b*0.01), 0.0);
        }
        glEnd();
    }
    RestoreDrawState();

    if (m_SkipDrawChecks)
    {
        return 0;
    }

    return badPixelCount;
}

//------------------------------------------------------------------------------
RC GLStressTest::CopyDrawFboToFront ()
{
    if (m_DoGpuCompare)
    {
        SuspendDrawState();

        // Bind the "draw" color buffer as texture 2.
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, m_FboTexIds[FBO_TEX_DRAW_COLOR]);
        glEnable(GL_TEXTURE_2D);

        glEnable(GL_FRAGMENT_PROGRAM_LW);
        glBindProgramLW(GL_FRAGMENT_PROGRAM_LW, m_GpuCopyFragProgId);

        // Draw one textured quad that covers the screen.
        // This copies the offscreen draw surface to the user surface.
        glBegin(GL_TRIANGLES);

        glMultiTexCoord2f(GL_TEXTURE2, 0.0, 2.0); // upper left + 1 screen height
        glVertex2f(-1.0, 3.0);

        glMultiTexCoord2f(GL_TEXTURE2, 0.0, 0.0); // lower left
        glVertex2f(-1.0, -1.0);

        glMultiTexCoord2f(GL_TEXTURE2, 2.0, 0.0);  // lower right + 1 screen width
        glVertex2f( 3.0,  -1.0);

        glEnd();
        glFlush();

        glDisable(GL_FRAGMENT_PROGRAM_LW);

        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);

        RestoreDrawState();

        return mglGetRC();
    }
    return OK;
}
//------------------------------------------------------------------------------
void GLStressTest::QuadStrip()
{
    const GLuint vtxPerStrip = m_Cols*2 + 2;
    const GLuint startIdx = m_Plane * m_IdxPerPlane +
            (m_StripCount % m_Rows) * vtxPerStrip;

    glDrawElements(
        GL_QUAD_STRIP,
        vtxPerStrip,
        GL_UNSIGNED_INT,
        NullPtr + startIdx * sizeof(GLuint));

    m_StripCount++;

    if (0 == m_StripCount % m_Rows)
        m_FrameCount++;

}

//------------------------------------------------------------------------------
void GLStressTest::Patch (GLuint rows)
{
    // With tessellation, m_Cols is forced to 1 since all horizontal
    // subdivision is done by the GPU (based on m_XTessRate).
    // But we still have an explicit vertex for each row boundary, in order
    // to support partial-frame drawing for "pulsing" mode.
    //
    // I.e. the vertex array is m_Rows high by 2 vertices wide.
    MASSERT(m_Cols == 1);

    // Vertex indices
    GLuint LLeft, LRight, URight, ULeft;

    if (0 == m_Plane)
    {
        // Near plane vertices are stored right-to-left, top-to-bottom.
        // v[0] is at (1024,768), v[1] is (0,768), etc.

        URight = (m_StripCount % m_Rows) * 2;
        LRight = URight + (2*rows);
        ULeft  = URight + 1;
        LLeft  = LRight + 1;
    }
    else
    {
        // Far plane vertices are stored left-to-right, bottom-to-top.
        // v[0] is at (0,0), v[1] is (1024,0), etc.

        LLeft  = m_VxPerPlane + (m_StripCount % m_Rows) * 2;
        ULeft  = LLeft + (2*rows);
        URight = ULeft + 1;
        LRight = LLeft + 1;
    }

    glProgramParameter4fLW (
        GL_TESS_CONTROL_PROGRAM_LW,
        22,
        (GLfloat)m_XTessRate, (GLfloat)rows, 0.0, 0.0);

    // Send vertices counter-clockwise from lower-left.
    glBegin(GL_PATCHES);
    glArrayElement(LLeft);
    glArrayElement(LRight);
    glArrayElement(URight);
    glArrayElement(ULeft);
    glEnd();
    m_StripCount += rows;

    if (0 == m_StripCount % m_Rows)
        m_FrameCount++;
}

//------------------------------------------------------------------------------
void GLStressTest::Triangles(GLuint rows)
{
    const GLuint idxPerStrip = 2*m_Cols*3;
    const GLuint startIdx = (m_StripCount % m_Rows) * idxPerStrip;

    glDrawElements(
        GL_TRIANGLES,
        idxPerStrip * rows,
        GL_UNSIGNED_INT,
        NullPtr + startIdx * sizeof(GLuint));

    m_StripCount += rows;

    if (0 == m_StripCount % m_Rows)
        m_FrameCount++;
}

//------------------------------------------------------------------------------
void GLStressTest::CommonBeginFrame()
{
    MASSERT(0 == m_StripCount % m_Rows);

    if (m_FrameCount < static_cast<UINT64>(m_DumpFrames))
    {
        char fname[80];
        snprintf(fname, sizeof(fname), "gls%lld_z.png", m_FrameCount);
        DumpImage(true, fname);

        snprintf(fname, sizeof(fname), "gls%lld_c.png", m_FrameCount);
        DumpImage(false, fname);
    }

    if (0 == m_FrameCount % 4)
    {
        if (m_NumTextures)
        {
            UINT32 txIdx = static_cast<UINT32>((m_FrameCount / 4) % m_NumTextures);

            GLenum txDim = GL_TEXTURE_2D;
            if (m_Use3dTextures)
                txDim    = GL_TEXTURE_3D;

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(txDim, m_TextureObjectIds[txIdx]);
            Printf(Tee::PriDebug, "%s::Draw, tx=%d\n", GetName().c_str(), txIdx);
        }
    }
}

//------------------------------------------------------------------------------
void GLStressTest::BeginFrame()
{
    CommonBeginFrame();

    switch (m_FrameCount % 4)
    {
        case 0:
        {
            // Initial depth is 0.0 for first loop, near for subsequent loops.
            // Initial stencil is 0 for all loops.
            // Draw at near view plane, touches all pixels.
            // Replace Z with near, stencil with 1.

            if (m_Ztest)
            {
                glDepthFunc(GL_GEQUAL);
            }
            if (m_Stencil)
            {
                glStencilFunc(GL_EQUAL, 0, 0xff);
                glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
            }
            if (m_NumTextures)
            {
                glDisable(GL_TEXTURE_GEN_Q);
            }
            m_Plane = 0; // near
            break;
        }
        case 1:
        {
            // Second draw:
            // Initial Z is near.
            // Initial S is 1.
            // Draw far plane, not all pixels are touched.
            // Replace some Z with far, some stencil with 2.
            if (m_Stencil)
            {
                glStencilFunc(GL_EQUAL, 1, 0xff);
            }
            if (m_NumTextures && m_TxGenQ)
            {
                glEnable(GL_TEXTURE_GEN_Q);
            }
            m_Plane = 1; // far
            break;
        }
        case 2:
        {
            // Third draw:
            // Initial Z is mixed near/far.
            // Initial S is mixed 1/2.
            // Draw far plane, touch only same the pixels as 2nd draw.
            // Replace some Z with far, some stencil with 3.
            if (m_Ztest)
            {
                glDepthFunc(GL_EQUAL);
            }
            if (m_Stencil)
            {
                glStencilFunc(GL_EQUAL, 2, 0xff);
            }
            m_Plane = 1; // far
            break;
        }
        case 3:
        {
            // Fourth draw:
            // Initial Z is mixed near/far.
            // Initial S is mixed 1/3.
            // Draw near plane, touches all pixels.
            // Replace all Z with near, all stencil with 0.
            if (m_Ztest)
            {
                glDepthFunc(GL_LEQUAL);
            }
            if (m_Stencil)
            {
                glStencilFunc(GL_EQUAL, 1, 0xfd);
                glStencilOp(GL_INCR, GL_INCR, GL_ZERO);
            }
            if (m_NumTextures && m_TxGenQ)
            {
                glDisable(GL_TEXTURE_GEN_Q);
            }
            m_Plane = 0; // near
            break;
        }
    }
}

//------------------------------------------------------------------------------
RC GLStressTest::EndFourthFrame()
{
    if (m_DoGpuCompare && ((m_FrameCount % m_FramesPerCheck) < 4))
    {
        glFlush();
        // Note: CopyBadPixelsToFront counts rendering errors, and also
        // reconfigures graphics to use current JS settings.
        GLuint errs = CopyBadPixelsToFront();
        if (errs > m_AllowedMiscompares)
        {
            AddJsonFailLoop();

            if (m_pGolden->GetStopOnError() || m_DumpPngOnError)
            {
                // Quick GPU-shader check reports errors, do the slow readback.
                CheckBuffer();
                if (m_pGolden->GetStopOnError())
                {
                    return RC::GPU_STRESS_TEST_FAILED;
                }
            }
            else
            {
                for (GLuint i = 0; i < errs; i++)
                {
                    // Don't stop to get error locations, just force the error
                    // count into the MemError object so we fail later.
                    GetMemError(0).LogMysteryError();
                }
                // Clear aclwmulated errors to avoid double-counting them.
                Clear();
            }
        }
    }
    else
    {
        SetupScissor(m_DrawPct);
    }
    if (m_DoubleBuffer)
        m_mglTestContext.SwapBuffers();

    // Don't let m_StripCount grow without bounds, or it will overflow
    // during long runs on fast GPUs.
    m_StripCount = 0;

    return mglGetRC();
}

void GLStressTest::UpdateTileMask()
{
    if (m_PulseMode != PULSE_TILE_MASK_GPC_SHUFFLE)
        return;

    UINT32 tileMask = 0;
    float enabledGpcs[8*sizeof(tileMask)] = { };
    while (static_cast<UINT32>(Utility::CountBits(tileMask)) < m_NumActiveGPCs)
    {
        const UINT32 gpcIdx = GetFpContext()->Rand.GetRandom(0, m_HighestGPCIdx);
        tileMask |= 1 << gpcIdx;
        enabledGpcs[gpcIdx] = 1.0f;
    }

    Printf(Tee::PriDebug, "GPC shuffle tileMask = 0x%08x\n", tileMask);
    m_GPCShuffleStats.AddPoint(tileMask);

    glBindBuffer(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, m_ConstIds[CONST_ENABLEDGPCS]);
    glBufferData(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, sizeof(enabledGpcs),
        enabledGpcs, GL_STATIC_DRAW);
    glBindBuffer(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, 0);
}

//------------------------------------------------------------------------------
// Draw a bunch of frames broken down into numStrips.
RC GLStressTest::Draw (GLuint numStrips, mglTimerQuery * pTimer)
{
    StickyRC firstRc;

    if (pTimer)
        pTimer->Begin();

    UpdateTileMask();

    if (m_Torus)
    {
        Tasker::DetachThread detach;
        while (numStrips && OK == firstRc)
        {
            const GLuint rowsLeftInFrame = m_Rows - (m_StripCount % m_Rows);

            if (m_Rows == rowsLeftInFrame)
            {
                CommonBeginFrame();
            }

            const GLuint strips = min(rowsLeftInFrame, numStrips);

            Triangles(strips);
            numStrips -= strips;

            if (0 == m_StripCount % (m_Rows * 4))
            {
                // Portions of EndFourthFrame are not thread safe, reattach
                Tasker::AttachThread attach;
                firstRc = EndFourthFrame();
            }
            Tasker::Yield();
        }
    }
    else
    {
        Tasker::DetachThread detach;
        while (numStrips && OK == firstRc)
        {
            const GLuint rowsLeftInFrame = m_Rows - (m_StripCount % m_Rows);

            if (m_Rows == rowsLeftInFrame)
            {
                BeginFrame();
            }

            const GLuint strips = min(rowsLeftInFrame, numStrips);

            if (m_GpuProgs[TessCtrl].IsLoaded())
            {
                // Send just the 4 corner vertices, GPU creates the rest.
                Patch (strips);
            }
            else
            {
                // Explicitly send all vertices for 1 frame.
                for (GLuint i = 0; i < strips; i++)
                    QuadStrip();
            }
            numStrips -= strips;
            if (0 == m_StripCount % (m_Rows * 4))
            {
                // Portions of EndFourthFrame are not thread safe, reattach
                Tasker::AttachThread attach;
                firstRc = EndFourthFrame();
            }
            Tasker::Yield();
        }
    }

    if (pTimer)
        pTimer->End();

    if (OK == firstRc)
        firstRc = WaitForPowerGate();

    firstRc = mglGetChannelError();

    if (mglSoftware == dglGetDispatchMode())
    {
        Printf(Tee::PriError, "Software rendering path used!\n");
        firstRc = RC::HW_ERROR;
    }

    return firstRc;
}

//------------------------------------------------------------------------------
RC GLStressTest::WaitForPowerGate()
{
    RC rc;
    StickyRC firstRc;

    if (!m_EnablePowerWait && !m_EnableDeepIdle)
        return OK;

    glFlush();

    if (m_FrameCount == static_cast<UINT64>(m_PowerWaitTrigger))
    {
        unique_ptr<PwrWait> pwrWait;

        if (m_EnableDeepIdle)
        {
            UINT32 DIPState;
            CHECK_RC(GetDeepIdlePState(&DIPState));
            pwrWait.reset(new DeepIdleWait(GetBoundGpuSubdevice(),
                                           m_mglTestContext.GetDisplay(),
                                           m_mglTestContext.GetPrimarySurface(),
                                           m_PowerWaitTimeoutMs,
                                           m_DeepIdleTimeoutMs,
                                           m_ShowDeepIdleStats,
                                           DIPState));
        }
        else if (m_TegraPowerWait)
        {
            const INT32 pri = m_PrintPowerEvents ? GetVerbosePrintPri() : Tee::PriLow;
            pwrWait.reset(new TegraElpgWait(GetBoundGpuSubdevice(), pri));
        }
        else
        {
            pwrWait.reset(new ElpgWait(GetBoundGpuSubdevice(),
                                       PMU::ELPG_GRAPHICS_ENGINE,
                                       m_PrintPowerEvents));
        }

        Tasker::Yield();

        CHECK_RC(pwrWait->Initialize());
        firstRc = pwrWait->Wait(PwrWait::PG_ON, m_PowerWaitTimeoutMs);
        if (firstRc != OK)
        {
            Printf(Tee::PriNormal,
                   "%s : Power gating failed at frame count %lld\n",
                   GetName().c_str(), m_FrameCount);
        }
        firstRc = pwrWait->Teardown();

        if (m_PowerWaitDelayMs > 0)
        {
            Tasker::Sleep(m_PowerWaitDelayMs);
        }
        m_PowerWaitTrigger += ((*m_pPickers)[FPK_GLSTRESSTEST_POWER_WAIT].Pick() * 4);
        m_PowerWaitDelayMs = (*m_pPickers)[FPK_GLSTRESSTEST_DELAYMS_AFTER_PG].Pick();
        m_pFpCtx->LoopNum++;
    }

    return firstRc;
}

//------------------------------------------------------------------------------
RC GLStressTest::GetDeepIdlePState(UINT32 *pPStateNum)
{
    MASSERT(pPStateNum);
    *pPStateNum = (*m_pPickers)[FPK_GLSTRESSTEST_DEEPIDLE_PSTATE].Pick();
    Printf(Tee::PriLow, "Selected DI pstate: %u\n", *pPStateNum);
    return OK;
}

//------------------------------------------------------------------------------
// Generate a timestamped filename and take into account the logging path just
// in case its not the current working dir.
string GLStressTest::GetTimestampedPngFilename(UINT64 errCnt)
{
    string filename = LwDiagUtils::StripFilename(CommandLine::LogFileName().c_str());
    string::size_type pos = filename.find_last_of("/\\");
    if ((pos != string::npos) && (pos != (filename.size()-1)))
    {   // append the proper type of separator
        filename.push_back(filename[pos]);
        filename += Utility::GetTimestampedFilename(
            Utility::StrPrintf("glstress_%lld", errCnt).c_str(), "png");
    }
    else
    {
        filename = Utility::GetTimestampedFilename(
            Utility::StrPrintf("glstress_%lld", errCnt).c_str(), "png");
    }
    return filename;
}

//------------------------------------------------------------------------------
RC GLStressTest::CheckBuffer ()
{
    StickyRC rc = OK;
    Printf(Tee::PriLow, "Checking buffer after frame:%lld\n", m_FrameCount);
    // We use this to skip the error checking.
    // Lwrrently it is used with HeatStressTest.
    if ((m_pGolden->GetAction() != Goldelwalues::Check) ||
        m_SkipDrawChecks)
    {
       Tasker::Yield();
       return OK;
    }

    for (UINT32 i = 0; i < m_ForceErrorCount; i++)
    {
        // Insert fake errors to test error-handling code.
        GetMemError(0).LogMysteryError();
    }

    glFinish();
    INT64 start = Xp::QueryPerformanceCounter();

    // On Android we can't read the surface directly, because we can't
    // force OpenGL to flush the GPU cache and it does not flush it.
    const bool noCoherence = GetBoundGpuSubdevice()->IsSOC() && !Platform::HasClientSideResman();

    if (!m_mglTestContext.UsingFbo() && !m_DoGpuCompare && m_pRawPrimarySurface && !noCoherence)
    {
        // We know the actual location of the surface.
        // We will report real FB memory locations and ram details.

        GpuDevice * pGpuDev = GetBoundGpuDevice();
        UINT32 sd;
        for (sd = 0; sd < pGpuDev->GetNumSubdevices(); sd++)
        {
            if (0 != m_pRawPrimarySurface->GetAddress())
                m_pRawPrimarySurface->Unmap();

            CHECK_RC(m_pRawPrimarySurface->Map(sd));
            const char * pSurfAddr = (const char *) m_pRawPrimarySurface->GetAddress();
            MASSERT(pSurfAddr);

            UINT32 wb = m_Width * m_Xbloat;
            UINT32 hb = m_Height * m_Ybloat;
            UINT32 bpp = m_pRawPrimarySurface->GetBytesPerPixel();
            UINT32 x, y;

            // A buffer big enough to hold the fattest float-128 pixel.
            UINT08 pixelBuf[16];
            MASSERT(sizeof(pixelBuf) >= bpp);

            // A buffer holding one pixel of the clear-color colwerted to the
            // actual surface format.
            UINT08 clearBuf[sizeof(pixelBuf)];
            ColorUtils::Colwert(
                    (const char *)&m_ClearColor,
                    ColorUtils::A8R8G8B8,
                    (char *)clearBuf,
                    ColorUtils::ColorDepthToFormat(m_Depth),
                    1);

            for (y = 0; y < hb; y++)
            {
                for (x = 0; x < wb; x++)
                {
                    UINT64 pixelOffset = m_pRawPrimarySurface->GetPixelOffset(x, y);
                    Platform::MemCopy (pixelBuf, pSurfAddr + pixelOffset, bpp);

                    // Conform to the hardware polling requirement requested by
                    // "-poll_hw_hz" (i.e. HW should not be accessed faster than a
                    // certain rate).  By default this will not sleep or yield unless
                    // the command line argument is present.
                    Tasker::PollMemSleep();

                    UINT32 errorByteMask = 0;
                    UINT32 b;

                    for (b = 0; b < bpp; b++)
                    {
                        if (clearBuf[b] != pixelBuf[b])
                            errorByteMask |= (1<<b);
                    }

                    if (errorByteMask)
                    {
                        if (!m_ReportMemLocs || m_MemLocsAreMisleading)
                        {
                            GetMemError(sd).LogMysteryError();
                        }
                        else
                        {
                            switch (bpp)
                            {
                                case 2:
                                    GetMemError(sd).LogUnknownMemoryError(
                                        16,
                                        m_pRawPrimarySurface->GetVidMemOffset() + pixelOffset,
                                        *(UINT16*)pixelBuf,
                                        *(UINT16*)clearBuf,
                                        m_pRawPrimarySurface->GetPteKind(),
                                        m_pRawPrimarySurface->GetActualPageSizeKB());
                                    break;

                                case 4:
                                    GetMemError(sd).LogUnknownMemoryError(
                                        32,
                                        m_pRawPrimarySurface->GetVidMemOffset() + pixelOffset,
                                        *(UINT32*)pixelBuf,
                                        *(UINT32*)clearBuf,
                                        m_pRawPrimarySurface->GetPteKind(),
                                        m_pRawPrimarySurface->GetActualPageSizeKB());
                                    break;

                                case 8:
                                {
                                    UINT32 w;
                                    for (w = 0; w < 4; w++)
                                    {
                                        if (errorByteMask & (3 << (2*w)))
                                        {
                                            GetMemError(sd).LogUnknownMemoryError(
                                                16,
                                                m_pRawPrimarySurface->GetVidMemOffset() + pixelOffset + 2*w,
                                                ((UINT16*)pixelBuf)[w],
                                                ((UINT16*)clearBuf)[w],
                                                m_pRawPrimarySurface->GetPteKind(),
                                                m_pRawPrimarySurface->GetActualPageSizeKB());
                                        }
                                    }
                                    break;
                                }
                                case 16:
                                {
                                    UINT32 w;
                                    for (w = 0; w < 4; w++)
                                    {
                                        if (errorByteMask & (0xf << (4*w)))
                                        {
                                            GetMemError(sd).LogUnknownMemoryError(
                                                32,
                                                m_pRawPrimarySurface->GetVidMemOffset() + pixelOffset + 4*w,
                                                ((UINT32*)pixelBuf)[w],
                                                ((UINT32*)clearBuf)[w],
                                                m_pRawPrimarySurface->GetPteKind(),
                                                m_pRawPrimarySurface->GetActualPageSizeKB());
                                        }
                                    }
                                    break;
                               }
                               default:
                                    MASSERT(!"GLStress: surface unexpected bpp");
                            } // switch bpp
                        } // if ReportMemLocs
                    } // if error
                } // x
            } // y


            if (m_DumpPngOnError && GetMemError(sd).GetUnknownErrorCount())
            {
                UINT64 errCnt = GetMemError(sd).GetUnknownErrorCount();
                CHECK_RC(m_pRawPrimarySurface->WritePng(
                    m_GenUniqueFilenames ?
                        GetTimestampedPngFilename(errCnt).c_str() :
                        Utility::StrPrintf("glstress_%lld.png", errCnt).c_str()));
            }
            m_pRawPrimarySurface->Unmap();
        } // subdevice
    }
    else
    {
        // @@@ How does SLI stuff work here?
        const mglTexFmtInfo * pFmtInfo = mglGetTexFmtInfo(mglTexFmt2Color(m_ActualColorSurfaceFormat));
        const UINT32 bpp = ColorUtils::PixelBytes(pFmtInfo->ExtColorFormat);

        // We *don't* know the actual location of the surface.
        glReadPixels(
            0,
            0,
            m_Width,
            m_Height,
            pFmtInfo->ExtFormat,
            pFmtInfo->Type,
            &m_ScreenBuf[0]
        );

        GLubyte * p = &m_ScreenBuf[0];
        GLubyte * pEnd = p + m_Width * m_Height * bpp;

        for (/**/; p < pEnd; p += bpp)
        {
            for (UINT32 i = 0; i < bpp; i++)
                if (p[i] != m_ClearColorPixel[i])
                {
                    GetMemError(0).LogMysteryError();
                    break;
                }
        }

        if (m_DumpPngOnError && GetMemError(0).GetUnknownErrorCount())
        {
            //Debug: Timestamp the png file to account for -loops x on the command line.
            ImageFile::WritePng(
                m_GenUniqueFilenames ?
                    GetTimestampedPngFilename(GetMemError(0).GetUnknownErrorCount()).c_str() :
                    "glstress.png",
                pFmtInfo->ExtColorFormat,
                &m_ScreenBuf[0],
                m_Width,
                m_Height,
                m_Width * bpp,
                false,
                true);
        }
    } // ReadPixels error checking
    m_ElapsedGpuIdle += (Xp::QueryPerformanceCounter()-start)/m_TicksPerSec;

    if (GetMemError(0).GetErrorCount())
    {
        const bool tooManyErrors = (GetMemError(0).GetErrorCount() > m_AllowedMiscompares);
        const Tee::Priority pri = tooManyErrors ? Tee::PriError : Tee::PriWarn;
        Printf(pri, "UnknownErrorCount=%lld m_FrameCount=%lld\n",
               GetMemError(0).GetErrorCount(),
               m_FrameCount);
        if (tooManyErrors)
        {
            rc = RC::BAD_MEMORY;
        }
    }
    return (rc);
}

//------------------------------------------------------------------------------
void GLStressTest::DumpImage
(
    bool readZS,
    const char * name
)
{
    const GLenum cFmt  = readZS ? GL_DEPTH_STENCIL_LW : GL_RGBA8;
    const mglTexFmtInfo * pFmtInfo = mglGetTexFmtInfo(mglTexFmt2Color(cFmt));
    const UINT32 bpp = ColorUtils::PixelBytes(pFmtInfo->ExtColorFormat);
    const UINT32 size = m_Width * m_Height * bpp;

    if ( size > m_ScreenBuf.size())
    {
        m_ScreenBuf.resize(size);
    }

    if (readZS)
    {
        const GLenum fmt =
            (m_ZDepth == 32) && mglExtensionSupported("GL_EXT_packed_depth_stencil")
            ? GL_DEPTH_STENCIL_LW : GL_DEPTH_COMPONENT;
        const GLenum type= m_ZDepth != 32 ?
            GL_UNSIGNED_INT : GL_UNSIGNED_INT_24_8_LW;
        glReadPixels(
            0,
            0,
            m_Width,
            m_Height,
            fmt,
            type,
            &m_ScreenBuf[0]);
    }
    else
    {
        glReadPixels(
            0,
            0,
            m_Width,
            m_Height,
            GL_BGRA,
            GL_UNSIGNED_BYTE,
            &m_ScreenBuf[0]);
    }
    ImageFile::WritePng(
        name,
        ColorUtils::ColorDepthToFormat(32),
        &m_ScreenBuf[0],
        m_Width,
        m_Height,
        m_Width * 4,
        false,
        false);

    Printf(Tee::PriNormal, "Wrote image file %s.\n", name);
}

//------------------------------------------------------------------------------
RC GLStressTest::SetDefaultsForPicker(UINT32 Idx)
{
    FancyPickerArray * pPickers = GetFancyPickerArray();
    FancyPicker &Fp = (*pPickers)[Idx];
    switch (Idx)
    {
        case FPK_GLSTRESSTEST_POWER_WAIT:
            Fp.ConfigConst(0);
            break;
        case FPK_GLSTRESSTEST_DELAYMS_AFTER_PG:
            Fp.ConfigConst(0);
            break;
        case FPK_GLSTRESSTEST_DEEPIDLE_PSTATE:
            Fp.ConfigConst(12);
            break;
        default:
            return RC::SOFTWARE_ERROR;
    }
    return OK;
}

//------------------------------------------------------------------------------
RC GLStressTest::SetProgParams
(
    const string& pgmType,
    UINT32 paramIdx,
    const JsArray& vals
)
{
    int progIdx = 0;
    for (/**/; progIdx < ProgType_NUM; progIdx++)
    {
        if (0 == stricmp(pgmType.c_str(), m_GpuProgs[progIdx].m_Name))
            break;
    }
    if (progIdx >= ProgType_NUM)
    {
        Printf(Tee::PriNormal, "%s prog type must be one of ", __FUNCTION__);
        for (progIdx = 0; progIdx < ProgType_NUM; progIdx++)
        {
            Printf(Tee::PriNormal, "%s\"%s\" ",
                    progIdx ? "," : "",
                    m_GpuProgs[progIdx].m_Name);
        }
        Printf(Tee::PriNormal, "\n");
        return RC::ILWALID_OBJECT_PROPERTY;
    }

    RC rc;
    JavaScriptPtr pJs;

    for (size_t vi = 0; vi < vals.size(); vi += 4)
    {
        ProgProp prop;

        for (size_t fi = 0; fi < 4 && vi+fi < vals.size(); fi++)
        {
            double tmp;
            CHECK_RC(pJs->FromJsval(vals[vi+fi], &tmp));
            prop.value[fi] = static_cast<GLfloat>(tmp);
        }
        m_GpuProgs[progIdx].m_UserProps[paramIdx + UINT32(vi/4)] = prop;
    }
    return OK;
}

//------------------------------------------------------------------------------
// Hook our test and its properties into JavaScript:

JS_CLASS_INHERIT_FULL(GLStressTest, GpuMemTest,
        "3d stress test",
        0);

CLASS_PROP_READWRITE(GLStressTest, RuntimeMs, UINT32,
        "How long to run GLStress, in milliseconds.");
CLASS_PROP_READWRITE(GLStressTest, BufferCheckMs, UINT32,
        "How long to run GLStress(in milliseconds) before checking the output buffer.");
CLASS_PROP_READWRITE(GLStressTest, LoopMs, UINT32,
        "How long to run the GPUs, in milliseconds.");
CLASS_PROP_READWRITE(GLStressTest, FullScreen, UINT32,
        "Full screen mode (otherwise, window)");
CLASS_PROP_READWRITE(GLStressTest, DoubleBuffer, bool,
        "Alternate front/back rendering (else front only)");
CLASS_PROP_READWRITE_LWSTOM(GLStressTest, PatternSet,
        "How many bad pixels to ignore, and still return OK.");

P_(GLStressTest_Set_PatternSet)
{
    MASSERT(pValue   != 0);

    RC rc = RC::SOFTWARE_ERROR;
    JavaScriptPtr pJs;
    JsArray jsa;
    GLStressTest * pTest = JS_GET_PRIVATE(GLStressTest, pContext,
                                          pObject, "GLStressTest");
    if (pTest != nullptr)
    {
        rc = pJs->FromJsval(*pValue, &jsa);
        if (OK == rc)
        {
            // Arg is an array of values, treat them as a list of pattern
            // indices to use.
            vector<UINT32> SelectedPatterns;
            UINT32 tmpI = 0;
            for (UINT32 i = 0; i < jsa.size(); i++)
            {
                pJs->FromJsval(jsa[i], &tmpI);
                SelectedPatterns.push_back( tmpI );
            }
            pTest->SelectPatterns( &SelectedPatterns );
        }
        else
        {
            // Arg is a scalar, assume it is the enum-value of a predefined set.
            UINT32 pttrnSelection;
            rc = pJs->FromJsval(*pValue, &pttrnSelection);

            pTest->SelectPatternSet(
                    (PatternClass::PatternSets)pttrnSelection);
        }
    }
    if (rc)
    {
        JS_ReportError(pContext, "Failed to set GLStress.PatternSet");
        return JS_FALSE;
    }
    return JS_TRUE;
}
P_(GLStressTest_Get_PatternSet)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    RC rc = RC::SOFTWARE_ERROR;
    JavaScriptPtr pJs;
    JsArray jsa;
    vector<UINT32> Patterns;
    GLStressTest * pTest = JS_GET_PRIVATE(GLStressTest, pContext,
                                          pObject, "GLStressTest");
    if (pTest != nullptr)
    {
        pTest->GetSelectedPatterns(&Patterns);

        for (size_t i = 0; i < Patterns.size(); i++)
        {
            jsval jsv;
            pJs->ToJsval(Patterns[i], &jsv);
            jsa.push_back(jsv);
        }
        rc = pJs->ToJsval(&jsa, pValue);
    }
    if (OK != rc)
    {
        JS_ReportError(pContext, "Failed to get GLStress.PatternSet");
        return JS_FALSE;
    }
    return JS_TRUE;
}

CLASS_PROP_READWRITE(GLStressTest, BeQuiet, bool,
        "While this is true, Run will skip any error checking.");
CLASS_PROP_READWRITE(GLStressTest, KeepRunning, bool,
        "While this is true, Run will continue even beyond LoopMs.");
CLASS_PROP_READWRITE(GLStressTest, ColorSurfaceFormat, UINT32,
        "if !0, use this surface format regardless of TestConfiguration.DisplayDepth.");
CLASS_PROP_READWRITE(GLStressTest, TxSize, UINT32,
        "Texture width & height in texels.");
CLASS_PROP_READWRITE(GLStressTest, NumTextures, UINT32,
        "How many different patterns (textures) to try (0 disables texturing).");
CLASS_PROP_READWRITE(GLStressTest, TxD, double,
        "log2(texels per pixel), affects tex coord setup.");
CLASS_PROP_READWRITE(GLStressTest, PpV, double,
        "Pixels per vertex (controls primitive size).");
CLASS_PROP_READWRITE(GLStressTest, Rotate, bool,
        "Rotate geometry 180deg around z axis.");
CLASS_PROP_READWRITE(GLStressTest, UseMM, bool,
        "Use LINEAR_MIPMAP_LINEAR instead of NEAREST texture filtering.");
CLASS_PROP_READWRITE(GLStressTest, TxGenQ, bool,
        "Enable txgen for the Q coordinate.");
CLASS_PROP_READWRITE(GLStressTest, Use3dTextures, bool,
        "Use 3d textures (else 2d).");
CLASS_PROP_READWRITE(GLStressTest, NumLights, UINT32,
        "Number of point-source lights to enable (0 == disable lighting).");
CLASS_PROP_READWRITE(GLStressTest, SpotLights, bool,
        "Make lights spotlights (else point-source lights).");
CLASS_PROP_READWRITE(GLStressTest, Normals, bool,
        "Send normals per-vertex.");
CLASS_PROP_READWRITE(GLStressTest, BigVertex, bool,
        "Use the 8-float \"big\" vertex format (pos[3],nrm[3],txc0[2]).");
CLASS_PROP_READWRITE(GLStressTest, Stencil, bool,
        "Enable stencil.");
CLASS_PROP_READWRITE(GLStressTest, Ztest, bool,
        "Enable depth test.");
CLASS_PROP_READWRITE(GLStressTest, TwoTex, bool,
        "Use dual-texturing (tx1 always tx pattern 0).");
CLASS_PROP_READWRITE(GLStressTest, Fog, bool,
        "Enable fog.");
CLASS_PROP_READWRITE(GLStressTest, LinearFog, bool,
        "Use linear fog (else exponential).");
CLASS_PROP_READWRITE(GLStressTest, BoringXform, bool,
        "Use a boring xform (x,y passthru, z=0).");
CLASS_PROP_READWRITE(GLStressTest, DrawLines, bool,
        "Draw GL_LINEs.");
CLASS_PROP_READWRITE(GLStressTest, DumpPngOnError, bool,
        "Dump a .png file of the failing surface.");
CLASS_PROP_READWRITE(GLStressTest, SendW, bool,
        "Send x,y,z,w positions (not just x,y,z).");
CLASS_PROP_READWRITE(GLStressTest, DoGpuCompare, bool,
        "Use the GPU shader to check for errors.");
CLASS_PROP_READWRITE(GLStressTest, AlphaTest, bool,
        "Enable alpha test.");
CLASS_PROP_READWRITE(GLStressTest, OffsetSecondTexture, bool,
        "if !0, offset the second texture's coordinates.");
CLASS_PROP_READWRITE(GLStressTest, ViewportXOffset, UINT32,
        "Offset viewport horizontally by this many pixels (or reduce width if <0).");
CLASS_PROP_READWRITE(GLStressTest, ViewportYOffset, UINT32,
        "Offset viewport vertically by this many pixels (or reduce height if <0).");
CLASS_PROP_READWRITE(GLStressTest, FragProg, string,
        "Fragment program (if empty, use traditional GL fragment processing).");
CLASS_PROP_READWRITE(GLStressTest, VtxProg, string,
        "Vertex program (if empty, use traditional GL vertex processing).");
CLASS_PROP_READWRITE(GLStressTest, TessCtrlProg, string,
        "Tessellation control program to use if UseTessellation is true.");
CLASS_PROP_READWRITE(GLStressTest, TessEvalProg, string,
        "Tessellation evaluation program to use if UseTessellation is true.");
CLASS_PROP_READWRITE(GLStressTest, SmoothLines, bool,
        "Use smooth lines when DrawLines is enabled");
CLASS_PROP_READWRITE(GLStressTest, StippleLines, bool,
        "Use stippled lines when DrawLines is enabled");
CLASS_PROP_READWRITE(GLStressTest, PulseMode, UINT32,
        "Pulse Mode");
CLASS_PROP_READWRITE(GLStressTest, TileMask, UINT32,
        "User or GPC tile bitmask");
CLASS_PROP_READWRITE(GLStressTest, NumActiveGPCs, UINT32,
        "Number of active GPCs in GPC shuffle mode");
CLASS_PROP_READWRITE(GLStressTest, SkipDrawChecks, bool,
        "Do not report pixel errors");
CLASS_PROP_READWRITE_LWSTOM(GLStressTest, TxImages,
       "List of filenames to load as texture images (if empty, use wfmats textures).");

P_(GLStressTest_Set_TxImages)
{
    MASSERT(pValue   != 0);

    RC rc = RC::SOFTWARE_ERROR;
    JavaScriptPtr pJs;
    JsArray jsa;
    GLStressTest * pTest = JS_GET_PRIVATE(GLStressTest, pContext,
                                          pObject, "GLStressTest");
    if (pTest != nullptr)
    {
        rc = pJs->FromJsval(*pValue, &jsa);
        if (OK == rc)
        {
            size_t numNames = jsa.size();
            vector<string> fileNames(numNames);
            string tmps;
            UINT32 n;
            for (n = 0; n < numNames; n++)
            {
                if (OK != (rc = pJs->FromJsval(jsa[n], &tmps)))
                {
                    Printf(Tee::PriError, "TxImages[%d] is not a string.\n", n);
                    break;
                }
                fileNames.push_back(tmps);
            }
            pTest->SetTxImages(&fileNames);
        }
    }
    if (rc)
    {
        JS_ReportError(pContext, "Failed to set GLStress.TxImages");
        return JS_FALSE;
    }
    return JS_TRUE;
}
P_(GLStressTest_Get_TxImages)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    RC rc = RC::SOFTWARE_ERROR;
    JavaScriptPtr pJs;
    JsArray jsa;
    GLStressTest * pTest = JS_GET_PRIVATE(GLStressTest, pContext,
                                          pObject, "GLStressTest");
    if (pTest != nullptr)
    {
        vector<string> * pfileNames = pTest->GetTxImages();

        for (size_t i = 0; i < pfileNames->size(); i++)
        {
            jsval jsv;
            pJs->ToJsval((*pfileNames)[i], &jsv);
            jsa.push_back(jsv);
        }
        rc = pJs->ToJsval(&jsa, pValue);
    }
    if (OK != rc)
    {
        JS_ReportError(pContext, "Failed to get GLStress.TxImages");
        return JS_FALSE;
    }
    return JS_TRUE;
}

CLASS_PROP_READWRITE(GLStressTest, ClearColor, UINT32,
       "Clear color in UINT32 LE_A8R8G8B8 format.");
CLASS_PROP_READWRITE(GLStressTest, GpioToDriveInDelay, INT32,
       "If >=0, toggle this GPIO pin to provide a phase reference for PulseGLS.");
CLASS_PROP_READWRITE(GLStressTest, StartFreq, double,
       "If >0.0, use a pulsing workload at this frequency (Hz).");
CLASS_PROP_READWRITE(GLStressTest, DutyPct, UINT32,
       "If StartFreq or FreqSweepOctavesPerSecond, use this pct duty cycle in pulsed workload.");
CLASS_PROP_READWRITE(GLStressTest, FreqSweepOctavesPerSecond, double,
       "If > 0, use pulsing workload with frequency sweep.");
CLASS_PROP_READWRITE(GLStressTest, ForceErrorCount, UINT32,
       "Force this many errors, to test reporting.");

CLASS_PROP_READWRITE(GLStressTest, UseFbo, bool,
        "if !0, use frame buffer objects to draw offscreen");
CLASS_PROP_READWRITE(GLStressTest, GenUniqueFilenames, bool,
        "generate timestamped filenames for the *.png files");

CLASS_PROP_READWRITE(GLStressTest, EnablePowerWait, bool,
        "Enable power wait insertion.");
CLASS_PROP_READWRITE(GLStressTest, EnableDeepIdle, bool,
        "Enable deep idle on power waits.");
CLASS_PROP_READWRITE(GLStressTest, ToggleELPGOlwideo, bool,
        "Toggle ELPG on the video engine in the background (default = false).");
CLASS_PROP_READWRITE(GLStressTest, ToggleELPGGatedTimeMs, UINT32,
        "When ToggleELPGOnGraphics = true, the time in ms that Video will remain gated (default = 100).");
CLASS_PROP_READWRITE(GLStressTest, PowerWaitTimeoutMs, UINT32,
        "Set the timeout in ms for power wait transitions(default = 5000).");
CLASS_PROP_READWRITE(GLStressTest, DeepIdleTimeoutMs, UINT32,
        "Set the timeout in ms for deep idle transitions(default = 5000).");
CLASS_PROP_READWRITE(GLStressTest, ShowDeepIdleStats, bool,
        "Show deep idle statistics for each transition (default = false).");
CLASS_PROP_READWRITE(GLStressTest, PrintPowerEvents, bool,
        "When EnablePowerWait = true, print the number of power gate events (default = false).");
CLASS_PROP_READWRITE(GLStressTest, EndLoopPeriodMs, UINT32,
        "Time in milliseconds between calls to EndLoop (default = 10000).");
CLASS_PROP_READWRITE(GLStressTest, FramesPerFlush, UINT32,
        "Frames to draw before each glFlush (nonpulsing) (default = 100).");
CLASS_PROP_READWRITE(GLStressTest, UseTessellation, bool,
        "Use the GL_LW_tessellation_program5 extension (default = false).");
CLASS_PROP_READWRITE(GLStressTest, DumpFrames, UINT32,
        "Number of frames to dump images of (default 0).");
CLASS_PROP_READWRITE(GLStressTest, Blend, bool,
        "Use glBlend instead of glColorLogicOp(XOR) (default false).");
CLASS_PROP_READWRITE(GLStressTest, Torus, bool,
        "Draw a torus (doughnut) instead of two planes.(default false).");
CLASS_PROP_READWRITE(GLStressTest, TorusMajor, bool,
        "Draw a torus in major order (long strips around hole) else minor (short strips around body) (default true).");
CLASS_PROP_READWRITE(GLStressTest, LwllBack, bool,
        "Lwll back-facing primitives (default false).");
CLASS_PROP_READWRITE(GLStressTest, DrawPct, double,
        "Percent of screen to draw (default 100.0).");
CLASS_PROP_READWRITE(GLStressTest, Watts, UINT32,
        "Limit gpu power consumed to this (default 0 == max possible)");
CLASS_PROP_READWRITE_FULL(GLStressTest, AllowedMiscompares, UINT32,
        "Controls how many pixels are allowed to be incorrect",
        MODS_INTERNAL_ONLY | SPROP_VALID_TEST_ARG, 0);
CLASS_PROP_READONLY(GLStressTest, Frames, double,
       "Frames completed in last Run().");
CLASS_PROP_READONLY(GLStressTest, Pixels, double,
       "Pixels drawn in last Run().");
CLASS_PROP_READONLY(GLStressTest, ElapsedGpu, double,
       "Elapsed GPU runtime seconds during drawing in last Run().");
CLASS_PROP_READONLY(GLStressTest, ElapsedRun, double,
       "Elapsed system runtime seconds during drawing in last Run().");
CLASS_PROP_READONLY(GLStressTest, ElapsedGpuIdle, double,
       "Elapsed GPU idle time(in seconds) while drawing during last Run().");
CLASS_PROP_READONLY(GLStressTest, Fps, double,
       "Frames per second in last Run().");
CLASS_PROP_READONLY(GLStressTest, Pps, double,
       "Pixels per second in last Run().");

JS_STEST_LWSTOM(GLStressTest, SetProgParams, 3,
        "Set TessCtrl/TessEval/Vtx/Frag program constants.")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    GLStressTest *pTest = (GLStressTest *)JS_GetPrivate(pContext, pObject);
    JavaScriptPtr pJavaScript;
    string     pgmType;
    UINT32     idx;
    JsArray    vals;

    if ( (NumArguments != 3) ||
         (OK != pJavaScript->FromJsval(pArguments[0], &pgmType)) ||
         (OK != pJavaScript->FromJsval(pArguments[1], &idx)) ||
         (OK != pJavaScript->FromJsval(pArguments[2], &vals)) )
    {
        JS_ReportError(pContext,
                       "Usage: GLStressTest.SetProgParams(\"Vtx\", idx, [vals])\n");
        return JS_FALSE;
    }

    RETURN_RC(pTest->SetProgParams(pgmType, idx, vals));
}

// These names match similar callbacks in nwfmats and glcombust.
PROP_CONST(GLStressTest, PRE_FILL,      ModsTest::MISC_A);
PROP_CONST(GLStressTest, PRE_CHECK,     ModsTest::MISC_B);

//------------------------------------------------------------------------------
PulseController::PulseController
(
    UINT32 MaxPulseSize,
    double FreqSweepOctavesPerSecond
)
:   m_MaxPulseSize(MaxPulseSize),
    m_FreqSweepOctavesPerSecond(FreqSweepOctavesPerSecond),
    m_DutyPct(50),
    m_WorkFraction(0.5),
    m_SweepDir(1.0),
    m_LwrFreq(0.0),
    m_TargetPulseTime(0.0),
    m_TargetCycleTime(0.0),
    m_LwrPulseSize(MaxPulseSize),
    m_MaxFreq(-1.0),
    m_MinFreq(-1.0),
    m_PulseTimeSum(0.0),
    m_PulseCount(PulseController::PulseCountLimit)
{
    MASSERT(m_MaxPulseSize > 0);
}

PulseController::~PulseController()
{
}

void PulseController::SetTargets (UINT32 DutyPct, double Hz)
{
    if (0.0 == m_FreqSweepOctavesPerSecond && Hz != m_LwrFreq)
    {
        // Start statistics.
        m_PulseTimeSum = 0.0;
        m_PulseCount = 0;
    }

    m_DutyPct = DutyPct;
    m_WorkFraction = max(0.0, min(DutyPct/100.0, 1.0));
    m_LwrFreq = max(0.0, Hz);
    if (m_LwrFreq > 0.0)
    {
        m_TargetCycleTime = 1.0 / m_LwrFreq;
        m_TargetPulseTime = m_TargetCycleTime * m_WorkFraction;
    }
    else
    {
        m_TargetCycleTime = 0.0;
        m_TargetPulseTime = 0.0;
    }
}

//! Called after each "pulse" (i.e. each draw of some number of
//! quad-strips), to inform us of the measured drawing time.
void PulseController::RecordPulseTime (double seconds)
{
    if ((m_LwrFreq > 0.0) && (seconds > 0.0))
    {
        // Thinking about the control loop here:
        //
        // if TargetPulseTime ==   0.0005 (1khz target freq with 50% duty cycle)
        //   and LwrPulseSize ==  50.0    (prev guess was to draw 50 quad strips)
        //
        // And seconds measured was:
        //    0.0010, time was 2x target, LwrPulseSize should decrease towards 25.
        //    0.0005, time was just right, LwrPulseSize is correct.
        //    0.00025 time was .5x target, LwrPulseSize should increase towards 100.
        //
        // Since the measured time is "noisy", our goal is to colwerge on the right
        // m_LwrPulseSize slowly, over several pulses, rather than all at once.
        //
        // Since PulseSize (in quad-strips) is an integer and is coarse, we also
        // want to let it "dither" to hit the target frequency/duty-cycle.

        // Smooth over 4 samples.
        const double SampleWindow = 4.0;

        m_LwrPulseSize = ((m_LwrPulseSize * (SampleWindow-1)) +
                          (m_LwrPulseSize * (m_TargetPulseTime / seconds))) /
                         SampleWindow;

        if (m_LwrPulseSize < 1.0)
        {
            m_LwrPulseSize = 1.0;
        }
        else if (m_LwrPulseSize > m_MaxPulseSize)
        {
            m_LwrPulseSize = m_MaxPulseSize;
        }
    }

    if (m_PulseCount < PulseCountLimit)
    {
        // Gather statistics.
        m_PulseTimeSum += seconds;
        m_PulseCount++;

        if (m_PulseCount == PulseCountLimit)
        {
            // Report.
            Printf(Tee::PriLow, "Target %.1fHz @%d%% active\n",
                    m_LwrFreq, m_DutyPct);

            const double actualPulse = m_PulseTimeSum / m_PulseCount;
            const int activePct = (int)(100.0 * actualPulse / m_TargetCycleTime);

            if (activePct <= 100)
            {
                Printf(Tee::PriLow, "Actual %.1fHz @%d%% active\n",
                        m_LwrFreq, activePct);
            }
            else
            {
                Printf(Tee::PriLow, "Actual %.1fHz @99%% active\n",
                        1.0/actualPulse);
            }
        }
    }

    if ((m_FreqSweepOctavesPerSecond > 0.0) && (m_LwrFreq > 0.0))
    {
        // If we've hit the boundary of min or max pulse size, we must switch
        // sweep directions.  We started with max pulse size (low freq) and
        // started with +1 direction (increasing freq).
        if (m_SweepDir > 0.0)
        {
            if (m_LwrPulseSize <= 1.0)
            {
                m_SweepDir = -1.0;
                m_MaxFreq = m_LwrFreq;
            }
        }
        else
        {
            if (m_LwrPulseSize >= m_MaxPulseSize)
            {
                m_SweepDir = 1.0;
                m_MinFreq = m_LwrFreq;
            }
        }

        // Update target frequency.

        SetTargets (m_DutyPct, m_LwrFreq + m_SweepDir * m_LwrFreq *
                        m_FreqSweepOctavesPerSecond * m_TargetCycleTime);
    }
}

//------------------------------------------------------------------------------
void GLStressTest::CalcGeometrySizeTorus()
{
    m_VxPerPlane  = 0;
    m_IdxPerPlane = 0;
    // No support for tessellation shader yet.
    m_XTessRate = 1;

    m_Cols = (GLuint)(0.5 + sqrt(m_Width * m_Height / m_PpV));
    if (m_Cols < 3)
        m_Cols = 3;

    m_Rows = m_Cols;

    m_NumVertexes = 2 * (m_Cols + 1) * m_Rows;
    m_NumIndexes  = 2 * m_Cols * m_Rows * 3;

    Printf(Tee::PriLow,
           "%s will use %d major and %d minor strips, %d vx %d idx.\n",
            GetName().c_str(),
            m_Rows,
            m_Cols,
            m_NumVertexes,
            m_NumIndexes);
}

//------------------------------------------------------------------------------
void GLStressTest::GenGeometryTorus()
{
    // Fill in the vertex data:
    void * pVtxBuf = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_WRITE);
    GLuint * pIdxBuf = static_cast<GLuint *>(
            glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_READ_WRITE));

    // The torus is drawn "flat" to the screen like a big letter 'O'.
    //
    // We can paint the torus in "major" or "minor" order:
    //
    //  - "Major" strips go around the 'O' parallel to the screen.
    //    An entire major strip is either visible or not visible.
    //    An "outside" major strip has a larger radius than an "inside" strip.
    //
    //  - "Minor" strips go around the body of the torus perpendilwlar to the
    //    screen.
    //    Half of each minor strip is visible, half is not.
    //    All the minor strips are the same diameter.
    //
    // We can draw the torus in either order for different workloads.
    //
    // Instead of generating the smallest possible list of vertices,
    // we generate lots of duplicates to keep locality (GPU cache hits) high.

    const GLuint iEnd = m_TorusMajor ? m_Rows : m_Cols;
    const GLuint jEnd = m_TorusMajor ? m_Cols : m_Rows;

    GLuint vIdx = 0;
    GLuint iIdx = 0;
    for (GLuint i = 0; i < iEnd; i++)
    {
        GLuint stripIdx = 0;  // Begin new triangle strip.

        for (GLuint j = 0; j <= jEnd; j++)
        {
            for (int k = 1; k >= 0; k--)
            {
                const GLuint majorIdx = (m_TorusMajor ? (i+k) : j) % m_Rows;
                const GLuint minorIdx = (m_TorusMajor ? j : (i+k)) % m_Cols;

                vtx_f3_f3_f2 v = GenTorusVertex(majorIdx, minorIdx, minorIdx&1);
                StoreVertex(v, vIdx, pVtxBuf);

                if (stripIdx >= 2)
                {
                    // Store indices to generate a clockwise triangle for every
                    // new vertex (after the first 2 of a new strip).
                    if (stripIdx & 1)
                    {
                        pIdxBuf[iIdx+0] = vIdx-1;
                        pIdxBuf[iIdx+1] = vIdx-2;
                        pIdxBuf[iIdx+2] = vIdx;
                    }
                    else
                    {
                        pIdxBuf[iIdx+0] = vIdx-2;
                        pIdxBuf[iIdx+1] = vIdx-1;
                        pIdxBuf[iIdx+2] = vIdx;
                    }
                    // @@@ doesn't exactly match strezz, which gets triangle
                    // winding backwards on tri 2, 4, 6, etc.
                    iIdx += 3;
                }
                vIdx++;
                stripIdx++;
            }
        }
    }
    glUnmapBuffer(GL_ARRAY_BUFFER);
    glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
}

//------------------------------------------------------------------------------
// Notes on the vertex transforms:  (w=1024, h=768)
//
// GL_MODELVIEW is from glTranslated(0,0,-8)
//
//     1 0 0  0
//     0 1 0  0
//     0 0 1 -8
//     0 0 0  1
//
// GL_PROJECTION is from glOrtho(-5.3, 5.3, -4, 4, 4, 12);
//
//     0.1875 0     0     0
//     0      0.25  0     0
//     0      0    -0.25 -2
//     0      0     0     1
//
// Corners of the view frustum are named:
//   NLL = Near Lower Left
//   NUR = Near Upper Right
//   FLL = Far Lower Left
//   FUR = Far Upper Right
//
// W is omitted below, it is always 1.0 with glOrtho.
//
//      (object)     (world)      (clip)    (normalized)
// NLL: -5.3 -4  4   -5.3 -4  -4  -1 -1  1  -1 -1  1
// NUR:  5.3  4  4    5.3  4  -4   1  1  1   1  1  1
// FLL: -5.3 -4 -4   -5.3 -4 -12  -1 -1 -1  -1 -1 -1
// FUR:  5.3  4 -4    5.3  4 -12   1  1 -1   1 -1 -1

GLStressTest::vtx_f3_f3_f2 GLStressTest::GenTorusVertex
(
    GLuint majorIdx,
    GLuint minorIdx,
    bool bump
)
{
    const double Pi = 3.14159265358979323846;
    const double twoPi = 2 * Pi;

    // Distance from center of tube body to surface of tube.
    const double tubeRadius = (m_TorusDiameter - m_TorusHoleDiameter)/2.0 /2.0;
    // tubeRadius is 2.0 (torus Z is -2 to 2)

    // Distance from (0,0,0) to center of tube body.
    const double torusRadius = (m_TorusDiameter/2.0) - tubeRadius;
    // torusRadius is 3.0

    // majorIdx is which long line around the torus.
    //
    // majorIdx goes from 0 (at outside max diameter) to m_Rows-1.
    //   m_Rows/4   is the near side of the torus.
    //   m_Rows/2   is the inner edge of the hole.
    //   m_Rows*3/4 is the back side of the torus.

    const double tubeAngle   = majorIdx * -1.0 * twoPi/m_Rows;
    const double majorRadius = torusRadius + tubeRadius * cos(tubeAngle);

    // minorIdx is which short line around the body of the torus.
    //
    // minorIdx goes from 0 (at right) to m_Cols-1.
    //   m_Cols/4   is the top slice of the torus.
    //   m_Cols/2   is the left slice of the torus.
    //   m_Cols*3/4 is the bottom slice of the torus.

    const double torusAngle = minorIdx * twoPi/m_Cols;

    vtx_f3_f3_f2 v;

    // "strezz" swaps z and y below for no obvious reason, glstress doesn't.
    v.pos[0] = (GLfloat)(majorRadius * cos(torusAngle));
    v.pos[1] = (GLfloat)(majorRadius * sin(torusAngle));
    v.pos[2] = (GLfloat)(tubeRadius *  sin(tubeAngle));

    // "strezz" swaps z and y below for no obvious reason, glstress doesn't.
    v.norm[0] = (GLfloat)(cos(tubeAngle) * cos(torusAngle));
    v.norm[1] = (GLfloat)(cos(tubeAngle) * sin(torusAngle));
    v.norm[2] = (GLfloat)(sin(tubeAngle));

    // "strezz" stores x,y as texture S,T.

    // Scale S,T so that textures are "square" on the torus and we get
    // about pow(2,TxD) texels per pixel on the near side.

    const double texRepeats = pow(2.0, -m_TxD) * m_Width / m_TxSize;
    v.tex0[0] = (GLfloat)(torusAngle / twoPi * texRepeats);
    v.tex0[1] = (GLfloat)(tubeAngle / twoPi * texRepeats);

    if (bump)
    {
        // "bump" out the vertex slightly to make grooves in the torus.
        v.pos[0] += 0.1F * v.norm[0];
        v.pos[1] += 0.1F * v.norm[1];
        v.pos[2] += 0.1F * v.norm[2];
    }

    return v;
}

//------------------------------------------------------------------------------
void GLStressTest::CalcGeometrySizePlanes()
{
    double numVertices = m_Width * m_Height / m_PpV;

    if (numVertices < 4.0)
    {
        // Minimum numVertices is 4 (we draw one big quad).
        numVertices = 4.0;
    }
    else if (numVertices > 1e6)
    {
        // Let's limit ourselves to no more than 1m vertices.
        numVertices = 1e6;
    }

    // Find a number of rows, columns of vertices that matches.

    m_Cols = (GLuint)(0.5 + m_Width / sqrt(m_PpV));
    if (m_Cols < 1)
        m_Cols = 1;

    m_Rows = (GLuint)(0.5 + numVertices / m_Cols);
    if (m_Rows < 1)
        m_Rows = 1;

    if (m_UseTessellation)
    {
        // We will send only a few vertices to HW, and let the gpu turn
        // them into many for us.
        // Leave m_Rows alone, because we need to have flexibility on
        // how many strips to draw at once for the "pulsing" feature.

        m_XTessRate = m_Cols;
        m_Cols = 1;
    }

    m_VxPerPlane  = (m_Rows+1) * (m_Cols+1);
    m_NumVertexes = m_VxPerPlane * 2;
    m_IdxPerPlane = m_Rows * 2 * (m_Cols + 1);
    m_NumIndexes  = m_IdxPerPlane * 2;

    Printf(Tee::PriLow,
           "%s will use %d rows and %d cols, X, Y step of %.3f,%.3f\n",
            GetName().c_str(),
            m_Rows,
            m_Cols,
            m_Width / (double)m_Cols,
            m_Height / (double)m_Rows);
}

//------------------------------------------------------------------------------
void GLStressTest::GenGeometryPlanes()
{
    // Notes on the vertex transforms:  (assume !BoringXform, w=1024, h=768)
    //
    // GL_MODELVIEW is from glTranslated(-w/2, -h/2, -200)
    //
    //     1 0 0 -512
    //     0 1 0 -384
    //     0 0 1 -200
    //     0 0 0  1
    //
    // GL_PROJECTION is from glFrustum(-w/2, w/2,  -h/2, h/2,  100, 300)
    //
    //     0.1953 0       0    0
    //     0      0.2604  0    0
    //     0      0      -2 -300
    //     0      0      -1    0
    //
    // Note that the sequence of the vertices differs for near/far.
    // In the near plane, v[0] is upper-right (1024,768) and v[n-1] is at (0,0).
    // In the far plane,  v[0] is at (0,0) and v[n-1] is at (1024,768).
    //
    // Also, the quad-strip indices are sent in different order for near/far.
    //
    // Near plane
    // (object)            (world)           (clip)             (normalized)
    // LL:    0   0 99 1  -512 -384 -101 1  -100 -100 -98 101  -0.99 -0.99 -0.97
    // UL:    0 768 99 1  -512  384 -101 1  -100  100 -98 101  -0.99 -0.99 -0.97
    // LR: 1024   0 99 1   512 -384 -101 1   100 -100 -98 101   0.99 -0.99 -0.97
    // UR: 1024 768 99 1   512  384 -101 1   100  100 -98 101   0.99 -0.99 -0.97
    // Cen  512 384 49 1     0    0 -151 1     0    0   2 151   0     0     0.01
    //
    // Far plane
    // (object)            (world)           (clip)             (normalized)
    // LL:    0   0 89 1  -512 -384 -111 1  -100 -100 -78 111  -0.90 -0.90 -0.70
    // UL:    0 768 89 1  -512  384 -111 1  -100  100 -78 111  -0.90  0.90 -0.70
    // LR: 1024   0 89 1   512 -384 -111 1   100 -100 -78 111   0.90 -0.90 -0.70
    // UR: 1024 768 89 1   512  384 -111 1   100  100 -78 111   0.90  0.90 -0.70
    // Cen  512 384 39 1     0    0 -161 1     0    0  22 161   0     0     0.13

    // Fill in the vertex data:
    void * pvtxBuf = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_WRITE);
    GLuint ivtxNear = 0;
    GLuint ivtxFar  = m_VxPerPlane;

    for (GLuint row=0;  row <= m_Rows;  row++)
    {
        const GLfloat y = row * m_Height / (GLfloat)m_Rows;

        for (GLuint col=0;  col <= m_Cols; col++)
        {
            const GLfloat x = col * m_Width / (GLfloat)m_Cols;

            vtx_f3_f3_f2 v = {{0,0,0},{0,0,0},{0,0}};

            // Normal points towards camera (& light) at center,
            // 45% away from camera at center of each edge.
            // Lighting will make center 100% brightness, dimming towards corners.

            v.norm[0] = x / m_Width - 0.5f;
            v.norm[1] = y / m_Height - 0.5f;
            v.norm[2] = sqrt(1.0f - v.norm[0]*v.norm[0] - v.norm[1]*v.norm[1]);

            if (m_BoringXform)
            {
                v.pos[0] = x / (m_Width/2) - 1.0f;
                v.pos[1] = y / (m_Height/2) - 1.0f;
                v.pos[2] = 0.1f;
                StoreVertex(v, ivtxNear, pvtxBuf);

                v.pos[0] *= 0.9f;
                v.pos[1] *= 0.9f;
                v.pos[2] = 0.2f;
                StoreVertex(v, ivtxFar, pvtxBuf);
            }
            else
            {
                // Z ranges from 100 at the near view-plane to -100 at the far.
                // We have to almost touch the near view-plane at the edges
                // to draw the whole screen when using a perspective projection.
                // Use "farther" (smaller) Z values towards the center of the screen.
                GLfloat z;

                float xDist = 1.0f - fabs((x - m_Width/2.0f)  / (m_Width/2.0f));
                float yDist = 1.0f - fabs((y - m_Height/2.0f) / (m_Height/2.0f));

                if (xDist > yDist)
                  z = 99 - (yDist * 50);
                else
                  z = 99 - (xDist * 50);

                v.pos[0] = x;
                v.pos[1] = y;
                v.pos[2] = z - 10;
                StoreVertex(v, ivtxFar, pvtxBuf);

                v.pos[0] = m_Width - x;
                v.pos[1] = m_Height - y;
                v.pos[2] = z;
                v.norm[0] *= -1.0f;
                v.norm[1] *= -1.0f;
                StoreVertex(v, ivtxNear, pvtxBuf);
            }

            ivtxFar++;
            ivtxNear++;
        }
    }
    glUnmapBuffer(GL_ARRAY_BUFFER);

    if (!m_UseTessellation)
    {
        // Fill in the indexes, both planes:
        GLuint * pidxNear = (GLuint *) glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_READ_WRITE);
        GLuint * pidxFar  = pidxNear + m_IdxPerPlane;

        for (UINT32 t = 0; t < m_Rows; t++)
        {
            pidxFar[0]  = m_VxPerPlane + (t+1)*(m_Cols+1);
            pidxFar[1]  = m_VxPerPlane + (t)  *(m_Cols+1);
            pidxNear[0] = (t)  *(m_Cols+1);
            pidxNear[1] = (t+1)*(m_Cols+1);
            pidxFar += 2;
            pidxNear += 2;

            for (UINT32 s = 1; s <= m_Cols; s++)
            {
                pidxFar[0] = pidxFar[-2] + 1;
                pidxFar[1] = pidxFar[-1] + 1;
                pidxNear[0] = pidxNear[-2] + 1;
                pidxNear[1] = pidxNear[-1] + 1;
                pidxFar += 2;
                pidxNear += 2;
            }
        }
        glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
    }
}

//------------------------------------------------------------------------------
void GLStressTest::StoreVertex
(
    const GLStressTest::vtx_f3_f3_f2 & bigVtx,
    UINT32 index,
    void* buf
)
{
    if (m_BigVertex)
    {
        vtx_f3_f3_f2 * pDst = static_cast<vtx_f3_f3_f2 *>(buf) + index;
        *pDst = bigVtx;
    }
    else
    {
        vtx * pDst = static_cast<vtx *>(buf) + index;

        if (m_BoringXform)
        {
            pDst->f4.pos[0] = bigVtx.pos[0];
            pDst->f4.pos[1] = bigVtx.pos[1];
            pDst->f4.pos[2] = bigVtx.pos[2];
            pDst->f4.pos[3] = 1.0f;
        }
        else
        {
            pDst->s4s3.pos[0] = static_cast<GLshort>(bigVtx.pos[0] + 0.5f);
            pDst->s4s3.pos[1] = static_cast<GLshort>(bigVtx.pos[1] + 0.5f);
            pDst->s4s3.pos[2] = static_cast<GLshort>(bigVtx.pos[2] + 0.5f);
            pDst->s4s3.pos[3] = 1;

            pDst->s4s3.norm[0] = static_cast<GLshort>(127.5 * bigVtx.norm[0]);
            pDst->s4s3.norm[1] = static_cast<GLshort>(127.5 * bigVtx.norm[1]);
            pDst->s4s3.norm[2] = static_cast<GLshort>(127.5 * bigVtx.norm[2]);
        }
    }
}

//------------------------------------------------------------------------------
void GLStressTest::AddJsonFailLoop()
{
    // Record how far along we were when the error oclwrred.
    UINT32 failMsec = static_cast<UINT32>(m_ElapsedGpu * 1000.0);
    GetJsonExit()->AddFailLoop(failMsec);
}

//------------------------------------------------------------------------------
// EndLoop will call the base class's EndLoop to see if any of the following
// conditions are set:
// Golden values stop on error is set
// TestConfig early exit on error is set.
// User is trying to abort the test through the g_AbortAllTests js property.
RC GLStressTest::DoEndLoop(UINT32 * pNextEndLoopMs)
{
    RC rc;
    if ((m_ElapsedGpu  * 1000.0) >= *pNextEndLoopMs)
    {
        rc = EndLoop(*pNextEndLoopMs);
        *pNextEndLoopMs += m_EndLoopPeriodMs;
    }
    return rc;
}

//------------------------------------------------------------------------------
/*virtual*/
void GLStressTest::PowerControlCallback(LwU32 milliWatts)
{
    if (m_Watts)
    {
        // The control constant "kp" scales "error milliwatts" to
        // "fractional jump in DrawPct", and was chosen by trial and error.
        //
        // When errMilliWatts is positive, it reduces DrawPct a bit at a time
        // until we reduce power enough to be at the target.
        const double kp = -0.00001;
        const double errMw = milliWatts - m_Watts*1000.0;
        const double newDrawPct = m_DrawPct + m_DrawPct * kp * errMw;
        m_DrawPct = max(1.0, min(100.0, newDrawPct));
    }
}

//------------------------------------------------------------------------------
RC GLStressTest::GpuProg::Load()
{
    if (!IsSet())
        return OK;

    if (!mglExtensionSupported(m_Extension))
        return OK;

    glGenProgramsLW(1, &m_Id);

    string text;
    text = m_Text;
    return mglLoadProgram(m_Enum, m_Id, (GLsizei) text.size(),
                          (const GLubyte *) text.c_str());
}

void GLStressTest::GpuProg::Unload()
{
    if (IsLoaded())
    {
        glDeleteProgramsLW(1, &m_Id);
        m_Id = 0;
    }
}

void GLStressTest::GpuProg::Setup()
{
    if (!IsLoaded())
        return;

    glEnable(m_Enum);
    glBindProgramLW(m_Enum, m_Id);

    // Handy constants available to all program types.
    glProgramParameter4fLW(m_Enum, 20, 0.5, 4.0, 64.0, 1.0);

    // User-defined constants from JavaScript for this program type.
    ProgProps & props = m_UserProps;
    Printf(Tee::PriDebug, "Setting %d %s user props.\n",
            (int)props.size(),
            m_Name);

    for (ProgProps::iterator it = props.begin(); it != props.end(); ++it)
    {
        glProgramParameter4fLW(m_Enum,
                it->first,
                it->second.value[0],
                it->second.value[1],
                it->second.value[2],
                it->second.value[3]);
    }
}

void GLStressTest::GpuProg::Suspend()
{
    if (!IsLoaded())
        return;

    glDisable(m_Enum);
}
void GLStressTest::GpuProg::Resume()
{
    if (!IsLoaded())
        return;

    glEnable(m_Enum);
    glBindProgramLW(m_Enum, m_Id);
}

