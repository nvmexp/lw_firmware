/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2009,2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#ifndef INCLUDED_PATTERN_H
#define INCLUDED_PATTERN_H
#include "types.h"
#include "memerror.h"
#include <boost/smart_ptr/make_shared.hpp>

// Data and Memory related prototypes
namespace Pattern
{

   // used to store a large noncontiguous amount of memory
   const INT32  NUMBUFS = 20;
   struct BufferSpace       // stuct for holding multiple
   {
      UINT32 *StartAddress[NUMBUFS];   //blocks of mem
      UINT32 *BufferAddress[NUMBUFS];
      UINT32 BufferSize[NUMBUFS];
   };

   struct DataPattern
   {
      UINT32   Length;
      UINT32 * Data;
   };

   // Data Pattern related utility functions
   RC AllocateAllMem(BufferSpace * Bufs, UINT32 MBs, UINT32 FreeMB);
   RC FreeAllMem(BufferSpace * Bufs);
   UINT32 SizeAllMem(BufferSpace * Bufs, INT32 Priority);
   RC GenRandPat(UINT32 seed);
   RC GenAciPat(UINT16 * data, UINT32 size);
   RC FillPattern(UINT32 * dst, DataPattern * src, UINT32 length);

   // Data Patterns for use by diags
   extern DataPattern patA;
   extern DataPattern pat0;
   extern DataPattern patWalking1;
   extern DataPattern pat01;
   extern DataPattern patTripleWhammy;
   extern DataPattern patIsolatedOnes;
   extern DataPattern patIsolatedZeros;
   extern DataPattern patSlowFalling;
   extern DataPattern patSlowRising;
   extern DataPattern patRand;

   extern DataPattern * patList[];

}

#endif
