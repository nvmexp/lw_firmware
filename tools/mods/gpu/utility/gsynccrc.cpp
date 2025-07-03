/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/platform.h"
#include "core/include/rc.h"
#include "core/include/utility.h"
#include "gpu/include/gsynccrc.h"

namespace
{
#include "gsynccrcgen.hpp"

    void GsyncR2LWCrc16_24Gen
    (
        const UINT08 crcIn[]
        , const UINT08 dIn[]
        , UINT08 crcOut[]
    )
    {
        // The below "hw" equations are from Tom Verbeure <tverbeure@lwpu.com>

        // I callwlate 4 CRC32s (the Ethernet one) for each
        //  vertical column modulo 4.
        // That is: CRC0 as pixel 0, 4, 8,... Etc. CRC1 has 1, 5, 9,...

        // "The polynomial is the following one:
        // crc[31:0]=1+x^1+x^2+x^4+x^5+x^7+x^8+x^10+x^11+x^12+x^16+x^22+x^23+x^26+x^32;
        // The LFSR is initialized with a value of 1 at vsync.
        // The 24-bit input is arranged as ( blue[7:0] << 16 | green[7:0] << 8 | red[7:0] )

        // To check that your algorithm is correct, if you feed the CRC engine
        // with the following sequence:
        //  0x000000
        //  0x000004
        //  0x000008
        //  0x00000c
        //  0x000010

        // You will get the following outputs:
        //  0x01000000
        //  0x12dcda5b
        //  0x41d5acc5
        //  0x077a14fb
        //  0xb6012d6d

        // Parallelized with a 24-bit wide input, this results in this:

        crcOut[0] = crcIn[8] ^ crcIn[14] ^ crcIn[17] ^ crcIn[18] ^ crcIn[20] ^ crcIn[24] ^ dIn[0] ^ dIn[6] ^ dIn[9] ^ dIn[10] ^ dIn[12] ^ dIn[16];
        crcOut[1] = crcIn[8] ^ crcIn[9] ^ crcIn[14] ^ crcIn[15] ^ crcIn[17] ^ crcIn[19] ^ crcIn[20] ^ crcIn[21] ^ crcIn[24] ^ crcIn[25] ^ dIn[0] ^ dIn[1] ^ dIn[6] ^ dIn[7] ^ dIn[9] ^ dIn[11] ^ dIn[12] ^ dIn[13] ^ dIn[16] ^ dIn[17];
        crcOut[2] = crcIn[8] ^ crcIn[9] ^ crcIn[10] ^ crcIn[14] ^ crcIn[15] ^ crcIn[16] ^ crcIn[17] ^ crcIn[21] ^ crcIn[22] ^ crcIn[24] ^ crcIn[25] ^ crcIn[26] ^ dIn[0] ^ dIn[1] ^ dIn[2] ^ dIn[6] ^ dIn[7] ^ dIn[8] ^ dIn[9] ^ dIn[13] ^ dIn[14] ^ dIn[16] ^ dIn[17] ^ dIn[18];
        crcOut[3] = crcIn[9] ^ crcIn[10] ^ crcIn[11] ^ crcIn[15] ^ crcIn[16] ^ crcIn[17] ^ crcIn[18] ^ crcIn[22] ^ crcIn[23] ^ crcIn[25] ^ crcIn[26] ^ crcIn[27] ^ dIn[1] ^ dIn[2] ^ dIn[3] ^ dIn[7] ^ dIn[8] ^ dIn[9] ^ dIn[10] ^ dIn[14] ^ dIn[15] ^ dIn[17] ^ dIn[18] ^ dIn[19];
        crcOut[4] = crcIn[8] ^ crcIn[10] ^ crcIn[11] ^ crcIn[12] ^ crcIn[14] ^ crcIn[16] ^ crcIn[19] ^ crcIn[20] ^ crcIn[23] ^ crcIn[26] ^ crcIn[27] ^ crcIn[28] ^ dIn[0] ^ dIn[2] ^ dIn[3] ^ dIn[4] ^ dIn[6] ^ dIn[8] ^ dIn[11] ^ dIn[12] ^ dIn[15] ^ dIn[18] ^ dIn[19] ^ dIn[20];
        crcOut[5] = crcIn[8] ^ crcIn[9] ^ crcIn[11] ^ crcIn[12] ^ crcIn[13] ^ crcIn[14] ^ crcIn[15] ^ crcIn[18] ^ crcIn[21] ^ crcIn[27] ^ crcIn[28] ^ crcIn[29] ^ dIn[0] ^ dIn[1] ^ dIn[3] ^ dIn[4] ^ dIn[5] ^ dIn[6] ^ dIn[7] ^ dIn[10] ^ dIn[13] ^ dIn[19] ^ dIn[20] ^ dIn[21];
        crcOut[6] = crcIn[9] ^ crcIn[10] ^ crcIn[12] ^ crcIn[13] ^ crcIn[14] ^ crcIn[15] ^ crcIn[16] ^ crcIn[19] ^ crcIn[22] ^ crcIn[28] ^ crcIn[29] ^ crcIn[30] ^ dIn[1] ^ dIn[2] ^ dIn[4] ^ dIn[5] ^ dIn[6] ^ dIn[7] ^ dIn[8] ^ dIn[11] ^ dIn[14] ^ dIn[20] ^ dIn[21] ^ dIn[22];
        crcOut[7] = crcIn[8] ^ crcIn[10] ^ crcIn[11] ^ crcIn[13] ^ crcIn[15] ^ crcIn[16] ^ crcIn[18] ^ crcIn[23] ^ crcIn[24] ^ crcIn[29] ^ crcIn[30] ^ crcIn[31] ^ dIn[0] ^ dIn[2] ^ dIn[3] ^ dIn[5] ^ dIn[7] ^ dIn[8] ^ dIn[10] ^ dIn[15] ^ dIn[16] ^ dIn[21] ^ dIn[22] ^ dIn[23];
        crcOut[8] = crcIn[8] ^ crcIn[9] ^ crcIn[11] ^ crcIn[12] ^ crcIn[16] ^ crcIn[18] ^ crcIn[19] ^ crcIn[20] ^ crcIn[25] ^ crcIn[30] ^ crcIn[31] ^ dIn[0] ^ dIn[1] ^ dIn[3] ^ dIn[4] ^ dIn[8] ^ dIn[10] ^ dIn[11] ^ dIn[12] ^ dIn[17] ^ dIn[22] ^ dIn[23];
        crcOut[9] = crcIn[9] ^ crcIn[10] ^ crcIn[12] ^ crcIn[13] ^ crcIn[17] ^ crcIn[19] ^ crcIn[20] ^ crcIn[21] ^ crcIn[26] ^ crcIn[31] ^ dIn[1] ^ dIn[2] ^ dIn[4] ^ dIn[5] ^ dIn[9] ^ dIn[11] ^ dIn[12] ^ dIn[13] ^ dIn[18] ^ dIn[23];
        crcOut[10] = crcIn[8] ^ crcIn[10] ^ crcIn[11] ^ crcIn[13] ^ crcIn[17] ^ crcIn[21] ^ crcIn[22] ^ crcIn[24] ^ crcIn[27] ^ dIn[0] ^ dIn[2] ^ dIn[3] ^ dIn[5] ^ dIn[9] ^ dIn[13] ^ dIn[14] ^ dIn[16] ^ dIn[19];
        crcOut[11] = crcIn[8] ^ crcIn[9] ^ crcIn[11] ^ crcIn[12] ^ crcIn[17] ^ crcIn[20] ^ crcIn[22] ^ crcIn[23] ^ crcIn[24] ^ crcIn[25] ^ crcIn[28] ^ dIn[0] ^ dIn[1] ^ dIn[3] ^ dIn[4] ^ dIn[9] ^ dIn[12] ^ dIn[14] ^ dIn[15] ^ dIn[16] ^ dIn[17] ^ dIn[20];
        crcOut[12] = crcIn[8] ^ crcIn[9] ^ crcIn[10] ^ crcIn[12] ^ crcIn[13] ^ crcIn[14] ^ crcIn[17] ^ crcIn[20] ^ crcIn[21] ^ crcIn[23] ^ crcIn[25] ^ crcIn[26] ^ crcIn[29] ^ dIn[0] ^ dIn[1] ^ dIn[2] ^ dIn[4] ^ dIn[5] ^ dIn[6] ^ dIn[9] ^ dIn[12] ^ dIn[13] ^ dIn[15] ^ dIn[17] ^ dIn[18] ^ dIn[21];
        crcOut[13] = crcIn[9] ^ crcIn[10] ^ crcIn[11] ^ crcIn[13] ^ crcIn[14] ^ crcIn[15] ^ crcIn[18] ^ crcIn[21] ^ crcIn[22] ^ crcIn[24] ^ crcIn[26] ^ crcIn[27] ^ crcIn[30] ^ dIn[1] ^ dIn[2] ^ dIn[3] ^ dIn[5] ^ dIn[6] ^ dIn[7] ^ dIn[10] ^ dIn[13] ^ dIn[14] ^ dIn[16] ^ dIn[18] ^ dIn[19] ^ dIn[22];
        crcOut[14] = crcIn[10] ^ crcIn[11] ^ crcIn[12] ^ crcIn[14] ^ crcIn[15] ^ crcIn[16] ^ crcIn[19] ^ crcIn[22] ^ crcIn[23] ^ crcIn[25] ^ crcIn[27] ^ crcIn[28] ^ crcIn[31] ^ dIn[2] ^ dIn[3] ^ dIn[4] ^ dIn[6] ^ dIn[7] ^ dIn[8] ^ dIn[11] ^ dIn[14] ^ dIn[15] ^ dIn[17] ^ dIn[19] ^ dIn[20] ^ dIn[23];
        crcOut[15] = crcIn[11] ^ crcIn[12] ^ crcIn[13] ^ crcIn[15] ^ crcIn[16] ^ crcIn[17] ^ crcIn[20] ^ crcIn[23] ^ crcIn[24] ^ crcIn[26] ^ crcIn[28] ^ crcIn[29] ^ dIn[3] ^ dIn[4] ^ dIn[5] ^ dIn[7] ^ dIn[8] ^ dIn[9] ^ dIn[12] ^ dIn[15] ^ dIn[16] ^ dIn[18] ^ dIn[20] ^ dIn[21];
        crcOut[16] = crcIn[8] ^ crcIn[12] ^ crcIn[13] ^ crcIn[16] ^ crcIn[20] ^ crcIn[21] ^ crcIn[25] ^ crcIn[27] ^ crcIn[29] ^ crcIn[30] ^ dIn[0] ^ dIn[4] ^ dIn[5] ^ dIn[8] ^ dIn[12] ^ dIn[13] ^ dIn[17] ^ dIn[19] ^ dIn[21] ^ dIn[22];
        crcOut[17] = crcIn[9] ^ crcIn[13] ^ crcIn[14] ^ crcIn[17] ^ crcIn[21] ^ crcIn[22] ^ crcIn[26] ^ crcIn[28] ^ crcIn[30] ^ crcIn[31] ^ dIn[1] ^ dIn[5] ^ dIn[6] ^ dIn[9] ^ dIn[13] ^ dIn[14] ^ dIn[18] ^ dIn[20] ^ dIn[22] ^ dIn[23];
        crcOut[18] = crcIn[10] ^ crcIn[14] ^ crcIn[15] ^ crcIn[18] ^ crcIn[22] ^ crcIn[23] ^ crcIn[27] ^ crcIn[29] ^ crcIn[31] ^ dIn[2] ^ dIn[6] ^ dIn[7] ^ dIn[10] ^ dIn[14] ^ dIn[15] ^ dIn[19] ^ dIn[21] ^ dIn[23];
        crcOut[19] = crcIn[11] ^ crcIn[15] ^ crcIn[16] ^ crcIn[19] ^ crcIn[23] ^ crcIn[24] ^ crcIn[28] ^ crcIn[30] ^ dIn[3] ^ dIn[7] ^ dIn[8] ^ dIn[11] ^ dIn[15] ^ dIn[16] ^ dIn[20] ^ dIn[22];
        crcOut[20] = crcIn[12] ^ crcIn[16] ^ crcIn[17] ^ crcIn[20] ^ crcIn[24] ^ crcIn[25] ^ crcIn[29] ^ crcIn[31] ^ dIn[4] ^ dIn[8] ^ dIn[9] ^ dIn[12] ^ dIn[16] ^ dIn[17] ^ dIn[21] ^ dIn[23];
        crcOut[21] = crcIn[13] ^ crcIn[17] ^ crcIn[18] ^ crcIn[21] ^ crcIn[25] ^ crcIn[26] ^ crcIn[30] ^ dIn[5] ^ dIn[9] ^ dIn[10] ^ dIn[13] ^ dIn[17] ^ dIn[18] ^ dIn[22];
        crcOut[22] = crcIn[8] ^ crcIn[17] ^ crcIn[19] ^ crcIn[20] ^ crcIn[22] ^ crcIn[24] ^ crcIn[26] ^ crcIn[27] ^ crcIn[31] ^ dIn[0] ^ dIn[9] ^ dIn[11] ^ dIn[12] ^ dIn[14] ^ dIn[16] ^ dIn[18] ^ dIn[19] ^ dIn[23];
        crcOut[23] = crcIn[8] ^ crcIn[9] ^ crcIn[14] ^ crcIn[17] ^ crcIn[21] ^ crcIn[23] ^ crcIn[24] ^ crcIn[25] ^ crcIn[27] ^ crcIn[28] ^ dIn[0] ^ dIn[1] ^ dIn[6] ^ dIn[9] ^ dIn[13] ^ dIn[15] ^ dIn[16] ^ dIn[17] ^ dIn[19] ^ dIn[20];
        crcOut[24] = crcIn[0] ^ crcIn[9] ^ crcIn[10] ^ crcIn[15] ^ crcIn[18] ^ crcIn[22] ^ crcIn[24] ^ crcIn[25] ^ crcIn[26] ^ crcIn[28] ^ crcIn[29] ^ dIn[1] ^ dIn[2] ^ dIn[7] ^ dIn[10] ^ dIn[14] ^ dIn[16] ^ dIn[17] ^ dIn[18] ^ dIn[20] ^ dIn[21];
        crcOut[25] = crcIn[1] ^ crcIn[10] ^ crcIn[11] ^ crcIn[16] ^ crcIn[19] ^ crcIn[23] ^ crcIn[25] ^ crcIn[26] ^ crcIn[27] ^ crcIn[29] ^ crcIn[30] ^ dIn[2] ^ dIn[3] ^ dIn[8] ^ dIn[11] ^ dIn[15] ^ dIn[17] ^ dIn[18] ^ dIn[19] ^ dIn[21] ^ dIn[22];
        crcOut[26] = crcIn[2] ^ crcIn[8] ^ crcIn[11] ^ crcIn[12] ^ crcIn[14] ^ crcIn[18] ^ crcIn[26] ^ crcIn[27] ^ crcIn[28] ^ crcIn[30] ^ crcIn[31] ^ dIn[0] ^ dIn[3] ^ dIn[4] ^ dIn[6] ^ dIn[10] ^ dIn[18] ^ dIn[19] ^ dIn[20] ^ dIn[22] ^ dIn[23];
        crcOut[27] = crcIn[3] ^ crcIn[9] ^ crcIn[12] ^ crcIn[13] ^ crcIn[15] ^ crcIn[19] ^ crcIn[27] ^ crcIn[28] ^ crcIn[29] ^ crcIn[31] ^ dIn[1] ^ dIn[4] ^ dIn[5] ^ dIn[7] ^ dIn[11] ^ dIn[19] ^ dIn[20] ^ dIn[21] ^ dIn[23];
        crcOut[28] = crcIn[4] ^ crcIn[10] ^ crcIn[13] ^ crcIn[14] ^ crcIn[16] ^ crcIn[20] ^ crcIn[28] ^ crcIn[29] ^ crcIn[30] ^ dIn[2] ^ dIn[5] ^ dIn[6] ^ dIn[8] ^ dIn[12] ^ dIn[20] ^ dIn[21] ^ dIn[22];
        crcOut[29] = crcIn[5] ^ crcIn[11] ^ crcIn[14] ^ crcIn[15] ^ crcIn[17] ^ crcIn[21] ^ crcIn[29] ^ crcIn[30] ^ crcIn[31] ^ dIn[3] ^ dIn[6] ^ dIn[7] ^ dIn[9] ^ dIn[13] ^ dIn[21] ^ dIn[22] ^ dIn[23];
        crcOut[30] = crcIn[6] ^ crcIn[12] ^ crcIn[15] ^ crcIn[16] ^ crcIn[18] ^ crcIn[22] ^ crcIn[30] ^ crcIn[31] ^ dIn[4] ^ dIn[7] ^ dIn[8] ^ dIn[10] ^ dIn[14] ^ dIn[22] ^ dIn[23];
        crcOut[31] = crcIn[7] ^ crcIn[13] ^ crcIn[16] ^ crcIn[17] ^ crcIn[19] ^ crcIn[23] ^ crcIn[31] ^ dIn[5] ^ dIn[8] ^ dIn[9] ^ dIn[11] ^ dIn[15] ^ dIn[23];
    }
};

