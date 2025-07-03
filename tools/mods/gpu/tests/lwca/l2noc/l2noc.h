/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <stdint.h>
#include "../tools/tools.h"

#ifndef LWDAL2NOC_H_INCLUDED
#define LWDAL2NOC_H_INCLUDED

#define UGPU_INDEX 31
#define UGPU_MASK  (1 << UGPU_INDEX)

#define ILWALID_UGPU_SMID 0xFFFFFFFF

// This is taken from LwdaXbar test, as there is no current guidance for L2 stress patterns
#define MAX_PATS 8

struct WritePatterns
{
    unsigned p[MAX_PATS];
};

struct L2NocParams
{
    device_ptr  dataPtr;
    device_ptr  smidMap;
    device_ptr  uGpu0ChunkArr;
    device_ptr  uGpu1ChunkArr;
    UINT32 chunksPerSMUGpu0;
    UINT32 chunksPerSMUGpu1;
    UINT32 dwordPerChunk;
    UINT32 innerIterations;
    WritePatterns patterns;
    device_ptr  errorPtr;
    device_ptr  numErrorsPtr;
    UINT64  maxErrors;
    UINT32  readVerify;
};

#pragma pack(1)
struct L2NocBadValue
{
#ifndef __LWDACC__
    L2NocBadValue() 
        : offset(0)
        , expected(0)
        , actual(0)
        , failingBits(0)
        , smid(0)        
        , thread(0)
        , block(0)
        { }
#endif
    UINT64 offset;
    UINT32 expected;
    UINT32 actual;
    UINT32 failingBits;
    UINT32 smid;
    UINT16 thread;
    UINT08 block;
    UINT08 __align;
    UINT32 __align1;
};
#pragma pack()

#endif // LWDAL2NOC_H_INCLUDED
