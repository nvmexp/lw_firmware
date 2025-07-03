/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>
#include <math.h>
#include <string>
#include <assert.h>

#define CHECK_RC(f)           \
   do {                       \
      if (OK != (rc = (f)))   \
         return rc;           \
   } while (0)

// return codes (0 == success)
enum RC {
    OK = 0,
    CANNOT_ALLOCATE_MEMORY,
    SOFTWARE_ERROR
};

//------------------------------------------------------------------------------
RC mglGetRC()
{
    return (RC) glGetError();
}

// Return true if this GL extension is supported on the current GPU & display.
extern bool mglExtensionSupported(const char *extension);

// Flush write-combined or cached-but-not-snooped memory so that the GPU
// will see the latest data written by CPU.
extern void FlushMem(volatile void * StartAddress, volatile void * EndAddress);

// Delay for at least this many milliseconds.
extern void SleepMs(int milliseconds);

// Return a 32-bit unsigned value, from a pseudo-random-number generator.
extern unsigned int GetRandom();

// message filtering system
enum PrintPri {
    PriHigh,     // should always print
    PriLow,      // should print if user asked for verbose
    PriDebug
};

extern void Printf(PrintPri pri, const char * Fmt, ...);

// this is a token to end the pattern array.  There isn't anything
// special about the constant 0x123

static const GLuint END_PATTERN = 0x123;

static const GLuint s_ShortMats[]=
{  0x00000000, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA, END_PATTERN};

static const GLuint s_LongMats[]=
{  0x00000000, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA,
    0x33333333, 0xCCCCCCCC, 0xF0F0F0F0, 0x0F0F0F0F,
    0x00FF00FF, 0xFF00FF00, 0x0000FFFF, 0xFFFF0000, END_PATTERN};

static const GLuint s_WalkingOnes[]=
{  0x00000001, 0x00000002, 0x00000004, 0x00000008,
    0x00000010, 0x00000020, 0x00000040, 0x00000080,
    0x00000100, 0x00000200, 0x00000400, 0x00000800,
    0x00001000, 0x00002000, 0x00004000, 0x00008000,
    0x00010000, 0x00020000, 0x00040000, 0x00080000,
    0x00100000, 0x00200000, 0x00400000, 0x00800000,
    0x01000000, 0x02000000, 0x04000000, 0x08000000,
    0x10000000, 0x20000000, 0x40000000, 0x80000000,
    0x00000000, END_PATTERN};

static const GLuint s_WalkingZeros[]=
{  0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFB, 0xFFFFFFF7,
    0xFFFFFFEF, 0xFFFFFFDF, 0xFFFFFFBF, 0xFFFFFF7F,
    0xFFFFFEFF, 0xFFFFFDFF, 0xFFFFFBFF, 0xFFFFF7FF,
    0xFFFFEFFF, 0xFFFFDFFF, 0xFFFFBFFF, 0xFFFF7FFF,
    0xFFFEFFFF, 0xFFFDFFFF, 0xFFFBFFFF, 0xFFF7FFFF,
    0xFFEFFFFF, 0xFFDFFFFF, 0xFFBFFFFF, 0xFF7FFFFF,
    0xFEFFFFFF, 0xFDFFFFFF, 0xFBFFFFFF, 0xF7FFFFFF,
    0xEFFFFFFF, 0xDFFFFFFF, 0xBFFFFFFF, 0x7FFFFFFF,
    0xFFFFFFFF, END_PATTERN};

static const GLuint s_SuperZeroOne[]=
{  0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
    0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
    0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
    0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
    0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
    0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
    0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
    0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
    0xFFFFFFFF, END_PATTERN};

static const GLuint s_ByteSuper[]=
{  0x00FF0000, 0x00FFFFFF, 0x00FF0000, 0xFF00FFFF,
    0x0000FF00, 0xFFFF00FF, 0x000000FF, 0xFFFFFF00,
    0x00000000, END_PATTERN};

static const GLuint s_CheckerBoard[]=
{  0xAAAAAAAA, 0x55555555, 0xAAAAAAAA, 0x55555555,
    0xAAAAAAAA, 0x55555555, 0xAAAAAAAA, 0x55555555,
    0xAAAAAAAA, END_PATTERN};

static const GLuint s_IlwCheckerBd[]=
{  0x55555555, 0xAAAAAAAA, 0x55555555, 0xAAAAAAAA,
    0x55555555, 0xAAAAAAAA, 0x55555555, 0xAAAAAAAA,
    0xAAAAAAAA, END_PATTERN};

static const GLuint s_WalkSwizzle[]=
{  0x00000001, 0x7FFFFFFF, 0x00000002, 0xBFFFFFFF,
    0x00000004, 0xDFFFFFFF, 0x00000008, 0xEFFFFFFF,
    0x00000010, 0xF7FFFFFF, 0x00000020, 0xFBFFFFFF,
    0x00000040, 0xFDFFFFFF, 0x00000080, 0xFEFFFFFF,
    0x00000100, 0xFF7FFFFF, 0x00000200, 0xFFBFFFFF,
    0x00000400, 0xFFDFFFFF, 0x00000800, 0xFFEFFFFF,
    0x00001000, 0xFFF7FFFF, 0x00002000, 0xFFFBFFFF,
    0x00004000, 0xFFFDFFFF, 0x00008000, 0xFFFEFFFF,
    0x00010000, 0xFFFF7FFF, 0x00020000, 0xFFFFBFFF,
    0x00040000, 0xFFFFDFFF, 0x00080000, 0xFFFFEFFF,
    0x00100000, 0xFFFFF7FF, 0x00200000, 0xFFFFFBFF,
    0x00400000, 0xFFFFFDFF, 0x00800000, 0xFFFFFEFF,
    0x01000000, 0xFFFFFF7F, 0x02000000, 0xFFFFFFBF,
    0x04000000, 0xFFFFFFDF, 0x08000000, 0xFFFFFFEF,
    0x10000000, 0xFFFFFFF7, 0x20000000, 0xFFFFFFFB,
    0x40000000, 0xFFFFFFFD, 0x80000000, 0xFFFFFFFE,
    0x00000000, END_PATTERN};

static const GLuint s_WalkNormal[]=
{  0x00000001, 0xFFFFFFFE, 0x00000002, 0xFFFFFFFD,
    0x00000004, 0xFFFFFFFB, 0x00000008, 0xFFFFFFF7,
    0x00000010, 0xFFFFFFEF, 0x00000020, 0xFFFFFFDF,
    0x00000040, 0xFFFFFFBF, 0x00000080, 0xFFFFFF7F,
    0x00000100, 0xFFFFFEFF, 0x00000200, 0xFFFFFDFF,
    0x00000400, 0xFFFFFBFF, 0x00000800, 0xFFFFF7FF,
    0x00001000, 0xFFFFEFFF, 0x00002000, 0xFFFFDFFF,
    0x00004000, 0xFFFFBFFF, 0x00008000, 0xFFFF7FFF,
    0x00010000, 0xFFFEFFFF, 0x00020000, 0xFFFDFFFF,
    0x00040000, 0xFFFBFFFF, 0x00080000, 0xFFF7FFFF,
    0x00100000, 0xFFEFFFFF, 0x00200000, 0xFFDFFFFF,
    0x00400000, 0xFFBFFFFF, 0x00800000, 0xFF7FFFFF,
    0x01000000, 0xFEFFFFFF, 0x02000000, 0xFDFFFFFF,
    0x04000000, 0xFBFFFFFF, 0x08000000, 0xF7FFFFFF,
    0x10000000, 0xEFFFFFFF, 0x20000000, 0xDFFFFFFF,
    0x40000000, 0xBFFFFFFF, 0x80000000, 0xEFFFFFFF,
    0x00000000, END_PATTERN};

