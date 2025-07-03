/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_EXTCRCDEV_H
#define INCLUDED_EXTCRCDEV_H

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

// Pure virtual interface definition for an external CRC device used with
// golden values
class CrcGoldenInterface
{
public:
    virtual ~CrcGoldenInterface() {}
    virtual UINT32 NumCrcBins() const = 0;
    virtual RC GetCrcSignature(string *pCrcSignature) = 0;
    virtual RC GetCrcValues(UINT32 *pCrcs) = 0;
    virtual void ReportCrcMiscompare
    (
        UINT32 bin,
        UINT32 expectedCRC,
        UINT32 actualCRC
    ) = 0;
};

#endif
