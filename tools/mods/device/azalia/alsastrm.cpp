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

#include "alsastrm.h"
#include "alsabdl.h"

AlsaStream::AlsaStream()
: AzaStream(NULL, NULL, AzaliaDmaEngine::DIR_INPUT, 0)
{
}

AlsaStream::~AlsaStream()
{
    Reset(false);
}

void AlsaStream::InitParams()
{
    Reset(false);

    m_pFormat = new AzaliaFormat(input.type, input.size,
                                 input.rate, input.channels);
    MASSERT(m_pFormat);

    m_Stats = (AudioUtil::SineStatistics*) malloc(
        sizeof(AudioUtil::SineStatistics) *
        m_pFormat->GetChannelsAsInt());

    MASSERT(m_Stats);
    memset(m_Stats,
           0,
           sizeof(AudioUtil::SineStatistics) * m_pFormat->GetChannelsAsInt());

    m_pBdl = new AlsaBDL();
}

void AlsaStream::InitBuffer(UINT32 NumBytes)
{
    AlsaBDL *pBld = (AlsaBDL*)m_pBdl;
    pBld->InitBuffer(NumBytes);

}

void AlsaStream::AddSamples(void *Samples, UINT32 NumBytes)
{
    AlsaBDL *pBld = (AlsaBDL*)m_pBdl;

    pBld->AddSamples(Samples, NumBytes);
}

bool AlsaStream::IsFinished()
{
    AlsaBDL *pBld = (AlsaBDL*)m_pBdl;

    return pBld->IsFinished();
}
