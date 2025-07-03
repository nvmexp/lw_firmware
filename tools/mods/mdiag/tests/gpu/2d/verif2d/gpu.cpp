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
#include "mdiag/tests/stdtest.h"
#include "mdiag/utils/types.h"

#include "verif2d.h"

V2dGpu::V2dGpu()
{
    m_pitchMultiple = 0;
}

V2dGpu::V2dGpu(GpuType type)
{
    m_pitchMultiple = 0;
    m_gpuType = type;
}

void V2dGpu::SetGpuType(GpuType gpuType)
{
    switch (gpuType)
    {
     case LW30:
     case LW40:
        m_pitchMultiple = 64;
        m_gpuType = gpuType;
        break;
     default:
        V2D_THROW( "unknown gpuType: " << gpuType );
    }
}

bool V2dGpu::SupportedClass(V2dClassObj::ClassId ClassId)
{
    // XXX todo
    return true;
}
