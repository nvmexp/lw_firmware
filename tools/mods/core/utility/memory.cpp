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

/**
 * @file  utility/memory.cpp
 * @brief Access to memory.
 *
 *
 */

#include "core/include/memoryif.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "random.h"
#include "core/include/massert.h"
#include "core/include/color.h"
#include "core/include/imagefil.h"
#include "core/include/tee.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include <algorithm>
#include "core/include/xp.h"
#include <string.h>

#include <math.h>
#if !defined(M_PI)
    #define M_PI 3.14159
#endif

static const char *MemAttribNames[] = {
    "UC",
    "WC",
    "WT",
    "WP",
    "WB"
};

const char *Memory::GetMemAttribName(Memory::Attrib ma)
{
    MASSERT((int)ma >= Memory::UC);
    MASSERT((int)ma <= Memory::WB);
    return MemAttribNames[(int)ma-1];
}

void Memory::Fill08
(
    volatile void * Address,
    UINT08 Data,
    UINT32 Words
)
{
    UINT32 i;
    volatile UINT08 * Add = (volatile UINT08*)Address;

    for (i = 0; i < Words; i++)
    {
        MEM_WR08(&Add[i], Data);
    }

    Platform::FlushCpuWriteCombineBuffer();
}

void Memory::Fill16
(
    volatile void * Address,
    UINT16 Data,
    UINT32 Words
)
{
    UINT32 i;
    volatile UINT16 * Add = (volatile UINT16*)Address;

    for (i = 0; i < Words; i++)
    {
        MEM_WR16(&Add[i], Data);
    }

    Platform::FlushCpuWriteCombineBuffer();
}

void Memory::Fill32
(
    volatile void * Address,
    UINT32 Data,
    UINT32 Words
)
{
    UINT32 i;
    volatile UINT32 * Add = (volatile UINT32*)Address;

    for (i = 0; i < Words; i++)
    {
        MEM_WR32(&Add[i], Data);
    }

    Platform::FlushCpuWriteCombineBuffer();
}

void Memory::Fill64
(
    volatile void * Address,
    UINT64 Data,
    UINT32 Words
)
{
    UINT32 i;
    volatile UINT64 * Add = (volatile UINT64*)Address;

    for (i = 0; i < Words; i++)
    {
        MEM_WR64(&Add[i], Data);
    }

    Platform::FlushCpuWriteCombineBuffer();
}

RC Memory::FillRandom
(
    void * Address,
    size_t Bytes,
    Random& random
)
{
    volatile UINT08 * Add = (volatile UINT08*)Address;

    for (size_t i = 0; i < Bytes; i++)
    {
        MEM_WR08( &Add[i], random.GetRandom() );
    }

    Platform::FlushCpuWriteCombineBuffer();

    return OK;
}

RC Memory::FillRandom
(
    void * Address,
    UINT32 Seed,
    size_t Bytes
)
{
    Random random;
    random.SeedRandom(Seed);

    return FillRandom(Address, Bytes, random);
}

RC Memory::FillRandom32
(
    void * address,
    UINT32 seed,
    size_t bytes
)
{
    Random random;
    random.SeedRandom(seed);
    volatile UINT32 * add = static_cast<volatile UINT32*>(address);

    for (size_t i = 0; i < (bytes / 4); ++i, ++add)
    {
        MEM_WR32(add, random.GetRandom());
    }

    Platform::FlushCpuWriteCombineBuffer();

    return RC::OK;
}

RC Memory::FillConstant
(
    void * Address,
    UINT32 Constant,
    UINT32 Width,
    UINT32 Height,
    UINT32 Depth,
    UINT32 Pitch
)
{
    UINT32 X;
    UINT32 Y;

    UINT08 * Row = static_cast<UINT08*>(Address);

    if (16 == Depth)
    {
        for (Y = 0; Y < Height; ++Y)
        {
            volatile UINT16 * Column = reinterpret_cast<volatile UINT16*>(Row);

            for (X = 0; X < Width; ++X)
            {
                MEM_WR16(Column++, Constant);
            }

            Row += Pitch;
        }
    }
    else if (32 == Depth)
    {
        for (Y = 0; Y < Height; ++Y)
        {
            volatile UINT32 * Column = reinterpret_cast<volatile UINT32*>(Row);

            for (X = 0; X < Width; ++X)
            {
                MEM_WR32(Column++, Constant);
            }

            Row += Pitch;
        }
    }
    else
    {
        return RC::UNSUPPORTED_DEPTH;
    }

    Platform::FlushCpuWriteCombineBuffer();

    return OK;
}

RC Memory::FillAddress
(
    void * Address,
    UINT32 Width,
    UINT32 Height,
    UINT32 Depth,
    UINT32 Pitch
)
{
    UINT32 X;
    UINT32 Y;

    UINT08 * Row = static_cast<UINT08*>(Address);

    if (16 == Depth)
    {
        for (Y = 0; Y < Height; ++Y)
        {
            volatile UINT16 * Column = reinterpret_cast<volatile UINT16*>(Row);

            for (X = 0; X < Width; ++X)
            {
                MEM_WR16(Column, (UINT16)(size_t)(Column));
                Column++;
            }

            Row += Pitch;
        }
    }
    else if (32 == Depth)
    {
        for (Y = 0; Y < Height; ++Y)
        {
            volatile UINT32 * Column = reinterpret_cast<volatile UINT32*>(Row);

            for (X = 0; X < Width; ++X)
            {
                MEM_WR32(Column, (UINT32)(size_t)(Column));
                Column++;
            }

            Row += Pitch;
        }
    }
    else
    {
        return RC::UNSUPPORTED_DEPTH;
    }

    Platform::FlushCpuWriteCombineBuffer();

    return OK;
}

RC Memory::FillRandom
(
    void * Address,
    UINT32 Seed,
    UINT32 Low,
    UINT32 High,
    UINT32 Width,
    UINT32 Height,
    UINT32 Depth,
    UINT32 Pitch
)
{
    UINT32 X;
    UINT32 Y;

    UINT08 * Row = static_cast<UINT08*>(Address);
    Random random;
    random.SeedRandom(Seed);

    if (16 == Depth)
    {
        for (Y = 0; Y < Height; ++Y)
        {
            volatile UINT16 * Column = reinterpret_cast<volatile UINT16*>(Row);

            for (X = 0; X < Width; ++X)
            {
                MEM_WR16(Column++, random.GetRandom(Low, High));
            }

            Row += Pitch;
        }
    }
    else if (32 == Depth)
    {
        for (Y = 0; Y < Height; ++Y)
        {
            volatile UINT32 * Column = reinterpret_cast<volatile UINT32*>(Row);

            for (X = 0; X < Width; ++X)
            {
                MEM_WR32(Column++, random.GetRandom(Low, High));
            }

            Row += Pitch;
        }
    }
    else
    {
        return RC::UNSUPPORTED_DEPTH;
    }

    Platform::FlushCpuWriteCombineBuffer();

    return OK;
}

RC Memory::Fill
(
    volatile void * Address,
    const vector<UINT08> * pData08
)
{
    MASSERT(pData08!=0);

    volatile UINT08* Add = (volatile UINT08*)Address;
    size_t Size = pData08->size();

    for (UINT32 i = 0; i<Size; ++i)
    {
        MEM_WR08(Add, (*pData08)[i]);
        Add += 1 ;
    }

    Platform::FlushCpuWriteCombineBuffer();

    return OK;
}

RC Memory::Fill
(
    volatile void * Address,
    const vector<UINT16> * pData16
)
{
    MASSERT(pData16!=0);

    volatile UINT16 * Add = (volatile UINT16*)Address;
    size_t Size = pData16->size();

    for (UINT32 i = 0; i<Size; ++i)
    {
        MEM_WR16(Add, (*pData16)[i]);
        Add ++ ;
    }

    Platform::FlushCpuWriteCombineBuffer();

    return OK;
}

RC Memory::Fill
(
    volatile void * Address,
    const vector<UINT32> * pData32
)
{
    MASSERT(pData32!=0);

    volatile UINT32* Add = (volatile UINT32*)Address;
    size_t Size = pData32->size();

    for (UINT32 i = 0; i<Size; ++i)
    {
        MEM_WR32(Add, (*pData32)[i]);
        Add ++ ;
    }

    Platform::FlushCpuWriteCombineBuffer();

    return OK;
}

RC Memory::FillPattern
(
    void * Address,
    const vector<UINT08> * pPattern,
    UINT32 ByteSize,
    UINT32 StartIndex
)
{
    MASSERT(pPattern!=0);
    MASSERT(pPattern->size()!=0);

    UINT08* Add = (UINT08*)Address;
    size_t Size = pPattern->size();

    for (UINT32 i = 0; i<ByteSize; ++i)
    {
        MEM_WR08(Add, (*pPattern)[(i+StartIndex)%Size]);
        Add ++ ;
    }

    Platform::FlushCpuWriteCombineBuffer();

    return OK;
}

RC Memory::FillPattern
(
    void * Address,
    const vector<UINT16> * pPattern,
    UINT32 ByteSize,
    UINT32 StartIndex
)
{
    MASSERT(pPattern!=0);
    MASSERT(pPattern->size()!=0);

    UINT16* Add = (UINT16*)(Address);
    size_t Size = pPattern->size();

    for (UINT32 i = 0; i<(ByteSize/2); i++)
    {
        MEM_WR16(Add, (*pPattern)[(i+StartIndex)%Size]);
        Add ++ ;
    }

    Platform::FlushCpuWriteCombineBuffer();

    return OK;
}

RC Memory::FillPattern
(
    void * Address,
    const vector<UINT32> * pPattern,
    UINT32 ByteSize,
    UINT32 StartIndex
)
{
    MASSERT(pPattern!=0);
    MASSERT(pPattern->size()!=0);

    UINT32* Add = (UINT32*)Address;
    size_t Size = pPattern->size();

    for (UINT32 i = 0; i<(ByteSize/4); i++)
    {
        MEM_WR32(Add, (*pPattern)[(i+StartIndex)%Size]);
        Add ++ ;
    }

    Platform::FlushCpuWriteCombineBuffer();

    return OK;
}

RC Memory::FillSurfaceWithPattern
(
    void * Address,
    const vector<UINT32> *pPattern,
    UINT32 WidthInBytes,
    UINT32 Lines,
    UINT32 PitchInBytes
)
{
    UINT32 X;
    UINT32 Y;
    size_t Size = pPattern->size();
    UINT32 PatIdx = 0;

    UINT08 * Row = static_cast<UINT08*>(Address);

    for(Y = 0; Y < Lines; ++Y)
    {
       volatile UINT08 * Column = Row;

       for (X = 0; X < WidthInBytes; X += 4)
       {
           UINT32 Pixel = (*pPattern)[PatIdx++ % Size];
           MEM_WR32(Column + X, Pixel);
       }

       Row += PitchInBytes;
    }

    Platform::FlushCpuWriteCombineBuffer();

    return OK;
}

RC Memory::FillWeightedRandom
(
    void *              Address,
    UINT32              Seed,
    Random::PickItem *  pItems,
    UINT32              Width,
    UINT32              Height,
    UINT32              Depth,
    UINT32              Pitch
)
{
    UINT32 X;
    UINT32 Y;

    UINT08 * Row = static_cast<UINT08*>(Address);
    Random random;
    random.SeedRandom(Seed);

    if (16 == Depth)
    {
        for (Y = 0; Y < Height; ++Y)
        {
            volatile UINT16 * Column = reinterpret_cast<volatile UINT16*>(Row);

            for (X = 0; X < Width; ++X)
            {
                MEM_WR16(Column++, random.Pick(pItems));
            }

            Row += Pitch;
        }
    }
    else if (32 == Depth)
    {
        for (Y = 0; Y < Height; ++Y)
        {
            volatile UINT32 * Column = reinterpret_cast<volatile UINT32*>(Row);

            for (X = 0; X < Width; ++X)
            {
                MEM_WR32(Column++, random.Pick(pItems));
            }

            Row += Pitch;
        }
    }
    else
    {
        return RC::UNSUPPORTED_DEPTH;
    }

    Platform::FlushCpuWriteCombineBuffer();

    return OK;
}

RC Memory::FillStripey
(
    void * Address,
    UINT32 Width,
    UINT32 Height,
    UINT32 Depth,
    UINT32 Pitch
)
{
    MASSERT(Address);

    const UINT32 brightnessBits  = 0xFF808080;
    const UINT32 randomRgbBits   = 0x003F3F3F;

    const UINT32 colorThemes[] = { 0x00400000,  // red
        0x00404000,                             // yellow
        0x00004000,                             // green
        0x00004040,                             // cyan
        0x00000040,                             // blue
        0x00400040                              // purple
    };
    const UINT32 numColorThemes = sizeof(colorThemes)/sizeof(UINT32);

    const UINT32 alphaThemes[] = { 0xFF000000,  // fully opaque
        0xFF000000,                             // fully opaque
        0xCF000000,                             // slightly transparent
        0x7F000000,                             // half transparent
        0x00000000                              // fully transparent
    };
    const UINT32 numAlphaThemes = sizeof(alphaThemes)/sizeof(UINT32);

    UINT32 bytesPP = (Depth+7) / 8;
    UINT32 nPixels = Width*Height;

    UINT32 pixelsPerTheme = nPixels / (numColorThemes * numAlphaThemes * 2);
    if (0 == pixelsPerTheme)
        pixelsPerTheme = 1;
    UINT32 idxColor = 0;
    UINT32 idxAlpha = 0;
    UINT32 lwrTheme = 0x00000000;

    UINT32 pixelsPerBrightness = 2;
    UINT32 bright   = 0;
    Random random;
    random.SeedRandom(2524);  // arbitrary

    UINT32 px;
    for ( px = 0; px < nPixels; ++px )
    {
        // next color/alpha theme?

        if ( 0 == (px % pixelsPerTheme) )
        {
            idxColor++;
            if ( idxColor >= numColorThemes )
                idxColor = 0;

            idxAlpha++;
            if ( idxAlpha >= numAlphaThemes )
                idxAlpha = 0;

            lwrTheme = colorThemes[idxColor] | alphaThemes[idxAlpha];

            pixelsPerBrightness++;
        }

        // next bright/dark run?

        if ( 0 == (px % pixelsPerBrightness) )
            bright = !bright;

        // generate the pixel

        UINT32 c = (random.GetRandom() & randomRgbBits) |
                    lwrTheme |
                   (bright * brightnessBits);

        // colwert and store the pixel

        UINT08* pbuf = static_cast<UINT08*>(Address) +
                            Pitch   * (px / Width) +
                            bytesPP * (px % Width);

        switch ( Depth )
        {
            case 32:
                {
                    volatile UINT32 * p = reinterpret_cast<volatile UINT32*>(pbuf);
                    p +=
                        *p = c;
                    break;
                }
            case 16:
                {
                    c = ColorUtils::Colwert(c, ColorUtils::A8R8G8B8, ColorUtils::R5G6B5);
                    volatile UINT16 * p = reinterpret_cast<volatile UINT16*>(pbuf);
                    *p = (UINT16)c;
                    break;
                }
            case 15:
                {
                    c = ColorUtils::Colwert(c, ColorUtils::A8R8G8B8, ColorUtils::Z1R5G5B5);
                    volatile UINT16 * p = reinterpret_cast<volatile UINT16*>(pbuf);
                    *p = (UINT16)c;
                    break;
                }
            default: MASSERT(0); return RC::UNSUPPORTED_DEPTH;
        }
    }

    Platform::FlushCpuWriteCombineBuffer();

    return OK;
}

