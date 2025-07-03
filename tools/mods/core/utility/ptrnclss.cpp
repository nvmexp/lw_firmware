/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011,2013,2016-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file ptrnclss.cpp
//! \brief Source file for the PatternClass class

#include "ptrnclss.h"
#include "core/include/memoryif.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "random.h"
#include "core/include/massert.h"
#include "core/include/color.h"
#include "core/include/imagefil.h"
#include "core/include/tee.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include <algorithm>
#include "core/include/xp.h"
#include "core/include/massert.h"
#include "core/include/memerror.h"
#include <string>
#include <algorithm>

//!-------------------- DATA PATTERNS --------------------

static UINT32 s_ShortMats[]=
{
    0x00000000, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA, PatternClass::END_PATTERN
};

static UINT32 s_LongMats[]=
{
    0x00000000, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA,
    0x33333333, 0xCCCCCCCC, 0xF0F0F0F0, 0x0F0F0F0F,
    0x00FF00FF, 0xFF00FF00, 0x0000FFFF, 0xFFFF0000, PatternClass::END_PATTERN
};

static UINT32 s_WalkingOnes[]=
{
    0x00000001, 0x00000002, 0x00000004, 0x00000008,
    0x00000010, 0x00000020, 0x00000040, 0x00000080,
    0x00000100, 0x00000200, 0x00000400, 0x00000800,
    0x00001000, 0x00002000, 0x00004000, 0x00008000,
    0x00010000, 0x00020000, 0x00040000, 0x00080000,
    0x00100000, 0x00200000, 0x00400000, 0x00800000,
    0x01000000, 0x02000000, 0x04000000, 0x08000000,
    0x10000000, 0x20000000, 0x40000000, 0x80000000,
    0x00000000, PatternClass::END_PATTERN
};

static UINT32 s_WalkingZeros[]=
{
    0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFB, 0xFFFFFFF7,
    0xFFFFFFEF, 0xFFFFFFDF, 0xFFFFFFBF, 0xFFFFFF7F,
    0xFFFFFEFF, 0xFFFFFDFF, 0xFFFFFBFF, 0xFFFFF7FF,
    0xFFFFEFFF, 0xFFFFDFFF, 0xFFFFBFFF, 0xFFFF7FFF,
    0xFFFEFFFF, 0xFFFDFFFF, 0xFFFBFFFF, 0xFFF7FFFF,
    0xFFEFFFFF, 0xFFDFFFFF, 0xFFBFFFFF, 0xFF7FFFFF,
    0xFEFFFFFF, 0xFDFFFFFF, 0xFBFFFFFF, 0xF7FFFFFF,
    0xEFFFFFFF, 0xDFFFFFFF, 0xBFFFFFFF, 0x7FFFFFFF,
    0xFFFFFFFF, PatternClass::END_PATTERN
};

static UINT32 s_SuperZeroOne[]=
{
    0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
    0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
    0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
    0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
    0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
    0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
    0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
    0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
    0xFFFFFFFF, PatternClass::END_PATTERN
};

static UINT32 s_ByteSuper[]=
{
    0x00FF0000, 0x00FFFFFF, 0x00FF0000, 0xFF00FFFF,
    0x0000FF00, 0xFFFF00FF, 0x000000FF, 0xFFFFFF00,
    0x00000000, PatternClass::END_PATTERN
};

static UINT32 s_CheckerBoard[]=
{
    0xAAAAAAAA, 0x55555555, 0xAAAAAAAA, 0x55555555,
    0xAAAAAAAA, 0x55555555, 0xAAAAAAAA, 0x55555555,
    0xAAAAAAAA, PatternClass::END_PATTERN
};

static UINT32 s_IlwCheckerBd[]=
{
    0x55555555, 0xAAAAAAAA, 0x55555555, 0xAAAAAAAA,
    0x55555555, 0xAAAAAAAA, 0x55555555, 0xAAAAAAAA,
    0xAAAAAAAA, PatternClass::END_PATTERN
};

static UINT32 s_SolidZero[]=
{
    0x00000000, 0x00000000, 0x00000000, 0x00000000, PatternClass::END_PATTERN
};

static UINT32 s_SolidOne[]=
{
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, PatternClass::END_PATTERN
};

static UINT32 s_WalkSwizzle[]=
{
    0x00000001, 0x7FFFFFFF, 0x00000002, 0xBFFFFFFF,
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
    0x00000000, PatternClass::END_PATTERN
};

static UINT32 s_WalkNormal[]=
{
    0x00000001, 0xFFFFFFFE, 0x00000002, 0xFFFFFFFD,
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
    0x00000000, PatternClass::END_PATTERN
};

