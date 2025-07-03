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

/**
 * @file glstress.h
 * class GLStressTest.
 *
 * GLStressTest is intended to stress power & heat.
 * https://wiki.lwpu.com/engwiki/index.php/MODS/Information_on_specific_GPU_tests/GLStress
 */

#ifndef INCLUDED_GLSTRESS_H
#define INCLUDED_GLSTRESS_H

#ifndef INCLUDED_MODSGL_H
#include "opengl/modsgl.h"
#endif

#ifndef INCLUDED_PATTERNCLASS_H
#include "core/utility/ptrnclss.h"
#endif

#ifndef INCLUDED_GPUMTEST_H
#include "gpu/tests/gpumtest.h"
#endif

#ifndef INCLUDED_FPICKER_H
#include "core/include/fpicker.h"
#endif

#include "lwdiagutils.h"

class ElpgBackgroundToggle;

class GLStressTest : public GpuMemTest
{
public:
    GLStressTest();
    virtual ~GLStressTest();
    virtual RC Setup();
    virtual bool IsSupported();
    virtual RC Run();
    virtual RC Cleanup();
    virtual RC InitFromJs();
    virtual void PrintJsProperties(Tee::Priority pri);

    //! True if this test exercises all subdevices at once, false if this
    //! test should be run once per subdevice to get full coverage.
    virtual bool GetTestsAllSubdevices() { return true; }

protected:
    virtual void PowerControlCallback(LwU32 milliWatts);

    // Default vertex format: 16 bytes per vertex
    struct vtx_s4_s3
    {
        GLshort pos[4];
        GLshort norm[3];
    };
    struct vtx_f4
    {
        GLfloat pos[4];
    };
    union vtx
    {
        vtx_s4_s3 s4s3;
        vtx_f4    f4;
    };

    // Larger vertex format: 32 bytes.
    struct vtx_f3_f3_f2
    {
        GLfloat pos[3];
        GLfloat norm[3];
        GLfloat tex0[2];
    };

    enum
    {
        //! Index to FboIds, the offscreen surface we render to.
        FBO_DRAW = 0,
        //! Index to FboIds, the reference surface we compare to when checking.
        //! It is cleared to the specified clear value only.
        FBO_REFERENCE,
        FBO_NUM
    };
    enum
    {
        //! Index to Fbo textures, draw fbo color surface
        FBO_TEX_DRAW_COLOR = 0,
        //! Index to Fbo textures, reference fbo color surface
        FBO_TEX_REF_COLOR,
        // Index to Fbo textures, shared draw/ref z/s surface
        FBO_TEX_DEPTH_STENCIL,
        FBO_TEX_NUM
    };
    enum
    {
        //! Index to vertex_buffer_objects, vertex data.
        VBO_VTX,
        //! Index to vertex_buffer_objects, index data
        VBO_IDX,
        VBO_NUM
    };
    enum
    {
        CONST_SMIDMAP = 0,
        CONST_ENABLEDGPCS = 1,
        CONST_NUM
    };
    enum
    {
        //! Number of timer_query objects 2 for ping-pong + 1 for checkBuffer.
        TIMER_PING = 0
        ,TIMER_PONG = 1
        ,TIMER_CHKBUF = 2
        ,TIMER_NUM = 3
    };
    enum
    {
        PULSE_DISABLED = 0,
        PULSE_TILE_MASK_USER = 8,
        PULSE_TILE_MASK_GPC = 9,
        PULSE_TILE_MASK_GPC_SHUFFLE = 10
    };

