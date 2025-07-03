/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file modsgl.h
//! \brief Portable OpenGL include file for MODS.
//!
//! Most mods platforms use our own  LW_MODS or "dgl" version of the GL driver.
//! Some use other GL drivers (i.e. the GL-ES driver for embedded systems).
//! This header file gives standard GL support across all supported platforms.
//!
//! In addition to portability, utility functions are declared here, for use
//!  by GL based tests.
//!

#ifndef INCLUDED_MODSGL_H
#define INCLUDED_MODSGL_H

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif
#ifndef INCLUDED_FSAA_H
#include "core/include/fsaa.h"
#endif
#ifndef INCLUDED_COLOR_H
#include "core/include/color.h"
#endif
#ifndef INCLUDED_DISPMGR_H
#include "gpu/include/dispmgr.h"
#endif

#ifdef INCLUDE_OGL
#include "GL/gl.h"
#include "GL/glext.h"
#include "GL/lwogldebug.h"
#include "g_debugMessages.h"
#endif

#ifndef _cl0080_h_
#include "class/cl0080.h"
#endif

// TODOWJM: Ugh, need a clean way to include GLES/gl.h.  Might have to add
// it to the default p4 client for everything?
#ifdef INCLUDE_OGL_ES
// Undefine __gl_h_ so that we can include FULL gl.h and ES version of gl.h
#ifdef __gl_h_
#undef __gl_h_
#endif
#include "GLES/gl.h"
#include "GLES/glext.h"
#include "GL/lwgle.h"
typedef double GLdouble;
#endif

#include "core/include/GL/glu.h"
#include "dgl.h"
#ifndef INCLUDED_STL_VECTOR
#define INCLUDED_STL_VECTOR
#include <vector>
#endif

#include <math.h>       // expm1()
//-------------------------------------------------------------------------------------------------
// Use the expm1() function to generate exp() results. We are seeing several machines producing
// different values for the exp() however expm1() appears to be more consistent across these
// machines. For more details see bug 1924343
#define EXP(x) (expm1(x)+1.0)

class Surface2D;
class GpuDevice;
class GpuSubdevice;
class GpuTestConfiguration;
class mglDesktop;

#if defined(DEBUG)
   #define MASSERT_NO_GLERROR()\
   do {                       \
      RC rc = mglGetRC();     \
      MASSERT(OK == rc);      \
   } while (0)
#else
   #define MASSERT_NO_GLERROR()  (void)(0)
#endif

//! Returns true if this HW is supported (accellerated) by the driver.
//! For example, returns false for lw11 in rel95 and later.
bool mglHwIsSupported(GpuSubdevice *pSubdevice = 0);

//! \class mglTestContext -- manage mods-GL context for a GpuTest.
//!
//! Each GpuTest that uses OpenGL should contain one of these objects.
//!
class mglTestContext
{
public:
    mglTestContext();
    ~mglTestContext();

    RC SetProperties
    (
        GpuTestConfiguration * pTstCfg,
        bool   DoubleBuffer,
        GpuDevice * pGpuDev,
        DisplayMgr::Requirements reqs,
        UINT32 ColorSurfaceFormatOverride,
        bool   ForceFbo,
        bool   RenderToSysmem,
        INT32  NumLayersInFBO
    );
    void SetSurfSize
    (
        UINT32 surfWidth,
        UINT32 surfHeight
    );
    RC AddColorBuffer
    (
        UINT32 ColorSurfaceFormat
    );
    bool IsSupported();
    RC Setup();
    RC Cleanup();

    RC CopyFboToFront();
    bool        IsGLInitialized() const;
    UINT32      ColorSurfaceFormat() const;
    UINT32      ColorSurfaceFormat(UINT32 colorIndex) const;
    UINT32      NumColorSurfaces() const;
    UINT32      NumColorSamples() const;
    Surface2D * GetPrimarySurface() const;
    DisplayMgr::TestContext *   GetTestContext() const;
    Display *   GetDisplay() const;
    UINT32      GetPrimaryDisplay() const;
    bool        UsingFbo() const;
    UINT32      GetFboId(){return m_FboId;}
    DisplayMgr::Requirements GetReqs() const;
    void        SetDumpECover(bool dumpECover);
    RC          UpdateDisplay();
    void        GetSurfaceTextureIDs(UINT32 *depthTextureID,
                                     UINT32 *colorSurfaceID) const;
    void SwapBuffers();
    void        AttachTexToFBO(GLenum attachment, GLuint texture) const;

    static void SpecifyTexImage(GLenum dim, GLint level, GLint internalFormat,
            GLsizei width, GLsizei height, GLsizei depth,
            GLint border, GLenum format, GLenum type, GLvoid *data);

private:
    // Properties stored by SetProperties.
    bool                m_GLInitialized;  // If true, its ok to make calls to the GL driver.
    UINT32              m_DispWidth;
    UINT32              m_DispHeight;
    UINT32              m_SurfWidth;
    UINT32              m_SurfHeight;
    UINT32              m_DispDepth;
    UINT32              m_ZDepth;
    UINT32              m_DispRR;
    FSAAMode            m_FsaaMode;
    bool                m_DoubleBuffer;
    GpuDevice *         m_pGpuDev;
    DisplayMgr::Requirements m_DisplayMgrReqs;
    bool                m_BlockLinear;
    UINT32              m_AA_METHOD;
    UINT32              m_Samples;
    bool                m_ColorCompress;
    UINT32              m_DownsampleHint;
    bool                m_PropertiesOK;
    vector<UINT32>      m_SurfColorFormats;
    bool                m_RenderToSysmem;
    bool                m_UsingFbo;
    bool                m_HaveFrontbuffer;
    bool                m_EnableEcovDumping;
    INT32               m_NumLayersInFBO;
    enum
    {
        //! Index to Fbo textures, z/s surface
        FBO_TEX_DEPTH_STENCIL,
        //! Index to Fbo textures, color surface 0
        FBO_TEX_COLOR0
    };

