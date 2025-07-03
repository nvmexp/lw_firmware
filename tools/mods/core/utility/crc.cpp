/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// Cyclic redundancy check (CRC).

#include "core/include/crc.h"
#include "core/include/cpu.h"
#include "core/include/jscript.h"
#include "core/include/massert.h"
#include "core/include/mods_profile.h"
#include "core/include/platform.h"
#include "core/include/script.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include <string.h>

#if defined(LWCPU_AARCH64) && !defined(BIONIC) // Android toolchain does not have it yet
#include <arm_acle.h>
#endif

//------------------------------------------------------------------------------
// Crc
//------------------------------------------------------------------------------

const UINT32 CRC_TABLE_SIZE = 256;
const UINT32 POLYNOMIAL     = 0x04C11DB7;

static UINT32 CrcTable[CRC_TABLE_SIZE];
static bool   IsCrcTableCreated = false;

static Crc     SomeStaticCrc;

// Ideally s_NumCRCBlocks (also equals total threads launched in Lwca) should be
// (warp count * SM count). Using 1024 to prevent any under utilization in GM20x
UINT32 Crc::s_NumCRCBlocks = 1024;

Crc::Crc()
{
   if (!IsCrcTableCreated)
   {
      // Compute the CRC-32 remainder table.

      // Compute the remainder of each possible dividend.
      for (UINT32 Dividend = 0; Dividend < CRC_TABLE_SIZE; ++Dividend)
      {
         UINT32 Remainder = Dividend << 24;

         // Perform a modulo-2 division, a bit at a time.
         for (UINT08 Bit = 8; Bit > 0; --Bit)
         {
            if (Remainder & 0x80000000)
            {
               Remainder = (Remainder << 1) ^ POLYNOMIAL;
            }
            else
            {
               Remainder = (Remainder << 1);
            }
         }

         CrcTable[Dividend] = Remainder;
      }
      IsCrcTableCreated = true;
   }
}

void Crc::GetCRCTable(vector<UINT32> *crcTable)
{
    crcTable->assign(&CrcTable[0], &CrcTable[0] + NUMELEMS(CrcTable));
}

UINT64 Crc::TableSizeBytes()
{
    return static_cast<UINT64>(sizeof(CrcTable));
}

UINT32 Crc::Callwlate
(
   const void * Address,
   UINT32 Width,
   UINT32 Height,
   UINT32 Depth,
   UINT32 Pitch
)
{
   MASSERT(Address != 0);

   if ((Depth != 16) && (Depth != 32))
   {
       MASSERT(!"Unsupported depth");
       Utility::ExitMods(RC::SOFTWARE_ERROR, Utility::ExitQuickAndDirty);
       return 0;
   }

   PROFILE(CRC);

   Tasker::DetachThread detach;

   const UINT08 * Row    = static_cast<const UINT08*>(Address);
   const UINT32 * Column = static_cast<const UINT32*>(Address);

   UINT32 X, Y;
   UINT32 Data;

   // We will read 32 bits at a time for faster access,
   // therefore the Width must be scaled accordingly.
   if (16 == Depth)
   {
      MASSERT((Width % 2) == 0);
      Width /= 2;
   }

   UINT32 Remainder = 0xFFFFFFFF;

   // Divide the data by the polynomial one byte at a time.
   for (Y = 0; Y < Height; ++Y)
   {
      Column = reinterpret_cast<const UINT32*>(Row);

      for (X = 0; X < Width; ++X)
      {
         // Read 32 bits at a time for faster access.
         Data = MEM_RD32(Column);
         Column++;

         Remainder = CrcTable[((Data >>  0) & 0xFF) ^ (Remainder >> 24)]
            ^(Remainder << 8);
         Remainder = CrcTable[((Data >>  8) & 0xFF) ^ (Remainder >> 24)]
            ^(Remainder << 8);
         Remainder = CrcTable[((Data >> 16) & 0xFF) ^ (Remainder >> 24)]
            ^(Remainder << 8);
         Remainder = CrcTable[((Data >> 24) & 0xFF) ^ (Remainder >> 24)]
            ^(Remainder << 8);
      }

      Row += Pitch;
   }

   return ~Remainder;
}

// Callwlate CRC of multiple noncontiguous buffers
// NULL Crc to start, carry over crc to continue
UINT32 Crc::Append
(
   const void * Address,
   UINT32 Length,
   UINT32 PrevCrc
)
{
   MASSERT(Address != 0);

   PROFILE(CRC);

   UINT32 Remainder = ~PrevCrc;
   UINT32 i;

   // Process any leading unaligned bytes.
   size_t PreBytes = (4 - ((size_t)Address & 0x3)) & 0x3;
   if (PreBytes > Length)
      PreBytes = Length;
   const UINT08 * PreByteBuffer = (const UINT08*)Address;

   for (i = 0; i < PreBytes; i++)
   {
      Remainder = CrcTable[PreByteBuffer[i] ^ (Remainder >> 24)]
         ^(Remainder << 8);
   }

   // We will read 32 bits at a time for faster access,
   // therefore the Width must be scaled accordingly.
   size_t Words = (Length - PreBytes) / 4;
   const UINT32 * WordBuffer = (const UINT32*)((const UINT08*)Address + PreBytes);
   UINT32 Data;

   // Divide the data by the polynomial one byte at a time.
   for (i = 0; i < Words; i++)
   {
      // Read 32 bits at a time for faster access.
      Data = WordBuffer[i];

      Remainder = CrcTable[((Data >>  0) & 0xFF) ^ (Remainder >> 24)]
         ^(Remainder << 8);
      Remainder = CrcTable[((Data >>  8) & 0xFF) ^ (Remainder >> 24)]
         ^(Remainder << 8);
      Remainder = CrcTable[((Data >> 16) & 0xFF) ^ (Remainder >> 24)]
         ^(Remainder << 8);
      Remainder = CrcTable[((Data >> 24) & 0xFF) ^ (Remainder >> 24)]
         ^(Remainder << 8);
   }

   // Process any trailing unaligned bytes.
   size_t PostBytes = Length - PreBytes - 4 * Words;
   const UINT08 * PostByteBuffer = (const UINT08*)Address + PreBytes + 4 * Words;

   for (i = 0; i < PostBytes; i++)
   {
      Remainder = CrcTable[PostByteBuffer[i] ^ (Remainder >> 24)]
         ^(Remainder << 8);
   }

   return ~Remainder;
}

// Callwlate an N-way interleaved per-element CRC.

