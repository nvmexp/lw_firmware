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

#include "alsabdl.h"

AlsaBDL::AlsaBDL()
: AzaBDL(0, Memory::WB, false, 0)
, m_NextSampleOffset(0)
{
}

RC AlsaBDL::Print()
{
    return OK;
}

UINT32 AlsaBDL::GetBufferLengthInBytes()
{
    return m_Samples.size();
}

RC AlsaBDL::ReadSampleList
(
    UINT32 StartSampleIndex,
    void *Samples,
    UINT32 nSamplesInArray,
    UINT32* nSamplesRead,
    UINT32 BytesPerSample
)
{
    UINT32 Index = StartSampleIndex * BytesPerSample;
    if (Index > GetBufferLengthInBytes())
    {
        *nSamplesRead = 0;
        return OK;
    }

    UINT32 Size = min(nSamplesInArray * BytesPerSample,
        GetBufferLengthInBytes()-Index);

    memcpy(Samples, &m_Samples[Index], Size);
    *nSamplesRead = Size/BytesPerSample;

    return OK;
}

void AlsaBDL::InitBuffer(UINT32 NumBytes)
{
    m_Samples.resize(NumBytes);
    m_NextSampleOffset = 0;
}

void AlsaBDL::AddSamples(void *Samples, UINT32 NumBytes)
{
    UINT32 RemainingCapacity = (UINT32)m_Samples.size()-m_NextSampleOffset;
    UINT32 SizeToCopy = min(NumBytes, RemainingCapacity);
    memcpy(&m_Samples[m_NextSampleOffset], Samples, SizeToCopy);
    m_NextSampleOffset += SizeToCopy;
}

bool AlsaBDL::IsFinished()
{
    return (m_NextSampleOffset >= m_Samples.size());
}