    // Context allocated by Setup and freed by Cleanup:
    void *              m_pGrCtx;
    void *              m_pDrawable;
    mglDesktop *        m_pDesktop;  //!< Shared with other mglTestContexts.
    GLuint              m_FboId;
    vector<GLuint>      m_FboTexIds;
    GLuint              m_CopyFboFragProgId;

    //! Number of mglTestContexts that are lwrrently active.
    //! When this goes from 0->1, we start GL, and when it goes from
    //! 1->0 we stop the GL driver and free its persistent resources.
    static UINT32       s_NumStarted;

    //! Global mutex to serialize calls by multiple threads into the
    //! Setup and Cleanup functions.
    static void*        s_Mutex;

    //! Count how many contexts exist now, so that we know when to free s_Mutex.
    static UINT32       s_NumContexts;
};

//! Simulates a DLL process-detach.
RC mglShutdownLibrary();

//! Wrapper for wglSwapBuffers(HDC) and aglSwapBuffers(AGLContext).
void mglSwapBuffers();

enum MglDispatchMode
{
   mglSoftware = 0,              //!< pure software rasterizer fallback rendering
   mglVpipe    = 1,              //!< sw T&L, hw everything else
   mglHardware = 2,              //!< pure hardware
   mglUnknown  = 0xffffffff      //!< this is not a MODS GL driver.
};

//! Get an enumerated value, of the current rendering mode (gc->dispatchMode)
MglDispatchMode mglGetDispatchMode();

//! Get and clear the current resman robust-channels error code (if any).
//! The special MODS GL drivers keep a global variable with the first
//! robust channels error that oclwrs on a GL pushbuffer.
//! The error is translated into the matching mods RC, or OK if no error.
//! If not a MODS GL driver, always returns OK (i.e. 0).
RC mglGetChannelError();

//! Check whether an extension is supported
bool mglExtensionSupported(const char *extension);

//! Prevent mods from detecting this extension (for debugging).
//! Pass a null pointer to clear out the suppression list.
void mglSuppressExtension(string extension);

//! Remove a GL extension from the suppression list (debug)
void mglUnsuppressExtension(string extension);

//! Get & clear global OpenGL error code, translate it into a MODS RC.
RC mglGetRC();

//! Colwert a gl error enumerant to a MODS RC (doesn't touch global GL error code).
RC mglErrorToRC(GLenum err);

//! Check that framebuffer_object state is consistent.
RC mglCheckFbo();

//! Load a vertex or fragment program, check for syntax errors,
//! and print a message with error location.
RC mglLoadProgram(GLuint target, GLuint id, GLsizei len, const GLubyte * str);

//! Set a rendering window to cover offset (XOffset, YOffset
void mglSetWindow(int XOffset, int YOffset, int Width, int Height);

