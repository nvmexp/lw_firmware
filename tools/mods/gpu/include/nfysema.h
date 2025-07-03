/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2011,2013,2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Notify Semaphore class: facilitates waiting on semaphore(s)

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_NFYSEMA_H
#define INCLUDED_NFYSEMA_H

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif
#ifndef INCLUDED_MEMTYPES_H
#include "core/include/memtypes.h"
#endif
#ifndef INCLUDED_LWRM_H
#include "core/include/lwrm.h"
#endif
#ifndef INCLUDED_SURF2D_H
#include "gpu/utility/surf2d.h"
#endif
#ifndef INCLUDED_MEMORYIF_H
#include <memory>
#endif
#ifndef INCLUDED_STL_VECTOR
#define INCLUDED_STL_VECTOR
#include <vector>
#endif

class Channel;
class GpuDevice;

//--------------------------------------------------------------------
//! \brief Class that handles waiting for semaphore releases
//!
//! Wraps 1 or more small surfaces (normally in sysmem)
//! that the gpu can write to via a semaphore release method to tell
//! the host that some work is complete.
//!
//! Multiple surfaces can be one-per-subdevice for SLI channels, or
//! can be one-per-engine for multiple engines, etc.
//!
//! You may wait on all or one of the surfaces to be updated.

class NotifySemaphore
{
public:

    typedef bool (*CondFunc)
    (
        UINT32 LwrrentSurfaceWord,
        UINT32 MostRecentTriggerPayload
    );
    // struct size for DMA semaphore sizes
    enum SemaphoreSize
    {
        ONE_WORD = 1,  // semaphore only
        FOUR_WORDS = 4 // semaphore and timestamp
    };

    NotifySemaphore();
    ~NotifySemaphore(){Free();};

    void Free();

    //! Get timestamp set with four word semaphore
    UINT64 GetTimestamp(UINT32 surfNum);

    //! Resize private vectors and set private data members
    void Setup
    (
        SemaphoreSize Size,
        Memory::Location Location,
        UINT32 NumSurfaces
    );

    //! Allocate one of the surfaces.
    //! If Location is FB, assume subdevice 0.
    RC Allocate
    (
        Channel * pChannel,
        UINT32 surfNum
    );
    //! Allocate one of the surfaces.
    RC Allocate
    (
        Channel * pChannel,
        UINT32 surfNum,
        UINT32 subdevInst
    );

    bool IsAllocated() const { return ! m_pSurface2Ds.empty(); }

    //! Get ctxdma handle to be used with the GPU.
    LwRm::Handle GetCtxDmaHandleGpu(UINT32 surfNum);

    //! Get ctxdma handle to be used with the GPU.
    UINT64 GetCtxDmaOffsetGpu(UINT32 surfNum);

    //! Gives read-only access to a particular surface.
    const Surface2D& GetSurface(UINT32 surfNum) const;

    //! \brief Reports if the copy has completed on all surfaces
    RC IsCopyComplete(bool *pComplete);

    //! \brief Wait on all surfaces to be written
    //!
    //! \param TimeoutMs
    //! \param Cond -- Comparator function used to detemine
    //!     whether wait condition has been met. Default
    //!     is waiting for semaphore surface to match nfysema
    //!     payload exactly
    //!
    //! \return RC
    RC Wait(FLOAT64 TimeoutMs, CondFunc Cond=&CondEquals);

    //! \brief Wait on only one surface.
    //!
    //! \param surfNum
    //! \param TimeoutMs
    //! \param Cond -- Comparator function used to detemine
    //!     whether wait condition has been met. Default
    //!     is waiting for semaphore surface to match nfysema
    //!     payload exactly
    //!
    //! \return RC
    RC WaitOnOne(UINT32 surfNum, FLOAT64 TimeoutMs, CondFunc=&CondEquals);

    //! Set one surface's current value;
    //! Called before flushing (starting the target engine)
    void SetOnePayload(UINT32 surfNum, UINT32 value);
    //! Set all surfaces' current value;
    //! Called before flushing (starting the target engine)
    void SetPayload(UINT32 value);
    //! Get a current surface value.
    UINT32 GetPayload(UINT32 surfNum);

    //! Set the expected (wait-for) value for the next Wait call, one surface.
    void SetOneTriggerPayload(UINT32 surfNum, UINT32 val);
    //! Set the expected (wait-for) value for the next Wait call, all surfaces.
    void SetTriggerPayload(UINT32 val);
    //! Get the expected (wait-for) value of the next Wait call.
    UINT32 GetTriggerPayload(UINT32 surfNum);

    static bool CondEquals
    (
        UINT32 LwrrentSurfaceWord,
        UINT32 MostRecentTriggerPayload
    );
    static bool CondGreaterEq
    (
        UINT32 LwrrentSurfaceWord,
        UINT32 MostRecentTriggerPayload
    );
private:
    // NotifySemaphore class is multi client compliant and should never use the
    // LwRmPtr constructor without specifying which client
    DISALLOW_LWRMPTR_IN_CLASS(NotifySemaphore);

    UINT32                m_NumSurfaces;    //!< Number of surfaces
    vector<Channel*>      m_pChannel;       //!< Channel object
    Memory::Location      m_Location;       //!< memory location for semaphores

    vector<UINT32>        m_TriggerPayload;
    vector<Surface2D>     m_pSurface2Ds;    //!< Surfaces to hold notifiers.

    SemaphoreSize         m_SemaphoreSize;

    static bool Poll(void*);      //!< Used by Wait.

};

#endif

//------------------------------------------------------------------------------