void Crc::Callwlate
(
    const void * Address,
    UINT32 Width,
    UINT32 Height,
    ColorUtils::Format Fmt,
    UINT32 Pitch,
    UINT32 NumBins,
    UINT32 * Bins
)
{
    MASSERT(Address != 0);

    PROFILE(CRC);

    Tasker::DetachThread detach;

    const UINT32 numElem = ColorUtils::PixelElements(Fmt);
    const UINT32 bpp     = ColorUtils::PixelBytes(Fmt);
    const UINT32 bpe     = bpp / numElem;

    const UINT08 * pRow = (const UINT08*) Address;
    UINT32 *  pBin;
    UINT32 *  pBinEnd = Bins + NumBins * numElem;

    for (pBin = Bins; pBin < pBinEnd; ++pBin)
        *pBin = 0xFFFFFFFF;

    pBin = Bins;

    // A common case is when there is no line padding.
    // We can treat this case as one very long line, which may be faster.

    if (ColorUtils::PixelBytes(Fmt) * Width == Pitch)
    {
        Width  = Width * Height;
        Pitch  = Pitch * Height;
        Height = 1;
    }

#if defined(LWCPU_AARCH64) && !defined(BIONIC) // Android toolchain does not have arm_acle.h yet
    const auto Next = [](auto lwr, auto begin, auto end)
    {
        const auto next = lwr + 1;
        return (next == end) ? begin : next;
    };

    const bool widthIsAligned = ((Width * bpp) % 4) == 0;

    if ((NumBins == 1 && numElem == 1 && widthIsAligned) || bpe == 4)
    {
        Printf(Tee::PriDebug, "FAST CRC 32b %u x %u, %u bins, %s, %u bpp, %u elems\n",
               Width, Height, NumBins, ColorUtils::FormatToString(Fmt).c_str(), bpp, numElem);
        for (UINT32 y = 0; y < Height; y++)
        {
            auto       ptr = static_cast<const UINT08*>(Address) + Pitch * y;
            const auto end = ptr + Width * bpp;
            for ( ; ptr < end; ptr += 4)
            {
                const auto v32 = MEM_RD32(ptr);

                *pBin = __crc32w(*pBin, v32);
                pBin = Next(pBin, Bins, pBinEnd);
            }
        }
    }
    else if (bpe == 2 && widthIsAligned && Fmt != ColorUtils::Z24S8)
    {
        Printf(Tee::PriDebug, "FAST CRC 16b %u x %u, %u bins, %s, %u bpp, %u elems\n",
               Width, Height, NumBins, ColorUtils::FormatToString(Fmt).c_str(), bpp, numElem);
        for (UINT32 y = 0; y < Height; y++)
        {
            auto       ptr = static_cast<const UINT08*>(Address) + Pitch * y;
            const auto end = ptr + Width * bpp;
            for ( ; ptr < end; ptr += 4)
            {
                const auto v32 = MEM_RD32(ptr);

                *pBin = __crc32h(*pBin, v32 & 0xFFFFU);
                pBin = Next(pBin, Bins, pBinEnd);

                *pBin = __crc32h(*pBin, v32 >> 16);
                pBin = Next(pBin, Bins, pBinEnd);
            }
        }
    }
    else if (bpe == 1 && widthIsAligned && Fmt != ColorUtils::A2R10G10B10)
    {
        Printf(Tee::PriDebug, "FAST CRC 8b %u x %u, %u bins, %s, %u bpp, %u elems\n",
               Width, Height, NumBins, ColorUtils::FormatToString(Fmt).c_str(), bpp, numElem);
        for (UINT32 y = 0; y < Height; y++)
        {
            auto       ptr = static_cast<const UINT08*>(Address) + Pitch * y;
            const auto end = ptr + Width * bpp;
            for ( ; ptr < end; ptr += 4)
            {
                const auto v32 = MEM_RD32(ptr);

                *pBin = __crc32b(*pBin, v32 & 0xFFU);
                pBin = Next(pBin, Bins, pBinEnd);

                *pBin = __crc32b(*pBin, (v32 >> 8) & 0xFFU);
                pBin = Next(pBin, Bins, pBinEnd);

                *pBin = __crc32b(*pBin, (v32 >> 16) & 0xFFU);
                pBin = Next(pBin, Bins, pBinEnd);

                *pBin = __crc32b(*pBin, v32 >> 24);
                pBin = Next(pBin, Bins, pBinEnd);
            }
        }
    }
    else
#endif
    {
        Printf(Tee::PriDebug, "SLOW CRC %u x %u, %u bins, %s, %u bpp, %u elems\n",
               Width, Height, NumBins, ColorUtils::FormatToString(Fmt).c_str(), bpp, numElem);
        switch (Fmt)
        {
            case ColorUtils::A8R8G8B8:
            case ColorUtils::R8G8B8A8:
            case ColorUtils::Z16:
            case ColorUtils::RF16:
            case ColorUtils::RF32:
            case ColorUtils::RF16_GF16_BF16_AF16:
            case ColorUtils::RF32_GF32_BF32_AF32:
            case ColorUtils::Y8:
            case ColorUtils::B8_G8_R8:
            case ColorUtils::Z24:
            case ColorUtils::I8:
            case ColorUtils::VOID8:
            case ColorUtils::VOID16:
            case ColorUtils::A2V10Y10U10:
            case ColorUtils::A2U10Y10V10:
            case ColorUtils::YE16_UE16_YO16_VE16:
            case ColorUtils::YE10Z6_UE10Z6_YO10Z6_VE10Z6:
            case ColorUtils::UE16_YE16_VE16_YO16:
            case ColorUtils::UE10Z6_YE10Z6_VE10Z6_YO10Z6:
            case ColorUtils::VOID32:
            case ColorUtils::CPST8:
            case ColorUtils::CPSTY8CPSTC8:
            case ColorUtils::AUDIOL16_AUDIOR16:
            case ColorUtils::AUDIOL32_AUDIOR32:
            case ColorUtils::A8B8G8R8:
            case ColorUtils::Z32:
            case ColorUtils::AN8BN8GN8RN8:
            case ColorUtils::AS8BS8GS8RS8:
            case ColorUtils::AU8BU8GU8RU8:
            case ColorUtils::A8RL8GL8BL8:
            case ColorUtils::A8BL8GL8RL8:
            case ColorUtils::RS32_GS32_BS32_AS32:
            case ColorUtils::RU32_GU32_BU32_AU32:
            case ColorUtils::R16_G16_B16_A16:
            case ColorUtils::RN16_GN16_BN16_AN16:
            case ColorUtils::RU16_GU16_BU16_AU16:
            case ColorUtils::RS16_GS16_BS16_AS16:
            case ColorUtils::RF32_GF32:
            case ColorUtils::RS32_GS32:
            case ColorUtils::RU32_GU32:
            case ColorUtils::RS32:
            case ColorUtils::RU32:
            case ColorUtils::RF16_GF16:
            case ColorUtils::RS16_GS16:
            case ColorUtils::RN16_GN16:
            case ColorUtils::RU16_GU16:
            case ColorUtils::R16_G16:
            case ColorUtils::G8R8:
            case ColorUtils::GN8RN8:
            case ColorUtils::GS8RS8:
            case ColorUtils::GU8RU8:
            case ColorUtils::R16:
            case ColorUtils::RN16:
            case ColorUtils::RS16:
            case ColorUtils::RU16:
            case ColorUtils::R8:
            case ColorUtils::RN8:
            case ColorUtils::RS8:
            case ColorUtils::RU8:
            case ColorUtils::A8:
            case ColorUtils::ZF32:
            case ColorUtils::U8:
            case ColorUtils::V8:
            case ColorUtils::CR8:
            case ColorUtils::CB8:
            case ColorUtils::U8V8:
            case ColorUtils::V8U8:
            case ColorUtils::A8Y8U8V8:
            case ColorUtils::B8G8R8A8:
            case ColorUtils::S8:
                {
                    // We have:
                    //  - 1 to 4 pixel elements,
                    //  - each of the same size,
                    //  - each a whole number of bytes
                    //  - no "dead" or "X" elements (as in X8R8G8B8 for example)

                    // IGNORED: rgba vs. argb bin-ordering for miscompare reports.

                    const UINT32 blockSize = 256;
                    vector<UINT08> pixelBuf(bpp * blockSize, 0);
                    for (UINT32 row = 0; row < Height; ++row)
                    {
                        for (UINT32 col = 0; col < Width; /*below*/)
                        {
                            const UINT32 colsThisTime = (Width-col > blockSize) ?
                                blockSize :
                                (Width-col);
                            Platform::VirtualRd (
                                    row*Pitch + col*bpp + static_cast<const UINT08*>(Address),
                                    &pixelBuf[0],
                                    colsThisTime*bpp);

                            col += colsThisTime;

                            for (UINT32 i = 0; i < colsThisTime*bpp; i++)
                            {
                                const UINT08 b   = pixelBuf[i];
                                const UINT32 crc = *pBin;

                                *pBin = CrcTable [b^(crc >> 24)] ^ (crc << 8);
                                // Go to next CRC bin each time we complete a pixel-element.
                                // bpe will be 1, 2, or 4.
                                if (0 == (i+1) % bpe)
                                {
                                    pBin++;
                                    if (pBin >= pBinEnd)
                                        pBin = Bins;
                                }
                            }
                        }
                    }
                    break;
                }

            case ColorUtils::YO8VE8YE8UE8:
            case ColorUtils::YO8UE8YE8VE8:
            case ColorUtils::VE8YO8UE8YE8:
            case ColorUtils::UE8YO8VE8YE8:
            case ColorUtils::YB8CR8YA8CB8:
                {
                    // Reading 2 pixels at a time, divide width in half
                    Width >>= 1;

                    for (UINT32 row = 0; row < Height; ++row)
                    {
                        const UINT32 * pData = (const UINT32 *)pRow;

                        for (UINT32 col = 0; col < Width; ++col)
                        {
                            // Remember portability to big-endian.
                            UINT32 pixel = MEM_RD32(pData);
                            pData++;

                            UINT32 cb;
                            UINT32 y0;
                            UINT32 cr;
                            UINT32 y1;
                            if ((Fmt == ColorUtils::YO8VE8YE8UE8) ||
                                    (Fmt == ColorUtils::YB8CR8YA8CB8))
                            {
                                cb = (pixel & 0x000000ff);
                                y0 = (pixel & 0x0000ff00) >>  8;
                                cr = (pixel & 0x00ff0000) >> 16;
                                y1 = (pixel             ) >> 24;
                            }
                            else if (Fmt == ColorUtils::YO8UE8YE8VE8)
                            {
                                cr = (pixel & 0x000000ff);
                                y0 = (pixel & 0x0000ff00) >>  8;
                                cb = (pixel & 0x00ff0000) >> 16;
                                y1 = (pixel             ) >> 24;
                            }
                            else if (Fmt == ColorUtils::UE8YO8VE8YE8)
                            {
                                y0 = (pixel & 0x000000ff);
                                cr = (pixel & 0x0000ff00) >>  8;
                                y1 = (pixel & 0x00ff0000) >> 16;
                                cb = (pixel             ) >> 24;
                            }
                            else
                            {
                                y0 = (pixel & 0x000000ff);
                                cb = (pixel & 0x0000ff00) >>  8;
                                y1 = (pixel & 0x00ff0000) >> 16;
                                cr = (pixel             ) >> 24;
                            }

                            pBin[0] = CrcTable[cb   ^ (pBin[0] >> 24)] ^ (pBin[0] << 8);
                            pBin[1] = CrcTable[cr   ^ (pBin[1] >> 24)] ^ (pBin[1] << 8);
                            pBin[2] = CrcTable[y0   ^ (pBin[2] >> 24)] ^ (pBin[2] << 8);
                            pBin[2] = CrcTable[y1   ^ (pBin[2] >> 24)] ^ (pBin[2] << 8);

                            pBin += 3;
                            if (pBin >= pBinEnd)
                                pBin = Bins;
                        }
                        pRow += Pitch;
                    }
                    break;
                }

            case ColorUtils::A2R10G10B10:
            case ColorUtils::A2B10G10R10:
            case ColorUtils::AU2BU10GU10RU10:
                {
                    for (UINT32 row = 0; row < Height; ++row)
                    {
                        const UINT32 * pData = (const UINT32 *)pRow;

                        for (UINT32 col = 0; col < Width; ++col)
                        {
                            // Remember portability to big-endian.
                            UINT32 pixel = MEM_RD32(pData);
                            pData++;
                            UINT32 blue  = (pixel & 0x000003ff);
                            UINT32 green = (pixel & 0x000ffc00) >> 10;
                            UINT32 red   = (pixel & 0x3ff00000) >> 20;
                            UINT32 alpha = (pixel             ) >> 30;

                            // Crc elements must be in standard LS-to-MSbit order.
                            pBin[0] = CrcTable[(blue&0xff)  ^ (pBin[0] >> 24)] ^ (pBin[0] << 8);
                            pBin[0] = CrcTable[(blue>>8)    ^ (pBin[0] >> 24)] ^ (pBin[0] << 8);
                            pBin[1] = CrcTable[(green&0xff) ^ (pBin[1] >> 24)] ^ (pBin[1] << 8);
                            pBin[1] = CrcTable[(green>>8)   ^ (pBin[1] >> 24)] ^ (pBin[1] << 8);
                            pBin[2] = CrcTable[(red&0xff)   ^ (pBin[2] >> 24)] ^ (pBin[2] << 8);
                            pBin[2] = CrcTable[(red>>8)     ^ (pBin[2] >> 24)] ^ (pBin[2] << 8);
                            pBin[3] = CrcTable[ alpha       ^ (pBin[3] >> 24)] ^ (pBin[3] << 8);

                            pBin += 4;
                            if (pBin >= pBinEnd)
                                pBin = Bins;
                        }
                        pRow += Pitch;
                    }
                    break;
                }

            case ColorUtils::R5G6B5:
            case ColorUtils::B5G6R5:
                {
                    for (UINT32 row = 0; row < Height; ++row)
                    {
                        const UINT16 * pData = (const UINT16 *)pRow;

                        for (UINT32 col = 0; col < Width; ++col)
                        {
                            // Remember portability to big-endian.
                            UINT16 pixel = MEM_RD16(pData);
                            pData++;
                            UINT32 blue  = (pixel & 0x001f);
                            UINT32 green = (pixel & 0x07e0) >>  5;
                            UINT32 red   =  pixel           >> 11;

                            // Crc elements must be in standard LS-to-MSbit order.
                            pBin[0] = CrcTable[blue  ^ (pBin[0] >> 24)] ^ (pBin[0] << 8);
                            pBin[1] = CrcTable[green ^ (pBin[1] >> 24)] ^ (pBin[1] << 8);
                            pBin[2] = CrcTable[red   ^ (pBin[2] >> 24)] ^ (pBin[2] << 8);

                            pBin += 3;
                            if (pBin >= pBinEnd)
                                pBin = Bins;
                        }
                        pRow += Pitch;
                    }
                    break;
                }

            case ColorUtils::A4R4G4B4:
            case ColorUtils::A4B4G4R4:
            case ColorUtils::R4G4B4A4:
            case ColorUtils::B4G4R4A4:
                {
                    for (UINT32 row = 0; row < Height; ++row)
                    {
                        const UINT16 * pData = (const UINT16 *)pRow;

                        for (UINT32 col = 0; col < Width; ++col)
                        {
                            // Remember portability to big-endian.
                            UINT16 pixel = MEM_RD16(pData);
                            pData++;
                            UINT32 blue  = (pixel & 0x000f);
                            UINT32 green = (pixel & 0x00f0) >>  4;
                            UINT32 red   = (pixel & 0x0f00) >>  8;
                            UINT32 alpha =  pixel           >> 12;

                            // Crc elements must be in standard LS-to-MSbit order.
                            pBin[0] = CrcTable[blue  ^ (pBin[0] >> 24)] ^ (pBin[0] << 8);
                            pBin[1] = CrcTable[green ^ (pBin[1] >> 24)] ^ (pBin[1] << 8);
                            pBin[2] = CrcTable[red   ^ (pBin[2] >> 24)] ^ (pBin[2] << 8);
                            pBin[3] = CrcTable[alpha ^ (pBin[3] >> 24)] ^ (pBin[3] << 8);

                            pBin += 4;
                            if (pBin >= pBinEnd)
                                pBin = Bins;
                        }
                        pRow += Pitch;
                    }
                    break;
                }

            case ColorUtils::B5G5R5A1:
            case ColorUtils::A1B5G5R5:
            case ColorUtils::R5G5B5A1:
            case ColorUtils::A1R5G5B5:
                {
                    for (UINT32 row = 0; row < Height; ++row)
                    {
                        const UINT16 * pData = (const UINT16 *)pRow;

                        for (UINT32 col = 0; col < Width; ++col)
                        {
                            // Remember portability to big-endian.
                            UINT16 pixel = MEM_RD16(pData);
                            UINT32 alpha, alphaBin, blueBin;
                            if ((Fmt == ColorUtils::R5G5B5A1) ||
                                    (Fmt == ColorUtils::B5G5R5A1))
                            {
                                alpha = (pixel & 0x0001);
                                pixel = (pixel >> 1);
                                alphaBin = 0;
                                blueBin = 1;
                            }
                            else
                            {
                                alpha = (pixel >> 15);
                                pixel = (pixel & 0x7fff);
                                alphaBin = 3;
                                blueBin = 0;
                            }
                            pData++;
                            UINT32 blue  = (pixel & 0x001f);
                            UINT32 green = (pixel & 0x03e0) >>  5;
                            UINT32 red   =  pixel           >> 10;

                            // Crc elements must be in standard LS-to-MSbit order.
                            pBin[blueBin    ] = CrcTable[blue  ^ (pBin[0] >> 24)] ^ (pBin[0] << 8);
                            pBin[blueBin + 1] = CrcTable[green ^ (pBin[1] >> 24)] ^ (pBin[1] << 8);
                            pBin[blueBin + 2] = CrcTable[red   ^ (pBin[2] >> 24)] ^ (pBin[2] << 8);
                            pBin[alphaBin   ] = CrcTable[alpha ^ (pBin[3] >> 24)] ^ (pBin[3] << 8);

                            pBin += 4;
                            if (pBin >= pBinEnd)
                                pBin = Bins;
                        }
                        pRow += Pitch;
                    }
                    break;
                }

            case ColorUtils::A8R6x2G6x2B6x2:
            case ColorUtils::A8B6x2G6x2R6x2:
                {
                    for (UINT32 row = 0; row < Height; ++row)
                    {
                        const UINT32 * pData = (const UINT32 *)pRow;

                        for (UINT32 col = 0; col < Width; ++col)
                        {
                            // Remember portability to big-endian.
                            UINT32 pixel = MEM_RD32(pData);
                            pData++;
                            UINT32 blue  = (pixel & 0x000000fc);
                            UINT32 green = (pixel & 0x0000fc00) >> 10;
                            UINT32 red   = (pixel & 0x00fc0000) >> 18;
                            UINT32 alpha =  pixel               >> 24;

                            // Crc elements must be in standard LS-to-MSbit order.
                            pBin[0] = CrcTable[blue  ^ (pBin[0] >> 24)] ^ (pBin[0] << 8);
                            pBin[1] = CrcTable[green ^ (pBin[1] >> 24)] ^ (pBin[1] << 8);
                            pBin[2] = CrcTable[red   ^ (pBin[2] >> 24)] ^ (pBin[2] << 8);
                            pBin[3] = CrcTable[alpha ^ (pBin[3] >> 24)] ^ (pBin[3] << 8);

                            pBin += 4;
                            if (pBin >= pBinEnd)
                                pBin = Bins;
                        }
                        pRow += Pitch;
                    }
                    break;
                }

            case ColorUtils::Z1R5G5B5:
            case ColorUtils::X1R5G5B5:
                {
                    for (UINT32 row = 0; row < Height; ++row)
                    {
                        const UINT16 * pData = (const UINT16 *)pRow;

                        for (UINT32 col = 0; col < Width; ++col)
                        {
                            // Remember portability to big-endian.
                            UINT16 pixel = MEM_RD16(pData);
                            pData++;
                            UINT32 blue  = (pixel & 0x001f);
                            UINT32 green = (pixel & 0x03e0) >> 5;
                            UINT32 red   =  pixel & 0x7c00  >> 10;

                            // Crc elements must be in standard LS-to-MSbit order.
                            pBin[0] = CrcTable[blue  ^ (pBin[0] >> 24)] ^ (pBin[0] << 8);
                            pBin[1] = CrcTable[green ^ (pBin[1] >> 24)] ^ (pBin[1] << 8);
                            pBin[2] = CrcTable[red   ^ (pBin[2] >> 24)] ^ (pBin[2] << 8);

                            pBin += 3;
                            if (pBin >= pBinEnd)
                                pBin = Bins;
                        }
                        pRow += Pitch;
                    }
                    break;
                }

            case ColorUtils::Z24S8:
                {
                    for (UINT32 row = 0; row < Height; ++row)
                    {
                        const UINT32 * pData = (const UINT32 *)pRow;

                        for (UINT32 col = 0; col < Width; ++col)
                        {
                            // Remember portability to big-endian.
                            UINT32 pixel = MEM_RD32(pData);
                            pData++;
                            UINT32 stencil = pixel & 0xff;
                            UINT32 z       = pixel >> 8;

                            // Crc elements must be in standard LS-to-MSbit order.
                            pBin[0] = CrcTable[stencil ^ (pBin[0] >> 24)] ^ (pBin[0] << 8);

                            UINT32 tmp = pBin[1];
                            tmp = CrcTable[(z & 0xff) ^ (tmp >> 24)] ^ (tmp << 8);
                            z >>= 8;
                            tmp = CrcTable[(z & 0xff) ^ (tmp >> 24)] ^ (tmp << 8);
                            z >>= 8;
                            pBin[1] = CrcTable[ z     ^ (tmp >> 24)] ^ (tmp << 8);

                            pBin += 2;
                            if (pBin >= pBinEnd)
                                pBin = Bins;
                        }
                        pRow += Pitch;
                    }
                    break;
                }

            case ColorUtils::X8Z24:
                {
                    for (UINT32 row = 0; row < Height; ++row)
                    {
                        const UINT32 * pData = (const UINT32 *)pRow;

                        for (UINT32 col = 0; col < Width; ++col)
                        {
                            // Remember portability to big-endian.
                            UINT32 pixel = MEM_RD32(pData);
                            pData++;
                            UINT32 z     = pixel & 0xffffff;

                            // Crc elements must be in standard LS-to-MSbit order.
                            UINT32 tmp = pBin[0];
                            tmp = CrcTable[(z & 0xff) ^ (tmp >> 24)] ^ (tmp << 8);
                            z >>= 8;
                            tmp = CrcTable[(z & 0xff) ^ (tmp >> 24)] ^ (tmp << 8);
                            z >>= 8;
                            pBin[0] = CrcTable[ z     ^ (tmp >> 24)] ^ (tmp << 8);

                            pBin++;
                            if (pBin >= pBinEnd)
                                pBin = Bins;
                        }
                        pRow += Pitch;
                    }
                    break;
                }

            default:
            case ColorUtils::LWFMT_NONE:
            case ColorUtils::R10G10B10A2:
            case ColorUtils::X8R8G8B8:
            case ColorUtils::X8B8G8R8:
            case ColorUtils::X8RL8GL8BL8:
            case ColorUtils::X8BL8GL8RL8:
            case ColorUtils::RF32_GF32_BF32_X32:
            case ColorUtils::RS32_GS32_BS32_X32:
            case ColorUtils::RU32_GU32_BU32_X32:
            case ColorUtils::RF16_GF16_BF16_X16:
            case ColorUtils::BF10GF11RF11:
            case ColorUtils::S8Z24:
            case ColorUtils::V8Z24:
            case ColorUtils::ZF32_X24S8:
            case ColorUtils::X8Z24_X16V8S8:
            case ColorUtils::ZF32_X16V8X8:
            case ColorUtils::ZF32_X16V8S8:
            case ColorUtils::I1:
            case ColorUtils::I2:
            case ColorUtils::I4:
                {
                    Printf(Tee::PriError,
                           "ColorFormat %s is unsupported for Crc!\n",
                           ColorUtils::FormatToString(Fmt).c_str());
                    MASSERT(!"Unsupported Color format");
                    break;
                }
        }  // switch (Fmt)
    }

    for (pBin = Bins; pBin < pBinEnd; ++pBin)
        *pBin = ~*pBin;
}

