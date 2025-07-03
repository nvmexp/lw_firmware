/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "opengl/modsgl.h"
#include "gpu/tests/gpumtest.h"

class GLHistogram : public GpuMemTest
{
public:
    GLHistogram();
    virtual ~GLHistogram();

    virtual bool IsSupported();
    virtual RC InitFromJs();
    virtual void PrintJsProperties(Tee::Priority pri);
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
private:
    //mods stuff
    mglTestContext      m_mglTestContext;

    // GL handle enum section
    enum
    {
        FBO_HIST = 0, //draw histogram -> RTT
        FBO_CHECK,    //copy histogram -> RTT (only red)
        //FBO_COPY,     //copy histogram -> front
        FBO_NUM
    };
    enum
    {
        TEX_DRAW_COLOR = 0, //The actual historgram
        TEX_CHECK_COLOR,    // only red pixels for occ query
        TEX_DRAW_DEPTH,     // for FBO completeness
        TEX_NUM
    };

    enum
    {
        PROG_FRAG_HIST = 0,
        PROG_FRAG_CHECK,
        PROG_FRAG_COPY,
        PROG_NUM
    };
    enum
    {
        OCC_QUERY0=0,
        OCC_NUM
    };
    enum
    {
        CONST_FRAG_HIST=0,//histogram data goes here
        CONST_NUM
    };

    // OGL Handles
    LwU32 m_FboIds   [FBO_NUM];
    LwU32 m_TexIds   [TEX_NUM]; //variable length due to param m_extraTex
    LwU32 m_ProgIds  [PROG_NUM];
    LwU32 m_OccIds   [OCC_NUM];
    LwU32 m_ConstIds [CONST_NUM]; //UBO

    typedef enum
    {
        CONFIG_HIST=0,
        CONFIG_CHECK,
        CONFIG_COPY,
        CONFIG_OFF,
        CONFIG_NUM
    } gpu_config_e;

    UINT32          m_NumBins;

    vector<string>  m_Labels;
    vector<FLOAT64> m_Values;
    vector<FLOAT64> m_Dividers;
    vector<FLOAT64> m_Thresholds;
    vector<FLOAT32> m_ConstScratch;

    UINT32          m_LwrrBin;
    bool            m_UseDiv;
    bool            m_UseLog;
    UINT32          m_GraphOffset;
    UINT32          m_GraphScale;
    bool            m_Inited;
    UINT32          m_ScreenWidth;
    UINT32          m_ScreenHeight;

    RC SetupFramebuffers(UINT32);
    RC SetupPrograms    (UINT32);
    RC SetupConstants   (UINT32);
    RC SetupTextures    (UINT32);
    RC SetupGL();
    RC ReconfigureGPU(gpu_config_e mode);
    RC CheckBufferGPU();
    RC CheckBuffer();
    RC DrawFunction ();
    void UpdateConstBuffer();

    string GetFragmentProgramCopy();
    string GetFragmentProgramCopyRed();
    string GetFragmentProgramHist(UINT32 bins);

    RC ResizeBins();
    RC PrintStats();

public:

    //SETGET_PROP(NumBins); custom accessors
    UINT32 GetNumBins()
    {
        return m_NumBins;
    }
    RC     SetNumBins(UINT32 x)
    {
        m_NumBins = x;

        m_Labels      .clear();
        m_Values      .clear();
        m_Dividers    .clear();
        m_Thresholds  .clear();
        m_ConstScratch.clear();

        m_Labels      .resize(x,"");
        m_Values      .resize(x,0.0);
        m_Dividers    .resize(x,0.0);
        m_Thresholds  .resize(x);
        m_ConstScratch.resize(x*2+1);

        if(m_Inited)
        {
            ResizeBins();
        }
        return OK;
    }

    string  GetLwrrLabel()  {   return m_Labels    [m_LwrrBin]; }
    RC      SetLwrrLabel(string x)   { m_Labels    [m_LwrrBin]=x; return OK; }
    FLOAT64 GetLwrrVal()    {   return m_Values    [m_LwrrBin]; }
    RC      SetLwrrVal(FLOAT64 x)    { m_Values    [m_LwrrBin]=x; return OK; }
    FLOAT64 GetLwrrDiv()    {   return m_Dividers  [m_LwrrBin]; }
    RC      SetLwrrDiv(FLOAT64 x)    { m_Dividers  [m_LwrrBin]=x; return OK; }
    FLOAT64 GetLwrrThresh() {   return m_Thresholds[m_LwrrBin]; }
    RC      SetLwrrThresh(FLOAT64 x) { m_Thresholds[m_LwrrBin]=x; return OK; }

    SETGET_PROP(LwrrBin, UINT32);
    SETGET_PROP(UseDiv, bool);
    SETGET_PROP(UseLog, bool);
    SETGET_PROP(GraphOffset, UINT32);
    SETGET_PROP(GraphScale,  UINT32);

    RC DumpImage() ;

};
