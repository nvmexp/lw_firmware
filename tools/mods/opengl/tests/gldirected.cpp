/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file gldirected.cpp
 * class GLDirected
 *
 * GLDirected uses OpenGL to create a very specific targeted test that writes a specific
 * geometry. This class is strickly for debugging complex OpenGL issues and the contents of
 * the Setup() and Run() functions can change to suit the current needs of the mods engineer.
 **/
#include "opengl/modsgl.h"
#include "core/include/tee.h"
#include "core/include/jscript.h"
#include "gpu/include/gpudev.h"
#include "gpu/tests/gputest.h"
#include "core/include/utility.h"
#include "core/include/imagefil.h"

//------------------------------------------------------------------------------
class GLDirected : public GpuTest
{
public:
    GLDirected();
    virtual ~GLDirected();

    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(VertexShader, string);
    SETGET_PROP(FragmentShader, string);
    SETGET_PROP(VertexProgram, string);
    SETGET_PROP(FragmentProgram, string);
    SETGET_PROP(DumpImage, UINT32);
    SETGET_PROP(UseGLSLShaders, UINT32);
    SETGET_PROP(BugNumber, UINT32);

private:
    string m_VertexShader;
    string m_FragmentShader;

    string m_VertexProgram;
    string m_FragmentProgram;

    UINT32 m_DumpImage          = 0x1;   // just dump the color image
    UINT32 m_UseGLSLShaders     = 0x1;
    UINT32 m_BugNumber          = 2242089;
    mglTestContext m_mglTestContext;

    // GL state for loading GLSL shaders
    GLuint m_VertexShaderId     = 0;
    GLuint m_FragmentShaderId   = 0;
    GLuint m_GraphicsProgramId  = 0;
    GLuint m_ProgramPipelineId  = 0;

    // GL state for loading GpuProgram assembly shaders
    GLuint m_VertexPgmId        = 0;
    GLuint m_FragmentPgmId      = 0;

    // Misc test variables

    // Private general purpose functions:
    RC      DumpImage(bool readZS, const char * name);
    RC      InitFromJs();
    RC      LoadGLSLShaders(string & vertexShader, string & fragShader);
    RC      LoadGpuPgm(GLuint * const pId, GLuint target, const string & pgm);
    void    PrintPgmError(Tee::Priority pri, int errOffset, const string & pgmStr);

    // Private bug specific functions
    RC      RunBug2242089();
    RC      SetupBug2242089();
};

//------------------------------------------------------------------------------
JS_CLASS_INHERIT_FULL(GLDirected, GpuTest,
                      "OpenGL directed test",
                      0);

CLASS_PROP_READWRITE(GLDirected, VertexShader, string,
                     "Vertex shader (GLSL) as a string");
CLASS_PROP_READWRITE(GLDirected, FragmentShader, string,
                     "Fragment shader (GLSL) as a string");
CLASS_PROP_READWRITE(GLDirected, VertexProgram, string,
                     "Vertex shader (asm) as a string");
CLASS_PROP_READWRITE(GLDirected, FragmentProgram, string,
                     "Fragment shader (asm) as a string");
CLASS_PROP_READWRITE(GLDirected, DumpImage, UINT32,
                     "Set bits 0:color and/or bit 1:depth to dump images(default = 0x01)");
CLASS_PROP_READWRITE(GLDirected, UseGLSLShaders, UINT32,
                     "If true use GLSL shaders, otherrwise use GpuPgm assembly shaders (default=true)"); //$
CLASS_PROP_READWRITE(GLDirected, BugNumber, UINT32,
                     "Specify which bug function to setup/run (default 2242089)");
//------------------------------------------------------------------------------
GLDirected::GLDirected()
{
    SetName("GLDirected");
}

//------------------------------------------------------------------------------
/*virtual*/
GLDirected::~GLDirected()
{
}

//------------------------------------------------------------------------------
/*virtual*/
bool GLDirected::IsSupported()
{
    return mglHwIsSupported(GetBoundGpuSubdevice());
}

