/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#include "core/include/tee.h"
#include "core/include/massert.h"
#include "core/include/patterns.h"
#include "random.h"
#include "core/include/tasker.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "lwtypes.h"
#include <ctype.h>

// --------------------------------------------------------------------
// Data Patterns for use by tests. Just declare them once to save space
//  Also some pattern related utility functions
// --------------------------------------------------------------------

namespace Pattern
{
   UINT32 pA[] =
   {
      0xAAAAAAAA, 0xAAAAAAAA,
      0xAAAAAAAA, 0xAAAAAAAA,
      0xAAAAAAAA, 0xAAAAAAAA,
      0xAAAAAAAA, 0xAAAAAAAA,
      0xAAAAAAAA, 0xAAAAAAAA,
      0xAAAAAAAA, 0xAAAAAAAA,
      0xAAAAAAAA, 0xAAAAAAAA,
      0xAAAAAAAA, 0xAAAAAAAA,
   };

   DataPattern patA =
   {
      sizeof(pA),
      pA
   };

   UINT32 p0[] =
   {
      0x00000000, 0x00000000,
      0x00000000, 0x00000000,
      0x00000000, 0x00000000,
      0x00000000, 0x00000000,
      0x00000000, 0x00000000,
      0x00000000, 0x00000000,
      0x00000000, 0x00000000,
      0x00000000, 0x00000000,
   };

   DataPattern pat0 =
   {
      sizeof(p0),
      p0
   };

   UINT32 pWalking1[] =
   {
      0x00000001, 0x00000001, 0xFFFFFFFE, 0xFFFFFFFE,
      0x00000002, 0x00000002, 0xFFFFFFFD, 0xFFFFFFFD,
      0x00000004, 0x00000004, 0xFFFFFFFB, 0xFFFFFFFB,
      0x00000008, 0x00000008, 0xFFFFFFF7, 0xFFFFFFF7,
      0x00000010, 0x00000010, 0xFFFFFFEF, 0xFFFFFFEF,
      0x00000020, 0x00000020, 0xFFFFFFDF, 0xFFFFFFDF,
      0x00000040, 0x00000040, 0xFFFFFFBF, 0xFFFFFFBF,
      0x00000080, 0x00000080, 0xFFFFFF7F, 0xFFFFFF7F,
      0x00000100, 0x00000100, 0xFFFFFEFF, 0xFFFFFEFF,
      0x00000200, 0x00000200, 0xFFFFFDFF, 0xFFFFFDFF,
      0x00000400, 0x00000400, 0xFFFFFBFF, 0xFFFFFBFF,
      0x00000800, 0x00000800, 0xFFFFF7FF, 0xFFFFF7FF,
      0x00001000, 0x00001000, 0xFFFFEFFF, 0xFFFFEFFF,
      0x00002000, 0x00002000, 0xFFFFDFFF, 0xFFFFDFFF,
      0x00004000, 0x00004000, 0xFFFFBFFF, 0xFFFFBFFF,
      0x00008000, 0x00008000, 0xFFFF7FFF, 0xFFFF7FFF,
      0x00010000, 0x00010000, 0xFFFEFFFF, 0xFFFEFFFF,
      0x00020000, 0x00020000, 0xFFFDFFFF, 0xFFFDFFFF,
      0x00040000, 0x00040000, 0xFFFBFFFF, 0xFFFBFFFF,
      0x00080000, 0x00080000, 0xFFF7FFFF, 0xFFF7FFFF,
      0x00100000, 0x00100000, 0xFFEFFFFF, 0xFFEFFFFF,
      0x00200000, 0x00200000, 0xFFDFFFFF, 0xFFDFFFFF,
      0x00400000, 0x00400000, 0xFFBFFFFF, 0xFFBFFFFF,
      0x00800000, 0x00800000, 0xFF7FFFFF, 0xFF7FFFFF,
      0x01000000, 0x01000000, 0xFEFFFFFF, 0xFEFFFFFF,
      0x02000000, 0x02000000, 0xFDFFFFFF, 0xFDFFFFFF,
      0x04000000, 0x04000000, 0xFBFFFFFF, 0xFBFFFFFF,
      0x08000000, 0x08000000, 0xF7FFFFFF, 0xF7FFFFFF,
      0x10000000, 0x10000000, 0xEFFFFFFF, 0xEFFFFFFF,
      0x20000000, 0x20000000, 0xDFFFFFFF, 0xDFFFFFFF,
      0x40000000, 0x40000000, 0xBFFFFFFF, 0xBFFFFFFF,
      0x80000000, 0x80000000, 0xEFFFFFFF, 0xEFFFFFFF,
   };

