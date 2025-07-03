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

#include "core/include/fsaa.h"
#include "core/include/script.h"
#include "core/include/jscript.h"

//------------------------------------------------------------------------------
const char * FsaaName (FSAAMode fm)
{
   const char * fsaaStrings[FSAA_NUM_MODES] =
   {
       "Disabled"
      ,"2x"
      ,"2xDac"
      ,"2xQuinlwnx"
      ,"2xQuinlwnxDac"
      ,"4x"
      ,"4xGaussian"
      ,"4xDac"
      ,"8x"
      ,"16x"
      ,"4v4"
      ,"4v12"
      ,"8v8"
      ,"8v24"
   };
   if (fm < FSAA_NUM_MODES)
      return fsaaStrings[fm];
   else
      return "FSAAModeUnknown";
}

//! Return true if this Full-Scene-Anti-Aliasing mode is one that
//! is implemented by Filter-On-Scanout i.e. downfiltering in EVO display HW.
//!
//! Most FSAA modes have an offscreen rendering surface and a separate
//! display surface that should not be compressed.  The 3d engine is used for
//! the downfilter-colwersion during copy to the display surface.
//!
//! Surface layout is different for FOS modes -- the target rendering surface
//! and the display surface are the same, and should be compressed.
//!
bool FsaaModeIsFOS (FSAAMode fm)
{
    const bool fsaaIsFos[FSAA_NUM_MODES] =
    {
         false  // FSAADisabled
        ,false  // FSAA2x
        ,true   // FSAA2xDac
        ,false  // FSAA2xQuinlwnx
        ,true   // FSAA2xQuinlwnxDac
        ,false  // FSAA4x
        ,false  // FSAA4xGaussian
        ,true   // FSAA4xDac
        ,false  // FSAA8x
        ,false  // FSAA16x
        ,false  // FSAA4v4
        ,false  // FSAA4v12
        ,false  // FSAA8v8
        ,false  // FSAA8v24
    };
    if (fm < FSAA_NUM_MODES)
        return fsaaIsFos[fm];
    else
        return false;
}

//!
static SProperty Global_FSAADisabled
(
   "FSAADisabled",
   0,
   FSAADisabled,
   0,
   0,
   JSPROP_READONLY,
   "no FSAA"
);
//!
static SProperty Global_FSAA2x
(
   "FSAA2x",
   0,
   FSAA2x,
   0,
   0,
   JSPROP_READONLY,
   "2x FSAA"
);
//!
static SProperty Global_FSAA2xDac
(
   "FSAA2xDac",
   0,
   FSAA2xDac,
   0,
   0,
   JSPROP_READONLY,
   "2x FSAA, downsample in DAC"
);
//!
static SProperty Global_FSAA2xQuinlwnx
(
   "FSAA2xQuinlwnx",
   0,
   FSAA2xQuinlwnx,
   0,
   0,
   JSPROP_READONLY,
   "2x FSAA, 5-input downsample filter"
);
//!
static SProperty Global_FSAA2xQuinlwnxDac
(
   "FSAA2xQuinlwnxDac",
   0,
   FSAA2xQuinlwnxDac,
   0,
   0,
   JSPROP_READONLY,
   "2x FSAA, 5-input downsample filter in DAC"
);
//!
static SProperty Global_FSAA4x
(
   "FSAA4x",
   0,
   FSAA4x,
   0,
   0,
   JSPROP_READONLY,
   "4x FSAA"
);
//!
static SProperty Global_FSAA4xGaussian
(
   "FSAA4xGaussian",
   0,
   FSAA4xGaussian,
   0,
   0,
   JSPROP_READONLY,
   "4x FSAA, 9-input downsample filter"
);
//!
static SProperty Global_FSAA4xDac
(
   "FSAA4xDac",
   0,
   FSAA4xDac,
   0,
   0,
   JSPROP_READONLY,
   "4x FSAA, downsample in DAC"
);
//!
static SProperty Global_FSAA8x
(
   "FSAA8x",
   0,
   FSAA8x,
   0,
   0,
   JSPROP_READONLY,
   "8x FSAA"
);
//!
static SProperty Global_FSAA16x
(
   "FSAA16x",
   0,
   FSAA16x,
   0,
   0,
   JSPROP_READONLY,
   "16x FSAA"
);

//!
static SProperty Global_FSAA4v4
(
   "FSAA4v4",
   0,
   FSAA4v4,
   0,
   0,
   JSPROP_READONLY,
   "VCAA 4v4"
);

//!
static SProperty Global_FSAA4v12
(
   "FSAA4v12",
   0,
   FSAA4v12,
   0,
   0,
   JSPROP_READONLY,
   "VCAA 4v12"
);

//!
static SProperty Global_FSAA8v8
(
   "FSAA8v8",
   0,
   FSAA8v8,
   0,
   0,
   JSPROP_READONLY,
   "VCAA 8v8"
);

//!
static SProperty Global_FSAA8v24
(
   "FSAA8v24",
   0,
   FSAA8v24,
   0,
   0,
   JSPROP_READONLY,
   "VCAA 8v24"
);

