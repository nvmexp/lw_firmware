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
#pragma once
#include "gpu/utility/surf2d.h"
using ColorFmt = ColorUtils::Format;
class GsyncCRC
{
public:
    GsyncCRC() = delete;
    explicit GsyncCRC(bool isGsyncR3);
    RC GenCrc(const Surface2D *pSurf, UINT32 *pRetCrcs);
private:
    const bool m_IsR3 = true;
    const UINT32 m_CrcPixelWidth = 48;
    static const UINT32 s_CrcWords;
    static const UINT32 s_CrcWidth;
    static const UINT32 s_NumCrcPixelGroups;
    void NumToBitArray(const UINT64 num, UINT08 out[], const UINT32 width);
    RC ColwertToGsyncAlignedFormat(ColorFmt fmt, UINT64 inPixel, UINT08 outPixel[]);
    RC GenGsyncR3Crc
    (
        const UINT08 crcIn[]
        , const UINT64 inPixel
        , UINT08 crcOut[]
        , ColorFmt fmt
    );
    RC GenGsyncCrc
    (
        const UINT08 crcIn[]
        , const UINT64 pixel
        , UINT08 crcOut[]
        , ColorFmt fmt
    );
};
