/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _RMT_MEMMAP_H
#define _RMT_MEMMAP_H

#include "core/include/utility.h"
#include "core/include/lwrm.h"
#include "rmt_random.h"
#include "rmt_memalloc.h"

namespace rmt
{
    //! Testing utility for mapping memory to GPU virtual space.
    class MemMapGpu : public Randomizable
    {
    public:
        MemMapGpu(LwRm *pLwRm, GpuDevice *pGpuDev,
                  MemAllocVirt *pVirt, MemAllocPhys *pPhys,
                  const UINT32 id = 0);
        virtual ~MemMapGpu();

        //! @sa Randomizable
        virtual void Randomize(const UINT32 seed);
        virtual const char *CheckValidity() const;
        virtual const char *CheckFeasibility() const;
        virtual void PrintParams(const INT32 pri) const;
        virtual const char *GetTypeName() const { return "MemMapGpu"; }
        virtual UINT32 GetID() const { return m_id; }
        virtual RC Create();

        //! Randomized parameters.
        #define LW_MEMMAP_STRATEGY_TYPE                1:0
        #define LW_MEMMAP_STRATEGY_TYPE_ALLOC_MAPPING  0x0
        #define LW_MEMMAP_STRATEGY_TYPE_FILL_PTE_MEM   0x1
        #define LW_MEMMAP_STRATEGY_TYPE_UPDATE_PDE_2   0x2
        struct Params
        {
            UINT64 offsetVirt;
            UINT64 offsetPhys;
            UINT64 length;
            UINT32 flags;
            UINT32 strategy;
        };
        void SetParams(const Params *pParams);
        const Params &GetParams() const { return m_params; }
        MemAllocVirt *GetVirt() const { return m_pVirt; }
        MemAllocPhys *GetPhys() const { return m_pPhys; }

        //! Will be 0 until Create() is called with a valid configuration.
        UINT64 GetVirtAddr() const { return m_virtAddr; }
        UINT64 GetPageAlignedVirtBase() const;
        UINT64 GetPageAlignedVirtLimit() const;
        UINT64 GetMappingPageSize() const;

    protected:
        LwRm               *m_pLwRm;
        GpuDevice          *m_pGpuDev;
        const UINT32        m_id;
        MemAllocVirt       *m_pVirt;
        MemAllocPhys       *m_pPhys;
        Params              m_params;
    private:
        UINT64              m_virtAddr;
    };

    //! Testing utility for mapping memory to CPU virtual space.
    class MemMapCpu
    {
    public:
        MemMapCpu(LwRm *pLwRm, GpuDevice *pGpuDev, const MemAllocPhys *pPhys,
                  const UINT64 offset, const UINT64 length);
        ~MemMapCpu();

        RC Create();

        //! Will be NULL until Create() is called with a valid configuration.
        void *GetPtr() const { return m_cpuAddr; }

    private:
        LwRm                *m_pLwRm;
        GpuDevice           *m_pGpuDev;
        const MemAllocPhys  *m_pPhys;
        const UINT64         m_offset;
        const UINT64         m_length;
        void                *m_cpuAddr;
    };
}

#endif