    // Properties settable from JavaScript:
    UINT32          m_RuntimeMs;        // total test runtime in seconds. If set take priority over
                                        // m_LoopMs.
    UINT32          m_LoopMs;           // time to loop, in milliseconds
    UINT32          m_BufferCheckMs;    // time interval to check the final buffer.
    UINT32          m_FullScreen;       // full screen (vs window)
    bool            m_DoubleBuffer;     // do alternate front/back rendering.
    PatternClass    m_PatternClass;
    bool            m_KeepRunning;
    int             m_Width;            // in pixels, ignoring FSAA stuff
    int             m_Height;           // in pixels, ignoring FSAA stuff
    int             m_Depth;            // color depth in bits, counting alpha
    UINT32          m_ColorSurfaceFormat; // Color surface format
    int             m_ZDepth;           // z depth in bits, counting stencil
    int             m_TxSize;           // size of textures in texels (w and h).
    int             m_NumTextures;      // how many different textures to draw
    double          m_TxD;              // log2(texels per pixel), affects tex coord setup
    double          m_PpV;              // pixels per vertex (controls prim size).
    bool            m_Rotate;           // rotate all rendering 180%
    bool            m_UseMM;            // use mipmapping
    bool            m_TxGenQ;           // generate texture Q coords
    bool            m_Use3dTextures;    // use 3d textures
    unsigned int    m_NumLights;        // enable lighting if > 0 (max 8)
    bool            m_SpotLights;       // use spotlights (else positional lights)
    bool            m_Normals;          // send normals
    bool            m_BigVertex;        // use the 8-float32 "big" vertex format
    bool            m_Stencil;          // enable stencil test
    bool            m_Ztest;            // enable depth testing
    bool            m_TwoTex;           // turn on a second texture unit
    bool            m_Fog;              // if true, use fog.
    bool            m_LinearFog;        // if true, use linear fog, else exponential.
    bool            m_BoringXform;      // use a straight-through x.y == pixel location transform
    bool            m_DrawLines;        // draw lines instead of quad-strips (wireframe).
    bool            m_DumpPngOnError;   // dump image of failing surface
    bool            m_SendW;            // Send a 4-tuple position, not 3-tuple
    bool            m_DoGpuCompare;     // Check for errors using GPU's shaders
    bool            m_AlphaTest;        // Enable alpha-test
    bool            m_OffsetSecondTexture; // Offset the second texture's coordinates.
    int             m_ViewportXOffset;  //
    int             m_ViewportYOffset;  //
    vector<string>  m_TxImages;         // image filenames to use for textures
    UINT32          m_ClearColor;       // Clear color in LE_ARGB8 format.
    INT32           m_GpioToDriveInDelay;  // Gpio pin to toggle in MaxFps mode, if present.
    double          m_StartFreq;
    UINT32          m_DutyPct;
    double          m_FreqSweepOctavesPerSecond;
    bool            m_BeQuiet;          // Suppress error messages, etc.
    UINT32          m_ForceErrorCount;  // for testing error reporting
    bool            m_ContinueOnError;  // for letting DoGpuCompare run to completion
    bool            m_EnablePowerWait;    // Enable power wait
    bool            m_TegraPowerWait;     // Use power wait implementation on CheetAh (not in JS)
    bool            m_EnableDeepIdle;     // Enable deep idle when doing power waits
    bool            m_ToggleELPGOlwideo;  // Toggle ELPG on video in the background
    UINT32          m_ToggleELPGGatedTimeMs; // When toggling, time in ms that video
                                             // will remain gated
    UINT32          m_PowerWaitTimeoutMs; // Timeout in ms for power waits
    UINT32          m_DeepIdleTimeoutMs;  // Timeout in ms for deep idle entry
    bool            m_ShowDeepIdleStats;  // Show Deep Idle statistics for each transition
    bool            m_PrintPowerEvents;   // Print the number of power events that oclwrred
                                          // during the test
    UINT32          m_EndLoopPeriodMs;    // Time in ms between calls to EndLoop
    UINT32          m_FramesPerFlush;     // Number of frames between glFlush calls
    bool            m_UseFbo;
    bool            m_GenUniqueFilenames = false;

