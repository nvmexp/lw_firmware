/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * \file  l2prefetch.h
 * \brief Utility for preteching surfaces from FB to L2 cache
 */

#pragma once

#include "core/include/display.h"
#include "gpu/include/gralloc.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/tests/gputestc.h"
#include "gpu/tests/gputest.h"

class Surface2D;

/**
 *  \addtogroup SurfaceUtils
 *  @{
 */
namespace SurfaceUtils
{

//----------------------------------------------------------------
/*
 * \class L2Prefetch
 *
 * \brief Prefetch surfaces from FB to L2 cache
 *
 * Only one instance is needed for one test and Gpu pair to pretech all of
 * the filled surfaces
 */
//----------------------------------------------------------------
class L2Prefetch
{
public:
    ~L2Prefetch() { Cleanup(); }
    RC Setup(GpuDevice *pGpuDevice, LwRm *pLwRm, FLOAT64 timeoutMs);
    RC Setup(GpuTestConfiguration *pTestConfig);
    RC InsertPersistentSurface(Surface2D *pSurface);
    RC InsertNonPersistentSurface(Surface2D *pSurface);
    RC RemovePersistentSurfaces();
    RC RemoveNonPersistentSurfaces();
    RC RemoveAllSurfaces();
    RC RunPrefetchSequence();
    RC Cleanup();

private:
    std::unique_ptr<GpuTestConfiguration> m_pTestConfigLocal;
    GpuTestConfiguration *m_pTestConfig = nullptr;
    Surfaces m_PersistentSurfaces;
    Surfaces m_NonPersistentSurfaces;
    Channel *m_pCh = nullptr;
    DmaCopyAlloc m_DmaCopyAlloc;

    RC GenerateTestConfig
    (
        GpuDevice *pGpuDevice,
        LwRm *pLwRm,
        FLOAT64 timeoutMs
    );
    RC FindNonGrCE(UINT32 *pEngineId);
    RC PrefetchSurfaces(const Surfaces& pSurfaces);
};

} // namespace SurfaceUtils
/** @}*/
