/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------

// Mods opengl utilities.

#include "core/include/massert.h"
#include "modsgl.h"
#include <string.h>
#include "core/include/tee.h"
#include "core/include/platform.h"
#include "core/include/version.h"
#include <set>
#include "core/include/utility.h"
#include "core/include/tasker.h"
//------------------------------------------------------------------------------
set<string> s_SuppressedExtensions;
void mglSuppressExtension( string extension)
{
    if (!extension.empty())
    {
        Printf(Tee::PriNormal, "Suppressing GLExtension: %s.\n", extension.c_str());
        s_SuppressedExtensions.insert(extension);
    }
    else
        s_SuppressedExtensions.clear();
}

void mglUnsuppressExtension( string extension)
{
    if (!extension.empty() && !s_SuppressedExtensions.empty())
    {
        set<string>::iterator iter = s_SuppressedExtensions.find(extension);
        if ( iter != s_SuppressedExtensions.end())
        {
            Printf(Tee::PriNormal, "Unsuppressing GLExtension: %s.\n", extension.c_str());
            s_SuppressedExtensions.erase(iter);
        }
    }
}

//------------------------------------------------------------------------------
bool mglExtensionSupported(const char *extension)
{
   const char *extensions = NULL;
   const char *start;
   const char *where, *terminator;

   if (0 == *extension)
   {
       // Special case for the "" extension -- always supported.
       return true;
   }

   // Extension names should not have spaces.
   where = strchr(extension, ' ');
   if (where || (*extension == '\0'))
   {
      return false;
   }

   if (s_SuppressedExtensions.count(extension))
      return false;

   extensions = (const char *)glGetString(GL_EXTENSIONS);

   // It takes a bit of care to be fool-proof about parsing the
   // OpenGL extensions string.  Don't be fooled by sub-strings,
   // etc.
   start = extensions;
   for (;;)
   {
      // If your application crashes in the strstr routine below,
      // you are probably calling mglExtensionSupported without
      // having a current window.  Calling glGetString without
      // a current OpenGL context has unpredictable results.
      // Please fix your program.
      where = strstr(start, extension);
      if (!where)
      {
         break;
      }
      terminator = where + strlen(extension);
      if ((where == start) || (*(where - 1) == ' '))
      {
         if ((*terminator == ' ') || (*terminator == '\0'))
         {
            return true;
         }
      }
      start = terminator;
   }
   return false;
}

//------------------------------------------------------------------------------
// Get & clear global OpenGL error code, translate it into a MODS RC.
//
RC mglGetRC()
{
   GLenum err = glGetError();

   if( GL_NO_ERROR == err )
      return OK;              // optimize the common case.
   else
      return mglErrorToRC(err);
}

//------------------------------------------------------------------------------
// Colwert a gl error enumerant to a MODS RC (doesn't touch global GL error code).
//
RC mglErrorToRC(GLenum err)
{
   switch( err )
   {
      case GL_NO_ERROR:                    return OK;
      case GL_ILWALID_ENUM:                return RC::MODS_GL_ILWALID_ENUM;
      case GL_ILWALID_VALUE:               return RC::MODS_GL_ILWALID_VALUE;
      case GL_ILWALID_OPERATION:           return RC::MODS_GL_ILWALID_OPERATION;
      case GL_STACK_OVERFLOW:              return RC::MODS_GL_STACK_OVERFLOW;
      case GL_STACK_UNDERFLOW:             return RC::MODS_GL_STACK_UNDERFLOW;
      case GL_OUT_OF_MEMORY:               return RC::MODS_GL_OUT_OF_MEMORY;
#ifndef INCLUDE_OGL_ES
      case GL_TABLE_TOO_LARGE:             return RC::MODS_GL_TABLE_TOO_LARGE;
#endif

      case RC::SOFTWARE_ERROR:             return RC::SOFTWARE_ERROR;
      case RC::NOTIFIER_TIMEOUT:           return RC::NOTIFIER_TIMEOUT;
      case RC::USER_ABORTED_SCRIPT:        return RC::USER_ABORTED_SCRIPT;

      case GLU_ILWALID_ENUM:               return RC::MODS_GLU_ILWALID_ENUM;
      case GLU_ILWALID_VALUE:              return RC::MODS_GLU_ILWALID_VALUE;
      case GLU_OUT_OF_MEMORY:              return RC::MODS_GLU_OUT_OF_MEMORY;
      case GLU_ILWALID_OPERATION:          return RC::MODS_GLU_ILWALID_OPERATION;

      case GLU_NURBS_ERROR1:
      case GLU_NURBS_ERROR2:
      case GLU_NURBS_ERROR3:
      case GLU_NURBS_ERROR4:
      case GLU_NURBS_ERROR5:
      case GLU_NURBS_ERROR6:
      case GLU_NURBS_ERROR7:
      case GLU_NURBS_ERROR8:
      case GLU_NURBS_ERROR9:
      case GLU_NURBS_ERROR10:
      case GLU_NURBS_ERROR11:
      case GLU_NURBS_ERROR12:
      case GLU_NURBS_ERROR13:
      case GLU_NURBS_ERROR14:
      case GLU_NURBS_ERROR15:
      case GLU_NURBS_ERROR16:
      case GLU_NURBS_ERROR17:
      case GLU_NURBS_ERROR18:
      case GLU_NURBS_ERROR19:
      case GLU_NURBS_ERROR20:
      case GLU_NURBS_ERROR21:
      case GLU_NURBS_ERROR22:
      case GLU_NURBS_ERROR23:
      case GLU_NURBS_ERROR24:
      case GLU_NURBS_ERROR25:
      case GLU_NURBS_ERROR26:
      case GLU_NURBS_ERROR27:
      case GLU_NURBS_ERROR28:
      case GLU_NURBS_ERROR29:
      case GLU_NURBS_ERROR30:
      case GLU_NURBS_ERROR31:
      case GLU_NURBS_ERROR32:
      case GLU_NURBS_ERROR33:
      case GLU_NURBS_ERROR34:
      case GLU_NURBS_ERROR35:
      case GLU_NURBS_ERROR36:
      case GLU_NURBS_ERROR37:              return RC::MODS_GLU_NURBS_ERROR;

      case GLU_TESS_MISSING_BEGIN_POLYGON: return RC::MODS_GLU_TESS_MISSING_BEGIN_POLYGON;
      case GLU_TESS_MISSING_BEGIN_CONTOUR: return RC::MODS_GLU_TESS_MISSING_BEGIN_CONTOUR;
      case GLU_TESS_MISSING_END_POLYGON:   return RC::MODS_GLU_TESS_MISSING_END_POLYGON;
      case GLU_TESS_MISSING_END_CONTOUR:   return RC::MODS_GLU_TESS_MISSING_END_CONTOUR;
      case GLU_TESS_COORD_TOO_LARGE:       return RC::MODS_GLU_TESS_COORD_TOO_LARGE;
      case GLU_TESS_NEED_COMBINE_CALLBACK: return RC::MODS_GLU_TESS_NEED_COMBINE_CALLBACK;
      case GLU_TESS_ERROR7:
      case GLU_TESS_ERROR8:                return RC::MODS_GLU_TESS_ERROR;

      default:                MASSERT(0);  return RC::MODS_GL_BAD_GLERROR;
   }
}

// GL tests can install this callback funtion to ensure that glFinish gets called
// before Golden::Run tries to grab CRCs
//------------------------------------------------------------------------------
RC GLFinishGoldenIdleCallback(void *nullPtr, void *nothing)
{
    // This function shouldn't take any data...both should be NULL
    MASSERT(!nullPtr);
    MASSERT(!nothing);

    glFinish();

    return OK;
}