void RndFromCrc(UINT32 * Seed)
{
   UINT32   OldSeed = *Seed;

   *Seed = CrcTable[(OldSeed ^ (OldSeed >> 24)) & 255] ^ (OldSeed >> 8);
}

// This is the same polynomial as used with SSE 4.2 CRC instruction:
constexpr UINT32 POLYNOMIAL_CRC32C = 0x82F63B78; // Reversed from 0x1EDC6F41
UINT32 Crc32c::s_Crc32cTable[256];

void Crc32c::GenerateCrc32cTable()
{
    static bool s_Crc32cTableInitialized = false;

    if (s_Crc32cTableInitialized)
        return;

    for (UINT32 tableIdx = 0; tableIdx < CRC_TABLE_SIZE; tableIdx++)
    {
        UINT32 tableValue = tableIdx;
        for (UINT08 bitIdx = 0; bitIdx < 8; bitIdx++)
        {
            if (tableValue & 1)
            {
                tableValue = (tableValue >> 1) ^ POLYNOMIAL_CRC32C;
            }
            else
            {
                tableValue >>= 1;
            }
        }
        s_Crc32cTable[tableIdx] = tableValue;
    }

    s_Crc32cTableInitialized = true;
}

UINT32 Crc32c::StepSw(UINT32 crc, UINT32 data)
{
    crc = s_Crc32cTable[(crc ^ (data >> 0)) & 0xff] ^ (crc >> 8);
    crc = s_Crc32cTable[(crc ^ (data >> 8)) & 0xff] ^ (crc >> 8);
    crc = s_Crc32cTable[(crc ^ (data >> 16)) & 0xff] ^ (crc >> 8);
    crc = s_Crc32cTable[(crc ^ (data >> 24)) & 0xff] ^ (crc >> 8);
    return crc;
}

