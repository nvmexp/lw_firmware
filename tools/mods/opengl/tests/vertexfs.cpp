/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "vertexfs.h"
#include "opengl/glgpugoldensurfaces.h"
#include "gpu/include/gpudev.h"
#include "core/include/display.h"
#include <float.h>
#define _USE_MATH_DEFINES
#include <math.h>

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
#define FLOAT_PER_ATTRIB 4

static const GLint m_Stream[/*m_MaxAttrib*/16*3] =
{
    GL_POSITION,            FLOAT_PER_ATTRIB,  0,
    GL_GENERIC_ATTRIB_LW,   FLOAT_PER_ATTRIB,  1,
    GL_GENERIC_ATTRIB_LW,   FLOAT_PER_ATTRIB,  2,
    GL_GENERIC_ATTRIB_LW,   FLOAT_PER_ATTRIB,  3,
    GL_GENERIC_ATTRIB_LW,   FLOAT_PER_ATTRIB,  4,
    GL_GENERIC_ATTRIB_LW,   FLOAT_PER_ATTRIB,  5,
    GL_GENERIC_ATTRIB_LW,   FLOAT_PER_ATTRIB,  6,
    GL_GENERIC_ATTRIB_LW,   FLOAT_PER_ATTRIB,  7,
    GL_GENERIC_ATTRIB_LW,   FLOAT_PER_ATTRIB,  8,
    GL_GENERIC_ATTRIB_LW,   FLOAT_PER_ATTRIB,  9,
    GL_GENERIC_ATTRIB_LW,   FLOAT_PER_ATTRIB, 10,
    GL_GENERIC_ATTRIB_LW,   FLOAT_PER_ATTRIB, 11,
    GL_GENERIC_ATTRIB_LW,   FLOAT_PER_ATTRIB, 12,
    GL_GENERIC_ATTRIB_LW,   FLOAT_PER_ATTRIB, 13,
    GL_GENERIC_ATTRIB_LW,   FLOAT_PER_ATTRIB, 14,
    GL_GENERIC_ATTRIB_LW,   FLOAT_PER_ATTRIB, 15,
};

GLVertexFS::GLVertexFS()
    : m_pGSurfs(NULL)
      ,m_NumVertices(1024*16)
      ,m_NumIndices(3*99991) //prime
      ,m_WorldX(0)
      ,m_WorldY(0)
      ,m_ScreenWidth(1024)
      ,m_ScreenHeight(768)
      ,m_MaxAttrib(0)
      ,m_BitErrors(32,0)
      ,m_BadCBufMask(0)
      ,m_WorstCBufMask(0)
{
    SetName("GLVertexFS");
}

GLVertexFS::~GLVertexFS()
{
}

bool GLVertexFS::IsSupported()
{
    InitFromJs();

    if (!m_mglTestContext.IsSupported())
        return false;

    if (GetBoundGpuDevice()->GetFamily() < GpuDevice::Fermi)
    {
        // This test uses fermi+ features.
        return false;
    }
    return true;
}

RC GLVertexFS::InitFromJs()
{
    RC rc;
    CHECK_RC( GpuTest::InitFromJs() );
    CHECK_RC( m_mglTestContext.SetProperties(
            GetTestConfiguration(),
            false,      // double buffer
            GetBoundGpuDevice(),
            GetDispMgrReqs(),
            0,          // don't override color format
            false,      //we explicitly enforce FBOs depending on the testmode.
            false,      //don't render to sysmem
            0));        //do not use layered FBO

    return rc;
}

void GLVertexFS::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);
    Printf(pri, "\tNumVertices:\t\t\t%d\n",    m_NumVertices);
    Printf(pri, "\tNumIndices:\t\t\t%d\n",     m_NumIndices);
}

RC GLVertexFS::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());
    CHECK_RC(m_mglTestContext.Setup());

    GetFpContext()->Rand.SeedRandom(0xCAFEBABE);

    m_pGSurfs = new GLGpuGoldenSurfaces(GetBoundGpuDevice());
    m_pGSurfs->DescribeSurfaces(
        m_ScreenWidth,
        m_ScreenHeight,
        mglTexFmt2Color( m_mglTestContext.ColorSurfaceFormat(0) ),
        ColorUtils::Z24S8,
        m_mglTestContext.GetPrimaryDisplay());
    CHECK_RC(GetGoldelwalues()->SetSurfaces(m_pGSurfs));

    CHECK_RC(SetupGL());
    return mglGetRC();
}

RC GLVertexFS::SetupGL()
{
    RC rc;

    //TEX
    glGenTextures(TEX_NUM, m_TexIds);
    CHECK_RC(SetupTextures());

    //FBO
    glGenFramebuffersEXT(FBO_NUM, m_FboIds);
    CHECK_RC(SetupFramebuffers());

    //Const
    glGenBuffers(CONST_NUM, m_ConstIds);

    CHECK_RC(SetupConstants());

    //VTX
    glGenBuffers(VBO_NUM, m_VboIds);
    CHECK_RC(SetupVertices());

    //PRG
    glGenProgramsLW(PROG_NUM, m_ProgIds);
    CHECK_RC(SetupPrograms());

    //queries (occ or feedback)
    glGenQueries(QUERY_NUM, m_QueryIds);

    //don't show junk
    glClear(GL_COLOR_BUFFER_BIT);

    //    CHECK_OPENGL_ERROR;
    return rc;
}

