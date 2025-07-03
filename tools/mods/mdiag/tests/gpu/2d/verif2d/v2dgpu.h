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
#ifndef INCLUDED_V2DGPU_H
#define INCLUDED_V2DGPU_H

#include "mdiag/utils/types.h"
#include "classobj.h"

class V2dGpu
{
public:
    enum GpuType
    {
        LW30,
        LW40
    };

    void SetGpuType(GpuType gpuType);
    GpuType GetGpuType() { return m_gpuType; }
    V2dGpu();
    V2dGpu(GpuType type);

    bool SupportedClass(V2dClassObj::ClassId classId);
    UINT32 GetPitchMultiple() { return m_pitchMultiple; }

private:
    GpuType m_gpuType;
    UINT32 m_pitchMultiple; // surface pitches must be a multiple of this value
                            // MUST BE A POWER OF TWO!
};

#endif

