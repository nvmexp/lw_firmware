/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2012,2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "opengl/modsgl.h"
#include "gpu/tests/gputest.h"

class GLGoldenSurfaces;

class GLVertexFS : public GpuTest
{
public:
    GLVertexFS();
    virtual ~GLVertexFS();

    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual RC InitFromJs();
    virtual void PrintJsProperties(Tee::Priority pri);

private:
    //mods stuff
    GLGoldenSurfaces *  m_pGSurfs;
    mglTestContext      m_mglTestContext;

    enum
    {
        TEX_MRT0=0,
        TEX_MRT1,
        TEX_MRT2,
        TEX_MRT3,
        TEX_MRT4,
        TEX_MRT5,
        TEX_MRT6,
        TEX_MRT7,
        TEX_DEPTH,
        TEX_NUM
    };

    enum
    {
        FBO_MRT=0,
        FBO_NUM
    };

    enum
    {
        PROG_VERT_MAIN = 0,
        PROG_FRAG_MAIN,
        PROG_VERT_COPY_FRONT,
        PROG_FRAG_COPY_FRONT,
        PROG_NUM
    };

    enum
    {
        VBO_VTX=0,
        VBO_IDX,
        VBO_STREAM_OUT1,
        VBO_STREAM_OUT2,
        VBO_STREAM_OUT3,
        VBO_NUM
    };

    enum
    {
        CONST_VERT_MAIN =0,
        CONST_FRAG_MAIN,
        CONST_NUM
    };

    enum
    {
        QUERY_FEEDBACK=0,
        QUERY_NUM
    };

    // OGL Handles
    GLuint m_TexIds   [TEX_NUM];
    GLuint m_FboIds   [FBO_NUM];   //
    GLuint m_ProgIds  [PROG_NUM];  //
    GLuint m_VboIds   [VBO_NUM];   //Vertex Buffer Objects
    GLuint m_ConstIds [CONST_NUM]; //UBO
    GLuint m_QueryIds [QUERY_NUM]; //UBO

    UINT32 m_NumVertices;
    UINT32 m_NumIndices;

    RC SetupGL();
    RC SetupTextures();
    RC SetupFramebuffers();
    RC SetupVertices();
    RC SetupPrograms();
    RC SetupConstants();

    void GenerateVertices   (vector<FLOAT32> &vbuff, UINT32 m_NumVertices);
//    void GenerateVerticesCPU(const vector<FLOAT32> &vbuff, const vector<UINT32> &ibuff);
    string GetVertexShader();

    typedef enum
    {
        CONFIG_STD1=0,
        CONFIG_STD2,
        CONFIG_STD3,
        CONFIG_COPY_FRONT,
        CONFIG_NUM
    } gpu_config_e;

    RC ReconfigureGPU(gpu_config_e);
    RC CheckBuffer();

    string GetFragmentProgram();
    string GetVertexProgram();

    //void DoMVP(vector<char> *vbuff);
    FLOAT32 m_WorldX;
    FLOAT32 m_WorldY;

    UINT32 m_ScreenWidth;
    UINT32 m_ScreenHeight;
    UINT32 m_MaxAttrib;

    //CPU callwlated results
    vector<FLOAT32> m_CpuCalcResult;
    vector<FLOAT32> m_ConstBuff;    //Stored cpuside for CpuResults

    bool DoCheck(UINT32 smid, UINT32 valid2, UINT32 idx, UINT32* p1, UINT32* p2, UINT32* p3);
    void BinHang(UINT32 smid);

    vector< UINT32> m_BitErrors;
    vector< bool >  m_HangDetect;

    UINT32 m_BadCBufMask;
    UINT32 m_WorstCBufMask;

public :
    SETGET_PROP(NumVertices,  UINT32);
    SETGET_PROP(NumIndices,   UINT32);
    SETGET_PROP(ScreenWidth,  UINT32);
    SETGET_PROP(ScreenHeight, UINT32);
    SETGET_PROP(BadCBufMask,  UINT32);
    SETGET_PROP(WorstCBufMask,UINT32);
};
