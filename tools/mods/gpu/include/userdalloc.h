/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016,2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef __USERDALLOC_H__
#define __USERDALLOC_H__

#include "core/include/rc.h"
#include "core/include/lwrm.h"
#include "core/include/memtypes.h"
#include "gpu/utility/surf2d.h"
#include <vector>
#include <queue>
#include <memory>

class GpuDevice;

//--------------------------------------------------------------------------
//! \brief Class which encasulates a GPFIFO channel USERD space allocation
//!
class UserdAlloc
{
public:
    virtual RC   Alloc(Memory::Location loc) = 0;
    virtual void Free() = 0;
    virtual LwRm::Handle   GetMemHandle() const = 0;
    virtual UINT64 GetMemOffset(UINT32 subdevInst) const = 0;
    virtual void * GetAddress(UINT32 subdevInst) const = 0;
    virtual Memory::Location GetLocation() const = 0;
    virtual RC Setup(LwRm::Handle hCh) = 0;
};
typedef shared_ptr<UserdAlloc> UserdAllocPtr;

//--------------------------------------------------------------------------
//! \brief Class which manages GPFIFO channel USERD space allocations.  It
//! preallocates a block of memory on the default RM client using the most
//! recently supported GPFIFO channel class for USERD space
//!
//! USERD blocks can be reserved in this preallocated space via Get().  If the
//! requested USERD is different from the preallocated space (different channel
//! class or different RM client) then a new block will be allocated instead
//!
//! USERD allocations which use a different channel class, different memory
//! location or different RM client can be allocated seperately via Get()
//!
class UserdManager
{
public:
    UserdManager(GpuDevice *pDev, UINT32 numChannels, Memory::Location loc);
    ~UserdManager();

    RC Alloc();
    RC Free();
    RC Create
    (
        UserdAllocPtr   *ppUserd
       ,UINT32           gpFifoClass
       ,Memory::Location loc
       ,LwRm            *pLwRm
    );
    RC Get(UserdAllocPtr *ppUserd, UINT32 gpFifoClass, LwRm *pLwRm);
    RC Release(UINT64 offset);
    bool RmAllocatesUserd() const { return m_bRmAllocatesUserd; }

    static const UINT32 DEFAULT_GPFIFO_CLASS = 0;
    static const UINT32 DEFAULT_USERD_ALLOCS = ~0U;

private:
    GpuDevice       *m_pGpuDev;     //!< GpuDevice for the USERD manager
    bool             m_bRmAllocatesUserd; //!< true if RM is creating/managing
                                          //!< USERD space
    Surface2D        m_UserdMemory; //!< Memory for the common USERD space
    vector<void *>   m_CpuAddresses;//!< Per-subdevice CPU addresses for the
                                    //!< start of the common USERD memory (non-FB
                                    //!< surfaces will only have one address)
    UINT32           m_NumChannels; //!< Number of channels supported by the
                                    //!< common USERD space
    Memory::Location m_Location;    //!< Location of common USERD space
    UINT32           m_GpFifoClass; //!< GPFIFO Channel class used when reserving
                                    //!< the common USERD space
    UINT32           m_UserdSize;   //!< Size of the USERD space for the
                                    //!< channel class used in the common USERD
                                    //!< allocation
    queue<UINT64>   m_FreeOffsets;  //!< Queue of USERD start offsets within the
                                    //!< common USERD memory allocation
    void *          m_AllocMutex;   //!< Mutex for protecting the allocation queue
};

#endif // __USERDALLOC_H__