//------------------------------------------------------------------------------
// Fill the screen with an RGB pattern.  If you change this code, be sure
// you also change Memory::FillRgbScaled() below
//------------------------------------------------------------------------------
RC Memory::FillRgb
(
    void * Address,
    UINT32 Width,
    UINT32 Height,
    UINT32 Depth,
    UINT32 Pitch
)
{
    UINT32 X;
    UINT32 Y;

    UINT08 * Row = static_cast<UINT08*>(Address);

    UINT32 Color;

    if (16 == Depth)
    {
        for (Y = 0; Y < Height; ++Y)
        {
            volatile UINT16 * Column = reinterpret_cast<volatile UINT16*>(Row);

            for (X = 0; X < Width; ++X)
            {
                Color = X + Y;

                if (Y < Height / 4 * 1)
                {
                    // White strips.
                    MEM_WR16(Column,
                           (((Color >> 3) & 0x1F) << 11)
                        |  (((Color >> 2) & 0x3F) <<  5)
                        |  (((Color >> 3) & 0x1F) <<  0));
                }
                else if (Y < Height / 4 * 2)
                {
                    // Red strips.
                    MEM_WR16(Column, ((Color >> 3) & 0x1F) << 11);
                }
                else if (Y < Height / 4 * 3)
                {
                    // Green strips.
                    MEM_WR16(Column, ((Color >> 2) & 0x3F) <<  5);
                }
                else
                {
                    // Blue strips.
                    MEM_WR16(Column, ((Color >> 3) & 0x1F) <<  0);
                }

                ++Column;
            }

            Row += Pitch;
        }
    }
    else if (32 == Depth)
    {
        for (Y = 0; Y < Height; ++Y)
        {
            volatile UINT32 * Column = reinterpret_cast<volatile UINT32*>(Row);

            for (X = 0; X < Width; ++X)
            {
                Color = (X + Y) & 0xFF;

                if (Y < Height / 4 * 1)
                {
                    // White strips.
                    MEM_WR32(Column,
                           (Color << 16)
                        |  (Color <<  8)
                        |  (Color <<  0));
                }
                else if (Y < Height / 4 * 2)
                {
                    // Red strips.
                    MEM_WR32(Column, Color << 16);
                }
                else if (Y < Height / 4 * 3)
                {
                    // Green strips.
                    MEM_WR32(Column, Color << 8);
                }
                else
                {
                    // Blue strips.
                    MEM_WR32(Column, Color << 0);
                }

                ++Column;
            }

            Row += Pitch;
        }
    }
    else
    {
        return RC::UNSUPPORTED_DEPTH;
    }

    Platform::FlushCpuWriteCombineBuffer();

    return OK;
}

//------------------------------------------------------------------------------
// FillRgbScaled.  It's Like Memory::FillRgb, except that it allows you to scale
// and shift the RGB pattern.  Assumes 32-bit color depth.
//------------------------------------------------------------------------------
RC Memory::FillRgbScaled
(
    void *  Address,
    UINT32  Width,
    UINT32  Height,
    UINT32  Depth,
    UINT32  Pitch,
    FLOAT64 HScale,
    FLOAT64 VScale,
    INT32   X0
)
{
    UINT32 HScale4K = INT32(HScale * 4096.0);
    UINT32 VScale4K = INT32(VScale * 4096.0);

    UINT08 * Row = static_cast<UINT08*>(Address);
    UINT32 Color;
    INT32 X, Y;

    for (Y = 0; Y < INT32(Height); ++Y)
    {
        volatile UINT32 * Column = reinterpret_cast<volatile UINT32*>(Row);

        for (X = 0; X < INT32(Width); ++X)
        {
            // use a simple anti-aliasing to reduce the "jaggies" on the diagonal
            // 0xFF->0x00 boundaries.  Each pixel is a weighted average of the
            // center pixel and the eight pixels around it, like this:
            //
            //    +---+---+---+
            //    |   |   |   |
            //    | 1 | 1 | 1 |
            //    |   |   |   |
            //    +---+---+---+
            //    |   |   |   |
            //    | 1 | 2 | 1 |
            //    |   |   |   |
            //    +---+---+---+
            //    |   |   |   |
            //    | 1 | 1 | 1 |
            //    |   |   |   |
            //    +---+---+---+
            //
            Color =
                    // pixels in the middle left, middle right, top center and bottom center
                    + 1 * ( ( X * HScale4K                   + Y * VScale4K + (VScale4K / 4 )  - X0 ) & 0xFFFFF)
                    + 1 * ( ( X * HScale4K                   + Y * VScale4K - (VScale4K / 4 )  - X0 ) & 0xFFFFF)
                    + 1 * ( ( X * HScale4K + (HScale4K / 4 ) + Y * VScale4K                    - X0 ) & 0xFFFFF)
                    + 1 * ( ( X * HScale4K - (HScale4K / 4 ) + Y * VScale4K                    - X0 ) & 0xFFFFF)

                    // the four corner pixels
                    + 1 * ( ( X * HScale4K + (HScale4K / 4 ) + Y * VScale4K + (VScale4K / 4 )  - X0 ) & 0xFFFFF)
                    + 1 * ( ( X * HScale4K + (HScale4K / 4 ) + Y * VScale4K - (VScale4K / 4 )  - X0 ) & 0xFFFFF)
                    + 1 * ( ( X * HScale4K - (HScale4K / 4 ) + Y * VScale4K + (VScale4K / 4 )  - X0 ) & 0xFFFFF)
                    + 1 * ( ( X * HScale4K - (HScale4K / 4 ) + Y * VScale4K - (VScale4K / 4 )  - X0 ) & 0xFFFFF)

                    // center pixel.  Give it 2x weighting
                    + 2 * ( ( X * HScale4K                   + Y * VScale4K                    - X0 ) & 0xFFFFF);

            // Divide by 4096 to undo the multiplication we did earlier in this
            // function.  Divide by 10 since we added up ten "weights" to get
            // this value.
            Color = (Color / 10 / 4096) & 0xFF;

            // Make the upper-left pixel always black.  Without this line, the
            // anti-aliasing algorithm above "mixes in" the invisible white pixels
            // at co-ordinates (-1,0), (-1,-1) and (0,-1).
            if( (0==X) && (0==Y) )
                Color = 0;

            if ( Y < INT32(Height) / 4 * 1)
            {
                // White strips.
                MEM_WR32(Column,
                   (Color << 16)
                |  (Color <<  8)
                |  (Color <<  0));
            }
            else if (Y < INT32(Height) / 4 * 2)
            {
                // Red strips.
                MEM_WR32(Column, Color << 16);
            }
            else if (Y < INT32(Height) / 4 * 3)
            {
                // Green strips.
                MEM_WR32(Column, Color <<  8);
            }
            else
            {
                // Blue strips.
                MEM_WR32(Column, Color <<  0);
            }

            ++Column;
        }

        Row += Pitch;
    }

    Platform::FlushCpuWriteCombineBuffer();

    return OK;
}

#define CALC_RED   (((X <<   RedCBits) / Width) <<   RedShift)
#define CALC_GREEN (((X << GreenCBits) / Width) << GreenShift)
#define CALC_BLUE  (((X <<  BlueCBits) / Width) <<  BlueShift)

RC Memory::FillRgbBars
(
    void * Address,
    UINT32 Width,
    UINT32 Height,
    ColorUtils::Format Format,
    UINT32 Pitch
)
{
    UINT32 Y;

    UINT08 * Row = static_cast<UINT08*>(Address);

    UINT32 Depth = ColorUtils::PixelBits(Format);
    UINT32 RedCBits = 0;
    UINT32 GreenCBits = 0;
    UINT32 BlueCBits = 0;
    UINT32 AlphaCBits = 0;
    ColorUtils::CompBitsRGBA(Format, &RedCBits, &GreenCBits, &BlueCBits, &AlphaCBits);
    UINT32 RedShift = 0;
    UINT32 GreenShift = 0;
    UINT32 BlueShift = 0;

    UINT32 LwmulativeShift = 0;
    for (UINT32 Idx = 0; Idx < 4; Idx++)
    {
        switch (ElementAt(Format, Idx))
        {
            case ColorUtils::elRed:
                RedShift = LwmulativeShift;
                LwmulativeShift += RedCBits;
                break;
            case ColorUtils::elGreen:
                GreenShift = LwmulativeShift;
                LwmulativeShift += GreenCBits;
                break;
            case ColorUtils::elBlue:
                BlueShift = LwmulativeShift;
                LwmulativeShift += BlueCBits;
                break;
            case ColorUtils::elAlpha:
                LwmulativeShift += AlphaCBits;
                break;
            default:
                break;
        }
    };

    if (16 == Depth)
    {
        for (Y = 0; Y < Height; ++Y)
        {
            volatile UINT16 * Column = reinterpret_cast<volatile UINT16*>(Row);

            for (UINT32 X = 0; X < Width; ++X)
            {
                if (     Y < Height / 4 * 1)
                {
                    // White bars
                    MEM_WR16(Column,
                          CALC_RED
                                | CALC_GREEN
                                | CALC_BLUE);
                }
                else if (Y < Height / 4 * 2)
                {
                    // Red bars
                    MEM_WR16(Column, CALC_RED);
                }
                else if (Y < Height / 4 * 3)
                {
                    // Green bars
                    MEM_WR16(Column, CALC_GREEN);
                }
                else
                {
                    // Blue bars
                    MEM_WR16(Column, CALC_BLUE);
                }

                ++Column;
            }

            Row += Pitch;
        }
    }
    else if (32 == Depth)
    {
        for (Y = 0; Y < Height; ++Y)
        {
            volatile UINT32 * Column = reinterpret_cast<volatile UINT32*>(Row);

            for (UINT32 X = 0; X < Width; ++X)
            {
                if (     Y < Height / 4 * 1)
                {
                    // White bars
                    MEM_WR32(Column,
                          CALC_RED
                                | CALC_GREEN
                                | CALC_BLUE);
                }
                else if (Y < Height / 4 * 2)
                {
                    // Red bars
                    MEM_WR32(Column, CALC_RED);
                }
                else if (Y < Height / 4 * 3)
                {
                    // Green bars
                    MEM_WR32(Column, CALC_GREEN);
                }
                else
                {
                    // Blue bars
                    MEM_WR32(Column, CALC_BLUE);
                }

                ++Column;
            }

            Row += Pitch;
        }
    }
    else if (64 == Depth)
    {
        for (Y = 0; Y < Height; ++Y)
        {
            volatile UINT64 * Column = reinterpret_cast<volatile UINT64*>(Row);

            for (UINT64 X = 0; X < Width; ++X)
            {
                if (     Y < Height / 4 * 1)
                {
                    // White bars
                    MEM_WR64(Column,
                          CALC_RED
                        | CALC_GREEN
                        | CALC_BLUE);
                }
                else if (Y < Height / 4 * 2)
                {
                    // Red bars
                    MEM_WR64(Column, CALC_RED);
                }
                else if (Y < Height / 4 * 3)
                {
                    // Green bars
                    MEM_WR64(Column, CALC_GREEN);
                }
                else
                {
                    // Blue bars
                    MEM_WR64(Column, CALC_BLUE);
                }

                ++Column;
            }

            Row += Pitch;
        }
    }
    else
    {
        return RC::UNSUPPORTED_DEPTH;
    }

    Platform::FlushCpuWriteCombineBuffer();

    return OK;
}

// Special pattern to detect bad LVDS/TMDS channels.
RC Memory::FillStripes
(
    void * Address,
    UINT32 Width,
    UINT32 Height,
    UINT32 Depth,
    UINT32 Pitch
)
{
    MASSERT(Address != 0);

    if (Depth != 32)
        return RC::UNSUPPORTED_DEPTH;

    // There will be six horizontal stripes on the screen.
    // The first stripe will have every even pixel full red.     All odd pixels will be black.
    // The second stripe will have every even pixel full green.  All odd pixels will be black.
    // The third stripe will have every even pixel full blue.    All odd pixels will be black.
    // The fourth stripe will have every odd pixel full red.     All even pixels will be black.
    // The fifth stripe will have every odd pixel full green.    All even pixels will be black.
    // The sixth stripe will have every odd pixel full blue.     All even pixels will be black.
    // If any area is the incorrect color, or is all black, at least one of the LVDS/TMDS channels is bad.

    const UINT32 Stripes = 6;

    UINT08 * Row = static_cast<UINT08*>(Address);
    FLOAT64  StripeHeight = (FLOAT64)Height / Stripes;

    for (UINT32 Stripe = 0; Stripe < Stripes; ++Stripe)
    {
        UINT32 Color = 0xFF << (16 - 8*(Stripe % 3));

        for (FLOAT64 Y = Stripe * StripeHeight; Y < ((Stripe + 1) * StripeHeight); ++Y)
        {
            UINT32 * Column = reinterpret_cast<UINT32*>(Row);

            for (UINT32 X = 0; X < Width; ++X)
            {
                if (((Stripe < 3) && !(X & 1)) || ((Stripe >= 3) && (X & 1)))
                {
                    MEM_WR32(Column, Color);
                }
                else
                {
                    MEM_WR32(Column, 0);
                }

                ++Column;
            }

            Row += Pitch;

        }
    }

    Platform::FlushCpuWriteCombineBuffer();

    return OK;
}