    Goldelwalues *  m_pGolden;          // Used to skip error checking whenever we need to
    mglTestContext  m_mglTestContext;   // Per-test OpenGL context.
    FancyPicker::FpContext *m_pFpCtx;
    FancyPickerArray       *m_pPickers;
    bool            m_UseTessellation;  // Use the GL_LW_tessellation_program5 extension.
    UINT32          m_DumpFrames;       // Number of frames to dump .png for.
    bool            m_Blend;            // Use blend rather than XOR.
    bool            m_Torus;            // Draw a torus (doughnut) instead of two planes.
    bool            m_TorusMajor;       // Draw in "major" strip order (else minor).
    bool            m_LwllBack;         // Lwll backfacing triangles.
    double          m_TorusHoleDiameter;// Torus hole diameter.
    double          m_TorusDiameter;    // Torus outer diameter.
    double          m_DrawPct;          // 100 = draw full screen, 1 = draw only center
    UINT32          m_Watts;            // Target power consumption
    UINT32          m_AllowedMiscompares;
    bool            m_SmoothLines;      // Draw smooth lines
    bool            m_StippleLines;     // Draw lines with a stipple pattern
    UINT32          m_PulseMode;
    UINT32          m_TileMask;
    UINT32          m_NumActiveGPCs;
    bool            m_SkipDrawChecks;
    GpuUtility::ValueStats m_GPCShuffleStats;

    RC GetDeepIdlePState(UINT32 *pPStateNum);

    enum ProgType
    {
        Vtx,
        TessCtrl,
        TessEval,
        Frag,

        ProgType_NUM
    };

    struct ProgProp
    {
        GLfloat value[4];
        ProgProp() { value[0] = value[1] = value[2] = value[3] = 0.0F; }
    };
    typedef map<UINT32, ProgProp> ProgProps;

    struct GpuProg
    {
        const char *    m_Name;
        const char *    m_Extension;
        GLenum          m_Enum;
        string          m_Text;
        GLuint          m_Id;
        ProgProps       m_UserProps;

        GpuProg
        (
            const char * name,
            const char * extension,
            GLenum def,
            string text
        )
         :  m_Name(name),
            m_Extension(extension),
            m_Enum(def),
            m_Text(text),
            m_Id(0)
        {
        }
        GpuProg()
         : m_Name(""), m_Extension(""), m_Enum(0), m_Text(""), m_Id(0)
        {
        }

        bool IsSet()    { return !m_Text.empty(); }
        bool IsLoaded() { return m_Id != 0; }
        RC   Load();
        void Unload();
        void Setup();
        void Suspend();
        void Resume();
    };
    GpuProg m_GpuProgs[ProgType_NUM];

    // Current rendering info.
    GLuint          m_Rows;
    GLuint          m_Cols;
    GLuint          m_XTessRate;
    GLuint          m_CenterLeftCol;
    GLuint          m_CenterRightCol;
    GLuint          m_CenterBottomRow;
    GLuint          m_CenterTopRow;
    GLuint          m_VxPerPlane;
    GLuint          m_NumVertexes;  // number of vertices needs to draw both near/far planes
    GLuint          m_IdxPerPlane;
    GLuint          m_NumIndexes;
    GLuint *        m_TextureObjectIds;
    GLuint          m_FboIds[FBO_NUM];
    GLuint          m_FboTexIds[FBO_TEX_NUM];
    GLuint          m_GpuCompareFragProgId;
    GLuint          m_GpuCopyFragProgId;
    GLuint          m_GpuCompareOcclusionQueryId;
    vector<GLubyte> m_ScreenBuf;
    GLint           m_WindowRect[4];
    GLint           m_ScissorRect[4];
    mglTimerQuery * m_Timers;
    GLuint          m_Vbos[VBO_NUM];    //!< Vertex Buffer Objects
    GLuint          m_ConstIds[CONST_NUM];
    vector<GLubyte> m_ClearColorPixel;         //!< enough space to hold 1 pixel of the clear color>
    // Counters used during Run, all times in seconds.
    double          m_ElapsedGpu;       //!< total GPU runtime (not test runtime)
    double          m_ElapsedRun;   //!< total test runtime in seconds
    double          m_ElapsedGpuIdle; //!< time when we know the GPU is idle.
    UINT64          m_FrameCount;       //!< Frames drawn so far
    GLuint          m_StripCount;       //!< Strips drawn so far in each quad-frame
    GLuint          m_Plane;
    GLuint          m_PowerWaitTrigger;
    UINT32          m_PowerWaitDelayMs; //!< Delay time in ms after power gating
    UINT64          m_FramesPerCheck;   //!< From Golden.SkipCount (default=100)
    UINT32          m_HighestGPCIdx;