static const GLuint s_DoubleZeroOne[]=
{  0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, END_PATTERN};

static const GLuint s_TripleSuper[]=
{  0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, END_PATTERN};

static const GLuint s_QuadZeroOne[]=
{  0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, END_PATTERN};

static const GLuint s_TripleWhammy[]=
{  0x00000000, 0x00000001, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFF,
    0x00000000, 0x00000002, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFD, 0xFFFFFFFF,
    0x00000000, 0x00000004, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFB, 0xFFFFFFFF,
    0x00000000, 0x00000008, 0x00000000, 0xFFFFFFFF, 0xFFFFFFF7, 0xFFFFFFFF,
    0x00000000, 0x00000010, 0x00000000, 0xFFFFFFFF, 0xFFFFFFEF, 0xFFFFFFFF,
    0x00000000, 0x00000020, 0x00000000, 0xFFFFFFFF, 0xFFFFFFDF, 0xFFFFFFFF,
    0x00000000, 0x00000040, 0x00000000, 0xFFFFFFFF, 0xFFFFFFBF, 0xFFFFFFFF,
    0x00000000, 0x00000080, 0x00000000, 0xFFFFFFFF, 0xFFFFFF7F, 0xFFFFFFFF,
    0x00000000, 0x00000100, 0x00000000, 0xFFFFFFFF, 0xFFFFFEFF, 0xFFFFFFFF,
    0x00000000, 0x00000200, 0x00000000, 0xFFFFFFFF, 0xFFFFFDFF, 0xFFFFFFFF,
    0x00000000, 0x00000400, 0x00000000, 0xFFFFFFFF, 0xFFFFFBFF, 0xFFFFFFFF,
    0x00000000, 0x00000800, 0x00000000, 0xFFFFFFFF, 0xFFFFF7FF, 0xFFFFFFFF,
    0x00000000, 0x00001000, 0x00000000, 0xFFFFFFFF, 0xFFFFEFFF, 0xFFFFFFFF,
    0x00000000, 0x00002000, 0x00000000, 0xFFFFFFFF, 0xFFFFDFFF, 0xFFFFFFFF,
    0x00000000, 0x00004000, 0x00000000, 0xFFFFFFFF, 0xFFFFBFFF, 0xFFFFFFFF,
    0x00000000, 0x00008000, 0x00000000, 0xFFFFFFFF, 0xFFFF7FFF, 0xFFFFFFFF,
    0x00000000, 0x00010000, 0x00000000, 0xFFFFFFFF, 0xFFFEFFFF, 0xFFFFFFFF,
    0x00000000, 0x00020000, 0x00000000, 0xFFFFFFFF, 0xFFFDFFFF, 0xFFFFFFFF,
    0x00000000, 0x00040000, 0x00000000, 0xFFFFFFFF, 0xFFFBFFFF, 0xFFFFFFFF,
    0x00000000, 0x00080000, 0x00000000, 0xFFFFFFFF, 0xFFF7FFFF, 0xFFFFFFFF,
    0x00000000, 0x00100000, 0x00000000, 0xFFFFFFFF, 0xFFEFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00200000, 0x00000000, 0xFFFFFFFF, 0xFFDFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00400000, 0x00000000, 0xFFFFFFFF, 0xFFBFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00800000, 0x00000000, 0xFFFFFFFF, 0xFF7FFFFF, 0xFFFFFFFF,
    0x00000000, 0x01000000, 0x00000000, 0xFFFFFFFF, 0xFEFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x02000000, 0x00000000, 0xFFFFFFFF, 0xFDFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x04000000, 0x00000000, 0xFFFFFFFF, 0xFBFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x08000000, 0x00000000, 0xFFFFFFFF, 0xF7FFFFFF, 0xFFFFFFFF,
    0x00000000, 0x10000000, 0x00000000, 0xFFFFFFFF, 0xEFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x20000000, 0x00000000, 0xFFFFFFFF, 0xDFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x40000000, 0x00000000, 0xFFFFFFFF, 0xBFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x80000000, 0x00000000, 0xFFFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF,
    0x00000000, END_PATTERN};

static const GLuint s_IsolatedOnes[]=
{  0x00000000, 0x00000000, 0x00000000, 0x00000001,
    0x00000000, 0x00000000, 0x00000000, 0x00000002,
    0x00000000, 0x00000000, 0x00000000, 0x00000004,
    0x00000000, 0x00000000, 0x00000000, 0x00000008,
    0x00000000, 0x00000000, 0x00000000, 0x00000010,
    0x00000000, 0x00000000, 0x00000000, 0x00000020,
    0x00000000, 0x00000000, 0x00000000, 0x00000040,
    0x00000000, 0x00000000, 0x00000000, 0x00000080,
    0x00000000, 0x00000000, 0x00000000, 0x00000100,
    0x00000000, 0x00000000, 0x00000000, 0x00000200,
    0x00000000, 0x00000000, 0x00000000, 0x00000400,
    0x00000000, 0x00000000, 0x00000000, 0x00000800,
    0x00000000, 0x00000000, 0x00000000, 0x00001000,
    0x00000000, 0x00000000, 0x00000000, 0x00002000,
    0x00000000, 0x00000000, 0x00000000, 0x00004000,
    0x00000000, 0x00000000, 0x00000000, 0x00008000,
    0x00000000, 0x00000000, 0x00000000, 0x00010000,
    0x00000000, 0x00000000, 0x00000000, 0x00020000,
    0x00000000, 0x00000000, 0x00000000, 0x00040000,
    0x00000000, 0x00000000, 0x00000000, 0x00080000,
    0x00000000, 0x00000000, 0x00000000, 0x00100000,
    0x00000000, 0x00000000, 0x00000000, 0x00200000,
    0x00000000, 0x00000000, 0x00000000, 0x00400000,
    0x00000000, 0x00000000, 0x00000000, 0x00800000,
    0x00000000, 0x00000000, 0x00000000, 0x01000000,
    0x00000000, 0x00000000, 0x00000000, 0x02000000,
    0x00000000, 0x00000000, 0x00000000, 0x04000000,
    0x00000000, 0x00000000, 0x00000000, 0x08000000,
    0x00000000, 0x00000000, 0x00000000, 0x10000000,
    0x00000000, 0x00000000, 0x00000000, 0x20000000,
    0x00000000, 0x00000000, 0x00000000, 0x40000000,
    0x00000000, 0x00000000, 0x00000000, 0x80000000,
    0x00000000, END_PATTERN};

static const GLuint s_IsolatedZeros[]=
{  0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFD,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFB,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFF7,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFEF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFDF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFBF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFF7F,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFEFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFDFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFBFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFF7FF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFEFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFDFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFBFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF7FFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFEFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFDFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFBFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFF7FFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFEFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFDFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFBFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFF7FFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFEFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFDFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFBFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xF7FFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xEFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xDFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xBFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FFFFFFF,
    0xFFFFFFFF, END_PATTERN};