static UINT32 s_SolidAlpha[] =
{
    0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFF000000, 0xFF000000, 0xFF000000, PatternClass::END_PATTERN
};

static UINT32 s_SolidRed[] =
{
    0x00FF0000, 0x00FF0000, 0x00FF0000, 0x00FF0000,
    0x00FF0000, 0x00FF0000, 0x00FF0000, PatternClass::END_PATTERN
};

static UINT32 s_SolidGreen[] =
{
    0x0000FF00, 0x0000FF00, 0x0000FF00, 0x0000FF00,
    0x0000FF00, 0x0000FF00, 0x0000FF00, PatternClass::END_PATTERN
};

static UINT32 s_SolidBlue[]=
{
    0x000000FF, 0x000000FF, 0x000000FF, 0x000000FF,
    0x000000FF, 0x000000FF, 0x000000FF, PatternClass::END_PATTERN
};

static UINT32 s_SolidWhite[]=
{
    0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF,
    0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, PatternClass::END_PATTERN
};

static UINT32 s_SolidCyan[]=
{
    0xFF00FFFF, 0xFF00FFFF, 0xFF00FFFF, 0xFF00FFFF,
    0xFF00FFFF, 0xFF00FFFF, 0xFF00FFFF, PatternClass::END_PATTERN
};

static UINT32 s_SolidMagenta[]=
{
    0xFFFF00FF, 0xFFFF00FF, 0xFFFF00FF, 0xFFFF00FF,
    0xFFFF00FF, 0xFFFF00FF, 0xFFFF00FF, PatternClass::END_PATTERN
};

static UINT32 s_SolidYellow[]=
{
    0xFFFFFF00, 0xFFFFFF00, 0xFFFFFF00, 0xFFFFFF00,
    0xFFFFFF00, 0xFFFFFF00, 0xFFFFFF00, PatternClass::END_PATTERN
};

static UINT32 s_DoubleZeroOne[]=
{
    0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, PatternClass::END_PATTERN
};

static UINT32 s_TripleSuper[]=
{
    0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, PatternClass::END_PATTERN
};

static UINT32 s_QuadZeroOne[]=
{
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, PatternClass::END_PATTERN
};

static UINT32 s_TripleWhammy[]=
{
    0x00000000, 0x00000001, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFF,
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
    0x00000000, PatternClass::END_PATTERN
};

static UINT32 s_IsolatedOnes[]=
{
    0x00000000, 0x00000000, 0x00000000, 0x00000001,
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
    0x00000000, PatternClass::END_PATTERN
};

static UINT32 s_IsolatedZeros[]=
{
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE,
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
    0xFFFFFFFF, PatternClass::END_PATTERN
};

static UINT32 s_SlowFalling[]=
{
    0xFFFFFFFF, 0x00000001, 0x00000000,
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
    0xFFFFFFFF, PatternClass::END_PATTERN
};

static UINT32 s_SlowRising[]=
{
    0x00000000, 0xFFFFFFFE, 0xFFFFFFFF,
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
    0x00000000, PatternClass::END_PATTERN
};

static UINT32 s_HoppingZeroes[]=
{
    0x00000000, 0x0F0F0F0F, 0x00000000, 0xF0F0F0F0,
    0xFFFFFFFF, 0x0F0F0F0F, 0xFFFFFFFF, 0xF0F0F0F0,
    PatternClass::END_PATTERN
};

static UINT32 s_MarchingZeroes[]=
{
    0xFFFFFFFF, 0x0FFFFFFF, 0x0F0FFFFF, 0x0F0F0FFF,
    0x0F0F0F0F, 0xFF0F0F0F, 0xFFFF0F0F, 0xFFFFFF0F,
    0xF0FFFFFF, 0xF0F0FFFF, 0xF0F0F0FF, 0xF0F0F0F0,
    0xFFF0F0F0, 0xFFFFF0F0, 0xFFFFFFF0, 0xFFFFFFFF,
    PatternClass::END_PATTERN
};

static UINT32 s_RunSwizzle[]=
{
    0x0FFFFFFF, 0x0000000F, 0xF0FFFFFF, 0x000000F0,
    0xFF0FFFFF, 0x00000F00, 0xFFF0FFFF, 0x0000F000,
    0xFFFF0FFF, 0x000F0000, 0xFFFFF0FF, 0x00F00000,
    0xFFFFFF0F, 0x0F00000F, 0xFFFFFFF0, 0xF0000000,
    PatternClass::END_PATTERN
};