namespace ModsGL
{
//! (GLSL) Load a shader, check for and display syntax errors.
//! *pId is the shader id, which if !0 will be freed and re-created.
RC LoadShader(GLuint target, GLuint *pId, const char * str);

//! (GLSL) Wrapper around glLinkProgram, with error reporting.
RC LinkProgram(GLuint id);

//! Wrapper for a GLSL uniform variable (const in shader, RW from C++).
//! The supported types for T are: GLfloat, GLint, GLuint
template <class T> class Uniform
{
public:
    Uniform(const char * name) : m_Name(name), m_ProgId(0), m_Loc(0) {}
    ~Uniform() {}

    void Init(GLuint programId)
    {
        m_ProgId = programId;
        m_Loc = glGetUniformLocation(m_ProgId, m_Name);
    }
    const char * Name() const   { return m_Name; }
    GLuint       ProgId() const { return m_ProgId; }
    GLuint       Loc() const    { return m_Loc; }

private:
    const char * m_Name;
    GLuint m_ProgId;
    GLuint m_Loc;
};
void GetUniform(const Uniform<GLfloat> & u, GLfloat * p);
void GetUniform(const Uniform<GLint> & u, GLint * p);
void GetUniform(const Uniform<GLuint> & u, GLuint * p);
void GetUniform(const Uniform<GLint64EXT> & u, GLint64EXT * p);
void GetUniform(const Uniform<GLuint64EXT> & u, GLuint64EXT * p);
void SetUniform(const Uniform<GLfloat> & u, GLfloat x);
void SetUniform(const Uniform<GLfloat> & u, GLfloat x, GLfloat y);
void SetUniform(const Uniform<GLfloat> & u, GLfloat x, GLfloat y, GLfloat z);
void SetUniform(const Uniform<GLfloat> & u, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void SetUniform(const Uniform<GLint> & u, GLint x);
void SetUniform(const Uniform<GLint> & u, GLint x, GLint y);
void SetUniform(const Uniform<GLint> & u, GLint x, GLint y, GLint z);
void SetUniform(const Uniform<GLint> & u, GLint x, GLint y, GLint z, GLint w);
void SetUniform(const Uniform<GLuint> & u, GLuint x);
void SetUniform(const Uniform<GLuint> & u, GLuint x, GLuint y);
void SetUniform(const Uniform<GLuint> & u, GLuint x, GLuint y, GLuint z);
void SetUniform(const Uniform<GLuint> & u, GLuint x, GLuint y, GLuint z, GLuint w);
void SetUniform(const Uniform<GLint64EXT> & u, GLint64EXT x);
void SetUniform(const Uniform<GLint64EXT> & u, GLint64EXT x, GLint64EXT y);
void SetUniform(const Uniform<GLint64EXT> & u, GLint64EXT x, GLint64EXT y, GLint64EXT z);
void SetUniform(const Uniform<GLint64EXT> & u, GLint64EXT x, GLint64EXT y, GLint64EXT z, GLint64EXT w); //$
void SetUniform(const Uniform<GLuint64EXT> & u, GLuint64EXT x);
void SetUniform(const Uniform<GLuint64EXT> & u, GLuint64EXT x, GLuint64EXT y);
void SetUniform(const Uniform<GLuint64EXT> & u, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z);
void SetUniform(const Uniform<GLuint64EXT> & u, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z, GLuint64EXT w); //$

class BindTexture
{
    public:
        BindTexture(GLenum target, GLuint texture) : m_Target(target)
        {
            glBindTexture(m_Target,texture);
        }

        ~BindTexture()
        {
            glBindTexture(m_Target,0);
        }

    private:
        GLenum m_Target;
};
} // namespace ModsGL

enum glEnumZeroOneMode
{
    glezom_False,       // return GL_FALSE for 0
    glezom_None,        // return GL_NONE for 0
    glezom_Disable,     // return DISABLE for 0
    glezom_Zero,        // return GL_ZERO for 0
    glezom_True,        // return GL_TRUE for 1
    glezom_One          // return GL_ONE for 1
};

//! Returns a string constant for the GLenum. Caller can optionally pass
//! in a string to be returned if the default case is hit.
const char * mglEnumToString
(
    GLenum EnumValue,                           //!< GLenum to colwert
    const char *pDefault = 0,                   //!< default string to return if none found
    bool bAssertOnError = false,                //!< assert if enum is not found
    glEnumZeroOneMode zeroMode = glezom_False,  //!< string to return for 0
    glEnumZeroOneMode oneMode = glezom_True     //!< string to return for 1
);

//! Texture format info.
//!
//! Maps ColorUtils::Format to GL texture Internal Format.
//!
//! The GL driver reads (glTexImage) or writes (glReadPixels) the ExtFormat
//! and Type in sysmem and uses the InternalFormat for the actual FB surfaces.
//!
//! To simplify glrandom texture-image generation, we map many Internal Formats
//! to only a few ExtFormat/Types.
//!
//! Since most of mods is not GL-based, we also supply the ColorUtils::Format
//! that matches the ExtFormat/Type, which will be passed to Goldelwalues
//! for use with CRC and PNG utilities operating on glReadPixels output.
//!
struct mglTexFmtInfo
{
    ColorUtils::Format  ColorFormat;    //!< Mods' own color format
    GLenum              InternalFormat; //!< GL's name for the same format
    ColorUtils::Format  ExtColorFormat; //!< Mods' format matching ExtFormat/Type
    GLenum              ExtFormat;      //!< Client-side format
    GLenum              Type;           //!< Client-side data type
    const char *        TexExtension;   //!< Extension needed as texture
};

#ifdef DEBUG
//! Verify that mgl texture formats are consistent with MODS defined color
//! formats in color.h
RC mglVerifyTexFmts ();
#endif

//! Look up info about a particular color format.
const mglTexFmtInfo * mglGetTexFmtInfo (ColorUtils::Format cf);
const mglTexFmtInfo * mglGetTexFmtInfo (GLenum txif);

//! Translates a mods color format (color.h) to a GL internal-texture-format.
GLenum mglColor2TexFmt (ColorUtils::Format cf);

//! Translates a GL internal-texture-format to a mods color format (color.h).
ColorUtils::Format mglTexFmt2Color (GLenum txif);

//! Wrapper class for LW_timer_query.
class mglTimerQuery
{
public:
    mglTimerQuery();
    ~mglTimerQuery();

    enum State
    {
        Idle,       //!< No pending query.
        Begun,      //!< Query begin-point marked in pushbuffer.
        Waiting,    //!< Query begin and end marked, waiting for GPU.
        Unread      //!< Complete, but results not yet reported.
    };
    State   GetState();
    bool    IsSupported() const { return m_Id != 0; }
    void    Begin();
    void    End();
    double  GetSeconds();

private:
    void FinishQuery();