// Special color pattern for characterizing TV output using a vectorscope.
RC Memory::FillTvPattern
(
    void * Address,
    UINT32 Width,
    UINT32 Height,
    UINT32 Depth,
    UINT32 Pitch
)
{
    const UINT32 WHITE  = 0xFFFFFF;
    const UINT32 BLACK  = 0x000000;
    const UINT32 YELLOW = 0xFFFF00;
    const UINT32 CYAN   = 0x00FFFF;
    const UINT32 GREEN  = 0x00FF00;
    const UINT32 PURPLE = 0xFF00FF;
    const UINT32 RED    = 0xFF0000;
    const UINT32 BLUE   = 0x0000FF;
    const UINT32 GRAY   = 0x083E59; // R= 8  G=62  B= 89
    const UINT32 BLUGRY = 0x3A007E; // R=58  G= 0  B=126
    const UINT32 PLUGE  = 0x0A0A0A; // R=10  G=10  B= 10

    const UINT32 L1COUNT = 7;
    const UINT32 L2COUNT = 7;
    const UINT32 L3COUNT = 6;

    UINT32 Line1Colors[L1COUNT]={ WHITE, YELLOW, CYAN,   GREEN, PURPLE, RED,   BLUE };
    UINT32 Line2Colors[L2COUNT]={ BLUE,  BLACK,  PURPLE, BLACK, CYAN,   BLACK, WHITE };
    UINT32 Line3Colors[L3COUNT]={ GRAY,  WHITE,  BLUGRY, BLACK, BLACK,  BLACK };

    UINT32 Row1Height = Height * 2 / 3; // two-thirds of the way down
    UINT32 Row2Height = Height * 3 / 4; // three-fourths of the way down

    UINT08 *Base = (UINT08 *)Address;

    MASSERT(Address != 0);

    if (Depth != 32)
        return RC::UNSUPPORTED_DEPTH;

    UINT32 i;
    for( i=0; i<L1COUNT ; i++)
    {
        FillConstant( Base + 4*(i*Width/L1COUNT),
                    Line1Colors[i],
                    ((i+1)*Width/L1COUNT) - (i*Width/L1COUNT),
                    Row1Height,
                    Depth,
                    Pitch);
    }

    for( i=0; i<L2COUNT ; i++)
    {
        FillConstant( Base + 4*(i*Width/L2COUNT) + (Row1Height * Pitch),
                    Line2Colors[i],
                    ((i+1)*Width/L2COUNT) - (i*Width/L2COUNT),
                    Row2Height - Row1Height,
                    Depth,
                    Pitch);
    }

    for( i=0; i<L3COUNT ; i++)
    {
        FillConstant( Base + 4*(i*Width/L3COUNT) + (Row2Height * Pitch),
                    Line3Colors[i],
                    ((i+1)*Width/L3COUNT) - (i*Width/L3COUNT),
                    Height - Row2Height,
                    Depth,
                    Pitch);
    }

    FillConstant( Base + 4*(5*Width/L2COUNT) + (2*Row2Height - Row1Height) * Pitch,
                 PLUGE,
                 (6*Width/L2COUNT) - (5*Width/L2COUNT),
                 Row2Height - Row1Height,
                 Depth,
                 Pitch);

    return OK;
}

//------------------------------------------------------------------------------
// Copies a smaller tile from upper left across rest of bigger buffer.
//
// Colwert this buffer:          to this buffer:
//
//    abc.000000                   abc.abc.ab
//    def.000000                   def.def.de
//    ghi.000000                   ghi.ghi.gh
//    ....000000                   ..........
//    0000000000                   abc.abc.ab
//------------------------------------------------------------------------------
RC Memory::Tile
(
    void *   Address,
    UINT32   SrcWidth,
    UINT32   SrcHeight,
    UINT32   Depth,
    UINT32   Pitch,
    UINT32   DstWidth,
    UINT32   DstHeight
)
{
    MASSERT(Address);
    MASSERT(SrcWidth > 0);
    MASSERT(SrcHeight > 0);
    MASSERT(Depth > 0);

    const UINT32 BpPixel = (Depth + 7) / 8;      // Bytes Per Pixel
    const UINT32 BpSrcRow = BpPixel * SrcWidth;  // Bytes Per Source Row
    const UINT32 BpDstRow = BpPixel * DstWidth;  // Bytes Per Destination Row

    MASSERT(Pitch >= BpSrcRow);
    MASSERT(Pitch >= BpDstRow);

    UINT08 * pRowBegin   = static_cast<UINT08*>(Address);
    UINT08 * pRowEnd     = pRowBegin + BpDstRow;

    for ( UINT32 row = 0; row < DstHeight; ++row )
    {
        UINT08 * pSrc = static_cast<UINT08*>(Address) + Pitch * (row % SrcHeight);
        UINT08 * pDst = pRowBegin;

        if ( row < SrcHeight )
            pDst += BpSrcRow;

        while ( pDst < pRowEnd )
        {
            size_t BytesToCopy = pRowEnd - pDst;
            if ( BytesToCopy > BpSrcRow )
                BytesToCopy = BpSrcRow;

            memcpy( pDst, pSrc, BytesToCopy );

            pDst += BytesToCopy;
        }
        pRowBegin += Pitch;
        pRowEnd   += Pitch;
    }

    Platform::FlushCpuWriteCombineBuffer();

    return OK;
}

inline FLOAT64 Round
(
    FLOAT64 val
)
{
    if (val > 0.0)
    {
        return floor(val + 0.5);
    }
    else
    {
        return floor(val - 0.5);
    }
}

RC Memory::FillSine
(
    void * Address,
    FLOAT64 Period,
    UINT32 Width,
    UINT32 Height,
    UINT32 Depth,
    UINT32 Pitch
)
{
    UINT32 X;
    UINT32 Y;

    UINT08 * Row = static_cast<UINT08*>(Address);

    if (Period<=0) return RC::BAD_PARAMETER;

    if (32 == Depth)
    {
        UINT08 val;
        UINT32 *sinebuffer=new UINT32[Width];

        for (X = 0; X < Width; ++X)
        {
            val=0xff&((unsigned int)Round(127.5+127.499*sin(-M_PI/2.+2.*M_PI*X/Period)));
            sinebuffer[X] = val|(val<<8)|(val<<16)|(val<<24);
        }

        for (Y = 0; Y < Height; ++Y)
        {
            volatile UINT32 * Column = reinterpret_cast<volatile UINT32*>(Row);
            for (X = 0; X < Width; ++X) MEM_WR32(Column++, sinebuffer[X]);
            Row += Pitch;
        }
        delete [] sinebuffer;
    }
    else
    {
        return RC::UNSUPPORTED_DEPTH;
    }

    Platform::FlushCpuWriteCombineBuffer();

    return OK;
}

RC Memory::FlipRGB
(
    void *pAddress,
    UINT32 width,
    UINT32 height,
    ColorUtils::Format format,
    UINT32 pitch
)
{
    // flipping RGB bits when source and destination addresses are same
    // This replaces bits in RGB part with its Complement inplace
    return FlipRGB(pAddress, pAddress, width, height, format, pitch);
}


RC Memory::FlipRGB
(
    void *pSrcAdd,
    void *pDestAdd,
    UINT32 width,
    UINT32 height,
    ColorUtils::Format format,
    UINT32 pitch
)
{
    //Don't change Alpha component
    MASSERT(pSrcAdd);
    MASSERT(pDestAdd);
    if (format != ColorUtils::A8R8G8B8)
    {
        Printf(Tee::PriError,
                "Colour format index %u is not supported for flipping\n", format);
        return RC::UNSUPPORTED_FUNCTION;
    }
    const UINT32 widthBytes = width * ColorUtils::PixelBytes(format);
    UINT08 *pSrcRow = static_cast<UINT08*>(pSrcAdd);
    UINT08 *pDestRow = static_cast<UINT08*>(pDestAdd);
    vector <UINT32> line(width, 0);
    for (UINT32 y = 0; y < height; ++y)
    {
        Platform::VirtualRd(pSrcRow, &(line[0]), widthBytes);
        for (UINT32 x = 0; x < width; x++)
        {
            UINT32 word = line[x];
            UINT32 alpha = word & 0xFF000000;
            UINT32 rgb = word & 0x00FFFFFF;
            line[x] = (~rgb)|alpha;
        }
        Platform::VirtualWr(pDestRow, &(line[0]), widthBytes);
        pSrcRow += pitch;
        pDestRow +=pitch;
    }
    return OK;
}

RC Memory::Dump
(
    void * Address,
    vector<UINT08> * pData,
    UINT32 ByteSize
)
{
    MASSERT(Address!=0);
    MASSERT(pData!=0);

    UINT08 Data;
    for ( UINT32 i = 0; i < ByteSize; i++)
    {
        Data = MEM_RD08( (void*)((size_t)Address + i));
        pData->push_back(Data);
    }
    return OK;
}

RC Memory::Dump
(
    void * Address,
    vector<UINT16> * pData,
    UINT32 ByteSize
)
{
    MASSERT(Address!=0);
    MASSERT(pData!=0);

    UINT16 Data;
    for ( UINT32 i = 0; i < (ByteSize/2); i++)
    {
        Data = MEM_RD16( (void*)((size_t)Address + 2*i));
        pData->push_back(Data);
    }
    return OK;
}

RC Memory::Dump
(
    void * Address,
    vector<UINT32> * pData,
    UINT32 ByteSize
)
{
    MASSERT(Address!=0);
    MASSERT(pData!=0);

    UINT32 Data;
    for ( UINT32 i = 0; i < (ByteSize/4); i++)
    {
        Data = MEM_RD32( (void*)((size_t)Address + 4*i));
        pData->push_back(Data);
    }
    return OK;
}

RC Memory::Compare
(
    void * Address1,
    void * Address2,
    UINT32 ByteSize
)
{
    MASSERT(Address1!=0);
    MASSERT(Address2!=0);
    MASSERT(ByteSize!=0);

    UINT32 DWordSize = ByteSize/4;
    UINT32 LeftBytes = ByteSize%4;
    UINT32 Data1 = 0;
    UINT32 Data2 = 0;

    // Do DWrod read and compare
    for ( UINT32 i = 0; i < DWordSize; i++)
    {
        Data1 = MEM_RD32( (void*)((size_t)Address1 + i*4));
        Data2 = MEM_RD32( (void*)((size_t)Address2 + i*4));

        if ( i == DWordSize)
        {
            Data1 = (Data1<<(LeftBytes*4))>>(LeftBytes*4);
            Data2 = (Data2<<(LeftBytes*4))>>(LeftBytes*4);
        }

        if (Data1!=Data2)
        {
            Printf(Tee::PriHigh, "*** EEROR: Memory::Compare(%10p, %10p) Fail @ 0x%x, 0x%08x, 0x%08x. ***\n",
                (void *)Address1, (void *)Address2, (i*4), Data1, Data2);
            return RC::BUFFER_MISMATCH;
        }
    }

    return OK;
}

RC Memory::ComparePattern
(
    void * Address,
    vector<UINT08> * pPattern,
    UINT32 ByteSize,
    UINT32 Offset
)
{
    MASSERT(Address!=0);
    MASSERT(pPattern!=0);
    MASSERT(pPattern->size()!=0);
    MASSERT(ByteSize!=0);

    size_t Add = (size_t)Address;
    size_t vSize = pPattern->size();
    UINT08 Data = 0;

    for (UINT32 i = 0; i<ByteSize; ++i)
    {
        Data = MEM_RD08((volatile void *)(Add));
        if ( Data != (*pPattern)[(Offset+i)%vSize] )
        {
            Printf(Tee::PriHigh, "*** Error: Buffers mis compare:Buf@[%i] = 0x%02x, Pattern[%i] = 0x%02x\n",
                i, Data, (int)(i%vSize), (*pPattern)[i%vSize] );
            return RC::BUFFER_MISMATCH;
        }
        Add += 1 ;
    }
    return OK;
}

RC Memory::ComparePattern
(
    void * Address,
    vector<UINT16> * pPattern,
    UINT32 ByteSize,
    UINT32 Offset
)
{
    MASSERT(Address!=0);
    MASSERT(pPattern!=0);
    MASSERT(pPattern->size()!=0);
    MASSERT(ByteSize!=0);
    MASSERT(ByteSize%2==0);

    size_t Add = (size_t)Address;
    size_t vSize = pPattern->size();
    UINT16 Data = 0;

    for (UINT32 i = 0; i<ByteSize/2; ++i)
    {
        Data = MEM_RD16((volatile void *)(Add));
        if ( Data != (*pPattern)[(Offset+i)%vSize] )
        {
            Printf(Tee::PriHigh, "*** Error: Buffers mis compare:Buf@[%i] = 0x%02x, Pattern[%i] = 0x%02x\n",
                i, Data, (int)(i%vSize), (*pPattern)[i%vSize] );
            return RC::BUFFER_MISMATCH;
        }
        Add += 2 ;
    }
    return OK;
}

RC Memory::ComparePattern
(
    void * Address,
    vector<UINT32> * pPattern,
    UINT32 ByteSize,
    UINT32 Offset
)
{
    MASSERT(Address!=0);
    MASSERT(pPattern!=0);
    MASSERT(pPattern->size()!=0);
    MASSERT(ByteSize!=0);
    MASSERT(ByteSize%4==0);

    size_t Add = (size_t)Address;
    size_t vSize = pPattern->size();
    UINT32 Data = 0;

    for (UINT32 i = 0; i<ByteSize/4; ++i)
    {
        Data = MEM_RD32((volatile void *)(Add));
        if ( Data != (*pPattern)[(Offset+i)%vSize] )
        {
            Printf(Tee::PriHigh, "*** Error: Buffers mis compare:Buf@[%i] = 0x%02x, Pattern[%i] = 0x%02x\n",
                i, Data, (int)(i%vSize), (*pPattern)[i%vSize] );
            return RC::BUFFER_MISMATCH;
        }
        Add += 4 ;
    }
    return OK;
}

RC Memory::CompareVector
(
    void * Address,
    vector<UINT08> * pvData08,
    UINT32 ByteSize,
    bool   IsFullCmp,
    size_t DataOffset
)
{
    MASSERT( pvData08!= NULL );
    MASSERT( pvData08->size()!=0 );
    MASSERT( (ByteSize+DataOffset)<=pvData08->size() );

    UINT32 i;
    size_t Off = DataOffset;
    UINT08 Data = 0;
    volatile UINT08 * Add = (volatile UINT08*)Address;

    if( (ByteSize+DataOffset)>pvData08->size() )
    {
        Printf(Tee::PriHigh, "*** ERROR: CompareVector(Data08) data over vector data size ***\n");
        return RC::ILWALID_INPUT;
    }

    if(IsFullCmp)
    {
        if(ByteSize!=pvData08->size())
        {
            Printf(Tee::PriHigh, "*** ERROR: Memory::CompareVector(Data08), MemSize = 0x%x, ExpSize = 0x%x ***\n",
                ByteSize, (UINT32)pvData08->size());
            return RC::MEMORY_SIZE_MISMATCH;
        }
        Off = 0;
    }

    for (i = 0; i < ByteSize; i++)
    {
        Data = MEM_RD08(&Add[i]);
        if(Data!=(*pvData08)[i+Off])
        {
            //!!!!!!
            Printf(Tee::PriHigh, "*** ERROR: Data Miscompare: 0x%02x!=[0x%x+0x%x]0x%02x ***\n",
                Data, i, (UINT32)Off, (*pvData08)[i+Off]);
            return RC::DATA_MISMATCH;
        }
    }

    return OK;
}