//------------------------------------------------------------------------------
/*virtual*/
RC GLDirected::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());
    switch (m_BugNumber)
    { // there is nothing special about the setup in Bug2242089 and it can be used for most cases.
        default:
        case 2242089:
            CHECK_RC(SetupBug2242089());
            break;
    }
    CHECK_RC(m_mglTestContext.Setup());
    return rc;
}

//------------------------------------------------------------------------------
RC GLDirected::LoadGLSLShaders(string & vertexShader, string & fragShader)
{
    RC rc;

    // Compile the vertex and fragment shaders, link the graphics program.
    m_GraphicsProgramId = glCreateProgram();

    CHECK_RC(ModsGL::LoadShader(GL_FRAGMENT_SHADER, &m_FragmentShaderId, fragShader.c_str())); //$
    glAttachShader(m_GraphicsProgramId, m_FragmentShaderId);

    CHECK_RC(ModsGL::LoadShader(GL_VERTEX_SHADER, &m_VertexShaderId, vertexShader.c_str()));
    glAttachShader(m_GraphicsProgramId, m_VertexShaderId);

    glProgramParameteri(m_GraphicsProgramId, GL_PROGRAM_SEPARABLE, GL_TRUE);
    CHECK_RC(ModsGL::LinkProgram(m_GraphicsProgramId));
    CHECK_RC(mglGetRC());

    // Create a program pipeline object so we can make both graphics
    // and compute stages "current" at the same time.
    glGenProgramPipelines(1, &m_ProgramPipelineId);
    glBindProgramPipeline(m_ProgramPipelineId);
    glUseProgramStages(m_ProgramPipelineId, GL_VERTEX_SHADER_BIT, m_GraphicsProgramId);
    glUseProgramStages(m_ProgramPipelineId, GL_FRAGMENT_SHADER_BIT, m_GraphicsProgramId);
    CHECK_RC(mglGetRC());

    return rc;
}

//------------------------------------------------------------------------------
void GLDirected::PrintPgmError(Tee::Priority pri, int errOffset, const string& pgmStr)
{
    const char * pstr = pgmStr.c_str();
    const GLint  pgmLen = (GLint) pgmStr.size();
    vector<char> buf (1024, 0);

    for (int pgmOffset = 0; pgmOffset < pgmLen; /**/)
    {
        int bytesThisLoop = static_cast<int>(buf.size() - 1);
        if (pgmOffset < errOffset)
        {
            if (pgmOffset + bytesThisLoop > errOffset)
                bytesThisLoop = errOffset - pgmOffset;
        }
        else if (pgmOffset == errOffset)
        {
            Printf(pri, "@@@");
        }
        if (pgmOffset + bytesThisLoop > pgmLen)
        {
            bytesThisLoop = pgmLen - pgmOffset;
        }
        memcpy (&buf[0], pstr + pgmOffset, bytesThisLoop);
        buf[bytesThisLoop] = '\0';

        Printf(pri, "%s", &buf[0]);
        pgmOffset += bytesThisLoop;
    }
}

//------------------------------------------------------------------------------
RC GLDirected::LoadGpuPgm(GLuint * const pId, GLuint target, const string & pgm)
{
    RC rc;
    glGenProgramsLW(1, pId);
    glEnable(target);
    glBindProgramLW(target, *pId);
    glProgramStringARB(target,
                       GL_PROGRAM_FORMAT_ASCII_ARB,
                       (GLsizei)pgm.size(),
                       (const GLubyte *)pgm.c_str());
    if (OK != (rc = mglGetRC()))
    {
        //
        // Print a report to the screen and logfile, showing where the syntax
        // error oclwrred in the program string.
        //
        GLint errOffset=0;
        const GLubyte * errMsg = (const GLubyte *)"";

        glGetIntegerv(GL_PROGRAM_ERROR_POSITION_LW, &errOffset);
        errMsg = glGetString(GL_PROGRAM_ERROR_STRING_LW);

        Printf(Tee::PriNormal, "glLoadProgramLW err %s at %d, \n\"",
               (const char *)errMsg, errOffset);
        PrintPgmError(Tee::PriNormal, errOffset, pgm);
        Printf(Tee::PriNormal, "\"\n");
    }
    return rc;
}