// End of pattern set

#define RAND_SIZE 83 // size of the random pattern array
static UINT32 s_RandomPattern[RAND_SIZE];

static const char RandomPatternName[] = "RandomPattern";
//! Here we declare our static pattern struct array
//! ATTENTION! Make sure you keep the random pattern at the end of the array
//! Otherwise you will need to change FillWithRandomPattern() which assumes
//! the random pattern exist in the very last element of this patternlist.
//! Be careful about changing the order of the patterns too since glr_comm.h and
//! r_tex.cpp are making assumptions about the ordering of the patterns.
static PatternClass::PTTRN s_PredefinedPatternList[] =
{
    {"ShortMats", PatternClass::PATTERNSET_STRESSFUL, s_ShortMats},
    {"LongMats", PatternClass::PATTERNSET_NORMAL, s_LongMats},
    {"WalkingOnes", PatternClass::PATTERNSET_NORMAL, s_WalkingOnes},
    {"WalkingZeros", PatternClass::PATTERNSET_STRESSFUL, s_WalkingZeros},
    {"SuperZeroOne", PatternClass::PATTERNSET_NORMAL, s_SuperZeroOne},
    {"ByteSuper", PatternClass::PATTERNSET_NORMAL, s_ByteSuper},
    {"CheckerBoard", PatternClass::PATTERNSET_STRESSFUL, s_CheckerBoard},
    {"IlwCheckerBd", PatternClass::PATTERNSET_NORMAL, s_IlwCheckerBd},
    {"WalkSwizzle", PatternClass::PATTERNSET_STRESSFUL, s_WalkSwizzle},
    {"WalkNormal", PatternClass::PATTERNSET_NORMAL, s_WalkNormal},
    {"DoubleZeroOne", PatternClass::PATTERNSET_NORMAL, s_DoubleZeroOne},
    {"TripleSuper", PatternClass::PATTERNSET_NORMAL, s_TripleSuper},
    {"QuadZeroOne", PatternClass::PATTERNSET_NORMAL, s_QuadZeroOne},
    {"TripleWhammy", PatternClass::PATTERNSET_STRESSFUL, s_TripleWhammy},
    {"IsolatedOnes", PatternClass::PATTERNSET_STRESSFUL, s_IsolatedOnes},
    {"IsolatedZeros", PatternClass::PATTERNSET_NORMAL, s_IsolatedZeros},
    {"SlowFalling", PatternClass::PATTERNSET_NORMAL, s_SlowFalling},
    {"SlowRising", PatternClass::PATTERNSET_STRESSFUL, s_SlowRising},
    {"SolidZero", PatternClass::PATTERNSET_NORMAL, s_SolidZero},
    {"SolidOne", PatternClass::PATTERNSET_NORMAL, s_SolidOne},
    {"SolidAlpha", PatternClass::PATTERNSET_STRESSFUL, s_SolidAlpha},
    {"SolidRed", PatternClass::PATTERNSET_NORMAL, s_SolidRed},
    {"SolidGreen", PatternClass::PATTERNSET_NORMAL, s_SolidGreen},
    {"SolidBlue", PatternClass::PATTERNSET_STRESSFUL, s_SolidBlue},
    {"SolidWhite", PatternClass::PATTERNSET_NORMAL, s_SolidWhite},
    {"SolidCyan", PatternClass::PATTERNSET_STRESSFUL, s_SolidCyan},
    {"SolidMagenta", PatternClass::PATTERNSET_NORMAL, s_SolidMagenta},
    {"SolidYellow", PatternClass::PATTERNSET_STRESSFUL, s_SolidYellow},
    {"MarchingZeroes", PatternClass::PATTERNSET_NORMAL, s_MarchingZeroes},
    {"HoppingZeroes", PatternClass::PATTERNSET_STRESSFUL, s_HoppingZeroes},
    {"RunSwizzle", PatternClass::PATTERNSET_STRESSFUL, s_RunSwizzle},
    {RandomPatternName, PatternClass::PATTERNSET_STRESSFUL, s_RandomPattern},
};
static const UINT32 s_NumPredefinedPatterns =
        sizeof(s_PredefinedPatternList) / sizeof(s_PredefinedPatternList[0]);

