/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/rc.h"
#include "core/include/tasker.h"
#include "gpu/include/gralloc.h"
#include "gpu/include/nfysema.h"

class GpuTestConfiguration;
class LwRm;
class GpuDevice;
class Surface2D;

namespace MfgTracePlayer
{
    // Class for accessing surfaces that would normally be mapped through BAR1 by
    // accessing a direct mapped sysmem surface and then DMA'ing the surface to
    // the target.  In Ampere+ any test that requires inst in sys support must not
    // have any BAR1 mappings
    class SurfaceAccessor
    {
        public:
            SurfaceAccessor() = default;
            ~SurfaceAccessor();

            SurfaceAccessor(const SurfaceAccessor&)            = delete;
            SurfaceAccessor& operator=(const SurfaceAccessor&) = delete;

            void Setup
            (
                GpuTestConfiguration * pTestConfig,
                LwRm *                 pLwRm,
                GpuDevice *            pGpuDev,
                LwRm::Handle           hVASpace,
                UINT32                 sizeMB
            );
            RC Free();
            RC ReadBytes
            (
                Surface2D * pSurf,
                UINT32      subdev,
                UINT64      offset,
                void *      buf,
                UINT64      sizeBytes
            );
            RC WriteBytes(Surface2D * pSurf, UINT64 offset, const void* buf, UINT64 sizeBytes);
        private:
            bool IsDirectMappingPossible(Surface2D *pSurf);
            RC Allocate();
            enum class AccessMode
            {
                Read,
                Write
            };
            RC AccessSurface
            (
                Surface2D  * pSurf,
                UINT64       offset,
                void *       buf,
                UINT64       sizeBytes,
                AccessMode   mode
            );
            RC Copy
            (
                Surface2D * pSrcSurf,
                UINT64      srcOffset,
                Surface2D * pDstSurf,
                UINT64      dstOffset,
                UINT32      linesToWrite,
                UINT32      lineWidth
            );

            GpuTestConfiguration * m_pTestConfig = nullptr;
            GpuDevice            * m_pGpuDevice  = nullptr;
            LwRm                 * m_pLwRm       = nullptr;
            Surface2D              m_SysmemSurface;
            Tasker::Mutex          m_Mutex       = Tasker::NoMutex();
            Channel              * m_pCh         = nullptr;
            LwRm::Handle           m_hVASpace    = 0;
            DmaCopyAlloc           m_DmaAlloc;
            NotifySemaphore        m_Semaphore;
            bool                   m_bAllocated  = false;
    };
}