static const GLuint s_SlowFalling[]=
{  0xFFFFFFFF, 0x00000001, 0x00000000,
    0xFFFFFFFF, 0x00000002, 0x00000000,
    0xFFFFFFFF, 0x00000004, 0x00000000,
    0xFFFFFFFF, 0x00000008, 0x00000000,
    0xFFFFFFFF, 0x00000010, 0x00000000,
    0xFFFFFFFF, 0x00000020, 0x00000000,
    0xFFFFFFFF, 0x00000040, 0x00000000,
    0xFFFFFFFF, 0x00000080, 0x00000000,
    0xFFFFFFFF, 0x00000100, 0x00000000,
    0xFFFFFFFF, 0x00000200, 0x00000000,
    0xFFFFFFFF, 0x00000400, 0x00000000,
    0xFFFFFFFF, 0x00000800, 0x00000000,
    0xFFFFFFFF, 0x00001000, 0x00000000,
    0xFFFFFFFF, 0x00002000, 0x00000000,
    0xFFFFFFFF, 0x00004000, 0x00000000,
    0xFFFFFFFF, 0x00008000, 0x00000000,
    0xFFFFFFFF, 0x00010000, 0x00000000,
    0xFFFFFFFF, 0x00020000, 0x00000000,
    0xFFFFFFFF, 0x00040000, 0x00000000,
    0xFFFFFFFF, 0x00080000, 0x00000000,
    0xFFFFFFFF, 0x00100000, 0x00000000,
    0xFFFFFFFF, 0x00200000, 0x00000000,
    0xFFFFFFFF, 0x00400000, 0x00000000,
    0xFFFFFFFF, 0x00800000, 0x00000000,
    0xFFFFFFFF, 0x01000000, 0x00000000,
    0xFFFFFFFF, 0x02000000, 0x00000000,
    0xFFFFFFFF, 0x04000000, 0x00000000,
    0xFFFFFFFF, 0x08000000, 0x00000000,
    0xFFFFFFFF, 0x10000000, 0x00000000,
    0xFFFFFFFF, 0x20000000, 0x00000000,
    0xFFFFFFFF, 0x40000000, 0x00000000,
    0xFFFFFFFF, 0x80000000, 0x00000000,
    0xFFFFFFFF, END_PATTERN};

static const GLuint s_SlowRising[]=
{  0x00000000, 0xFFFFFFFE, 0xFFFFFFFF,
    0x00000000, 0xFFFFFFFD, 0xFFFFFFFF,
    0x00000000, 0xFFFFFFFB, 0xFFFFFFFF,
    0x00000000, 0xFFFFFFF7, 0xFFFFFFFF,
    0x00000000, 0xFFFFFFEF, 0xFFFFFFFF,
    0x00000000, 0xFFFFFFDF, 0xFFFFFFFF,
    0x00000000, 0xFFFFFFBF, 0xFFFFFFFF,
    0x00000000, 0xFFFFFF7F, 0xFFFFFFFF,
    0x00000000, 0xFFFFFEFF, 0xFFFFFFFF,
    0x00000000, 0xFFFFFDFF, 0xFFFFFFFF,
    0x00000000, 0xFFFFFBFF, 0xFFFFFFFF,
    0x00000000, 0xFFFFF7FF, 0xFFFFFFFF,
    0x00000000, 0xFFFFEFFF, 0xFFFFFFFF,
    0x00000000, 0xFFFFDFFF, 0xFFFFFFFF,
    0x00000000, 0xFFFFBFFF, 0xFFFFFFFF,
    0x00000000, 0xFFFF7FFF, 0xFFFFFFFF,
    0x00000000, 0xFFFEFFFF, 0xFFFFFFFF,
    0x00000000, 0xFFFDFFFF, 0xFFFFFFFF,
    0x00000000, 0xFFFBFFFF, 0xFFFFFFFF,
    0x00000000, 0xFFF7FFFF, 0xFFFFFFFF,
    0x00000000, 0xFFEFFFFF, 0xFFFFFFFF,
    0x00000000, 0xFFDFFFFF, 0xFFFFFFFF,
    0x00000000, 0xFFBFFFFF, 0xFFFFFFFF,
    0x00000000, 0xFF7FFFFF, 0xFFFFFFFF,
    0x00000000, 0xFEFFFFFF, 0xFFFFFFFF,
    0x00000000, 0xFDFFFFFF, 0xFFFFFFFF,
    0x00000000, 0xFBFFFFFF, 0xFFFFFFFF,
    0x00000000, 0xF7FFFFFF, 0xFFFFFFFF,
    0x00000000, 0xEFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0xDFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0xBFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x7FFFFFFF, 0xFFFFFFFF,
    0x00000000, END_PATTERN};

static const GLuint * s_Patterns[]=
{
    s_ShortMats
    ,s_LongMats
    ,0                // random
    ,s_WalkingOnes
    ,s_WalkingZeros
    ,s_SuperZeroOne
    ,0                // random
    ,s_ByteSuper
    ,s_CheckerBoard
    ,s_IlwCheckerBd
    ,s_WalkSwizzle
    ,s_WalkNormal
    ,s_DoubleZeroOne
    ,s_TripleSuper
    ,s_QuadZeroOne
    ,s_TripleWhammy
    ,0                // random
    ,s_IsolatedOnes
    ,s_IsolatedZeros
    ,s_SlowFalling
    ,s_SlowRising
};

static const GLuint m_NumPatterns = sizeof(s_Patterns) / sizeof(const GLuint *);

class GLStress
{
public:

    struct GLS_Args
    {
        int            Width;            // in pixels, ignoring FSAA stuff
        int            Height;           // in pixels, ignoring FSAA stuff
        int            Depth;            // color depth in bits, counting alpha
        int            ZDepth;           // z depth in bits, counting stencil
        int            TxSize;           // size of textures in texels (w and h).
        int            NumTextures;      // how many different textures to draw
        double         TxD;              // log2(texels per pixel), affects tex coord setup
        bool           HighTess;         // use a very high tesselation (small triangles).
        bool           Rotate;           // rotate all rendering 180%
        bool           UseMM;            // use mipmapping
        bool           TxGenQ;           // generate texture Q coords
        bool           Use3dTextures;    // use 3d textures
        unsigned int   NumLights;        // enable lighting if > 0 (max 8)
        bool           SpotLights;       // use spotlights (else positional lights)
        bool           Normals;          // send normals
        bool           Stencil;          // enable stencil test
        bool           Ztest;            // enable depth testing
        bool           Stipple;          // enable polygon stipple test
        bool           TwoTex;           // turn on a second texture unit
        bool           Slow;             // for debugging
        bool           Fog;              // if true, use fog.
        bool           SkipCenter;       // don't render to center 10% of screen
        bool           BoringXform;      // use a straight-through x.y == pixel location transform
        bool           DrawElements;     // use glDrawElements (else glVertex).
        bool           DrawPoints;       // draw points instead of quad-strips.
        bool           SendW;            // Send a 4-tuple position, not 3-tuple
        double         PointSize;        // size of points, if 0.0 vx prog control
        int            ViewportXOffset;  //
        int            ViewportYOffset;  //
        unsigned int   VxRepCount;       // If 0, draw normally, else, draw just vertex 0 n times.
        unsigned int   VxMaxUnique;      // If 0, draw normally, else draw no more than n different vx's
        string         FragProg;         // fragment shader program
        string         VtxProg;          // vertex shader program
        bool           AutoMipmap;       // generate mipmaps automatically (coherent mipmaps)
    };

    RC GetDefaultArgs (GLS_Args * pArgs);
    RC Allocate (const GLS_Args * pArgs);
    RC SendState ();
    RC Draw ();
    RC UnSendState ();
    RC Free ();

    GLStress();
    ~GLStress();

private:

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