RC GLVertexFS::SetupTextures()
{
    glActiveTexture(GL_TEXTURE0_ARB);
    glEnable     (GL_TEXTURE_2D);

    for(UINT32 i =0; i < TEX_DEPTH; i++)
    {
        glBindTexture(GL_TEXTURE_2D, m_TexIds[TEX_MRT0+i]);
        glTexImage2D (GL_TEXTURE_2D,
                      0,
                      GL_RGBA8,
                      m_ScreenWidth,
                      m_ScreenHeight,
                      0,
                      GL_RGBA,
                      GL_UNSIGNED_BYTE,
                      NULL);

        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT );
    }
    glBindTexture(GL_TEXTURE_2D, m_TexIds[TEX_DEPTH]);
    glTexImage2D (GL_TEXTURE_2D,
                  0,
                  GL_DEPTH_STENCIL_EXT,
                  m_ScreenWidth,
                  m_ScreenHeight,
                  0,
                  GL_DEPTH_STENCIL_EXT,
                  GL_UNSIGNED_INT_24_8_EXT,
                  NULL);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT );

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
    return mglGetRC();
}

RC GLVertexFS::SetupFramebuffers()
{
    RC rc;

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FboIds[FBO_MRT]);
    for(UINT32 i =0; i < TEX_DEPTH; i++)
    {
        glFramebufferTexture2DEXT(
            GL_FRAMEBUFFER_EXT,
            GL_COLOR_ATTACHMENT0_EXT+i,
            GL_TEXTURE_2D,
            m_TexIds[TEX_MRT0+i],
            0);
    }
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT,
        GL_DEPTH_ATTACHMENT_EXT,
        GL_TEXTURE_2D,
        m_TexIds[TEX_DEPTH],
        0);
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT,
        GL_STENCIL_ATTACHMENT_EXT,
        GL_TEXTURE_2D,
        m_TexIds[TEX_DEPTH],
        0);
    CHECK_RC(mglCheckFbo());
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    return mglGetRC();
}

#define BUFFER_OFFSET(i)  ( (const GLvoid*) (( (char *)NULL + (i))))

//#define MYRAND lwFloatRand(0.0,1.0)
#define MYRAND GetFpContext()->Rand.GetRandomFloat(0,1.0)

void GLVertexFS::GenerateVertices(vector<FLOAT32> &vbuff, UINT32 m_NumVertices)
{

// Not ready to support this fully yet
//#define RND_ODD(x)
//    (x>0.333 ?
//       (x>0.667 ?
//         (x>0.833 ? NAN      :-NAN) :
//         (x>0.500 ? 0.0      :-0.0) ) :
//       (x>0.167 ?   INFINITY :-INFINITY) )
#define RND_ODD(x) 1.0f

    for (UINT32 i =0; i < m_NumVertices; i++) {
        for (UINT32 j =0; j < m_MaxAttrib; j++) {
            //ATTR1-15 are attributes ATTR0 is position
            UINT32 idx = (i*m_MaxAttrib + j)*FLOAT_PER_ATTRIB;

            FLOAT32 rnd  = MYRAND;
            FLOAT32 rnd0 = MYRAND;
            FLOAT32 rnd1 = MYRAND;
            FLOAT32 rnd2 = MYRAND;
            FLOAT32 rnd3 = MYRAND;

            FLOAT32 IN_LWBE          = j==0 ? 0.70f : 0.60f;
            FLOAT32 OUT_LWBE         = j==0 ? 0.27f : 0.30f;
            FLOAT32 OUT_LWBE_EXTREME = j==0 ? 0.01f : 0.05f;
            FLOAT32 DENORMS          = j==0 ? 0.01f : 0.03f; // not supported yet
            //FLOAT32 NANS_ZEROS     = j==0 ? 0.01f : 0.02f; // not supported yet
            if (        rnd < IN_LWBE ) {
                vbuff[idx+0] =               rnd0*2.0f - 1.0f;
                vbuff[idx+1] =               rnd1*2.0f - 1.0f;
                vbuff[idx+2] =               rnd2*2.0f - 1.0f;
                vbuff[idx+3] = j==0 ? 1.0f : rnd3*2.0f - 1.0f ;
            } else if ( rnd < IN_LWBE+OUT_LWBE) {
                vbuff[idx+0] =               rnd0*4.0f - 2.0f;
                vbuff[idx+1] =               rnd1*4.0f - 2.0f;
                vbuff[idx+2] =               rnd2*4.0f - 2.0f;
                vbuff[idx+3] = j==0 ? 1.0f : rnd3*4.0f - 2.0f ;
            } else if ( rnd < IN_LWBE+OUT_LWBE+OUT_LWBE_EXTREME ) {
                vbuff[idx+0] =               rnd0*FLT_MAX - FLT_MAX/2;
                vbuff[idx+1] =               rnd1*FLT_MAX - FLT_MAX/2;
                vbuff[idx+2] =               rnd2*FLT_MAX - FLT_MAX/2;
                vbuff[idx+3] = j==0 ? 1.0f : rnd3*FLT_MAX - FLT_MAX/2 ;
            } else if ( rnd < IN_LWBE+OUT_LWBE+OUT_LWBE_EXTREME+DENORMS ) {
                vbuff[idx+0] =               rnd0*FLT_MIN - FLT_MIN/2;
                vbuff[idx+1] =               rnd1*FLT_MIN - FLT_MIN/2;
                vbuff[idx+2] =               rnd2*FLT_MIN - FLT_MIN/2;
                vbuff[idx+3] = j==0 ? 1.0f : rnd3*FLT_MIN - FLT_MIN/2 ;
            } else {
                vbuff[idx+0] =               RND_ODD(rnd0);
                vbuff[idx+1] =               RND_ODD(rnd1);
                vbuff[idx+2] =               RND_ODD(rnd2);
                vbuff[idx+3] = j==0 ? 1.0f : RND_ODD(rnd3);
            }
        }
    }
}