RC Memory::CompareVector
(
    void * Address,
    vector<UINT16> * pvData16,
    UINT32 ByteSize,
    bool   IsFullCmp,
    size_t DataOffset
)
{
    MASSERT( ByteSize%2==0 );
    MASSERT( pvData16!= NULL );
    MASSERT( pvData16->size()!=0 );
    MASSERT( (ByteSize/2+DataOffset)<=pvData16->size() );

    UINT32 i;
    UINT16 Data = 0;
    size_t Off = DataOffset;
    volatile UINT16 * Add = (volatile UINT16*)Address;

    if ( (ByteSize/2+DataOffset)>pvData16->size() )
    {
        Printf(Tee::PriHigh, "*** ERROR: CompareVector(Data16) data over vector data size ***\n");
        return RC::ILWALID_INPUT;
    }

    if(IsFullCmp)
    {
        if(ByteSize!=(pvData16->size()<<1))
        {
            Printf(Tee::PriHigh, "*** ERROR: Memory::CompareVector(Data16), MemSize = 0x%x, ExpSize = 0x%x ***\n",
                ByteSize, (UINT32)pvData16->size()<<1);
            return RC::MEMORY_SIZE_MISMATCH;
        }
        Off = 0;
    }

    for (i = 0; i < ByteSize/2; i++)
    {
        Data = MEM_RD16(&Add[i]);
        if(Data!=(*pvData16)[i+Off])
        {
            //!!!!!!
            Printf(Tee::PriHigh, "*** ERROR: Data Miscompare: 0x%04x!=[0x%x+0x%x]0x%04x ***\n",
                Data, i, (UINT32)Off, (*pvData16)[i+Off]);
            return RC::DATA_MISMATCH;
        }
    }
    return OK;
}

RC Memory::CompareVector
(
    void * Address,
    vector<UINT32> * pvData32,
    UINT32 ByteSize,
    bool   IsFullCmp,
    size_t DataOffset
)
{
    MASSERT( ByteSize%4==0 );
    MASSERT( pvData32!= NULL );
    MASSERT( pvData32->size()!=0 );
    MASSERT( (ByteSize/4+DataOffset)<=pvData32->size() );

    UINT32 i;
    size_t Off = DataOffset;
    UINT32 Data = 0;
    volatile UINT32 * Add = (volatile UINT32*)Address;
    if( (ByteSize/4+DataOffset)>pvData32->size() )
    {
        Printf(Tee::PriHigh, "*** ERROR: Memory::CompareVector(Data32) data over vector data size ***\n");
        return RC::ILWALID_INPUT;
    }

    if(IsFullCmp)
    {
        if(ByteSize!=(pvData32->size()<<2))
        {
            Printf(Tee::PriHigh, "*** ERROR: Memory::CompareVector(Data32), MemSize = 0x%x, ExpSize = 0x%x ***\n",
                ByteSize, (UINT32)pvData32->size()<<2);
            return RC::MEMORY_SIZE_MISMATCH;
        }
        Off = 0;
    }

    for (i = 0; i < ByteSize/4; i++)
    {
        Data = MEM_RD32(&Add[i]);
        if(Data!=(*pvData32)[i+Off])
        {
            //!!!!!!
            Printf(Tee::PriHigh, "*** ERROR: Data Miscompare: 0x%08x!=[0x%x+0x%x]0x%08x ***\n",
                Data, i, (UINT32)Off, (*pvData32)[i+Off]);
            return RC::DATA_MISMATCH;
        }
    }
    return OK;
}

static const UINT32 s_ShortMats[]=
{  0x00000000, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA };

const vector<UINT32> Memory::PatShortMats
(
    &s_ShortMats[0],
    &s_ShortMats[0] + sizeof(s_ShortMats)/sizeof(UINT32)
);

static UINT32 s_LongMats[]=
{  0x00000000, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA,
   0x33333333, 0xCCCCCCCC, 0xF0F0F0F0, 0x0F0F0F0F,
   0x00FF00FF, 0xFF00FF00, 0x0000FFFF, 0xFFFF0000 };

const vector<UINT32> Memory::PatLongMats
(
    &s_LongMats[0],
    &s_LongMats[0] + sizeof(s_LongMats)/sizeof(UINT32)
);

static UINT32 s_WalkingOnes[]=
{  0x00000001, 0x00000002, 0x00000004, 0x00000008,
   0x00000010, 0x00000020, 0x00000040, 0x00000080,
   0x00000100, 0x00000200, 0x00000400, 0x00000800,
   0x00001000, 0x00002000, 0x00004000, 0x00008000,
   0x00010000, 0x00020000, 0x00040000, 0x00080000,
   0x00100000, 0x00200000, 0x00400000, 0x00800000,
   0x01000000, 0x02000000, 0x04000000, 0x08000000,
   0x10000000, 0x20000000, 0x40000000, 0x80000000,
   0x00000000 };

const vector<UINT32> Memory::PatWalkingOnes
(
    &s_WalkingOnes[0],
    &s_WalkingOnes[0] + sizeof(s_WalkingOnes)/sizeof(UINT32)
);

static UINT32 s_WalkingZeros[]=
{  0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFB, 0xFFFFFFF7,
   0xFFFFFFEF, 0xFFFFFFDF, 0xFFFFFFBF, 0xFFFFFF7F,
   0xFFFFFEFF, 0xFFFFFDFF, 0xFFFFFBFF, 0xFFFFF7FF,
   0xFFFFEFFF, 0xFFFFDFFF, 0xFFFFBFFF, 0xFFFF7FFF,
   0xFFFEFFFF, 0xFFFDFFFF, 0xFFFBFFFF, 0xFFF7FFFF,
   0xFFEFFFFF, 0xFFDFFFFF, 0xFFBFFFFF, 0xFF7FFFFF,
   0xFEFFFFFF, 0xFDFFFFFF, 0xFBFFFFFF, 0xF7FFFFFF,
   0xEFFFFFFF, 0xDFFFFFFF, 0xBFFFFFFF, 0x7FFFFFFF,
   0xFFFFFFFF };

const vector<UINT32> Memory::PatWalkingZeros
(
    &s_WalkingZeros[0],
    &s_WalkingZeros[0] + sizeof(s_WalkingZeros)/sizeof(UINT32)
);

static UINT32 s_SuperZeroOne[]=
{  0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
   0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
   0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
   0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
   0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
   0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
   0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
   0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
   0xFFFFFFFF };

const vector<UINT32> Memory::PatSuperZeroOne
(
    &s_SuperZeroOne[0],
    &s_SuperZeroOne[0] + sizeof(s_SuperZeroOne)/sizeof(UINT32)
);

static UINT32 s_ByteSuper[]=
{  0x00FF0000, 0x00FFFFFF, 0x00FF0000, 0xFF00FFFF,
   0x0000FF00, 0xFFFF00FF, 0x000000FF, 0xFFFFFF00,
   0x00000000 };

const vector<UINT32> Memory::PatByteSuper
(
    &s_ByteSuper[0],
    &s_ByteSuper[0] + sizeof(s_ByteSuper)/sizeof(UINT32)
);

static UINT32 s_CheckerBoard[]=
{  0xAAAAAAAA, 0x55555555, 0xAAAAAAAA, 0x55555555,
   0xAAAAAAAA, 0x55555555, 0xAAAAAAAA, 0x55555555,
   0xAAAAAAAA };

const vector<UINT32> Memory::PatCheckerBoard
(
    &s_CheckerBoard[0],
    &s_CheckerBoard[0] + sizeof(s_CheckerBoard)/sizeof(UINT32)
);

static UINT32 s_IlwCheckerBd[]=
{  0x55555555, 0xAAAAAAAA, 0x55555555, 0xAAAAAAAA,
   0x55555555, 0xAAAAAAAA, 0x55555555, 0xAAAAAAAA,
   0xAAAAAAAA };

const vector<UINT32> Memory::PatIlwCheckerBd
(
    &s_IlwCheckerBd[0],
    &s_IlwCheckerBd[0] + sizeof(s_IlwCheckerBd)/sizeof(UINT32)
);

static UINT32 s_SolidZero[]=
{  0x00000000, 0x00000000, 0x00000000, 0x00000000 };

const vector<UINT32> Memory::PatSolidZero
(
    &s_SolidZero[0],
    &s_SolidZero[0] + sizeof(s_SolidZero)/sizeof(UINT32)
);

static UINT32 s_SolidOne[]=
{  0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };

const vector<UINT32> Memory::PatSolidOne
(
    &s_SolidOne[0],
    &s_SolidOne[0] + sizeof(s_SolidOne)/sizeof(UINT32)
);

static UINT32 s_WalkSwizzle[]=
{  0x00000001, 0x7FFFFFFF, 0x00000002, 0xBFFFFFFF,
   0x00000004, 0xDFFFFFFF, 0x00000008, 0xEFFFFFFF,
   0x00000010, 0xF7FFFFFF, 0x00000020, 0xFBFFFFFF,
   0x00000040, 0xFDFFFFFF, 0x00000080, 0xFEFFFFFF,
   0x00000100, 0xFF7FFFFF, 0x00000200, 0xFFBFFFFF,
   0x00000400, 0xFFDFFFFF, 0x00000800, 0xFFEFFFFF,
   0x00001000, 0xFFF7FFFF, 0x00002000, 0xFFFBFFFF,
   0x00004000, 0xFFFDFFFF, 0x00008000, 0xFFFEFFFF,
   0x00010000, 0xFFFF7FFF, 0x00020000, 0xFFFFBFFF,
   0x00040000, 0xFFFFDFFF, 0x00080000, 0xFFFFEFFF,
   0x00100000, 0xFFFFF7FF, 0x00200000, 0xFFFFFBFF,
   0x00400000, 0xFFFFFDFF, 0x00800000, 0xFFFFFEFF,
   0x01000000, 0xFFFFFF7F, 0x02000000, 0xFFFFFFBF,
   0x04000000, 0xFFFFFFDF, 0x08000000, 0xFFFFFFEF,
   0x10000000, 0xFFFFFFF7, 0x20000000, 0xFFFFFFFB,
   0x40000000, 0xFFFFFFFD, 0x80000000, 0xFFFFFFFE,
   0x00000000 };

const vector<UINT32> Memory::PatWalkSwizzle
(
    &s_WalkSwizzle[0],
    &s_WalkSwizzle[0] + sizeof(s_WalkSwizzle)/sizeof(UINT32)
);

static UINT32 s_WalkNormal[]=
{  0x00000001, 0xFFFFFFFE, 0x00000002, 0xFFFFFFFD,
   0x00000004, 0xFFFFFFFB, 0x00000008, 0xFFFFFFF7,
   0x00000010, 0xFFFFFFEF, 0x00000020, 0xFFFFFFDF,
   0x00000040, 0xFFFFFFBF, 0x00000080, 0xFFFFFF7F,
   0x00000100, 0xFFFFFEFF, 0x00000200, 0xFFFFFDFF,
   0x00000400, 0xFFFFFBFF, 0x00000800, 0xFFFFF7FF,
   0x00001000, 0xFFFFEFFF, 0x00002000, 0xFFFFDFFF,
   0x00004000, 0xFFFFBFFF, 0x00008000, 0xFFFF7FFF,
   0x00010000, 0xFFFEFFFF, 0x00020000, 0xFFFDFFFF,
   0x00040000, 0xFFFBFFFF, 0x00080000, 0xFFF7FFFF,
   0x00100000, 0xFFEFFFFF, 0x00200000, 0xFFDFFFFF,
   0x00400000, 0xFFBFFFFF, 0x00800000, 0xFF7FFFFF,
   0x01000000, 0xFEFFFFFF, 0x02000000, 0xFDFFFFFF,
   0x04000000, 0xFBFFFFFF, 0x08000000, 0xF7FFFFFF,
   0x10000000, 0xEFFFFFFF, 0x20000000, 0xDFFFFFFF,
   0x40000000, 0xBFFFFFFF, 0x80000000, 0xEFFFFFFF,
   0x00000000 };

const vector<UINT32> Memory::PatWalkNormal
(
    &s_WalkNormal[0],
    &s_WalkNormal[0] + sizeof(s_WalkNormal)/sizeof(UINT32)
);

static UINT32 s_SolidAlpha[]=
{  0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000,
   0xFF000000, 0xFF000000, 0xFF000000 };

const vector<UINT32> Memory::PatSolidAlpha
(
    &s_SolidAlpha[0],
    &s_SolidAlpha[0] + sizeof(s_SolidAlpha)/sizeof(UINT32)
);

static UINT32 s_SolidRed[]=
{  0x00FF0000, 0x00FF0000, 0x00FF0000, 0x00FF0000,
   0x00FF0000, 0x00FF0000, 0x00FF0000 };

const vector<UINT32> Memory::PatSolidRed
(
    &s_SolidRed[0],
    &s_SolidRed[0] + sizeof(s_SolidRed)/sizeof(UINT32)
);

static UINT32 s_SolidGreen[]=
{  0x0000FF00, 0x0000FF00, 0x0000FF00, 0x0000FF00,
   0x0000FF00, 0x0000FF00, 0x0000FF00 };

const vector<UINT32> Memory::PatSolidGreen
(
    &s_SolidGreen[0],
    &s_SolidGreen[0] + sizeof(s_SolidGreen)/sizeof(UINT32)
);

static UINT32 s_SolidBlue[]=
{  0x000000FF, 0x000000FF, 0x000000FF, 0x000000FF,
   0x000000FF, 0x000000FF, 0x000000FF };

const vector<UINT32> Memory::PatSolidBlue
(
    &s_SolidBlue[0],
    &s_SolidBlue[0] + sizeof(s_SolidBlue)/sizeof(UINT32)
);

static UINT32 s_SolidWhite[]=
{  0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF,
   0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF };

const vector<UINT32> Memory::PatSolidWhite
(
    &s_SolidWhite[0],
    &s_SolidWhite[0] + sizeof(s_SolidWhite)/sizeof(UINT32)
);

static UINT32 s_SolidCyan[]=
{  0xFF00FFFF, 0xFF00FFFF, 0xFF00FFFF, 0xFF00FFFF,
   0xFF00FFFF, 0xFF00FFFF, 0xFF00FFFF };

const vector<UINT32> Memory::PatSolidCyan
(
    &s_SolidCyan[0],
    &s_SolidCyan[0] + sizeof(s_SolidCyan)/sizeof(UINT32)
);

static UINT32 s_SolidMagenta[]=
{  0xFFFF00FF, 0xFFFF00FF, 0xFFFF00FF, 0xFFFF00FF,
   0xFFFF00FF, 0xFFFF00FF, 0xFFFF00FF };

const vector<UINT32> Memory::PatSolidMagenta
(
    &s_SolidMagenta[0],
    &s_SolidMagenta[0] + sizeof(s_SolidMagenta)/sizeof(UINT32)
);

static UINT32 s_SolidYellow[]=
{  0xFFFFFF00, 0xFFFFFF00, 0xFFFFFF00, 0xFFFFFF00,
   0xFFFFFF00, 0xFFFFFF00, 0xFFFFFF00 };

const vector<UINT32> Memory::PatSolidYellow
(
    &s_SolidYellow[0],
    &s_SolidYellow[0] + sizeof(s_SolidYellow)/sizeof(UINT32)
);

static UINT32 s_DoubleZeroOne[]=
{  0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
   0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
   0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
   0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
   0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
   0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
   0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
   0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
   0xFFFFFFFF };

const vector<UINT32> Memory::PatDoubleZeroOne
(
    &s_DoubleZeroOne[0],
    &s_DoubleZeroOne[0] + sizeof(s_DoubleZeroOne)/sizeof(UINT32)
);

