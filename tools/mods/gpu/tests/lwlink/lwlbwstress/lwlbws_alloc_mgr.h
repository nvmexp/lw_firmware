/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015,2017-2018,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_LWLBWS_ALLOC_MGR_H
#define INCLUDED_LWLBWS_ALLOC_MGR_H

#include "core/include/memtypes.h"
#include "core/include/lwrm.h"
#include "core/include/rc.h"
#include "core/include/tee.h"
#include "core/include/types.h"
#include "gpu/include/gralloc.h"
#include "gpu/lwlink/lwlinkimpl.h"
#include "gpu/tests/lwlink/lwldatatest/lwldt_alloc_mgr.h"
#include "gpu/tests/lwlink/lwldatatest/lwldt_priv.h"
#include "gpu/utility/surf2d.h"
#include "gpu/utility/tsg.h"

#include <vector>
#include <map>
#include <memory>

class GpuDevice;
class Channel;
class GpuTestConfiguration;

namespace LwLinkDataTestHelper
{
    //--------------------------------------------------------------------------
    //! \brief Class which manages allocations for the LwLink bandwidth stress
    //! test.  This is useful to avoid constant re-allocation of resources when
    //! test modes are changed since many resources can be reused in between
    //! test modes.  This manager takes a "lazy allocation" approach for
    //! surfaces and channels such that when a resource is "acquired" if one is
    //! not available it creates one and then returns that resource.  All
    //! semaphores are allocated at initialization time
    class AllocMgrCe2d : public AllocMgr
    {
        public:
            enum SurfaceType
            {
                 ST_NORMAL
                ,ST_FULLFABRIC
            };

            //! Structure returned when acquiring a semaphore
            struct SemaphoreData
            {
                Surface2DPtr pSemSurf;          //!< The actual semaphore surface
                void *       pAddress;          //!< CPU pointer to the semaphore
                UINT64       ctxDmaOffsetGpu;   //!< GPU virtual pointer to the semaphore
                SemaphoreData() : pAddress(nullptr), ctxDmaOffsetGpu(0) {}
            };

            AllocMgrCe2d
            (
                GpuTestConfiguration * pTestConfig,
                UINT32 numSemaphores,
                Tee::Priority pri,
                bool bFailRelease
            );
            virtual ~AllocMgrCe2d() {}

            RC AcquireSurface(GpuDevice*, Memory::Location, Surface2D::Layout,
                              UINT32, UINT32, Surface2DPtr*) = delete;
            RC AcquireSurface
            (
                GpuDevice *pGpuDev
               ,Memory::Location loc
               ,Surface2D::Layout layout
               ,SurfaceType type
               ,UINT32 surfaceWidth
               ,UINT32 surfaceHeight
               ,Surface2DPtr *pSurf
            );
            RC AcquireChannel
            (
                GpuDevice *pGpuDev,
                LwLinkImpl::HwType transferHw,
                UINT32 engineInstance,
                Channel **pCh,
                LwRm::Handle *pChHandle,
                UINT32 *pCopyClass
            );
            RC AcquireSemaphore(GpuDevice *pGpuDev, SemaphoreData *pSemData);
            Surface2DPtr GetGoldASurface() { return m_pGoldSurfA; }
            Surface2DPtr GetGoldBSurface() { return m_pGoldSurfB; }
            virtual RC Initialize
            (
                UINT32 goldSurfWidth,
                UINT32 goldSurfHeight,
                UINT32 surfMode
            );
            virtual RC ReleaseSurface(Surface2DPtr pSurf);
            RC ReleaseChannel(Channel *pCh);
            RC ReleaseSemaphore(void *pAddress);
            void SetFbSurfPageSizeKb(UINT32 fbSurfPageSizeKb)
                { m_FbSurfPageSizeKb = fbSurfPageSizeKb; }
            void SetUseFla(bool bUseFla)
                { m_bUseFla = bUseFla; }
            void SetFlaPageSizeKb(UINT32 flaPageSizeKb)
                { m_FlaPageSizeKb = flaPageSizeKb; }
            RC ShareSemaphore
            (
                const SemaphoreData & lwrSemData
               ,GpuDevice *pGpuDev
               ,SemaphoreData *pSharedSemData
            );
            virtual RC Shutdown();
        private:
            RC AllocSurface
            (
                Surface2DPtr pSurf,
                Memory::Location loc,
                Surface2D::Layout layout,
                SurfaceType type,
                UINT32 surfaceWidth,
                UINT32 surfaceHeight,
                GpuDevice *pGpuDev
            );
            //! Structure describing a surface allocation
            struct SurfaceAlloc
            {
                bool         bAcquired;   //!< true if allocation is used
                SurfaceType  type;        //!< type of surface
                Surface2DPtr pSurf;       //!< Pointer to the channel
                SurfaceAlloc() : bAcquired(false), type(ST_NORMAL) {}
            };

            typedef unique_ptr<GrClassAllocator> GrClassAllocPtr;

            //! Structure describing a channel allocation
            struct ChannelAlloc
            {
                bool               bAcquired;      //!< true if allocation is used
                Channel *          pCh;            //!< Pointer to the channel
                LwRm::Handle       hCh;            //!< Handle of the channel
                LwLinkImpl::HwType transferHw;     //!< Transfer HW being used
                UINT32             engineInstance; //!< Instance of the engine that was reserved
                GrClassAllocPtr    pDmaAlloc;      //!< Actual dma hw allocation
                ChannelAlloc() : bAcquired(false), pCh(nullptr), hCh(0),
                                 transferHw(LwLinkImpl::HT_CE), engineInstance(0) {}
            };

            //! Structure describing a semaphore allocation
            struct SemaphoreAlloc
            {
                bool          bAcquired;  //!< true if allocation is used
                UINT32        surfOffset; //!< Offset within the semaphore surface
                void *        pAddress;   //!< CPU virtual pointer of the semaphore
                SemaphoreAlloc() : bAcquired(false), surfOffset(0), pAddress(nullptr) {}
            };

            UINT32        m_NumSemaphores;         //!< Number of semaphores to allocate

            //! Internal static surfaces
            Surface2DPtr m_pGoldSurfA;
            Surface2DPtr m_pGoldSurfB;
            Surface2DPtr m_pSemSurf;

            UINT32 m_FbSurfPageSizeKb = 0;
            bool   m_bUseFla          = false;
            UINT32 m_FlaPageSizeKb    = 0;

            map<GpuDevice *, vector<SurfaceAlloc>> m_Surfaces;
            map<GpuDevice *, vector<ChannelAlloc>> m_Channels;
            map<GpuDevice *, unique_ptr<Tsg>>      m_GrceTsgs;
            vector<SemaphoreAlloc> m_Semaphores;

            RC GetGrceInstance(GpuDevice *pGpuDev, UINT32 ceInstance, UINT32 *pGrceInst);
            RC SetupSemaphores();
    };
};

#endif // INCLUDED_LWLBWS_ALLOC_MGR_H