RC GLVertexFS::SetupVertices()
{
    m_MaxAttrib = 16;
    //    Printf(Tee::PriNormal, "*+* Feedback buffer size : %ld bytes\n",m_NumIndices*m_MaxAttrib*FLOAT_PER_ATTRIB*sizeof(FLOAT32) );

    vector<FLOAT32> vbuff(m_NumVertices*m_MaxAttrib*FLOAT_PER_ATTRIB, 0.0);
    vector<FLOAT32> sbuff(m_NumIndices *m_MaxAttrib*FLOAT_PER_ATTRIB, 0.0);
    vector<UINT32>  ibuff(m_NumIndices                              , 0x0);

    //Vertex Data
    GenerateVertices( vbuff, m_NumVertices );
    glBindBuffer(GL_ARRAY_BUFFER, m_VboIds[VBO_VTX]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(FLOAT32)* vbuff.size(),
                 (void*)&vbuff[0],
                 GL_STATIC_DRAW_ARB);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    for (UINT32 i = 0; i < m_NumIndices; i++)
    {
        ibuff[i] = GetFpContext()->Rand.GetRandom(0,1<<30) % m_NumVertices;
    }
    m_CpuCalcResult.resize( sbuff.size() );  //same size as sbuff -- the stream buffer

//    GenerateVerticesCPU(vbuff, ibuff);

    //idx
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_VboIds[VBO_IDX]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(UINT32)* ibuff.size(),
                 (void*)&ibuff[0],
                 GL_STATIC_DRAW_ARB);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER_LW, m_VboIds[VBO_STREAM_OUT1]);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER_LW,
                 sizeof(FLOAT32)* sbuff.size(),
                 (void*)&sbuff[0],
                 GL_DYNAMIC_COPY_ARB);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER_LW, 0);

    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER_LW, m_VboIds[VBO_STREAM_OUT2]);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER_LW,
                 sizeof(FLOAT32)* sbuff.size(),
                 (void*)&sbuff[0],
                 GL_DYNAMIC_COPY_ARB);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER_LW, 0);

    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER_LW, m_VboIds[VBO_STREAM_OUT3]);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER_LW,
                 sizeof(FLOAT32)* sbuff.size(),
                 (void*)&sbuff[0],
                 GL_DYNAMIC_COPY_ARB);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER_LW, 0);

    return mglGetRC();
}

