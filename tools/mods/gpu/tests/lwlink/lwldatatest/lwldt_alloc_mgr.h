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

#ifndef INCLUDED_LWLDT_ALLOC_MGR_H
#define INCLUDED_LWLDT_ALLOC_MGR_H

#include "core/include/types.h"
#include "core/include/rc.h"
#include "core/include/tee.h"
#include "core/include/lwrm.h"
#include "core/include/memtypes.h"
#include "gpu/include/gralloc.h"
#include "gpu/utility/surf2d.h"
#include "lwldt_priv.h"

#include <vector>
#include <map>
#include <memory>

class GpuDevice;
class Channel;
class GpuTestConfiguration;
class Tsg;
class LwSurfRoutines;

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
    class AllocMgr
    {
        public:
            //! Enum describing fill patterns available
            enum PatternType
            {
                PT_BARS
               ,PT_RANDOM
               ,PT_SOLID
               ,PT_LINES
               ,PT_ALTERNATE
            };

            AllocMgr
            (
                GpuTestConfiguration * pTestConfig,
                Tee::Priority pri,
                bool bFailRelease
            );
            virtual ~AllocMgr() { Shutdown(); }

            RC AcquireLwdaUtils
            (
                GpuDevice      *pGpuDev,
                LwSurfRoutines **ppLwdaUtils
            );
            RC AcquireSurface
            (
                GpuDevice *pGpuDev
               ,Memory::Location loc
               ,Surface2D::Layout layout
               ,UINT32 surfaceWidth
               ,UINT32 surfaceHeight
               ,Surface2DPtr *pSurf
            );
            RC FillSurface(Surface2DPtr pSurf, PatternType pt, UINT32 patternData, UINT32 patternData2, bool bTwoDFill);
            bool GetUseDmaInit() { return m_bUseDmaInit; }
            RC ReleaseLwdaUtils(LwSurfRoutines **ppLwdaUtils);
            virtual RC ReleaseSurface(Surface2DPtr pSurf);
            void SetUseLwda(bool bUseLwda) { m_bUseLwda = bUseLwda; }
            void SetUseDmaInit(bool bUseDma) { m_bUseDmaInit = bUseDma; }
            virtual RC Shutdown();
        protected:
            RC AllocSurface
            (
                Surface2DPtr pSurf,
                Memory::Location loc,
                Surface2D::Layout layout,
                UINT32 surfaceWidth,
                UINT32 surfaceHeight,
                GpuDevice *pGpuDev
            );
            Tee::Priority GetPrintPri() { return m_PrintPri; }
            Tee::Priority GetReleaseFailPri() { return m_ReleaseFailPri; }
            RC GetReleaseFailRc() { return m_ReleaseFailRc; }
            GpuTestConfiguration * GetTestConfig() { return m_pTestConfig; }
        private:
            //! Structure describing lwca surface utility usage
            struct LwdaSurfUtilData
            {
                shared_ptr<LwSurfRoutines> pLwSurfUtils;
                UINT32                     lwSurfUtilsUseCount;
                LwdaSurfUtilData() : lwSurfUtilsUseCount(0U) { }
            };

            //! Structure describing a surface allocation
            struct SurfaceAlloc
            {
                bool         bAcquired;   //!< true if allocation is used
                Surface2DPtr pSurf;       //!< Pointer to the channel
                SurfaceAlloc() : bAcquired(false) {}
            };

            GpuTestConfiguration * m_pTestConfig;    //!< Pointer to test configuration
            Tee::Priority          m_PrintPri;       //!< Print priority for informational prints
            Tee::Priority          m_ReleaseFailPri; //!< Priority of prints for release failures
            RC                     m_ReleaseFailRc;  //!< RC to return if release fails
            bool                   m_bUseLwda;       //!< Whether or not to use LWCA when filling surfaces
            bool                   m_bUseDmaInit;    //!< Whether or not to use DMA when filling surfaces

            //! Allocated resources
            map<GpuDevice *, vector<SurfaceAlloc>> m_Surfaces;
            map<GpuDevice *, LwdaSurfUtilData>     m_LwdaSurfaceUtils;

    };

    //! Pointer type for passing alloc manager around
    typedef shared_ptr<AllocMgr> AllocMgrPtr;
};

#endif // INCLUDED_LWLDT_ALLOC_MGR_H
