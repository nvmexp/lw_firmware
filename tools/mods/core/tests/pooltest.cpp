/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// Test Fast allocation of memory

#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/tee.h"
#include "core/include/pool.h"
#include "random.h"
#include "core/include/massert.h"
#include <vector>

//
// This should match the private constant NUM_POOL_LISTS in pool.cpp.
//
const UINT32 ARRAY_SIZE = 13;

namespace PoolTest
{
   UINT32 d_TotalMemNum = 100;
   vector<void *>  d_Pointers[ARRAY_SIZE+1];

   Random::PickItem d_SizePicks[] =
   {
      PI_ONE( 5,  0),  // 1
      PI_ONE( 5,  1),  // 2
      PI_ONE( 5,  2),  // 4
      PI_ONE( 15, 3),  // 8
      PI_ONE( 15, 4),  // 16
      PI_ONE( 15, 5),  // 32
      PI_ONE( 15, 6),  // 64
      PI_ONE( 10, 7),  // 128
      PI_ONE( 5,  8),  // 256
      PI_ONE( 3,  9),  // 518
      PI_ONE( 2,  10), // 1024   (1K)
      PI_ONE( 1,  11), // 1024*2 (2K)
      PI_ONE( 1,  12), // 1024*4 (4K)
      PI_ONE( 1,  13), // more than 4K
   };
   STATIC_PI_PREP(d_SizePicks);
   Random d_Random;

   RC Alloc( UINT32 TotalNumBlock );
   RC Alloc( UINT32 SizeIndex, UINT32 NumBlock );
   RC AllocAligned();
   RC Free( UINT32 NumBlock );
   RC Free( UINT32 SizeIndex, UINT32 NumBlock );
   RC FreeRandom( UINT32 SizeIndex );
   RC FreeAll();
   void PrintAll();
   void PrintDetail(UINT32 SizeIndex);
}

RC PoolTest::Alloc( UINT32 SizeIndex, UINT32 NumBlock )
{
   UINT32 MemSize = 0;
   void * Pointer = 0;

   for (UINT32 i = 0; i< NumBlock; i++)
   {
      if ( SizeIndex == 0 )
         MemSize = 1;
      else
         MemSize = d_Random.GetRandom( (1 << (SizeIndex-1)) + 1, 1 << SizeIndex);

      Pointer = Pool::Alloc(MemSize);

      if (Pointer)
         d_Pointers[SizeIndex].push_back( Pointer );
      else
         Printf(Tee::PriHigh, "PoolTest::Alloc(): Fail @Pool.Alloc(%i)\n", MemSize);

      if (7 & (size_t)Pointer)
      {
         // supposed to be 8-byte aligned!
         Printf(Tee::PriHigh, "PoolTest::Alloc(): misalign @Pool.Alloc(%i) (%p)\n",
               MemSize,
               Pointer);
      }
   }
   return OK;
}

RC PoolTest::Alloc(UINT32 TotalNumBlock)
{
   UINT32 SizeIndex = 0;
   UINT32 MemSize = 0;
   void * Pointer = 0;
   UINT32 i = 0;

   // Allocate the total nubmer of memory
   for ( i = 0; i < TotalNumBlock; ++i)
   {
      SizeIndex = d_Random.Pick(d_SizePicks);
      if ( SizeIndex > 0)
         MemSize = d_Random.GetRandom((1 << (SizeIndex-1)) + 1, 1 << SizeIndex);
      else
         MemSize = 1;

      Pointer = Pool::Alloc(MemSize);

      if (Pointer)
         d_Pointers[SizeIndex].push_back( Pointer );
      else
         Printf(Tee::PriHigh, "PoolTest::Alloc(): Fail @Pool.Alloc(%i)\n", MemSize);

      if (7 & (size_t)Pointer)
      {
         // supposed to be 8-byte aligned!
         Printf(Tee::PriHigh, "PoolTest::Alloc(): misalign @Pool.Alloc(%i) (%p)\n",
               MemSize,
               Pointer);
      }
   }
   for ( i = 0; i< ARRAY_SIZE+1; i++)
   {
      Printf(Tee::PriNormal, "%i %d/\t", i, (UINT32)d_Pointers[i].size());
   }
   Printf(Tee::PriNormal, "\n");
   return OK;
}