   DataPattern patWalking1 =
   {
      sizeof(pWalking1),
      pWalking1
   };

   UINT32 p01[] =
   {
      0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
   };

   DataPattern pat01 =
   {
      sizeof(p01),
      p01
   };

//32 x 64B
   UINT32 pTripleWhammy[] =
   {
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0x00000001, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFE, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000002, 0x00000002, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFD, 0xFFFFFFFD, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000004, 0x00000004, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFB, 0xFFFFFFFB, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000008, 0x00000008, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFF7, 0xFFFFFFF7, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000010, 0x00000010, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFEF, 0xFFFFFFEF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000020, 0x00000020, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFDF, 0xFFFFFFDF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000040, 0x00000040, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFBF, 0xFFFFFFBF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000080, 0x00000080, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFF7F, 0xFFFFFF7F, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000100, 0x00000100, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFEFF, 0xFFFFFEFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000200, 0x00000200, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFDFF, 0xFFFFFDFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000400, 0x00000400, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFBFF, 0xFFFFFBFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000800, 0x00000800, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFF7FF, 0xFFFFF7FF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00001000, 0x00001000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFEFFF, 0xFFFFEFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00002000, 0x00002000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFDFFF, 0xFFFFDFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00004000, 0x00004000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFBFFF, 0xFFFFBFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00008000, 0x00008000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF7FFF, 0xFFFF7FFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00010000, 0x00010000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFEFFFF, 0xFFFEFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00020000, 0x00020000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFDFFFF, 0xFFFDFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00040000, 0x00040000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFBFFFF, 0xFFFBFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00080000, 0x00080000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFF7FFFF, 0xFFF7FFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00100000, 0x00100000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFEFFFFF, 0xFFEFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00200000, 0x00200000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFDFFFFF, 0xFFDFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00400000, 0x00400000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFBFFFFF, 0xFFBFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00800000, 0x00800000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFF7FFFFF, 0xFF7FFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x01000000, 0x01000000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFEFFFFFF, 0xFEFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x02000000, 0x02000000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFDFFFFFF, 0xFDFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x04000000, 0x04000000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFBFFFFFF, 0xFBFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x08000000, 0x08000000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xF7FFFFFF, 0xF7FFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x10000000, 0x10000000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xEFFFFFFF, 0xEFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x20000000, 0x20000000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xDFFFFFFF, 0xDFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x40000000, 0x40000000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xBFFFFFFF, 0xBFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x80000000, 0x80000000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
   };
   DataPattern patTripleWhammy =
   {
      sizeof(pTripleWhammy),
      pTripleWhammy
   };

   UINT32 pIsolatedOnes[] =
   {
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0x00000001,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000002, 0x00000002,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000004, 0x00000004,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000008, 0x00000008,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000010, 0x00000010,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000020, 0x00000020,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000040, 0x00000040,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000080, 0x00000080,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000100, 0x00000100,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000200, 0x00000200,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000400, 0x00000400,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000800, 0x00000800,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00001000, 0x00001000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00002000, 0x00002000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00004000, 0x00004000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00008000, 0x00008000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00010000, 0x00010000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00020000, 0x00020000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00040000, 0x00040000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00080000, 0x00080000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00100000, 0x00100000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00200000, 0x00200000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00400000, 0x00400000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00800000, 0x00800000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x01000000, 0x01000000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x02000000, 0x02000000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x04000000, 0x04000000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x08000000, 0x08000000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x10000000, 0x10000000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x20000000, 0x20000000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x40000000, 0x40000000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x80000000, 0x80000000,
   };
   DataPattern patIsolatedOnes =
   {
      sizeof(pIsolatedOnes),
      pIsolatedOnes
   };

