/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _CRCINFO_H_
#define _CRCINFO_H_

namespace CrcEnums
{
    enum CRC_MODE
    {
        CRC_NONE =       0,
        CRC_REPORT =     1,
        CRC_CHECK =      2,
        CRC_DUMP =       3,
        CRC_UPDATE =     4,
        INTERRUPT_ONLY = 5,
        CRC_ENDMARK =    5
    };

    enum CRC_STATUS
    {
        LEAD = 0,
        GOLD = 1,
        MISS = 2,
        FAIL = 3,
    };
};

#endif // _CRCINFO_H_
