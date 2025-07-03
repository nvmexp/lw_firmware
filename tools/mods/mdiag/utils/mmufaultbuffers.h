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
#pragma once

// This file contains functionality for managing per-subdevice MMU fault
// buffers, which must only be allocated once per subdevice, but may need to be
// shared (i.e. between UTL and policy manager).
#include <map>
#include <memory>

#include "class/clc369.h"
#include "containers/queue.h"
#include "core/include/lwrm.h"
#include "gpu/include/gpusbdev.h"

// Wraps a handle to a RM-allocated fault buffer. Obtain an instance of this
// class by calling SubdeviceFaultBuffer::GetFaultBuffer(...) rather than
// constructing objects directly.
class SubdeviceFaultBuffer
{
public:
    // Obtains a pointer to a SubdeviceFaultBuffer object for the given
    // subdevice. Attempts to allocate a new fault buffer if necessary.
    // Asserts that the underlying object was successfully allocated.
    static SubdeviceFaultBuffer* GetFaultBuffer(const GpuSubdevice* pGpuSubdevice);

    // To be called once, when mdiag is exiting. Frees all allocated
    // SubdeviceFaultBuffer objects. Called by CleanupMMUFaultBuffers.
    static void FreeAllFaultBuffers();

    // Will return a 0 handle if the fault buffer couldn't be allocated for any
    // reason.
    LwRm::Handle GetFaultBufferHandle() const { return m_FaultBufferHandle; }

    // Returns the size of subdevice fault buffer.
    RC GetFaultBufferSize(UINT32 *pSize);

    // Returns the get index of the subdevice fault buffer.
    RC GetFaultBufferGetIndex(UINT32 *pOffset);

    // Returns the put index of the subdevice fault buffer.
    RC GetFaultBufferPutIndex(UINT32 *pOffset);

    // Maps the subdevice fault buffer so that the fault entries can be retrieved.
    RC MapFaultBuffer
    (
        UINT64 offset,
        UINT64 length,
        void **pAddress,
        UINT32 flags,
        GpuSubdevice *pGpuSubdev
    );

    // Updates the get index of the subdevice fault buffer to the specified index.
    RC UpdateFaultBufferGetIndex(UINT32 newIndex);

    // Enables the notifications on subdevice fault buffer.
    RC EnableNotificationsForFaultBuffer();

    // Returns the max number of entries in subdevice fault buffer.
    RC GetMaxFaultBufferEntries(UINT32 *pSize);

    explicit SubdeviceFaultBuffer(const GpuSubdevice* pGpuSubdevice);
    ~SubdeviceFaultBuffer();

private:
    LwRm::Handle m_FaultBufferHandle = 0;
    static std::map<const GpuSubdevice*, std::unique_ptr<SubdeviceFaultBuffer>> s_FaultBuffers;
};

// This class wraps handles to work with shadow buffers, used by RM to provide
// access to the contents of nonreplayable fault buffers. Each instance of this
// class will be associated with a SubdeviceFaultBuffer. As with
// SubdeviceFaultBuffer, instances of this class should be obtained by calling
// GetShadowFaultBuffer(...) rather than constructing them directly, so that
// per-subdevice instances can be shared.
class SubdeviceShadowFaultBuffer
{
public:
    // Obtains a pointer to a SubdeviceShadowFaultBuffer object for the given
    // subdevice. Attempts to allocate a new shadow buffer if necessary.
    // Asserts that the underlying buffers were successfully allocated.
    static SubdeviceShadowFaultBuffer* GetShadowFaultBuffer(const GpuSubdevice* pGpuSubdevice);

    // To be called once, when mdiag is exiting. Unregisters all shadow
    // buffers, dissociating them from the underlying SubdeviceFaultBuffers.
    // Called by CleanupMMUFaultBuffers().
    static void ReleaseAllShadowFaultBuffers();

    explicit SubdeviceShadowFaultBuffer(const GpuSubdevice* pGpuSubdevice);
    ~SubdeviceShadowFaultBuffer();

    // Sets pSize to the number of packets the underlying fault buffer can
    // hold.
    RC GetFaultBufferSize(UINT32 *pSize);

    // Returns the fault buffer's current put pointer.
    RC GetFaultBufferPutPointer(UINT32 *pOffset);

    // Sets the fault buffer's get pointer to the given location.
    RC UpdateFaultBufferGetPointer(UINT32 offset);

    struct FaultData
    {
        UINT32 data[LWC369_BUF_SIZE / sizeof(UINT32)];
    };
    MAKE_QUEUE_CIRLWLAR(ShadowBufferQueue, FaultData);

    // These will return nullptr if the class didn't initialize properly.
    ShadowBufferQueue* GetQueue() { return m_pShadowBuffer; }
    QueueContext* GetQueueContext() { return m_pShadowBufferContext; }
    SubdeviceFaultBuffer* GetFaultBuffer() { return m_pFaultBuffer; }

    // Returns true if the showdow fault buffer is empty.
    RC IsFaultBufferEmpty(bool *pIsEmpty) const;

    // Returns the next entry in shadow fault buffer.
    RC PopAndCopyNextFaultEntry(FaultData *pData);

private:
    ShadowBufferQueue *m_pShadowBuffer;
    QueueContext *m_pShadowBufferContext;
    SubdeviceFaultBuffer *m_pFaultBuffer;
    static std::map<const GpuSubdevice*, std::unique_ptr<SubdeviceShadowFaultBuffer>> s_ShadowFaultBuffers;
};

// This should be called once, when mdiag is exiting. Releases any shadow
// buffers and frees fault buffers.
void CleanupMMUFaultBuffers();
