/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef LWDAXBAR_H_INCLUDED
#define LWDAXBAR_H_INCLUDED

enum XbarModeMask
{
    XBAR_MODE_RD        = 0x1 << 0, // Reads
    XBAR_MODE_WR        = 0x1 << 1, // Writes
    XBAR_MODE_PM        = 0x1 << 2, // Pin Mapping
    XBAR_MODE_L2        = 0x1 << 3, // Use L2 Slice Patterns during read mode
    XBAR_MODE_ILW       = 0x1 << 4, // Instead of flipping enabled bits to 0, ilwert all bits
    XBAR_MODE_RD_CMP    = 0x1 << 5, // Compare the data from reads to expected value
};

// This reprents the number of DWORDS that fit on the XbarBar bus, which is not expected to change
// from its current size of 256 bits
#define MAX_XBAR_PATS 8
struct XbarPatterns
{
    unsigned p[MAX_XBAR_PATS];
};

enum XbarSwitchingMode
{
    XBAR_SWITCHING_MODE_ZEROES = 0, // Set all bits to 0's
    XBAR_SWITCHING_MODE_ONES   = 1, // Set all bits to 1's
    XBAR_SWITCHING_MODE_ILW    = 2, // Ilwert all bits
};

#include "../tools/tools.h"

#pragma pack(1)
struct XbarBadValue
{
#ifndef __LWDACC__
    XbarBadValue() 
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

struct XbarRangeErrors
{
#ifndef __LWDACC__
    XbarRangeErrors()
        : numErrors(0)
        , numReportedErrors(0)
        { }
#endif

    UINT64 numErrors;
    UINT32 numReportedErrors;
    UINT32 dummy; // for alignment
    XbarBadValue reportedValues[1];
};
#pragma pack()

#endif // LWDAXBAR_H_INCLUDED