bool Crc32c::IsHwCrcSupported()
{
#if defined(_M_IX86) || defined(_M_X64) || defined(__amd64) || defined(__i386)
    return Cpu::IsFeatureSupported(Cpu::CRC32C_INSTRUCTION);
#elif defined(LWCPU_AARCH64)
    return true;
#else
    return false;
#endif
}

#if defined(__amd64) || defined(__i386)
__attribute__((target("sse4.2")))
#endif
UINT32 Crc32c::Callwlate
(
    const void *address,
    UINT32 widthInBytes,
    UINT32 height,
    UINT64 pitch
)
{
    UINT32 crc = ~0U;

    if (IsHwCrcSupported())
    {
        for (UINT32 y = 0; y < height; y++)
        {
            const UINT32 *data = reinterpret_cast<const UINT32 *>
                (static_cast<const UINT08*>(address) + y * pitch);
            const UINT32 *dataEnd = data + widthInBytes/4;
            for (; data < dataEnd; data++)
            {
                crc = CRC32C4(crc, *data);
            }
            if (widthInBytes & 0x3)
            {
                const UINT32 numLastBytes = widthInBytes & 0x3;
                const UINT32 lastBytesMask = (1 << (8 * numLastBytes)) - 1;
                const UINT32 lastData = *data & lastBytesMask;
                crc = CRC32C4(crc, lastData);
            }
        }
    }
    else
    {
        GenerateCrc32cTable();
        for (UINT32 y = 0; y < height; y++)
        {
            const UINT32 *data = reinterpret_cast<const UINT32 *>
                (static_cast<const UINT08*>(address) + y * pitch);
            const UINT32 *dataEnd = data + widthInBytes/4;
            for (; data < dataEnd; data++)
            {
                crc = StepSw(crc, *data);
            }
            if (widthInBytes & 0x3)
            {
                const UINT32 numLastBytes = widthInBytes & 0x3;
                const UINT32 lastBytesMask = (1 << (8 * numLastBytes)) - 1;
                const UINT32 lastData = *data & lastBytesMask;
                crc = StepSw(crc, lastData);
            }
        }
    }

    return crc;
}

