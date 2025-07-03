/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file glcompute.cpp
 * class GLComputeTest
 *
 * GLComputeTest uses GL compute shaders to exercise fast subchannel context
 * switches between graphics and compute work.
 * Maybe it will also hit simultaneous compute & graphics paths on gpus that
 * support such a thing?  Need to verify/test that.
 * http://lwbugs/884913
 *
 * Run:
 *   while not done
 *     pick x,y size of simulation grid
 *     pick #generations to evolve the simulation
 *     initialize surfaces
 *     for (gen=0; gen < numGenerations; gen++)
 *       call compute shader to evolve simulation
 *       draw results on screen
 *       note: overlap computing gen N+1 while drawing gen N
 *     for (gen=numGenerations-1; gen > 0; gen--)
 *       call compute shader to evolve simulation backwards
 *       draw results on screen
 *       note: overlap computing gen N-1 while drawing gen N
 *     check that simulation ended in starting state
 *   next simulation
 */

#include "opengl/modsgl.h"
#include "core/include/tee.h"
#include "core/include/jscript.h"
#include "core/include/fpicker.h"
#include "core/include/platform.h"
#include "gpu/include/gpudev.h"
#include "gpu/tests/gputest.h"
#include "core/include/xp.h"
#include "gpu/js/fpk_comm.h"
#include "core/include/utility.h"
#include "core/include/jsonlog.h"
//------------------------------------------------------------------------------
class GLComputeTest : public GpuTest
{
public:
    GLComputeTest();
    virtual ~GLComputeTest();

    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    virtual RC SetDefaultsForPicker(UINT32 Idx);

    // JS properties:
private:
    bool   m_KeepRunning;
    UINT32 m_LoopMs;
    string m_ComputeShader;
    string m_VertexShader;
    string m_FragmentShader;
    bool   m_DumpShaderBuffer;
    UINT32 m_EndLoopPeriodMs;
    UINT32 m_FrameSleepMs;

public:
    SETGET_PROP(KeepRunning, bool);
    SETGET_PROP(LoopMs, UINT32);
    SETGET_PROP(ComputeShader, string);
    SETGET_PROP(VertexShader, string);
    SETGET_PROP(FragmentShader, string);
    SETGET_PROP(DumpShaderBuffer, bool);
    SETGET_PROP(EndLoopPeriodMs, UINT32);
    SETGET_PROP(FrameSleepMs, UINT32);
    SETGET_PROP(PrintFrames, bool);
private:
    enum TxIndexes
    {
        TxDEAD_CELL,
        TxLIVE_CELL,
        TxNUM_TEXTURES
    };
    //! Also controls how many frames ahead of HW we get before waiting.
    static const int NumTimers = 3;
    //! Size of a cta (thread block): 16x16 threads.
    static const GLuint CtaSize = 16U;

    // GL state:
    mglTestContext m_mglTestContext;
    GLuint m_TextureObjectIds[TxNUM_TEXTURES];
    GLuint m_CellsBufferId;
    GLuint m_ComputeShaderId;
    GLuint m_VertexShaderId;
    GLuint m_FragmentShaderId;
    GLuint m_GraphicsProgramId;
    GLuint m_ComputeProgramId;
    GLuint m_ProgramPipelineId;
    mglTimerQuery * m_Timers;

    // Controls for the current sim-world run.
    GLuint64EXT m_SimCellsGpuAddr; //WAR for bug 2315762
    GLuint m_SimWidthCells;
    GLuint m_SimHeightCells;
    GLuint m_WindowWidthPixels;
    GLuint m_WindowHeightPixels;
    GLfloat m_DrawLeft;
    GLfloat m_DrawRight;
    GLfloat m_DrawTop;
    GLfloat m_DrawBottom;
    GLint  m_SimMaxGeneration;
    GLint  m_SimGeneration;
    GLint  m_SimStep;          //!< 1 for forwards time, -1 for backwards.
    vector<GLint> m_SimWorldAtGen0;

    // Hooks into the GLSL uniform variables.
    ModsGL::Uniform<GLuint64EXT> m_CsimCellsGpuAddr; //WAR for bug 2315762
    ModsGL::Uniform<GLint> m_CsimWidthCells;
    ModsGL::Uniform<GLint> m_CsimHeightCells;
    ModsGL::Uniform<GLint> m_Cgeneration;
    ModsGL::Uniform<GLint> m_CgenStep;

    ModsGL::Uniform<GLint> m_Ggeneration;
    ModsGL::Uniform<GLint> m_GgenStep;
    ModsGL::Uniform<GLint> m_GsimWidthCells;
    ModsGL::Uniform<GLint> m_GsimHeightCells;
    ModsGL::Uniform<GLfloat> m_GscreenWidth;
    ModsGL::Uniform<GLfloat> m_GscreenHeight;
    ModsGL::Uniform<GLint> m_Gdead;
    ModsGL::Uniform<GLint> m_Glive;