//------------------------------------------------------------------------------
RC mglCheckFbo()
{
#if defined GL_EXT_framebuffer_object
    const char * s = "bad";
    GLuint status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    switch (status)
    {
        default:
            Printf(Tee::PriLow, "FBO status enum %d unrecognized.\n", status);
            break;

        case GL_FRAMEBUFFER_COMPLETE_EXT:
            return OK;

        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
            s = "incomplete attachment"; break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
            s = "incomplete missing attachment"; break;
        case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
            s = "incomplete dimensions"; break;
        case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
            s = "incomplete formats"; break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
            s = "incomplete draw buffer"; break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
            s = "incomplete read buffer"; break;
        case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
            s = "unsupported";
            // OpenGL driver has a complicated error propagation technique.
            // When a timeout oclwrs waiting for GPU to complete some work
            // the driver may modify localModeSwitchCount vs
            // *dev->glsDevice.deviceModeSwitchCountPtr values. The count
            // mismatch that theoretically means that a SetMode has oclwrred
            // but driver state/surfaces were not adjusted to a new resolution,
            // is really signaling an error that has oclwrred much earlier.
            // More info:
            // https://wiki.lwpu.com/engwiki/index.php/OpenGL/modeswitch_windowchanged
            Printf(Tee::PriNormal,
                "Note: GL_FRAMEBUFFER_UNSUPPORTED_EXT may mean that a timeout "
                "has oclwred inside of the OpenGL driver, try "
                "\"-gl_timeout_sec 0\" argument.\n");
            break;
    }
    Printf(Tee::PriError, "FramebufferObject status is %s.\n", s);
    return RC::MODS_GL_ILWALID_OPERATION;
#else
    return OK;
#endif
}
namespace ModsGL
{
//------------------------------------------------------------------------------
RC LoadShader(GLuint target, GLuint *pId, const char * str)
{
    RC rc;
    Tee::Priority pri = Tee::PriLow;

    MASSERT(pId && str);
    if (*pId)
    {
        glDeleteShader(*pId);
        *pId = 0;
    }
    *pId = glCreateShader(target);
    glShaderSource(*pId, 1, &str, nullptr);
    glCompileShader(*pId);

    GLint status;
    glGetShaderiv(*pId, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
    {
        rc = RC::MODS_GL_ILWALID_OPERATION;
        pri = Tee::PriHigh;
    }
    Printf(pri, "Compiled %s shader: rc = %d\n",
           mglEnumToString(target), rc.Get());

    int len = 0;
    glGetShaderiv(*pId, GL_INFO_LOG_LENGTH, &len);
    if (len)
    {
        vector<char>info(len+1);
        glGetShaderInfoLog(*pId, len, nullptr, &info[0]);
        Printf(pri, "InfoLog: %s\n", &info[0]);
    }
    return rc;
}

//------------------------------------------------------------------------------
RC LinkProgram(GLuint id)
{
    RC rc;
    Tee::Priority pri = Tee::PriLow;

    glLinkProgram(id);

    GLint status;
    glGetProgramiv(id, GL_LINK_STATUS, &status);
    if (status != GL_TRUE)
    {
        rc = RC::MODS_GL_ILWALID_OPERATION;
        pri = Tee::PriHigh;
    }
    Printf(pri, "Linked program: rc = %d\n", rc.Get());

    int len = 0;
    glGetProgramiv(id, GL_INFO_LOG_LENGTH, &len);
    if (len)
    {
        vector<char>info(len+1);
        glGetProgramInfoLog(id, len, nullptr, &info[0]);
        Printf(pri, "InfoLog: %s\n", &info[0]);
    }
    return rc;
}

//------------------------------------------------------------------------------
void GetUniform(const Uniform<GLfloat> & u, GLfloat * p)
{
    glGetUniformfv(u.ProgId(), u.Loc(), p);
}
void GetUniform(const Uniform<GLint> & u, GLint * p)
{
    glGetUniformiv(u.ProgId(), u.Loc(), p);
}
void GetUniform(const Uniform<GLuint> & u, GLuint * p)
{
    glGetUniformuiv(u.ProgId(), u.Loc(), p);
}
void GetUniform(const Uniform<GLuint64EXT> & u, GLuint64EXT * p)
{
    glGetUniformui64vARB(u.ProgId(), u.Loc(), p);
}
void GetUniform(const Uniform<GLint64EXT> & u, GLint64EXT * p)
{
    glGetUniformi64vARB(u.ProgId(), u.Loc(), p);
}

//------------------------------------------------------------------------------
void SetUniform(const Uniform<GLfloat> & u, GLfloat x)
{
    glProgramUniform1f(u.ProgId(), u.Loc(), x);
}
void SetUniform(const Uniform<GLfloat> & u, GLfloat x, GLfloat y)
{
    glProgramUniform2f(u.ProgId(), u.Loc(), x, y);
}
void SetUniform(const Uniform<GLfloat> & u, GLfloat x, GLfloat y, GLfloat z)
{
    glProgramUniform3f(u.ProgId(), u.Loc(), x, y, z);
}
void SetUniform(const Uniform<GLfloat> & u, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    glProgramUniform4f(u.ProgId(), u.Loc(), x, y, z, w);
}

//------------------------------------------------------------------------------
void SetUniform(const Uniform<GLint> & u, GLint x)
{
    glProgramUniform1i(u.ProgId(), u.Loc(), x);
}
void SetUniform(const Uniform<GLint> & u, GLint x, GLint y)
{
    glProgramUniform2i(u.ProgId(), u.Loc(), x, y);
}
void SetUniform(const Uniform<GLint> & u, GLint x, GLint y, GLint z)
{
    glProgramUniform3i(u.ProgId(), u.Loc(), x, y, z);
}
void SetUniform(const Uniform<GLint> & u, GLint x, GLint y, GLint z, GLint w)
{
    glProgramUniform4i(u.ProgId(), u.Loc(), x, y, z, w);
}

//------------------------------------------------------------------------------
void SetUniform(const Uniform<GLuint> & u, GLuint x)
{
    glProgramUniform1ui(u.ProgId(), u.Loc(), x);
}
void SetUniform(const Uniform<GLuint> & u, GLuint x, GLuint y)
{
    glProgramUniform2ui(u.ProgId(), u.Loc(), x, y);
}
void SetUniform(const Uniform<GLuint> & u, GLuint x, GLuint y, GLuint z)
{
    glProgramUniform3ui(u.ProgId(), u.Loc(), x, y, z);
}
void SetUniform(const Uniform<GLuint> & u, GLuint x, GLuint y, GLuint z, GLuint w)
{
    glProgramUniform4ui(u.ProgId(), u.Loc(), x, y, z, w);
}

//------------------------------------------------------------------------------
void SetUniform(const Uniform<GLint64EXT> & u, GLint64EXT x)
{
    glProgramUniform1i64ARB(u.ProgId(), u.Loc(), x);
}
void SetUniform(const Uniform<GLint64EXT> & u, GLint64EXT x, GLint64EXT y)
{
    glProgramUniform2i64ARB(u.ProgId(), u.Loc(), x, y);
}
void SetUniform(const Uniform<GLint64EXT> & u, GLint64EXT x, GLint64EXT y, GLint64EXT z)
{
    glProgramUniform3i64ARB(u.ProgId(), u.Loc(), x, y, z);
}
void SetUniform
(
    const Uniform<GLint64EXT> & u,
    GLint64EXT x,
    GLint64EXT y,
    GLint64EXT z,
    GLint64EXT w
)
{
    glProgramUniform4i64ARB(u.ProgId(), u.Loc(), x, y, z, w);
}

//------------------------------------------------------------------------------
void SetUniform(const Uniform<GLuint64EXT> & u, GLuint64EXT x)
{
    glProgramUniform1ui64ARB(u.ProgId(), u.Loc(), x);
}
void SetUniform(const Uniform<GLuint64EXT> & u, GLuint64EXT x, GLuint64EXT y)
{
    glProgramUniform2ui64ARB(u.ProgId(), u.Loc(), x, y);
}
void SetUniform(const Uniform<GLuint64EXT> & u, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z)
{
    glProgramUniform3ui64ARB(u.ProgId(), u.Loc(), x, y, z);
}
void SetUniform
(
    const Uniform<GLuint64EXT> & u,
    GLuint64EXT x,
    GLuint64EXT y,
    GLuint64EXT z,
    GLuint64EXT w
)
{
    glProgramUniform4ui64ARB(u.ProgId(), u.Loc(), x, y, z, w);
}

} // namespace ModsGL

//------------------------------------------------------------------------------
static void PrintProgram(void * pv_str)
{
    const char * str = (const char *)pv_str;
    Printf (Tee::PriHigh, "BreakPoint during glLoadProgram of:\n");
    Printf (Tee::PriHigh, "%s\n", str);
}

RC mglLoadProgram(GLuint target, GLuint id, GLsizei len, const GLubyte * str)
{
    StickyRC rc;

#if defined GL_LW_fragment_program
    // Runtime check for the extension.
    if (mglExtensionSupported("GL_LW_fragment_program"))
    {
        {
            // If the GL driver asserts, print out what we were trying to do.
            Platform::BreakPointHooker hooker(&PrintProgram, const_cast<GLubyte *>(str));

            glBindProgramLW(target, id);
            glProgramStringARB(target, GL_PROGRAM_FORMAT_ASCII_ARB, len, str);
            rc = mglGetRC();
        }
        if (OK != rc)
        {
            //
            // Print a report to the screen and logfile, showing where the syntax
            // error oclwrred in the program string.
            //
            GLint errOffset;
            const GLubyte * errMsg;

            glGetIntegerv(GL_PROGRAM_ERROR_POSITION_LW, &errOffset);
            errMsg = glGetString(GL_PROGRAM_ERROR_STRING_LW);

            Printf(Tee::PriHigh, "glLoadProgramLW err %s at %d ,\n",
                   (const char *)errMsg,
                   errOffset);

            GLint bufLen = 1024;
            char * buf = new char [bufLen];

            for (int pgmOffset = 0; pgmOffset < len; /**/)
            {
                int bytesThisLoop = bufLen - 1;

                if (pgmOffset < errOffset)
                {
                    if (pgmOffset + bytesThisLoop > errOffset)
                        bytesThisLoop = errOffset - pgmOffset;
                }
                else if (pgmOffset == errOffset)
                {
                    Printf(Tee::PriHigh, "@@@");
                }
                if (pgmOffset + bytesThisLoop > len)
                {
                    bytesThisLoop = len - pgmOffset;
                }
                memcpy( buf, str + pgmOffset, bytesThisLoop );
                buf[bytesThisLoop] = '\0';

                Printf(Tee::PriHigh, "%s", buf);

                pgmOffset += bytesThisLoop;
            }
            delete [] buf;
        }
    }
    rc = Utility::DumpProgram(".glprog", str, len);
#endif
    return rc;
}

//! This routine will return a string constant for the GLenum. Its useful
//! for printing debug information in the Print() routines.
const char * mglEnumToString
(
    GLenum EnumValue,           //!< GLenum to colwert
    const char *pDefault,       //!< default string to return if none found
    bool bAssertOnError,        //!< assert if enum is not found
    glEnumZeroOneMode zeroMode, //!< string to return for 0
    glEnumZeroOneMode oneMode  //!< string to return for 1
)
{
    #define GL_ENUM_STRING(x)   case x: return #x

    switch (EnumValue)
    {
        default:
            if(bAssertOnError)
            {
                Printf(Tee::PriHigh, "Unknown GL enum 0x%x\n", EnumValue);
                MASSERT(0);
            }
            return (pDefault ? pDefault : "Unknown GLEnum");

        //!< special case conditions with multiple defines for the same value
        case GL_FALSE: // or GL_ZERO, GL_NONE, GL_NO_ERROR
            switch(zeroMode)
            {
                case glezom_False:      return "GL_FALSE";
                case glezom_Zero:       return "GL_ZERO";
                case glezom_Disable:    return "DISABLED";
                case glezom_None:       return "GL_NONE";
                default:
                    if(bAssertOnError) MASSERT(0);
                    return (pDefault ? pDefault : "Unknown zeroMode");
            }
        case GL_TRUE: // or GL_ONE
            switch(oneMode)
            {
                case glezom_True:       return "GL_TRUE";
                case glezom_One:        return "GL_ONE";
                default:
                    if(bAssertOnError) MASSERT(0);
                    return (pDefault ? pDefault : "Unknown oneMode");
            }

        //!< Data type enums
        GL_ENUM_STRING(GL_BYTE);
        GL_ENUM_STRING(GL_UNSIGNED_BYTE);
        GL_ENUM_STRING(GL_SHORT);
        GL_ENUM_STRING(GL_UNSIGNED_SHORT);
        GL_ENUM_STRING(GL_INT);
        GL_ENUM_STRING(GL_UNSIGNED_INT);
        GL_ENUM_STRING(GL_FLOAT);
        GL_ENUM_STRING(GL_BITMAP);
        GL_ENUM_STRING(GL_UNSIGNED_BYTE_3_3_2);
        GL_ENUM_STRING(GL_UNSIGNED_BYTE_2_3_3_REV);
        GL_ENUM_STRING(GL_UNSIGNED_SHORT_5_6_5);
        GL_ENUM_STRING(GL_UNSIGNED_SHORT_5_6_5_REV);
        GL_ENUM_STRING(GL_UNSIGNED_SHORT_4_4_4_4);
        GL_ENUM_STRING(GL_UNSIGNED_SHORT_4_4_4_4_REV);
        GL_ENUM_STRING(GL_UNSIGNED_SHORT_5_5_5_1);
        GL_ENUM_STRING(GL_UNSIGNED_SHORT_1_5_5_5_REV);
        GL_ENUM_STRING(GL_UNSIGNED_INT_8_8_8_8);
        GL_ENUM_STRING(GL_UNSIGNED_INT_8_8_8_8_REV);
        GL_ENUM_STRING(GL_UNSIGNED_INT_10_10_10_2);
        GL_ENUM_STRING(GL_UNSIGNED_INT_2_10_10_10_REV);
        #if defined(GL_LW_texture_shader)
         GL_ENUM_STRING(GL_UNSIGNED_INT_S8_S8_8_8_LW);
        #endif
        #if defined(GL_LW_half_float)
         GL_ENUM_STRING(GL_HALF_FLOAT_LW);
        #endif
        #if defined(GL_EXT_texture_shared_exponent)
         GL_ENUM_STRING(GL_UNSIGNED_INT_5_9_9_9_REV_EXT);
         GL_ENUM_STRING(GL_RGB9_E5_EXT);
        #endif

        //!< PixelFormat
        GL_ENUM_STRING(GL_COLOR_INDEX);
        GL_ENUM_STRING(GL_STENCIL_INDEX);
        GL_ENUM_STRING(GL_DEPTH_COMPONENT);
        GL_ENUM_STRING(GL_RED);
        GL_ENUM_STRING(GL_GREEN);
        GL_ENUM_STRING(GL_BLUE);
        GL_ENUM_STRING(GL_ALPHA);
        GL_ENUM_STRING(GL_RGB);
        GL_ENUM_STRING(GL_RGBA);
        GL_ENUM_STRING(GL_LUMINANCE);
        GL_ENUM_STRING(GL_LUMINANCE_ALPHA);
        GL_ENUM_STRING(GL_BGR);
        GL_ENUM_STRING(GL_BGRA);

        //!< PixelInternalFormat
        GL_ENUM_STRING(GL_ALPHA8);
        GL_ENUM_STRING(GL_R11F_G11F_B10F_EXT);
        GL_ENUM_STRING(GL_R16);
        GL_ENUM_STRING(GL_R16F);
        GL_ENUM_STRING(GL_R16I);
        GL_ENUM_STRING(GL_R16UI);
        GL_ENUM_STRING(GL_R16_SNORM);
        GL_ENUM_STRING(GL_R32F);
        GL_ENUM_STRING(GL_R32I);
        GL_ENUM_STRING(GL_R32UI);
        GL_ENUM_STRING(GL_R8);
        GL_ENUM_STRING(GL_R8I);
        GL_ENUM_STRING(GL_R8UI);
        GL_ENUM_STRING(GL_R8_SNORM);
        GL_ENUM_STRING(GL_RG16);
        GL_ENUM_STRING(GL_RG16F);
        GL_ENUM_STRING(GL_RG16I);
        GL_ENUM_STRING(GL_RG16UI);
        GL_ENUM_STRING(GL_RG16_SNORM);
        GL_ENUM_STRING(GL_RG32F);
        GL_ENUM_STRING(GL_RG32I);
        GL_ENUM_STRING(GL_RG32UI);
        GL_ENUM_STRING(GL_RG8);
        GL_ENUM_STRING(GL_RG8I);
        GL_ENUM_STRING(GL_RG8UI);
        GL_ENUM_STRING(GL_RG8_SNORM);
        GL_ENUM_STRING(GL_RGB10_A2);
        GL_ENUM_STRING(GL_RGB16F);
        GL_ENUM_STRING(GL_RGB32F);
        GL_ENUM_STRING(GL_RGB5);
        GL_ENUM_STRING(GL_RGB5_A1);
        GL_ENUM_STRING(GL_RGB8);
        GL_ENUM_STRING(GL_RGBA16);
        GL_ENUM_STRING(GL_RGBA16F);
        GL_ENUM_STRING(GL_RGBA16I);
        GL_ENUM_STRING(GL_RGBA16UI);
        GL_ENUM_STRING(GL_RGBA16_SNORM);
        GL_ENUM_STRING(GL_RGBA32F);
        GL_ENUM_STRING(GL_RGBA32I);
        GL_ENUM_STRING(GL_RGBA32UI);
        GL_ENUM_STRING(GL_RGBA8);
        GL_ENUM_STRING(GL_RGBA8I);
        GL_ENUM_STRING(GL_RGBA8UI);
        GL_ENUM_STRING(GL_RGBA8_SNORM);
        GL_ENUM_STRING(GL_STENCIL_INDEX8);
        GL_ENUM_STRING(GL_DEPTH_COMPONENT16);
        GL_ENUM_STRING(GL_DEPTH_COMPONENT24);
        GL_ENUM_STRING(GL_RGB16);
        // ShadingModel
        GL_ENUM_STRING(GL_FLAT);
        GL_ENUM_STRING(GL_SMOOTH);

        //!< LW_register_combiners
        GL_ENUM_STRING(GL_PRIMARY_COLOR_LW);
        GL_ENUM_STRING(GL_SECONDARY_COLOR_LW);
        GL_ENUM_STRING(GL_TEXTURE0_ARB);
        GL_ENUM_STRING(GL_TEXTURE1_ARB);
        GL_ENUM_STRING(GL_TEXTURE2_ARB);
        GL_ENUM_STRING(GL_TEXTURE3_ARB);
        GL_ENUM_STRING(GL_SPARE0_LW);
        GL_ENUM_STRING(GL_SPARE1_LW);
        GL_ENUM_STRING(GL_FOG);
        GL_ENUM_STRING(GL_CONSTANT_COLOR0_LW);
        GL_ENUM_STRING(GL_CONSTANT_COLOR1_LW);
        //GL_ENUM_STRING(GL_ZERO);                           return "GL_ZERO"                          ;
        GL_ENUM_STRING(GL_DISCARD_LW);
        GL_ENUM_STRING(GL_E_TIMES_F_LW);
        GL_ENUM_STRING(GL_SPARE0_PLUS_SECONDARY_COLOR_LW);

        //!< LW_register_combiners
        GL_ENUM_STRING(GL_SIGNED_IDENTITY_LW);
        GL_ENUM_STRING(GL_UNSIGNED_IDENTITY_LW);
        GL_ENUM_STRING(GL_EXPAND_NORMAL_LW);
        GL_ENUM_STRING(GL_HALF_BIAS_NORMAL_LW);
        GL_ENUM_STRING(GL_SIGNED_NEGATE_LW);
        GL_ENUM_STRING(GL_UNSIGNED_ILWERT_LW);
        GL_ENUM_STRING(GL_EXPAND_NEGATE_LW);
        GL_ENUM_STRING(GL_HALF_BIAS_NEGATE_LW);

        //!< LW_fog_distance
        GL_ENUM_STRING(GL_EYE_RADIAL_LW);
        GL_ENUM_STRING(GL_EYE_PLANE);
        GL_ENUM_STRING(GL_EYE_PLANE_ABSOLUTE_LW);

        //!< EXT_fog_coord
        GL_ENUM_STRING(GL_FOG_COORDINATE); // GL_FOG_COORDINATE_EXT & GL_FOG_COORD are defined the same
        GL_ENUM_STRING(GL_FRAGMENT_DEPTH_EXT);

        //!< TextureMagFilter
        GL_ENUM_STRING(GL_LINEAR);
        GL_ENUM_STRING(GL_NEAREST);

        //!< TextureMinFilter
        GL_ENUM_STRING(GL_NEAREST_MIPMAP_NEAREST);
        GL_ENUM_STRING(GL_NEAREST_MIPMAP_LINEAR);
        GL_ENUM_STRING(GL_LINEAR_MIPMAP_NEAREST);
        GL_ENUM_STRING(GL_LINEAR_MIPMAP_LINEAR);

        //!< FogMode
        GL_ENUM_STRING(GL_EXP);
        GL_ENUM_STRING(GL_EXP2);

        //!< AlphaFunction
        GL_ENUM_STRING(GL_NEVER);
        GL_ENUM_STRING(GL_ALWAYS);
        GL_ENUM_STRING(GL_EQUAL);
        GL_ENUM_STRING(GL_NOTEQUAL);
        GL_ENUM_STRING(GL_LESS);
        GL_ENUM_STRING(GL_LEQUAL);
        GL_ENUM_STRING(GL_GEQUAL);
        GL_ENUM_STRING(GL_GREATER);

        //!< EXT_blend_minmax
        GL_ENUM_STRING(GL_FUNC_ADD_EXT);
        GL_ENUM_STRING(GL_MAX_EXT);
        GL_ENUM_STRING(GL_MIN_EXT);
        GL_ENUM_STRING(GL_FUNC_SUBTRACT_EXT);
        GL_ENUM_STRING(GL_FUNC_REVERSE_SUBTRACT_EXT);

        //!< BlendingFactorDest & EXT_blend_color
        GL_ENUM_STRING(GL_SRC_ALPHA);
        GL_ENUM_STRING(GL_SRC_ALPHA_SATURATE);
        GL_ENUM_STRING(GL_DST_ALPHA);
        GL_ENUM_STRING(GL_SRC_COLOR);
        GL_ENUM_STRING(GL_ONE_MINUS_SRC_COLOR);
        GL_ENUM_STRING(GL_DST_COLOR);
        GL_ENUM_STRING(GL_ONE_MINUS_DST_COLOR);
        GL_ENUM_STRING(GL_ONE_MINUS_SRC_ALPHA);
        GL_ENUM_STRING(GL_ONE_MINUS_DST_ALPHA);
        GL_ENUM_STRING(GL_CONSTANT_COLOR_EXT);
        GL_ENUM_STRING(GL_ONE_MINUS_CONSTANT_COLOR_EXT);
        GL_ENUM_STRING(GL_CONSTANT_ALPHA_EXT);
        GL_ENUM_STRING(GL_ONE_MINUS_CONSTANT_ALPHA_EXT);

        //!< LogicOp
        GL_ENUM_STRING(GL_COPY);
        GL_ENUM_STRING(GL_CLEAR);
        GL_ENUM_STRING(GL_SET);
        GL_ENUM_STRING(GL_COPY_ILWERTED);
        GL_ENUM_STRING(GL_NOOP);
        GL_ENUM_STRING(GL_ILWERT);
        GL_ENUM_STRING(GL_AND);
        GL_ENUM_STRING(GL_NAND);
        GL_ENUM_STRING(GL_OR);
        GL_ENUM_STRING(GL_NOR);
        GL_ENUM_STRING(GL_XOR);
        GL_ENUM_STRING(GL_EQUIV);
        GL_ENUM_STRING(GL_AND_REVERSE);
        GL_ENUM_STRING(GL_AND_ILWERTED);
        GL_ENUM_STRING(GL_OR_REVERSE);
        GL_ENUM_STRING(GL_OR_ILWERTED);

        //!< StencilOp
        GL_ENUM_STRING(GL_KEEP);
        GL_ENUM_STRING(GL_REPLACE);
        GL_ENUM_STRING(GL_INCR);
        GL_ENUM_STRING(GL_DECR);
        GL_ENUM_STRING(GL_INCR_WRAP_EXT);
        GL_ENUM_STRING(GL_DECR_WRAP_EXT);

        //!< TextureGenMode
        GL_ENUM_STRING(GL_OBJECT_LINEAR);
        GL_ENUM_STRING(GL_EYE_LINEAR);
        GL_ENUM_STRING(GL_SPHERE_MAP);
        GL_ENUM_STRING(GL_NORMAL_MAP);
        GL_ENUM_STRING(GL_REFLECTION_MAP);

        //!< MaterialParameter
        GL_ENUM_STRING(GL_AMBIENT_AND_DIFFUSE);
        GL_ENUM_STRING(GL_AMBIENT);
        GL_ENUM_STRING(GL_DIFFUSE);
        GL_ENUM_STRING(GL_SPELWLAR);
        GL_ENUM_STRING(GL_EMISSION);

        #if defined(GL_LW_texture_shader3)
        //!< LW_texture_shader3
         GL_ENUM_STRING(GL_FORCE_BLUE_TO_ONE_LW);
        #endif

        //!< TexShaderEdge
        GL_ENUM_STRING(GL_CLAMP);
        GL_ENUM_STRING(GL_CLAMP_TO_EDGE);
        GL_ENUM_STRING(GL_CLAMP_TO_BORDER);
        GL_ENUM_STRING(GL_REPEAT);
        GL_ENUM_STRING(GL_MIRRORED_REPEAT_IBM);
        GL_ENUM_STRING(GL_MIRROR_CLAMP_EXT);
        GL_ENUM_STRING(GL_MIRROR_CLAMP_TO_EDGE_EXT);
        GL_ENUM_STRING(GL_MIRROR_CLAMP_TO_BORDER_EXT);

        //! TexShaderOp
        GL_ENUM_STRING(GL_TEXTURE_1D);
        GL_ENUM_STRING(GL_TEXTURE_2D);
        GL_ENUM_STRING(GL_TEXTURE_3D);
        GL_ENUM_STRING(GL_TEXTURE_LWBE_MAP_EXT);
        GL_ENUM_STRING(GL_TEXTURE_RECTANGLE_LW);
        GL_ENUM_STRING(GL_TEXTURE_1D_ARRAY_EXT);
        GL_ENUM_STRING(GL_TEXTURE_2D_ARRAY_EXT);
        GL_ENUM_STRING(GL_TEXTURE_LWBE_MAP_POSITIVE_X_EXT);
        GL_ENUM_STRING(GL_TEXTURE_LWBE_MAP_NEGATIVE_X_EXT);
        GL_ENUM_STRING(GL_TEXTURE_LWBE_MAP_POSITIVE_Y_EXT);
        GL_ENUM_STRING(GL_TEXTURE_LWBE_MAP_NEGATIVE_Y_EXT);
        GL_ENUM_STRING(GL_TEXTURE_LWBE_MAP_POSITIVE_Z_EXT);
        GL_ENUM_STRING(GL_TEXTURE_LWBE_MAP_NEGATIVE_Z_EXT);
        GL_ENUM_STRING(GL_TEXTURE_LWBE_MAP_ARRAY_ARB);
        GL_ENUM_STRING(GL_PROXY_TEXTURE_1D);
        GL_ENUM_STRING(GL_PROXY_TEXTURE_1D_ARRAY_EXT);
        GL_ENUM_STRING(GL_PROXY_TEXTURE_2D);
        GL_ENUM_STRING(GL_PROXY_TEXTURE_2D_ARRAY_EXT);
        GL_ENUM_STRING(GL_PROXY_TEXTURE_LWBE_MAP_EXT);
        GL_ENUM_STRING(GL_PROXY_TEXTURE_LWBE_MAP_ARRAY_ARB);
        GL_ENUM_STRING(GL_PROXY_TEXTURE_RECTANGLE_LW);
        GL_ENUM_STRING(GL_PROXY_TEXTURE_3D);
        #if defined(GL_LW_texture_shader)
         GL_ENUM_STRING(GL_PASS_THROUGH_LW);
         GL_ENUM_STRING(GL_LWLL_FRAGMENT_LW);
         GL_ENUM_STRING(GL_OFFSET_TEXTURE_2D_LW);
         GL_ENUM_STRING(GL_OFFSET_TEXTURE_RECTANGLE_LW);
         GL_ENUM_STRING(GL_OFFSET_TEXTURE_2D_SCALE_LW);
         GL_ENUM_STRING(GL_OFFSET_TEXTURE_RECTANGLE_SCALE_LW);
         GL_ENUM_STRING(GL_DEPENDENT_AR_TEXTURE_2D_LW);
         GL_ENUM_STRING(GL_DEPENDENT_GB_TEXTURE_2D_LW);
         GL_ENUM_STRING(GL_DOT_PRODUCT_LW);
         GL_ENUM_STRING(GL_DOT_PRODUCT_TEXTURE_2D_LW);
         GL_ENUM_STRING(GL_DOT_PRODUCT_TEXTURE_RECTANGLE_LW);
         #if defined(GL_LW_texture_shader2)
          GL_ENUM_STRING(GL_DOT_PRODUCT_TEXTURE_3D_LW);
         #endif
         GL_ENUM_STRING(GL_DOT_PRODUCT_TEXTURE_LWBE_MAP_LW);
         GL_ENUM_STRING(GL_DOT_PRODUCT_REFLECT_LWBE_MAP_LW);
         GL_ENUM_STRING(GL_DOT_PRODUCT_CONST_EYE_REFLECT_LWBE_MAP_LW);
         GL_ENUM_STRING(GL_DOT_PRODUCT_DIFFUSE_LWBE_MAP_LW);
         GL_ENUM_STRING(GL_DOT_PRODUCT_DEPTH_REPLACE_LW);
         #if defined(GL_LW_texture_shader3)
          GL_ENUM_STRING(GL_OFFSET_PROJECTIVE_TEXTURE_2D_LW);
          GL_ENUM_STRING(GL_OFFSET_PROJECTIVE_TEXTURE_2D_SCALE_LW);
          GL_ENUM_STRING(GL_OFFSET_PROJECTIVE_TEXTURE_RECTANGLE_LW);
          GL_ENUM_STRING(GL_OFFSET_PROJECTIVE_TEXTURE_RECTANGLE_SCALE_LW);
          GL_ENUM_STRING(GL_OFFSET_HILO_TEXTURE_2D_LW);
          GL_ENUM_STRING(GL_OFFSET_HILO_TEXTURE_RECTANGLE_LW);
          GL_ENUM_STRING(GL_OFFSET_HILO_PROJECTIVE_TEXTURE_2D_LW);
          GL_ENUM_STRING(GL_OFFSET_HILO_PROJECTIVE_TEXTURE_RECTANGLE_LW);
          GL_ENUM_STRING(GL_DEPENDENT_HILO_TEXTURE_2D_LW);
          GL_ENUM_STRING(GL_DEPENDENT_RGB_TEXTURE_3D_LW);
          GL_ENUM_STRING(GL_DEPENDENT_RGB_TEXTURE_LWBE_MAP_LW);
          GL_ENUM_STRING(GL_DOT_PRODUCT_PASS_THROUGH_LW);
          GL_ENUM_STRING(GL_DOT_PRODUCT_TEXTURE_1D_LW);
          GL_ENUM_STRING(GL_DOT_PRODUCT_AFFINE_DEPTH_REPLACE_LW);
         #endif
        #endif
        // Transform feedback
        GL_ENUM_STRING(GL_POSITION);
        GL_ENUM_STRING(GL_PRIMARY_COLOR);
        GL_ENUM_STRING(GL_BACK_PRIMARY_COLOR_LW);
        GL_ENUM_STRING(GL_BACK_SECONDARY_COLOR_LW);
        GL_ENUM_STRING(GL_POINT_SIZE);
        GL_ENUM_STRING(GL_TEXTURE_COORD_LW);
        GL_ENUM_STRING(GL_CLIP_DISTANCE_LW);
        GL_ENUM_STRING(GL_VERTEX_ID_LW);
        GL_ENUM_STRING(GL_PRIMITIVE_ID_LW);
        GL_ENUM_STRING(GL_GENERIC_ATTRIB_LW);
        // Explicit multisample
        GL_ENUM_STRING(GL_TEXTURE_RENDERBUFFER_LW);
        // OpenGL 1.4
        GL_ENUM_STRING(GL_COMPARE_R_TO_TEXTURE);

        GL_ENUM_STRING(GL_LUMINANCE16);
        GL_ENUM_STRING(GL_LUMINANCE16F_ARB);
        GL_ENUM_STRING(GL_LUMINANCE16I_EXT);
        GL_ENUM_STRING(GL_LUMINANCE16UI_EXT);
        GL_ENUM_STRING(GL_LUMINANCE16_ALPHA16);
        GL_ENUM_STRING(GL_LUMINANCE32F_ARB);
        GL_ENUM_STRING(GL_LUMINANCE32I_EXT);
        GL_ENUM_STRING(GL_LUMINANCE32UI_EXT);
        GL_ENUM_STRING(GL_LUMINANCE8);
        GL_ENUM_STRING(GL_LUMINANCE8I_EXT);
        GL_ENUM_STRING(GL_LUMINANCE8UI_EXT);
        GL_ENUM_STRING(GL_LUMINANCE8_ALPHA8);
        GL_ENUM_STRING(GL_LUMINANCE_ALPHA16F_ARB);
        GL_ENUM_STRING(GL_LUMINANCE_ALPHA16I_EXT);
        GL_ENUM_STRING(GL_LUMINANCE_ALPHA16UI_EXT);
        GL_ENUM_STRING(GL_LUMINANCE_ALPHA32F_ARB);
        GL_ENUM_STRING(GL_LUMINANCE_ALPHA32I_EXT);
        GL_ENUM_STRING(GL_LUMINANCE_ALPHA32UI_EXT);
        GL_ENUM_STRING(GL_LUMINANCE_ALPHA8I_EXT);
        GL_ENUM_STRING(GL_LUMINANCE_ALPHA8UI_EXT);
        GL_ENUM_STRING(GL_RGB16I_EXT);
        GL_ENUM_STRING(GL_RGB16UI_EXT);
        GL_ENUM_STRING(GL_RGB16_SNORM);
        GL_ENUM_STRING(GL_RGB32I_EXT);
        GL_ENUM_STRING(GL_RGB32UI_EXT);
        GL_ENUM_STRING(GL_RGB8I_EXT);
        GL_ENUM_STRING(GL_RGB8UI_EXT);
        GL_ENUM_STRING(GL_SIGNED_LUMINANCE8_ALPHA8_LW);
        GL_ENUM_STRING(GL_SIGNED_LUMINANCE8_LW);
        GL_ENUM_STRING(GL_SIGNED_RGB8_LW);
        GL_ENUM_STRING(GL_SIGNED_RGBA8_LW);
        GL_ENUM_STRING(GL_ALPHA16);
        GL_ENUM_STRING(GL_ALPHA16F_ARB);
        GL_ENUM_STRING(GL_ALPHA32F_ARB);
        GL_ENUM_STRING(GL_INTENSITY);
        GL_ENUM_STRING(GL_INTENSITY8);
        GL_ENUM_STRING(GL_INTENSITY16F_ARB);
        GL_ENUM_STRING(GL_INTENSITY32F_ARB);
        GL_ENUM_STRING(GL_RGBA4);
        GL_ENUM_STRING(GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
        GL_ENUM_STRING(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);
        GL_ENUM_STRING(GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);
        GL_ENUM_STRING(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
        GL_ENUM_STRING(GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT);
        GL_ENUM_STRING(GL_COMPRESSED_LUMINANCE_LATC1_EXT);
        GL_ENUM_STRING(GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT);
        GL_ENUM_STRING(GL_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT);
        GL_ENUM_STRING(GL_SRC_LW           );
        GL_ENUM_STRING(GL_DST_LW           );
        GL_ENUM_STRING(GL_SRC_OVER_LW      );
        GL_ENUM_STRING(GL_DST_OVER_LW      );
        GL_ENUM_STRING(GL_SRC_IN_LW        );
        GL_ENUM_STRING(GL_DST_IN_LW        );
        GL_ENUM_STRING(GL_SRC_OUT_LW       );
        GL_ENUM_STRING(GL_DST_OUT_LW       );
        GL_ENUM_STRING(GL_SRC_ATOP_LW      );
        GL_ENUM_STRING(GL_DST_ATOP_LW      );
        GL_ENUM_STRING(GL_PLUS_LW          );
        GL_ENUM_STRING(GL_PLUS_CLAMPED_LW   );
        GL_ENUM_STRING(GL_PLUS_CLAMPED_ALPHA_LW  );
        GL_ENUM_STRING(GL_MULTIPLY_LW      );
        GL_ENUM_STRING(GL_SCREEN_LW        );
        GL_ENUM_STRING(GL_OVERLAY_LW       );
        GL_ENUM_STRING(GL_DARKEN_LW        );
        GL_ENUM_STRING(GL_LIGHTEN_LW       );
        GL_ENUM_STRING(GL_COLORDODGE_LW    );
        GL_ENUM_STRING(GL_COLORBURN_LW     );
        GL_ENUM_STRING(GL_HARDLIGHT_LW     );
        GL_ENUM_STRING(GL_SOFTLIGHT_LW     );
        GL_ENUM_STRING(GL_DIFFERENCE_LW    );
        GL_ENUM_STRING(GL_MINUS_LW      );
        GL_ENUM_STRING(GL_MINUS_CLAMPED_LW );
        GL_ENUM_STRING(GL_EXCLUSION_LW     );
        GL_ENUM_STRING(GL_CONTRAST_LW      );
        GL_ENUM_STRING(GL_ILWERT_RGB_LW    );
        GL_ENUM_STRING(GL_LINEARDODGE_LW   );
        GL_ENUM_STRING(GL_LINEARBURN_LW    );
        GL_ENUM_STRING(GL_VIVIDLIGHT_LW    );
        GL_ENUM_STRING(GL_LINEARLIGHT_LW   );
        GL_ENUM_STRING(GL_PINLIGHT_LW      );
        GL_ENUM_STRING(GL_HARDMIX_LW       );
        GL_ENUM_STRING(GL_HSL_HUE_LW       );
        GL_ENUM_STRING(GL_HSL_SATURATION_LW);
        GL_ENUM_STRING(GL_HSL_COLOR_LW     );
        GL_ENUM_STRING(GL_HSL_LUMINOSITY_LW);
        GL_ENUM_STRING(GL_UNCORRELATED_LW);
        GL_ENUM_STRING(GL_DISJOINT_LW);
        GL_ENUM_STRING(GL_CONJOINT_LW);
        GL_ENUM_STRING(GL_COMPRESSED_RGB8_ETC2);
        GL_ENUM_STRING(GL_COMPRESSED_SRGB8_ETC2);
        GL_ENUM_STRING(GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2);
        GL_ENUM_STRING(GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2);
        GL_ENUM_STRING(GL_COMPRESSED_RGBA8_ETC2_EAC);
        GL_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC);
        GL_ENUM_STRING(GL_COMPRESSED_R11_EAC);
        GL_ENUM_STRING(GL_COMPRESSED_SIGNED_R11_EAC);
        GL_ENUM_STRING(GL_COMPRESSED_RG11_EAC);
        GL_ENUM_STRING(GL_COMPRESSED_SIGNED_RG11_EAC);
        GL_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_4x4_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_5x4_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_5x5_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_6x5_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_6x6_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_8x5_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_8x6_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_8x8_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_10x5_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_10x6_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_10x8_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_10x10_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_12x10_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_12x12_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR);
        GL_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR);
        GL_ENUM_STRING(GL_DEPTH_STENCIL_LW);
        GL_ENUM_STRING(GL_DEPTH_COMPONENT32);

        GL_ENUM_STRING(GL_VERTEX_SHADER);
        GL_ENUM_STRING(GL_FRAGMENT_SHADER);
        GL_ENUM_STRING(GL_COMPUTE_SHADER);

        GL_ENUM_STRING(GL_VIEWPORT_SWIZZLE_POSITIVE_X_LW);
        GL_ENUM_STRING(GL_VIEWPORT_SWIZZLE_NEGATIVE_X_LW);
        GL_ENUM_STRING(GL_VIEWPORT_SWIZZLE_POSITIVE_Y_LW);
        GL_ENUM_STRING(GL_VIEWPORT_SWIZZLE_NEGATIVE_Y_LW);
        GL_ENUM_STRING(GL_VIEWPORT_SWIZZLE_POSITIVE_Z_LW);
        GL_ENUM_STRING(GL_VIEWPORT_SWIZZLE_NEGATIVE_Z_LW);
        GL_ENUM_STRING(GL_VIEWPORT_SWIZZLE_POSITIVE_W_LW);

        // Conservative Raster
        GL_ENUM_STRING(GL_CONSERVATIVE_RASTER_MODE_LW);
        GL_ENUM_STRING(GL_CONSERVATIVE_RASTER_MODE_POST_SNAP_LW);
        GL_ENUM_STRING(GL_CONSERVATIVE_RASTER_MODE_PRE_SNAP_TRIANGLES_LW);
        GL_ENUM_STRING(GL_CONSERVATIVE_RASTER_MODE_PRE_SNAP_LW);
    }
}