    State  m_State;
    GLuint m_Id;
    double m_Seconds;
};

// Functions to provide ASTC information based on GL surface format enums
UINT08 mglGetAstcFormatBlockX(GLenum format);
UINT08 mglGetAstcFormatBlockY(GLenum format);
bool   mglIsFormatAstc(GLenum format);

#ifdef INCLUDE_OGL
// Declare pointers to extension functions
#define GLEXTTAB(func, type) extern type __mods_##func;
#include "core/include/glexttab.h"
#undef GLEXTTAB
#endif

// Mangle extension function names to match function pointer names -- needed
// to avoid name collisions with the functions themselves

// EXT_depth_bounds_test
#define glDepthBoundsEXT                         __mods_glDepthBoundsEXT

// EXT_stencil_two_side
#define glActiveStencilFaceEXT                   __mods_glActiveStencilFaceEXT

// LW_half_float
#define glVertex2hLW                             __mods_glVertex2hLW
#define glVertex2hvLW                            __mods_glVertex2hvLW
#define glVertex3hLW                             __mods_glVertex3hLW
#define glVertex3hvLW                            __mods_glVertex3hvLW
#define glVertex4hLW                             __mods_glVertex4hLW
#define glVertex4hvLW                            __mods_glVertex4hvLW
#define glNormal3hLW                             __mods_glNormal3hLW
#define glNormal3hvLW                            __mods_glNormal3hvLW
#define glColor3hLW                              __mods_glColor3hLW
#define glColor3hvLW                             __mods_glColor3hvLW
#define glColor4hLW                              __mods_glColor4hLW
#define glColor4hvLW                             __mods_glColor4hvLW
#define glTexCoord1hLW                           __mods_glTexCoord1hLW
#define glTexCoord1hvLW                          __mods_glTexCoord1hvLW
#define glTexCoord2hLW                           __mods_glTexCoord2hLW
#define glTexCoord2hvLW                          __mods_glTexCoord2hvLW
#define glTexCoord3hLW                           __mods_glTexCoord3hLW
#define glTexCoord3hvLW                          __mods_glTexCoord3hvLW
#define glTexCoord4hLW                           __mods_glTexCoord4hLW
#define glTexCoord4hvLW                          __mods_glTexCoord4hvLW
#define glMultiTexCoord1hLW                      __mods_glMultiTexCoord1hLW
#define glMultiTexCoord1hvLW                     __mods_glMultiTexCoord1hvLW
#define glMultiTexCoord2hLW                      __mods_glMultiTexCoord2hLW
#define glMultiTexCoord2hvLW                     __mods_glMultiTexCoord2hvLW
#define glMultiTexCoord3hLW                      __mods_glMultiTexCoord3hLW
#define glMultiTexCoord3hvLW                     __mods_glMultiTexCoord3hvLW
#define glMultiTexCoord4hLW                      __mods_glMultiTexCoord4hLW
#define glMultiTexCoord4hvLW                     __mods_glMultiTexCoord4hvLW
#define glFogCoordhLW                            __mods_glFogCoordhLW
#define glFogCoordhvLW                           __mods_glFogCoordhvLW
#define glSecondaryColor3hLW                     __mods_glSecondaryColor3hLW
#define glSecondaryColor3hvLW                    __mods_glSecondaryColor3hvLW
#define glVertexAttrib1hLW                       __mods_glVertexAttrib1hLW
#define glVertexAttrib1hvLW                      __mods_glVertexAttrib1hvLW
#define glVertexAttrib2hLW                       __mods_glVertexAttrib2hLW
#define glVertexAttrib2hvLW                      __mods_glVertexAttrib2hvLW
#define glVertexAttrib3hLW                       __mods_glVertexAttrib3hLW
#define glVertexAttrib3hvLW                      __mods_glVertexAttrib3hvLW
#define glVertexAttrib4hLW                       __mods_glVertexAttrib4hLW
#define glVertexAttrib4hvLW                      __mods_glVertexAttrib4hvLW
#define glVertexAttribs1hvLW                     __mods_glVertexAttribs1hvLW
#define glVertexAttribs2hvLW                     __mods_glVertexAttribs2hvLW
#define glVertexAttribs3hvLW                     __mods_glVertexAttribs3hvLW
#define glVertexAttribs4hvLW                     __mods_glVertexAttribs4hvLW
#define glVertexAttrib3f                         __mods_glVertexAttrib3f

// LW_occlusion_query
#define glGenOcclusionQueriesLW                  __mods_glGenOcclusionQueriesLW
#define glDeleteOcclusionQueriesLW               __mods_glDeleteOcclusionQueriesLW
#define glIsOcclusionQueryLW                     __mods_glIsOcclusionQueryLW
#define glBeginOcclusionQueryLW                  __mods_glBeginOcclusionQueryLW
#define glEndOcclusionQueryLW                    __mods_glEndOcclusionQueryLW
#define glGetOcclusionQueryivLW                  __mods_glGetOcclusionQueryivLW
#define glGetOcclusionQueryuivLW                 __mods_glGetOcclusionQueryuivLW

// GL_ARB_occlusion_query
#define glGenQueriesARB                          __mods_glGenQueriesARB
#define glDeleteQueriesARB                       __mods_glDeleteQueriesARB
#define glIsQueryARB                             __mods_glIsQueryARB
#define glBeginQueryARB                          __mods_glBeginQueryARB
#define glEndQueryARB                            __mods_glEndQueryARB
#define glGetQueryObjectivARB                    __mods_glGetQueryObjectivARB
#define glGetQueryObjectuivARB                   __mods_glGetQueryObjectuivARB
#define glGetQueryivARB                          __mods_glGetQueryivARB

// GL_EXT_timer_query
#define glGetQueryObjecti64vEXT                  __mods_glGetQueryObjecti64vEXT
#define glGetQueryObjectui64vEXT                 __mods_glGetQueryObjectui64vEXT

// LW_primitive_restart
#define glPrimitiveRestartLW                     __mods_glPrimitiveRestartLW
#define glPrimitiveRestartIndexLW                __mods_glPrimitiveRestartIndexLW

// LW_register_combiners
#define glCombinerParameterfvLW                  __mods_glCombinerParameterfvLW
#define glCombinerParameterfLW                   __mods_glCombinerParameterfLW
#define glCombinerParameterivLW                  __mods_glCombinerParameterivLW
#define glCombinerParameteriLW                   __mods_glCombinerParameteriLW
#define glCombinerInputLW                        __mods_glCombinerInputLW
#define glCombinerOutputLW                       __mods_glCombinerOutputLW
#define glFinalCombinerInputLW                   __mods_glFinalCombinerInputLW
#define glGetCombinerInputParameterfvLW          __mods_glGetCombinerInputParameterfvLW
#define glGetCombinerInputParameterivLW          __mods_glGetCombinerInputParameterivLW
#define glGetCombinerOutputParameterfvLW         __mods_glGetCombinerOutputParameterfvLW
#define glGetCombinerOutputParameterivLW         __mods_glGetCombinerOutputParameterivLW
#define glGetFinalCombinerInputParameterfvLW     __mods_glGetFinalCombinerInputParameterfvLW
#define glGetFinalCombinerInputParameterivLW     __mods_glGetFinalCombinerInputParameterivLW

// LW_register_combiners2
#define glCombinerStageParameterfvLW             __mods_glCombinerStageParameterfvLW
#define glGetCombinerStageParameterfvLW          __mods_glGetCombinerStageParameterfvLW

// LW_vertex_program
#define glAreProgramsResidentLW                  __mods_glAreProgramsResidentLW
#define glBindProgramLW                          __mods_glBindProgramLW
#define glDeleteProgramsLW                       __mods_glDeleteProgramsLW
#define glExelwteProgramLW                       __mods_glExelwteProgramLW
#define glGenProgramsLW                          __mods_glGenProgramsLW
#define glGetProgramParameterdvLW                __mods_glGetProgramParameterdvLW
#define glGetProgramParameterfvLW                __mods_glGetProgramParameterfvLW
#define glGetProgramivLW                         __mods_glGetProgramivLW
#define glGetProgramStringLW                     __mods_glGetProgramStringLW
#define glGetTrackMatrixivLW                     __mods_glGetTrackMatrixivLW
#define glGetVertexAttribdvLW                    __mods_glGetVertexAttribdvLW
#define glGetVertexAttribfvLW                    __mods_glGetVertexAttribfvLW
#define glGetVertexAttribivLW                    __mods_glGetVertexAttribivLW
#define glGetVertexAttribPointervLW              __mods_glGetVertexAttribPointervLW
#define glIsProgramLW                            __mods_glIsProgramLW
#define glLoadProgramLW                          __mods_glLoadProgramLW
#define glProgramStringARB                       __mods_glProgramStringARB
#define glProgramParameter4dLW                   __mods_glProgramParameter4dLW
#define glProgramParameter4dvLW                  __mods_glProgramParameter4dvLW
#define glProgramParameter4fLW                   __mods_glProgramParameter4fLW
#define glProgramParameter4fvLW                  __mods_glProgramParameter4fvLW
#define glProgramParameters4dvLW                 __mods_glProgramParameters4dvLW
#define glProgramParameters4fvLW                 __mods_glProgramParameters4fvLW
#define glRequestResidentProgramsLW              __mods_glRequestResidentProgramsLW
#define glTrackMatrixLW                          __mods_glTrackMatrixLW
#define glVertexAttribPointerLW                  __mods_glVertexAttribPointerLW
#define glVertexAttrib1dLW                       __mods_glVertexAttrib1dLW
#define glVertexAttrib1dvLW                      __mods_glVertexAttrib1dvLW
#define glVertexAttrib1fLW                       __mods_glVertexAttrib1fLW
#define glVertexAttrib1fvLW                      __mods_glVertexAttrib1fvLW
#define glVertexAttrib1sLW                       __mods_glVertexAttrib1sLW
#define glVertexAttrib1svLW                      __mods_glVertexAttrib1svLW
#define glVertexAttrib2dLW                       __mods_glVertexAttrib2dLW
#define glVertexAttrib2dvLW                      __mods_glVertexAttrib2dvLW
#define glVertexAttrib2fLW                       __mods_glVertexAttrib2fLW
#define glVertexAttrib2fvLW                      __mods_glVertexAttrib2fvLW
#define glVertexAttrib2sLW                       __mods_glVertexAttrib2sLW
#define glVertexAttrib2svLW                      __mods_glVertexAttrib2svLW
#define glVertexAttrib3dLW                       __mods_glVertexAttrib3dLW
#define glVertexAttrib3dvLW                      __mods_glVertexAttrib3dvLW
#define glVertexAttrib3fLW                       __mods_glVertexAttrib3fLW
#define glVertexAttrib3fvLW                      __mods_glVertexAttrib3fvLW
#define glVertexAttrib3sLW                       __mods_glVertexAttrib3sLW
#define glVertexAttrib3svLW                      __mods_glVertexAttrib3svLW
#define glVertexAttrib4dLW                       __mods_glVertexAttrib4dLW
#define glVertexAttrib4dvLW                      __mods_glVertexAttrib4dvLW
#define glVertexAttrib4fLW                       __mods_glVertexAttrib4fLW
#define glVertexAttrib4fvLW                      __mods_glVertexAttrib4fvLW
#define glVertexAttrib4sLW                       __mods_glVertexAttrib4sLW
#define glVertexAttrib4svLW                      __mods_glVertexAttrib4svLW
#define glVertexAttrib4ubLW                      __mods_glVertexAttrib4ubLW
#define glVertexAttrib4ubvLW                     __mods_glVertexAttrib4ubvLW
#define glVertexAttribs1dvLW                     __mods_glVertexAttribs1dvLW
#define glVertexAttribs1fvLW                     __mods_glVertexAttribs1fvLW
#define glVertexAttribs1svLW                     __mods_glVertexAttribs1svLW
#define glVertexAttribs2dvLW                     __mods_glVertexAttribs2dvLW
#define glVertexAttribs2fvLW                     __mods_glVertexAttribs2fvLW
#define glVertexAttribs2svLW                     __mods_glVertexAttribs2svLW
#define glVertexAttribs3dvLW                     __mods_glVertexAttribs3dvLW
#define glVertexAttribs3fvLW                     __mods_glVertexAttribs3fvLW
#define glVertexAttribs3svLW                     __mods_glVertexAttribs3svLW
#define glVertexAttribs4dvLW                     __mods_glVertexAttribs4dvLW
#define glVertexAttribs4fvLW                     __mods_glVertexAttribs4fvLW
#define glVertexAttribs4svLW                     __mods_glVertexAttribs4svLW
#define glVertexAttribs4ubvLW                    __mods_glVertexAttribs4ubvLW

// EXT_blend_equation_separate
#define glBlendEquationSeparateEXT               __mods_glBlendEquationSeparateEXT

// ARB_vertex_program
#define glGetProgramivARB                        __mods_glGetProgramivARB

// GL_EXT_framebuffer_object
#define glIsRenderbufferEXT                      __mods_glIsRenderbufferEXT
#define glBindRenderbufferEXT                    __mods_glBindRenderbufferEXT
#define glDeleteRenderbuffersEXT                 __mods_glDeleteRenderbuffersEXT
#define glGenRenderbuffersEXT                    __mods_glGenRenderbuffersEXT
#define glRenderbufferStorageEXT                 __mods_glRenderbufferStorageEXT
#define glGetRenderbufferParameterivEXT          __mods_glGetRenderbufferParameterivEXT
#define glIsFramebufferEXT                       __mods_glIsFramebufferEXT
#define glBindFramebufferEXT                     __mods_glBindFramebufferEXT
#define glDeleteFramebuffersEXT                  __mods_glDeleteFramebuffersEXT
#define glGenFramebuffersEXT                     __mods_glGenFramebuffersEXT
#define glCheckFramebufferStatusEXT              __mods_glCheckFramebufferStatusEXT
#define glFramebufferTexture1DEXT                __mods_glFramebufferTexture1DEXT
#define glFramebufferTexture2DEXT                __mods_glFramebufferTexture2DEXT
#define glFramebufferTexture3DEXT                __mods_glFramebufferTexture3DEXT
#define glFramebufferTextureEXT                  __mods_glFramebufferTextureEXT
#define glFramebufferRenderbufferEXT             __mods_glFramebufferRenderbufferEXT
#define glGetFramebufferAttachmentParameterivEXT __mods_glGetFramebufferAttachmentParameterivEXT
#define glGenerateMipmapEXT                      __mods_glGenerateMipmapEXT
#define glBlitFramebufferEXT                     __mods_glBlitFramebufferEXT

// Transform Feedback Extentions
#define glBeginTransformFeedbackLW              __mods_glBeginTransformFeedbackLW
#define glEndTransformFeedbackLW                __mods_glEndTransformFeedbackLW
#define glTransformFeedbackAttribsLW            __mods_glTransformFeedbackAttribsLW
#define glBindBufferRangeLW                     __mods_glBindBufferRangeLW
#define glBindBufferOffsetLW                    __mods_glBindBufferOffsetLW
#define glBindBufferBaseLW                      __mods_glBindBufferBaseLW
#define glTransformFeedbackVaryingsLW           __mods_glTransformFeedbackVaryingsLW
#define glActiveVaryingLW                       __mods_glActiveVaryingLW
#define glGetVaryingLocationLW                  __mods_glGetVaryingLocationLW
#define glGetActiveVaryingLW                    __mods_glGetActiveVaryingLW
#define glGetTransformFeedbackVaryingLW         __mods_glGetTransformFeedbackVaryingLW

// ARB_draw_buffers
#define glDrawBuffersARB                        __mods_glDrawBuffersARB

// EXT_draw_buffers2
#define glColorMaskIndexedEXT                   __mods_glColorMaskIndexedEXT
#define glGetBooleanIndexedvEXT                 __mods_glGetBooleanIndexedvEXT
#define glGetIntegerIndexedvEXT                 __mods_glGetIntegerIndexedvEXT
#define glEnableIndexedEXT                      __mods_glEnableIndexedEXT
#define glDisableIndexedEXT                     __mods_glDisableIndexedEXT
#define glIsEnabledIndexedEXT                   __mods_glIsEnabledIndexedEXT

// GL_ARB_draw_buffers_blend
#define glBlendEquationSeparateiARB             __mods_glBlendEquationSeparateiARB
#define glBlendFuncSeparateiARB                 __mods_glBlendFuncSeparateiARB

// GL_EXT_framebuffer_multisample
#define glRenderbufferStorageMultisampleEXT     __mods_glRenderbufferStorageMultisampleEXT

// GL_LW_explicit_multisample
#define glGetMultisamplefvLW                    __mods_glGetMultisamplefvLW
#define glSampleMaskIndexedLW                   __mods_glSampleMaskIndexedLW
#define glTexRenderbufferLW                     __mods_glTexRenderbufferLW

// GL_LW_gpu_program4
#define glProgramElwParameterI4iLW              __mods_glProgramElwParameterI4iLW
#define glProgramElwParameterI4ivLW             __mods_glProgramElwParameterI4ivLW
#define glProgramElwParametersI4ivLW            __mods_glProgramElwParametersI4ivLW
#define glProgramElwParameterI4uiLW             __mods_glProgramElwParameterI4uiLW
#define glProgramElwParameterI4uivLW            __mods_glProgramElwParameterI4uivLW
#define glProgramElwParametersI4uivLW           __mods_glProgramElwParametersI4uivLW
#define glProgramElwParameter4fARB              __mods_glProgramElwParameter4fARB
#define glProgramElwParameter4fvARB             __mods_glProgramElwParameter4fvARB

#define glGetProgramElwParameterIivLW           __mods_glGetProgramElwParameterIivLW
#define glGetProgramElwParameterIuivLW          __mods_glGetProgramElwParameterIuivLW

#define glProgramLocalParameterI4uiLW           __mods_glProgramLocalParameterI4uiLW
#define glProgramLocalParameterI4uivLW          __mods_glProgramLocalParameterI4uivLW
#define glProgramLocalParametersI4uivLW         __mods_glProgramLocalParametersI4uivLW

#define glGetProgramLocalParameterIivLW         __mods_glGetProgramLocalParameterIivLW
#define glGetProgramLocalParameterIuivLW        __mods_glGetProgramLocalParameterIuivLW
// GL_LW_tessellation_program5
#define glPatchParameteri                       __mods_glPatchParameteri
#define glPatchParameterfv                      __mods_glPatchParameterfv
#define glPatchParameteriLW                     __mods_glPatchParameteriLW
#define glPatchParameterfvLW                    __mods_glPatchParameterfvLW

// GL_ARB_color_buffer_float
#define glClampColorARB                         __mods_glClampColorARB

// GL_LW_shader_buffer_load
#define glMakeBufferResidentLW                  __mods_glMakeBufferResidentLW
#define glMakeBufferNonResidentLW               __mods_glMakeBufferNonResidentLW
#define glIsBufferResidentLW                    __mods_glIsBufferResidentLW
#define glGetBufferParameterui64vLW             __mods_glGetBufferParameterui64vLW
#define glGetIntegerui64vLW                     __mods_glGetIntegerui64vLW
#define glMakeNamedBufferResidentLW             __mods_glMakeNamedBufferResidentLW
#define glMakeNamedBufferNonResidentLW          __mods_glMakeNamedBufferNonResidentLW
#define glIsNamedBufferResidentLW               __mods_glIsNamedBufferResidentLW
#define glMemoryBarrierEXT                      __mods_glMemoryBarrierEXT

// GL_LW_parameter_buffer_object
#define glProgramBufferParametersfvLW           __mods_glProgramBufferParametersfvLW
#define glProgramBufferParametersIivLW          __mods_glProgramBufferParametersIivLW
#define glProgramBufferParametersIuivLW         __mods_glProgramBufferParametersIuivLW
#define glGenBuffersARB                         __mods_glGenBuffersARB
#define glBindBufferARB                         __mods_glBindBufferARB
#define glBufferDataARB                         __mods_glBufferDataARB

// GL_ARB_shader_subroutine
#define glProgramSubroutineParametersuivLW      __mods_glProgramSubroutineParametersuivLW

// GL_EXT_shader_image_load_store
#define glBindImageTextureEXT                   __mods_glBindImageTextureEXT

// GL_LW_copy_image
#define glCopyImageSubDataEXT                   __mods_glCopyImageSubDataEXT

// GL_LW_bindless_texture
#define glGetTextureHandleLW                            __mods_glGetTextureHandleLW
#define glMakeTextureHandleResidentLW                   __mods_glMakeTextureHandleResidentLW
#define glMakeTextureHandleNonResidentLW                __mods_glMakeTextureHandleNonResidentLW
#define glIsTextureHandleResidentLW                     __mods_glIsTextureHandleResidentLW

// GL_EXT_direct_state_access
#define glNamedBufferDataEXT                    __mods_glNamedBufferDataEXT
#define glNamedBufferSubDataEXT                 __mods_glNamedBufferSubDataEXT
#define glGetNamedBufferSubDataEXT              __mods_glGetNamedBufferSubDataEXT
#define glMapNamedBufferEXT                     __mods_glMapNamedBufferEXT
#define glUnmapNamedBufferEXT                   __mods_glUnmapNamedBufferEXT
#define glGetNamedBufferParameteri64vEXT        __mods_glGetNamedBufferParameteri64vEXT
#define glGetNamedBufferParameterui64vLW        __mods_glGetNamedBufferParameterui64vLW
#define glNamedProgramStringEXT                 __mods_glNamedProgramStringEXT
#define glNamedProgramLocalParameterI4iEXT      __mods_glNamedProgramLocalParameterI4iEXT
#define glNamedProgramLocalParameterI4uiEXT     __mods_glNamedProgramLocalParameterI4uiEXT

// GL_VERSION_4_3
#define glDebugMessageCallbackARB               __mods_glDebugMessageCallbackARB
#define glDebugMessageControlARB                __mods_glDebugMessageControlARB

// GL_LW_blend_equation_advanced
#define glBlendParameteriLW                     __mods_glBlendParameteriLW

// GL_EXT_draw_instanced
#define glDrawArraysInstancedEXT                __mods_glDrawArraysInstancedEXT
#define glDrawElementsInstancedEXT              __mods_glDrawElementsInstancedEXT

// GL_LW_draw_texture
#define glDrawTextureLW                         __mods_glDrawTextureLW

// Path object management
#define glGenPathsLW                            __mods_glGenPathsLW
#define glDeletePathsLW                         __mods_glDeletePathsLW
#define glIsPathLW                              __mods_glIsPathLW

// GL_ARB_viewport_array
#define glViewportArrayv                        __mods_glViewportArrayv
#define glViewportSwizzleLW                     __mods_glViewportSwizzleLW

// Path object specification
#define glPathCommandsLW                        __mods_glPathCommandsLW
#define glPathStringLW                          __mods_glPathStringLW
#define glPathGlyphRangeLW                      __mods_glPathGlyphRangeLW
#define glPathGlyphsLW                          __mods_glPathGlyphsLW

// "Stencil, then Cover" path rendering commands
#define glCoverFillPathLW                       __mods_glCoverFillPathLW
#define glCoverStrokePathLW                     __mods_glCoverStrokePathLW
#define glStencilStrokePathLW                   __mods_glStencilStrokePathLW
#define glStencilFillPathLW                     __mods_glStencilFillPathLW

// Instanced path rendering
#define glStencilFillPathInstancedLW            __mods_glStencilFillPathInstancedLW
#define glCoverFillPathInstancedLW              __mods_glCoverFillPathInstancedLW
#define glStencilStrokePathInstancedLW          __mods_glStencilStrokePathInstancedLW
#define glCoverStrokePathInstancedLW            __mods_glCoverStrokePathInstancedLW

// Glyph metric queries
#define glGetPathMetricRangeLW                  __mods_glGetPathMetricRangeLW
#define glGetPathSpacingLW                      __mods_glGetPathSpacingLW

// Path object parameter specification
#define glPathParameteriLW                      __mods_glPathParameteriLW
#define glPathParameterfLW                      __mods_glPathParameterfLW

// Color generation
#define glPathColorGenLW                        __mods_glPathColorGenLW

//GL_EXT_raster_multisample
#define glRasterSamplesEXT                      __mods_glRasterSamplesEXT
#define glCoverageModulationLW                  __mods_glCoverageModulationLW

//Direct state access (DSA) matrix manipulation
#define glMatrixLoadIdentityEXT                __mods_glMatrixLoadIdentityEXT
#define glMatrixOrthoEXT                       __mods_glMatrixOrthoEXT
#define glMatrixTranslatefEXT                  __mods_glMatrixTranslatefEXT
#define glMatrixRotatefEXT                     __mods_glMatrixRotatefEXT

// GL_ARB_texture_storage
#define glTexStorage1D                         __mods_glTexStorage1D
#define glTexStorage2D                         __mods_glTexStorage2D
#define glTexStorage3D                         __mods_glTexStorage3D

// GLSL
#define glGetUniformLocation                   __mods_glGetUniformLocation
#define glUniform1i                            __mods_glUniform1i
#define glUniform1ui                           __mods_glUniform1ui
#define glUniform1uiv                          __mods_glUniform1uiv
#define glGetUniformfv                         __mods_glGetUniformfv
#define glGetUniformiv                         __mods_glGetUniformiv
#define glGetUniformuiv                        __mods_glGetUniformuiv
#define glProgramUniform1f                     __mods_glProgramUniform1f
#define glProgramUniform2f                     __mods_glProgramUniform2f
#define glProgramUniform3f                     __mods_glProgramUniform3f
#define glProgramUniform4f                     __mods_glProgramUniform4f
#define glProgramUniform1i                     __mods_glProgramUniform1i
#define glProgramUniform2i                     __mods_glProgramUniform2i
#define glProgramUniform3i                     __mods_glProgramUniform3i
#define glProgramUniform4i                     __mods_glProgramUniform4i
#define glProgramUniform1ui                    __mods_glProgramUniform1ui
#define glProgramUniform2ui                    __mods_glProgramUniform2ui
#define glProgramUniform3ui                    __mods_glProgramUniform3ui
#define glProgramUniform4ui                    __mods_glProgramUniform4ui

//GL_ARB_gpu_shader_int64
#define glGetUniformi64vARB                    __mods_glGetUniformi64vARB           //$
#define glGetUniformui64vARB                   __mods_glGetUniformui64vARB          //$
#define glProgramUniform1i64ARB                __mods_glProgramUniform1i64ARB       //$
#define glProgramUniform2i64ARB                __mods_glProgramUniform2i64ARB       //$
#define glProgramUniform3i64ARB                __mods_glProgramUniform3i64ARB       //$
#define glProgramUniform4i64ARB                __mods_glProgramUniform4i64ARB       //$
#define glProgramUniform1ui64ARB               __mods_glProgramUniform1ui64ARB      //$
#define glProgramUniform2ui64ARB               __mods_glProgramUniform2ui64ARB      //$
#define glProgramUniform3ui64ARB               __mods_glProgramUniform3ui64ARB      //$
#define glProgramUniform4ui64ARB               __mods_glProgramUniform4ui64ARB      //$

#define glBindBufferBase                       __mods_glBindBufferBase
#define glVertexAttrib2f                       __mods_glVertexAttrib2f
#define glUseProgramStages                     __mods_glUseProgramStages
#define glBindProgramPipeline                  __mods_glBindProgramPipeline
#define glGenProgramPipelines                  __mods_glGenProgramPipelines
#define glProgramParameteri                    __mods_glProgramParameteri
#define glGetAttachedShaders                   __mods_glGetAttachedShaders
#define glDetachShader                         __mods_glDetachShader

// Compute Shaders
#define glMemoryBarrier                        __mods_glMemoryBarrier
#define glDispatchCompute                      __mods_glDispatchCompute
#define glDispatchComputeGroupSizeARB          __mods_glDispatchComputeGroupSizeARB

#define glWaitSync                             __mods_glWaitSync
#define glClearBufferData                      __mods_glClearBufferData
#define glBufferStorage                        __mods_glBufferStorage
#define glDeleteSync                           __mods_glDeleteSync
#define glFenceSync                            __mods_glFenceSync

// Internal Format query
#define glGetInternalformati64v                __mods_glGetInternalformati64v
#define glGetInternalformativ                  __mods_glGetInternalformativ

// Sparse and View textures
#define glTextureView                          __mods_glTextureView
#define glTexPageCommitmentARB                 __mods_glTexPageCommitmentARB
#define glTexturePageCommitmentEXT             __mods_glTexturePageCommitmentEXT
// For CRC check on GPU
#define glTexBuffer                            __mods_glTexBuffer
#define glMapBufferRange                       __mods_glMapBufferRange
#define glGetIntegeri_v                        __mods_glGetIntegeri_v

//Conservative Raster
#define glSubpixelPrecisionBiasLW              __mods_glSubpixelPrecisionBiasLW
#define glConservativeRasterParameteriLW       __mods_glConservativeRasterParameteriLW
#define glConservativeRasterParameterfLW       __mods_glConservativeRasterParameterfLW

// Vulkan
#define glWaitVkSemaphoreLW                    __mods_glWaitVkSemaphoreLW
#define glSignalVkSemaphoreLW                  __mods_glSignalVkSemaphoreLW
#define glDrawVkImageLW                        __mods_glDrawVkImageLW

//GL_LW_shading_rate_image
#define glShadingRateImagePaletteLW            __mods_glShadingRateImagePaletteLW //$
#define glBindShadingRateImageLW               __mods_glBindShadingRateImageLW //$
#define glGetShadingRateImagePaletteLW         __mods_glGetShadingRateImagePaletteLW //$
#define glShadingRateImageBarrierLW            __mods_glShadingRateImageBarrierLW //$

//GL_LW_texture_barrier
#define glTextureBarrierLW                     __mods_glTextureBarrierLW //$
#endif // INCLUDED_MODSGL_H
