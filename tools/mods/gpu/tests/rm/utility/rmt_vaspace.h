/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _RMT_VASPACE_H
#define _RMT_VASPACE_H

#include "core/include/utility.h"
#include "lwos.h"
#include "core/include/lwrm.h"
#include "gpu/tests/gputestc.h"
#include "core/include/heap.h"
#include "rmt_pool.h"
#include "rmt_random.h"
#include "rmt_memalloc.h"
#include "rmt_memmap.h"
#include "ctrl/ctrl0080/ctrl0080dma.h" // DMA_ADV_SCHED_GET_VA_CAPS

namespace rmt
{
    class VASpace;

    struct VASTracker
    {
        VASTracker(): numAllocs(0), numMaps(0) {}

        Pool<VASpace*> all;
        Pool<VASpace*> hasAllocs;
        Pool<VASpace*> hasMaps;
        Pool<VASpace*> parentable;
        Pool<VASpace*> subsets;
        size_t         numAllocs;
        size_t         numMaps;
    };

    //! Testing utility for VA space objects.
    class VASpace : public Randomizable
    {
    public:
        VASpace(LwRm *pLwRm, GpuDevice *pGpuDev, VASTracker *pTracker,
                VASpace *pParent, UINT32 vmmRegkey, const UINT32 id = 0);
        virtual ~VASpace();

        //! @sa Randomizable
        virtual void Randomize(const UINT32 seed);
        virtual const char *CheckValidity() const;
        virtual void PrintParams(const INT32 pri) const;
        virtual UINT32 GetID() const { return m_id; }
        virtual const char *GetTypeName() const { return "VASpace"; }
        virtual RC Create();

        //! Randomized parameters.
        typedef LW_VASPACE_ALLOCATION_PARAMETERS Params;
        void SetParams(const Params *pParams);
        const Params &GetParams() const { return m_params; }

        //! Will be 0 until Create() is called with a valid configuration.
        LwRm::Handle GetHandle() const { return m_handle; }

        UINT32 GetVmmRegkey() const { return m_vmmRegkey; }
        UINT64 GetBigPageSize() const;

        virtual void AddAlloc(MemAllocVirt *pVirt);
        virtual void RemAlloc(MemAllocVirt *pVirt);
        virtual void AddMapping(MemMapGpu *pMap);
        virtual void RemMapping(MemMapGpu *pMap);

        //! Get the current virtual allocations associated with this VA space.
        const Pool<MemAllocVirt*> &GetAllocs() const { return m_allocs; }

        //! Get the current virtual mapping associated with this VA space.
        const Pool<MemMapGpu*> &GetMappings() const { return m_mappings; }

        //! Get the heap tracking which regions of this VA space have been allocated.
        Heap *GetHeap() const { return m_pHeap.get(); }

        //! Check a size for validity.
        const char *CheckSize(UINT64 size) const;

        //! Resize the VAS.
        RC Resize(UINT64 *pSize);

        //! Commit shared ownership of the page directory with parent VAS.
        RC SubsetCommit();

        //! Revoke shared ownership of the page directory with parent VAS.
        RC SubsetRevoke();

        //! Interface providing channel test operations on our VA space.
        class ChannelTester
        {
        public:
            virtual ~ChannelTester() {};

            //! Just like memcpy.
            virtual RC MemCopy(const UINT64 dstVA, const UINT64 srcVA,
                               const UINT32 length) = 0;
        };

        //! Factory to create a channel tester interface.
        RC CreateChannel(GpuTestConfiguration *pTestConfig,
                         unique_ptr<ChannelTester>* ppChan);

    protected:
        LwRm                       *m_pLwRm;
        GpuDevice                  *m_pGpuDev;
        const UINT32                m_vmmRegkey;
        const UINT32                m_id;
        Params                      m_params;
    private:
        LW0080_CTRL_DMA_ADV_SCHED_GET_VA_CAPS_PARAMS m_caps;
        LwRm::Handle                m_handle;
        Pool<MemAllocVirt*>         m_allocs;
        unique_ptr<Heap>            m_pHeap;
        Pool<MemMapGpu*>            m_mappings;
        set<VASpace*>               m_subsets;
        VASpace*                    m_pParent;
        unique_ptr<MemAllocVirt>    m_pSubset;
        VASTracker*                 m_pTracker;
        bool                        m_bSubsetCommitted;
    };

    //! Colwenience utility that associates a single tester channel
    //! with a VA space.
    class VASpaceWithChannel : public VASpace
    {
    public:
        VASpaceWithChannel(LwRm *pLwRm, GpuDevice *pGpuDev, VASTracker *pTracker,
                           VASpace *pParent, UINT32 vmmRegkey, const UINT32 id = 0);

        RC CreateChannel(GpuTestConfiguration *pTestConfig);

        //! Will by NULL until CreateChannel is called successfully.
        VASpace::ChannelTester *GetChannel() const { return m_pChan.get(); }

    private:
        unique_ptr<VASpace::ChannelTester> m_pChan;
    };

    //! Testing utility to resize an existing VASpace.
    class VASpaceResizer : public Randomizable
    {
    public:
        VASpaceResizer(LwRm *pLwRm, GpuDevice *pGpuDev, VASpace *pVAS);
        virtual ~VASpaceResizer();

        //! @sa Randomizable
        virtual void Randomize(const UINT32 seed);
        virtual const char *CheckValidity() const;
        virtual void PrintParams(const INT32 pri) const;
        virtual UINT32 GetID() const { return 0; }
        virtual const char *GetTypeName() const { return "VASpaceResizer"; }
        virtual RC Create();

        //! Randomized parameters.
        struct Params
        {
            LwU64 size;
        };
        void SetParams(const Params *pParams);
        const Params &GetParams() const { return m_params; }

        bool GetValid() const { return m_bValid; }

    protected:
        LwRm                       *m_pLwRm;
        GpuDevice                  *m_pGpuDev;
        Params                      m_params;
    private:
        VASpace                    *m_pVAS;
        bool                        m_bValid;
    };
}

#endif