//------------------------------------------------------------------------------
// OldCrc
//------------------------------------------------------------------------------
const UINT32 OLDCRC_TABLE_SIZE = 256;
const UINT32 OLDPOLYNOMIAL     = 0xEDB88320L;
static UINT32 OldCrcTable[OLDCRC_TABLE_SIZE];

static bool   IsOldCrcTableCreated = false;
static OldCrc SomeStaticOldCrc;

OldCrc::OldCrc()
{
   if (!IsOldCrcTableCreated)
   {
      for (UINT32 Dividend = 0; Dividend < OLDCRC_TABLE_SIZE; ++Dividend)
      {
         UINT32 Remainder = Dividend;

         for (UINT08 Bit = 0; Bit < 8; ++Bit)
         {
            if (Remainder & 1)
            {
               Remainder = (Remainder >> 1) ^ OLDPOLYNOMIAL;
            }
            else
            {
               Remainder = (Remainder >> 1);
            }
         }

         OldCrcTable[Dividend] = Remainder;
      }
      IsOldCrcTableCreated = true;
   }
}

UINT32 OldCrc::Callwlate
(
   void * Address,
   UINT32 Width,
   UINT32 Height,
   UINT32 Depth,
   UINT32 Pitch
)
{
   MASSERT(Address != 0);

   if ((Depth != 16) && (Depth != 32))
      return RC::UNSUPPORTED_DEPTH;

   PROFILE(CRC);

   Tasker::DetachThread detach;

   UINT08 * Row    = static_cast<UINT08*>(Address);
   UINT32 * Column = static_cast<UINT32*>(Address);

   UINT32 X, Y;
   UINT32 Data;

   // We will read 32 bits at a time for faster access,
   // therefore the Width must be scaled accordingly.
   if (16 == Depth)
   {
      MASSERT((Width % 2) == 0);
      Width /= 2;
   }

   UINT32 Remainder = 0xFFFFFFFF;

   // Divide the data by the polynomial one byte at a time.
   for (Y = 0; Y < Height; ++Y)
   {
      Column = reinterpret_cast<UINT32*>(Row);

      for (X = 0; X < Width; ++X)
      {
         // Read 32 bits at a time for faster access.
         Data = MEM_RD32(Column);
         Column++;

         Remainder = OldCrcTable[((Data >>  0) & 0xFF) ^ (Remainder & 0xFF)]
            ^(Remainder >> 8);
         Remainder = OldCrcTable[((Data >>  8) & 0xFF) ^ (Remainder & 0xFF)]
            ^(Remainder >> 8);
         Remainder = OldCrcTable[((Data >> 16) & 0xFF) ^ (Remainder & 0xFF)]
            ^(Remainder >> 8);
         Remainder = OldCrcTable[((Data >> 24) & 0xFF) ^ (Remainder & 0xFF)]
            ^(Remainder >> 8);
      }

      Row += Pitch;
   }

   return ~Remainder;
}