//-----------------------------------------------------------------------------
static GLenum StdDataType
(
    UINT32 bitsPerComponent,
    bool isFloat,
    bool isSigned
)
{
    if (isFloat)
    {
        switch (bitsPerComponent)
        {
            case 16: return GL_HALF_FLOAT_LW;  // GL_HALF_FLOAT in GL3.0+
            case 32: return GL_FLOAT;
        }
    }
    else if (isSigned)
    {
        switch (bitsPerComponent)
        {
            case  8: return GL_BYTE;
            case 16: return GL_SHORT;
            case 32: return GL_INT;
        }
    }
    else
    {
        switch (bitsPerComponent)
        {
            case  8: return GL_UNSIGNED_BYTE;
            case 16: return GL_UNSIGNED_SHORT;
            case 32: return GL_UNSIGNED_INT;
        }
    }
    MASSERT(!"bad arg");
    return 0;
}

//-----------------------------------------------------------------------------
static GLenum StdIntCFormat
(
    UINT32 numComponents,
    UINT32 bitsPerComponent,
    bool isFloat,
    bool isNormalized,
    bool isSigned
)
{
    // When we have full GL 3.0 support for all supported chips, we can use
    // RGBA,RGB,RG,RED in place of RGBA,RGB,LUMINANCE_ALPHA,LUMINANCE.
    // That will be nice and symmetrical...

    if (isFloat)
    {
        switch (bitsPerComponent)
        {
            // ARB_texture_float (LW40+)

            // No 8-bit float formats.

            case 16:
                switch (numComponents)
                {
                    case 4: return GL_RGBA16F_ARB;
                    case 3: return GL_RGB16F_ARB;
                    case 2: return GL_LUMINANCE_ALPHA16F_ARB;
                    case 1: return GL_LUMINANCE16F_ARB;
                }
                break;
            case 32:
                switch (numComponents)
                {
                    case 4: return GL_RGBA32F_ARB;
                    case 3: return GL_RGB32F_ARB;
                    case 2: return GL_LUMINANCE_ALPHA32F_ARB;
                    case 1: return GL_LUMINANCE32F_ARB;
                }
                break;
        }
    }
    else if (isNormalized)
    {
        if (isSigned)
        {
            // integer, normalized, signed
            // Integer values are mapped to the -1..1 range
            switch (bitsPerComponent)
            {
                case  8:
                    // LW_texture_shader (lw25+)
                    switch (numComponents)
                    {
                        case 4: return GL_SIGNED_RGBA8_LW;
                        case 3: return GL_SIGNED_RGB8_LW;
                        case 2: return GL_SIGNED_LUMINANCE8_ALPHA8_LW;
                        case 1: return GL_SIGNED_LUMINANCE8_LW;
                    }
                    break;
                case 16:
                    // GL 3.1 (fermi)
                    switch (numComponents)
                    {
                        case 4: return GL_RGBA16_SNORM;
                        case 3: return GL_RGB16_SNORM;
                        case 2: return GL_RG16_SNORM;
                        case 1: return GL_R16_SNORM;
                    }
                    break;

                // No xx32_SNORM.
            }
        }
        else
        {
            // integer, normalized, unsigned
            // Uint values are mapped to the 0..1 range.
            // These are core-GL formats, no extension needed.
            switch (bitsPerComponent)
            {
                case  8:
                    switch (numComponents)
                    {
                        case 4: return GL_RGBA8;
                        case 3: return GL_RGB8;
                        case 2: return GL_LUMINANCE8_ALPHA8;
                        case 1: return GL_LUMINANCE8;
                    }
                    break;
                case 16:
                    switch (numComponents)
                    {
                        case 4: return GL_RGBA16;
                        case 3: return GL_RGB16;
                        case 2: return GL_LUMINANCE16_ALPHA16;
                        case 1: return GL_LUMINANCE16;
                    }
                    break;

                // No xx32 normalized, unsigned.
            }
        }
    }
    else
    {
        if (isSigned)
        {
            // integer, !normalized, signed
            // EXT_texture_integer, LW50+, but require special shaders.
            switch (bitsPerComponent)
            {
                case  8:
                    switch (numComponents)
                    {
                        case 4: return GL_RGBA8I_EXT;
                        case 3: return GL_RGB8I_EXT;
                        case 2: return GL_LUMINANCE_ALPHA8I_EXT;
                        case 1: return GL_LUMINANCE8I_EXT;
                    }
                    break;
                case 16:
                    switch (numComponents)
                    {
                        case 4: return GL_RGBA16I_EXT;
                        case 3: return GL_RGB16I_EXT;
                        case 2: return GL_LUMINANCE_ALPHA16I_EXT;
                        case 1: return GL_LUMINANCE16I_EXT;
                    }
                    break;
                case 32:
                    switch (numComponents)
                    {
                        case 4: return GL_RGBA32I_EXT;
                        case 3: return GL_RGB32I_EXT;
                        case 2: return GL_LUMINANCE_ALPHA32I_EXT;
                        case 1: return GL_LUMINANCE32I_EXT;
                    }
                    break;
            }
        }
        else
        {
            // integer, !normalized, unsigned
            // EXT_texture_integer, LW50+, but require special shaders.
            switch (bitsPerComponent)
            {
                case  8:
                    switch (numComponents)
                    {
                        case 4: return GL_RGBA8UI_EXT;
                        case 3: return GL_RGB8UI_EXT;
                        case 2: return GL_LUMINANCE_ALPHA8UI_EXT;
                        case 1: return GL_LUMINANCE8UI_EXT;
                    }
                    break;
                case 16:
                    switch (numComponents)
                    {
                        case 4: return GL_RGBA16UI_EXT;
                        case 3: return GL_RGB16UI_EXT;
                        case 2: return GL_LUMINANCE_ALPHA16UI_EXT;
                        case 1: return GL_LUMINANCE16UI_EXT;
                    }
                    break;
                case 32:
                    switch (numComponents)
                    {
                        case 4: return GL_RGBA32UI_EXT;
                        case 3: return GL_RGB32UI_EXT;
                        case 2: return GL_LUMINANCE_ALPHA32UI_EXT;
                        case 1: return GL_LUMINANCE32UI_EXT;
                    }
                    break;
            }
        }
    }
    MASSERT(!"bad arg");
    return 0;
}

