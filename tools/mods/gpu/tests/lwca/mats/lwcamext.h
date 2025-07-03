/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "../../matshelp.h"
#include "../tools/tools.h"

#ifdef __LWDACC__
#define UTILITY_FUNC __device__
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#else
#define UTILITY_FUNC inline
#endif // __LWDACC__

struct CommandData
{
    UINT64 value;
    OperationType op_type;
    OperationMode op_mode;
    OperationSize op_size;
    UINT32 increment;
    UINT32 runflag;
    UINT32 _reserved0;
};

struct BoxData
{
    UINT64 offset;
};

struct ErrorDescriptor
{
    UINT64 address;     // physical address
    UINT32 bitcount;    // size of operation (8/16/32/64)
    UINT32 actual;      // incorrect value read from memory
    UINT32 expected;    // value that we expected to read
    UINT32 read1;       // reread attempt #1
    UINT32 read2;       // reread attempt #2
    UINT32 checksum;    // checksum of preceding values
};

struct ThreadErrors
{
    UINT32 reads;
    UINT32 writes;
    UINT32 errors;
    UINT32 _reserved0;  // alignment
};

struct KernelInput
{
    device_ptr output;    // pointer to thread result storage
    device_ptr mem_min;   // starting memory address (actual)
    device_ptr mem_max;   // ending memory address (actual)
    device_ptr mem_start; // starting memory address (aligned)
    device_ptr mem_end;   // ending memory address (aligned)
    UINT32 chunksize;     // amount of memory for each thread to process
    UINT32 commands;      // number of commands in the current sequence
    UINT32 runflags;      // run flags for the current sequence
    UINT32 maxerrors;     // maximum number of errors to record per thread
    UINT32 resx;          // display resolution (box test)
    UINT32 boxcount;      // number of boxes to test (box test)
    UINT32 memsize;       // size, in bytes, of each op of this sequence
    UINT32 ascending;     // memory direction
    UINT32 useLdg;        // Use LDG on first read at address
};

UTILITY_FUNC UINT32 Checksum(UINT08 *buf, UINT32 length)
{
    UINT32 checksum = 0;
    for (UINT32 i = 0; i < length; ++i)
        checksum += buf[i];
    return checksum;
}

static const UINT32 BOX_WIDTH = 16;
static const UINT32 BOX_HEIGHT = 16;
static const UINT32 CONST_MAX_COMMAND = 128;
static const UINT32 CONST_MAX_BOXES = 7168;
static const UINT32 CHECKSUM_SIZE = sizeof(ErrorDescriptor) - sizeof(UINT32);
