/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "opengl/modsgl.h"
#include "gpu/tests/gpumtest.h"

class GLGoldenSurfaces;

class GLCombustTest : public GpuMemTest
{
public:
    GLCombustTest();
    virtual ~GLCombustTest();

    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual RC InitFromJs();
    virtual void PrintJsProperties(Tee::Priority pri);
    virtual void ReportErrors(UINT32 errs);

private:
    //mods stuff
    GLGoldenSurfaces *  m_pGSurfs;
    mglTestContext      m_mglTestContext;

    //param enums
    enum  //C-ROP MODE
    {
        CM_REPLACE=0,
        CM_BLEND,
        CM_XOR
    };
    enum //SHADER HASH MODE
    {
        SHM_NONE = 0,
        SHM_MOD_257,
    };
    enum
    {
        PULSE_DISABLED = 0,
        PULSE_TILE_MASK_USER = 8,
        PULSE_TILE_MASK_GPC = 9,
        PULSE_TILE_MASK_GPC_SHUFFLE = 10
    };

    // GL handle enum section
    enum
    {
        FBO_DRAW = 0,
        FBO_COMP,
        FBO_NUM
    };
    enum
    {
        TEX_DRAW_COLOR0 = 0,
        TEX_DRAW_COLOR1 ,
        TEX_DRAW_COLOR2 ,
        TEX_DRAW_COLOR3 ,
        TEX_DRAW_COLOR4 ,
        TEX_DRAW_COLOR5 ,
        TEX_DRAW_COLOR6 ,
        TEX_DRAW_COLOR7 ,
        TEX_DRAW_DEPTH ,
        TEX_COMP_COLOR ,
        TEX_NUM //+FBO_TEX_DRAW_n ,
    };

    enum
    {
        PROG_FRAG_MAIN = 0,
        PROG_VERT_MAIN,
        PROG_VERT_MAIN_SIMPLE,
        PROG_FRAG_COPY,   //used for  PROG_FRAG_COPY_8_SMID_RGBA, as well
        PROG_FRAG_COPY_16_SMID_RG,
        PROG_FRAG_COPY_16_SMID_BA,
        PROG_FRAG_COPY_32_SMID_R,
        PROG_FRAG_COPY_32_SMID_G,
        PROG_FRAG_COPY_32_SMID_B,
        PROG_FRAG_COPY_32_SMID_A,
        PROG_NUM
    };
    enum
    {
        VBO_VTX=0,  //! Index to vertex_buffer_objects, vertex data.
        VBO_IDX,  //! Index to vertex_buffer_objects, index data
        VBO_NUM
    };
    enum
    {
        OCC_QUERY0=0,
        OCC_NUM
    };
    enum
    {
        CONST_FRAG_MAIN=0,
        CONST_FRAG_SMIDMAP=1,
        CONST_FRAG_ENABLEDGPCS=2,
        CONST_NUM
    };

    enum
    {
        FS_MODE_NO_FS=0,
        FS_MODE_UP_TO_8_SMID,
        FS_MODE_UP_TO_16_SMID,
        FS_MODE_UP_TO_32_SMID,
    };
    int GetMrtCountForFsMode() {
        switch(m_FsMode) {
        case FS_MODE_NO_FS         : return 1; break;
        case FS_MODE_UP_TO_8_SMID  : return 8; break;
        case FS_MODE_UP_TO_16_SMID : return 8; break;
        case FS_MODE_UP_TO_32_SMID : return 8; break;
        }
        return 1;
    };
    int GetSmidCountForFsMode() {
        switch(m_FsMode) {
        case FS_MODE_NO_FS         : return 1;  break;
        case FS_MODE_UP_TO_8_SMID  : return 8;  break;
        case FS_MODE_UP_TO_16_SMID : return 16; break;
        case FS_MODE_UP_TO_32_SMID : return 32; break;
        }
        return 1;
    };
    int GetBaseConfigCopyForFsMode() {
        switch(m_FsMode) {
        case FS_MODE_NO_FS         : return CONFIG_COPY;  break;
        case FS_MODE_UP_TO_8_SMID  : return CONFIG_COPY_MODE_8_SMID_0;  break;
        case FS_MODE_UP_TO_16_SMID : return CONFIG_COPY_MODE_16_SMID_0; break;
        case FS_MODE_UP_TO_32_SMID : return CONFIG_COPY_MODE_32_SMID_0; break;
        }
        return CONFIG_COPY;
    };

