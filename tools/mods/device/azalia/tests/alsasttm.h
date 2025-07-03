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

#ifndef INCLUDED_ALSASTTM_H
#define INCLUDED_ALSASTTM_H

#include "azasttm.h"

class AlsaStream;
struct snd_pcm_t;

class AlsaStreamTestModule : public AzaliaStreamTestModule
{
public:
    AlsaStreamTestModule();
    ~AlsaStreamTestModule();
    virtual RC Start();
    virtual RC Continue();
    virtual RC Stop();

    virtual RC GetStream(UINT32 index, AzaStream **pStrm);

private:
    AlsaStream *m_pStream;
    snd_pcm_t  *m_CaptureHandle;
};

#endif // INCLUDED_ALSASTTM_H

