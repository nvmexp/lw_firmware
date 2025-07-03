/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/types.h"
#include "hwref/hopper/gh100/dev_c2c.h"

#define C2C_DIAG_DRIVER_GH100_MAX_C2C_PARTITIONS        2
#define C2C_DIAG_DRIVER_GH100_MAX_LINKS_PER_PARTITION   5

//--------------------------------------------------------------------
//! \brief Class containing C2C regs offsets for GH100
//!
class Gh100C2cDiagRegs
{
public:
    // NOTE: TH500 is also using GH100 register offsets. If the offsets differ for
    // a device then a new API GetRegisterOffset will be required. This should be called
    // to get proper offset for each device.
    static const LwU32 s_C2CLinkStatusRegs[C2C_DIAG_DRIVER_GH100_MAX_C2C_PARTITIONS
                                * C2C_DIAG_DRIVER_GH100_MAX_LINKS_PER_PARTITION];
    static const LwU32 s_C2CLinkFreqRegs[C2C_DIAG_DRIVER_GH100_MAX_C2C_PARTITIONS];
    static const LwU32 s_C2CLinkFreqRegsLink0;
    static const LwU32 s_C2CCrcCntRegs[C2C_DIAG_DRIVER_GH100_MAX_C2C_PARTITIONS
                                * C2C_DIAG_DRIVER_GH100_MAX_LINKS_PER_PARTITION];
    static const LwU32 s_C2CReplayCntRegs[C2C_DIAG_DRIVER_GH100_MAX_C2C_PARTITIONS
                                * C2C_DIAG_DRIVER_GH100_MAX_LINKS_PER_PARTITION];
};