    // Misc test variables
    bool m_PrintFrames;
    UINT64 m_FrameCount;
    UINT32 m_ElapsedWallClockMs;
    FancyPicker::FpContext *m_pFpCtx;
    FancyPickerArray       *m_pPickers;

    // Private functions:
    RC LoadShaders();
    RC LoadTextures();
    RC DoOneFrame();
    void PickNewSim();
    RC SetupNewSimInGL();
    RC CheckSimReversibility();
    enum PrintMode { PrintBefore, PrintAfter };
    void Print(Tee::Priority pri, PrintMode p);
    void ReadSimCells(vector<GLint> *pvec, int whichHalf);
    void DumpSimCells(int whichHalf);
    enum PrintSimMode { ActualValues, As01, AsSymbol };
    void PrintSimCells(Tee::Priority pri, const vector<GLint> & vec, PrintSimMode p);
    RC DoEndLoop(UINT32 * pNextEndLoopMs);
};

//------------------------------------------------------------------------------
JS_CLASS_INHERIT_FULL(GLComputeTest, GpuTest,
                      "3d,Compute context-switching test",
                      0);

CLASS_PROP_READWRITE(GLComputeTest, KeepRunning, bool,
                     "Test will keep running while this is true.");
CLASS_PROP_READWRITE(GLComputeTest, LoopMs, UINT32,
                     "Test target test time in milliseconds wall clock.");
CLASS_PROP_READWRITE(GLComputeTest, ComputeShader, string,
                     "Compute shader (GLSL) as a string");
CLASS_PROP_READWRITE(GLComputeTest, VertexShader, string,
                     "Vertex shader (GLSL) as a string");
CLASS_PROP_READWRITE(GLComputeTest, FragmentShader, string,
                     "Fragment shader (GLSL) as a string");
CLASS_PROP_READWRITE(GLComputeTest, DumpShaderBuffer, bool,
                     "Debug feature: dump SBO after each compute step.");
CLASS_PROP_READWRITE(GLComputeTest, EndLoopPeriodMs, UINT32,
                     "Time in milliseconds between calls to EndLoop (default = 10000).");
CLASS_PROP_READWRITE(GLComputeTest, FrameSleepMs, UINT32,
                     "Time in milliseconds to sleep between frames (default 0).");
CLASS_PROP_READWRITE(GLComputeTest, PrintFrames, bool,
                    "Print each frame's progress(debug only)");
//------------------------------------------------------------------------------
// Default shaders from glcompute_default_shaders.cpp.
// These can be overridden from JS at runtime.
extern const char * defaultComputeShader;
extern const char * defaultVertexShader;
extern const char * defaultFragmentShader;

//------------------------------------------------------------------------------
GLComputeTest::GLComputeTest() :
    m_KeepRunning(false)
    ,m_LoopMs(5000)
    ,m_ComputeShader()  // Avoid setting default shaders in constructor as they
    ,m_VertexShader()   // will be dumped to log with -testhelp arg
    ,m_FragmentShader()
    ,m_DumpShaderBuffer(false)
    ,m_EndLoopPeriodMs(10000)
    ,m_FrameSleepMs(0)
    ,m_CellsBufferId(0)
    ,m_ComputeShaderId(0)
    ,m_VertexShaderId(0)
    ,m_FragmentShaderId(0)
    ,m_GraphicsProgramId(0)
    ,m_ComputeProgramId(0)
    ,m_ProgramPipelineId(0)
    ,m_Timers(nullptr)
    ,m_SimCellsGpuAddr(0)
    ,m_SimWidthCells(0)
    ,m_SimHeightCells(0)
    ,m_WindowWidthPixels(0)
    ,m_WindowHeightPixels(0)
    ,m_DrawLeft(0.0f)
    ,m_DrawRight(0.0f)
    ,m_DrawTop(0.0f)
    ,m_DrawBottom(0.0f)
    ,m_SimMaxGeneration(0)
    ,m_SimGeneration(0)
    ,m_SimStep(0)
    ,m_CsimCellsGpuAddr("pCells")
    ,m_CsimWidthCells("simWidthCells")
    ,m_CsimHeightCells("simHeightCells")
    ,m_Cgeneration("generation")
    ,m_CgenStep("genStep")
    ,m_Ggeneration("generation")
    ,m_GgenStep("genStep")
    ,m_GsimWidthCells("simWidthCells")
    ,m_GsimHeightCells("simHeightCells")
    ,m_GscreenWidth("screenWidth")
    ,m_GscreenHeight("screenHeight")
    ,m_Gdead("dead")
    ,m_Glive("live")
    ,m_PrintFrames(false)
    ,m_FrameCount(0)
    ,m_ElapsedWallClockMs(0)
{
    SetName("GLComputeTest");
    memset(m_TextureObjectIds, 0, sizeof(m_TextureObjectIds));
    CreateFancyPickerArray(FPK_GLCOMPUTE_NUM_PICKERS);
    m_pPickers = GetFancyPickerArray();
    m_pFpCtx = GetFpContext();
}

