/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef CRC_H__
#define CRC_H__

#ifndef TYPES_H__
#include "types.h"
#endif

#ifndef INCLUDED_COLOR_H
#include "color.h"
#endif

#include <vector>
//------------------------------------------------------------------------------
//
// byte-by-byte 32-bit Cyclic Redundancy Check
//
// Note: not big-endian/little-endian clean.
//
class Crc
{
public:
   Crc();

   //While computing CRC on GPU, the buffer is divided into these many blocks
   //so that the shader/kernel can compute CRC in parallel.
   static UINT32 s_NumCRCBlocks;

   static void GetCRCTable(vector<UINT32> *crcTable);
   static UINT64 TableSizeBytes();
   static UINT32 Callwlate(const void * Address, UINT32 Width, UINT32 Height,
      UINT32 Depth, UINT32 Pitch);
      // Callwlate a CRC-32 of a buffer.

   static UINT32 Append(const void * Address, UINT32 Length, UINT32 PrevCrc);
      // Add more data to a crc. Useful for non-video, noncontiguous data.

   static void Callwlate(const void * Address, UINT32 Width, UINT32 Height,
      ColorUtils::Format Fmt, UINT32 Pitch, UINT32 NumBins, UINT32 * BinArray);
      // Callwlate NumBins pixel-interleaved CRC-32s on buffer.
};
// Psuedo-random number generator based on the CRC table.  Modifies *Seed.
void RndFromCrc(UINT32 * Seed);

// Support CRC32C (Castagnoli) variant implemented in CPU hw with SSE 4.2
// Software implementation fallback is provided
namespace Crc32c
{
    extern UINT32 s_Crc32cTable[256];
    void GenerateCrc32cTable();
    // Software implementation of the SSE 4.2 crc instruction:
    UINT32 StepSw(UINT32 crc, UINT32 data);
    bool IsHwCrcSupported();
    UINT32 Callwlate(const void * address, UINT32 widthInBytes,
        UINT32 height, UINT64 pitch);
};
#if defined(_M_IX86) || defined(_M_X64) || defined(__amd64) || defined(__i386)
#include <nmmintrin.h>
#define CRC32C4 _mm_crc32_u32
#elif defined(LWCPU_AARCH64)
#include <arm_acle.h>
#define CRC32C4 __crc32cw
#else
#define CRC32C4 Crc32c::StepSw
#endif

//------------------------------------------------------------------------------
//
// byte-by-byte 32-bit Cyclic Redundancy Check
//
// Note: not big-endian/little-endian clean.
//
// This is just like Crc, but uses the same algorithm as the old HW diags.
// Use this to run traces to compare against emulation golden values.
//
class OldCrc
{
public:
   OldCrc();

   static UINT32 Callwlate(void * Address, UINT32 Width, UINT32 Height,
      UINT32 Depth, UINT32 Pitch);
      // Callwlate a CRC-32 of a buffer.

   static UINT32 Append(void * Address, UINT32 Length, UINT32 PrevCrc);
      // Add more data to a crc. Useful for non-video, noncontiguous data.

   static void Callwlate(void * Address, UINT32 Width, UINT32 Height,
      ColorUtils::Format Fmt, UINT32 Pitch, UINT32 NumBins, UINT32 * BinArray);
      // Callwlate NumBins pixel-interleaved CRC-32s of a buffer.
};

//------------------------------------------------------------------------------
//
// byte-by-byte Check Sum.
//
// Note: not big-endian/little-endian clean.
//
class CheckSum
{
public:
   CheckSum();

   static UINT32 Callwlate(void * Address, UINT32 Width, UINT32 Height,
      UINT32 Depth, UINT32 Pitch);
      // Callwlate a checksum of a buffer.

   static void Callwlate(void * Address, UINT32 Width, UINT32 Height,
      ColorUtils::Format Fmt, UINT32 Pitch, UINT32 NumBins, UINT32 * BinArray);
      // Callwlate NumBins pixel-interleaved checksums of a buffer.
};

//------------------------------------------------------------------------------
//
// Component-wise Check Sum.
//
//
class CheckSums
{
public:
   CheckSums();

   static void Callwlate
   (
      void * Address,
      UINT32 Width,
      UINT32 Height,
      ColorUtils::Format Fmt,
      UINT32 Pitch,
      UINT32 NumBins,
      UINT32 * SumsArray
   );
};

#endif // !CRC_H__