//! CRC is callwlated for column%4 of pixels in a pitch surface
const UINT32 GsyncCRC::s_NumCrcPixelGroups = 4;
const UINT32 GsyncCRC::s_CrcWidth          = 16;
const UINT32 GsyncCRC::s_CrcWords          = 2;

//-----------------------------------------------------------------------------
GsyncCRC::GsyncCRC(bool isGsyncR3):
    m_IsR3(isGsyncR3)
    , m_CrcPixelWidth(isGsyncR3 ? 48 : 24)
{}

//-----------------------------------------------------------------------------
RC GsyncCRC::ColwertToGsyncAlignedFormat(ColorFmt fmt, UINT64 inPixel, UINT08 outPixel[])
{
    if ((ColorUtils::A8R8G8B8 != fmt) &&
        (ColorUtils::A8B8G8R8 != fmt) &&
        (ColorUtils::B8_G8_R8 != fmt))
    {
        Printf(Tee::PriError, "Gsync crc callwlation is lwrrently only supported"
                " for A8R8GB8/A8R8G8B8/B8_G8_R8 input surface format.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    UINT64 val = 0;
    if (m_IsR3)
    {
        //All components are pushed to Msb
        val = ((inPixel & 0xFF0000) << 24) |
            ((inPixel & 0x00FF00) << 16) |
            ((inPixel & 0x0000FF) << 8);
    }
    else
    {
        //remove alpha component
        val = inPixel & 0x00FFFFFF;
    }
    NumToBitArray(val, outPixel, m_CrcPixelWidth);
    return OK;
}

//-----------------------------------------------------------------------------
RC GsyncCRC::GenGsyncCrc
(
    const UINT08 crcIn[]
    , const UINT64 inPixel
    , UINT08 crcOut[]
    , ColorFmt fmt
)
{
    RC rc;
    vector <UINT08> pixel(m_CrcPixelWidth);
    CHECK_RC(ColwertToGsyncAlignedFormat(fmt, inPixel, &pixel[0]));
    GsyncR2LWCrc16_24Gen(crcIn, &pixel[0], crcOut);
    return rc;
}

//-----------------------------------------------------------------------------
RC GsyncCRC::GenGsyncR3Crc
(
    const UINT08 crcIn[]
    , const UINT64 inPixel
    , UINT08 crcOut[]
    , ColorFmt fmt
)
{
    RC rc;
    vector <UINT08> pixel(m_CrcPixelWidth);
    CHECK_RC(ColwertToGsyncAlignedFormat(fmt, inPixel, &pixel[0]));
    LWODCrc16_24Gen(crcIn, &pixel[0], crcOut); //CRC LSB
    LWODCrc16_24Gen(&crcIn[s_CrcWidth], &pixel[m_CrcPixelWidth/2], &crcOut[s_CrcWidth]); //CRC MSB
    return rc;
}

//-----------------------------------------------------------------------------
RC GsyncCRC::GenCrc(const Surface2D *pSurf, UINT32 *pRetCrcs)
{
    RC rc;

    const UINT32 initCrc = m_IsR3 ? 0x00010001 : 0x1;
    UINT08 crcOut[s_NumCrcPixelGroups][s_CrcWords * s_CrcWidth];
    UINT08 crcIn[s_NumCrcPixelGroups][s_CrcWords * s_CrcWidth]; 
    UINT08 *pCrcNext[s_NumCrcPixelGroups];
    UINT08 *pCrcLwrrent[s_NumCrcPixelGroups];

    for (UINT32 i = 0; i < s_NumCrcPixelGroups; i++)
    {
        NumToBitArray(initCrc, crcIn[i], s_CrcWords * s_CrcWidth);
        NumToBitArray(initCrc, crcOut[i], s_CrcWords * s_CrcWidth);
        pCrcNext[i] = crcOut[i];
        pCrcLwrrent[i] = crcIn[i];
    }
    const UINT32 ht = pSurf->GetHeight();
    const UINT32 wd = pSurf->GetWidth();
    const ColorFmt fmt = pSurf->GetColorFormat();
    vector<UINT32> row(wd);
    for (UINT32 rowIdx = 0; rowIdx < ht; rowIdx++)
    {
        UINT08 *pRow = static_cast <UINT08 *> (pSurf->GetAddress())
            + pSurf->GetPixelOffset(0, rowIdx);
        Platform::VirtualRd(pRow,
                &row[0],
                wd * sizeof(row[0]));

        for (UINT32 col = 0; col < wd; col++)
        {
            UINT32 group = col % s_NumCrcPixelGroups;
            if (m_IsR3)
            {
                CHECK_RC(GenGsyncR3Crc(pCrcLwrrent[group], row[col], pCrcNext[group], fmt));
            }
            else
            {
                CHECK_RC(GenGsyncCrc(pCrcNext[group], row[col], pCrcLwrrent[group], fmt));
            }
            UINT08 *pTemp = pCrcLwrrent[group];
            pCrcLwrrent[group] = pCrcNext[group];
            pCrcNext[group] = pTemp;
        }
    }

    for (UINT32 col = 0; col < s_NumCrcPixelGroups; col++)
    {
        pRetCrcs[col] = 0;
        for (UINT32 i = 0; i < s_CrcWords * s_CrcWidth; i++)
        {
            if (pCrcLwrrent[col][i])
            {
                pRetCrcs[col] |= 1 << i ;
            }
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
void GsyncCRC::NumToBitArray(const UINT64 num, UINT08 bits[], const UINT32 width)
{
    memset(bits, 0, width);
    INT32 i = -1;

    while (-1 != (i = Utility::BitScanForward64(num, i + 1)))
    {
        if (static_cast<UINT32> (i) >= width)
            break;
        bits[i] = 1;
    }
}