RC GLVertexFS::SetupPrograms()
{
    RC rc;
#define PROGRAM_SETUP(A,B,C)                                                      \
{                                                                                 \
    string prog_##A = B;                                                          \
    CHECK_RC(mglLoadProgram(GL_##C##_PROGRAM_LW, m_ProgIds[PROG_##A],             \
                            static_cast<GLsizei>(prog_##A.size()),                \
                            reinterpret_cast<const GLubyte *>(prog_##A.c_str())));\
}
    PROGRAM_SETUP(VERT_MAIN, GetVertexShader(), VERTEX);
    //there will be more programs here for the VPC varients

#undef PROGRAM_SETUP
    return mglGetRC();
}

RC GLVertexFS::SetupConstants()
{
    //1024k * 16 bytes = 16k (opengl limit?)
    //row major

    m_ConstBuff.resize(256*16);

    for(INT32 i = 0; i < 4096; i+=16)
    {
        FLOAT32* c = &m_ConstBuff[i+0];
        if (i == 0)
        {
            //ortho
            c[0 ]=1.0; c[ 1]=0.0; c[ 2]= 0.0; c[ 3]=0.0;
            c[4 ]=0.0; c[ 5]=1.0; c[ 6]= 0.0; c[ 7]=0.0;
            c[8 ]=0.0; c[ 9]=0.0; c[10]=-1.0; c[11]=0.0;
            c[12]=0.0; c[13]=0.0; c[14]= 0.0; c[15]=1.0;
        }
        else
        {
            c[0 ]=MYRAND; c[ 1]=MYRAND; c[ 2]=MYRAND; c[ 3]=MYRAND;
            c[4 ]=MYRAND; c[ 5]=MYRAND; c[ 6]=MYRAND; c[ 7]=MYRAND;
            c[8 ]=MYRAND; c[ 9]=MYRAND; c[10]=MYRAND; c[11]=MYRAND;
            c[12]=MYRAND; c[13]=MYRAND; c[14]=MYRAND; c[15]=MYRAND;
        }
    }

    glBindBuffer    (GL_VERTEX_PROGRAM_PARAMETER_BUFFER_LW, m_ConstIds[CONST_VERT_MAIN]);
    glBufferData    (GL_VERTEX_PROGRAM_PARAMETER_BUFFER_LW, sizeof(FLOAT32)*m_ConstBuff.size(), &m_ConstBuff[0], GL_DYNAMIC_DRAW);
    glBindBuffer    (GL_VERTEX_PROGRAM_PARAMETER_BUFFER_LW, 0);

    return mglGetRC();
}

RC GLVertexFS::ReconfigureGPU(gpu_config_e mode)
{
    //cbuffer
    glBindBufferBaseLW(GL_VERTEX_PROGRAM_PARAMETER_BUFFER_LW, 0, 0);

    //PRog reset
    glDisable(GL_VERTEX_PROGRAM_LW);
    glDisable(GL_FRAGMENT_PROGRAM_LW);

    //FBO reset
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

    for (UINT32 i = 0; i < m_MaxAttrib; i++)
    {
        glDisableClientState(GL_VERTEX_ATTRIB_ARRAY0_LW + i);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBufferOffsetLW(GL_TRANSFORM_FEEDBACK_BUFFER_LW, 0, 0, 0);

    //TEX reset
    for(int i = 0; i < 8; i++)
    {
        glActiveTexture(GL_TEXTURE0_ARB+i);
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        glTexElwf(GL_TEXTURE_ELW, GL_TEXTURE_ELW_MODE, GL_MODULATE);
    }

    //viewport
    glViewport(0, 0, m_ScreenWidth, m_ScreenHeight);

    //MVP
    FLOAT32 m1[16], m2[16];

    glMatrixMode(GL_PROJECTION);    // add perspective to scene
    glLoadIdentity();
    glOrtho (-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glGetFloatv(GL_PROJECTION_MATRIX, m1);

    glMatrixMode(GL_MODELVIEW);
    glGetFloatv(GL_MODELVIEW_MATRIX, m2);
    glLoadIdentity();

    switch(mode)
    {
    case CONFIG_STD1:
    case CONFIG_STD2:
    case CONFIG_STD3:
        glEnable(GL_RASTERIZER_DISCARD);

        //VBO
        glBindBuffer(GL_ARRAY_BUFFER,              m_VboIds[VBO_VTX]);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,      m_VboIds[VBO_IDX]);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,  m_VboIds[VBO_STREAM_OUT1+(mode-CONFIG_STD1)]);
        for (UINT32 i = 0; i < m_MaxAttrib; i++) {
            glVertexAttribPointerLW  (i,
                                      FLOAT_PER_ATTRIB,
                                      GL_FLOAT,
                                      m_MaxAttrib*      sizeof(FLOAT32)*FLOAT_PER_ATTRIB,
                                      (const GLvoid*)(i*sizeof(FLOAT32)*FLOAT_PER_ATTRIB) );
            glEnableClientState(GL_VERTEX_ATTRIB_ARRAY0_LW + i);
        }

        glBindBufferOffsetLW(GL_TRANSFORM_FEEDBACK_BUFFER_LW, 0, m_VboIds[VBO_STREAM_OUT1+(mode-CONFIG_STD1)], 0);
        glTransformFeedbackAttribsLW(m_MaxAttrib, m_Stream, GL_INTERLEAVED_ATTRIBS_LW);

        //VS
        glEnable          (GL_VERTEX_PROGRAM_LW);
        glBindProgramLW   (GL_VERTEX_PROGRAM_LW,                       m_ProgIds[PROG_VERT_MAIN] );
        glBindBufferBaseLW(GL_VERTEX_PROGRAM_PARAMETER_BUFFER_LW, 0, m_ConstIds[CONST_VERT_MAIN]);

        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        break;
    case CONFIG_COPY_FRONT:
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

        glActiveTexture(GL_TEXTURE0_ARB);
        glEnable       (GL_TEXTURE_2D);
        glBindTexture  (GL_TEXTURE_2D, m_TexIds[TEX_MRT0]);
        glTexElwf(GL_TEXTURE_ELW, GL_TEXTURE_ELW_MODE, GL_DECAL);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        break;
    default:
        break;
    }
    return mglGetRC();
}

//
bool GLVertexFS::DoCheck(UINT32 smid, UINT32 valid2, UINT32 idx, UINT32* p1, UINT32* p2, UINT32* p3)
{
    //DEADBEEF is filled in @ valid2 offset upon exelwtion of the shader
    //SMID#    is filled in @ smid and used to id the producer of the vertex
    //DATA     is filled in @ idx and used as a source of comparison data.
    //don't have enough info
    if      ( p1[valid2] != 0xDEADBEEF || p2[valid2] != 0xDEADBEEF || p3[valid2] != 0xDEADBEEF ) {
        return false;
    }
    else //VOTE
    {
        //p1 and p2 agree, p3 disagrees
        if (      p1[idx]  == p2[idx]  && p1[idx]  != p3[idx] )
        {
            //p1 and p2 drawn by same smid and p3 diff
            if (  p1[smid] == p2[smid] && p1[smid] != p3[smid] )
            {
                ;//inconclusive
            }
            else
            {
                //p3 is believed to be bad
                m_BitErrors[p3[smid]]++;
            }
        }
        //p1 and p3 agree, p2 disagrees
        else if ( p1[idx]  == p3[idx]  && p1[idx]  != p2[idx] )
        {
            //p1 and p3 drawn by same smid and p2 diff
            if (  p1[smid] == p3[smid] && p1[smid] != p2[smid] )
            {
                ;//inconclusive
            }
            else
            {
                //p2 is believed to be bad
                m_BitErrors[p2[smid]]++;
            }
        }
        //p2 and p3 agree, p1 disagrees
        else if ( p2[idx]  == p3[idx]  && p1[idx]  != p2[idx] )
        {
            if (  p2[smid] == p3[smid] && p1[smid] != p2[smid] )
            {
                ;//inconclusive
            }
            else
            {
                //p1 is believed to be bad
                m_BitErrors[p1[smid]]++;
            }
        }
        //all are equal or all are unequal, either way, no info
        //unless the same smid draws different values
        else
        {
            if     ( p1[idx]  != p2[idx] && p2[idx] != p3[idx] )
            {
                if ( p1[smid] == p2[smid] )
                {
                    m_BitErrors[p1[smid]]++;
                }
                else if ( p2[smid] == p3[smid] )
                {
                    m_BitErrors[p2[smid]]++;
                }
                else if ( p1[smid] == p3[smid] )
                {
                    m_BitErrors[p3[smid]]++;
                }
                else
                {
                    ;//inconclusive -- 3 smids, 3 values
                }
            }
            else
            {
                ;//Values are identical across all
            }
        }
    }
    return true;
}

// flag hangs
void GLVertexFS::BinHang(UINT32 smid)
{
    m_HangDetect[smid] = true;
}

//only
static void print_buffers(UINT32* ptr, UINT32* ptr2, UINT32* ptr3, int size) {
   for( int j = 0 ; j < size/8; j++) {
        int i = j*8;
        Printf(Tee::PriNormal, "0x%08x : 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",j*4*8,
               ptr[i+0], ptr[i+1], ptr[i+2], ptr[i+3],
               ptr[i+4], ptr[i+5], ptr[i+6], ptr[i+7],
               ptr2[i+0], ptr2[i+1], ptr2[i+2], ptr2[i+3],
               ptr2[i+4], ptr2[i+5], ptr2[i+6], ptr2[i+7],
               ptr3[i+0], ptr3[i+1], ptr3[i+2], ptr3[i+3],
               ptr3[i+4], ptr3[i+5], ptr3[i+6], ptr3[i+7]);
    }
}

RC GLVertexFS::Run()
{
    //Vote #1
    ReconfigureGPU(CONFIG_STD1);
    glBeginTransformFeedbackLW(GL_TRIANGLES);
    glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN_LW, m_QueryIds[QUERY_FEEDBACK]);
    glDrawElements(GL_TRIANGLES, m_NumIndices, GL_UNSIGNED_INT, 0);
    glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN_LW);
    glEndTransformFeedbackLW();
    glFinish();
    UINT32* p1= reinterpret_cast<UINT32*>(glMapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER_LW, GL_READ_WRITE));

    //Vote #2
    ReconfigureGPU(CONFIG_STD2);
    glBeginTransformFeedbackLW(GL_TRIANGLES);
    glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN_LW, m_QueryIds[QUERY_FEEDBACK]);
    glDrawElements(GL_TRIANGLES, m_NumIndices, GL_UNSIGNED_INT, 0);
    glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN_LW);
    glEndTransformFeedbackLW();
    glFinish();
    UINT32* p2= reinterpret_cast<UINT32*>(glMapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER_LW, GL_READ_WRITE));

    //Vote #3
    ReconfigureGPU(CONFIG_STD3);
    glBeginTransformFeedbackLW(GL_TRIANGLES);
    glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN_LW, m_QueryIds[QUERY_FEEDBACK]);
    glDrawElements(GL_TRIANGLES, m_NumIndices, GL_UNSIGNED_INT, 0);
    glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN_LW);
    glEndTransformFeedbackLW();
    glFinish();
    UINT32* p3= reinterpret_cast<UINT32*>(glMapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER_LW, GL_READ_WRITE));

    if(0) print_buffers(p1,p2,p3,m_NumIndices*m_MaxAttrib*FLOAT_PER_ATTRIB);

    //CHECK_OPENGL_ERROR;

    UINT32 checked = 0;
    UINT32 attempted = 0;
    for (UINT32 i = 0; i < m_NumIndices; i++)
    {
        for (UINT32 j = 0; j < m_MaxAttrib ; j++)
        {
            for (UINT32 k = 0; k < FLOAT_PER_ATTRIB; k++)
            {

                UINT32 smid   = i*m_MaxAttrib*FLOAT_PER_ATTRIB+ (m_MaxAttrib-1)*FLOAT_PER_ATTRIB;
                UINT32 valid2 = i*m_MaxAttrib*FLOAT_PER_ATTRIB+ (m_MaxAttrib-1)*FLOAT_PER_ATTRIB+3;
                UINT32 idx    = i*m_MaxAttrib*FLOAT_PER_ATTRIB+ j              *FLOAT_PER_ATTRIB+k;

                checked += DoCheck(smid,valid2,idx, p1,p2,p3) ? 1 : 0;
                attempted++;
            }
        }
    }
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,  m_VboIds[VBO_STREAM_OUT1]);
    glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER_LW);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,  m_VboIds[VBO_STREAM_OUT2]);
    glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER_LW);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,  m_VboIds[VBO_STREAM_OUT3]);
    glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER_LW);

    CHECK_OPENGL_ERROR;

    UINT32 worst = 0;
    UINT32 bad   = 0;
    for (UINT32 i = 0; i < 32; i++)
    {
        if (m_BitErrors[i])
        {
            bad |= (1<<i);
            SetBadCBufMask(GetBadCBufMask() | (1<<i));
            if (m_BitErrors[i] > worst)
            {
                SetWorstCBufMask( 1<<i );
                worst = (1<<i);
            }
        }
    }
    Printf(Tee::PriNormal, "Checked       = %d/%d\n",  checked,attempted);
    Printf(Tee::PriNormal, "BadCBufMask   = 0x%08x\n",    bad);
    Printf(Tee::PriNormal, "WorstCBufMask = 0x%08x\n",  worst);

    return GetBadCBufMask() ? RC::GPU_STRESS_TEST_FAILED: OK;
}