//------------------------------------------------------------------------------
/*virtual*/
GLComputeTest::~GLComputeTest()
{
}

//------------------------------------------------------------------------------
RC GLComputeTest::SetDefaultsForPicker(UINT32 Idx)
{
    FancyPickerArray * pPickers = GetFancyPickerArray();
    FancyPicker &Fp = (*pPickers)[Idx];
    switch (Idx)
    {
        case FPK_GLCOMPUTE_SIM_STEPS:
            Fp.ConfigConst(400);
            break;
        case FPK_GLCOMPUTE_SIM_CELLS:
            Fp.ConfigList(FancyPicker::WRAP);
            Fp.AddListItem(307200, 5);   // 640x480 cells
            Fp.AddListItem( 43200, 4);   // 240x180 cells
            Fp.AddListItem( 10800, 3);   // 120x90 cells
            Fp.AddListItem(  2700, 2);   // 60x45 cells
            Fp.AddListItem(   520, 1);   // 26x20 cells
            break;
        case FPK_GLCOMPUTE_CELL_PIXELS:
            Fp.FConfigList(FancyPicker::WRAP);
            Fp.FAddListItem( 1.0, 1); // draw each cell as 1x1 pixels, for each sim size
            Fp.FAddListItem( 4.0, 2);
            Fp.FAddListItem( 9.0, 3);
            Fp.FAddListItem(16.0, 4);
            Fp.FAddListItem(25.0, 5);
            Fp.FAddListItem(36.0, 6);
            Fp.FAddListItem(49.0, 7);  // draw each cell as 7x7 pixels
            // Note: 7 items here, 5 for SIM_CELLS: these are mutually prime,
            // so if we run long enough we will hit all combinations.
            break;
        default:
            Printf(Tee::PriError, "Invalid fancy picker index:%d\n", Idx);
            return RC::SOFTWARE_ERROR;
    }
    return OK;
}

//------------------------------------------------------------------------------
/*virtual*/
bool GLComputeTest::IsSupported()
{
    if (!mglHwIsSupported(GetBoundGpuSubdevice()))
       return false;

    // Since IsSupported is called before Setup, we can't call into
    // the GL driver to check extension support.
    // GL supports GL_LW_compute_program5 on Fermi and later.
    switch(GetBoundGpuDevice()->GetFamily())
    {
        case GpuDevice::None:
        case GpuDevice::Celsius:
        case GpuDevice::Rankine:
        case GpuDevice::Lwrie:
        case GpuDevice::Tesla:
        case GpuDevice::Fermi:
            return false;

        case GpuDevice::Kepler:
        case GpuDevice::Maxwell:
        default:  // Assume future families will support this as well.
            break;
    }

    return true;
}

//------------------------------------------------------------------------------
/*virtual*/
RC GLComputeTest::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());
    CHECK_RC(m_mglTestContext.SetProperties(
                 GetTestConfiguration(),
                 false,  // double buffer
                 GetBoundGpuDevice(),
                 GetDispMgrReqs(),
                 0,      // don't override color format
                 false,  // Force render to framebuffer object
                 false,  // Don't render FBOs to sysmem.
                 0       // Do not use layered FBO
                 ));

    CHECK_RC(m_mglTestContext.Setup());

    MASSERT(mglExtensionSupported("GL_LW_compute_program5"));
    glGenBuffers(1, &m_CellsBufferId);

    CHECK_RC(LoadShaders());
    GetFpContext()->Rand.SeedRandom(GetTestConfiguration()->Seed());
    CHECK_RC(LoadTextures());
    m_Timers = new mglTimerQuery[NumTimers];

    // Tell vertex shader the screen size.
    ModsGL::SetUniform(m_GscreenWidth, static_cast<GLfloat>
                  (GetTestConfiguration()->SurfaceWidth()));
    ModsGL::SetUniform(m_GscreenHeight, static_cast<GLfloat>
                  (GetTestConfiguration()->SurfaceHeight()));

    // Tell fragment shader which texture units to use.
    ModsGL::SetUniform(m_Gdead, TxDEAD_CELL);
    ModsGL::SetUniform(m_Glive, TxLIVE_CELL);

    return rc;
}