    // OGL Handles
    LwU32 m_FboIds   [FBO_NUM];
    vector<LwU32> m_TexIds; //variable length due to param m_extraTex
    LwU32 m_ProgIds  [PROG_NUM];
    LwU32 m_VboIds   [VBO_NUM];    //!< Vertex Buffer Objects
    LwU32 m_OccIds   [OCC_NUM];
    LwU32 m_ConstIds [CONST_NUM]; //UBO

    // implied by params
    int                 m_NumVertices;
    int                 m_NumIndices;

    // scalar to compare to when considering what error code to return
    UINT32              m_PixelErrorTol;

    RC SetupGL();
    RC SetupFramebuffers();
    RC SetupVertices();
    RC SetupTextures();
    RC SetupPrograms();
    RC SetupConstants();
    RC SetupPulseModeConstants();

    typedef enum
    {
        CONFIG_DRAW=0,

        CONFIG_COPY,

        CONFIG_COPY_MODE_8_SMID_0,
        CONFIG_COPY_MODE_8_SMID_1,
        CONFIG_COPY_MODE_8_SMID_2,
        CONFIG_COPY_MODE_8_SMID_3,
        CONFIG_COPY_MODE_8_SMID_4,
        CONFIG_COPY_MODE_8_SMID_5,
        CONFIG_COPY_MODE_8_SMID_6,
        CONFIG_COPY_MODE_8_SMID_7,

        CONFIG_COPY_MODE_16_SMID_0,
        CONFIG_COPY_MODE_16_SMID_1,
        CONFIG_COPY_MODE_16_SMID_2,
        CONFIG_COPY_MODE_16_SMID_3,
        CONFIG_COPY_MODE_16_SMID_4,
        CONFIG_COPY_MODE_16_SMID_5,
        CONFIG_COPY_MODE_16_SMID_6,
        CONFIG_COPY_MODE_16_SMID_7,
        CONFIG_COPY_MODE_16_SMID_8,
        CONFIG_COPY_MODE_16_SMID_9,
        CONFIG_COPY_MODE_16_SMID_10,
        CONFIG_COPY_MODE_16_SMID_11,
        CONFIG_COPY_MODE_16_SMID_12,
        CONFIG_COPY_MODE_16_SMID_13,
        CONFIG_COPY_MODE_16_SMID_14,
        CONFIG_COPY_MODE_16_SMID_15,

        CONFIG_COPY_MODE_32_SMID_0,
        CONFIG_COPY_MODE_32_SMID_1,
        CONFIG_COPY_MODE_32_SMID_2,
        CONFIG_COPY_MODE_32_SMID_3,
        CONFIG_COPY_MODE_32_SMID_4,
        CONFIG_COPY_MODE_32_SMID_5,
        CONFIG_COPY_MODE_32_SMID_6,
        CONFIG_COPY_MODE_32_SMID_7,
        CONFIG_COPY_MODE_32_SMID_8,
        CONFIG_COPY_MODE_32_SMID_9,
        CONFIG_COPY_MODE_32_SMID_10,
        CONFIG_COPY_MODE_32_SMID_11,
        CONFIG_COPY_MODE_32_SMID_12,
        CONFIG_COPY_MODE_32_SMID_13,
        CONFIG_COPY_MODE_32_SMID_14,
        CONFIG_COPY_MODE_32_SMID_15,
        CONFIG_COPY_MODE_32_SMID_16,
        CONFIG_COPY_MODE_32_SMID_17,
        CONFIG_COPY_MODE_32_SMID_18,
        CONFIG_COPY_MODE_32_SMID_19,
        CONFIG_COPY_MODE_32_SMID_20,
        CONFIG_COPY_MODE_32_SMID_21,
        CONFIG_COPY_MODE_32_SMID_22,
        CONFIG_COPY_MODE_32_SMID_23,
        CONFIG_COPY_MODE_32_SMID_24,
        CONFIG_COPY_MODE_32_SMID_25,
        CONFIG_COPY_MODE_32_SMID_26,
        CONFIG_COPY_MODE_32_SMID_27,
        CONFIG_COPY_MODE_32_SMID_28,
        CONFIG_COPY_MODE_32_SMID_29,
        CONFIG_COPY_MODE_32_SMID_30,
        CONFIG_COPY_MODE_32_SMID_31,

        CONFIG_HIST,
        CONFIG_RESET,
        CONFIG_NUM
    } gpu_config_e;

    void SetupScissor(FLOAT64 drawPct);
    void UpdateTileMask();
    RC ReconfigureGPU(gpu_config_e);
    RC DrawFunction();
    RC CheckBuffer();
    void DumpImage(const char *filename);