//-----------------------------------------------------------------------------
// Get a pointer to the global patterns list (including any patterns that
// might have been added by the user at runtime).
//
// Makes sure the global list is initialized before it is used.
//
static vector<PatternClass::PTTRN*> & GetAllPatternsList()
{
    static vector<PatternClass::PTTRN*> s_AllPatternsList;
    if (s_AllPatternsList.size() == 0)
    {
        for (UINT32 i = 0; i < s_NumPredefinedPatterns; i++)
            s_AllPatternsList.push_back( &s_PredefinedPatternList[i] );

        s_RandomPattern[RAND_SIZE - 1] = PatternClass::END_PATTERN;
        // @@@ Should we fill the random image here?
    }

    return s_AllPatternsList;
}

PatternClass::PatternContainer PatternClass::GetAllPatternsList()
{
    const vector<PatternClass::PTTRN*>& AllPatterns = ::GetAllPatternsList();
    PatternContainer Copy(AllPatterns.size());
    std::copy(AllPatterns.begin(), AllPatterns.end(), Copy.begin());
    return Copy;
}

// Class methods go below
PatternClass::PatternClass() :m_RefAllPatternsList(::GetAllPatternsList())
{
    m_LwrrentFillPatternIndex = 0;
    m_LwrrentCheckPatternIndex = 0;

    SelectPatternSet(PatternClass::PATTERNSET_ALL); // select all existing patterns by default
}

RC DeleteUserPatterns();

PatternClass::~PatternClass()
{
}

// Utility functions go here
//---------------------------------

//! Sets the current fill pattern index to 0
void PatternClass::ResetFillIndex()
{
    m_LwrrentFillPatternIndex = 0;
}

//! Sets the current check pattern index to 0
void PatternClass::ResetCheckIndex()
{
    m_LwrrentCheckPatternIndex = 0;
}

//! Randomly fills the random pattern array. Must be called before random pattern is used
void PatternClass::FillRandomPattern(UINT32 seed)
{
    Random rnd;
    rnd.SeedRandom( seed );

    for (UINT32 i = 0; i < RAND_SIZE - 1; i++)
    {
        s_RandomPattern[i] = rnd.GetRandom();
    }

    s_RandomPattern[RAND_SIZE - 1] = PatternClass::END_PATTERN;
}

//! Selects the patterns according to the given parameter flag
RC PatternClass::SelectPatternSet(PatternSets Set)
{
    SelectedPatternSet.clear();
    ResetFillIndex();
    ResetCheckIndex();

    for (UINT32 i = 0; i < m_RefAllPatternsList.size(); i++)
    {
        if ((m_RefAllPatternsList[i]->patternAttributes & Set))
            SelectedPatternSet.push_back( i );
    }
    if (SelectedPatternSet.size() == 0)
        return RC::ILWALID_INPUT;

    return OK;

}

//! Selects the patterns having a vector provided as the parameter
RC PatternClass::SelectPatterns(vector<UINT32> *Patterns)
{
    SelectedPatternSet.clear();
    ResetFillIndex();
    ResetCheckIndex();

    for (UINT32 pattern: *Patterns)
    {
        if (pattern >= m_RefAllPatternsList.size())
            return RC::ILWALID_INPUT;

        SelectedPatternSet.push_back(pattern);
    }
    return OK;
}

RC PatternClass::DisableRandomPattern()
{
    ResetFillIndex();
    ResetCheckIndex();

    const size_t selectedPatternSetSize = SelectedPatternSet.size();
    for (UINT32 patternSetIdx = 0; patternSetIdx < selectedPatternSetSize; patternSetIdx++)
    {
        UINT32 patternIdx = SelectedPatternSet[patternSetIdx];

        if (m_RefAllPatternsList[patternIdx]->thePattern)
        {
            if (m_RefAllPatternsList[patternIdx]->patternName.compare(
                RandomPatternName) == 0)
            {
                SelectedPatternSet.erase(SelectedPatternSet.begin() + patternSetIdx);
                break;
            }
        }
    }

    if (SelectedPatternSet.size() == 0)
        return RC::ILWALID_INPUT;

    return RC::OK;
}

//! Writes the IDs of the requested pattern type in the provided vector.
//! Clears the provided vector before filling it
RC PatternClass::GetPatterns(PatternSets Set, vector<UINT32> *Patterns)
{
    MASSERT( Patterns != 0);

    //flush the vector
    Patterns->clear();

    for (UINT32 i = 0; i < m_RefAllPatternsList.size(); i++)
        if ((m_RefAllPatternsList[i]->patternAttributes & Set))
            Patterns->push_back(i);

    return OK;
}

//! Returns how many selected patterns there are right now
UINT32 PatternClass::GetLwrrentNumOfSelectedPatterns()
{
    return static_cast<UINT32>(SelectedPatternSet.size());
}