RC GLVertexFS::Cleanup()
{
    StickyRC firstRc;
    if (m_mglTestContext.IsGLInitialized())
    {
        if(m_ProgIds[0])
            glDeleteProgramsLW        (PROG_NUM,  m_ProgIds);
        if(m_ConstIds[0])
            glDeleteBuffers           (CONST_NUM, m_ConstIds);
        if(m_FboIds[0])
            glDeleteFramebuffersEXT   (FBO_NUM,   m_FboIds);
        if(m_TexIds[0])
            glDeleteTextures          (TEX_NUM,   m_TexIds);
        if(m_VboIds[0])
            glDeleteBuffers           (VBO_NUM,   m_VboIds);

        firstRc = mglGetRC();
    }

    memset(m_FboIds,   0, sizeof(m_FboIds));
    memset(m_VboIds,   0, sizeof(m_VboIds));
    memset(m_ProgIds,  0, sizeof(m_ProgIds));
    memset(m_TexIds,   0, sizeof(m_TexIds));
    memset(m_ConstIds, 0, sizeof(m_ConstIds));

    GetGoldelwalues()->ClearSurfaces();
    delete m_pGSurfs;
    m_pGSurfs = NULL;

    if (m_mglTestContext.GetDisplay())
    {
        firstRc = m_mglTestContext.GetDisplay()->TurnScreenOff(false);
    }
    firstRc = m_mglTestContext.Cleanup();
    firstRc = GpuTest::Cleanup();
    return firstRc;
}