RC PoolTest::AllocAligned()
{
   size_t Size;
   size_t Align;
   size_t OffsetIdx;
   void * Pointer;

   // Try sizes 1 and 4095.
   for (Size = 1; Size < 4096; Size += 4094)
   {
      // Try all valid alignment masks.
      for (Align = 1; Align <= 4096; Align *= 2)
      {
         // Try offsets 0, 1, Align/2, and Align-1.
         for (OffsetIdx = 0; OffsetIdx < 4; OffsetIdx++)
         {
            size_t Offset;
            switch (OffsetIdx)
            {
               default:
               case 0:  Offset = 0;       break;
               case 1:  Offset = 1;       break;
               case 2:  Offset = Align/2; break;
               case 3:  Offset = Align-1; break;
            }
            if (Offset >= Align)
               break;

            Pointer = Pool::AllocAligned(Size, Align, Offset);

            if (Pointer)
               d_Pointers[ARRAY_SIZE-1].push_back( Pointer );
            else
               Printf(Tee::PriHigh, "PoolTest::AllocAligned(): Fail Size,Align,Offset = %ld,%ld,%ld\n",
                     Size, Align, Offset);

            if ((((size_t)Pointer) & (Align-1)) != Offset)
            {
               Printf(Tee::PriHigh, "PoolTest::AllocAligned(): misaligned Size,Align,Offset = %ld,%ld,%ld  p=%p\n",
                     Size, Align, Offset, Pointer);
            }
         }
      }
   }
   return OK;
}

RC PoolTest::Free( UINT32 SizeIndex, UINT32 NumMem )
{
   UINT32 MaxBlock = d_Pointers[SizeIndex].size();

   if (MaxBlock == 0)
   {
      Printf(Tee::PriNormal, "No More memory to free %i\n", SizeIndex);
      return OK;
   }

   if (NumMem > MaxBlock)
   {
      Printf(Tee::PriDebug, "Invalid NumMem %i > Max, use the Max\n", NumMem);
      NumMem = MaxBlock;
   }

   for (UINT32 i = 0; i< NumMem; ++i)
   {
      Pool::Free(d_Pointers[SizeIndex][0]);
      d_Pointers[SizeIndex].erase(d_Pointers[SizeIndex].begin());
   }
   return OK;
}

RC PoolTest::Free( UINT32 NumMem )
{
   UINT32 SizeIndex = 0;
   UINT32 i = 0;

   for ( i = 0; i < NumMem; ++i)
   {
      SizeIndex = d_Random.Pick(d_SizePicks);
      FreeRandom( SizeIndex );
   }
   for ( i = 0; i < ARRAY_SIZE+1; i++)
   {
      Printf(Tee::PriNormal, "%i %d/\t", i, (UINT32)(d_Pointers[i].size()));
   }
   Printf(Tee::PriNormal, "\n");
   return OK;
}

RC PoolTest::FreeRandom( UINT32 SizeIndex )
{
   UINT32 MaxBlock = d_Pointers[SizeIndex].size();

   if (MaxBlock == 0)
   {
      Printf(Tee::PriDebug, "No More memory to free %i\n", SizeIndex);
      return OK;
   }

   UINT32 Index = d_Random.GetRandom(0, (MaxBlock-1));

   Printf(Tee::PriDebug, "FreeRandom(%i), Max = %i, Index = %i, 0x%p.\n",
          SizeIndex, (MaxBlock-1), Index, d_Pointers[SizeIndex][Index]);

   Pool::Free(d_Pointers[SizeIndex][Index]);
   d_Pointers[SizeIndex].erase( d_Pointers[SizeIndex].begin() + Index );

   return OK;
}

RC PoolTest::FreeAll()
{
   UINT32 Num;

   for ( UINT32 i = 0; i < ARRAY_SIZE+1; ++i)
   {
      Num = d_Pointers[i].size();
      for ( UINT32 j = 0; j<Num; ++j)
      {
         Pool::Free(d_Pointers[i][j]);
      }
      d_Pointers[i].clear();
   }
   return OK;
}

