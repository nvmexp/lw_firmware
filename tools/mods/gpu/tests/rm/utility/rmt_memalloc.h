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
#ifndef _RMT_MEMALLOC_H
#define _RMT_MEMALLOC_H

#include "core/include/utility.h"
#include "core/include/lwrm.h"
#include "core/include/heap.h"
#include "rmt_random.h"

namespace rmt
{
    // Forward declarations.
    class VASpace;
    class MemMapGpu;

    //! Testing utility for memory allocatios (base for physical and virtual).
    class MemAlloc : public Randomizable
    {
    public:
        MemAlloc(LwRm *pLwRm, GpuDevice *pGpuDev, const UINT32 id = 0);
        virtual ~MemAlloc();

        //! @sa Randomizable
        virtual void Randomize(const UINT32 seed);
        virtual const char *CheckValidity() const;
        virtual const char *CheckFeasibility() const;
        virtual void PrintParams(const INT32 pri) const;
        virtual UINT32 GetID() const { return m_id; }
        virtual RC Create();

        //! Randomized parameters.
        struct Params
        {
            UINT32 flags;
            UINT32 attr;
            UINT32 attr2;
            UINT64 size;
            UINT64 alignment;
            UINT64 offset;
            UINT64 limit;
            UINT64 rangeBeg;
            UINT64 rangeEnd;
        };
        void SetParams(const Params *pParams);
        const Params &GetParams() const { return m_params; }
        virtual UINT64 GetPageSize() const;

        //! Will be 0 until Create() is called with a valid configuration.
        LwRm::Handle GetHandle() const { return m_handle; }

        virtual void AddMapping(MemMapGpu *pMap);
        virtual void RemMapping(MemMapGpu *pMap);

        //! Get the current mappings associated with this allocation.
        const vector<MemMapGpu*> &GetMappings() const { return m_mappings; }

    protected:
        void ClearMappings();

        //! Accessor for the parent VA space (for virtual only).
        virtual VASpace *GetVASpace() const { return NULL; }

        LwRm              *m_pLwRm;
        GpuDevice         *m_pGpuDev;
        const UINT32       m_id;
        Params             m_params;
    private:
        LwRm::Handle       m_handle;
        vector<MemMapGpu*> m_mappings;
    };

    //! Utility for virtual allocations.
    class MemAllocVirt : public MemAlloc
    {
    public:
        MemAllocVirt(LwRm *pLwRm, GpuDevice *pGpuDev, VASpace *pVAS,
                     const UINT32 id = 0);
        virtual ~MemAllocVirt();

        //! @sa Randomizable
        virtual void Randomize(const UINT32 seed);
        virtual const char *CheckValidity() const;
        virtual const char *CheckFeasibility() const;
        virtual void PrintParams(const INT32 pri) const;
        virtual const char *GetTypeName() const { return "MemAllocVirt"; }
        virtual RC Create();
        virtual UINT64 GetPageSize() const;

        virtual void AddMapping(MemMapGpu *pMap);
        virtual void RemMapping(MemMapGpu *pMap);

        //! Get the heap tracking which regions of this VA have been mapped.
        Heap *GetHeap() const { return m_pHeap.get(); }

        //! Get the parent VA space.
        virtual VASpace *GetVASpace() const { return m_pVAS; };

    private:
        VASpace             *m_pVAS;
        unique_ptr<Heap>     m_pHeap;
    };

    //! Utility for physical allocations.
    class MemAllocPhys : public MemAlloc
    {
    public:
        MemAllocPhys(LwRm *pLwRm, GpuDevice *pGpuDev, const UINT32 largeAllocWeight,
                     const UINT32 largePageInSysmem, const UINT32 id = 0);
        virtual ~MemAllocPhys();

        //! @sa Randomizable
        virtual void Randomize(const UINT32 seed);
        virtual const char *CheckValidity() const;
        virtual const char *CheckFeasibility() const;
        virtual const char *GetTypeName() const { return "MemAllocPhys"; }

        RC AddChunk(const UINT64 offset, const UINT64 length, const UINT32 seed,
                    const bool bInit = false);
        RC CopyChunks(const UINT64 offsetDst, const UINT64 offsetSrc,
                      const UINT64 length, const MemAllocPhys *pSrc);
        RC VerifyChunks();

    private:
        //! Represents a contiguous region of memory initialized with
        //! a deterministic sequence of values.
        struct Chunk
        {
            UINT64 length;
            UINT32 seed;
        };
        map<UINT64, Chunk> m_chunks;
        const UINT32       m_largeAllocWeight;
        const UINT32       m_largePageInSysmem;
    };
}

#endif

