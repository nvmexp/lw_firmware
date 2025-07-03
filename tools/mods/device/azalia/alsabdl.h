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

#ifndef INCLUDED_ALSABDL_H
#define INCLUDED_ALSABDL_H

#include "azabdl.h"

class AlsaBDL : public AzaBDL
{
public:
    AlsaBDL();

    virtual RC Print();

    virtual UINT32 GetBufferLengthInBytes();
    virtual RC ReadSampleList(UINT32  StartSampleIndex,
                              void*   Samples,
                              UINT32  nSamplesInArray,
                              UINT32* nSamplesRead,
                              UINT32  BytesPerSample);

    void InitBuffer(UINT32 NumBytes);
    void AddSamples(void *Samples, UINT32 NumBytes);
    bool IsFinished();

private:
    std::vector<UINT08> m_Samples;
    UINT32 m_NextSampleOffset;
};

#endif