//-----------------------------------------------------------------------------
static ColorUtils::Format StdExtModsCFormat
(
    UINT32 numComponents,
    UINT32 bitsPerComponent,
    bool isFloat,
    bool isNormalized,
    bool isSigned
)
{
    if (isFloat)
    {
        switch (bitsPerComponent)
        {
            case 16:
                switch (numComponents)
                {
                    case 4: return ColorUtils::RF16_GF16_BF16_AF16;
                    case 3: return ColorUtils::RF16_GF16_BF16_X16;
                    case 2: return ColorUtils::RF16_GF16;
                    case 1: return ColorUtils::RF16;
                }
                break;
            case 32:
                switch (numComponents)
                {
                    case 4: return ColorUtils::RF32_GF32_BF32_AF32;
                    case 3: return ColorUtils::RF32_GF32_BF32_X32;
                    case 2: return ColorUtils::RF32_GF32;
                    case 1: return ColorUtils::RF32;
                }
                break;
        }
    }
    else
    {
        // Treat all integer types as unsigned, normalized for purposes of
        // CRC and PNG utilities.
        // We use RGBA or RGBX rather than RGB  when there is no 3-component
        // HW format.
        //
        // To simplify mods development, we map all argb/bgra/rgba to just one
        // ordering (rgba in GL/big-endian, abgr in LW/little-endian).
        //
        // Note that 16- and 32-bit-per-component formats have rgba LW names
        // as each component is treated as its own unit, rather than as a
        // bitfield in a 32-bit or 16-bit unit.

        switch (bitsPerComponent)
        {
            case  8:
                switch (numComponents)
                {
                    case 4: return ColorUtils::A8B8G8R8;
                    case 3: return ColorUtils::B8_G8_R8;
                    case 2: return ColorUtils::G8R8;
                    case 1: return ColorUtils::R8;
                }
                break;
            case 16:
                switch (numComponents)
                {
                    case 4: // fall through
                    case 3: return ColorUtils::R16_G16_B16_A16;
                    case 2: return ColorUtils::R16_G16;
                    case 1: return ColorUtils::R16;
                }
                break;
            case 32:
                switch (numComponents)
                {
                    case 4: return ColorUtils::RU32_GU32_BU32_AU32;
                    case 3: return ColorUtils::RU32_GU32_BU32_X32;
                    case 2: return ColorUtils::RU32_GU32;
                    case 1: return ColorUtils::RU32;
                }
                break;
        }
    }
    MASSERT(!"bad arg");
    return ColorUtils::LWFMT_NONE;
}