//------------------------------------------------------------------------------
void GLComputeTest::PickNewSim()
{
    // Reseed the PRNG on each new sim (aka "loop") so that each sim run is
    // independent of the others and we can start a test from a previously
    // seen-to-fail point.
    m_pFpCtx->Rand.SeedRandom(GetTestConfiguration()->Seed() + m_pFpCtx->LoopNum);

    // How many steps to run the simulation (forwards, then back):
    m_SimMaxGeneration = (*m_pPickers)[FPK_GLCOMPUTE_SIM_STEPS].Pick();

    // How large the sim world should be:
    // Given the chosen number of cells, pick a width and height that have
    // about that number of cells and that will give us approximately
    // square cells on-screen at 1024x768 resolution (mods test default).
    // Don't use the actual resolution, we don't want workload to depend
    // on what display is lwrrently plugged in.
    // Width and height must be multiples of two.
    // A reasonable minimum size is 6x4 cells.
    const UINT32 simCells =
        max<UINT32>(24, (*m_pPickers)[FPK_GLCOMPUTE_SIM_CELLS].Pick());
    const double logSimCells = log(static_cast<double>(simCells));
    const double logAspectRatio = log(1024.0 / 768.0);
    const double logWidth = (logSimCells + logAspectRatio)/2.0;
    const double logHeight = (logSimCells - logAspectRatio)/2.0;
    m_SimWidthCells  =
        max<UINT32>(6, static_cast<UINT32>(0.5 + exp(logWidth)));
    m_SimHeightCells =
        max<UINT32>(4, static_cast<UINT32>(0.5 + exp(logHeight)));
    m_SimWidthCells  = Utility::AlignUp(m_SimWidthCells, 2U);
    m_SimHeightCells = Utility::AlignUp(m_SimHeightCells, 2U);

    // How large to draw each sim cell on-screen:
    // We'll draw these as square, so W=H.
    const double cellPixels = max
        (1.0F, (*m_pPickers)[FPK_GLCOMPUTE_CELL_PIXELS].FPick());
    const double cellWHPixels = sqrt(cellPixels);
    m_WindowWidthPixels  = static_cast<UINT32>
        (0.5 + cellWHPixels * m_SimWidthCells);
    m_WindowHeightPixels = static_cast<UINT32>
        (0.5 + cellWHPixels * m_SimHeightCells);

    // Origin is lower-left in OpenGL.
    const float screenW = static_cast<float>
        (GetTestConfiguration()->SurfaceWidth());
    const float screenH = static_cast<float>
        (GetTestConfiguration()->SurfaceHeight());
    m_DrawLeft = screenW/2.0F - m_WindowWidthPixels/2.0F;
    m_DrawRight = m_DrawLeft + m_WindowWidthPixels;
    m_DrawTop = screenH/2.0F + m_WindowHeightPixels/2.0F;
    m_DrawBottom = m_DrawTop - m_WindowHeightPixels;

    // Resize and random-fill our initial-sim-world grid.
    // The center cells in the grid are 75% randomly "live", the outer cells are
    // initially zero.
    m_SimWorldAtGen0.resize(m_SimWidthCells * m_SimHeightCells);
    for (GLuint y = 0; y < m_SimHeightCells; ++y)
    {
        const bool vCenter = (y > m_SimHeightCells/3 &&
                              y < m_SimHeightCells*2/3);
        for (GLuint x = 0; x < m_SimWidthCells; ++x)
        {
            const bool cellIsAlive =
                vCenter &&
                (x > m_SimWidthCells/3 && x < m_SimWidthCells*2/3) &&
                (m_pFpCtx->Rand.GetRandom(0,3) > 0);

            m_SimWorldAtGen0[y*m_SimWidthCells + x] = cellIsAlive ? 1 : -1;
        }
    }

    // At the start of each sim, time is 0 and time evolves forwards.
    m_SimGeneration = 0;
    m_SimStep = 1;
}

//------------------------------------------------------------------------------
void GLComputeTest::Print(Tee::Priority pri, GLComputeTest::PrintMode p)
{
    int calcStep = m_SimGeneration;
    int displayStep = m_SimGeneration;
    // Are we printing before or after using m_SimStep to inc/dec?
    if (p == PrintBefore)
        calcStep += m_SimStep;
    else
        displayStep -= m_SimStep;

    Printf(pri,
           "Frame %u reads from %s writes to %s: sim %d %dx%d 0->%d->0, pixels %dx%d, compute %d,"
           " display %d generation %d simStep %d.\n",
           static_cast<unsigned>(m_FrameCount),
           m_FrameCount & 1 ? "high half" : "low half",
           m_FrameCount & 1 ? "low half" : "high half",
           m_pFpCtx->LoopNum,
           m_SimWidthCells,
           m_SimHeightCells,
           m_SimMaxGeneration,
           m_WindowWidthPixels,
           m_WindowHeightPixels,
           calcStep,
           displayStep,
           m_SimGeneration,
           m_SimStep);
}