    // programs
    string GetFragmentProgram();
    string GetVertexProgram();
    string GetVertexProgramSimple();
    string GetFragmentProgramHist();
    string GetFragmentProgramCopy() ;
    string GetFragmentProgramCopyRG() ;
    string GetFragmentProgramCopyBA() ;
    string GetFragmentProgramCopyR() ;
    string GetFragmentProgramCopyG() ;
    string GetFragmentProgramCopyB() ;
    string GetFragmentProgramCopyA() ;

    // Private vertex format
    struct vattr_t
    {
        float x, y, z;      //coordinates
        float nx, ny, nz;   //normals
        float tcx, tcy;     //texcoordinates
    };
    //functions used to init vertex data
    void Torus(vector<char> &vbuff, vector<char> &ibuff, int* lW, int* nI);
    void DoMVP(vector<char> *vbuff);
    void MatrixMultiply(float*,float*,float*);

    //temp vars for worldview
    FLOAT32 m_WorldX;
    FLOAT32 m_WorldY;

    // setup for interaction with T117-style clock pulsing
    bool m_FreqSweep;
    RC CheckBufferGPU();

    RC DoEndLoop(UINT32* pNextEndLoopMs, double elapsed);

    // test runtime specific stuff
    UINT32              m_LoopMs;
    bool                m_KeepRunning;
    UINT32              m_DrawRepeats;
    mglTimerQuery*      m_Timers;
    GLint               m_WindowRect[4];
    GLint               m_ScissorRect[4];
    UINT32              m_HighestGPCIdx;
    vector<GLubyte>     m_ScreenBuf;

    // params
    GLenum              m_LwllToggle;
    UINT32              m_ExtraInstr;
    UINT32              m_ExtraTex;
    UINT32              m_ScreenWidth;
    UINT32              m_ScreenHeight;
    UINT32              m_NumXSeg;
    UINT32              m_NumTSeg;
    FLOAT64             m_TorusTubeDia;
    FLOAT64             m_TorusHoleDia;
    bool                m_EnableLwll;
    UINT32              m_BlendMode;
    UINT32              m_ShaderHashMode;
    bool                m_DepthTest;
    bool                m_UseMM;
    bool                m_SimpleVS;
    UINT32              m_EndLoopPeriodMs;
    FLOAT64             m_DrawPct;
    UINT32              m_PulseMode;
    UINT32              m_TileMask;
    UINT32              m_NumActiveGPCs;
    bool                m_SkipDrawChecks;
    GpuUtility::ValueStats m_GPCShuffleStats;

    UINT32              m_FailingPixels;
    UINT32              m_FramesDrawn;
    UINT32              m_FsMode;
    UINT32              m_Watts;
    UINT32              m_DumpFrames;       // Number of frames to dump .png
                                            // Best use odd "DrawRepeats" values

protected:
    virtual void PowerControlCallback(LwU32 milliWatts);

public:
    SETGET_PROP(LoopMs,         UINT32);
    SETGET_PROP(KeepRunning,    bool);

    //params
    SETGET_PROP(ExtraInstr,     UINT32);
    SETGET_PROP(ExtraTex,       UINT32);
    SETGET_PROP(ScreenWidth,    UINT32);
    SETGET_PROP(ScreenHeight,   UINT32);
    SETGET_PROP(NumXSeg,        UINT32);
    SETGET_PROP(NumTSeg,        UINT32);
    SETGET_PROP(TorusTubeDia,   FLOAT64);
    SETGET_PROP(TorusHoleDia,   FLOAT64);
    SETGET_PROP(EnableLwll,     bool);
    SETGET_PROP(BlendMode,      UINT32);
    SETGET_PROP(ShaderHashMode, UINT32);
    SETGET_PROP(DepthTest,      bool);
    SETGET_PROP(UseMM,          bool);
    SETGET_PROP(PixelErrorTol,  UINT32);
//    SETGET_PROP(ForceFBO,       bool);
    SETGET_PROP(SimpleVS,       bool);
    SETGET_PROP(EndLoopPeriodMs,UINT32);
    SETGET_PROP(DrawPct,        FLOAT64);
    SETGET_PROP(PulseMode,      UINT32);
    SETGET_PROP(TileMask,       UINT32);
    SETGET_PROP(NumActiveGPCs,  UINT32);
    SETGET_PROP(SkipDrawChecks, bool);
    SETGET_PROP(FreqSweep,      bool);
    SETGET_PROP(DrawRepeats,    UINT32);
    SETGET_PROP(FailingPixels,  UINT32);
    SETGET_PROP(FramesDrawn,    UINT32);
    SETGET_PROP(FsMode,         UINT32);
    SETGET_PROP(Watts,          UINT32);
    SETGET_PROP(DumpFrames,     UINT32);
};
