/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/utility/lwsurf.h"

//-----------------------------------------------------------------------------
RC LwSurfRoutines::Initialize(GpuDevice *pGpuDev)
{
    m_pGpuDevice  = pGpuDev;
    m_Initialized = true;
    return OK;
}

//------------------------------------------------------------------------------
bool LwSurfRoutines::IsSupported() const
{
    return false;
}

//------------------------------------------------------------------------------
void LwSurfRoutines::Free()
{
    m_pGpuDevice  = nullptr;
    m_Initialized = false;
}

//------------------------------------------------------------------------------
RC LwSurfRoutines::GetCRC
(
    GpuDevice             *pGpuDev,
    const LwRm::Handle     handle,
    const UINT64           offset,
    const UINT64           size,
    const Memory::Location loc,
    const bool             perComponent,
    vector<UINT32>        *pCrc
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC LwSurfRoutines::InjectError
(
    GpuDevice          *pGpuDev,
    const LwRm::Handle  handle,
    const UINT64        size,
    const UINT64        location,
    const UINT64        errorValue
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC LwSurfRoutines::CompareBuffers
(
    GpuDevice             *pGpuDev,
    const LwRm::Handle     handle1,
    const UINT64           size1,
    const Memory::Location loc1,
    const LwRm::Handle     handle2,
    const UINT64           size2,
    const Memory::Location loc2,
    const UINT32           mask,
    bool                  *pResult
)
{
    return RC::UNSUPPORTED_FUNCTION;
}