//------------------------------------------------------------------------------
void GLComputeTest::ReadSimCells(vector<GLint> *pvec, int whichHalf)
{
    const int numCells = m_SimWidthCells * m_SimHeightCells;
    const int baseIndex = (whichHalf > 0) ? numCells : 0;

    pvec->clear();
    pvec->reserve(m_SimWidthCells * m_SimHeightCells);

    const GLint * pCells = static_cast<const GLint*>
        (glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY));

    for (int ii = 0; ii < numCells; ii++)
    {
        pvec->push_back(pCells[ii + baseIndex]);
    }
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
}

//------------------------------------------------------------------------------
void GLComputeTest::DumpSimCells(int whichHalf)
{
    vector<GLint> simCells;
    ReadSimCells(&simCells, whichHalf);

    Tee::Priority pri = GetVerbosePrintPri();
    Printf(pri, "Sim world %s half:\n", whichHalf ? "high" : " low");
    PrintSimCells(pri, simCells, ActualValues);
    Printf(pri, "\n");
}

//------------------------------------------------------------------------------
void GLComputeTest::PrintSimCells
(
    Tee::Priority pri,
    const vector<GLint> & vec,
    GLComputeTest::PrintSimMode p
)
{
    const size_t numCells = m_SimWidthCells * m_SimHeightCells;
    MASSERT(numCells <= vec.size());

    // Max format width for a cell in generation N is if it has been "dead" the
    // whole time and has value -N.  For generations up to 9, two columns.
    int cols = 2;
    if (p == ActualValues)
    {
        for (unsigned gen = m_SimMaxGeneration * 2; gen >= 10; gen /= 10)
            cols++;
    }

    for (size_t ii = 0; ii < numCells; ii++)
    {
        if (p == AsSymbol)
        {
            Printf(pri, "%*c", cols, vec[ii]);
        }
        else
        {
            const GLint val = (p == ActualValues) ? vec[ii] : (vec[ii] > 0);
            Printf(pri, "%*d", cols, val);
        }
        if ((m_SimWidthCells - 1) == (ii % m_SimWidthCells))
            Printf(pri, "\n");
    }
}

//------------------------------------------------------------------------------
RC GLComputeTest::SetupNewSimInGL()
{
    RC rc;

    // Realloc the compute buffer in GL to the new size.
    const GLuint cellsPerHalf = m_SimWidthCells * m_SimHeightCells;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_CellsBufferId);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 cellsPerHalf * 2 * sizeof(INT32),
                 NULL,
                 GL_DYNAMIC_DRAW);  // shaders will repeatedly r&w this

    // Link the reallocated compute buffer to the shaders.
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_CellsBufferId);

    // Fill in the initial values of the compute buffer.
    GLint * pCells = static_cast<GLint*>
        (glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_WRITE));

    for (GLuint ii = 0; ii < cellsPerHalf; ii++)
    {
        pCells[ii] = m_SimWorldAtGen0[ii];
    }
    for (GLuint ii = 0; ii < cellsPerHalf; ii++)
    {
        pCells[ii + cellsPerHalf] = 0;
    }
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    //******************************************
    // WAR for bug 2315762
    glGetBufferParameterui64vLW(
                GL_SHADER_STORAGE_BUFFER,
                GL_BUFFER_GPU_ADDRESS_LW,
                &m_SimCellsGpuAddr);

    // make it accessable to all the shaders.
    glMakeBufferResidentLW(GL_SHADER_STORAGE_BUFFER, GL_READ_WRITE);
    // Give the compute program the GpuAddress of the cells[]
    ModsGL::SetUniform(m_CsimCellsGpuAddr, m_SimCellsGpuAddr);
    // end of WAR
    // *******************************************
    // Tell compute program the dimensions of the new array.
    ModsGL::SetUniform(m_CsimWidthCells, m_SimWidthCells);
    ModsGL::SetUniform(m_CsimHeightCells, m_SimHeightCells);

    // Tell graphics program the dimensions of the new array.
    ModsGL::SetUniform(m_GsimWidthCells, m_SimWidthCells);
    ModsGL::SetUniform(m_GsimHeightCells, m_SimHeightCells);

    CHECK_RC(mglGetRC());

    return rc;
}

/*
  (gen 0 dir 1 frames 0)
  copy random setup to cell buf 0
  compute cellbuf 0 -> 1 gen 0->1
  draw frame 0
  sync
  (gen 1 dir 1 frame 1)
  compute cellbuf 1->0 gen 1->2
  draw frame 1
  sync
 */