static UINT32 s_TripleSuper[]=
{  0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF,
   0xFFFFFFFF, 0xFFFFFFFF };

const vector<UINT32> Memory::PatTripleSuper
(
    &s_TripleSuper[0],
    &s_TripleSuper[0] + sizeof(s_TripleSuper)/sizeof(UINT32)
);

static UINT32 s_QuadZeroOne[]=
{  0x00000000, 0x00000000, 0x00000000, 0x00000000,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
   0x00000000, 0x00000000, 0x00000000, 0x00000000,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
   0x00000000, 0x00000000, 0x00000000, 0x00000000,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
   0x00000000, 0x00000000, 0x00000000, 0x00000000,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
   0xFFFFFFFF };

const vector<UINT32> Memory::PatQuadZeroOne
(
    &s_QuadZeroOne[0],
    &s_QuadZeroOne[0] + sizeof(s_QuadZeroOne)/sizeof(UINT32)
);

static UINT32 s_TripleWhammy[]=
{  0x00000000, 0x00000001, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFF,
   0x00000000, 0x00000002, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFD, 0xFFFFFFFF,
   0x00000000, 0x00000004, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFB, 0xFFFFFFFF,
   0x00000000, 0x00000008, 0x00000000, 0xFFFFFFFF, 0xFFFFFFF7, 0xFFFFFFFF,
   0x00000000, 0x00000010, 0x00000000, 0xFFFFFFFF, 0xFFFFFFEF, 0xFFFFFFFF,
   0x00000000, 0x00000020, 0x00000000, 0xFFFFFFFF, 0xFFFFFFDF, 0xFFFFFFFF,
   0x00000000, 0x00000040, 0x00000000, 0xFFFFFFFF, 0xFFFFFFBF, 0xFFFFFFFF,
   0x00000000, 0x00000080, 0x00000000, 0xFFFFFFFF, 0xFFFFFF7F, 0xFFFFFFFF,
   0x00000000, 0x00000100, 0x00000000, 0xFFFFFFFF, 0xFFFFFEFF, 0xFFFFFFFF,
   0x00000000, 0x00000200, 0x00000000, 0xFFFFFFFF, 0xFFFFFDFF, 0xFFFFFFFF,
   0x00000000, 0x00000400, 0x00000000, 0xFFFFFFFF, 0xFFFFFBFF, 0xFFFFFFFF,
   0x00000000, 0x00000800, 0x00000000, 0xFFFFFFFF, 0xFFFFF7FF, 0xFFFFFFFF,
   0x00000000, 0x00001000, 0x00000000, 0xFFFFFFFF, 0xFFFFEFFF, 0xFFFFFFFF,
   0x00000000, 0x00002000, 0x00000000, 0xFFFFFFFF, 0xFFFFDFFF, 0xFFFFFFFF,
   0x00000000, 0x00004000, 0x00000000, 0xFFFFFFFF, 0xFFFFBFFF, 0xFFFFFFFF,
   0x00000000, 0x00008000, 0x00000000, 0xFFFFFFFF, 0xFFFF7FFF, 0xFFFFFFFF,
   0x00000000, 0x00010000, 0x00000000, 0xFFFFFFFF, 0xFFFEFFFF, 0xFFFFFFFF,
   0x00000000, 0x00020000, 0x00000000, 0xFFFFFFFF, 0xFFFDFFFF, 0xFFFFFFFF,
   0x00000000, 0x00040000, 0x00000000, 0xFFFFFFFF, 0xFFFBFFFF, 0xFFFFFFFF,
   0x00000000, 0x00080000, 0x00000000, 0xFFFFFFFF, 0xFFF7FFFF, 0xFFFFFFFF,
   0x00000000, 0x00100000, 0x00000000, 0xFFFFFFFF, 0xFFEFFFFF, 0xFFFFFFFF,
   0x00000000, 0x00200000, 0x00000000, 0xFFFFFFFF, 0xFFDFFFFF, 0xFFFFFFFF,
   0x00000000, 0x00400000, 0x00000000, 0xFFFFFFFF, 0xFFBFFFFF, 0xFFFFFFFF,
   0x00000000, 0x00800000, 0x00000000, 0xFFFFFFFF, 0xFF7FFFFF, 0xFFFFFFFF,
   0x00000000, 0x01000000, 0x00000000, 0xFFFFFFFF, 0xFEFFFFFF, 0xFFFFFFFF,
   0x00000000, 0x02000000, 0x00000000, 0xFFFFFFFF, 0xFDFFFFFF, 0xFFFFFFFF,
   0x00000000, 0x04000000, 0x00000000, 0xFFFFFFFF, 0xFBFFFFFF, 0xFFFFFFFF,
   0x00000000, 0x08000000, 0x00000000, 0xFFFFFFFF, 0xF7FFFFFF, 0xFFFFFFFF,
   0x00000000, 0x10000000, 0x00000000, 0xFFFFFFFF, 0xEFFFFFFF, 0xFFFFFFFF,
   0x00000000, 0x20000000, 0x00000000, 0xFFFFFFFF, 0xDFFFFFFF, 0xFFFFFFFF,
   0x00000000, 0x40000000, 0x00000000, 0xFFFFFFFF, 0xBFFFFFFF, 0xFFFFFFFF,
   0x00000000, 0x80000000, 0x00000000, 0xFFFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF,
   0x00000000 };

const vector<UINT32> Memory::PatTripleWhammy
(
    &s_TripleWhammy[0],
    &s_TripleWhammy[0] + sizeof(s_TripleWhammy)/sizeof(UINT32)
);

static UINT32 s_IsolatedOnes[]=
{  0x00000000, 0x00000000, 0x00000000, 0x00000001,
   0x00000000, 0x00000000, 0x00000000, 0x00000002,
   0x00000000, 0x00000000, 0x00000000, 0x00000004,
   0x00000000, 0x00000000, 0x00000000, 0x00000008,
   0x00000000, 0x00000000, 0x00000000, 0x00000010,
   0x00000000, 0x00000000, 0x00000000, 0x00000020,
   0x00000000, 0x00000000, 0x00000000, 0x00000040,
   0x00000000, 0x00000000, 0x00000000, 0x00000080,
   0x00000000, 0x00000000, 0x00000000, 0x00000100,
   0x00000000, 0x00000000, 0x00000000, 0x00000200,
   0x00000000, 0x00000000, 0x00000000, 0x00000400,
   0x00000000, 0x00000000, 0x00000000, 0x00000800,
   0x00000000, 0x00000000, 0x00000000, 0x00001000,
   0x00000000, 0x00000000, 0x00000000, 0x00002000,
   0x00000000, 0x00000000, 0x00000000, 0x00004000,
   0x00000000, 0x00000000, 0x00000000, 0x00008000,
   0x00000000, 0x00000000, 0x00000000, 0x00010000,
   0x00000000, 0x00000000, 0x00000000, 0x00020000,
   0x00000000, 0x00000000, 0x00000000, 0x00040000,
   0x00000000, 0x00000000, 0x00000000, 0x00080000,
   0x00000000, 0x00000000, 0x00000000, 0x00100000,
   0x00000000, 0x00000000, 0x00000000, 0x00200000,
   0x00000000, 0x00000000, 0x00000000, 0x00400000,
   0x00000000, 0x00000000, 0x00000000, 0x00800000,
   0x00000000, 0x00000000, 0x00000000, 0x01000000,
   0x00000000, 0x00000000, 0x00000000, 0x02000000,
   0x00000000, 0x00000000, 0x00000000, 0x04000000,
   0x00000000, 0x00000000, 0x00000000, 0x08000000,
   0x00000000, 0x00000000, 0x00000000, 0x10000000,
   0x00000000, 0x00000000, 0x00000000, 0x20000000,
   0x00000000, 0x00000000, 0x00000000, 0x40000000,
   0x00000000, 0x00000000, 0x00000000, 0x80000000,
   0x00000000 };

const vector<UINT32> Memory::PatIsolatedOnes
(
    &s_IsolatedOnes[0],
    &s_IsolatedOnes[0] + sizeof(s_IsolatedOnes)/sizeof(UINT32)
);

static UINT32 s_IsolatedZeros[]=
{  0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFD,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFB,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFF7,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFEF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFDF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFBF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFF7F,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFEFF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFDFF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFBFF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFF7FF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFEFFF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFDFFF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFBFFF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF7FFF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFEFFFF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFDFFFF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFBFFFF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFF7FFFF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFEFFFFF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFDFFFFF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFBFFFFF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFF7FFFFF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFEFFFFFF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFDFFFFFF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFBFFFFFF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xF7FFFFFF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xEFFFFFFF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xDFFFFFFF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xBFFFFFFF,
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FFFFFFF,
   0xFFFFFFFF };

const vector<UINT32> Memory::PatIsolatedZeros
(
    &s_IsolatedZeros[0],
    &s_IsolatedZeros[0] + sizeof(s_IsolatedZeros)/sizeof(UINT32)
);

static UINT32 s_SlowFalling[]=
{  0xFFFFFFFF, 0x00000001, 0x00000000,
   0xFFFFFFFF, 0x00000002, 0x00000000,
   0xFFFFFFFF, 0x00000004, 0x00000000,
   0xFFFFFFFF, 0x00000008, 0x00000000,
   0xFFFFFFFF, 0x00000010, 0x00000000,
   0xFFFFFFFF, 0x00000020, 0x00000000,
   0xFFFFFFFF, 0x00000040, 0x00000000,
   0xFFFFFFFF, 0x00000080, 0x00000000,
   0xFFFFFFFF, 0x00000100, 0x00000000,
   0xFFFFFFFF, 0x00000200, 0x00000000,
   0xFFFFFFFF, 0x00000400, 0x00000000,
   0xFFFFFFFF, 0x00000800, 0x00000000,
   0xFFFFFFFF, 0x00001000, 0x00000000,
   0xFFFFFFFF, 0x00002000, 0x00000000,
   0xFFFFFFFF, 0x00004000, 0x00000000,
   0xFFFFFFFF, 0x00008000, 0x00000000,
   0xFFFFFFFF, 0x00010000, 0x00000000,
   0xFFFFFFFF, 0x00020000, 0x00000000,
   0xFFFFFFFF, 0x00040000, 0x00000000,
   0xFFFFFFFF, 0x00080000, 0x00000000,
   0xFFFFFFFF, 0x00100000, 0x00000000,
   0xFFFFFFFF, 0x00200000, 0x00000000,
   0xFFFFFFFF, 0x00400000, 0x00000000,
   0xFFFFFFFF, 0x00800000, 0x00000000,
   0xFFFFFFFF, 0x01000000, 0x00000000,
   0xFFFFFFFF, 0x02000000, 0x00000000,
   0xFFFFFFFF, 0x04000000, 0x00000000,
   0xFFFFFFFF, 0x08000000, 0x00000000,
   0xFFFFFFFF, 0x10000000, 0x00000000,
   0xFFFFFFFF, 0x20000000, 0x00000000,
   0xFFFFFFFF, 0x40000000, 0x00000000,
   0xFFFFFFFF, 0x80000000, 0x00000000,
   0xFFFFFFFF };

const vector<UINT32> Memory::PatSlowFalling
(
    &s_SlowFalling[0],
    &s_SlowFalling[0] + sizeof(s_SlowFalling)/sizeof(UINT32)
);

static UINT32 s_SlowRising[]=
{  0x00000000, 0xFFFFFFFE, 0xFFFFFFFF,
   0x00000000, 0xFFFFFFFD, 0xFFFFFFFF,
   0x00000000, 0xFFFFFFFB, 0xFFFFFFFF,
   0x00000000, 0xFFFFFFF7, 0xFFFFFFFF,
   0x00000000, 0xFFFFFFEF, 0xFFFFFFFF,
   0x00000000, 0xFFFFFFDF, 0xFFFFFFFF,
   0x00000000, 0xFFFFFFBF, 0xFFFFFFFF,
   0x00000000, 0xFFFFFF7F, 0xFFFFFFFF,
   0x00000000, 0xFFFFFEFF, 0xFFFFFFFF,
   0x00000000, 0xFFFFFDFF, 0xFFFFFFFF,
   0x00000000, 0xFFFFFBFF, 0xFFFFFFFF,
   0x00000000, 0xFFFFF7FF, 0xFFFFFFFF,
   0x00000000, 0xFFFFEFFF, 0xFFFFFFFF,
   0x00000000, 0xFFFFDFFF, 0xFFFFFFFF,
   0x00000000, 0xFFFFBFFF, 0xFFFFFFFF,
   0x00000000, 0xFFFF7FFF, 0xFFFFFFFF,
   0x00000000, 0xFFFEFFFF, 0xFFFFFFFF,
   0x00000000, 0xFFFDFFFF, 0xFFFFFFFF,
   0x00000000, 0xFFFBFFFF, 0xFFFFFFFF,
   0x00000000, 0xFFF7FFFF, 0xFFFFFFFF,
   0x00000000, 0xFFEFFFFF, 0xFFFFFFFF,
   0x00000000, 0xFFDFFFFF, 0xFFFFFFFF,
   0x00000000, 0xFFBFFFFF, 0xFFFFFFFF,
   0x00000000, 0xFF7FFFFF, 0xFFFFFFFF,
   0x00000000, 0xFEFFFFFF, 0xFFFFFFFF,
   0x00000000, 0xFDFFFFFF, 0xFFFFFFFF,
   0x00000000, 0xFBFFFFFF, 0xFFFFFFFF,
   0x00000000, 0xF7FFFFFF, 0xFFFFFFFF,
   0x00000000, 0xEFFFFFFF, 0xFFFFFFFF,
   0x00000000, 0xDFFFFFFF, 0xFFFFFFFF,
   0x00000000, 0xBFFFFFFF, 0xFFFFFFFF,
   0x00000000, 0x7FFFFFFF, 0xFFFFFFFF,
   0x00000000 };

const vector<UINT32> Memory::PatSlowRising
(
    &s_SlowRising[0],
    &s_SlowRising[0] + sizeof(s_SlowRising)/sizeof(UINT32)
);

#ifndef VULKAN_STANDALONE

RC Memory::FillWeightedRandom
(
    void *              Address,
    FancyPicker &       Picker,
    UINT32              Width,
    UINT32              Height,
    UINT32              Depth,
    UINT32              Pitch
)
{
    UINT32 X;
    UINT32 Y;

    UINT08 * Row = static_cast<UINT08*>(Address);

    if (16 == Depth)
    {
        for (Y = 0; Y < Height; ++Y)
        {
            volatile UINT16 * Column = reinterpret_cast<volatile UINT16*>(Row);

            for (X = 0; X < Width; ++X)
            {
                MEM_WR16(Column++, Picker.Pick());
            }

            Row += Pitch;
        }
    }
    else if (32 == Depth)
    {
        for (Y = 0; Y < Height; ++Y)
        {
            volatile UINT32 * Column = reinterpret_cast<volatile UINT32*>(Row);

            for (X = 0; X < Width; ++X)
            {
                MEM_WR32(Column++, Picker.Pick());
            }

            Row += Pitch;
        }
    }
    else
    {
        return RC::UNSUPPORTED_DEPTH;
    }

    Platform::FlushCpuWriteCombineBuffer();

    return OK;
}