JS_CLASS_INHERIT_FULL(GLVertexFS, GpuTest, "OpenGL Vertex FS test", MODS_INTERNAL_ONLY);

CLASS_PROP_READWRITE(GLVertexFS, NumVertices,   UINT32, ".");
CLASS_PROP_READWRITE(GLVertexFS, NumIndices,    UINT32, ".");
CLASS_PROP_READWRITE(GLVertexFS, ScreenWidth,   UINT32, ".");
CLASS_PROP_READWRITE(GLVertexFS, ScreenHeight,  UINT32, ".");
CLASS_PROP_READWRITE(GLVertexFS, BadCBufMask,   UINT32, "Vector of bad SMID");
CLASS_PROP_READWRITE(GLVertexFS, WorstCBufMask, UINT32, "Vector of worst SMID");

string GLVertexFS::GetVertexShader()
{
    string s =
        "!!LWvp5.0\n"
        "BUFFER4 c[1024] = { program.buffer[0][0..1023] };\n" //EDIT PARAM/local ->BUFFER4/buffer, MVP via PaBO
        "ATTRIB vertex_attrib[] = { vertex.attrib[0..15] };\n"
        "OUTPUT result_attrib[] = { result.attrib[0..15] };\n"
        "TEMP R0;\n"
        "MOD.S R0.x, vertex.id, {255}.x;\n"
        "MUL.S R0.x, R0, {4};\n"
        "ADD.S R0.y, vertex.id.x, {1}.x;\n"
        "MOD.S R0.y, R0, {255}.x;\n"
        "MOV.U R0.x, R0;\n"
        "DP4.F result.position.w, vertex.attrib[0], c[3];\n"
        "DP4.F result.position.z, vertex.attrib[0], c[2];\n"
        "DP4.F result.position.y, vertex.attrib[0], c[1];\n"
        "DP4.F result.position.x, vertex.attrib[0], c[0];\n"
        "DP4.F result.attrib[1].w, c[R0.x + 7], vertex.attrib[1];\n"
        "DP4.F result.attrib[1].z, vertex.attrib[1], c[R0.x + 6];\n"
        "DP4.F result.attrib[1].y, vertex.attrib[1], c[R0.x + 5];\n"
        "MUL.S R0.y, R0, {4}.x;\n"
        "DP4.F result.attrib[1].x, vertex.attrib[1], c[R0.x + 4];\n"
        "MOV.U R0.x, R0.y;\n"
        "ADD.S R0.y, vertex.id.x, {2}.x;\n"
        "MOD.S R0.y, R0, {255}.x;\n"
        "MUL.S R0.y, R0, {4}.x;\n"
        "MOV.U R0.y, R0;\n"
        "DP4.F result.attrib[2].w, c[R0.x + 7], vertex.attrib[2];\n"
        "DP4.F result.attrib[2].z, vertex.attrib[2], c[R0.x + 6];\n"
        "DP4.F result.attrib[2].y, vertex.attrib[2], c[R0.x + 5];\n"
        "DP4.F result.attrib[2].x, vertex.attrib[2], c[R0.x + 4];\n"
        "ADD.S R0.x, vertex.id, {3};\n"
        "MOD.S R0.x, R0, {255}.x;\n"
        "MUL.S R0.x, R0, {4};\n"
        "MOV.U R0.x, R0;\n"
        "DP4.F result.attrib[3].w, c[R0.y + 7], vertex.attrib[3];\n"
        "DP4.F result.attrib[3].z, vertex.attrib[3], c[R0.y + 6];\n"
        "DP4.F result.attrib[3].y, vertex.attrib[3], c[R0.y + 5];\n"
        "DP4.F result.attrib[3].x, vertex.attrib[3], c[R0.y + 4];\n"
        "ADD.S R0.y, vertex.id.x, {4}.x;\n"
        "MOD.S R0.y, R0, {255}.x;\n"
        "MUL.S R0.y, R0, {4}.x;\n"
        "MOV.U R0.y, R0;\n"
        "DP4.F result.attrib[4].w, c[R0.x + 7], vertex.attrib[4];\n"
        "DP4.F result.attrib[4].z, vertex.attrib[4], c[R0.x + 6];\n"
        "DP4.F result.attrib[4].y, vertex.attrib[4], c[R0.x + 5];\n"
        "DP4.F result.attrib[4].x, vertex.attrib[4], c[R0.x + 4];\n"
        "ADD.S R0.x, vertex.id, {5};\n"
        "MOD.S R0.x, R0, {255}.x;\n"
        "MUL.S R0.x, R0, {4};\n"
        "MOV.U R0.x, R0;\n"
        "DP4.F result.attrib[5].w, c[R0.y + 7], vertex.attrib[5];\n"
        "DP4.F result.attrib[5].z, vertex.attrib[5], c[R0.y + 6];\n"
        "DP4.F result.attrib[5].y, vertex.attrib[5], c[R0.y + 5];\n"
        "DP4.F result.attrib[5].x, vertex.attrib[5], c[R0.y + 4];\n"
        "ADD.S R0.y, vertex.id.x, {6}.x;\n"
        "MOD.S R0.y, R0, {255}.x;\n"
        "MUL.S R0.y, R0, {4}.x;\n"
        "MOV.U R0.y, R0;\n"
        "DP4.F result.attrib[6].w, c[R0.x + 7], vertex.attrib[6];\n"
        "DP4.F result.attrib[6].z, vertex.attrib[6], c[R0.x + 6];\n"
        "DP4.F result.attrib[6].y, vertex.attrib[6], c[R0.x + 5];\n"
        "DP4.F result.attrib[6].x, vertex.attrib[6], c[R0.x + 4];\n"
        "ADD.S R0.x, vertex.id, {7};\n"
        "MOD.S R0.x, R0, {255}.x;\n"
        "MUL.S R0.x, R0, {4};\n"
        "MOV.U R0.x, R0;\n"
        "DP4.F result.attrib[7].w, c[R0.y + 7], vertex.attrib[7];\n"
        "DP4.F result.attrib[7].z, vertex.attrib[7], c[R0.y + 6];\n"
        "DP4.F result.attrib[7].y, vertex.attrib[7], c[R0.y + 5];\n"
        "DP4.F result.attrib[7].x, vertex.attrib[7], c[R0.y + 4];\n"
        "ADD.S R0.y, vertex.id.x, {8}.x;\n"
        "MOD.S R0.y, R0, {255}.x;\n"
        "MUL.S R0.y, R0, {4}.x;\n"
        "MOV.U R0.y, R0;\n"
        "DP4.F result.attrib[8].w, c[R0.x + 7], vertex.attrib[8];\n"
        "DP4.F result.attrib[8].z, vertex.attrib[8], c[R0.x + 6];\n"
        "DP4.F result.attrib[8].y, vertex.attrib[8], c[R0.x + 5];\n"
        "DP4.F result.attrib[8].x, vertex.attrib[8], c[R0.x + 4];\n"
        "ADD.S R0.x, vertex.id, {9};\n"
        "MOD.S R0.x, R0, {255}.x;\n"
        "MUL.S R0.x, R0, {4};\n"
        "MOV.U R0.x, R0;\n"
        "DP4.F result.attrib[9].w, c[R0.y + 7], vertex.attrib[9];\n"
        "DP4.F result.attrib[9].z, vertex.attrib[9], c[R0.y + 6];\n"
        "DP4.F result.attrib[9].y, vertex.attrib[9], c[R0.y + 5];\n"
        "DP4.F result.attrib[9].x, vertex.attrib[9], c[R0.y + 4];\n"
        "ADD.S R0.y, vertex.id.x, {10}.x;\n"
        "MOD.S R0.y, R0, {255}.x;\n"
        "MUL.S R0.y, R0, {4}.x;\n"
        "MOV.U R0.y, R0;\n"
        "DP4.F result.attrib[10].w, c[R0.x + 7], vertex.attrib[10];\n"
        "DP4.F result.attrib[10].z, vertex.attrib[10], c[R0.x + 6];\n"
        "DP4.F result.attrib[10].y, vertex.attrib[10], c[R0.x + 5];\n"
        "DP4.F result.attrib[10].x, vertex.attrib[10], c[R0.x + 4];\n"
        "ADD.S R0.x, vertex.id, {11};\n"
        "MOD.S R0.x, R0, {255}.x;\n"
        "MUL.S R0.x, R0, {4};\n"
        "MOV.U R0.x, R0;\n"
        "DP4.F result.attrib[11].w, c[R0.y + 7], vertex.attrib[11];\n"
        "DP4.F result.attrib[11].z, vertex.attrib[11], c[R0.y + 6];\n"
        "DP4.F result.attrib[11].y, vertex.attrib[11], c[R0.y + 5];\n"
        "DP4.F result.attrib[11].x, vertex.attrib[11], c[R0.y + 4];\n"
        "ADD.S R0.y, vertex.id.x, {12}.x;\n"
        "MOD.S R0.y, R0, {255}.x;\n"
        "MUL.S R0.y, R0, {4}.x;\n"
        "MOV.U R0.y, R0;\n"
        "DP4.F result.attrib[12].w, c[R0.x + 7], vertex.attrib[12];\n"
        "DP4.F result.attrib[12].z, vertex.attrib[12], c[R0.x + 6];\n"
        "DP4.F result.attrib[12].y, vertex.attrib[12], c[R0.x + 5];\n"
        "DP4.F result.attrib[12].x, vertex.attrib[12], c[R0.x + 4];\n"
        "ADD.S R0.x, vertex.id, {13};\n"
        "MOD.S R0.x, R0, {255}.x;\n"
        "MUL.S R0.x, R0, {4};\n"
        "MOV.U R0.x, R0;\n"
        "DP4.F result.attrib[13].w, c[R0.y + 7], vertex.attrib[13];\n"
        "DP4.F result.attrib[13].z, vertex.attrib[13], c[R0.y + 6];\n"
        "DP4.F result.attrib[13].y, vertex.attrib[13], c[R0.y + 5];\n"
        "DP4.F result.attrib[13].x, vertex.attrib[13], c[R0.y + 4];\n"
        "ADD.S R0.y, vertex.id.x, {14}.x;\n"
        "MOD.S R0.y, R0, {255}.x;\n"
        "MUL.S R0.y, R0, {4}.x;\n"
        "MOV.U R0.y, R0;\n"
        "DP4.F result.attrib[14].w, c[R0.x + 7], vertex.attrib[14];\n"
        "DP4.F result.attrib[14].z, vertex.attrib[14], c[R0.x + 6];\n"
        "DP4.F result.attrib[14].y, vertex.attrib[14], c[R0.x + 5];\n"
        "DP4.F result.attrib[14].x, vertex.attrib[14], c[R0.x + 4];\n"
        "DP4.F result.attrib[15].w, c[R0.y + 7], vertex.attrib[15];\n"
        "DP4.F result.attrib[15].z, vertex.attrib[15], c[R0.y + 6];\n"
        "DP4.F result.attrib[15].y, vertex.attrib[15], c[R0.y + 5];\n"
        "DP4.F result.attrib[15].x, vertex.attrib[15], c[R0.y + 4];\n"
        "MOV.U result.attrib[15], { 0x3f800000, 0x3f800000, 0xCAFEBABE, 0xDEADBEEF};\n"
        "MOV.U result.attrib[15].x, vertex.smid.x;\n"
        "MOV.U result.attrib[15].y, vertex.id.x;\n"
        "ADD.S R0.y, R0.y, {4}.x;\n"
        "MOV.U result.attrib[15].z, R0.y;\n"
        "END\n"
        "# 123 instructions, 1 R-regs\n";
    return s;
}