//------------------------------------------------------------------------------
/*virtual*/
RC GLComputeTest::Run()
{
    StickyRC firstRc;
    m_FrameCount = 0;
    m_ElapsedWallClockMs = 0;
    unsigned timerIdx = 0;

    const UINT32 firstSim = GetTestConfiguration()->StartLoop();
    const UINT32 endSim = GetTestConfiguration()->Loops() + firstSim;
    const UINT64 startWallClockMs = Xp::GetWallTimeMS();

    m_SimGeneration = 0;
    m_SimStep = 1;
    m_pFpCtx->LoopNum = firstSim;
    UINT32 nextEndLoopMs = m_EndLoopPeriodMs;
    Printf(Tee::PriNormal, "Testing synchronous compute & graphics\n");

    while (m_KeepRunning ||
           (m_ElapsedWallClockMs < m_LoopMs) ||
           (m_pFpCtx->LoopNum < endSim))
    {
        if (m_FrameCount > timerIdx)
        {
            // Wait for frame n-NumTimers to complete.
            // This keeps us no more than NumTimers frames ahead of HW.
            (void) m_Timers[timerIdx].GetSeconds();
        }

        // Mark the start of this frame.
        m_Timers[timerIdx].Begin();

        StickyRC frameRc = DoOneFrame();
        firstRc = frameRc;

        // Mark the end of this frame
        m_Timers[timerIdx].End();
        timerIdx = (timerIdx + 1) % NumTimers;

        m_FrameCount++;
        m_ElapsedWallClockMs = static_cast<UINT32>(Xp::GetWallTimeMS() - startWallClockMs);

        if (OK != frameRc)
        {
            GetJsonExit()->AddFailLoop(static_cast<UINT32>(m_FrameCount));
            if (GetGoldelwalues()->GetStopOnError())
                break;
        }
        RC rc = DoEndLoop(&nextEndLoopMs);
        if (OK != rc)
        {
            firstRc = rc;
            break;
        }
    }
    glFinish();
    m_ElapsedWallClockMs = static_cast<UINT32>
        (Xp::GetWallTimeMS() - startWallClockMs);

    return firstRc;
}

//------------------------------------------------------------------------------
// Off-by-one errors are easy, try to get this right!
//  what      lo  hi  gen step
// ========== === === === ==
// PickNewSim >0<     0   1
// dispatch    0  >1< 0   1
// gen += step        1   1
// dispatch   >2<  1  1   1
// gen += step        2   1
// dispatch    2  >3< 2   1
// gen += step        3   1
// reverse            3  -1
// dispatch   >2<  3  3  -1
// gen += step        2  -1
// dispatch    2  >1< 2  -1
// gen += step        1  -1
// dispatch   >0<  1  1  -1
// gen += step        0  -1
// CheckSimReversibility
//
// Note: The workload distribution for Simultaneous Compute and Graphics runs as follows:
// 1. Place a fence in the graphics stream
// 2. Switch to the compute stream
// 3. Tell compute stream to wait on fence in graphics stream (#1)
// 4. Dispatch the new work on the compute stream
// 5. Add a fence to the compute stream
// 6. Switch to the graphics stream
// 7. Dispatch graphics work
// 8. Tell graphic stream to wait on fence from compute stream (#5)
RC GLComputeTest::DoOneFrame()
{
    StickyRC rc;

    // Set up and run the compute program.
    if (m_SimGeneration == 0 && m_SimStep == 1)
    {
        PickNewSim();
        CHECK_RC(SetupNewSimInGL());

        if (m_DumpShaderBuffer)
        {
            // Dump the initial surface before the first compute run.
            glFinish();
            Printf(GetVerbosePrintPri(), "Initial Sim Cells:\n");
            DumpSimCells(0);
        }
    }
    if (m_PrintFrames)
    {
        Print(GetVerbosePrintPri(), PrintBefore);
    }

    // Call the compute shader to callwlate the next frame's cells.
    ModsGL::SetUniform(m_Cgeneration, m_SimGeneration);
    ModsGL::SetUniform(m_Ggeneration, m_SimGeneration);
    ModsGL::SetUniform(m_CgenStep, m_SimStep);
    ModsGL::SetUniform(m_GgenStep, m_SimStep);

    glDispatchCompute((m_SimWidthCells + CtaSize - 1)/ CtaSize,
                      (m_SimHeightCells + CtaSize - 1) / CtaSize,
                      1);
    glFlush();

    // While the compute shader for the next frame is running,
    // Draw the current frame.
    glClearColor(0.0F, 0.0F, 0.0F, 0.0F);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw one big rectangle.  Counter-clockwise, origin is lower-left.
    glBegin(GL_QUADS);
        glVertexAttrib2f(1, 0.0F, 0.0F);
        glVertexAttrib2f(0, m_DrawLeft,  m_DrawBottom);

        glVertexAttrib2f(1, (float)m_SimWidthCells, 0.0F);
        glVertexAttrib2f(0, m_DrawRight, m_DrawBottom);

        glVertexAttrib2f(1, (float)m_SimWidthCells, (float)m_SimHeightCells);
        glVertexAttrib2f(0, m_DrawRight, m_DrawTop);

        glVertexAttrib2f(1, 0.0F, (float)m_SimHeightCells);
        glVertexAttrib2f(0, m_DrawLeft,  m_DrawTop);
    glEnd();

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    if (m_FrameSleepMs)
    {
        glFinish();
        Tasker::Sleep(m_FrameSleepMs);
    }

    m_SimGeneration += m_SimStep;

    if (m_DumpShaderBuffer)
    {
        // Dump the surface the compute shader just generated.
        glFinish();
        DumpSimCells(m_SimGeneration & 1);
    }

    if (m_SimStep > 0 && (m_SimGeneration >= m_SimMaxGeneration))
    {
        // When we've run the sim to the end, reverse direction and start
        // evolving it back towards the initial state.
        m_SimStep = -1;
    }
    else if (m_SimStep < 0 && m_SimGeneration == 0)
    {
        // When we've run the sim backwards to generation 0 again,
        // all the cell live/dead data should match initial state.
        // If not, we've got an error.
        if (Goldelwalues::Check == GetGoldelwalues()->GetAction())
            rc = CheckSimReversibility();

        // Reset, we will pick a new sim setup on the next call.
        m_SimGeneration = 0;
        m_SimStep = 1;
        // Each simulation run is it's own "loop".
        m_pFpCtx->LoopNum++;
        // ****************************************************
        // WAR for bug 2168478
        // make it inaccessable to all the shaders.
        glMakeBufferNonResidentLW(GL_SHADER_STORAGE_BUFFER);
    }

    rc = mglGetRC();
    return rc;
}

