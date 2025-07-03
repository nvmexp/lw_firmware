/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_lwlinktests.h
//! \RM-Test to test functionalities of LWLink.
//!

#include "gpu/tests/rmtest.h"
#include "gpu/include/gpusbdev.h"
#include "ctrl/ctrl2080/ctrl2080lwlink.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "core/include/massert.h"
#include "core/include/script.h"
#include "core/include/jscript.h"

#define LWLINK_MAX_NUM_LINK_STATES        7

const char *linkStateNames[LWLINK_MAX_NUM_LINK_STATES] =
{
    "Init",
    "HWCFG",
    "Safe",
    "Active",
    "Fault",
    "Recovery",
    "Invalid"
};

#define LWLINK_MAX_NUM_SUBLINK_STATES     16

const char *sublinkStateNames[LWLINK_MAX_NUM_SUBLINK_STATES] =
{
    "High Speed",
    "",
    "",
    "",
    "",
    "Training",
    "Safe Mode",
    "Off",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "Invalid"
};

#define LWLINK_MAX_PHY_TYPES             3

const char *phyTypeNames[LWLINK_MAX_PHY_TYPES] =
{
    "Invalid",
    "LWHS",
    "GRS"
};