void PoolTest::PrintAll()
{
   Printf(Tee::PriNormal,  "PoolTest::PrintAll(): \n");
   for ( UINT32 i = 0; i<ARRAY_SIZE+1; ++i)
   {
      Printf(Tee::PriNormal,  "%i %i\t", i, (UINT32)(d_Pointers[i].size()));
   }
}

void PoolTest::PrintDetail( UINT32 SizeIndex)
{
   Printf(Tee::PriNormal,  "PoolTest::PrintDetail(%i): \n", SizeIndex);
   UINT32 Num = d_Pointers[SizeIndex].size();

   for ( UINT32 i = 0; i<Num; ++i)
   {
      Printf(Tee::PriNormal,  "0x%p\t", d_Pointers[SizeIndex][i]);
   }
   Printf(Tee::PriNormal,  "\n");
}

//------------------------------------------------------------------------------
// Pool object, methods, and tests.
//------------------------------------------------------------------------------
JS_CLASS(PoolTest);

SObject PoolTest_Object
   (
   "PoolTest",
   PoolTestClass,
   0,
   0,
   "Pool Class Test Program."
   );

// Methods and Tests

C_(PoolTest_AllocIndex);
static STest PoolTest_AllocIndex
   (
   PoolTest_Object,
   "AllocIndex",
   C_PoolTest_AllocIndex,
   2,
   "Allocate memory for given index."
   );

C_(PoolTest_Alloc);
static STest PoolTest_Alloc
   (
   PoolTest_Object,
   "Alloc",
   C_PoolTest_Alloc,
   1,
   "Allocate memory randomly."
   );

C_(PoolTest_AllocAligned);
static STest PoolTest_AllocAligned
   (
   PoolTest_Object,
   "AllocAligned",
   C_PoolTest_AllocAligned,
   0,
   "Exercise AllocAligned."
   );

C_(PoolTest_FreeOneRandom);
static STest PoolTest_FreeOneRandom
   (
   PoolTest_Object,
   "FreeOneRandom",
   C_PoolTest_FreeOneRandom,
   1,
   "Free one memory Randomly fron the given SizeIndex."
   );

C_(PoolTest_FreeIndexRandom);
static STest PoolTest_FreeIndexRandom
   (
   PoolTest_Object,
   "FreeIndexRandom",
   C_PoolTest_FreeIndexRandom,
   2,
   "Free several memory Randomly from given SizeIndex."
   );

C_(PoolTest_FreeIndex);
static STest PoolTest_FreeIndex
   (
   PoolTest_Object,
   "FreeIndex",
   C_PoolTest_FreeIndex,
   2,
   "Free memory allocated by allocation time."
   );

C_(PoolTest_Free);
static STest PoolTest_Free
   (
   PoolTest_Object,
   "Free",
   C_PoolTest_Free,
   1,
   "Free memory randomly allocated with PoolTest.Alloc()."
   );

C_(PoolTest_FreeAll);
static STest PoolTest_FreeAll
   (
   PoolTest_Object,
   "FreeAll",
   C_PoolTest_FreeAll,
   0,
   "Free All remained memory allocated with PoolTest.Alloc()."
   );

C_(PoolTest_PrintDetail);
static STest PoolTest_PrintDetail
   (
   PoolTest_Object,
   "PrintDetail",
   C_PoolTest_PrintDetail,
   1,
   "Print the detail about the pools of the given index."
   );

C_(PoolTest_PrintAll);
static STest PoolTest_PrintAll
   (
   PoolTest_Object,
   "PrintAll",
   C_PoolTest_PrintAll,
   0,
   "Print the memory blocks for all range of memory."
   );

// Implementation
C_(PoolTest_AllocIndex)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;
   UINT32 SizeIndex = 0;
   UINT32 Size = 0;

   if ( (NumArguments != 2)
        || (OK != pJavaScript->FromJsval(pArguments[0], &SizeIndex) )
        || (OK != pJavaScript->FromJsval(pArguments[1], &Size) )
      )
   {
      JS_ReportError(pContext, "Usage: PoolTest.AllocIndex(Index, NumOfMem)");
      return JS_FALSE;
   }

   RETURN_RC( PoolTest::Alloc(SizeIndex, Size) );
}