//! Returns total # of patterns
UINT32 PatternClass::GetTotalNumOfPatterns()
{
    return static_cast<UINT32>(m_RefAllPatternsList.size());
}

//! Fills up the provided vector with pattern IDs.
//! The vector is supposed to be empty
RC PatternClass::GetSelectedPatterns(vector<UINT32> *Patterns)
{
    MASSERT(Patterns != 0);

    for (UINT32 pattern: SelectedPatternSet)
    {
        Patterns->push_back(pattern);
    }

    return OK;
}

//! Provides the pattern name with the given ID
RC PatternClass::GetPatternName(UINT32 pattern, string *pName) const
{
    MASSERT( pName != nullptr );

    if (pattern >= m_RefAllPatternsList.size())
        return RC::ILWALID_INPUT;

    *pName = m_RefAllPatternsList[pattern]->patternName;
    return OK;
}

RC PatternClass::GetPatternNumber(UINT32 *pPatternNum, string szName) const
{
    for (UINT32 i = 0; i < m_RefAllPatternsList.size(); i++)
    {
        if (m_RefAllPatternsList[i]->patternName == szName)
        {
            *pPatternNum = i;
            return OK;
        }
    }
    return RC::SOFTWARE_ERROR;
}

UINT32 PatternClass::GetPatternLen (UINT32 Pattern) const
{
    UINT32 i = 0;
    if (Pattern < m_RefAllPatternsList.size())
    {
        while (m_RefAllPatternsList[Pattern]->thePattern[i] != END_PATTERN)
        {
            ++i;
        }
    }
    return i;
}

//! Fills the given memory address with the provided pattern ID.
RC PatternClass::FillMemoryWithPattern
(
    volatile void *pBuffer,
    UINT32 pitchInBytes,
    UINT32 lines,
    UINT32 widthInBytes,
    UINT32 depth,
    string patternName,
    UINT32 *repeatInterval
)
{
    for (UINT32 pattern = 0; pattern < m_RefAllPatternsList.size(); pattern++)
    {
        if (m_RefAllPatternsList[pattern]->patternName == patternName)
        {
            return FillMemoryWithPattern(pBuffer, pitchInBytes, lines,
                                         widthInBytes, depth, pattern, repeatInterval);
        }
    }
    Printf(Tee::PriError, "Invalid pattern name:%s specified\n", patternName.c_str());
    return RC::SOFTWARE_ERROR;
}
//! Fills the given memory address with the provided pattern ID.
RC PatternClass::FillMemoryWithPattern
(
    volatile void *pBuffer,
    UINT32 PitchInBytes,
    UINT32 Lines,
    UINT32 WidthInBytes,
    UINT32 Depth,
    UINT32 PatternNum,
    UINT32 *RepeatInterval
)
{
    MASSERT( pBuffer != nullptr );

    if (PatternNum >= m_RefAllPatternsList.size())
        return RC::ILWALID_INPUT;

    if (RepeatInterval)
        *RepeatInterval = GetPatternLen (PatternNum);

    UINT32 X;
    UINT32 Y;
    volatile UINT08 * Row = static_cast<volatile UINT08*>(pBuffer);

    const UINT32 *pattern = m_RefAllPatternsList[PatternNum]->thePattern;

    for (Y = 0; Y < Lines; ++Y)
    {
        if (Depth == 32)
        {
            volatile UINT08 * Column = Row;
            UINT32 iterations = WidthInBytes/4;
            UINT32 cnt = 0;
            for (X = 0, cnt = 0; cnt < iterations; X += 4, cnt++)
            {
                MEM_WR32((volatile void *)(Column + X), *pattern);
                if (*(++pattern) == PatternClass::END_PATTERN)
                    pattern = m_RefAllPatternsList[PatternNum]->thePattern;
            }
            if (WidthInBytes & 3)
            {
                // fill up the rest if width is not word aligned
                volatile UINT08 *pEnd = Column + X + (WidthInBytes & 3);
                while (Column + X < pEnd)
                {
                 MEM_WR08( Column + X, *pattern );
                 if (*(++pattern) == PatternClass::END_PATTERN)
                    pattern = m_RefAllPatternsList[PatternNum]->thePattern;
                 X++;
                }
            }
        }
        else if (Depth == 16)
        {
            volatile UINT08 * Column = Row;
            UINT32 iterations = WidthInBytes/2;
            UINT32 cnt = 0;
            for (X = 0; cnt < iterations; X += 2, cnt++)
            {
                MEM_WR16((volatile void *)(Column + X), *pattern);
                if (*(++pattern) == PatternClass::END_PATTERN)
                    pattern = m_RefAllPatternsList[PatternNum]->thePattern;
            }
            if (WidthInBytes & 1)
            {
                MEM_WR08( Column + X, *pattern  );
                if (*(++pattern) == PatternClass::END_PATTERN)
                    pattern = m_RefAllPatternsList[PatternNum]->thePattern;
            }
        }
        else
        {
            return RC::UNSUPPORTED_DEPTH;
        }
        Row += PitchInBytes;
    }
    return OK;
}