   UINT32 pIsolatedZeros[] =
   {
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFE,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFD, 0xFFFFFFFD,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFB, 0xFFFFFFFB,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFF7, 0xFFFFFFF7,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFEF, 0xFFFFFFEF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFDF, 0xFFFFFFDF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFBF, 0xFFFFFFBF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFF7F, 0xFFFFFF7F,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFEFF, 0xFFFFFEFF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFDFF, 0xFFFFFDFF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFBFF, 0xFFFFFBFF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFF7FF, 0xFFFFF7FF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFEFFF, 0xFFFFEFFF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFDFFF, 0xFFFFDFFF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFBFFF, 0xFFFFBFFF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF7FFF, 0xFFFF7FFF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFEFFFF, 0xFFFEFFFF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFDFFFF, 0xFFFDFFFF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFBFFFF, 0xFFFBFFFF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFF7FFFF, 0xFFF7FFFF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFEFFFFF, 0xFFEFFFFF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFDFFFFF, 0xFFDFFFFF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFBFFFFF, 0xFFBFFFFF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFF7FFFFF, 0xFF7FFFFF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFEFFFFFF, 0xFEFFFFFF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFDFFFFFF, 0xFDFFFFFF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFBFFFFFF, 0xFBFFFFFF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xF7FFFFFF, 0xF7FFFFFF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xEFFFFFFF, 0xEFFFFFFF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xDFFFFFFF, 0xDFFFFFFF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xBFFFFFFF, 0xBFFFFFFF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF,
   };
   DataPattern patIsolatedZeros =
   {
      sizeof(pIsolatedZeros),
      pIsolatedZeros
   };

   UINT32 pSlowFalling[] =
   {
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000001, 0x00000001, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000002, 0x00000002, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000004, 0x00000004, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000008, 0x00000008, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000010, 0x00000010, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000020, 0x00000020, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000040, 0x00000040, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000080, 0x00000080, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000100, 0x00000100, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000200, 0x00000200, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000400, 0x00000400, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000800, 0x00000800, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00001000, 0x00001000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00002000, 0x00002000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00004000, 0x00004000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00008000, 0x00008000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00010000, 0x00010000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00020000, 0x00020000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00040000, 0x00040000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00080000, 0x00080000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00100000, 0x00100000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00200000, 0x00200000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00400000, 0x00400000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00800000, 0x00800000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x01000000, 0x01000000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x02000000, 0x02000000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x04000000, 0x04000000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x08000000, 0x08000000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x10000000, 0x10000000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x20000000, 0x20000000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x40000000, 0x40000000, 0x00000000, 0x00000000,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x80000000, 0x80000000, 0x00000000, 0x00000000,
   };
   DataPattern patSlowFalling =
   {
      sizeof(pSlowFalling),
      pSlowFalling
   };

   UINT32 pSlowRising[] =
   {
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFE, 0xFFFFFFFE, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFD, 0xFFFFFFFD, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFB, 0xFFFFFFFB, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFF7, 0xFFFFFFF7, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFEF, 0xFFFFFFEF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFDF, 0xFFFFFFDF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFBF, 0xFFFFFFBF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFFFFF7F, 0xFFFFFF7F, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFFFFEFF, 0xFFFFFEFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFFFFDFF, 0xFFFFFDFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFFFFBFF, 0xFFFFFBFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFFFF7FF, 0xFFFFF7FF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFFFEFFF, 0xFFFFEFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFFFDFFF, 0xFFFFDFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFFFBFFF, 0xFFFFBFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFFF7FFF, 0xFFFF7FFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFFEFFFF, 0xFFFEFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFFDFFFF, 0xFFFDFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFFBFFFF, 0xFFFBFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFF7FFFF, 0xFFF7FFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFEFFFFF, 0xFFEFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFDFFFFF, 0xFFDFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFFBFFFFF, 0xFFBFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFF7FFFFF, 0xFF7FFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFEFFFFFF, 0xFEFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFDFFFFFF, 0xFDFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFBFFFFFF, 0xFBFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xF7FFFFFF, 0xF7FFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xEFFFFFFF, 0xEFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xDFFFFFFF, 0xDFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xBFFFFFFF, 0xBFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x7FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
   };
   DataPattern patSlowRising =
   {
      sizeof(pSlowRising),
      pSlowRising
   };

   UINT32       patRandBuffer[0x10000];
   DataPattern patRand =
   {
      sizeof(patRandBuffer),
      patRandBuffer
   };

   DataPattern * patList[] =
   {
      &patRand,         //0
      &patA,
      &pat0,
      &patWalking1,     //3
      &pat01,
      &patTripleWhammy, //5
      &patIsolatedOnes,
      &patIsolatedZeros,
      &patSlowFalling,  //8
      &patSlowRising,
   };