    // Stuff remembered from the last run of GLStressTest.Run().
    double m_RememberedFps;
    double m_RememberedPps;
    double m_RememberedFrames;
    double m_RememberedPixels;
    bool   m_PrintedHelpfulMessage;
    bool   m_MemLocsAreMisleading;
    bool   m_ReportMemLocs;
    const char * m_CantReportMemLocsReason;
    Surface2D * m_pRawPrimarySurface;
    UINT32 m_Xbloat;
    UINT32 m_Ybloat;

    ElpgBackgroundToggle *m_pElpgToggle;  //!< Used to toggle ELPG in the background

    RC GetDefaultArgs();
    RC SetupTileMasking();
    RC Allocate();
    RC SetupDrawState();
    RC Clear();
    RC CheckFinalSurface(bool bCheckBothBuffers);
    RC CompleteQuadFrame();
    RC Draw(GLuint numStrips, mglTimerQuery * pTimer);
    RC WaitForPowerGate();
    void CommonBeginFrame();
    void BeginFrame();
    void QuadStrip();
    void Triangles(GLuint rows);
    void Patch(GLuint rows);
    RC EndFourthFrame();
    void UpdateTileMask();
    RC Free();
    RC CheckBuffer();
    RC   LoadTextures();
    RC   SuspendDrawState();
    void RestoreDrawState();
    void SetupViewport();
    void SetupScissor(double drawPct);
    RC CopyDrawFboToFront ();          // copy contents of the draw buffer to the primary surface
    GLuint CopyBadPixelsToFront();  // copy the miscompared pixels to the primary surface
    void DumpImage(bool readZS, const char * name);

    static const char * s_DefaultTessCtrlProg;
    static const char * s_DefaultTessEvalProg;

private:
    UINT32 m_ActualColorSurfaceFormat; // Color surface format
    ElpgOwner m_ElpgOwner;
    PStateOwner m_PStateOwner;
    void CalcGeometrySizePlanes();
    void CalcGeometrySizeTorus();
    void GenGeometryPlanes();
    void GenGeometryTorus();
    vtx_f3_f3_f2 GenTorusVertex(GLuint majorIdx,GLuint minorIdx, bool bump);
    void StoreVertex(const vtx_f3_f3_f2 & bigVtx, UINT32 index, void* buf);
    void AddJsonFailLoop();
    RC DoEndLoop(UINT32 * pNextEndLoopMs);
    void UpdateTestProgress() const;
    // Speedup variabled
    double m_TicksPerSec;
    void PrintNonJsProperties();
    string GetTimestampedPngFilename(UINT64 errCnt);

public:
    SETGET_PROP(RuntimeMs, UINT32);
    SETGET_PROP(BufferCheckMs, UINT32);
    SETGET_PROP(LoopMs, UINT32);
    SETGET_PROP(FullScreen, UINT32);
    SETGET_PROP(DoubleBuffer, bool);

    void SelectPatterns(vector<UINT32> * pPatterns)
    {
        m_PatternClass.SelectPatterns(pPatterns);
    }
    void SelectPatternSet(UINT32 pttrnSelection)
    {
        m_PatternClass.SelectPatternSet(PatternClass::PatternSets(pttrnSelection));
    }
    void GetSelectedPatterns(vector<UINT32> * pPatterns)
    {
        m_PatternClass.GetSelectedPatterns(pPatterns);
    }
    SETGET_PROP(BeQuiet, bool);
    SETGET_PROP(KeepRunning, bool);
    SETGET_PROP(ColorSurfaceFormat, UINT32);
    SETGET_PROP(TxSize, UINT32);
    SETGET_PROP(NumTextures, UINT32);
    SETGET_PROP(TxD, double);
    SETGET_PROP(PpV, double);
    SETGET_PROP(Rotate, bool);
    SETGET_PROP(UseMM, bool);
    SETGET_PROP(TxGenQ, bool);
    SETGET_PROP(Use3dTextures, bool);
    SETGET_PROP(NumLights, UINT32);
    SETGET_PROP(SpotLights, bool);
    SETGET_PROP(Normals, bool);
    SETGET_PROP(BigVertex, bool);
    SETGET_PROP(Stencil, bool);
    SETGET_PROP(Ztest, bool);
    SETGET_PROP(TwoTex, bool);
    SETGET_PROP(Fog, bool);
    SETGET_PROP(LinearFog, bool);
    SETGET_PROP(BoringXform, bool);
    SETGET_PROP(DrawLines, bool);
    SETGET_PROP(DumpPngOnError, bool);
    SETGET_PROP(SendW, bool);
    SETGET_PROP(DoGpuCompare, bool);
    SETGET_PROP(UseFbo, bool);
    SETGET_PROP(AlphaTest, bool);
    SETGET_PROP(OffsetSecondTexture, bool);
    SETGET_PROP(ViewportXOffset, UINT32);
    SETGET_PROP(ViewportYOffset, UINT32);
    SETGET_PROP(GenUniqueFilenames, bool);

#define PROG_STRING_SETGET_PROP(n) \
    string Get##n##Prog()         { return m_GpuProgs[n].m_Text; } \
    RC     Set##n##Prog(string s) { m_GpuProgs[n].m_Text = s; return OK; }