// Callwlate CRC of multiple noncontiguous buffers
// NULL Crc to start, carry over crc to continue
UINT32 OldCrc::Append
(
   void * Address,
   UINT32 Length,
   UINT32 PrevCrc
)
{
   MASSERT(Address != 0);

   PROFILE(CRC);

   UINT32 Remainder = ~PrevCrc;
   UINT32 i;

   // Process any leading unaligned bytes.
   size_t PreBytes = (4 - ((size_t)Address & 0x3)) & 0x3;
   if (PreBytes > Length)
      PreBytes = Length;
   UINT08 * PreByteBuffer = (UINT08*)Address;

   for (i = 0; i < PreBytes; i++)
   {
      Remainder = OldCrcTable[PreByteBuffer[i] ^ (Remainder & 0xFF)]
         ^(Remainder >> 8);
   }

   // We will read 32 bits at a time for faster access,
   // therefore the Width must be scaled accordingly.
   size_t Words = (Length - PreBytes) / 4;
   UINT32 * WordBuffer = (UINT32*)((UINT08*)Address + PreBytes);
   UINT32 Data;

   // Divide the data by the polynomial one byte at a time.
   for (i = 0; i < Words; i++)
   {
      // Read 32 bits at a time for faster access.
      Data = WordBuffer[i];

      Remainder = OldCrcTable[((Data >>  0) & 0xFF) ^ (Remainder & 0xFF)]
         ^(Remainder >> 8);
      Remainder = OldCrcTable[((Data >>  8) & 0xFF) ^ (Remainder & 0xFF)]
         ^(Remainder >> 8);
      Remainder = OldCrcTable[((Data >> 16) & 0xFF) ^ (Remainder & 0xFF)]
         ^(Remainder >> 8);
      Remainder = OldCrcTable[((Data >> 24) & 0xFF) ^ (Remainder & 0xFF)]
         ^(Remainder >> 8);
   }

   // Process any trailing unaligned bytes.
   size_t PostBytes = Length - PreBytes - 4 * Words;
   UINT08 * PostByteBuffer = (UINT08*)Address + PreBytes + 4 * Words;

   for (i = 0; i < PostBytes; i++)
   {
      Remainder = OldCrcTable[PostByteBuffer[i] ^ (Remainder & 0xFF)]
         ^(Remainder >> 8);
   }

   return ~Remainder;
}

// Callwlate an N-way interleaved Crc.

void OldCrc::Callwlate
(
   void * Address,
   UINT32 Width,
   UINT32 Height,
   ColorUtils::Format Fmt,
   UINT32 Pitch,
   UINT32 NumBins,
   UINT32 * Bins
)
{
   MASSERT(Address != 0);

   PROFILE(CRC);

   Tasker::DetachThread detach;

   const UINT08 * pRow  = (const UINT08*) Address;
   UINT32         byteDepth  = ColorUtils::PixelBytes(Fmt);
   UINT32 *       pBin;
   UINT32 *       pBinEnd = Bins + NumBins;

   for (pBin = Bins; pBin < pBinEnd; ++pBin)
      *pBin = 0xFFFFFFFF;

   pBin = Bins;
   for (UINT32 row = 0; row < Height; ++row)
   {
      const UINT08 * pData = pRow;

      for (UINT32 col = 0; col < Width; ++col)
      {
         UINT32 code = *pBin;

         for (UINT32 b = 0; b < byteDepth; ++b)
         {
            UINT32 data = MEM_RD32(pData);
            pData++;
            code = OldCrcTable[data ^ (code & 0xFF)] ^ (code >> 8);
         }
         *pBin = code;
         ++pBin;
         if( pBin >= pBinEnd )
            pBin = Bins;
      }
      pRow += Pitch;
   }

   for (pBin = Bins; pBin < pBinEnd; ++pBin)
      *pBin = ~*pBin;
}

//------------------------------------------------------------------------------
// CheckSum
//------------------------------------------------------------------------------

CheckSum::CheckSum()
{
}

UINT32 CheckSum::Callwlate
(
   void * Address,
   UINT32 Width,
   UINT32 Height,
   UINT32 Depth,
   UINT32 Pitch
)
{
   MASSERT(Address != 0);

   if ((Depth != 16) && (Depth != 32))
      return RC::UNSUPPORTED_DEPTH;

   PROFILE(CRC);

   Tasker::DetachThread detach;

   UINT08 * Row    = static_cast<UINT08*>(Address);
   UINT32 * Column = static_cast<UINT32*>(Address);

   UINT32 X, Y;
   UINT32 Data;

   // We will read 32 bits at a time for faster access,
   // therefore the Width must be scaled accordingly.
   if (16 == Depth)
   {
      MASSERT((Width % 2) == 0);
      Width /= 2;
   }

   UINT32 Sum = 0;

   for (Y = 0; Y < Height; ++Y)
   {
      Column = reinterpret_cast<UINT32*>(Row);

      for (X = 0; X < Width; ++X)
      {
         // Read 32 bits at a time for faster access.
         Data = MEM_RD32(Column);
         Column++;

         Sum += (Data >>  0) & 0xFF;
         Sum += (Data >>  8) & 0xFF;
         Sum += (Data >> 16) & 0xFF;
         Sum += (Data >> 24) & 0xFF;
      }

      Row += Pitch;
   }

   return Sum;
}

// Callwlate an N-way interleaved CheckSum.

void CheckSum::Callwlate
(
   void * Address,
   UINT32 Width,
   UINT32 Height,
   ColorUtils::Format Fmt,
   UINT32 Pitch,
   UINT32 NumBins,
   UINT32 * Bins
)
{
   MASSERT(Address != 0);

   PROFILE(CRC);

   Tasker::DetachThread detach;

   const UINT08 * pRow  = (const UINT08*) Address;
   UINT32         byteDepth  = ColorUtils::PixelBytes(Fmt);
   UINT32 *       pBin;
   UINT32 *       pBinEnd = Bins + NumBins;

   for (pBin = Bins; pBin < pBinEnd; ++pBin)
      *pBin = 0;

   pBin = Bins;
   for (UINT32 row = 0; row < Height; ++row)
   {
      const UINT08 * pData = pRow;

      for (UINT32 col = 0; col < Width; ++col)
      {
         UINT32 code = *pBin;

         for (UINT32 b = 0; b < byteDepth; ++b)
         {
            UINT32 data = MEM_RD32(pData);
            pData++;
            code += data;
         }
         *pBin = code;
         ++pBin;
         if( pBin >= pBinEnd )
            pBin = Bins;
      }
      pRow += Pitch;
   }
}