//-----------------------------------------------------------------------------
static GLenum StdExtCFormat
(
    UINT32 numComponents,
    UINT32 bitsPerComponent,
    bool isFloat,
    bool isNormalized
)
{
    // When we have full GL 3.0 support for all supported chips, we can use
    // RGBA,RGB,RG,RED in place of RGBA,RGB,LUMINANCE_ALPHA,LUMINANCE.
    // That will be nice and symmetrical.

    // Many 3-component formats are supported in HW only as 4-component
    // format with 'X' for alpha, so use GL_RGBA except for the 8bit case.
    //
    // Otherwise we end up with an external format that matches no known HW
    // color format in layout, and the CRC and PNG utilities barf.

    if (isFloat || isNormalized)
    {
        switch (numComponents)
        {
            case 4: return GL_RGBA;
            case 3: return (bitsPerComponent == 8) ? GL_RGB : GL_RGBA;
            case 2: return GL_LUMINANCE_ALPHA;
            case 1: return GL_LUMINANCE;
        }
    }
    else
    {
        switch (numComponents)
        {
            case 4: return GL_RGBA_INTEGER_EXT;
            case 3: return (bitsPerComponent == 8) ? GL_RGB_INTEGER_EXT :
                                                     GL_RGBA_INTEGER_EXT;
            case 2: return GL_LUMINANCE_ALPHA_INTEGER_EXT;
            case 1: return GL_LUMINANCE_INTEGER_EXT;
        }
    }
    MASSERT(!"bad arg");
    return 0;
}

//-----------------------------------------------------------------------------
const char * const NoExt = "";

const char * StdExtRqd
(
    UINT32 bitsPerComponent,
    bool isFloat,
    bool isNormalized,
    bool isSigned
)
{
    if (isFloat)
    {
        // Supported in lw40+
        return "GL_ARB_texture_float";
    }
    if (isNormalized)
    {
        if (isSigned)
        {
            if (bitsPerComponent == 8)
                return "GL_LW_texture_shader";
            else
                // Fermi+ (GL 3.1)
                // Driver doesn't export this, even on fermi...
                return "GL_EXT_texture_snorm";
                // Could we check for GL version 3.1 directly?
        }
        else
        {
            // Supported everywhere.
            return NoExt;
        }
    }
    else
    {
        // Non-normalized integer textures require lw50+ extension.
        return "GL_EXT_texture_integer";
    }
}