//! Fills the given memory space with random pattern that is pointed
//! by the m_LwrrentFillPatternIndex. Must be called after the random pattern is initialized.
//! ATTENTION: This function assumes that random pattern is the last pattern in the
//! static patternsList struct above.
RC PatternClass::FillWithRandomPattern
(
    volatile void *pBuffer,
    UINT32 PitchInBytes,
    UINT32 Lines,
    UINT32 WidthInBytes,
    UINT32 Depth
)
{
    UINT32 Dummy; // just to be able to call fillmemory
    return FillMemoryWithPattern( pBuffer, PitchInBytes, Lines, WidthInBytes,
            Depth, s_NumPredefinedPatterns - 1, &Dummy);
}

//! Fills the given memory space with current pattern that is pointed
//! by the m_LwrrentFillPatternIndex
RC PatternClass::FillMemoryIncrementing
(
    volatile void *pBuffer,
    UINT32 PitchInBytes,
    UINT32 Lines,
    UINT32 WidthInBytes,
    UINT32 Depth
)
{
    UINT32 toSend = SelectedPatternSet[m_LwrrentFillPatternIndex];
    UINT32 patternLength; // wont get used

    m_LwrrentFillPatternIndex =
        (++m_LwrrentFillPatternIndex == SelectedPatternSet.size()
         ? 0 : m_LwrrentFillPatternIndex);

    return FillMemoryWithPattern(pBuffer, PitchInBytes, Lines, WidthInBytes,
                                 Depth, toSend, &patternLength);
}

//! Does what it says in the function name :)
void PatternClass::IncrementLwrrentCheckPatternIndex()
{
    ++m_LwrrentCheckPatternIndex;
    if (m_LwrrentCheckPatternIndex == SelectedPatternSet.size())
    {
        m_LwrrentCheckPatternIndex = 0;
    }
}

void PatternClass::DecrementLwrrentCheckPatternIndex()
{
    m_LwrrentCheckPatternIndex = ( m_LwrrentCheckPatternIndex == 0 ) ?
        (UINT32)SelectedPatternSet.size() : (m_LwrrentCheckPatternIndex - 1);
}

//! Verifies the content of memory agains the selected pattern with given ID
RC PatternClass::CheckMemory
(
    volatile void *pBuffer,
    UINT32 pitchInBytes,
    UINT32 lines,
    UINT32 widthInBytes,
    UINT32 depth,
    UINT32 patternNum,
    CheckMemoryCallback *pCallback,
    void *pCaller
) const
{
    MASSERT(pBuffer != nullptr);

    if (patternNum >= m_RefAllPatternsList.size())
        return RC::ILWALID_INPUT;

    UINT32 X;
    UINT32 Y;
    volatile UINT32 ReadData;
    UINT32 *const patternStart = m_RefAllPatternsList[patternNum]->thePattern;
    const string &patternName  = m_RefAllPatternsList[patternNum]->patternName;
    UINT32 *pattern = patternStart;
    volatile UINT08 * Row = static_cast<volatile UINT08*>(pBuffer);
    for (Y = 0; Y < lines; ++Y)
    {
        volatile UINT08 * Column = Row;

        if (depth == 32)
        {
            for (X = 0; X < widthInBytes; X += 4)
            {
                ReadData = MEM_RD32(Column + X);

                if (ReadData != *pattern)
                {
                    UINT32 Offset = (UINT32)((Column + X) -
                                             (volatile UINT08 *)(pBuffer));
                    const UINT32 PatternOffset = static_cast<UINT32>(
                            (pattern - patternStart) * sizeof(UINT32));
                    // callback here
                    pCallback(pCaller, Offset, *pattern, ReadData,
                              PatternOffset, patternName);
                }
                if (*(++pattern) == PatternClass::END_PATTERN)
                    pattern = patternStart;
            }
        }
        else if (depth == 16)
        {
            volatile UINT16 Actual = 0;
            for (X = 0; X < widthInBytes; X += 2)
            {
                ReadData = MEM_RD32(Column + X);
                Actual = *((volatile UINT16 *)pattern);
                if (ReadData != Actual)
                {
                    UINT32 Offset = (UINT32)((Column + X) -
                                             (volatile UINT08 *)(pBuffer));
                    const UINT32 PatternOffset = static_cast<UINT32>(
                            (pattern - patternStart) * sizeof(UINT32));
                    // callback here
                    pCallback(pCaller, Offset, *pattern, ReadData,
                              PatternOffset, patternName);
                }
                if (*(++pattern) == PatternClass::END_PATTERN)
                    pattern = patternStart;
            }
        }
        else
        {
            return RC::UNSUPPORTED_DEPTH;
        }

        Row += pitchInBytes;
    }

    return OK;
}

