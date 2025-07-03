/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2010 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "glcombust.h"

//------------------------------------------------------------------------------
//! Defeatured (quiet) version of GLCombust.
//------------------------------------------------------------------------------
class PowerTest : public GLCombustTest
{
public:
    PowerTest() { SetName("Powertest"); }
    ~PowerTest() {}

    virtual void PrintJsProperties(Tee::Priority) { /*do nothing*/ }
    virtual void ReportErrors(UINT32) { /*do nothing*/ }
};

JS_CLASS_INHERIT_FULL(PowerTest, GpuTest, "Power stress test", 0);

CLASS_PROP_READWRITE(PowerTest, LoopMs, UINT32,
        "Minimum test duration in milliseconds.");

CLASS_PROP_READWRITE(PowerTest, KeepRunning, bool,
        "While this is true, Run will continue even beyond Loops.");

CLASS_PROP_READWRITE(PowerTest, ExtraInstr, UINT32,
        "Idempotent instruction segments to append to fragment shader.");

CLASS_PROP_READWRITE(PowerTest, ExtraTex, UINT32,
        "Extra Textures to use.");

CLASS_PROP_READWRITE(PowerTest, ScreenWidth, UINT32,
        ".");

CLASS_PROP_READWRITE(PowerTest, ScreenHeight, UINT32,
        ".");

CLASS_PROP_READWRITE(PowerTest, NumXSeg, UINT32,
        ".");

CLASS_PROP_READWRITE(PowerTest, NumTSeg, UINT32,
        ".");

CLASS_PROP_READWRITE(PowerTest, TorusTubeDia, FLOAT64,
        ".");

CLASS_PROP_READWRITE(PowerTest, TorusHoleDia, FLOAT64,
        ".");

CLASS_PROP_READWRITE(PowerTest, EnableLwll, bool,
        ".");

CLASS_PROP_READWRITE(PowerTest, BlendMode,  UINT32,
        ".0:none 1:additive 2:xor ");

CLASS_PROP_READWRITE(PowerTest, ShaderHashMode, UINT32,
        ".0:no hash 1:mod 257 hash");

CLASS_PROP_READWRITE(PowerTest, DepthTest, bool,
        ".");

CLASS_PROP_READWRITE(PowerTest, UseMM, bool,
        ".");

CLASS_PROP_READWRITE(PowerTest, PixelErrorTol, UINT32,
        ".");