//------------------------------------------------------------------------------
/*virtual*/
RC GLComputeTest::Cleanup()
{
    StickyRC rc;
    delete [] m_Timers;
    m_Timers = nullptr;
    if (m_mglTestContext.IsGLInitialized())
    {
        // @@@ free all GL resources, or let them die with mglTestContext?
        if (m_CellsBufferId)
            glDeleteBuffers(1, &m_CellsBufferId);
    }
    rc = m_mglTestContext.Cleanup();
    rc = GpuTest::Cleanup();
    return rc;
}

//------------------------------------------------------------------------------
RC GLComputeTest::LoadShaders()
{
    RC rc;

    // Load default shaders if user has not defined any
    if (m_ComputeShader.empty())
    {
        m_ComputeShader = defaultComputeShader;
    }
    if (m_VertexShader.empty())
    {
        m_VertexShader = defaultVertexShader;
    }
    if (m_FragmentShader.empty())
    {
        m_FragmentShader = defaultFragmentShader;
    }

    // Compile the vertex and fragment shaders, link the graphics program.
    m_GraphicsProgramId = glCreateProgram();

    CHECK_RC(ModsGL::LoadShader(GL_FRAGMENT_SHADER, &m_FragmentShaderId,
                           m_FragmentShader.c_str()));
    glAttachShader(m_GraphicsProgramId, m_FragmentShaderId);

    CHECK_RC(ModsGL::LoadShader(GL_VERTEX_SHADER, &m_VertexShaderId,
                           m_VertexShader.c_str()));
    glAttachShader(m_GraphicsProgramId, m_VertexShaderId);

    glProgramParameteri(m_GraphicsProgramId, GL_PROGRAM_SEPARABLE, GL_TRUE);
    CHECK_RC(ModsGL::LinkProgram(m_GraphicsProgramId));

    m_Ggeneration.Init(m_GraphicsProgramId);
    m_GgenStep.Init(m_GraphicsProgramId);
    m_GscreenWidth.Init(m_GraphicsProgramId);
    m_GscreenHeight.Init(m_GraphicsProgramId);
    m_GsimWidthCells.Init(m_GraphicsProgramId);
    m_GsimHeightCells.Init(m_GraphicsProgramId);
    m_Glive.Init(m_GraphicsProgramId);
    m_Gdead.Init(m_GraphicsProgramId);
    CHECK_RC(mglGetRC());

    // Compile the compute shader, link the compute program.
    m_ComputeProgramId = glCreateProgram();

    CHECK_RC(ModsGL::LoadShader(GL_COMPUTE_SHADER, &m_ComputeShaderId,
                           m_ComputeShader.c_str()));
    glAttachShader(m_ComputeProgramId, m_ComputeShaderId);

    glProgramParameteri(m_ComputeProgramId, GL_PROGRAM_SEPARABLE, GL_TRUE);
    CHECK_RC(ModsGL::LinkProgram(m_ComputeProgramId));
    m_CsimCellsGpuAddr.Init(m_ComputeProgramId);
    m_CsimWidthCells.Init(m_ComputeProgramId);
    m_CsimHeightCells.Init(m_ComputeProgramId);
    m_Cgeneration.Init(m_ComputeProgramId);
    m_CgenStep.Init(m_ComputeProgramId);
    CHECK_RC(mglGetRC());

    // Create a program pipeline object so we can make both graphics
    // and compute stages "current" at the same time.
    glGenProgramPipelines(1, &m_ProgramPipelineId);
    glBindProgramPipeline(m_ProgramPipelineId);
    glUseProgramStages(m_ProgramPipelineId,
                       GL_VERTEX_SHADER_BIT,
                       m_GraphicsProgramId);
    glUseProgramStages(m_ProgramPipelineId,
                       GL_FRAGMENT_SHADER_BIT,
                       m_GraphicsProgramId);
    glUseProgramStages(m_ProgramPipelineId,
                       GL_COMPUTE_SHADER_BIT,
                       m_ComputeProgramId);
    CHECK_RC(mglGetRC());

    glBindAttribLocation(m_GraphicsProgramId, 0, "inPosition");

    return rc;
}