    GLS_Args    m_Args;
    GLuint      m_Rows;
    GLuint      m_Cols;
    GLuint      m_CenterLeftCol;
    GLuint      m_CenterRightCol;
    GLuint      m_CenterBottomRow;
    GLuint      m_CenterTopRow;
    GLuint      m_VxPerPlane;
    GLuint      m_NumVertexes;
    GLuint      m_IdxPerPlane;
    GLuint      m_NumIndexes;
    GLuint      m_VxPerStrip;
    GLuint *    m_TextureObjectIds;
    GLuint      m_DL_Draw4Planes;
    GLuint      m_DL_Draw;
    vtx *       m_Vertexes;
    GLushort *  m_Indexes;
    GLuint      m_FragProgId;
    GLuint      m_VtxProgId;
    int         m_LwrTx;

    void Draw4Planes();
    void DrawPlane(int);
    void DrawQuadStrip(GLushort *, GLuint);
    RC   LoadTextures();
};

//------------------------------------------------------------------------------
GLStress::GLStress()
{
    m_Rows = 0;
    m_Cols = 0;
    m_CenterLeftCol = 0;
    m_CenterRightCol = 0;
    m_CenterBottomRow = 0;
    m_CenterTopRow = 0;
    m_VxPerPlane = 0;
    m_NumVertexes = 0;
    m_IdxPerPlane = 0;
    m_NumIndexes = 0;
    m_VxPerStrip = 0;
    m_TextureObjectIds = 0;
    m_DL_Draw4Planes = 0;
    m_DL_Draw = 0;
    m_Vertexes = 0;
    m_Indexes = 0;
    m_FragProgId = 0;
    m_VtxProgId = 0;
    m_LwrTx = 0;
}

//------------------------------------------------------------------------------
GLStress::~GLStress()
{
    Free();
}

//------------------------------------------------------------------------------
// GetDefaultArgs
//
RC GLStress::GetDefaultArgs(GLStress::GLS_Args * pArgs)
{
    pArgs->Width            = 1024;
    pArgs->Height           = 768;
    pArgs->Depth            = 32;
    pArgs->ZDepth           = 32;  // actually, 24d 8s
    pArgs->TxSize           = 32;
    pArgs->NumTextures      = 12;
    pArgs->TxD              = 2.25;
    pArgs->HighTess         = false;
    pArgs->Rotate           = false;
    pArgs->UseMM            = true;
    pArgs->TxGenQ           = true;
    pArgs->Use3dTextures    = false;
    pArgs->NumLights        = 3;
    pArgs->SpotLights       = true;
    pArgs->Normals          = false;
    pArgs->Stencil          = true;
    pArgs->Ztest            = true;
    pArgs->Stipple          = false;
    pArgs->TwoTex           = true;
    pArgs->Slow             = false;
    pArgs->Fog              = true;
    pArgs->SkipCenter       = false;
    pArgs->BoringXform      = false;
    pArgs->DrawElements     = true;
    pArgs->DrawPoints       = false;
    pArgs->SendW            = false;
    pArgs->PointSize        = 1.0;
    pArgs->ViewportXOffset  = 0;
    pArgs->ViewportYOffset  = 0;
    pArgs->VxRepCount       = 0;
    pArgs->VxMaxUnique      = 0;
    pArgs->FragProg         = "";
    pArgs->VtxProg          = "";
    pArgs->AutoMipmap       = false;
    return OK;
}