//! Intended to be used with FillMemoryIncrementing
RC PatternClass::CheckMemoryIncrementing
(
    volatile void *pBuffer,
    UINT32 pitchInBytes,
    UINT32 lines,
    UINT32 widthInBytes,
    UINT32 depth,
    CheckMemoryCallback *pCallback,
    void *pCaller
)
{
    UINT32 toSend = SelectedPatternSet[m_LwrrentCheckPatternIndex];
    m_LwrrentCheckPatternIndex = ((m_LwrrentCheckPatternIndex + 1) %
                                  SelectedPatternSet.size());
    return (CheckMemory(pBuffer, pitchInBytes, lines, widthInBytes,
                        depth, toSend, pCallback, pCaller));

}

//! Prints out the lwrrently selected patterns that are used.
RC PatternClass::DumpSelectedPatterns()
{
    Printf(Tee::PriNormal, "  MASK       NAME     PATTERNS\n");
    Printf(Tee::PriNormal, "-------- ------------ ---------------------------------------------------\n");

    for (UINT32 i = 0; i< SelectedPatternSet.size(); i++)
    {
        UINT32 pat = SelectedPatternSet[i];
        if (pat >= m_RefAllPatternsList.size())
            return RC::ILWALID_INPUT;

        if (m_RefAllPatternsList[pat]->thePattern)
        {
            Printf(Tee::PriNormal, "%08x %-13s ",
                   1<<i, m_RefAllPatternsList[pat]->patternName.c_str());

            UINT32 j=0;
            while (j < 6 &&
                   m_RefAllPatternsList[pat]->thePattern[j] != END_PATTERN)
            {
                Printf(Tee::PriNormal, "%08x ", m_RefAllPatternsList[pat]->thePattern[j]);
                j++;
            }
            Printf(Tee::PriNormal, "\n");
        }
    }
    return OK;
}

//! Prints out a specific pattern with the give ID
RC DumpSinglePattern(UINT32 PatternNumber)
{
    vector<PatternClass::PTTRN*> & RefAllPatternsList = GetAllPatternsList();

    const UINT32 DATA_TO_DISP = 144;
    if (PatternNumber >= RefAllPatternsList.size())
    {
        Printf(Tee::PriNormal, "Argument is out of range.");
        return RC::ILWALID_INPUT;
    }

    if (RefAllPatternsList[PatternNumber]->thePattern == NULL)
    {
        Printf(Tee::PriNormal, "Pattern %d is not defined\n", PatternNumber);
        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriNormal, "First %d data in pattern %s (Number %d, Mask %08x)\n",
        DATA_TO_DISP, RefAllPatternsList[PatternNumber]->patternName.c_str(),
           PatternNumber,
           1 << PatternNumber);

    UINT32 pidx = 0;

    for (UINT32 i = 0; i < DATA_TO_DISP; i++)
    {
        Printf(Tee::PriNormal, "%08x ",
               RefAllPatternsList[PatternNumber]->thePattern[pidx]);
        pidx++;
        if (RefAllPatternsList[PatternNumber]->thePattern[pidx] ==
            PatternClass::END_PATTERN)
        {
            pidx = 0;
        }
        if (7 == i % 8)
        {
            Printf(Tee::PriNormal, "\n");
        }
    }
    Printf(Tee::PriNormal, "\n");
    return OK;

}

RC DumpAllPatterns()
{
    RC  rc;
    vector<PatternClass::PTTRN*> & RefAllPatternsList = GetAllPatternsList();
    for (UINT32 i = 0; i < RefAllPatternsList.size(); i++)
    {
        CHECK_RC(DumpSinglePattern(i));
    }
    return OK;
}

RC DeleteUserPatterns()
{
    vector<PatternClass::PTTRN*> & RefAllPatternsList = GetAllPatternsList();

    while (RefAllPatternsList.size() > s_NumPredefinedPatterns)
    {
        PatternClass::PTTRN *ptr = RefAllPatternsList.back();
        delete[] ptr->thePattern;
        delete ptr;
        RefAllPatternsList.pop_back();
    }

    return OK;
}