void Memory::Print08
(
    volatile void * Address,
    UINT32 ByteSize,
    Tee::Priority Pri,
    UINT08 BytePerLine
)
{
    MASSERT(Address!=0);

    UINT08 Data = 0;

    Printf(Tee::PriDebug, " Memroy::Print08 @ %10p ( Phys 0x%08llx ), Bytes = 0x%x\n",
          Address, Platform::VirtualToPhysical(Address), ByteSize);

    for ( UINT32 i = 0; i < ByteSize; i++)
    {
        Data = MEM_RD08( (void*)((size_t)Address + i));
        if ( (i%BytePerLine)==0 )
        {
            if (i!=0)
                Printf(Pri, "\n");
            Printf(Pri, "0x%04x [0x%08llx]\t", i,
                Platform::VirtualToPhysical((void *)((size_t)Address+i)));
        }
        Printf(Pri, "%02x ", Data);
    }
    Printf(Pri, "\n");
    return;
}

void  Memory::Print16
(
    volatile void * Address,
    UINT32 ByteSize,
    Tee::Priority Pri,
    UINT08 WordPerLine
)
{
    MASSERT(Address!=0);

    UINT16 Data = 0;
    UINT32 WordSize = ByteSize/2;

    Printf(Tee::PriDebug, " Memroy::Print16 @ %10p ( Phys 0x%08llx ), Bytes = 0x%x\n",
          Address, Platform::VirtualToPhysical(Address), ByteSize);

    for ( UINT32 i = 0; i < WordSize; i++)
    {
        Data = MEM_RD16( (void*)((size_t)Address + i*2));
        if ( (i%WordPerLine)==0 )
        {
            if (i!=0)
                Printf(Pri, "\n");
            Printf(Pri, "0x%04x [0x%08llx]\t", i,
                Platform::VirtualToPhysical((void *)((size_t)Address+i*2)));
        }
        Printf(Pri, "%04x ", Data);
    }
    Printf(Pri, "\n");
    return;
}

void  Memory::Print32
(
    volatile void * Address,
    UINT32 ByteSize,
    Tee::Priority Pri,
    UINT08 DWordPerLine
)
{
    MASSERT(Address!=0);

    UINT32 DWordSize = ByteSize/4;
    UINT32 Data = 0;

    Printf(Tee::PriDebug, " Memroy::Print32 @ %10p ( Phys 0x%08llx ), Bytes = 0x%x\n",
          Address, Platform::VirtualToPhysical(Address), ByteSize);

    for ( UINT32 i = 0; i < DWordSize; i++)
    {
        Data = MEM_RD32( (void*)((size_t)Address + i*4));
        if ( (i%DWordPerLine)==0 )
        {
            if (i!=0)
                Printf(Pri, "\n");
            Printf(Pri, "0x%04x [0x%08llx]\t", i,
                Platform::VirtualToPhysical((void *)((size_t)Address+i*4)));
        }
        Printf(Pri, "%08x ", Data);
    }
    Printf(Pri, "\n");
    return;
}

void  Memory::PrintFloat
(
    volatile void * Address,
    UINT32 ByteSize,
    Tee::Priority Pri,
    UINT08 FloatPerLine
)
{
    MASSERT(Address!=0);

    UINT32 DWordSize = ByteSize/4;
    float  Data = 0;

    Printf(Tee::PriDebug, " Memroy::PrintFloat @ %10p ( Phys %08X ), Bytes = 0x%x\n",
          Address, Platform::VirtualToPhysical32(Address), ByteSize);

    for ( UINT32 i = 0; i < DWordSize; i++)
    {
        Data = *(float*)((size_t)Address + i*4);
        if ( (i%FloatPerLine)==0 )
        {
            if (i!=0)
                Printf(Pri, "\n");
            Printf(Pri, "0x%04x ", i);
        }
        Printf(Pri, "%f ", Data);
    }
    Printf(Pri, "\n");
    return;
}

//------------------------------------------------------------------------------
// For tests: get a JavaScript object property of type Memory::Location.
//------------------------------------------------------------------------------
RC Memory::GetLocationProperty
(
    const SObject &      object,
    const SProperty &    property,
    Memory::Location *   pLoc
)
{
    RC rc = OK;
    UINT32 tmpI;

    if (OK != (rc = JavaScriptPtr()->GetProperty(object, property, &tmpI)))
        return rc;

    switch (tmpI)
    {
        case Coherent:          *pLoc = Coherent;          break;
        case NonCoherent:       *pLoc = NonCoherent;       break;
        case CachedNonCoherent: *pLoc = CachedNonCoherent; break;
        case Fb:                *pLoc = Fb;                break;
        case Optimal:           *pLoc = Optimal;           break;

        default:            return RC::BAD_MEMLOC;
    }
    return OK;
}
RC Memory::GetLocationProperty
(
    const JSObject *     pObject,
    const char *         propertyName,
    Memory::Location *   pLoc
)
{
    RC rc = OK;
    UINT32 tmpI;

    if (OK != (rc = JavaScriptPtr()->GetProperty(const_cast<JSObject*>(pObject), propertyName, &tmpI)))
        return rc;

    switch (tmpI)
    {
        case Coherent:          *pLoc = Coherent;          break;
        case NonCoherent:       *pLoc = NonCoherent;       break;
        case CachedNonCoherent: *pLoc = CachedNonCoherent; break;
        case Fb:                *pLoc = Fb;                break;
        case Optimal:           *pLoc = Optimal;           break;

        default:            return RC::BAD_MEMLOC;
    }
    return OK;
}

//------------------------------------------------------------------------------
// Memory object, methods, and tests.
//------------------------------------------------------------------------------

JS_CLASS(Memory);

//! Memory_Object
/**
 *
 */
static SObject Memory_Object
(
    "Memory",
    MemoryClass,
    0,
    0,
    "Memory read and write."
);

// Constants

static SProperty Memory_LocFb
(
    Memory_Object,
    "Fb",
    0,
    Memory::Fb,
    0,
    0,
    JSPROP_READONLY,
    "Device frame-buffer memory location"
);
static SProperty Memory_LocCoherent
(
    Memory_Object,
    "Coherent",
    0,
    Memory::Coherent,
    0,
    0,
    JSPROP_READONLY,
    "Host Coherent memory location"
);

// For now, alias PCI to Coherent
static SProperty Memory_LocPci
(
    Memory_Object,
    "Pci",
    0,
    Memory::Coherent,
    0,
    0,
    JSPROP_READONLY,
    "Host Coherent memory location"
);

static SProperty Memory_LocNonCoherent
(
    Memory_Object,
    "NonCoherent",
    0,
    Memory::NonCoherent,
    0,
    0,
    JSPROP_READONLY,
    "Host NonCoherent memory location"
);

static SProperty Memory_LocCachedNonCoherent
(
    Memory_Object,
    "CachedNonCoherent",
    0,
    Memory::CachedNonCoherent,
    0,
    0,
    JSPROP_READONLY,
    "Host CachedNonCoherent memory location"
);

// For now alias AGP to NonCoherent
static SProperty Memory_LocAgp
(
    Memory_Object,
    "Agp",
    0,
    Memory::NonCoherent,
    0,
    0,
    JSPROP_READONLY,
    "Host NonCoherent memory location"
);

static SProperty Memory_LocOptimal
(
    Memory_Object,
    "Optimal",
    0,
    Memory::Optimal,
    0,
    0,
    JSPROP_READONLY,
    "Optimal local memory. FB on cards with FB, NonCoherent on cards with 0-FB"
);

static SProperty Memory_SegmentationModel
(
    Memory_Object,
    "Segmentation",
    0,
    Memory::Segmentation,
    0,
    0,
    JSPROP_READONLY,
    "Segmentation model - one context DMA per surface"
);

static SProperty Memory_PagingModel
(
    Memory_Object,
    "Paging",
    0,
    Memory::Paging,
    0,
    0,
    JSPROP_READONLY,
    "Paging model - one context DMA for all surfaces"
);

static SProperty Memory_OptimalAddressModel
(
    Memory_Object,
    "OptimalAddress",
    0,
    Memory::OptimalAddress,
    0,
    0,
    JSPROP_READONLY,
    "Optimal address model - paging if supported, segmentation if not"
);

static SProperty Memory_ModelPciExpress
(
    Memory_Object,
    "PciExpressModel",
    0,
    Memory::PciExpressModel,
    0,
    0,
    JSPROP_READONLY,
    "PCI Express System"
);

static SProperty Memory_ModelPci
(
    Memory_Object,
    "PciModel",
    0,
    Memory::PciModel,
    0,
    0,
    JSPROP_READONLY,
    "PCI System"
);

static SProperty Memory_ModelOptimal
(
    Memory_Object,
    "OptimalModel",
    0,
    Memory::OptimalModel,
    0,
    0,
    JSPROP_READONLY,
    "Use the fastest settings the system supports"
);

static SProperty Memory_DmaProtocolDefault
(
    Memory_Object,
    "Default",
    0,
    Memory::Default,
    0,
    0,
    JSPROP_READONLY,
    "Default DMA Protocol"
);

static SProperty Memory_DmaProtocolSafe
(
    Memory_Object,
    "Safe",
    0,
    Memory::Safe,
    0,
    0,
    JSPROP_READONLY,
    "Safe DMA Protocol"
);

static SProperty Memory_DmaProtocolFast
(
    Memory_Object,
    "Fast",
    0,
    Memory::Fast,
    0,
    0,
    JSPROP_READONLY,
    "Fast DMA Protocol"
);

static SProperty Memory_AttribUC
(
    Memory_Object,
    "UC",
    0,
    Memory::UC,
    0, 0, JSPROP_READONLY,
    "UnCached."
);

static SProperty Memory_AttribWC
(
    Memory_Object,
    "WC",
    0,
    Memory::WC,
    0, 0, JSPROP_READONLY,
    "Write-Combined."
);

static SProperty Memory_AttribWT
(
    Memory_Object,
    "WT",
    0,
    Memory::WT,
    0, 0, JSPROP_READONLY,
    "Write-Through."
);

static SProperty Memory_AttribWP
(
    Memory_Object,
    "WP",
    0,
    Memory::WP,
    0, 0, JSPROP_READONLY,
    "Write-Protected (i.e. read only)."
);

static SProperty Memory_AttribWB
(
    Memory_Object,
    "WB",
    0,
    Memory::WB,
    0, 0, JSPROP_READONLY,
    "Write-Back."
);

static SProperty Memory_AttribReadable
(
    Memory_Object,
    "Readable",
    0,
    Memory::Readable,
    0, 0, JSPROP_READONLY,
    "Readable."
);

static SProperty Memory_AttribWriteable
(
    Memory_Object,
    "Writeable",
    0,
    Memory::Writeable,
    0, 0, JSPROP_READONLY,
    "Writeable."
);
static SProperty Memory_AttribExelwtable
(
    Memory_Object,
    "Exelwtable",
    0,
    Memory::Exelwtable,
    0, 0, JSPROP_READONLY,
    "Exelwtable."
);

static SProperty Memory_AttribReadWrite
(
    Memory_Object,
    "ReadWrite",
    0,
    Memory::ReadWrite,
    0, 0, JSPROP_READONLY,
    "Read/Write."
);

C_(Memory_Read08);
static SMethod Memory_Read08
(
    Memory_Object,
    "Read08",
    C_Memory_Read08,
    1,
    "Read byte from memory."
);

C_(Memory_Read16);
static SMethod Memory_Read16
(
    Memory_Object,
    "Read16",
    C_Memory_Read16,
    1,
    "Read word from memory."
);

C_(Memory_Read32);
static SMethod Memory_Read32
(
    Memory_Object,
    "Read32",
    C_Memory_Read32,
    1,
    "Read long from memory."
);

C_(Memory_Write08);
static STest Memory_Write08
(
    Memory_Object,
    "Write08",
    C_Memory_Write08,
    2,
    "Write byte to memory."
);

C_(Memory_Write16);
static STest Memory_Write16
(
    Memory_Object,
    "Write16",
    C_Memory_Write16,
    2,
    "Write word to memory."
);

C_(Memory_Write32);
static STest Memory_Write32
(
    Memory_Object,
    "Write32",
    C_Memory_Write32,
    2,
    "Write long to memory."
);

C_(Memory_Copy);
static STest Memory_Copy
(
    Memory_Object,
    "Copy",
    C_Memory_Copy,
    3,
    "Use CPU to copy memory from src to dst"
);
static STest Global_Memory_Copy
(
    "Copy",
    C_Memory_Copy,
    3,
    "Use CPU to copy memory from src to dst"
);

C_(Memory_XorCopy);
static STest Memory_XorCopy
(
    Memory_Object,
    "XorCopy",
    C_Memory_XorCopy,
    3,
    "Use CPU to xor blit memory from src to dst"
);

#ifndef _WIN64

C_(Memory_Fill08);
static STest Memory_Fill08
(
    Memory_Object,
    "Fill08",
    C_Memory_Fill08,
    3,
    "Fill memory with byte data."
);

C_(Memory_Fill16);
static STest Memory_Fill16
(
    Memory_Object,
    "Fill16",
    C_Memory_Fill16,
    3,
    "Fill memory with word data."
);

C_(Memory_Fill32);
static STest Memory_Fill32
(
    Memory_Object,
    "Fill32",
    C_Memory_Fill32,
    3,
    "Fill memory with long data."
);

C_(Memory_Fill64);
static STest Memory_Fill64
(
    Memory_Object,
    "Fill64",
    C_Memory_Fill64,
    3,
    "Fill memory with long long data."
);

C_(Memory_FillConstant);
static STest Memory_FillConstant
(
    Memory_Object,
    "FillConstant",
    C_Memory_FillConstant,
    6,
    "Fill memory with a constant value."
);

C_(Memory_FillAddress);
static STest Memory_FillAddress
(
    Memory_Object,
    "FillAddress",
    C_Memory_FillAddress,
    5,
    "Fill memory with memory address."
);

C_(Memory_FillRandom);
static STest Memory_FillRandom
(
    Memory_Object,
    "FillRandom",
    C_Memory_FillRandom,
    8,
    "Fill memory with a random pattern."
);

C_(Memory_FillWeightedRandom);
static STest Memory_FillWeightedRandom
(
    Memory_Object,
    "FillWeightedRandom",
    C_Memory_FillWeightedRandom,
    7,
    "Fill memory with a random pattern."
);

C_(Memory_FillStripey);
static STest Memory_FillStripey
(
    Memory_Object,
    "FillStripey",
    C_Memory_FillStripey,
    5,
    "Fill memory with a stripey colored noise pattern."
);

C_(Memory_FillRgb);
static STest Memory_FillRgb
(
    Memory_Object,
    "FillRgb",
    C_Memory_FillRgb,
    5,
    "Fill memory with an RGB pattern."
);