//------------------------------------------------------------------------------
/*virtual*/
RC GLDirected::Run()
{
    RC rc;
    switch (m_BugNumber)
    {
        case 2242089:
            return RunBug2242089();
        default:
        {
            Printf(Tee::PriError, "Failed to provide a bug number to run.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }
}

//------------------------------------------------------------------------------
RC GLDirected::DumpImage
(
    bool readZS,
    const char * name
)
{
    RC rc;
    const GLenum cFmt  = readZS ? GL_DEPTH_STENCIL_LW : GL_RGBA8;
    const mglTexFmtInfo * pFmtInfo = mglGetTexFmtInfo(mglTexFmt2Color(cFmt));
    const UINT32 bpp = ColorUtils::PixelBytes(pFmtInfo->ExtColorFormat);
    const UINT32 width = GetTestConfiguration()->SurfaceWidth();
    const UINT32 height = GetTestConfiguration()->SurfaceHeight();
    const UINT32 zDepth = GetTestConfiguration()->ZDepth();
    const UINT32 size = width * height * bpp;
    vector<GLubyte> screenBuf;

    screenBuf.resize(size);

    if (readZS)
    {
        const GLenum fmt =
            (zDepth == 32) && mglExtensionSupported("GL_EXT_packed_depth_stencil")
            ? GL_DEPTH_STENCIL_LW : GL_DEPTH_COMPONENT;
        const GLenum type = zDepth != 32 ?
            GL_UNSIGNED_INT : GL_UNSIGNED_INT_24_8_LW;
        glReadPixels(0, 0, width, height, fmt, type, &screenBuf[0]);
    }
    else
    {
        glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, &screenBuf[0]);
    }
    rc = mglGetRC();
    if (OK != rc)
    {
        Printf(Tee::PriError, "glReadPixels() failed with %d(%s)\n", rc.Get(), rc.Message());
    }
    else
    {
        rc = ImageFile::WritePng(
            name,
            ColorUtils::ColorDepthToFormat(32),
            &screenBuf[0],
            GetTestConfiguration()->SurfaceWidth(),
            GetTestConfiguration()->SurfaceHeight(),
            GetTestConfiguration()->SurfaceWidth() * 4,
            false,
            false);
        if (OK == rc)
        {
            Printf(Tee::PriNormal, "Wrote image file %s.\n", name);
        }
        else
        {
            Printf(Tee::PriNormal, "WritePng(%s) failed with error code %d(%s)\n",
                   name, rc.Get(), rc.Message());
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
/*virtual*/
RC GLDirected::Cleanup()
{
    StickyRC rc;
    if (m_mglTestContext.IsGLInitialized())
    {
        // @@@ free all GL resources, or let them die with mglTestContext?
        if (m_VertexPgmId)
        {
            glDeleteProgramsLW(1, &m_VertexPgmId);
            m_VertexPgmId = 0;
        }
        if (m_FragmentPgmId)
        {
            glDeleteProgramsLW(1, &m_FragmentPgmId);
            m_FragmentPgmId = 0;
        }
        if (m_VertexShaderId)
        {
            glDetachShader(m_GraphicsProgramId, m_VertexShaderId);
            glDeleteShader(m_VertexShaderId);
            m_VertexShaderId = 0;
        }
        if (m_FragmentShaderId)
        {
            glDetachShader(m_GraphicsProgramId, m_FragmentShaderId);
            glDeleteShader(m_FragmentShaderId);
            m_FragmentShaderId = 0;
        }
        if (m_GraphicsProgramId)
        {
            glUseProgram(0);
            m_GraphicsProgramId = 0;
        }
    }

    rc = m_mglTestContext.Cleanup();
    rc = GpuTest::Cleanup();
    return rc;
}

//------------------------------------------------------------------------------
RC GLDirected::InitFromJs()
{
    RC rc;
    CHECK_RC(GpuTest::InitFromJs());

    // Get GLDirected-specific properties (if any) from JavaScript:
    return rc;
}

//------------------------------------------------------------------------------------------------
RC GLDirected::SetupBug2242089()
{
    return m_mglTestContext.SetProperties(
                 GetTestConfiguration(),
                 false,  // double buffer
                 GetBoundGpuDevice(),
                 GetDispMgrReqs(),
                 0,      // don't override color format
                 false,  // Force render to framebuffer object
                 false,  // Don't render FBOs to sysmem.
                 0       // Do not use layered FBO
                 );
}
//------------------------------------------------------------------------------------------------
// This function is used to reproduce hardware bug identified in B2242089.
// Note there are two versions of shaders (lw_gpu_program & GLSL) both sets of shaders
// have been verified to produce the failure (a miscompare).
// To reproduce the failure you need to do one run with -tile_size_128x128 and a second run
// with -tile_size_256x256.
RC GLDirected::RunBug2242089()
{
    RC rc;
    //------------------------------------------------------------------------------
    // These can be overridden from JS at runtime.
    string  vertexShader =
    "#version 430 compatibility \n"
    "out gl_PerVertex \n"
    "{\n" //$
    "    vec4 gl_Position; \n"
    "    vec4 gl_TexCoord[1]; \n"
    "    vec4 gl_Color; \n"
    "};\n"
    "void main() \n"
    "{\n" //$
    "    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex; \n"
    "    gl_TexCoord[0] = gl_MultiTexCoord0; \n"
    "}\n";

    string fragmentShader =
    "#version 430  compatibility // GL version 4.3 assumed \n"
    "in gl_PerFragment \n"
    "{\n" //$
    "    vec4 gl_FragCoord; \n"
    "    vec4 gl_TexCoord[1]; \n"
    "    vec4 gl_Color; \n"
    "};\n"
    "void main() \n"
    "{\n" //$
    "    vec4 color; \n"
    "    color = vec4(0.0); \n"
    "    color.xyz = gl_Color.xyz; \n"
    "    color.xyz = abs(gl_TexCoord[0]).xyz; \n"
    "    gl_FragColor = fract(color); \n"
    "}\n";

    string vertexProgram =
    "!!LWvp5.0 \n"
    "#Fixed Pgm TemplateAttrUse \n"
    "PARAM pgmElw[256] = { program.elw[0..255] }; \n"
    "TEMP pos; \n"
    "main: \n"
    " DP4 pos.x, pgmElw[0], vertex.position; \n"
    " DP4 pos.y, pgmElw[1], vertex.position; \n"
    " DP4 pos.z, pgmElw[2], vertex.position; \n"
    " DP4 pos.w, pgmElw[3], vertex.position; \n"
    " MOV result.position, pos; \n"
    " MOV result.texcoord[0], vertex.texcoord[0]; \n"
    "END \n";

    string fragmentProgram =
    "!!LWfp5.0 \n"
    "OPTION LW_gpu_program_fp64; \n"
    "OPTION LW_internal; \n"
    "#Simple_template \n"
    "#RND_GPU_PROG_FR_END_STRATEGY_frc \n"
    "PARAM pgmElw[256] = { program.elw[0..255] }; \n"
    "main: \n"
    " MOV result.color.w, {0.0, 0.0, 0.0, 0.0}; \n" //$
    " FRC result.color.xyz, |fragment.texcoord[0].xyz|; \n"
    "END \n";

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-10240, 10240, -7680, 7680, 7680, 23040);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslated(0, 0, -15360);

    glClearColor(0.25, 0.5, 0.75, 0.25);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT); // Clear #0002

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslated(0, 0, -15360);
    glTranslatef(-7716, -3700, -3725);

    glTrackMatrixLW(GL_VERTEX_PROGRAM_LW,
                    0, // GpuPgmGen::ModelViewProjReg
                    GL_MODELVIEW_PROJECTION_LW,
                    GL_IDENTITY_LW);
    glTrackMatrixLW(GL_VERTEX_PROGRAM_LW,
                    4, //GpuPgmGen::ModelViewIlwReg,
                    GL_MODELVIEW,
                    GL_ILWERSE_TRANSPOSE_LW);

    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_ALWAYS, (GLclampf)0.282259);
    glBlendColor((GLclampf)0.508817, (GLclampf)0.868028, (GLclampf)0.280533, (GLclampf)0.719058);
    glEnable(GL_BLEND);
    glBlendEquationSeparateEXT(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_ONE_MINUS_SRC_COLOR,
                        GL_ONE_MINUS_DST_ALPHA,
                        GL_ONE_MINUS_SRC_COLOR,
                        GL_ONE_MINUS_DST_ALPHA);

    glEnable(GL_CONSERVATIVE_RASTERIZATION_LW);
    glConservativeRasterParameterfLW(GL_CONSERVATIVE_RASTER_DILATE_LW, 0);
    glConservativeRasterParameteriLW(GL_CONSERVATIVE_RASTER_MODE_LW,
                                     GL_CONSERVATIVE_RASTER_MODE_PRE_SNAP_TRIANGLES_LW);
    glSubpixelPrecisionBiasLW(6, 3);

    glPolygonMode(GL_FRONT, GL_FILL);
    glPolygonMode(GL_BACK, GL_FILL);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset((GLfloat)-0.308738, (GLfloat)1.0);

    CHECK_RC(mglGetRC());
    if (m_UseGLSLShaders == 1)
    {
        // Load default shaders if user has not defined any
        if (m_VertexShader.empty())
        {
            m_VertexShader = vertexShader;
        }
        if (m_FragmentShader.empty())
        {
            m_FragmentShader = fragmentShader;
        }

        CHECK_RC(LoadGLSLShaders(m_VertexShader, m_FragmentShader));
    }
    else
    {
        if (m_VertexProgram.empty())
        {
            m_VertexProgram = vertexProgram;
        }
        CHECK_RC(LoadGpuPgm(&m_VertexPgmId, GL_VERTEX_PROGRAM_LW, m_VertexProgram));

        if (m_FragmentProgram.empty())
        {
            m_FragmentProgram = fragmentProgram;
        }
        CHECK_RC(LoadGpuPgm(&m_FragmentPgmId, GL_FRAGMENT_PROGRAM_LW, m_FragmentProgram));
    }

    glNormal3f(0, 1, 0);
    glColor4ub(164, 94, 202, 145);
    glSecondaryColor3ub(77, 35, 145);
    glMultiTexCoord4f(GL_TEXTURE0,
                      (GLfloat)-0.166667,
                      (GLfloat)-0.5,
                      (GLfloat)0,
                      (GLfloat)1.02259);

    glBegin(GL_TRIANGLE_STRIP);
        glVertex4s(5120, 7680, 89, 1);
        glVertex4s(5120, 5761, 166, 1);
        glVertex4s(5120, 7680, 89, 1);
        glVertex4s(8961, 5761, 50, 1);
        glVertex4s(5120, 7680, 89, 1);
        glVertex4s(1285, 5761, 50, 1);
        glVertex4s(5120, 7680, 89, 1);
        glVertex4s(5120, 5761, 166, 1);
    glEnd();
    glBegin(GL_TRIANGLE_STRIP);
        glVertex4s(5120, 5761, 166, 1);
        glVertex4s(5120, 1923, 167, 1);
        glVertex4s(8961, 5761, 50, 1);
        glVertex4s(8964, 1923, 50, 1);
        glVertex4s(1285, 5761, 50, 1);
        glVertex4s(1282, 1923, 50, 1);
        glVertex4s(5120, 5761, 166, 1);
        glVertex4s(5120, 1923, 167, 1);
    glEnd();
    glBegin(GL_TRIANGLE_STRIP);
        glVertex4s(5120, 1923, 167, 1);
        glVertex4s(5120, 0, 89, 1);
        glVertex4s(8964, 1923, 50, 1);
        glVertex4s(5120, 0, 89, 1);
        glVertex4s(1282, 1923, 50, 1);
        glVertex4s(5120, 0, 89, 1);
        glVertex4s(5120, 1923, 167, 1);
        glVertex4s(5120, 0, 89, 1);
    glEnd();
    glFinish();
    CHECK_RC(mglGetRC());

    if (m_DumpImage & 1)
    {
        CHECK_RC(DumpImage(false, "glDirected_c.png"));
    }
    if (m_DumpImage & 2)
    {
        CHECK_RC(DumpImage(true, "glDirected_z.png"));
    }
    return (rc);
}