   // Colwenient tones for audio
   enum AudioTone
   {
       PAT_A        = 440,
       PAT_Bb       = 466,
       PAT_B        = 494,
       PAT_C        = 523,
       PAT_Db       = 554,
       PAT_D        = 587,
       PAT_Eb       = 624,
       PAT_E        = 659,
       PAT_F        = 698,
       PAT_Gb       = 740,
       PAT_G        = 784,
       PAT_Ab       = 831,
       PAT_P        = 0xffffffff
   };

   // A recognizable series of tones for audio
   //  to make sure different buffers are being played
   UINT32 pAci[] =
   {
      PAT_P,  PAT_D,  PAT_P,  PAT_B,  PAT_G/2,  PAT_E/2,  PAT_D,  PAT_B,
      PAT_G/2, PAT_B,  PAT_P,  PAT_G/2,  PAT_P,  PAT_B,  PAT_P,
      PAT_P,  PAT_A,  PAT_G/2,  PAT_A,  PAT_G/2,  PAT_F/2,  PAT_G/2,
      PAT_P,  PAT_P,  PAT_F/2,  PAT_G/2,  PAT_B,  PAT_D,  PAT_E,
      PAT_F,

      PAT_P,  PAT_P,  PAT_P,  PAT_P,
   };

   DataPattern patAci =
   {
      sizeof(pAci)/sizeof(UINT32),
      pAci
   };
}

//---------------------------------------------------------------
// Data Pattern & Memory related utility functions
//---------------------------------------------------------------

// allocate the maximum amount of memory possible
RC Pattern::AllocateAllMem(BufferSpace * Bufs, UINT32 MBs, UINT32 FreeMB)
{
   INT32 alloc_mem, max_mem;
   UINT32 Unit = 1<<20; //1MB
   UINT32 MemSize = 0;
   UINT32 FreeSize = FreeMB * Unit;
   max_mem = MBs * Unit;

   // Allocate Memory
   // we support up to 10 different blocks of memory
   for (INT32 i = 0; i < NUMBUFS; i++)
   {
      Bufs->BufferAddress[i] = 0;

      if (max_mem > 0)
      {
         alloc_mem = max_mem;

         // allocate the largest block of memory left in the system
         while ( alloc_mem > 0 )
         {
            Bufs->BufferAddress[i] = (UINT32 *)malloc(alloc_mem + Unit);
            if (Bufs->BufferAddress[i] != 0)
               break;
            alloc_mem -= Unit;
         }

         Bufs->BufferSize[i] = alloc_mem;
         max_mem -= alloc_mem;
         MemSize += alloc_mem;

         //Align on 1M boundary
         Bufs->StartAddress[i] =
            (UINT32*)(((LwUPtr)(Bufs->BufferAddress[i]) + Unit) & ~(Unit - 1));
      }

      // clear the entry out if we didnt allocate
      if (Bufs->BufferAddress[i] == 0)
      {
         Bufs->BufferSize[i] = 0;
         Bufs->StartAddress[i] = 0;
      }
   }

   // We need to free some of this memory so other things can allocate it
   if (MemSize <= FreeSize)
   {
      Printf(Tee::PriNormal, "Error: Free Memory Requirement of %dM not met.\n",
             FreeMB);
      FreeAllMem(Bufs);
      return RC::CANNOT_ALLOCATE_MEMORY;
   }
   else
   {
      // Free the smallest buffer, repeat until we have our required free mem
      INT32 i = NUMBUFS - 1;
      while (FreeSize)
      {
         if (i < 0)
            MASSERT(0);

         if (Bufs->BufferSize[i])
         {
            // Free the buffer, then reallocate smaller
            if (Bufs->BufferSize[i] > FreeSize)
            {
               UINT32 PlugSize = FreeSize;
               void * Plug;

               Bufs->BufferSize[i] -= FreeSize;
               FreeSize -= FreeSize;

               free(Bufs->BufferAddress[i]);

               // XXXX Cheap hack!! For some reason, when we free xM, and
               //  alloc (x - n)M, we still get xM back, and therefore no
               //  free memory, and alot of hangs due to null pointers.
               //  The solution? Allocate some memory to hold a space to
               //  free later. This will change whan we take over all
               //  memory allocation.
               Plug = malloc(PlugSize);

               // This was so buggy that removing some static objects
               // for MODS caused this to fail.  Hacking to make buggy
               // code at least work.

               while ( Bufs->BufferSize[i] > 0 )
               {
                   Bufs->BufferAddress[i] = (UINT32 *)malloc(Bufs->BufferSize[i] + Unit);
                   if (Bufs->BufferAddress[i] != 0)
                     break;
                   Bufs->BufferSize[i] -= Unit;
               }
               free(Plug);

               // clear the entry out if we didnt allocate
               if (Bufs->BufferAddress[i] == 0)
               {
                   Bufs->BufferSize[i] = 0;
                   Bufs->StartAddress[i] = 0;
               }

               if (!Bufs->BufferAddress[i])
               {
                  Bufs->StartAddress[i] = 0;
                  Bufs->BufferAddress[i] = 0;
                  Bufs->BufferSize[i] = 0;
                  MASSERT(Bufs->BufferAddress[i]);

                  Printf(Tee::PriNormal,
                         "Warning: Failed to realloc any of the freed block in Patterns::AllocAllMem.\n");
                  if (i == 0)
                  {
                      Printf(Tee::PriNormal,
                             "No blocks allocated, failing.\n");
                      FreeAllMem(Bufs);
                      return RC::CANNOT_ALLOCATE_MEMORY;
                  }
               }
               else
               {
                   Bufs->StartAddress[i] =
                     (UINT32*)(((LwUPtr)(Bufs->BufferAddress[i]) + Unit) & ~(Unit - 1));
               }
            }
            else // if (Bufs->BufferSize[i] <= FreeSize)
            {
               FreeSize -= Bufs->BufferSize[i];

               free(Bufs->BufferAddress[i]);
               Bufs->StartAddress[i] = 0;
               Bufs->BufferAddress[i] = 0;
               Bufs->BufferSize[i] = 0;
            }
         }
         i--;
      } // while (FreeSize)
   }

   return OK;
}