C_(Memory_FillPattern32);
static STest Memory_FillPattern32
(
    Memory_Object,
    "FillPattern32",
    C_Memory_FillPattern32,
    3,
    "Fill memory with the given pattern."
);

C_(Memory_FillSurfaceWithPattern32);
static STest Memory_FillSurfaceWithPattern32
(
    Memory_Object,
    "FillSurfaceWithPattern32",
    C_Memory_FillSurfaceWithPattern32,
    5,
    "Fill surface with the given pattern."
);

C_(Memory_FillRgbBars);
static STest Memory_FillRgbBars
(
    Memory_Object,
    "FillRgbBars",
    C_Memory_FillRgbBars,
    5,
    "Fill memory with RGB bars."
);

C_(Memory_FillStripes);
static STest Memory_FillStripes
(
    Memory_Object,
    "FillStripes",
    C_Memory_FillStripes,
    5,
    "Fill memory with stripes."
);

C_(Memory_FillSine);
static STest Memory_FillSine
(
    Memory_Object,
    "FillSine",
    C_Memory_FillSine,
    6,
    "Fill memory with a sine wave."
);

C_(Memory_ReadYuv);
static STest Memory_ReadYuv
(
    Memory_Object,
    "ReadYuv",
    C_Memory_ReadYuv,
    7,
    "Load YUV image file into memory"
);

C_(Memory_ReadTga);
static STest Memory_ReadTga
(
    Memory_Object,
    "ReadTga",
    C_Memory_ReadTga,
    7,
    "Load TGA image file into memory"
);

C_(Memory_WriteTga);
static STest Memory_WriteTga
(
    Memory_Object,
    "WriteTga",
    C_Memory_WriteTga,
    7,
    "Write TGA image file from memory image"
);
#endif
// Implementation

// SMethod
C_(Memory_Read08)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    *pReturlwalue = JSVAL_NULL;

    JavaScriptPtr pJavaScript;

    // Check the arguments.

    // we are using FLOAT64 to get >32bit argument passing from
    //   Javascript. FLOAT64 allow us 53bit of precision when casted to
    //   PHYSADDR
    PHYSADDR Address = 0;
    RC rc;
    if (NumArguments != 1)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.Read08(address)");
        return JS_FALSE;
    }
    if (OK != (rc = pJavaScript->FromJsval(pArguments[0], &Address)))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.Read08(address)");
        return JS_FALSE;
    }

    if (OK != (rc = pJavaScript->ToJsval(Platform::PhysRd08(Address), pReturlwalue)))
    {
        pJavaScript->Throw(pContext, rc, "Error oclwrred in Memory.Read08()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// SMethod
C_(Memory_Read16)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    *pReturlwalue = JSVAL_NULL;

    JavaScriptPtr pJavaScript;

    // Check the arguments.

    // we are using FLOAT64 to get >32bit argument passing from
    //   Javascript. FLOAT64 allow us 53bit of precision when casted to
    //   PHYSADDR
    PHYSADDR Address = 0;

    RC rc;
    if (NumArguments != 1)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.Read16(address)");
        return JS_FALSE;
    }
    if (OK != (rc = pJavaScript->FromJsval(pArguments[0], &Address)))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.Read16(address)");
        return JS_FALSE;
    }

    if (OK != (rc = pJavaScript->ToJsval(Platform::PhysRd16(Address), pReturlwalue)))
    {
        pJavaScript->Throw(pContext, rc, "Error oclwrred in Memory.Read16()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// SMethod
C_(Memory_Read32)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    *pReturlwalue = JSVAL_NULL;

    JavaScriptPtr pJavaScript;

    // Check the arguments.

    // we are using FLOAT64 to get >32bit argument passing from
    //   Javascript. FLOAT64 allow us 53bit of precision when casted to
    //   PHYSADDR
    PHYSADDR Address = 0;

    RC rc;
    if (NumArguments != 1)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.Read32(address)");
        return JS_FALSE;
    }
    if (OK != (rc = pJavaScript->FromJsval(pArguments[0], &Address)))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.Read32(address)");
        return JS_FALSE;
    }

    if (OK != (rc = pJavaScript->ToJsval(Platform::PhysRd32(Address), pReturlwalue)))
    {
        pJavaScript->Throw(pContext, rc, "Error oclwrred in Memory.Read32()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// STest
C_(Memory_Write08)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.

    // we are using FLOAT64 to get >32bit argument passing from
    //   Javascript. FLOAT64 allow us 53bit of precision when casted to
    //   PHYSADDR
    UINT32 Data    = 0;
    PHYSADDR Address = 0;

    RC rc;
    if (NumArguments != 2)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.Write08(address, data)");
        return JS_FALSE;
    }
    if ((OK != (rc = pJavaScript->FromJsval(pArguments[0], &Address))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[1], &Data))))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.Write08(address, data)");
        return JS_FALSE;
    }

    if (0 == Address)
    {
        Printf(Tee::PriHigh, "Write to phys mem addr 0 DENIED!\n");
        return JS_FALSE;
    }

    Platform::PhysWr08(Address, Data);

    RETURN_RC(OK);
}

// STest
C_(Memory_Write16)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.

    // we are using FLOAT64 to get >32bit argument passing from
    //   Javascript. FLOAT64 allow us 53bit of precision when casted to
    //   PHYSADDR
    UINT32 Data    = 0;
    PHYSADDR Address = 0;

    RC rc;
    if (NumArguments != 2)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.Write16(address, data)");
        return JS_FALSE;
    }
    if ((OK != (rc = pJavaScript->FromJsval(pArguments[0], &Address))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[1], &Data))))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.Write16(address, data)");
        return JS_FALSE;
    }

    if (0 == Address)
    {
        Printf(Tee::PriHigh, "Write to phys mem addr 0 DENIED!\n");
        return JS_FALSE;
    }

    Platform::PhysWr16(Address, Data);

    RETURN_RC(OK);
}

// STest
C_(Memory_Write32)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.

    // we are using FLOAT64 to get >32bit argument passing from
    //   Javascript. FLOAT64 allow us 53bit of precision when casted to
    //   PHYSADDR
    PHYSADDR Address = 0;
    UINT32 Data    = 0;

    RC rc;
    if (NumArguments != 2)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.Write32(address, data)");
        return JS_FALSE;
    }
    if ((OK != (rc = pJavaScript->FromJsval(pArguments[0], &Address))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[1], &Data))))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.Write32(address, data)");
        return JS_FALSE;
    }

    if (0 == Address)
    {
        Printf(Tee::PriHigh, "Write to phys mem addr 0 DENIED!\n");
        return JS_FALSE;
    }

    Platform::PhysWr32(Address, Data);

    RETURN_RC(OK);
}

// Takes physical addresses
C_(Memory_Copy)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJs;
    PHYSADDR Dst;
    PHYSADDR Src;
    UINT32 Count;

    RC rc;
    if (NumArguments != 3)
    {
        pJs->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.Copy(Dst, Src, Count)");
        return JS_FALSE;
    }
    if ((OK != (rc = pJs->FromJsval(pArguments[0], &Dst))) ||
        (OK != (rc = pJs->FromJsval(pArguments[1], &Src))) ||
        (OK != (rc = pJs->FromJsval(pArguments[2], &Count))))
    {
        pJs->Throw(pContext, rc, "Usage: Memory.Copy(Dst, Src, Count)");
        return JS_FALSE;
    }
    if (0 == Dst)
    {
       Printf(Tee::PriHigh, "Write to phys mem addr 0 DENIED!\n");
       return JS_FALSE;
    }

    void * VirtualDst = Platform::PhysicalToVirtual(Dst);
    void * VirtualSrc = Platform::PhysicalToVirtual(Src);

    Platform::MemCopy(VirtualDst, VirtualSrc, Count);
    RETURN_RC(OK);
}

// Takes physical addresses
C_(Memory_XorCopy)
{
    MASSERT(pContext != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJs;
    PHYSADDR Dst;
    PHYSADDR Src;
    UINT32 Count;

    RC rc;
    if (NumArguments != 3)
    {
        pJs->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.XorCopy(Dst, Src, Count)");
        return JS_FALSE;
    }
    if ((OK != (rc = pJs->FromJsval(pArguments[0], &Dst))) ||
        (OK != (rc = pJs->FromJsval(pArguments[1], &Src))) ||
        (OK != (rc = pJs->FromJsval(pArguments[2], &Count))))
    {
        pJs->Throw(pContext, rc, "Usage: Memory.XorCopy(Dst, Src, Count)");
        return JS_FALSE;
    }
    if (0 == Dst)
    {
        Printf(Tee::PriHigh, "Write to phys mem addr 0 DENIED!\n");
        return JS_FALSE;
    }

    void * dst = Platform::PhysicalToVirtual(Dst);
    void * src = Platform::PhysicalToVirtual(Src);
    size_t n = Count;

    uintptr_t d = (uintptr_t)dst;
    uintptr_t s = (uintptr_t)src;

    // Align dst to 4 bytes
    if (d & 3)
    {
        size_t num = 4 - (d & 3); // how many bytes we need to get aligned
        if (n >= num) // do we have at least that many in total to copy?
        {
            // Copy 1 byte at a time until we are aligned
            while (d & 3)
            {
                *(UINT08 *)d = *(UINT08 *)d ^ *(const UINT08 *)s;
                d++;
                s++;
                n--;
            }
        }
    }
    while (n >= 4)
    {
        *(UINT32 *)d = *(UINT32 *)d ^ *(const UINT32 *)s;
        d += 4;
        s += 4;
        n -= 4;
    }
    while (n)
    {
        *(UINT08 *)d = *(UINT08 *)d ^ *(const UINT08 *)s;
        d++;
        s++;
        n--;
    }

    RETURN_RC(OK);
}

#ifndef _WIN64

// STest
C_(Memory_Fill08)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    PHYSADDR Address = 0;
    UINT32   Data    = 0;
    UINT32   Words   = 0;

    RC rc;
    if (NumArguments != 3)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.Fill08(address, data, words)");
        return JS_FALSE;
    }
    if ((OK != (rc = pJavaScript->FromJsval(pArguments[0], &Address))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[1], &Data))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[2], &Words))))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.Fill08(address, data, words)");
        return JS_FALSE;
    }

    if (0 == Address)
    {
        Printf(Tee::PriHigh, "Write to phys mem addr 0 DENIED!\n");
        return JS_FALSE;
    }
    void * VirtualAddr = Platform::PhysicalToVirtual(Address);

    Memory::Fill08(VirtualAddr, Data, Words);

    RETURN_RC(OK);
}

// STest
C_(Memory_Fill16)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    PHYSADDR Address = 0;
    UINT32   Data    = 0;
    UINT32   Words   = 0;

    RC rc;
    if (NumArguments != 3)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.Fill16(address, data, words)");
        return JS_FALSE;
    }
    if ((OK != (rc = pJavaScript->FromJsval(pArguments[0], &Address))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[1], &Data))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[2], &Words))))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.Fill16(address, data, words)");
        return JS_FALSE;
    }

    if (0 == Address)
    {
        Printf(Tee::PriHigh, "Write to phys mem addr 0 DENIED!\n");
        return JS_FALSE;
    }
    void * VirtualAddr = Platform::PhysicalToVirtual(Address);

    Memory::Fill16(VirtualAddr, Data, Words);

    RETURN_RC(OK);
}

// STest
C_(Memory_Fill32)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    PHYSADDR Address = 0;
    UINT32   Data    = 0;
    UINT32   Words   = 0;

    RC rc;
    if (NumArguments != 3)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.Fill32(address, data, words)");
        return JS_FALSE;
    }
    if ((OK != (rc = pJavaScript->FromJsval(pArguments[0], &Address))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[1], &Data))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[2], &Words))))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.Fill32(address, data, words)");
        return JS_FALSE;
    }

    if (0 == Address)
    {
        Printf(Tee::PriHigh, "Write to phys mem addr 0 DENIED!\n");
        return JS_FALSE;
    }
    void * VirtualAddr = Platform::PhysicalToVirtual(Address);

    Memory::Fill32(VirtualAddr, Data, Words);

    RETURN_RC(OK);
}

// STest
C_(Memory_Fill64)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    PHYSADDR Address = 0;
    UINT64   Data    = 0;
    UINT32   Words   = 0;

    RC rc;
    if (NumArguments != 3)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.Fill64(address, data, words)");
        return JS_FALSE;
    }
    if ((OK != (rc = pJavaScript->FromJsval(pArguments[0], &Address))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[1], &Data))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[2], &Words))))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.Fill64(address, data, words)");
        return JS_FALSE;
    }

    if (0 == Address)
    {
        Printf(Tee::PriHigh, "Write to phys mem addr 0 DENIED!\n");
        return JS_FALSE;
    }
    void * VirtualAddr = Platform::PhysicalToVirtual(Address);

    Memory::Fill64(VirtualAddr, Data, Words);

    RETURN_RC(OK);
}

// STest
C_(Memory_FillConstant)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    PHYSADDR Address;
    UINT32 Constant;
    UINT32 Width;
    UINT32 Height;
    UINT32 Depth;
    UINT32 Pitch;

    RC rc;
    if (NumArguments != 6)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.FillConstant("
                                "address, constant, width, height, depth, pitch)");
        return JS_FALSE;
    }
    if ((OK != (rc = pJavaScript->FromJsval(pArguments[0], &Address))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[1], &Constant))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[2], &Width))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[3], &Height))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[4], &Depth))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[5], &Pitch))))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.FillConstant("
                                "address, constant, width, height, depth, pitch)");
        return JS_FALSE;
    }

    if (0 == Address)
    {
        Printf(Tee::PriHigh, "Write to phys mem addr 0 DENIED!\n");
        return JS_FALSE;
    }
    void * VirtualAddr = Platform::PhysicalToVirtual(Address);

    RETURN_RC(Memory::FillConstant(VirtualAddr, Constant, Width, Height, Depth,
                                  Pitch));
}

// STest
C_(Memory_FillAddress)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    PHYSADDR Address;
    UINT32 Width;
    UINT32 Height;
    UINT32 Depth;
    UINT32 Pitch;

    RC rc;
    if (NumArguments != 5)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.FillConstant("
                                "address, width, height, depth, pitch)");
        return JS_FALSE;
    }
    if ((OK != (rc = pJavaScript->FromJsval(pArguments[0], &Address))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[1], &Width))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[2], &Height))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[3], &Depth))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[4], &Pitch))))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.FillConstant("
                            "address, width, height, depth, pitch)");
        return JS_FALSE;
    }

    if (0 == Address)
    {
        Printf(Tee::PriHigh, "Write to phys mem addr 0 DENIED!\n");
        return JS_FALSE;
    }
    void * VirtualAddr = Platform::PhysicalToVirtual(Address);

    RETURN_RC(Memory::FillAddress(VirtualAddr, Width, Height, Depth, Pitch));
}

