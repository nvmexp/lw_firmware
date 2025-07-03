/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef GR_RANDOM_H
#define GR_RANDOM_H

#include "types.h"

class GrRandom
{
private:
    UINT32 RandData;

public:
    GrRandom();

    // Returns a 32-bit random number
    UINT32 LWRandom();

    // Returns a random number between low & high (inclusive)
    UINT32 RandomRoll(UINT32 from, UINT32 to);
};

#endif // GR_RANDOM_H