//------------------------------------------------------------------------------
RC GLStress::Allocate (const GLS_Args * pArgs)
{
    assert(pArgs);

    m_Args = *pArgs;
    m_LwrTx = 0;

    GLuint x, y, s, t;

    m_Cols = 0;
    for (x=0, s=1;  x < (GLuint)m_Args.Width;  x+=s, s++)
    {
        m_Cols++;
        if (m_Args.HighTess)
        {
            x++;
            m_Cols++;
        }
    }

    m_Rows = 0;
    for (y=0, t=1;  y < (GLuint) m_Args.Height;  y+=t, t++)
    {
        m_Rows++;
        if (m_Args.HighTess)
        {
            y++;
            m_Rows++;
        }
    }

    m_VxPerPlane  = (m_Rows+1) * (m_Cols+1);
    m_NumVertexes = m_VxPerPlane * 2;
    m_IdxPerPlane = m_Rows * 2 * (m_Cols + 1);

    if (m_Args.VxRepCount)
    {
        m_NumIndexes = m_Args.VxRepCount * m_NumVertexes;
        if ((m_Args.VxMaxUnique > 0) && (m_Args.VxMaxUnique < m_NumVertexes))
            m_NumIndexes = m_Args.VxRepCount * m_Args.VxMaxUnique;
    }
    else
    {
        m_NumIndexes  = m_IdxPerPlane * 2;
    }

    m_Vertexes = new vtx [m_NumVertexes];
    if (! m_Vertexes)
        return CANNOT_ALLOCATE_MEMORY;

    m_Indexes  = new GLushort [m_NumIndexes];
    if (! m_Indexes)
        return CANNOT_ALLOCATE_MEMORY;

    // Compile-time check for the extension.
#if defined GL_LW_fragment_program
    RC rc;

    if (m_Args.FragProg.size())
    {
        // Runtime check for the extension.
        if (mglExtensionSupported("GL_LW_fragment_program"))
        {
            const char * str = m_Args.FragProg.c_str();

            glGenProgramsLW(1, &m_FragProgId);
            glLoadProgramLW(
                           GL_FRAGMENT_PROGRAM_LW,
                           m_FragProgId,
                           (GLsizei) strlen(str),
                           (const GLubyte *)str);

            if (OK != (rc = mglGetRC()))
            {
                //
                // Print a report to the screen and logfile, showing where the syntax
                // error oclwrred in the program string.
                //
                GLint errOffset;
                const GLubyte * errMsg;

                glGetIntegerv(GL_PROGRAM_ERROR_POSITION_LW, &errOffset);
                errMsg = glGetString(GL_PROGRAM_ERROR_STRING_LW);

                Printf(PriHigh, "glLoadProgramLW err %s at %d ,\n",
                       (const char *)errMsg,
                       errOffset);
                return rc;
            }
        }
    }

    if (m_Args.VtxProg.size())
    {
        // Runtime extension check.
        if (mglExtensionSupported("GL_LW_vertex_program"))
        {
            const char * str = m_Args.VtxProg.c_str();

            glGenProgramsLW(1, &m_VtxProgId);
            glLoadProgramLW(
                           GL_VERTEX_PROGRAM_LW,
                           m_VtxProgId,
                           (GLsizei) strlen(str),
                           (const GLubyte *)str);

            if (OK != (rc = mglGetRC()))
            {
                //
                // Print a report to the screen and logfile, showing where the syntax
                // error oclwrred in the program string.
                //
                GLint errOffset;
                const GLubyte * errMsg;

                glGetIntegerv(GL_PROGRAM_ERROR_POSITION_LW, &errOffset);
                errMsg = glGetString(GL_PROGRAM_ERROR_STRING_LW);

                Printf(PriHigh, "glLoadProgramLW err %s at %d ,\n",
                       (const char *)errMsg,
                       errOffset);
                return rc;
            }
        }
    }
#endif

    // Fill in the vertex data:
    vtx * pvtxFar  = &m_Vertexes[0];
    vtx * pvtxNear = &m_Vertexes[m_VxPerPlane];

    GLuint row;
    GLuint col;
    for (y=0, t=0, row=0;  row <= m_Rows;  row++)
    {
        if (m_Args.HighTess && (row & 1))
        {
            y++;
        }
        else
        {
            y += t;
            t++;
        }
        if (y > (GLuint) m_Args.Height)
            y = m_Args.Height;

        for (x=0, s=0, col=0;  col <= m_Cols; col++)
        {
            if (m_Args.HighTess && (col & 1))
            {
                x++;
            }
            else
            {
                x += s;
                s++;
            }
            if (x > (GLuint) m_Args.Width)
                x = m_Args.Width;

            // For SkipCenter mode, we will avoid drawing quads that cover the
            // center 10% of the screen.
            if (m_Args.SkipCenter)
            {
                if ((0 == m_CenterLeftCol) &&  (x+s >= (GLuint)(m_Args.Width * 0.45)))
                    m_CenterLeftCol = col;
                if ((0 == m_CenterRightCol) && (x >= (GLuint)(m_Args.Width * 0.55)))
                    m_CenterRightCol = col;
                if ((0 == m_CenterBottomRow) && (y+t >= (GLuint)(m_Args.Height * 0.45)))
                    m_CenterBottomRow = row;
                if ((0 == m_CenterTopRow) && (y >= (GLuint)(m_Args.Height * 0.55)))
                    m_CenterTopRow = row;
            }

            // Normal points towards camera (& light) at center,
            // 45% away from camera at center of each edge.
            // Lighting will make center 100% brightness, dimming towards corners.
            GLfloat fnorm[3];
            fnorm[0] = x / (float)m_Args.Width - 0.5f;
            fnorm[1] = y / (float)m_Args.Height - 0.5f;
            fnorm[2] = sqrt(1.0f - fnorm[0]*fnorm[0] - fnorm[1]*fnorm[1]);

            // Z == near view plane at edges, far view plane at center.
            // But, offset far plane a bit.
            float xDist = 1.0f - fabs((x - m_Args.Width/2.0f)  / (m_Args.Width/2.0f));
            float yDist = 1.0f - fabs((y - m_Args.Height/2.0f) / (m_Args.Height/2.0f));

            GLshort z;
            if (xDist > yDist)
                z = 100 - (GLshort)(yDist * 25);
            else
                z = 100 - (GLshort)(xDist * 25);

            if (m_Args.BoringXform)
            {
                pvtxFar->f4.pos[0] = x / (float)(m_Args.Width/2) - 1.0f;
                pvtxFar->f4.pos[1] = y / (float)(m_Args.Height/2) - 1.0f;
                pvtxFar->f4.pos[2] = 0.0f;
                pvtxFar->f4.pos[3] = 1.0f;

                pvtxNear->f4.pos[0] = pvtxFar->f4.pos[0];
                pvtxNear->f4.pos[1] = pvtxFar->f4.pos[1];
                pvtxNear->f4.pos[2] = 0.1f;
                pvtxNear->f4.pos[3] = 1.0f;
            }
            else
            {
                pvtxFar->s4s3.pos[0] = x;
                pvtxFar->s4s3.pos[1] = y;
                pvtxFar->s4s3.pos[2] = z - 3;
                pvtxFar->s4s3.pos[3] = 1;

                pvtxFar->s4s3.norm[0] = (GLshort)(127.5 * fnorm[0]);
                pvtxFar->s4s3.norm[1] = (GLshort)(127.5 * fnorm[1]);
                pvtxFar->s4s3.norm[2] = (GLshort)(127.5 * fnorm[2]);

                pvtxNear->s4s3.pos[0] = m_Args.Width - x;
                pvtxNear->s4s3.pos[1] = m_Args.Height - y;
                pvtxNear->s4s3.pos[2] = z;
                pvtxNear->s4s3.pos[3] = 1;

                pvtxNear->s4s3.norm[0] = -pvtxFar->s4s3.norm[0];
                pvtxNear->s4s3.norm[1] = -pvtxFar->s4s3.norm[1];
                pvtxNear->s4s3.norm[2] = pvtxFar->s4s3.norm[2];
            }

            pvtxFar++;
            pvtxNear++;
        }
    }
    FlushMem(m_Vertexes, (void*)((size_t)pvtxFar - 1));
    FlushMem(m_Vertexes + m_VxPerPlane, (void*)((size_t)pvtxNear - 1));

    // Fill in the indexes, both planes:
    GLushort * pidxFar  = m_Indexes;
    GLushort * pidxNear = m_Indexes + m_IdxPerPlane;

    if (m_Args.VxRepCount)
    {
        GLuint numVx = m_NumVertexes;
        if ((m_Args.VxMaxUnique > 0) && (m_Args.VxMaxUnique < m_NumVertexes))
            numVx = m_Args.VxMaxUnique;

        for (t = 0; t < numVx; t++)
        {
            for (s = 0; s < m_Args.VxRepCount; s++)
            {
                m_Indexes[t*m_Args.VxRepCount + s] = t;
            }
        }
    }
    else
    {
        for (t = 0; t < m_Rows; t++)
        {
            pidxFar[0]  = (t+1)*(m_Cols+1);
            pidxFar[1]  = (t)  *(m_Cols+1);
            pidxNear[0] = m_VxPerPlane + (t)  *(m_Cols+1);
            pidxNear[1] = m_VxPerPlane + (t+1)*(m_Cols+1);
            pidxFar += 2;
            pidxNear += 2;

            for (s = 1; s <= m_Cols; s++)
            {
                pidxFar[0] = pidxFar[-2] + 1;
                pidxFar[1] = pidxFar[-1] + 1;
                pidxNear[0] = pidxNear[-2] + 1;
                pidxNear[1] = pidxNear[-1] + 1;
                pidxFar += 2;
                pidxNear += 2;
            }
        }
    }

    // Load some textures.
    if (m_Args.NumTextures)
    {
        RC rc = LoadTextures();
        if (OK != rc)
            return rc;
    }

    m_DL_Draw4Planes = 0;
    m_DL_Draw        = 0;

    return OK;
}