C_(PoolTest_Alloc)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;
   UINT32 Size = 0;

   // Check the arguments.
   if ( (NumArguments != 1)
        || (OK != pJavaScript->FromJsval(pArguments[0], &Size) )
      )
   {
      JS_ReportError(pContext, "Usage: PoolTest.Alloc(NumOfMem)");
      return JS_FALSE;
   }

   RETURN_RC( PoolTest::Alloc(Size) );
}

C_(PoolTest_AllocAligned)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // Check the arguments.
   if (NumArguments != 0)
   {
      JS_ReportError(pContext, "Usage: PoolTest.AllocAligned()");
      return JS_FALSE;
   }

   RETURN_RC( PoolTest::AllocAligned() );
}

C_(PoolTest_FreeOneRandom)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;
   UINT32 SizeIndex = 0;

   // Check the arguments.
   if ( (NumArguments != 1)
        || (OK != pJavaScript->FromJsval(pArguments[0], &SizeIndex) )
      )
   {
      JS_ReportError(pContext, "Usage: PoolTest.FreeOneRandom(Index)");
      return JS_FALSE;
   }

   RETURN_RC( PoolTest::FreeRandom(SizeIndex) );
}

C_(PoolTest_FreeIndexRandom)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;
   UINT32 SizeIndex = 0;
   UINT32 Num = 0;

   // Check the arguments.
   if ( (NumArguments != 2)
        || (OK != pJavaScript->FromJsval(pArguments[0], &SizeIndex))
        || (OK != pJavaScript->FromJsval(pArguments[1], &Num)) )
   {
      JS_ReportError(pContext, "Usage: PoolTest.FreeIndexRandom(Index, Num)");
      return JS_FALSE;
   }

   RC rc;
   for ( UINT32 i = 0; i<Num; i++)
   {
      if ( OK != (rc = PoolTest::FreeRandom(SizeIndex)))
         RETURN_RC(rc);
   }
   RETURN_RC(OK);
}

C_(PoolTest_FreeIndex)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;
   UINT32 SizeIndex = 0;
   UINT32 Size = 0;

   if ( (NumArguments != 2)
        || (OK != pJavaScript->FromJsval(pArguments[0], &SizeIndex) )
        || (OK != pJavaScript->FromJsval(pArguments[1], &Size) )
      )
   {
      JS_ReportError(pContext, "Usage: PoolTest.FreeIndex(Index, NumOfMem)");
      return JS_FALSE;
   }

   RETURN_RC( PoolTest::Free(SizeIndex, Size) );
}

C_(PoolTest_Free)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;
   UINT32 Size = 0;

   // Check the arguments.
   if ( (NumArguments != 1)
        || (OK != pJavaScript->FromJsval(pArguments[0], &Size) )
      )
   {
      JS_ReportError(pContext, "Usage: PoolTest.Free(NumOfMem)");
      return JS_FALSE;
   }

   RETURN_RC( PoolTest::Free(Size) );
}

C_(PoolTest_FreeAll)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // Check the arguments.
   if (NumArguments != 0)
   {
      JS_ReportError(pContext, "Usage: PoolTest.FreeAll()");
      return JS_FALSE;
   }
   RETURN_RC( PoolTest::FreeAll() );
}

C_(PoolTest_PrintDetail)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;

   // Check the arguments.
   UINT32 Index = 0;
   if ( (NumArguments != 1)
        || (OK != pJavaScript->FromJsval(pArguments[0], &Index)) )
   {
      JS_ReportError(pContext, "Usage: PoolTest.PrintDetail(Index)");
      return JS_FALSE;
   }
   PoolTest::PrintDetail(Index);
   RETURN_RC(OK);
}

C_(PoolTest_PrintAll)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // Check the arguments.
   if ( NumArguments != 0 )
   {
      JS_ReportError(pContext, "Usage: PoolTest.PrintAll()");
      return JS_FALSE;
   }
   PoolTest::PrintAll();
   RETURN_RC(OK);
}