//! Texture format data.
//! The ColorUtils::Format enum is intended to have one value for each
//! GPU HW pixel/texel format, so we index by that.
//!
//! There are many, many GL texture internal formats that redundantly map
//! to the same ColorUtils::Format.
//!

#define STD_CF_FULL(cf,numComponents,compBits,isFloat,isNormalized,isSigned)\
    {   ColorUtils::cf,                                                     \
        StdIntCFormat(numComponents,compBits,isFloat,isNormalized,isSigned),\
        StdExtModsCFormat(numComponents,compBits,isFloat,isNormalized,isSigned),\
        StdExtCFormat(numComponents,compBits,isFloat,isNormalized),         \
        StdDataType(compBits,isFloat,isSigned),                             \
        StdExtRqd(compBits,isFloat,isNormalized,isSigned)                   \
    }

// Declare a color format where the internal/external number of components
// differ, but everything else is the same
#define STD_CF_COMP_DIFF(cf,numCompInt,numCompExt,compBits,isFloat,isNormalized,isSigned)\
    {   ColorUtils::cf,                                                      \
        StdIntCFormat(numCompInt,compBits,isFloat,isNormalized,isSigned),    \
        StdExtModsCFormat(numCompExt,compBits,isFloat,isNormalized,isSigned),\
        StdExtCFormat(numCompExt,compBits,isFloat,isNormalized),             \
        StdDataType(compBits,isFloat,isSigned),                              \
        StdExtRqd(compBits,isFloat,isNormalized,isSigned)                    \
    }

//                                                                                  isFloat,isNorm,isSigned
#define STD_CF(   cf,numComponents,compBits)  STD_CF_FULL(cf,numComponents,compBits,false,true,false)
#define STD_CF_SN(cf,numComponents,compBits)  STD_CF_FULL(cf,numComponents,compBits,false,true,true)
#define STD_CF_UI(cf,numComponents,compBits)  STD_CF_FULL(cf,numComponents,compBits,false,false,false)
#define STD_CF_I( cf,numComponents,compBits)  STD_CF_FULL(cf,numComponents,compBits,false,false,true)
#define STD_CF_F( cf,numComponents,compBits)  STD_CF_FULL(cf,numComponents,compBits,true,false,true)

#define NONSTD_CF(cf,if,ocf,of,t,e) { ColorUtils::cf, if, ColorUtils::ocf, of, t, e }
#define UNSUPPORTED_CF(cf) { ColorUtils::cf, 0, ColorUtils::LWFMT_NONE, 0, 0, NoExt }