//------------------------------------------------------------------------------
RC GLStress::LoadTextures ()
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glActiveTexture(GL_TEXTURE0_ARB);

    RC rc = mglGetRC();
    if (OK != rc)
        return rc;

    m_TextureObjectIds = new GLuint [m_Args.NumTextures];
    glGenTextures(m_Args.NumTextures, m_TextureObjectIds);

    GLuint numTexels = m_Args.TxSize * m_Args.TxSize;
    GLenum txDim     = GL_TEXTURE_2D;

    if (m_Args.Use3dTextures)
    {
        numTexels *= m_Args.TxSize;
        txDim      = GL_TEXTURE_3D;
    }

    GLuint * buf = new GLuint [ numTexels ];
    if (!buf)
        return CANNOT_ALLOCATE_MEMORY;

    GLint txIdx;
    for (txIdx = 0; txIdx < m_Args.NumTextures; txIdx++)
    {
        GLuint * pFill    = buf;
        GLuint * pFillEnd = buf + numTexels;

        const GLuint * pPattern = s_Patterns[txIdx % m_NumPatterns];

        if (! pPattern)
        {
            while (pFill < pFillEnd)
                *pFill++ = GetRandom();
        }
        else
        {
            const GLuint * pPat = pPattern;
            while (pFill < pFillEnd)
            {
                *pFill++ = *pPat++;

                if (END_PATTERN == *pPat)
                    pPat = pPattern;
            }
        }
        glBindTexture(txDim, m_TextureObjectIds[txIdx]);

        bool mmap = m_Args.UseMM && (0 == txIdx % 2);
        if (mmap)
        {
            glTexParameteri(txDim, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(txDim, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        else
        {
            glTexParameteri(txDim, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(txDim, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
        if (m_Args.AutoMipmap)
            glTexParameteri(txDim, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
        else
            glTexParameteri(txDim, GL_GENERATE_MIPMAP_SGIS, GL_FALSE);

        GLuint level;
        GLuint sz;
        for (level = 0, sz = m_Args.TxSize;  sz;  level++, sz >>= 1)
        {
            if ((level > 0) && ((!mmap) || m_Args.AutoMipmap))
                break;

            if (m_Args.Use3dTextures)
            {
                glTexImage3D(
                            GL_TEXTURE_3D,
                            level,
                            GL_RGBA8,
                            sz,
                            sz,
                            sz,
                            0,
                            GL_RGBA,
                            GL_UNSIGNED_BYTE,
                            buf );
            }
            else
            {
                glTexImage2D(
                            GL_TEXTURE_2D,
                            level,
                            GL_RGBA8,
                            sz,
                            sz,
                            0,
                            GL_RGBA,
                            GL_UNSIGNED_BYTE,
                            buf );
            }
            RC rc = mglGetRC();
            if (OK != rc)
                return rc;
        }
    }

    delete [] buf;
    return OK;
}

//------------------------------------------------------------------------------
RC GLStress::Free ()
{
    glDeleteLists(m_DL_Draw,        1);
    glDeleteLists(m_DL_Draw4Planes, 1);

    if (m_Args.NumTextures)
    {
        glDeleteTextures(m_Args.NumTextures, m_TextureObjectIds);
        if (m_TextureObjectIds)
            delete [] m_TextureObjectIds;
    }

    if (m_Vertexes)
    {
        delete [] m_Vertexes;
    }
    if (m_Indexes)
        delete [] m_Indexes;

    return OK;
}

//------------------------------------------------------------------------------
RC GLStress::SendState ()
{
    RC rc;

#if defined(GL_LW_fragment_program)
    if (m_FragProgId)
    {
        glEnable(GL_FRAGMENT_PROGRAM_LW);
        glBindProgramLW(GL_FRAGMENT_PROGRAM_LW, m_FragProgId);
    }
    if (m_VtxProgId)
    {
        glEnable(GL_VERTEX_PROGRAM_LW);
        glBindProgramLW(GL_VERTEX_PROGRAM_LW, m_VtxProgId);
        glTrackMatrixLW(GL_VERTEX_PROGRAM_LW, 0, GL_MODELVIEW_PROJECTION_LW, GL_IDENTITY_LW);
        glTrackMatrixLW(GL_VERTEX_PROGRAM_LW, 4, GL_MODELVIEW,               GL_ILWERSE_TRANSPOSE_LW);
        if (m_Args.PointSize == 0.0)
            glEnable(GL_VERTEX_PROGRAM_POINT_SIZE_LW);
        glProgramParameter4fLW (
                               GL_VERTEX_PROGRAM_LW,
                               20,
                               0.5, 4.0, 64.0, 1.0
                               );
    }
#endif

    if (0.0 != m_Args.PointSize)
        glPointSize((GLfloat)m_Args.PointSize);

    GLfloat x_to_s[4] = {0.0, 0.0, 0.0, 0.0};
    GLfloat y_to_t[4] = {0.0, 0.0, 0.0, 0.0};
    if (m_Args.TxSize)
    {
        x_to_s[0] = (GLfloat)(pow(2.0, m_Args.TxD) / m_Args.TxSize);
        y_to_t[1] = (GLfloat)(pow(2.0, m_Args.TxD) / m_Args.TxSize);
    }

    GLfloat z_to_r[4] = {0.0, 0.0, 0.0, 1.0};
    z_to_r[2] = 1.0f / 200.0f;

    GLfloat xy_to_q[4] = {0.0, 0.0, 0.0, 1.0};
    xy_to_q[0] =  0.9f/m_Args.Width;
    xy_to_q[1] =  0.9f/m_Args.Height;

    // set up texturing
    if (m_Args.NumTextures)
    {
        GLenum txDim = GL_TEXTURE_2D;
        if (m_Args.Use3dTextures)
            txDim     = GL_TEXTURE_3D;

        if (m_Args.TwoTex)
        {
            glActiveTexture(GL_TEXTURE1_ARB);
            glEnable(txDim);
            glBindTexture(txDim, m_TextureObjectIds[0]);
            glEnable(GL_TEXTURE_GEN_S);
            glEnable(GL_TEXTURE_GEN_T);
            if (m_Args.Use3dTextures)
                glEnable(GL_TEXTURE_GEN_R);
            if (m_Args.TxGenQ)
                glEnable(GL_TEXTURE_GEN_Q);
            glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
            glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
            glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
            glTexGeni(GL_Q, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
            glTexGenfv(GL_S, GL_OBJECT_PLANE, x_to_s);
            glTexGenfv(GL_T, GL_OBJECT_PLANE, y_to_t);
            glTexGenfv(GL_R, GL_OBJECT_PLANE, z_to_r);
            glTexGenfv(GL_Q, GL_OBJECT_PLANE, xy_to_q);

            glTexElwi(GL_TEXTURE_ELW, GL_TEXTURE_ELW_MODE, GL_DECAL);

            CHECK_RC(mglGetRC());
        }
        glActiveTexture(GL_TEXTURE0_ARB);
        glEnable(txDim);
        glEnable(GL_TEXTURE_GEN_S);
        glEnable(GL_TEXTURE_GEN_T);
        if (m_Args.Use3dTextures)
            glEnable(GL_TEXTURE_GEN_R);
        if (m_Args.TxGenQ)
            glEnable(GL_TEXTURE_GEN_Q);
        glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
        glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
        glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
        glTexGeni(GL_Q, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
        glTexGenfv(GL_S, GL_OBJECT_PLANE, x_to_s);
        glTexGenfv(GL_T, GL_OBJECT_PLANE, y_to_t);
        glTexGenfv(GL_R, GL_OBJECT_PLANE, z_to_r);
        glTexGenfv(GL_Q, GL_OBJECT_PLANE, xy_to_q);
        CHECK_RC(mglGetRC());
    }

    // Set up projection
    if (m_Args.BoringXform)
    {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
    }
    else
    {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glFrustum( m_Args.Width/-2.0,  m_Args.Width/2.0,
                   m_Args.Height/-2.0, m_Args.Height/2.0,
                   100, 300 );

        if (m_Args.Rotate)
            glRotatef(-180.0, 0.0, 0.0, 1.0);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glTranslated(m_Args.Width/-2.0, m_Args.Height/-2.0, -200);
    }

    CHECK_RC(mglGetRC());

    // Set up viewport.
    GLint x = 0;
    GLint y = 0;
    GLsizei w = m_Args.Width;
    GLsizei h = m_Args.Height;

    if (m_Args.ViewportXOffset > 0)
    {
        x = m_Args.ViewportXOffset;
        w -= m_Args.ViewportXOffset;
    }
    else
    {
        w += m_Args.ViewportXOffset;
    }

    if (m_Args.ViewportYOffset > 0)
    {
        y = m_Args.ViewportYOffset;
        h -= m_Args.ViewportYOffset;
    }
    else
    {
        h += m_Args.ViewportYOffset;
    }
    glViewport (x, y, w, h);

    // Clear the screen.
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    CHECK_RC(mglGetRC());

    if (m_Args.NumLights)
    {
        // Lighting:
        glEnable(GL_LIGHTING);

        GLfloat white[4] = { 1.0, 1.0, 1.0, 1.0};
        GLfloat gray[4]  = { 0.8F, 0.8F, 0.8F, 0.8F};
        GLfloat cols[8*4] =
        { 1.0F, 1.0F, 1.0F, 1.0F,
            1.0F, 0.3F, 0.3F, 1.0F,
            0.3F, 1.0F, 0.3F, 1.0F,
            0.3F, 0.3F, 1.0F, 1.0F,
            1.0F, 1.0f, 0.3F, 1.0F,
            1.0F, 0.3F, 1.0F, 1.0F,
            0.3F, 1.0F, 1.0F, 1.0F,
            1.0F, 0.3F, 0.3F, 1.0F
        };
        GLfloat locs[8*4] =
        { m_Args.Width*3/6.0f, m_Args.Height*5/6.0f,  95.0f, 1.0f,
            m_Args.Width*1/6.0f, m_Args.Height*1/6.0f,  95.0f, 1.0f,
            m_Args.Width*5/6.0f, m_Args.Height*1/6.0f,  95.0f, 1.0f,
            m_Args.Width*3/6.0f, m_Args.Height*3/6.0f,  95.0f, 1.0f,
            m_Args.Width*5/6.0f, m_Args.Height*5/6.0f,  95.0f, 1.0f,
            m_Args.Width*1/6.0f, m_Args.Height*5/6.0f,  95.0f, 1.0f,
            m_Args.Width*3/6.0f, m_Args.Height*1/6.0f,  95.0f, 1.0f,
            m_Args.Width*2/6.0f, m_Args.Height*3/6.0f,  95.0f, 1.0f
        };

        GLuint L;
        for (L = 0; L < m_Args.NumLights; L++)
        {
            glEnable(GL_LIGHT0+L);
            glLightfv(GL_LIGHT0+L, GL_POSITION, locs + L*4);
            glLightf (GL_LIGHT0+L, GL_LINEAR_ATTENUATION, 0.001F);
            glLightfv(GL_LIGHT0+L, GL_DIFFUSE, cols + L*4);

            if (m_Args.SpotLights)
                glLightf (GL_LIGHT0+L, GL_SPOT_EXPONENT, 25);

            if (L == 0)
                glLightfv(GL_LIGHT0, GL_AMBIENT, gray);
        }

        glMaterialfv(GL_FRONT, GL_AMBIENT, white );
        CHECK_RC(mglGetRC());
    }

    // Set up the vertex array pointers.
    if (m_Args.DrawElements)
    {
        glEnableClientState(GL_VERTEX_ARRAY);
        if (m_Args.BoringXform)
        {
            if (m_Args.SendW)
                glVertexPointer( 4, GL_FLOAT, sizeof(vtx), &m_Vertexes[0].f4.pos[0]);
            else
                glVertexPointer( 3, GL_FLOAT, sizeof(vtx), &m_Vertexes[0].f4.pos[0]);
        }
        else
        {
            if (m_Args.SendW)
                glVertexPointer( 4, GL_SHORT, sizeof(vtx), &m_Vertexes[0].s4s3.pos[0]);
            else
                glVertexPointer( 3, GL_SHORT, sizeof(vtx), &m_Vertexes[0].s4s3.pos[0]);

            if (m_Args.Normals)
            {
                glEnableClientState(GL_NORMAL_ARRAY);
                glNormalPointer( GL_SHORT, sizeof(vtx), &m_Vertexes[0].s4s3.norm[0]);
            }
        }
        CHECK_RC(mglGetRC());
    }

    // Set up fragment operations:
    if (m_Args.Ztest)
        glEnable(GL_DEPTH_TEST);
    glEnable(GL_COLOR_LOGIC_OP);
    glLogicOp(GL_XOR);
    if (m_Args.Stencil)
        glEnable(GL_STENCIL_TEST);

    if (m_Args.Stipple)
    {
        GLubyte stipple[32*32/8] =
        {
            0x7f, 0xff, 0xff, 0xff,
            0xbf, 0xff, 0xff, 0xff,
            0xdf, 0xff, 0xff, 0xff,
            0xef, 0xff, 0xff, 0xff,
            0xf7, 0xff, 0xff, 0xff,
            0xfb, 0xff, 0xff, 0xff,
            0xfd, 0xff, 0xff, 0xff,
            0xfe, 0xff, 0xff, 0xff,

            0xff, 0x7f, 0xff, 0xff,
            0xff, 0xbf, 0xff, 0xff,
            0xff, 0xdf, 0xff, 0xff,
            0xff, 0xef, 0xff, 0xff,
            0xff, 0xf7, 0xff, 0xff,
            0xff, 0xfb, 0xff, 0xff,
            0xff, 0xfd, 0xff, 0xff,
            0xff, 0xfe, 0xff, 0xff,

            0xff, 0xff, 0x7f, 0xff,
            0xff, 0xff, 0xbf, 0xff,
            0xff, 0xff, 0xdf, 0xff,
            0xff, 0xff, 0xef, 0xff,
            0xff, 0xff, 0xf7, 0xff,
            0xff, 0xff, 0xfb, 0xff,
            0xff, 0xff, 0xfd, 0xff,
            0xff, 0xff, 0xfe, 0xff,

            0xff, 0xff, 0xff, 0x7f,
            0xff, 0xff, 0xff, 0xbf,
            0xff, 0xff, 0xff, 0xdf,
            0xff, 0xff, 0xff, 0xef,
            0xff, 0xff, 0xff, 0xf7,
            0xff, 0xff, 0xff, 0xfb,
            0xff, 0xff, 0xff, 0xfd,
            0xff, 0xff, 0xff, 0xfe
        };
        glEnable(GL_POLYGON_STIPPLE);
        glPolygonStipple(stipple);
    }

    if (m_Args.Fog)
    {
        GLfloat fogcolor[4] = { 1.0f, 1.0f, 0.8f, 0.8f};

        glEnable(GL_FOG);
        glFogi(GL_FOG_MODE, GL_EXP2);
        glFogf(GL_FOG_DENSITY, 1.0f/(2.0f*200));
        glFogi(GL_FOG_DISTANCE_MODE_LW, GL_EYE_RADIAL_LW);
        //glFogi(GL_FOG_MODE, GL_LINEAR);
        //glFogf(GL_FOG_START, 100.0f);
        //glFogf(GL_FOG_END, 150.0f);
        glFogfv(GL_FOG_COLOR, fogcolor);
    }

    return mglGetRC();
}

//------------------------------------------------------------------------------
RC GLStress::UnSendState ()
{
    // Texture
    if (m_Args.NumTextures)
    {
        glActiveTexture(GL_TEXTURE0_ARB);
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_GEN_S);
        glDisable(GL_TEXTURE_GEN_T);
        glDisable(GL_TEXTURE_GEN_R);
        glDisable(GL_TEXTURE_GEN_Q);
    }

    // Matrix
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    // Fragment
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_COLOR_LOGIC_OP);

    // Tear down the vertex array pointers.
    if (m_Args.DrawElements)
    {
        glDisableClientState(GL_VERTEX_ARRAY);
        if (m_Args.Normals)
            glDisableClientState(GL_NORMAL_ARRAY);
    }

    if (m_Args.NumLights)
    {
        glDisable(GL_LIGHTING);
        GLuint L;
        for (L = 0; L < m_Args.NumLights; L++)
            glDisable(GL_LIGHT0+L);
    }
    glDisable(GL_STENCIL);

    if (m_Args.Fog)
    {
        glDisable(GL_FOG);
    }

#if defined(GL_LW_fragment_program)
    if (m_FragProgId)
    {
        glDisable(GL_FRAGMENT_PROGRAM_LW);
    }
    if (m_VtxProgId)
    {
        glDisable(GL_VERTEX_PROGRAM_LW);
    }
#endif

    return mglGetRC();
}

//------------------------------------------------------------------------------
void GLStress::DrawQuadStrip(GLushort * idxs, GLuint num)
{
    GLenum prim;

    if (m_Args.DrawPoints)
        prim = GL_POINTS;
    else
        prim = GL_QUAD_STRIP;

    if (m_Args.DrawElements)
    {
        glDrawElements(prim, num, GL_UNSIGNED_SHORT, idxs);
    }
    else
    {
        GLuint i;
        glBegin(prim);
        for (i = 0; i < num; i++)
        {
            vtx * pv = &m_Vertexes[ idxs[i] ];
            if (m_Args.BoringXform)
            {
                if (m_Args.SendW)
                    glVertex4fv(&pv->f4.pos[0]);
                else
                    glVertex3fv(&pv->f4.pos[0]);
            }
            else
            {
                if (m_Args.SendW)
                    glVertex4sv(&pv->s4s3.pos[0]);
                else
                    glVertex3sv(&pv->s4s3.pos[0]);

                if (m_Args.Normals)
                {
                    glNormal3sv(&pv->s4s3.norm[0]);
                }
            }
        }
        glEnd();
    }
}

//------------------------------------------------------------------------------
void GLStress::DrawPlane (int plane)
{
    GLuint startIdx = plane * m_Rows * (m_Cols+1) * 2;

    // debug, one quad at a time: for (row = 0; row < m_Rows; row++)
    // debug, one quad at a time: {
    // debug, one quad at a time:    GLuint rowStartIdx = startIdx + row * (m_Cols+1) * 2;
    // debug, one quad at a time:    GLuint col;
    // debug, one quad at a time:    for (col = 0; col < m_Cols; col++)
    // debug, one quad at a time:    {
    // debug, one quad at a time:       glBegin(GL_QUADS);
    // debug, one quad at a time:       glArrayElement( m_Indexes[ rowStartIdx + col * 2 + 0 ] );
    // debug, one quad at a time:       glArrayElement( m_Indexes[ rowStartIdx + col * 2 + 1 ] );
    // debug, one quad at a time:       glArrayElement( m_Indexes[ rowStartIdx + col * 2 + 3 ] );
    // debug, one quad at a time:       glArrayElement( m_Indexes[ rowStartIdx + col * 2 + 2 ] );
    // debug, one quad at a time:       glEnd();
    // debug, one quad at a time:       glFinish(); // breakpoint here in debugger!
    // debug, one quad at a time:       Tasker::SleepMs(10);
    // debug, one quad at a time:    }
    // debug, one quad at a time: }

    if (m_Args.VxRepCount)
    {
        GLuint numVx = m_NumVertexes;
        if ((m_Args.VxMaxUnique > 0) && (m_Args.VxMaxUnique < m_NumVertexes))
            numVx = m_Args.VxMaxUnique;

        GLuint v;
        for (v = 0; v < numVx; v++)
        {
            DrawQuadStrip(&(m_Indexes[v * m_Args.VxRepCount]),
                          m_Args.VxRepCount
                         );
        }
    }
    else
    {
        GLuint row;
        for (row = 0; row < m_Rows; row++)
        {
            if (m_Args.SkipCenter &&
                (row >= m_CenterBottomRow) &&
                (row <= m_CenterTopRow))
            {
                DrawQuadStrip(
                             m_Indexes + startIdx + (row * (m_Cols+1) * 2),
                             (m_CenterLeftCol + 1)*2
                             );
                DrawQuadStrip(
                             m_Indexes + startIdx + (row * (m_Cols+1) * 2) +
                             (m_CenterRightCol + 1) * 2,
                             (m_Cols - m_CenterRightCol)*2
                             );
            }
            else
            {
                DrawQuadStrip(
                             m_Indexes + startIdx + row * (m_Cols+1) * 2,
                             (m_Cols+1) * 2
                             );
            }
            if (m_Args.Slow)
            {
                glFinish(); // breakpoint here in debugger!
                SleepMs(5);
            }
        }
    }
}

//------------------------------------------------------------------------------
void GLStress::Draw4Planes ()
{
    // If we have recorded a display-list, call it.

    if (m_DL_Draw4Planes)
    {
        glCallList(m_DL_Draw4Planes);
        return;
    }

    // We don't have a display-list.  Should we record one?

    GLuint listId = 0;
    if (!m_Args.Slow)
    {
        listId = glGenLists(1);
        glNewList(listId, GL_COMPILE_AND_EXELWTE);
    }

    // Xor-draw 4 times, which should return us to the original frame-buffer.

    if (m_Args.Ztest)
        glDepthFunc(GL_LEQUAL);
    if (m_Args.Stencil)
    {
        glStencilFunc(GL_EQUAL, 0, 0xff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
    }
    if (m_Args.NumTextures)
        glDisable(GL_TEXTURE_GEN_Q);
    DrawPlane(0);  // far plane

    if (m_Args.Stencil)
    {
        glStencilFunc(GL_EQUAL, 1, 0xff);
    }
    if (m_Args.NumTextures && m_Args.TxGenQ)
    {
        glEnable(GL_TEXTURE_GEN_Q);
    }
    DrawPlane(1);  // near plane
    if (m_Args.Stencil)
    {
        glStencilFunc(GL_EQUAL, 2, 0xff);
    }
    DrawPlane(1);  // near plane

    if (m_Args.Ztest)
        glDepthFunc(GL_GREATER);
    if (m_Args.Stencil)
    {
        glStencilFunc(GL_EQUAL, 3, 0xff);
        glStencilOp(GL_INCR, GL_INCR, GL_ZERO);
    }
    if (m_Args.NumTextures && m_Args.TxGenQ)
    {
        glDisable(GL_TEXTURE_GEN_Q);
    }
    DrawPlane(0);  // far plane

    if (listId)
    {
        glEndList();
        m_DL_Draw4Planes = listId;
    }
}

//------------------------------------------------------------------------------
RC GLStress::Draw ()
{
    if (m_Args.NumTextures)
    {
        // Bind a texture.
        m_LwrTx++;
        if (m_LwrTx >= m_Args.NumTextures)
            m_LwrTx = 0;

        GLenum txDim = GL_TEXTURE_2D;
        if (m_Args.Use3dTextures)
            txDim    = GL_TEXTURE_3D;

        glBindTexture(txDim, m_TextureObjectIds[m_LwrTx]);
    }
    Draw4Planes();
    glFlush();
    return mglGetRC();
}

//------------------------------------------------------------------------------
int main (int argc, const char ** argv)
{
    RC rc;
    GLStress gls;
    GLStress::GLS_Args args;

    gls->GetDefaultArgs(&args);

    // Tinker with rendering args here, to try different options...

    // OS-dependant code here:
    //  - set mode or create window
    //  - set up GL context
    //  - set args.Width, args.Height, and args.Depth to match the window
    //  - query for available GL extensions
    //  - if available, get extension function pointers
    //    for glGenProgramsLW, glLoadProgramLW, glBindProgramLW,
    //    glTrackMatrixLW, and glProgramParameter4fLW

    CHECK_RC( gls.Allocate(&args) );
    CHECK_RC( gls.SendState() );

    int i;
    for (i = 0; i < 1000; i++)
    {
        if (OK != (rc = gls.Draw()))
            break;
    }
    gls.UnSendState();
    gls.Free();
    return rc;
}
