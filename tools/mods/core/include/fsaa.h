/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2003-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// Full Scene Anti-Aliasing definitions and utilities.

#ifndef INCLUDED_FSAA_H
#define INCLUDED_FSAA_H

enum FSAAMode
{
    FSAADisabled        // no Full Screen Anti-Aliasing
   ,FSAA2x              //
   ,FSAA2xDac           // 2x, downfilter in DAC (lw17/25 or later)
   ,FSAA2xQuinlwnx      // 2x, quinlwnx filter method (lw 20/17 or later)
   ,FSAA2xQuinlwnxDac   // 2x, quinlwnx, downfilter in DAC (lw17/25 or later)
   ,FSAA4x              //
   ,FSAA4xGaussian      // 4x, 9-input filter method (lw20 or later)
   ,FSAA4xDac           // 4x, filter in DAC (lw35 or later)
   ,FSAA8x              //
   ,FSAA16x             //
   ,FSAA4v4             //
   ,FSAA4v12            //
   ,FSAA8v8             //
   ,FSAA8v24            //

   ,FSAA_NUM_MODES
};

const char * FsaaName (FSAAMode fm);
bool FsaaModeIsFOS (FSAAMode fm);

#endif // INCLUDED_FSAA_H