// Find the total size of the buffer, and print out the locations
UINT32 Pattern::SizeAllMem(BufferSpace * Bufs, INT32 Priority)
{
   UINT32 MemorySize = 0;

   for (INT32 i = 0; i < NUMBUFS; i++)
   {
      if (Bufs->BufferAddress[i])
      {
         Printf(Priority,
                "Buffer[%d]:0x%08llx, Size:%dMB.\n", i,(UINT64)(LwUPtr)Bufs->StartAddress[i],
                Bufs->BufferSize[i] >> 20);

         MemorySize += Bufs->BufferSize[i];
      }
   }

   return MemorySize;
}

// Easy Call to free all the memory allocated by AllocateAllRam
RC Pattern::FreeAllMem(BufferSpace * Bufs)
{
   for (INT32 i = 0; i < NUMBUFS; i++)
   {
      if (Bufs->BufferAddress[i])
      {
         free(Bufs->BufferAddress[i]);
         Bufs->BufferAddress[i] = 0;
      }
   }
   return OK;
}

// Fill up the random data pattern
RC Pattern::GenRandPat(UINT32 seed)
{
   Random random;
   random.SeedRandom(seed);
   for (UINT32 i = 0; i < (patRand.Length >> 2); i++)
   {
      patRand.Data[i]  = random.GetRandom();
   }
   return OK;
}

RC Pattern::FillPattern(UINT32 * dst, DataPattern * src, UINT32 length)
{
   UINT32 sindex = 0;
   UINT32 slength = src->Length >> 2;
   UINT32 dlength = length >> 2;  //colwert from bytes to DW index

   for (UINT32 i = 0; i < dlength; i++)
   {
      dst[i] = src->Data[sindex];
      sindex++;
      if (sindex >= slength)
      {
         sindex = 0;
      }
   }
   return OK;
}

JS_CLASS(Patterns);

P_(Patterns_Get_A);
P_(Patterns_Get_0);
P_(Patterns_Get_Walking1);
P_(Patterns_Get_01);
P_(Patterns_Get_TripleWhammy);
P_(Patterns_Get_IsolatedOnes);
P_(Patterns_Get_IsolatedZeros);
P_(Patterns_Get_SlowFalling);
P_(Patterns_Get_SlowRising);

//!  The Pattern Object
/**
 *
 */
static SObject Patterns_Object
(
   "Patterns",
   PatternsClass,
   0,
   0,
   "Patterns Class"
);

//! A pattern
/**
 * Read Only
 */
