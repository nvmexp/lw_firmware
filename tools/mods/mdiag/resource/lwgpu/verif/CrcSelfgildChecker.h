/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_CRCSELFGILDCHECKER_H
#define INCLUDED_CRCSELFGILDCHECKER_H

#include "mdiag/utils/CrcInfo.h"
#include "CrcChecker.h"
#include "mdiag/tests/gpu/trace_3d/selfgild.h"

class Surface2D;
class CheckDmaBuffer;
class ITestStateReport;

class CrcSelfgildChecker: public CrcChecker
{
public:
    typedef vector<SelfgildState*> SelfgildStates;

    virtual bool Verify
    (
        const char *key, // Key used to find stored crcs
        UINT32 measuredCrc, // Measured crc
        SurfaceInfo* surfInfo,
        UINT32 subdev,
        ITestStateReport *report, // File to touch in case of error
        CrcStatus *crcStatus // Result of comparison
    );
};

#endif /* INCLUDED_CRCSELFGILDCHECKER_H */