    PROG_STRING_SETGET_PROP(Frag)
    PROG_STRING_SETGET_PROP(Vtx)
    PROG_STRING_SETGET_PROP(TessCtrl)
    PROG_STRING_SETGET_PROP(TessEval)

    void SetTxImages(vector<string>* psv) { m_TxImages = *psv; }
    vector<string>* GetTxImages() { return &m_TxImages; }
    SETGET_PROP(ClearColor, UINT32);
    SETGET_PROP(GpioToDriveInDelay, INT32);
    SETGET_PROP(StartFreq, double);
    SETGET_PROP(DutyPct, UINT32);
    SETGET_PROP(FreqSweepOctavesPerSecond, double);
    SETGET_PROP(ForceErrorCount, UINT32);
    SETGET_PROP(ContinueOnError, bool);
    SETGET_PROP(EnablePowerWait, bool);
    SETGET_PROP(EnableDeepIdle, bool);
    SETGET_PROP(ToggleELPGOlwideo, bool);
    SETGET_PROP(ToggleELPGGatedTimeMs, UINT32);
    SETGET_PROP(PowerWaitTimeoutMs, UINT32);
    SETGET_PROP(DeepIdleTimeoutMs, UINT32);
    SETGET_PROP(ShowDeepIdleStats, bool);
    SETGET_PROP(PrintPowerEvents, bool);
    SETGET_PROP(EndLoopPeriodMs, UINT32);
    SETGET_PROP(FramesPerFlush, UINT32);
    SETGET_PROP(UseTessellation, bool);
    SETGET_PROP(DumpFrames, UINT32);
    SETGET_PROP(Blend, bool);
    SETGET_PROP(Torus, bool);
    SETGET_PROP(TorusMajor, bool);
    SETGET_PROP(LwllBack, bool);
    SETGET_PROP(TorusHoleDiameter, double);
    SETGET_PROP(TorusDiameter, double);
    SETGET_PROP(DrawPct, double);
    SETGET_PROP(Watts, UINT32);
    SETGET_PROP(AllowedMiscompares, UINT32);
    SETGET_PROP(SmoothLines, bool);
    SETGET_PROP(StippleLines, bool);
    SETGET_PROP(PulseMode, UINT32);
    SETGET_PROP(TileMask, UINT32);
    SETGET_PROP(NumActiveGPCs, UINT32);
    SETGET_PROP(SkipDrawChecks, bool);

    double GetFrames() const  { return m_RememberedFrames; }
    double GetPixels() const  { return m_RememberedPixels; }
    double GetElapsedGpu() const { return m_ElapsedGpu; }
    double GetElapsedRun() const { return m_ElapsedRun; }
    double GetElapsedGpuIdle() const { return m_ElapsedGpuIdle; }

    double GetFps() const     { return m_RememberedFps; }
    double GetPps() const     { return m_RememberedPps; }

    RC SetProgParams
    (
        const string& pgmType,
        UINT32 idx,
        const JsArray& vals
    );

    void PrintStatus();
    RC   RunJsWrapper();
    RC   SetDefaultsForPicker(UINT32 Idx);
};

#endif // INCLUDED_GLSTRESS_H