static SProperty Patterns_A
(
   Patterns_Object,
   "A",
   0,
   0,
   Patterns_Get_A,
   0,
   JSPROP_READONLY,
   "A pattern"
);

//! 0 pattern
/**
 * Read Only
 */
static SProperty Patterns_0
(
   Patterns_Object,
   "Zero",
   0,
   0,
   Patterns_Get_0,
   0,
   JSPROP_READONLY,
   "0 pattern"
);

//! Walking1 pattern
/**
 * Read Only
 */
static SProperty Patterns_Walking1
(
   Patterns_Object,
   "Walking1",
   0,
   0,
   Patterns_Get_Walking1,
   0,
   JSPROP_READONLY,
   "Walking 1 pattern"
);

//! 01 pattern
/**
 * Read Only
 */
static SProperty Patterns_01
(
   Patterns_Object,
   "ZeroOne",
   0,
   0,
   Patterns_Get_01,
   0,
   JSPROP_READONLY,
   "01 pattern"
);

//! TripleWhammy pattern
/**
 * Read Only
 */
static SProperty Patterns_TripleWhammy
(
   Patterns_Object,
   "TripleWhammy",
   0,
   0,
   Patterns_Get_TripleWhammy,
   0,
   JSPROP_READONLY,
   "TripleWhammy pattern"
);

//! IsolatedOnes pattern
/**
 * Read Only
 */
static SProperty Patterns_IsolatedOnes
(
   Patterns_Object,
   "IsolatedOnes",
   0,
   0,
   Patterns_Get_IsolatedOnes,
   0,
   JSPROP_READONLY,
   "IsolatedOnes pattern"
);

//! IsolatedZeros pattern
/**
 * Read Only
 */
static SProperty Patterns_IsolatedZeros
(
   Patterns_Object,
   "IsolatedZeros",
   0,
   0,
   Patterns_Get_IsolatedZeros,
   0,
   JSPROP_READONLY,
   "IsolatedZeros pattern"
);

//! SlowFalling pattern
/**
 * Read Only
 */
static SProperty Patterns_SlowFalling
(
   Patterns_Object,
   "SlowFalling",
   0,
   0,
   Patterns_Get_SlowFalling,
   0,
   JSPROP_READONLY,
   "SlowFalling pattern"
);

//! SlowRising pattern
/**
 * Read Only
 */
static SProperty Patterns_SlowRising
(
   Patterns_Object,
   "SlowRising",
   0,
   0,
   Patterns_Get_SlowRising,
   0,
   JSPROP_READONLY,
   "SlowRising pattern"
);

static JSBool ReturnJSArray(JSContext *pContext, jsval *pReturlwalue, const Pattern::DataPattern &dp)
{
   RC rc;
   JavaScriptPtr pJs;
   JsArray pattern;
   jsval pixel;

   for (UINT32 i = 0; i < (dp.Length / 4); ++i)
   {
      C_CHECK_RC(pJs->ToJsval(dp.Data[i], &pixel));
      pattern.push_back(pixel);
   }

   if (OK != (rc = pJs->ToJsval(&pattern, pReturlwalue)))
   {
      Printf(Tee::PriWarn, "Failed to colwert pattern into JsArray\n");
      *pReturlwalue = INT_TO_JSVAL(rc);
   }

   return JS_TRUE;
}

P_(Patterns_Get_A)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   return ReturnJSArray(pContext, pValue, Pattern::patA);
}

P_(Patterns_Get_0)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   return ReturnJSArray(pContext, pValue, Pattern::pat0);
}

P_(Patterns_Get_Walking1)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   return ReturnJSArray(pContext, pValue, Pattern::patWalking1);
}

P_(Patterns_Get_01)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   return ReturnJSArray(pContext, pValue, Pattern::pat01);
}

P_(Patterns_Get_TripleWhammy)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   return ReturnJSArray(pContext, pValue, Pattern::patTripleWhammy);
}

P_(Patterns_Get_IsolatedOnes)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   return ReturnJSArray(pContext, pValue, Pattern::patIsolatedOnes);
}

P_(Patterns_Get_IsolatedZeros)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   return ReturnJSArray(pContext, pValue, Pattern::patIsolatedZeros);
}

P_(Patterns_Get_SlowFalling)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   return ReturnJSArray(pContext, pValue, Pattern::patSlowFalling);
}

P_(Patterns_Get_SlowRising)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   return ReturnJSArray(pContext, pValue, Pattern::patSlowRising);
}