static const mglTexFmtInfo s_TexInfos [] =
{
    // The first Format_NUM_FORMATS entries in s_TexInfos correspond to the
    // ColorUtils::ColorFormat enum, so that we are guaranteed to have one
    // mglTexFmtInfo struct for each mods ColorFormat.

    UNSUPPORTED_CF (LWFMT_NONE),

    NONSTD_CF (R5G6B5, GL_RGB5, R5G6B5, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NoExt),

    STD_CF (A8R8G8B8,4,8),
    STD_CF (R8G8B8A8,4,8),

    NONSTD_CF (A2R10G10B10, GL_RGB10_A2, A2B10G10R10, GL_RGBA,
                GL_UNSIGNED_INT_2_10_10_10_REV, NoExt),
    NONSTD_CF (R10G10B10A2, GL_RGB10_A2, A2B10G10R10, GL_RGBA,
                GL_UNSIGNED_INT_2_10_10_10_REV, NoExt),

    UNSUPPORTED_CF (CR8YB8CB8YA8),
    UNSUPPORTED_CF (YB8CR8YA8CB8),

    NONSTD_CF (Z1R5G5B5, GL_RGB5, Z1R5G5B5, GL_RGB, GL_UNSIGNED_SHORT_5_5_5_1, NoExt),
    NONSTD_CF (Z24S8, GL_DEPTH_STENCIL_LW, Z24S8, GL_DEPTH_STENCIL_LW,
                GL_UNSIGNED_INT_24_8_LW, "GL_EXT_packed_depth_stencil"),
    NONSTD_CF (Z16, GL_DEPTH_COMPONENT16, Z16, GL_DEPTH_COMPONENT,
                GL_UNSIGNED_SHORT, "GL_ARB_depth_texture"),

    STD_CF_F (RF16, 1, 16),
    STD_CF_F (RF32, 1, 32),
    STD_CF_F (RF16_GF16_BF16_AF16, 4, 16),
    STD_CF_F (RF32_GF32_BF32_AF32, 4, 32),

    NONSTD_CF (Y8, GL_INTENSITY8, Y8, GL_RED, GL_UNSIGNED_BYTE, NoExt),

    STD_CF (B8_G8_R8,3,8),

    NONSTD_CF (Z24, GL_DEPTH_COMPONENT24, Z32, GL_DEPTH_COMPONENT,
                GL_UNSIGNED_INT, "GL_ARB_depth_texture"),

    UNSUPPORTED_CF (I8),
    UNSUPPORTED_CF (VOID8),
    UNSUPPORTED_CF (VOID16),
    UNSUPPORTED_CF (A2V10Y10U10),
    UNSUPPORTED_CF (A2U10Y10V10),
    UNSUPPORTED_CF (VE8YO8UE8YE8),
    UNSUPPORTED_CF (UE8YO8VE8YE8),
    UNSUPPORTED_CF (YO8VE8YE8UE8),
    UNSUPPORTED_CF (YO8UE8YE8VE8),
    UNSUPPORTED_CF (YE16_UE16_YO16_VE16),
    UNSUPPORTED_CF (YE10Z6_UE10Z6_YO10Z6_VE10Z6),
    UNSUPPORTED_CF (UE16_YE16_VE16_YO16),
    UNSUPPORTED_CF (UE10Z6_YE10Z6_VE10Z6_YO10Z6),
    UNSUPPORTED_CF (VOID32),
    UNSUPPORTED_CF (CPST8),
    UNSUPPORTED_CF (CPSTY8CPSTC8),
    UNSUPPORTED_CF (AUDIOL16_AUDIOR16),
    UNSUPPORTED_CF (AUDIOL32_AUDIOR32),

    NONSTD_CF (A2B10G10R10, GL_RGB10_A2, A2B10G10R10, GL_RGBA,
                GL_UNSIGNED_INT_2_10_10_10_REV, NoExt),

    STD_CF (A8B8G8R8,4,8),

    NONSTD_CF (A1R5G5B5, GL_RGB5_A1, A1R5G5B5, GL_RGBA,
                GL_UNSIGNED_SHORT_5_5_5_1, NoExt),

    STD_CF (Z8R8G8B8,3,8),

    NONSTD_CF (Z32, GL_DEPTH_COMPONENT32, Z32, GL_DEPTH_COMPONENT,
                GL_UNSIGNED_INT, "GL_ARB_depth_texture"),

    STD_CF (X8R8G8B8,3,8),

    NONSTD_CF (X1R5G5B5, GL_RGB5, X1R5G5B5, GL_RGB, GL_UNSIGNED_SHORT_5_5_5_1, NoExt),

    STD_CF_SN (AN8BN8GN8RN8,4,8),
    STD_CF_I  (AS8BS8GS8RS8,4,8),
    STD_CF_UI (AU8BU8GU8RU8,4,8),
    STD_CF    (X8B8G8R8,3,8),

    UNSUPPORTED_CF (A8RL8GL8BL8),
    UNSUPPORTED_CF (X8RL8GL8BL8),
    UNSUPPORTED_CF (A8BL8GL8RL8),
    UNSUPPORTED_CF (X8BL8GL8RL8),

    STD_CF_F  (RF32_GF32_BF32_X32, 3,32),
    STD_CF_I  (RS32_GS32_BS32_AS32,4,32),
    STD_CF_I  (RS32_GS32_BS32_X32, 3,32),
    STD_CF_UI (RU32_GU32_BU32_AU32,4,32),
    STD_CF_UI (RU32_GU32_BU32_X32, 3,32),
    STD_CF    (R16_G16_B16_A16,    4,16),
    STD_CF_SN (RN16_GN16_BN16_AN16,4,16),
    STD_CF_UI (RU16_GU16_BU16_AU16,4,16),
    STD_CF_I  (RS16_GS16_BS16_AS16,4,16),
    STD_CF_F  (RF16_GF16_BF16_X16, 3,16),
    STD_CF_F  (RF32_GF32,          2,32),
    STD_CF_I  (RS32_GS32,          2,32),
    STD_CF_UI (RU32_GU32,          2,32),
    STD_CF_I  (RS32,               1,32),
    STD_CF_UI (RU32,               1,32),

    UNSUPPORTED_CF (AU2BU10GU10RU10),

    STD_CF_F  (RF16_GF16,          2,16),
    STD_CF_I  (RS16_GS16,          2,16),
    STD_CF_SN (RN16_GN16,          2,16),
    STD_CF_UI (RU16_GU16,          2,16),
    STD_CF    (R16_G16,            2,16),

    // Easiest GL format/type is RGB half-float, but that's not a HW format.
    // So go up to rgba half-float.
    // GL will discard alpha on glTexImage, fill in 1.0 on glReadPixels.

    NONSTD_CF (BF10GF11RF11, GL_R11F_G11F_B10F_EXT, RF16_GF16_BF16_X16, GL_RGBA,
                GL_HALF_FLOAT, "GL_EXT_packed_float"),

    STD_CF    (G8R8,  2,8),
    STD_CF_SN (GN8RN8,2,8),
    STD_CF_I  (GS8RS8,2,8),
    STD_CF_UI (GU8RU8,2,8),
    STD_CF    (R16,   1,16),
    STD_CF_SN (RN16,  1,16),
    STD_CF_I  (RS16,  1,16),
    STD_CF_UI (RU16,  1,16),
    STD_CF    (R8,    1,8),
    STD_CF_SN (RN8,   1,8),
    STD_CF_I  (RS8,   1,8),
    STD_CF_UI (RU8,   1,8),

    NONSTD_CF (A8, GL_ALPHA8, A8, GL_ALPHA, GL_UNSIGNED_BYTE, NoExt),

    UNSUPPORTED_CF (ZF32),
    UNSUPPORTED_CF (S8Z24),
    NONSTD_CF (X8Z24, GL_DEPTH_COMPONENT, X8Z24, GL_DEPTH_COMPONENT,
                GL_UNSIGNED_INT, NoExt),
    UNSUPPORTED_CF (V8Z24),
    UNSUPPORTED_CF (ZF32_X24S8),
    UNSUPPORTED_CF (X8Z24_X16V8S8),
    UNSUPPORTED_CF (ZF32_X16V8X8),
    UNSUPPORTED_CF (ZF32_X16V8S8),

    NONSTD_CF (S8, GL_STENCIL_INDEX8, S8, GL_STENCIL_INDEX,
              GL_UNSIGNED_BYTE, "GL_ARB_texture_stencil8"),
    UNSUPPORTED_CF (X2BL10GL10RL10_XRBIAS),
    UNSUPPORTED_CF (R16_G16_B16_A16_LWBIAS),
    UNSUPPORTED_CF (X2BL10GL10RL10_XVYCC),
    UNSUPPORTED_CF (Y8_U8__Y8_V8_N422),
    UNSUPPORTED_CF (U8_Y8__V8_Y8_N422),
    UNSUPPORTED_CF (Y8___U8V8_N444),
    UNSUPPORTED_CF (Y8___U8V8_N422),
    UNSUPPORTED_CF (Y8___U8V8_N422R),
    UNSUPPORTED_CF (Y8___V8U8_N420),
    UNSUPPORTED_CF (Y8___U8___V8_N444),
    UNSUPPORTED_CF (Y8___U8___V8_N420),
    UNSUPPORTED_CF (Y10___U10V10_N444),
    UNSUPPORTED_CF (Y10___U10V10_N422),
    UNSUPPORTED_CF (Y10___U10V10_N422R),
    UNSUPPORTED_CF (Y10___V10U10_N420),
    UNSUPPORTED_CF (Y10___U10___V10_N444),
    UNSUPPORTED_CF (Y10___U10___V10_N420),
    UNSUPPORTED_CF (Y12___U12V12_N444),
    UNSUPPORTED_CF (Y12___U12V12_N422),
    UNSUPPORTED_CF (Y12___U12V12_N422R),
    UNSUPPORTED_CF (Y12___V12U12_N420),
    UNSUPPORTED_CF (Y12___U12___V12_N444),
    UNSUPPORTED_CF (Y12___U12___V12_N420),
    UNSUPPORTED_CF (A8Y8U8V8),
    UNSUPPORTED_CF (I1),
    UNSUPPORTED_CF (I2),
    UNSUPPORTED_CF (I4),
    UNSUPPORTED_CF (A4R4G4B4),
    UNSUPPORTED_CF (R4G4B4A4),
    UNSUPPORTED_CF (B4G4R4A4),
    UNSUPPORTED_CF (R5G5B5A1),
    UNSUPPORTED_CF (B5G5R5A1),
    UNSUPPORTED_CF (A8R6x2G6x2B6x2),
    UNSUPPORTED_CF (A8B6x2G6x2R6x2),
    UNSUPPORTED_CF (X1B5G5R5),
    UNSUPPORTED_CF (B5G5R5X1),
    UNSUPPORTED_CF (R5G5B5X1),
    UNSUPPORTED_CF (U8),
    UNSUPPORTED_CF (V8),
    UNSUPPORTED_CF (CR8),
    UNSUPPORTED_CF (CB8),
    UNSUPPORTED_CF (U8V8),
    UNSUPPORTED_CF (V8U8),
    UNSUPPORTED_CF (CR8CB8),
    UNSUPPORTED_CF (CB8CR8),
    UNSUPPORTED_CF (R32_G32_B32_A32),
    UNSUPPORTED_CF (R32_G32_B32),
    UNSUPPORTED_CF (R32_G32),
    UNSUPPORTED_CF (R32_B24G8),
    UNSUPPORTED_CF (G8R24),
    UNSUPPORTED_CF (G24R8),
    UNSUPPORTED_CF (R32),
    UNSUPPORTED_CF (A4B4G4R4),
    UNSUPPORTED_CF (A5B5G5R1),
    UNSUPPORTED_CF (A1B5G5R5),
    UNSUPPORTED_CF (B5G6R5),
    UNSUPPORTED_CF (B6G5R5),
    UNSUPPORTED_CF (Y8_VIDEO),
    UNSUPPORTED_CF (G4R4),
    UNSUPPORTED_CF (R1),
    UNSUPPORTED_CF (E5B9G9R9_SHAREDEXP),
    UNSUPPORTED_CF (G8B8G8R8),
    UNSUPPORTED_CF (B8G8R8G8),
    UNSUPPORTED_CF (DXT1),
    UNSUPPORTED_CF (DXT23),
    UNSUPPORTED_CF (DXT45),
    UNSUPPORTED_CF (DXN1),
    UNSUPPORTED_CF (DXN2),
    UNSUPPORTED_CF (BC6H_SF16),
    UNSUPPORTED_CF (BC6H_UF16),
    UNSUPPORTED_CF (BC7U),
    UNSUPPORTED_CF (X4V4Z24__COV4R4V),
    UNSUPPORTED_CF (X4V4Z24__COV8R8V),
    UNSUPPORTED_CF (V8Z24__COV4R12V),
    UNSUPPORTED_CF (X8Z24_X20V4S8__COV4R4V),
    UNSUPPORTED_CF (X8Z24_X20V4S8__COV8R8V),
    UNSUPPORTED_CF (ZF32_X20V4X8__COV4R4V),
    UNSUPPORTED_CF (ZF32_X20V4X8__COV8R8V),
    UNSUPPORTED_CF (ZF32_X20V4S8__COV4R4V),
    UNSUPPORTED_CF (ZF32_X20V4S8__COV8R8V),
    UNSUPPORTED_CF (X8Z24_X16V8S8__COV4R12V),
    UNSUPPORTED_CF (ZF32_X16V8X8__COV4R12V),
    UNSUPPORTED_CF (ZF32_X16V8S8__COV4R12V),
    UNSUPPORTED_CF (V8Z24__COV8R24V),
    UNSUPPORTED_CF (X8Z24_X16V8S8__COV8R24V),
    UNSUPPORTED_CF (ZF32_X16V8X8__COV8R24V),
    UNSUPPORTED_CF (ZF32_X16V8S8__COV8R24V),
    UNSUPPORTED_CF (B8G8R8A8),

    NONSTD_CF (ASTC_2D_4X4, GL_COMPRESSED_RGBA_ASTC_4x4_KHR, ASTC_2D_4X4, GL_COMPRESSED_RGBA_ASTC_4x4_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_2D_5X4, GL_COMPRESSED_RGBA_ASTC_5x4_KHR, ASTC_2D_5X4, GL_COMPRESSED_RGBA_ASTC_5x4_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_2D_5X5, GL_COMPRESSED_RGBA_ASTC_5x5_KHR, ASTC_2D_5X5, GL_COMPRESSED_RGBA_ASTC_5x5_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_2D_6X5, GL_COMPRESSED_RGBA_ASTC_6x5_KHR, ASTC_2D_6X5, GL_COMPRESSED_RGBA_ASTC_6x5_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_2D_6X6, GL_COMPRESSED_RGBA_ASTC_6x6_KHR, ASTC_2D_6X6, GL_COMPRESSED_RGBA_ASTC_6x6_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_2D_8X5, GL_COMPRESSED_RGBA_ASTC_8x5_KHR, ASTC_2D_8X5, GL_COMPRESSED_RGBA_ASTC_8x5_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_2D_8X6, GL_COMPRESSED_RGBA_ASTC_8x6_KHR, ASTC_2D_8X6, GL_COMPRESSED_RGBA_ASTC_8x6_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_2D_8X8, GL_COMPRESSED_RGBA_ASTC_8x8_KHR, ASTC_2D_8X8, GL_COMPRESSED_RGBA_ASTC_8x8_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_2D_10X5, GL_COMPRESSED_RGBA_ASTC_10x5_KHR, ASTC_2D_10X5, GL_COMPRESSED_RGBA_ASTC_10x5_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_2D_10X6, GL_COMPRESSED_RGBA_ASTC_10x6_KHR, ASTC_2D_10X6, GL_COMPRESSED_RGBA_ASTC_10x6_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_2D_10X8, GL_COMPRESSED_RGBA_ASTC_10x8_KHR, ASTC_2D_10X8, GL_COMPRESSED_RGBA_ASTC_10x8_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_2D_10X10, GL_COMPRESSED_RGBA_ASTC_10x10_KHR, ASTC_2D_10X10, GL_COMPRESSED_RGBA_ASTC_10x10_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_2D_12X10, GL_COMPRESSED_RGBA_ASTC_12x10_KHR, ASTC_2D_12X10, GL_COMPRESSED_RGBA_ASTC_12x10_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_2D_12X12, GL_COMPRESSED_RGBA_ASTC_12x12_KHR, ASTC_2D_12X12, GL_COMPRESSED_RGBA_ASTC_12x12_KHR, GL_UNSIGNED_BYTE,"GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_SRGB_2D_4X4, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR, ASTC_SRGB_2D_4X4, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR, GL_UNSIGNED_BYTE,"GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_SRGB_2D_5X4, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR, ASTC_SRGB_2D_5X4, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_SRGB_2D_5X5, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR, ASTC_SRGB_2D_5X5, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_SRGB_2D_6X5, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR, ASTC_SRGB_2D_6X5, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_SRGB_2D_6X6, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR, ASTC_SRGB_2D_6X6, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_SRGB_2D_8X5, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR, ASTC_SRGB_2D_8X5, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_SRGB_2D_8X6, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR, ASTC_SRGB_2D_8X6, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_SRGB_2D_8X8, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR, ASTC_SRGB_2D_8X8, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_SRGB_2D_10X5, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR, ASTC_SRGB_2D_10X5, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_SRGB_2D_10X6, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR, ASTC_SRGB_2D_10X6, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_SRGB_2D_10X8, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR, ASTC_SRGB_2D_10X8, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_SRGB_2D_10X10, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR, ASTC_SRGB_2D_10X10, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_SRGB_2D_12X10, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR, ASTC_SRGB_2D_12X10, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR, GL_UNSIGNED_BYTE, "GL_KHR_texture_compression_astc_ldr"),
    NONSTD_CF (ASTC_SRGB_2D_12X12, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR, ASTC_SRGB_2D_12X12, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR, GL_UNSIGNED_BYTE,"GL_KHR_texture_compression_astc_ldr"),

    UNSUPPORTED_CF (ETC2_RGB),
    UNSUPPORTED_CF (ETC2_RGB_PTA),
    UNSUPPORTED_CF (ETC2_RGBA),
    UNSUPPORTED_CF (EAC),
    UNSUPPORTED_CF (EACX2),

    // Size of array *must* be Format_NUM_FORMATS at this point.

    // Here begin entries for GL Texture Internal Formats that don't map
    // to a unique mods ColorFormat.
    //
    // Without a ColorFormat, these cannot be used as a render-target.
    // But they're OK for GL texture formats, the GL lib will colwert for us.
    //
    // It would be good to add mods ColorFormats for each of these that is
    // a valid ROP format (i.e. not the compressed formats).

    // TODO : These formats seem to cause intermittent miscompares on
    // GT2xx systems.
    // Color formats not covered by any of the ColorUtils formats by default
    STD_CF_I  (B8_G8_R8, 3,8),
    STD_CF_SN (B8_G8_R8, 3,8),
    STD_CF_UI (B8_G8_R8, 3,8),

    // Use R16_G16_B16_A16 as the base format, but use a 3 component 16-bit
    // format internally
    STD_CF_COMP_DIFF (R16_G16_B16_A16, 3, 4, 16, false, true, false),
    STD_CF_COMP_DIFF (R16_G16_B16_A16, 3, 4, 16, false, true, true),
    STD_CF_COMP_DIFF (R16_G16_B16_A16, 3, 4, 16, false, false, false),
    STD_CF_COMP_DIFF (R16_G16_B16_A16, 3, 4, 16, false, false, true),

    // The compressed texture formats use a 4x4 block compression
    // scheme.  They may not have borders, and must be 2d or 3d.
    // They are not valid ROP formats, so don't use them as FBOs.

    {   ColorUtils::G8R8, GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT,
        ColorUtils::G8R8, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE,
        "GL_EXT_texture_compression_latc"
    },
    {   ColorUtils::R8, GL_COMPRESSED_LUMINANCE_LATC1_EXT,
        ColorUtils::R8, GL_LUMINANCE, GL_UNSIGNED_BYTE,
        "GL_EXT_texture_compression_latc"
    },
    {   ColorUtils::GN8RN8, GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT,
        ColorUtils::GN8RN8, GL_LUMINANCE_ALPHA, GL_BYTE,
        "GL_EXT_texture_compression_latc"
    },
    {   ColorUtils::RN8, GL_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT,
        ColorUtils::RN8, GL_LUMINANCE, GL_BYTE,
        "GL_EXT_texture_compression_latc"
    },
    {   ColorUtils::B8_G8_R8, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
        ColorUtils::B8_G8_R8, GL_RGB, GL_UNSIGNED_BYTE,
        "GL_EXT_texture_compression_s3tc"
    },
    {   ColorUtils::R8G8B8A8, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
        ColorUtils::R8G8B8A8, GL_RGBA, GL_UNSIGNED_BYTE,
        "GL_EXT_texture_compression_s3tc"
    },
    {   ColorUtils::R8G8B8A8, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
        ColorUtils::R8G8B8A8, GL_RGBA, GL_UNSIGNED_BYTE,
        "GL_EXT_texture_compression_s3tc"
    },
    {   ColorUtils::R8G8B8A8, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
        ColorUtils::R8G8B8A8, GL_RGBA, GL_UNSIGNED_BYTE,
        "GL_EXT_texture_compression_s3tc"
    },
    {   ColorUtils::RU16, GL_COMPRESSED_R11_EAC,
        ColorUtils::RU16, GL_RED, GL_UNSIGNED_SHORT,
        "GL_ARB_ES3_compatibility"
    },
    {   ColorUtils::RS16, GL_COMPRESSED_SIGNED_R11_EAC,
        ColorUtils::RS16, GL_RED, GL_SHORT,
        "GL_ARB_ES3_compatibility"
    },
    {   ColorUtils::RU16_GU16, GL_COMPRESSED_RG11_EAC,
        ColorUtils::RU16_GU16, GL_RG, GL_UNSIGNED_SHORT,
        "GL_ARB_ES3_compatibility"
    },
    {   ColorUtils::RS16_GS16, GL_COMPRESSED_SIGNED_RG11_EAC,
        ColorUtils::RS16_GS16, GL_RG, GL_SHORT,
        "GL_ARB_ES3_compatibility"
    },
    {   ColorUtils::B8_G8_R8, GL_COMPRESSED_RGB8_ETC2,
        ColorUtils::B8_G8_R8, GL_RGB, GL_UNSIGNED_BYTE,
        "GL_ARB_ES3_compatibility"
    },
    {   ColorUtils::B8_G8_R8, GL_COMPRESSED_SRGB8_ETC2, // Not sure about MODS color format
        ColorUtils::B8_G8_R8, GL_RGB, GL_BYTE,
        "GL_ARB_ES3_compatibility"
    },
    {   ColorUtils::A8R8G8B8, GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2, // Not sure about MODS color format
        ColorUtils::A8R8G8B8, GL_RGBA, GL_UNSIGNED_BYTE,
        "GL_ARB_ES3_compatibility"
    },
    {   ColorUtils::A8R8G8B8, GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, // Not sure about MODS color format
        ColorUtils::A8R8G8B8, GL_RGBA, GL_BYTE,
        "GL_ARB_ES3_compatibility"
    },
    {   ColorUtils::A8R8G8B8, GL_COMPRESSED_RGBA8_ETC2_EAC, // Not sure about MODS color format
        ColorUtils::A8R8G8B8, GL_RGBA, GL_UNSIGNED_BYTE,
        "GL_ARB_ES3_compatibility"
    },
    {   ColorUtils::A8R8G8B8, GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC, // Not sure about MODS color format
        ColorUtils::A8R8G8B8, GL_RGBA, GL_BYTE,
        "GL_ARB_ES3_compatibility"
    },

    // Most correct would be to generate a 5,9,9,9 pixel myself, but that's hard.
    // Easiest is to pass RGBA_F16 to GL and let them discard alpha and calc
    // the best-fit shared exponent.  But that might be really slow for a big
    // texture.
    // Instead, pretend it's r8g8b8a8 on the mods side and let HW deal with the
    // resulting highly-random garbage texel data.

    {   ColorUtils::R8G8B8A8, GL_RGB9_E5_EXT,
        ColorUtils::R8G8B8A8, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV_EXT,
        "GL_EXT_texture_shared_exponent"
    },

    // These formats are valid ROP formats, we just don't have a matching
    // unique mods ColorFormat for them yet.

    {   ColorUtils::R16,  GL_ALPHA16,
        ColorUtils::R16,  GL_ALPHA, GL_UNSIGNED_SHORT, NoExt
    },
    {   ColorUtils::RF16, GL_ALPHA16F_ARB,
        ColorUtils::RF16, GL_ALPHA, GL_HALF_FLOAT, "GL_ARB_texture_float"
    },
    {   ColorUtils::RF32, GL_ALPHA32F_ARB,
        ColorUtils::RF32, GL_ALPHA, GL_FLOAT, "GL_ARB_texture_float"
    },
    {   ColorUtils::RF16, GL_INTENSITY16F_ARB,
        ColorUtils::RF16, GL_RED, GL_HALF_FLOAT, "GL_ARB_texture_float"
    },
    {   ColorUtils::RF32, GL_INTENSITY32F_ARB,
        ColorUtils::RF32, GL_RED, GL_FLOAT, "GL_ARB_texture_float"
    },
    {   ColorUtils::R8G8B8A8, GL_RGBA4,
        ColorUtils::R8G8B8A8, GL_RGBA, GL_UNSIGNED_BYTE, NoExt
    },
    // These color formats are used for ATOMIM instructions.
    {   // 32 bit LOADIM/STOREIM/ATOMIM.U32
        ColorUtils::RU32, GL_R32UI,
        ColorUtils::RU32, GL_RED_INTEGER, GL_UNSIGNED_INT, NoExt
    },
    {   // 32 bit LOADIM/STOREIM/ATOMIM.S32
        ColorUtils::RS32, GL_RG32I,
        ColorUtils::RS32, GL_RED_INTEGER, GL_INT, NoExt
    },
    {   // 32 bit LOADIM/STOREIM/ATOMIM.U32X2
        ColorUtils::RU32_GU32, GL_RG32UI,
        ColorUtils::RU32_GU32, GL_RG_INTEGER, GL_UNSIGNED_INT, NoExt
    },
    {   // 32 bit LOADIM/STOREIM/ATOMIM.S32X2
        ColorUtils::RS32_GS32, GL_R32I,
        ColorUtils::RS32_GS32, GL_RG_INTEGER, GL_INT, NoExt
    }

};