///////////////////////////////////////////////////////////////////////////////////////
// Java Script Interface of PatternClass class
///////////////////////////////////////////////////////////////////////////////////////

JS_CLASS( PatternClass );

static SObject PatternClass_Object
(
 "PatternClass",
 PatternClassClass,
 0,
 0,
 "This is the class that contains the data patterns that are used in several memory tests."
 );

// Constants
static SProperty PatternClass_Stressful
(
   PatternClass_Object,
   "Stressful",
   0,
   PatternClass::PATTERNSET_STRESSFUL,
   0,
   0,
   JSPROP_READONLY,
   "CONSTANT - Stressful Patterns"
);

static SProperty PatternClass_Normal
(
   PatternClass_Object,
   "Normal",
   0,
   PatternClass::PATTERNSET_NORMAL,
   0,
   0,
   JSPROP_READONLY,
   "CONSTANT - Normal Patterns"
);

static SProperty PatternClass_All
(
   PatternClass_Object,
   "All",
   0,
   PatternClass::PATTERNSET_ALL,
   0,
   0,
   JSPROP_READONLY,
   "CONSTANT - All Patterns"
);

static SProperty PatternClass_User
(
   PatternClass_Object,
   "User",
   0,
   PatternClass::PATTERNSET_USER,
   0,
   0,
   JSPROP_READONLY,
   "CONSTANT - User Patterns"
);

C_(PatternClass_AddUserPattern);
static STest PatternClass_AddUserPattern
(
 PatternClass_Object,
 "AddUserPattern",
 C_PatternClass_AddUserPattern,
 0,
 "Adds a user pattern to the list of patterns to use."
);

C_(PatternClass_AddUserPattern)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJs;
    RC rc = OK;

    if (NumArguments == 0)
    {
        Printf(Tee::PriNormal,
               "Number of arguments to AddUserPattern() cannot be 0.\n");
        RETURN_RC( RC::ILWALID_INPUT );
    }

    if (NumArguments > 0)
    {
        // process the data pattern
        UINT32 tmpI;
        JsArray jsa;
        if (OK == pJs->FromJsval(pArguments[0], &jsa))
        {
            UINT32 *ptrn = new UINT32[jsa.size() + 1];
            UINT32 i;
            for (i = 0; i < jsa.size(); i++)
            {
                if (OK != (rc = pJs->FromJsval(jsa[i], &tmpI)))
                {
                    delete[] ptrn;
                    return rc;
                }
                ptrn[i] = tmpI;
            }
            ptrn[i] = PatternClass::END_PATTERN;

            PatternClass::PTTRN* ptr = new PatternClass::PTTRN;

            ptr->patternName = "UserPattern";
            ptr->patternAttributes = PatternClass::PATTERNSET_USER;
            ptr->thePattern = ptrn;
            if (jsa.size() == 0) // we would not want to keep empty patterns
            {
                delete[] ptrn;
                delete ptr;
            }
            else
            {
                GetAllPatternsList().push_back( ptr );
            }
        }
        else
        {
            Printf(Tee::PriNormal, "First parameter should be an array of values!");
            RETURN_RC(RC::ILWALID_INPUT);
        }
    }
    if (NumArguments > 1)
    {
        UINT32 i = UINT32(GetAllPatternsList().size());

        JSObject * pReturlwals;
        CHECK_RC(pJs->FromJsval( pArguments[1], &pReturlwals));

        if (OK != pJs->SetElement(pReturlwals, 0, i-1))
        {
            Printf(Tee::PriNormal, "Error during write into return value.");
            RETURN_RC(RC::CANNOT_SET_ELEMENT);
        }

    }
    RETURN_RC(OK);
}

C_(PatternClass_DumpAllPatterns);
static STest PatternClass_DumpAllPatterns
(
 PatternClass_Object,
 "DumpAllPatterns",
 C_PatternClass_DumpAllPatterns,
 0,
 "Prints out the patterns that exist in this class."
);

C_(PatternClass_DumpAllPatterns)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    RETURN_RC(DumpAllPatterns());
}

C_(PatternClass_DeleteUserPatterns);
static STest PatternClass_DeleteUserPatterns
(
 PatternClass_Object,
 "DeleteUserPatterns",
 C_PatternClass_DeleteUserPatterns,
 0,
 "Deletes the user defined patterns."
);

C_(PatternClass_DeleteUserPatterns)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);
    RETURN_RC(DeleteUserPatterns());
}
