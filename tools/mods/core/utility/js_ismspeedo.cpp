/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  gpu\js_const.cpp
 * @brief Define JavaScript constants for the IsmSpeedo names
 *
 */

#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/ismspeedo.h"
//-----------------------------------------------------------------------------
JS_CLASS(IsmSpeedoConst);
static SObject IsmSpeedoConst_Object
(
    "IsmSpeedoConst",
    IsmSpeedoConstClass,
    0,
    0,
    "IsmSpeedoConst JS Object"
);

PROP_CONST(IsmSpeedoConst, COMP,         IsmSpeedo::COMP);
PROP_CONST(IsmSpeedoConst, BIN,          IsmSpeedo::BIN);
PROP_CONST(IsmSpeedoConst, METAL,        IsmSpeedo::METAL);
PROP_CONST(IsmSpeedoConst, MINI,         IsmSpeedo::MINI);
PROP_CONST(IsmSpeedoConst, TSOSC,        IsmSpeedo::TSOSC);
PROP_CONST(IsmSpeedoConst, VNSENSE,      IsmSpeedo::VNSENSE);
PROP_CONST(IsmSpeedoConst, NMEAS,        IsmSpeedo::NMEAS);
PROP_CONST(IsmSpeedoConst, HOLD,         IsmSpeedo::HOLD);
PROP_CONST(IsmSpeedoConst, AGING,        IsmSpeedo::AGING);
PROP_CONST(IsmSpeedoConst, BIN_AGING,    IsmSpeedo::BIN_AGING);
PROP_CONST(IsmSpeedoConst, AGING2,       IsmSpeedo::AGING2);
PROP_CONST(IsmSpeedoConst, IMON,         IsmSpeedo::IMON);
PROP_CONST(IsmSpeedoConst, ALL,          IsmSpeedo::ALL);