//------------------------------------------------------------------------------
RC GLComputeTest::LoadTextures ()
{
    RC rc;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glActiveTexture(GL_TEXTURE0_ARB);

    CHECK_RC(mglGetRC());

    glGenTextures(TxNUM_TEXTURES, m_TextureObjectIds);

    {
        // Load the read-only textures.
        const int txSize = 4;
        vector<GLuint> txImageLoadBuf(txSize * txSize);

        for (int ti = 0; ti < TxNUM_TEXTURES; ti++)
        {
            glActiveTexture(GL_TEXTURE0 + ti);
            glBindTexture(GL_TEXTURE_2D, m_TextureObjectIds[ti]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            for (int y = 0; y < txSize; ++y)
            {
                for (int x = 0; x < txSize; ++x)
                {
                    const int offset = x + y*txSize;
                    if (ti == TxDEAD_CELL)
                    {
                        // Random dark red pattern for dead-cell.
                        txImageLoadBuf[offset] = 0xff7f7fff &
                            m_pFpCtx->Rand.GetRandom();
                    }
                    else
                    {
                        // gray/white check for "live-cell" texture.
                        if ((x + y) & 1)
                            txImageLoadBuf[offset] = 0xffffffff;
                        else
                            txImageLoadBuf[offset] = 0xc0c0c0c0;
                    }
                }
                glTexImage2D(
                    GL_TEXTURE_2D,
                    0,
                    GL_RGBA8,
                    txSize,
                    txSize,
                    0,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    &txImageLoadBuf[0]);
            }
            CHECK_RC(mglGetRC());
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
RC GLComputeTest::CheckSimReversibility()
{
    RC rc;

    // We check when the sim has been evolved forwards, then backwards back
    // to gen 0.  So m_SimStep will be -1 and m_SimGeneration will be 0.
    MASSERT(m_SimStep == -1);
    MASSERT(m_SimGeneration == 0);

    // The final set of cells will be in the "bottom half" of the buffer.
    const int whichHalf = 0;

    // Read final cells from HW.
    vector<GLint> simCells;
    ReadSimCells(&simCells, whichHalf);

    
    // Compare to initial random config, treating all positive values as 1 and
    // all nonpositive values as 0.
    const size_t numCells = m_SimWorldAtGen0.size();
    MASSERT(numCells == simCells.size());

    vector<int> simCompareMap(numCells, static_cast<int>('-'));
    int errCount = 0;
    for (size_t ii = 0; ii < numCells; ii++)
    {
        if ((m_SimWorldAtGen0[ii] > 0) != (simCells[ii] > 0))
        {
            Printf(GetVerbosePrintPri(), "Error index:%04ld Gen0[]:%d vs cells[]:%d\n",
                   ii, m_SimWorldAtGen0[ii], simCells[ii]);
            errCount++;
            simCompareMap[ii] = '*';
        }
    }
    if (errCount)
    {
        // print stuff
        Tee::Priority pri = Tee::PriHigh;
        Printf(pri, "Compute program error: %d items miscomapared.\n", errCount);
        Print(pri, PrintAfter);
        Printf(pri, "Initial sim world:\n");
        PrintSimCells(pri, m_SimWorldAtGen0, ActualValues);
        Printf(pri, "Final sim world: (collapsing values to 0 or 1)\n");
        PrintSimCells(pri, simCells, As01);
        Printf(pri, "\n");
        
        PrintSimCells(GetVerbosePrintPri(), simCompareMap, AsSymbol);
        Printf(GetVerbosePrintPri(), "\n");
        return RC::GPU_STRESS_TEST_FAILED;
    }
    else
    {
        Printf(GetVerbosePrintPri(),
               "m_SimWorldAtGen0 and SimCells compared OK!. %ld cells compared\n",
               numCells);
    }
    return OK;
}

//------------------------------------------------------------------------------
RC GLComputeTest::DoEndLoop(UINT32 * pNextEndLoopMs)
{
    RC rc;
    if (m_ElapsedWallClockMs >= *pNextEndLoopMs)
    {
        rc = EndLoop(*pNextEndLoopMs);
        *pNextEndLoopMs += m_EndLoopPeriodMs;
    }
    return rc;
}