static const UINT32 s_TexInfos_NUM = static_cast<UINT32>(NUMELEMS(s_TexInfos));

#ifdef DEBUG
RC mglVerifyTexFmts ()
{
    bool bError = false;
    for (UINT32 i = 0;
          (i < ColorUtils::Format_NUM_FORMATS) && (i < s_TexInfos_NUM); i++)
    {
        ColorUtils::Format lwrFmt = static_cast<ColorUtils::Format>(i);
        if (lwrFmt != s_TexInfos[i].ColorFormat)
        {
            Printf(Tee::PriHigh,
                 "Texture info error at index %d\n"
                 "    Expected format : %s\n"
                 "    Actual format   : %s\n",
                 i,
                 ColorUtils::FormatToString(lwrFmt).c_str(),
                 ColorUtils::FormatToString(s_TexInfos[i].ColorFormat).c_str());
            bError = true;
        }
    }

    if (s_TexInfos_NUM < ColorUtils::Format_NUM_FORMATS)
    {
        bError = true;
        for (UINT32 i = s_TexInfos_NUM;
              (i < ColorUtils::Format_NUM_FORMATS); i++)
        {
            ColorUtils::Format lwrFmt = static_cast<ColorUtils::Format>(i);
            Printf(Tee::PriHigh,
                   "Missing color format %s\n",
                   ColorUtils::FormatToString(lwrFmt).c_str());
        }
    }
    return bError ? RC::SOFTWARE_ERROR : OK;
}
#endif

const mglTexFmtInfo * mglGetTexFmtInfo (ColorUtils::Format cf)
{
    if (cf < ColorUtils::Format_NUM_FORMATS)
    {
        MASSERT(cf == s_TexInfos[cf].ColorFormat);
        return &s_TexInfos[cf];
    }
    return NULL;
}

const mglTexFmtInfo * mglGetTexFmtInfo (GLenum txif)
{
    // To Do: keep a InternalFormat index for s_TexInfos, so that
    // we don't have to do a linear search when not looking up by ColorFormat.

    for (UINT32 i = 0; i < s_TexInfos_NUM; i++)
    {
        if (s_TexInfos[i].InternalFormat == txif)
            return &s_TexInfos[i];
    }
    return &s_TexInfos[ColorUtils::LWFMT_NONE];
}

GLenum mglColor2TexFmt (ColorUtils::Format cf)
{
    if (cf < ColorUtils::Format_NUM_FORMATS)
    {
        MASSERT(cf == s_TexInfos[cf].ColorFormat);
        return s_TexInfos[cf].InternalFormat;
    }
    return 0;
}

ColorUtils::Format mglTexFmt2Color (GLenum tif)
{
    // To Do: keep a InternalFormat index for s_TexInfos, so that
    // we don't have to do a linear search when not looking up by ColorFormat.

    for (UINT32 i = 0; i < s_TexInfos_NUM; i++)
    {
        if (s_TexInfos[i].InternalFormat == tif)
            return (ColorUtils::Format)i;
    }
    return ColorUtils::LWFMT_NONE;
}

//-----------------------------------------------------------------------------
mglTimerQuery::mglTimerQuery()
 :  m_State(Idle),
    m_Id(0),
    m_Seconds(0.0)
{
#if defined(GL_EXT_timer_query)
    if (mglExtensionSupported("GL_EXT_timer_query"))
    {
        glGenQueriesARB(1, &m_Id);
    }
#endif
}

mglTimerQuery::~mglTimerQuery()
{
#if defined(GL_EXT_timer_query)
    if (m_Id)
        glDeleteQueriesARB(1, &m_Id);
#endif
}

void mglTimerQuery::Begin()
{
#if defined(GL_EXT_timer_query)
    if (m_Id)
    {
        MASSERT(m_State == Idle);
        m_State = Begun;
        glBeginQueryARB(GL_TIME_ELAPSED_EXT, m_Id);
    }
#endif
}

void mglTimerQuery::End()
{
#if defined(GL_EXT_timer_query)
    if (m_Id)
    {
        MASSERT(m_State == Begun);
        m_State = Waiting;
        glEndQueryARB(GL_TIME_ELAPSED_EXT);
    }
#endif
}

mglTimerQuery::State mglTimerQuery::GetState()
{
#if defined(GL_EXT_timer_query)
    if (m_State == Waiting)
    {
        // Check with GL, we might be done now.
        GLint tmp;
        glGetQueryObjectivARB(m_Id, GL_QUERY_RESULT_AVAILABLE, &tmp);
        if (tmp == GL_TRUE)
        {
            FinishQuery();
        }
    }
#endif
    return m_State;
}

double mglTimerQuery::GetSeconds()
{
    if (m_State == Waiting)
    {
        // Blocks until results are available.
        FinishQuery();
    }
    m_State = Idle;
    return m_Seconds;
}

void mglTimerQuery::FinishQuery()
{
    if (m_State != Waiting)
        return;
#if defined(GL_EXT_timer_query)
    GLuint64EXT nsecs;
    Tasker::DetachThread detach;
    glGetQueryObjectui64vEXT(m_Id, GL_QUERY_RESULT, &nsecs);
    m_State = Unread;
    m_Seconds = nsecs / 1e9;
#endif
}

UINT08 mglGetAstcFormatBlockX(GLenum format)
{
    UINT08 ret = 0;

    switch(format)
    {
        case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
            ret = 4;
            break;

        case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
        case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
            ret = 5;
            break;

        case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
        case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
            ret = 6;
            break;

        case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
        case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
        case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
            ret = 8;
            break;

        case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
        case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
        case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
        case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
            ret = 10;
            break;

        case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
        case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
            ret = 12;
            break;

        default:
            Printf(Tee::PriError, "Format is not an ASTC enum: 0x%x.\n", format);
            break;
    }

    return ret;
}

UINT08 mglGetAstcFormatBlockY(GLenum format)
{
    UINT08 ret = 0;

    switch(format)
    {
        case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
        case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
            ret = 4;
            break;

        case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
        case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
        case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
        case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
            ret = 5;
            break;

        case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
        case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
        case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
            ret = 6;
            break;

        case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
        case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
            ret = 8;
            break;

        case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
        case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
            ret = 10;
            break;

        case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
            ret = 12;
            break;

        default:
            Printf(Tee::PriError, "Format is not an ASTC enum: 0x%x.\n", format);
            break;
    }

    return ret;
}

bool mglIsFormatAstc(GLenum format)
{
    bool ret = false;

    switch(format)
    {
        case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
        case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
        case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
        case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
        case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
        case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
        case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
        case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
        case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
        case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
        case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
        case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
        case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
        case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
            ret = true;

        default:
            break;
    }

    return ret;
}

