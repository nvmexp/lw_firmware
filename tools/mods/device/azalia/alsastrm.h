/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_ALSASTRM_H
#define INCLUDED_ALSASTRM_H

#include "azastrm.h"

class AlsaStream : public AzaStream
{
public:
    AlsaStream();
    ~AlsaStream();

    void InitParams();
    void InitBuffer(UINT32 NumBytes);
    void AddSamples(void *Samples, UINT32 NumBytes);
    bool IsFinished();
};

#endif // INCLUDED_ALSASTRM_H
