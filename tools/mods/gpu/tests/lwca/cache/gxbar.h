/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <stdint.h>
#include "../tools/tools.h"

using DataType = UINT32;

struct GXbarParams
{
    device_ptr routingPtr;
    device_ptr launchCountPtr;
    device_ptr errorCountPtr;
    device_ptr errorLogPtr;
    device_ptr accValPtr;
    device_ptr patternsPtr;
    UINT64   iterations;
    UINT32   bytesPerCta;
    UINT32   numClusters;
    UINT32   clusterIdx;
    UINT32   maxClusterSize;
    UINT32   errorLogLen;
    UINT32   verifyResults;
    UINT32   randSeed;
    UINT32   numPatterns;
    UINT32   useRandomData;
    UINT32   usePulse;
    UINT32   periodNs;
    UINT32   pulseNs;
};

struct GXbarError
{
    DataType readVal;
    DataType expectedVal;
    UINT64 iteration;
    UINT32 innerLoop;
    UINT32 smid;
    UINT32 warpid;
    UINT32 laneid;
};
