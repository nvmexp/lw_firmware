/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "opengl/modsgl.h"
#include "gpu/tests/gpumtest.h"
#include "random.h"

#if 0
#define CHECK_OPENGL_ERROR \
{ GLenum error; \
  while ( (error = glGetError()) != GL_NO_ERROR) { \
    printf( "OpenGL ERROR: %s\nCHECK POINT: %s (line %d)\n", \
      gluErrorString(error), __FILE__, __LINE__ ); \
  } \
}
#else
#define CHECK_OPENGL_ERROR
#endif

#ifdef GLINFERNO_CTOR

#endif

#ifdef GLINFERNO_SETGET
#endif

#ifdef GLINFERNO_DEFN

#endif

#ifdef GLINFERNO_JS
#define GLINFERNO_OPTS(A,B,C,D) \
CLASS_PROP_READWRITE(GLInfernoTest, A, B, D);
#define GLINFERNO_GLOBALS(A,B,C,D)
#endif

//Fail RTTI -- Just picture a really ugly, dyncast based macro here
#ifdef GLINFERNO_PRINT
#define GLINFERNO_OPTS(A,B,C,D)
#define GLINFERNO_GLOBALS(A,B,C,D)
#endif

typedef unsigned short GLhalfLW;

class GLInfernoTest : public GpuMemTest
{
public:
    GLInfernoTest();
    virtual ~GLInfernoTest();

    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual RC InitFromJs();
    virtual void PrintJsProperties(Tee::Priority pri);
    virtual void ReportErrors(UINT32 errs);

private:
    //mods stuff
    mglTestContext      m_mglTestContext;

    enum
    {
        PULSE_DISABLED = 0,
        PULSE_TILE_MASK_USER = 8,
        PULSE_TILE_MASK_GPC = 9,
        PULSE_TILE_MASK_GPC_SHUFFLE = 10
    };

    Random& m_Rand;
    UINT32 m_HighestGPCIdx;
    GpuUtility::ValueStats m_GPCShuffleStats;
    vector<GLubyte> m_ScreenBuf;
#define GLINFERNO_OPTS(A,B,C,D)    B m_##A;
#define GLINFERNO_GLOBALS(A,B,C,D) B m_##A;
#include "glinferno_vars.h"
#undef GLINFERNO_OPTS
#undef GLINFERNO_GLOBALS

    //DrawFunction
    RC DoEndLoop(UINT32 * pNextEndLoopMs, double l_Elapsed);
    RC DrawOnce();

    //setup helper functions
    unsigned int IeeeToS10E5Trunc(unsigned int ieee);
    void LwFloat32ToFloat32Raw(const GLfloat &from, GLuint &to);
    void LwFloat32ToFloat16Raw(const GLfloat &from, GLhalfLW &to);
    void PrintShaderInfoLog(GLuint obj);
    void PrintProgramInfoLog(GLuint obj);
    void MyFillTexture(unsigned int texw, unsigned int texh, GLubyte* image);
    float LwFloatRand(GLfloat min, GLfloat max);
    void GetTextureRatios(float *sxRatio,float *tyRatio,float *maxS,float *maxT);
    int SetupFullscreenPrim(int i, GLfloat* ptr);
    int SetupFrustumLwllPrims(GLfloat* ptr, unsigned int FLOAT_PER_TRI);
    int SetupBackfaceLwllPrims(GLfloat* ptr, unsigned int FLOAT_PER_TRI);
    int SetupZLwllPrims(GLfloat* ptr, int FLOAT_PER_TRI);
    int ZLwllStripPrimCount();
    void SetupFramebuffer(GLuint& fboid, GLuint& colorid, GLuint& depthid);
    void SetupDumpFramebuffer(GLuint& fboid, GLuint& colorid);
    RC ErrorCheckFramebuffer(void);
    void UpdateGpcShuffle();
    void DumpImage(const char *filename);

    //setup main functions
    int  SetupZLwllStripPrims(GLfloat*,int);
    void SetupVertices();
    void SetupTextures();
    RC SetupShaders();
    RC SetupPulseModeConstants();

    //Global test properties
    UINT32 NUM_TRI;
    UINT32 VERTS_PER_TRI;
    UINT32 VERTICES;
    UINT32 FLOAT_PER_VERTEX;
    UINT32 zlwll_strip_ptr;
    UINT32 fullscreen_ptr;
    UINT32 lwll_ptr;
    UINT32 lwll_count;

public:
#define GLINFERNO_OPTS(A,B,C,D) SETGET_PROP(A, B);
#define GLINFERNO_GLOBALS(A,B,C,D)
#include "glinferno_vars.h"
#undef GLINFERNO_OPTS
#undef GLINFERNO_GLOBALS
};