//------------------------------------------------------------------------------
// CheckSums
//------------------------------------------------------------------------------

CheckSums::CheckSums()
{
}

void CheckSums::Callwlate
(
   void * Address,
   UINT32 Width,
   UINT32 Height,
   ColorUtils::Format Fmt,
   UINT32 Pitch,
   UINT32 NumBins,
   UINT32 * SumsArray
)
{
   Tasker::DetachThread detach;

   // Clear the sums.

   UINT32 sumsPerBin = ColorUtils::PixelElements (Fmt);
   memset(SumsArray, 0, sizeof(UINT32) * NumBins * sumsPerBin);

   // A common case is when there is no line padding.
   // We can treat this case as one very long line, which may be faster.
   if( ColorUtils::PixelBytes(Fmt) * Width == Pitch )
   {
      Width  = Width * Height;
      Pitch  = Pitch * Height;
      Height = 1;
   }

   UINT32 * pSum     = SumsArray;
   UINT32 * pSumEnd  = SumsArray + NumBins * sumsPerBin;

   const UINT08 * pRow     = (const UINT08*)Address;  // first line
   const UINT08 * pRowEnd  = pRow + Pitch * Height;   // last+1 line

   switch (Fmt)
   {
      default:
      {
         // Many formats are inappropriate for CheckSumming, for example
         // floating-point LSBit errors tend to get lost in sums.
         //
         // Rather than silently leaving the sums at zero, print a warning
         // and switch to Crcs, which is much safer.
         Printf(Tee::PriHigh,
              "ColorFormat %s is unsupported for CheckSums, fallback to CRC!\n",
              ColorUtils::FormatToString(Fmt).c_str());

         Crc::Callwlate(Address, Width, Height, Fmt, Pitch, NumBins, SumsArray);
         break;
      }

      case ColorUtils::Z24S8:
      {
         PROFILE(CRC);
         for (/**/; pRow < pRowEnd; pRow += Pitch)
         {
            const UINT32 * pCol     = (const UINT32*)pRow;
            const UINT32 * pColEnd  = pCol + Width;

            for (/**/; pCol < pColEnd; pCol++)
            {
               UINT32 x = MEM_RD32(pCol);

               pSum[0] += (x & 0xff); // low 8 bits == stencil
               pSum[1] += (x >> 8);   // high 24 bits == depth
               pSum += 2;
               if (pSum >= pSumEnd)
                  pSum = SumsArray;
            }
         }
         break;
      }
      case ColorUtils::X8Z24:
      {
         PROFILE(CRC);
         for (/**/; pRow < pRowEnd; pRow += Pitch)
         {
            const UINT32 * pCol     = (const UINT32*)pRow;
            const UINT32 * pColEnd  = pCol + Width;
            for (/**/; pCol < pColEnd; pCol++)
            {
                UINT32 x = MEM_RD32(pCol);
                pSum[0] += (x & 0xffffff);
                pSum++;
               if (pSum >= pSumEnd)
                  pSum = SumsArray;
            }
         }
         break;
      }
      case ColorUtils::Z16:
      {
         PROFILE(CRC);
         for (/**/; pRow < pRowEnd; pRow += Pitch)
         {
            const UINT16 * pCol     = (const UINT16*)pRow;
            const UINT16 * pColEnd  = pCol + Width;

            for (/**/; pCol < pColEnd; pCol++)
            {
               *pSum += MEM_RD16(pCol) << 8;  // scale up to match scale of 24-bit Z
               pSum++;
               if (pSum >= pSumEnd)
                  pSum = SumsArray;
            }
         }
         break;
      }
      case ColorUtils::A8R8G8B8:
      case ColorUtils::A8B8G8R8:
      {
         PROFILE(CRC);
         for (/**/; pRow < pRowEnd; pRow += Pitch)
         {
            const UINT32 * pCol     = (const UINT32*)pRow;
            const UINT32 * pColEnd  = pCol + Width;

            for (/**/; pCol < pColEnd; pCol++)
            {
               UINT32 x = MEM_RD32(pCol);

               pSum[0] += ( x        & 0xff);  // blue
               pSum[1] += ((x >> 8)  & 0xff);  // green
               pSum[2] += ((x >> 16) & 0xff);  // red
               pSum[3] += ( x >> 24);          // alpha
               pSum += 4;
               if (pSum >= pSumEnd)
                  pSum = SumsArray;
            }
         }
         break;
      }
      case ColorUtils::A2R10G10B10:
      {
         PROFILE(CRC);
         for (/**/; pRow < pRowEnd; pRow += Pitch)
         {
            const UINT32 * pCol     = (const UINT32*)pRow;
            const UINT32 * pColEnd  = pCol + Width;

            for (/**/; pCol < pColEnd; pCol++)
            {
               UINT32 x = MEM_RD32(pCol);

               pSum[0] += ( x        & 0x3ff);  // blue
               pSum[1] += ((x >> 10) & 0x3ff);  // green
               pSum[2] += ((x >> 20) & 0x3ff);  // red
               pSum[3] += ( x >> 30);           // alpha
               pSum += 4;
               if (pSum >= pSumEnd)
                  pSum = SumsArray;
            }
         }
         break;
      }
      case ColorUtils::R5G6B5:
      {
         PROFILE(CRC);
         for (/**/; pRow < pRowEnd; pRow += Pitch)
         {
            const UINT16 * pCol     = (const UINT16*)pRow;
            const UINT16 * pColEnd  = pCol + Width;

            for (/**/; pCol < pColEnd; pCol++)
            {
               UINT16 data = MEM_RD16(pCol);

               pSum[0] += ((data & 0x001f) << 3);  // blue
               pSum[1] += ((data & 0x07e0) >> 3);  // green
               pSum[2] += ((data & 0xf800) >> 8);  // red (scale up to 8 bits)
               pSum += 3;
               if (pSum >= pSumEnd)
                  pSum = SumsArray;
            }
         }
         break;
      }
      case ColorUtils::Z1R5G5B5:
      {
         PROFILE(CRC);
         for (/**/; pRow < pRowEnd; pRow += Pitch)
         {
            const UINT16 * pCol     = (const UINT16*)pRow;
            const UINT16 * pColEnd  = pCol + Width;

            for (/**/; pCol < pColEnd; pCol++)
            {
               UINT16 data = MEM_RD16(pCol);

               pSum[0] += ((data & 0x001f) << 3);  // blue
               pSum[1] += ((data & 0x03e0) >> 2);  // green
               pSum[2] += ((data & 0x7c00) >> 7);  // red (scale up to 8 bits)
               pSum += 3;
               if (pSum >= pSumEnd)
                  pSum = SumsArray;
            }
         }
         break;
      }
      case ColorUtils::Z24:
      {
         PROFILE(CRC);
         for (/**/; pRow < pRowEnd; pRow += Pitch)
         {
            const UINT08 * p    = pRow;
            const UINT08 * pEnd = pRow + (Width * 3);

            while (p < pEnd)
            {
               UINT32 x = MEM_RD08(p);
               p++;
               x += (MEM_RD08(p) << 8);
               p++;
               x += (MEM_RD08(p) << 16);
               p++;

               *pSum += x;
               pSum++;
               if (pSum >= pSumEnd)
                  pSum = SumsArray;
            }
         }
         break;
      }
      case ColorUtils::B8_G8_R8:
      {
         PROFILE(CRC);
         for (/**/; pRow < pRowEnd; pRow += Pitch)
         {
            const UINT08 * p    = pRow;
            const UINT08 * pEnd = pRow + (Width * 3);

            while (p < pEnd)
            {
               pSum[0] += p[0];
               pSum[1] += p[1];
               pSum[2] += p[2];
               p += 3;
               pSum += 3;
               if (pSum >= pSumEnd)
                  pSum = SumsArray;
            }
         }
         break;
      }
      case ColorUtils::Y8:
      {
         PROFILE(CRC);
         for (/**/; pRow < pRowEnd; pRow += Pitch)
         {
            const UINT08 * pCol     = (const UINT08*)pRow;
            const UINT08 * pColEnd  = pCol + Width;

            for (/**/; pCol < pColEnd; pCol++)
            {
               *pSum += *pCol;
               ++pSum;
               if (pSum >= pSumEnd)
                  pSum = SumsArray;
            }
         }
         break;
      }
      case ColorUtils::VOID32:
      case ColorUtils::Z32:
      case ColorUtils::RU32:
      case ColorUtils::RS32:
      {
         PROFILE(CRC);
         for (/**/; pRow < pRowEnd; pRow += Pitch)
         {
            const UINT32 * pCol     = (const UINT32*)pRow;
            const UINT32 * pColEnd  = pCol + Width;

            for (/**/; pCol < pColEnd; pCol++)
            {
               *pSum += MEM_RD32(pCol);
               pSum++;
               if (pSum >= pSumEnd)
                  pSum = SumsArray;
            }
         }
         break;
      }
      case ColorUtils::RU32_GU32:
      case ColorUtils::RS32_GS32:
      {
         PROFILE(CRC);
         for (/**/; pRow < pRowEnd; pRow += Pitch)
         {
            const UINT32 * pCol     = (const UINT32*)pRow;
            const UINT32 * pColEnd  = pCol + Width*2;

            for (/**/; pCol < pColEnd; pCol++)
            {
               pSum[0] += MEM_RD32(pCol++); // red
               pSum[1] +=  MEM_RD32(pCol); // green
               pSum += 2;
               if (pSum >= pSumEnd)
                  pSum = SumsArray;
            }
         }
         break;
      }

      case ColorUtils::A4R4G4B4:
      {
         PROFILE(CRC);
         for (/**/; pRow < pRowEnd; pRow += Pitch)
         {
            const UINT16 * pCol     = (const UINT16*)pRow;
            const UINT16 * pColEnd  = pCol + Width;

            for (/**/; pCol < pColEnd; pCol++)
            {
               UINT16 data = MEM_RD16(pCol);

               pSum[0] += ((data & 0x000f) << 4);  // blue
               pSum[1] +=  (data & 0x00f0);        // green
               pSum[2] += ((data & 0x0f00) >> 4);  // red (scale up to 8 bits)
               pSum[3] += ((data & 0xf000) >> 8);  // alpha (scale up to 8 bits)
               pSum += 4;
               if (pSum >= pSumEnd)
                  pSum = SumsArray;
            }
         }
         break;
      }

      case ColorUtils::A8R6x2G6x2B6x2:
      case ColorUtils::A8B6x2G6x2R6x2:
      {
         PROFILE(CRC);
         for (/**/; pRow < pRowEnd; pRow += Pitch)
         {
            const UINT32 * pCol     = (const UINT32*)pRow;
            const UINT32 * pColEnd  = pCol + Width;

            for (/**/; pCol < pColEnd; pCol++)
            {
               UINT32 data = MEM_RD32(pCol);

               pSum[0] +=  (data & 0x000000fc);        // blue
               pSum[1] += ((data & 0x0000fc00) >>  8); // green
               pSum[2] += ((data & 0x00fc0000) >> 16); // red
               pSum[3] += ((data & 0xff000000) >> 24); // alpha
               pSum += 4;
               if (pSum >= pSumEnd)
                  pSum = SumsArray;
            }
         }
         break;
      }
   } // switch (Fmt);
}