// STest
C_(Memory_FillRandom)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    PHYSADDR Address;
    UINT32 Seed;
    UINT32 Low;
    UINT32 High;
    UINT32 Width;
    UINT32 Height;
    UINT32 Depth;
    UINT32 Pitch;

    RC rc;
    if (NumArguments != 8)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.FillRandom("
                            "address, seed, low, high, width, height, depth, pitch)");
        return JS_FALSE;
    }
    if ((OK != (rc = pJavaScript->FromJsval(pArguments[0], &Address))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[1], &Seed))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[2], &Low))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[3], &High))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[4], &Width))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[5], &Height))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[6], &Depth))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[7], &Pitch))))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.FillRandom("
                            "address, seed, low, high, width, height, depth, pitch)");
        return JS_FALSE;
    }

    if (0 == Address)
    {
        Printf(Tee::PriHigh, "Write to phys mem addr 0 DENIED!\n");
        return JS_FALSE;
    }
    void * VirtualAddr = Platform::PhysicalToVirtual(Address);

    RETURN_RC(Memory::FillRandom(VirtualAddr, Seed, Low, High, Width, Height,
                                Depth, Pitch));
}

// STest
C_(Memory_FillWeightedRandom)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    PHYSADDR Address;
    UINT32 Seed;
    UINT32 Width;
    UINT32 Height;
    UINT32 Depth;
    UINT32 Pitch;
    vector<Random::PickItem> PickItems;

    RC rc;
    if (NumArguments != 7)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.FillWeightedRandom("
                            "address, seed, PickItems[], width, height, depth, pitch)");
        return JS_FALSE;
    }
    if ((OK != (rc = pJavaScript->FromJsval(pArguments[0], &Address))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[1], &Seed))) ||
        (OK != (rc = Utility::PickItemsFromJsval(pArguments[2], &PickItems))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[3], &Width))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[4], &Height))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[5], &Depth))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[6], &Pitch))))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.FillWeightedRandom("
                            "address, seed, PickItems[], width, height, depth, pitch)");
        return JS_FALSE;
    }

    if (0 == Address)
    {
        Printf(Tee::PriHigh, "Write to phys mem addr 0 DENIED!\n");
        return JS_FALSE;
    }
    void * VirtualAddr = Platform::PhysicalToVirtual(Address);

    // NOTE: We are assuming vector<> is implemented as an array.
    rc = Memory::FillWeightedRandom(VirtualAddr, Seed, &PickItems[0],
                                      Width, Height, Depth, Pitch);

    // Expand RETURN_RC to fix compiler warning.
    // I do not understand why this needs to be done.
    *pReturlwalue = INT_TO_JSVAL(rc);
    return JS_TRUE;
}

// STest
C_(Memory_FillStripey)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    PHYSADDR Address;
    UINT32 Width;
    UINT32 Height;
    UINT32 Depth;
    UINT32 Pitch;

    RC rc;
    if (NumArguments != 5)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.FillStripey("
                            "address, width, height, depth, pitch)");
        return JS_FALSE;
    }
    if ((OK != (rc = pJavaScript->FromJsval(pArguments[0], &Address))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[1], &Width))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[2], &Height))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[3], &Depth))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[4], &Pitch))))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.FillStripey("
                            "address, width, height, depth, pitch)");
        return JS_FALSE;
    }

    if (0 == Address)
    {
        Printf(Tee::PriHigh, "Write to phys mem addr 0 DENIED!\n");
        return JS_FALSE;
    }
    void * VirtualAddr = Platform::PhysicalToVirtual(Address);

    RETURN_RC(Memory::FillStripey(VirtualAddr, Width, Height, Depth, Pitch));
}

// STest
C_(Memory_FillRgb)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    PHYSADDR Address;
    UINT32 Width;
    UINT32 Height;
    UINT32 Depth;
    UINT32 Pitch;

    RC rc;
    if (NumArguments != 5)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.FillRgb("
                            "address, width, height, depth, pitch)");
        return JS_FALSE;
    }
    if ((OK != (rc = pJavaScript->FromJsval(pArguments[0], &Address))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[1], &Width))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[2], &Height))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[3], &Depth))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[4], &Pitch))))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.FillRgb("
                            "address, width, height, depth, pitch)");
        return JS_FALSE;
    }

    if (0 == Address)
    {
        Printf(Tee::PriHigh, "Write to phys mem addr 0 DENIED!\n");
        return JS_FALSE;
    }
    void * VirtualAddr = Platform::PhysicalToVirtual(Address);

    RETURN_RC(Memory::FillRgb(VirtualAddr, Width, Height, Depth, Pitch));
}

C_(Memory_FillPattern32)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    PHYSADDR Address;
    JsArray JsPattern;
    vector<UINT32> Pattern;
    UINT32 ByteSize;

    RC rc;
    if (NumArguments != 3)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.FillPattern32("
                "PhysAddr, [pattern], BytesSize)");
        return JS_FALSE;
    }
    if ((OK != (rc = pJavaScript->FromJsval(pArguments[0], &Address))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[1], &JsPattern))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[2], &ByteSize))))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.FillPattern32("
                "PhysAddr, [pattern], BytesSize)");
        return JS_FALSE;
    }

    if (0 == Address)
    {
       Printf(Tee::PriHigh, "Write to phys mem addr 0 DENIED!\n");
       return JS_FALSE;
    }
    for(size_t i = 0; i < JsPattern.size(); ++i)
    {
        UINT32 val;
        C_CHECK_RC(pJavaScript->FromJsval(JsPattern[i], &val));
        Pattern.push_back(val);
    }
    for(size_t i = 0; i < Pattern.size(); ++i)
    {
        Printf(Tee::PriLow, "Pattern[%d]= 0x%08x\n", (int)i, Pattern[i]);
    }

    void * VirtualAddr = Platform::PhysicalToVirtual(Address);

    RETURN_RC(Memory::FillPattern(VirtualAddr, &Pattern, ByteSize));
}

C_(Memory_FillSurfaceWithPattern32)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    PHYSADDR Address;
    JsArray JsPattern;
    vector<UINT32> Pattern;
    UINT32 ByteWidth;
    UINT32 Lines;
    UINT32 Pitch;

    RC rc;
    if (NumArguments != 5)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.FillSurfaceWithPattern32("
                "PhysAddr, [pattern], WidthInBytes, Lines, PitchInBytes)");
        return JS_FALSE;
    }
    if ((OK != (rc = pJavaScript->FromJsval(pArguments[0], &Address))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[1], &JsPattern))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[2], &ByteWidth))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[3], &Lines))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[4], &Pitch))))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.FillSurfaceWithPattern32("
                "PhysAddr, [pattern], WidthInBytes, Lines, PitchInBytes)");
        return JS_FALSE;
    }

    if (0 == Address)
    {
       Printf(Tee::PriHigh, "Write to phys mem addr 0 DENIED!\n");
       return JS_FALSE;
    }
    for(size_t i = 0; i < JsPattern.size(); ++i)
    {
        UINT32 val;
        C_CHECK_RC(pJavaScript->FromJsval(JsPattern[i], &val));
        Pattern.push_back(val);
    }
    for(size_t i = 0; i < Pattern.size(); ++i)
    {
        Printf(Tee::PriLow, "Pattern[%d]= 0x%08x\n", (int)i, Pattern[i]);
    }

    void * VirtualAddr = Platform::PhysicalToVirtual(Address);

    RETURN_RC(Memory::FillSurfaceWithPattern(VirtualAddr, &Pattern, ByteWidth, Lines, Pitch));
}

// STest
C_(Memory_FillRgbBars)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    PHYSADDR Address;
    UINT32 Width;
    UINT32 Height;
    UINT32 Depth;
    UINT32 Pitch;

    RC rc;
    if (NumArguments != 5)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.FillRgbBars("
                            "address, width, height, depth, pitch)");
        return JS_FALSE;
    }
    if ((OK != (rc = pJavaScript->FromJsval(pArguments[0], &Address))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[1], &Width))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[2], &Height))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[3], &Depth))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[4], &Pitch))))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.FillRgbBars("
                            "address, width, height, depth, pitch)");
        return JS_FALSE;
    }

    if (0 == Address)
    {
        Printf(Tee::PriHigh, "Write to phys mem addr 0 DENIED!\n");
        return JS_FALSE;
    }
    void * VirtualAddr = Platform::PhysicalToVirtual(Address);

    ColorUtils::Format Format;
    switch (Depth)
    {
        case 16:
            Format = ColorUtils::R5G6B5;
            break;
        case 32:
            Format = ColorUtils::A8R8G8B8;
            break;
        default:
            Printf(Tee::PriHigh, "Depth = %d is not suppoted\n", Depth);
            return JS_FALSE;
    }

    RETURN_RC(Memory::FillRgbBars(VirtualAddr, Width, Height, Format, Pitch));
}

// STest
C_(Memory_FillStripes)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    PHYSADDR Address;
    UINT32 Width;
    UINT32 Height;
    UINT32 Depth;
    UINT32 Pitch;

    RC rc;
    if (NumArguments != 5)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.FillStripes("
                            "address, width, height, depth, pitch)");
        return JS_FALSE;
    }
    if ((OK != (rc = pJavaScript->FromJsval(pArguments[0], &Address))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[1], &Width))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[2], &Height))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[3], &Depth))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[4], &Pitch))))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.FillStripes(address, width, height, depth, pitch)");
        return JS_FALSE;
    }

    if (0 == Address)
    {
        Printf(Tee::PriHigh, "Write to phys mem addr 0 DENIED!\n");
        return JS_FALSE;
    }
    void * VirtualAddr = Platform::PhysicalToVirtual(Address);

    RETURN_RC(Memory::FillStripes(VirtualAddr, Width, Height, Depth, Pitch));
}

// STest
C_(Memory_FillSine)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    PHYSADDR Address;
    FLOAT64 Period;
    UINT32 Width;
    UINT32 Height;
    UINT32 Depth;
    UINT32 Pitch;

    RC rc;
    if (NumArguments != 6)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.FillSine("
                            "address, period, width, height, depth, pitch)");
        return JS_FALSE;
    }
    if ((OK != (rc = pJavaScript->FromJsval(pArguments[0], &Address))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[1], &Period))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[2], &Width))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[3], &Height))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[4], &Depth))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[5], &Pitch))))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.FillSine("
                            "address, period, width, height, depth, pitch)");
        return JS_FALSE;
    }

    if (0 == Address)
    {
        Printf(Tee::PriHigh, "Write to phys mem addr 0 DENIED!\n");
        return JS_FALSE;
    }
    void * VirtualAddr = Platform::PhysicalToVirtual(Address);

    RETURN_RC(Memory::FillSine(VirtualAddr, Period, Width, Height, Depth,
                                        Pitch));
}

// STest
C_(Memory_ReadYuv)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    string FileName;
    UINT32 DoSwapBytes;
    PHYSADDR Address;
    UINT32 Width;
    UINT32 Height;
    UINT32 Pitch;
    UINT32 Dummy;

    RC rc;
    if (NumArguments != 6)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.ReadYuv("
                            "filename, doSwapBytes, address, width, height, pitch)");
        return JS_FALSE;
    }
    if ((OK != (rc = pJavaScript->FromJsval(pArguments[0], &FileName))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[1], &DoSwapBytes))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[2], &Address))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[3], &Width))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[4], &Height))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[5], &Pitch))))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.ReadYuv("
                            "filename, doSwapBytes, address, width, height, pitch)");
        return JS_FALSE;
    }

    if (0 == Address)
    {
        Printf(Tee::PriHigh, "Write to phys mem addr 0 DENIED!\n");
        return JS_FALSE;
    }
    void * VirtualAddr = Platform::PhysicalToVirtual(Address);

    RETURN_RC(ImageFile::ReadYuv(
                               FileName.c_str(),
                               (0 != DoSwapBytes),
                               false,            // tile image if smaller than screen
                               VirtualAddr,
                               Width,
                               Height,
                               Pitch,
                               &Dummy,
                               &Dummy));
}

// STest
C_(Memory_ReadTga)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    string FileName;
    PHYSADDR Address;
    UINT32 Width;
    UINT32 Height;
    UINT32 Depth;
    UINT32 Pitch;

    RC rc;
    if (NumArguments != 6)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.ReadTga("
                            "filename, address, width, height, depth, pitch)");
        return JS_FALSE;
    }
    if ((OK != (rc = pJavaScript->FromJsval(pArguments[0], &FileName))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[1], &Address))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[2], &Width))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[3], &Height))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[4], &Depth))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[5], &Pitch))))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.ReadTga("
                            "filename, address, width, height, depth, pitch)");
        return JS_FALSE;
    }

    if (0 == Address)
    {
        Printf(Tee::PriHigh, "Write to phys mem addr 0 DENIED!\n");
        return JS_FALSE;
    }
    void * VirtualAddr = Platform::PhysicalToVirtual(Address);

    ColorUtils::Format cfmt;
    switch (Depth)
    {
        default:
        case 32: cfmt = ColorUtils::A8R8G8B8;    break;
        case 16: cfmt = ColorUtils::R5G6B5;      break;
        case 15: cfmt = ColorUtils::Z1R5G5B5;    break;
    }
    RETURN_RC(ImageFile::ReadTga(
                               FileName.c_str(),
                               true,            // tile image if smaller than screen
                               cfmt,
                               VirtualAddr,
                               Width,
                               Height,
                               Pitch ));
}

// STest
C_(Memory_WriteTga)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    string FileName;
    UINT32 AlphaToRgb;
    PHYSADDR Address;
    UINT32 Width;
    UINT32 Height;
    UINT32 Depth;
    UINT32 Pitch;

    RC rc;
    if (NumArguments != 7)
    {
        pJavaScript->Throw(pContext, RC::BAD_PARAMETER, "Usage: Memory.WriteTga("
                            "filename, CopyAlphaToRgb, address, width, height, depth, pitch)");
        return JS_FALSE;
    }
    if ((OK != (rc = pJavaScript->FromJsval(pArguments[0], &FileName))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[1], &AlphaToRgb))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[2], &Address))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[3], &Width))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[4], &Height))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[5], &Depth))) ||
        (OK != (rc = pJavaScript->FromJsval(pArguments[6], &Pitch))))
    {
        pJavaScript->Throw(pContext, rc, "Usage: Memory.WriteTga("
                            "filename, CopyAlphaToRgb, address, width, height, depth, pitch)");
        return JS_FALSE;
    }

    bool AtoRGB = (AlphaToRgb != 0);

    void * VirtualAddr = Platform::PhysicalToVirtual(Address);

    ColorUtils::Format cfmt;
    switch (Depth)
    {
        default:
        case 32: cfmt = ColorUtils::A8R8G8B8;    break;
        case 16: cfmt = ColorUtils::R5G6B5;      break;
        case 15: cfmt = ColorUtils::Z1R5G5B5;    break;
    }
    RETURN_RC(ImageFile::WriteTga(
                                FileName.c_str(),
                                cfmt,
                                VirtualAddr,
                                Width,
                                Height,
                                Pitch,
                                AtoRGB));
}

#endif  // VULKAN_STANDALONE

#endif