//------------------------------------------------------------------------------
// Script objects, properties, and methods.
//------------------------------------------------------------------------------

#ifndef _WIN64

JS_CLASS(Crc);

static SObject Crc_Object
(
   "Crc",
   CrcClass,
   0,
   0,
   "Crc interface."
);

C_(Crc_Callwlate);
static SMethod Crc_Callwlate
(
   Crc_Object,
   "Callwlate",
   C_Crc_Callwlate,
   5,
   "Callwlate the CRC-32 of a buffer."
);

// SMethod
C_(Crc_Callwlate)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;

   // Check the argument.
   UINT32 Address;
   UINT32 Width;
   UINT32 Height;
   UINT32 Depth;
   UINT32 Pitch;
   if
   (
         (NumArguments != 5)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Address))
      || (OK != pJavaScript->FromJsval(pArguments[1], &Width))
      || (OK != pJavaScript->FromJsval(pArguments[2], &Height))
      || (OK != pJavaScript->FromJsval(pArguments[3], &Depth))
      || (OK != pJavaScript->FromJsval(pArguments[4], &Pitch))
   )
   {
      JS_ReportError(pContext, "Usage: Crc.Callwlate(address, width, height, "
         "depth, pitch)");
      return JS_FALSE;
   }

   void * VirtualAddress = Platform::PhysicalToVirtual(Address);

   UINT32 Crc32 = Crc().Callwlate(VirtualAddress, Width, Height, Depth, Pitch);

   if (OK != pJavaScript->ToJsval(Crc32, pReturlwalue))
   {
      JS_ReportError(pContext, "Error oclwrred in Crc.Callwlate().");
      *pReturlwalue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

//------------------------------------------------------------------------------

JS_CLASS(CheckSum);

static SObject CheckSum_Object
(
   "CheckSum",
   CheckSumClass,
   0,
   0,
   "CheckSum interface."
);

C_(CheckSum_Callwlate);
static SMethod CheckSum_Callwlate
(
   CheckSum_Object,
   "Callwlate",
   C_CheckSum_Callwlate,
   5,
   "Callwlate the checksum of a buffer."
);

// SMethod
C_(CheckSum_Callwlate)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;

   // Check the argument.
   UINT32 Address;
   UINT32 Width;
   UINT32 Height;
   UINT32 Depth;
   UINT32 Pitch;
   if
   (
         (NumArguments != 5)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Address))
      || (OK != pJavaScript->FromJsval(pArguments[1], &Width))
      || (OK != pJavaScript->FromJsval(pArguments[2], &Height))
      || (OK != pJavaScript->FromJsval(pArguments[3], &Depth))
      || (OK != pJavaScript->FromJsval(pArguments[4], &Pitch))
   )
   {
      JS_ReportError(pContext, "Usage: CheckSum.Callwlate(address, width, "
         "height, depth, pitch)");
      return JS_FALSE;
   }

   void * VirtualAddress = Platform::PhysicalToVirtual(Address);

   UINT32 Sum = CheckSum().Callwlate(VirtualAddress, Width, Height, Depth,
      Pitch);

   if (OK != pJavaScript->ToJsval(Sum, pReturlwalue))
   {
      JS_ReportError(pContext, "Error oclwrred in CheckSum.Callwlate().");
      *pReturlwalue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

#endif
