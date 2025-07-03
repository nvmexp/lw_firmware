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
#include <map>
#include <utility>

#include "class/clb069sw.h"
#include "class/clc369.h"
#include "ctrl/ctrlb069.h"
#include "ctrl/ctrlc369.h"
#include "core/include/rc.h"
#include "mdiag/sysspec.h"
#include "mmufaultbuffers.h"
#include "containers/queue.h"

std::map<const GpuSubdevice*, std::unique_ptr<SubdeviceFaultBuffer>> SubdeviceFaultBuffer::s_FaultBuffers;
std::map<const GpuSubdevice*, std::unique_ptr<SubdeviceShadowFaultBuffer>> SubdeviceShadowFaultBuffer::s_ShadowFaultBuffers;

/* static */ SubdeviceFaultBuffer* SubdeviceFaultBuffer::GetFaultBuffer
(
    const GpuSubdevice* pGpuSubdevice
)
{
    if (s_FaultBuffers.count(pGpuSubdevice) == 0)
    {
        s_FaultBuffers.emplace(pGpuSubdevice,
            std::make_unique<SubdeviceFaultBuffer>(pGpuSubdevice));
    }
    SubdeviceFaultBuffer *pFaultBuffer = s_FaultBuffers.find(pGpuSubdevice)->second.get();
    // Ensure the object was initialized properly.
    MASSERT(pFaultBuffer->GetFaultBufferHandle() != 0);
    return pFaultBuffer;
}

/* static */ void SubdeviceFaultBuffer::FreeAllFaultBuffers()
{
    s_FaultBuffers.clear();
}

SubdeviceFaultBuffer::SubdeviceFaultBuffer
(
    const GpuSubdevice *pGpuSubdevice
) :
    m_FaultBufferHandle(0)
{
    RC rc;
    LwRmPtr pLwRm;
    const UINT32 faultBufferClass = MMU_FAULT_BUFFER;
    LwRm::Handle subdeviceHandle = pLwRm->GetSubdeviceHandle(pGpuSubdevice);
    LWB069_ALLOCATION_PARAMETERS params = {0};

    if (!pLwRm->IsClassSupported(faultBufferClass, pGpuSubdevice->GetParentDevice()))
    {
        ErrPrintf("MMU fault buffer is unsupported on the given subdevice.\n");
        return;
    }

    rc = pLwRm->Alloc(subdeviceHandle, &m_FaultBufferHandle,
        faultBufferClass, &params);
    if (rc != OK)
    {
        ErrPrintf("Failed allocating MMU fault buffer: %s\n", rc.Message());
        return;
    }
    DebugPrintf("Allocated MMU fault buffer, handle = 0x%x\n", m_FaultBufferHandle);
}

RC SubdeviceFaultBuffer::GetFaultBufferSize
(
    UINT32 *pSize
)
{
    RC rc;
    LwRmPtr pLwRm;
    LWB069_CTRL_FAULTBUFFER_GET_SIZE_PARAMS sizeParams = {0};

    CHECK_RC(pLwRm->Control(m_FaultBufferHandle,
        LWB069_CTRL_CMD_FAULTBUFFER_GET_SIZE,
        (void*)&sizeParams,
        sizeof(sizeParams)));

    *pSize = sizeParams.faultBufferSize;
    DebugPrintf("Size of subdevice fault buffer = 0x%x\n", *pSize);
    return rc;
}

RC SubdeviceFaultBuffer::GetFaultBufferGetIndex
(
    UINT32 *pOffset
)
{
    RC rc;
    LwRmPtr pLwRm;
    LWB069_CTRL_FAULTBUFFER_READ_GET_PARAMS getParams = {0};
    getParams.faultBufferType = LWB069_CTRL_FAULT_BUFFER_REPLAYABLE;

    CHECK_RC(pLwRm->Control(m_FaultBufferHandle,
                 LWB069_CTRL_CMD_FAULTBUFFER_READ_GET,
                 (void*)&getParams,
                 sizeof(getParams)));

    *pOffset = getParams.faultBufferGetOffset;
    DebugPrintf("Get index of subdevice fault buffer = 0x%x\n", *pOffset);
    return rc;
}

RC SubdeviceFaultBuffer::GetFaultBufferPutIndex
(
    UINT32 *pOffset
)
{
    RC rc;
    LwRmPtr pLwRm;
    LWB069_CTRL_FAULTBUFFER_READ_PUT_PARAMS putParams = {0};
    putParams.faultBufferType = LWB069_CTRL_FAULT_BUFFER_REPLAYABLE;

    CHECK_RC(pLwRm->Control(m_FaultBufferHandle,
                 LWB069_CTRL_CMD_FAULTBUFFER_READ_PUT,
                 (void*)&putParams,
                 sizeof(putParams)));

    *pOffset = putParams.faultBufferPutOffset;
    DebugPrintf("Put index of subdevice fault buffer = 0x%x\n", *pOffset);
    return rc;
}

RC SubdeviceFaultBuffer::MapFaultBuffer
(
    UINT64 offset,
    UINT64 length,
    void **pAddress,
    UINT32 flags,
    GpuSubdevice *pGpuSubdev
)
{
    RC rc;
    LwRmPtr pLwRm;

    CHECK_RC(pLwRm->MapMemory(
        m_FaultBufferHandle,
        offset,
        length,
        pAddress,
        flags,
        pGpuSubdev));

    DebugPrintf("Mapped subdevice fault buffer.\n");
    return rc;
}

RC SubdeviceFaultBuffer::UpdateFaultBufferGetIndex
(
    UINT32 newIndex
)
{
    RC rc;
    LWB069_CTRL_FAULTBUFFER_WRITE_GET_PARAMS writeGetParams = {0};
    writeGetParams.faultBufferGetOffset = newIndex;
    writeGetParams.faultBufferType = LWB069_CTRL_FAULT_BUFFER_REPLAYABLE;

    LwRmPtr pLwRm;
    CHECK_RC(pLwRm->Control(m_FaultBufferHandle,
            LWB069_CTRL_CMD_FAULTBUFFER_WRITE_GET,
            (void*)&writeGetParams,
            sizeof(writeGetParams)));

    DebugPrintf("Updated get index of subdevice fault buffer to 0x%x\n", newIndex);
    return rc;
}

RC SubdeviceFaultBuffer::EnableNotificationsForFaultBuffer()
{
    RC rc;
    LWB069_CTRL_FAULTBUFFER_ENABLE_NOTIFICATION_PARAMS notificationParams = {0};
    notificationParams.Enable = true;

    LwRmPtr pLwRm;
    CHECK_RC(pLwRm->Control(m_FaultBufferHandle,
        LWB069_CTRL_CMD_FAULTBUFFER_ENABLE_NOTIFICATION,
        (void*)&notificationParams,
        sizeof(notificationParams)));

    DebugPrintf("Enabled notifications for subdevice fault buffer.\n");
    return rc;
}

RC SubdeviceFaultBuffer::GetMaxFaultBufferEntries(UINT32 *pSize)
{
    RC rc;
    UINT32 size;
    GetFaultBufferSize(&size);
    *pSize = size / LWC369_BUF_SIZE;

    DebugPrintf("Max fault buffer entries = 0x%x\n", *pSize);
    return rc;
}

SubdeviceFaultBuffer::~SubdeviceFaultBuffer()
{
    LwRmPtr pLwRm;
    if (m_FaultBufferHandle == 0)
    {
        DebugPrintf("Destroying an uninitialized SubdeviceFaultbuffer.\n");
        return;
    }
    pLwRm->Free(m_FaultBufferHandle);
    m_FaultBufferHandle = 0;
    DebugPrintf("Freed MMU fault buffer\n");
}

/* static */ SubdeviceShadowFaultBuffer* SubdeviceShadowFaultBuffer::GetShadowFaultBuffer
(
    const GpuSubdevice* pGpuSubdevice
)
{
    if (s_ShadowFaultBuffers.count(pGpuSubdevice) == 0)
    {
        s_ShadowFaultBuffers.emplace(pGpuSubdevice,
            std::make_unique<SubdeviceShadowFaultBuffer>(pGpuSubdevice));
    }
    SubdeviceShadowFaultBuffer *pShadowBuffer = s_ShadowFaultBuffers.find(pGpuSubdevice)->second.get();
    // Ensure that the shadow buffer was registered successfully.
    MASSERT(pShadowBuffer->GetQueue());
    return pShadowBuffer;
}

/* static */ void SubdeviceShadowFaultBuffer::ReleaseAllShadowFaultBuffers()
{
    s_ShadowFaultBuffers.clear();
}

SubdeviceShadowFaultBuffer::SubdeviceShadowFaultBuffer
(
    const GpuSubdevice* pGpuSubdevice
) :
    m_pShadowBuffer(nullptr),
    m_pShadowBufferContext(nullptr),
    m_pFaultBuffer(nullptr)
{
    LwRmPtr pLwRm;
    RC rc;
    LwRm::Handle faultBufferHandle = 0;
    LWC369_CTRL_MMU_FAULT_BUFFER_REGISTER_NON_REPLAY_BUF_PARAMS params = {0};

    m_pFaultBuffer = SubdeviceFaultBuffer::GetFaultBuffer(pGpuSubdevice);
    faultBufferHandle = m_pFaultBuffer->GetFaultBufferHandle();
    rc = pLwRm->Control(faultBufferHandle,
        LWC369_CTRL_CMD_MMU_FAULT_BUFFER_REGISTER_NON_REPLAY_BUF,
        (void*) &params, sizeof(params));
    if (rc != OK)
    {
        m_pFaultBuffer = nullptr;
        ErrPrintf("Failed to register nonreplayable fault buffer: %s\n",
            rc.Message());
        return;
    }
    m_pShadowBuffer = reinterpret_cast<ShadowBufferQueue *>(params.pShadowBuffer);
    m_pShadowBufferContext = reinterpret_cast<QueueContext *>(params.pShadowBufferContext);
    DebugPrintf("SubdeviceShadowFaultBuffer: registered nonreplayable fault buffer\n");
}

SubdeviceShadowFaultBuffer::~SubdeviceShadowFaultBuffer()
{
    RC rc;
    LwRmPtr pLwRm;
    LWC369_CTRL_MMU_FAULT_BUFFER_UNREGISTER_NON_REPLAY_BUF_PARAMS params = {0};

    if (!m_pShadowBuffer)
    {
        DebugPrintf("Destroying an uninitialized SubdeviceShadowFaultBufer\n");
        return;
    }
    params.pShadowBuffer = reinterpret_cast<LwP64>(m_pShadowBuffer);
    rc = pLwRm->Control(m_pFaultBuffer->GetFaultBufferHandle(),
        LWC369_CTRL_CMD_MMU_FAULT_BUFFER_UNREGISTER_NON_REPLAY_BUF,
        (void*) &params, sizeof(params));
    if (rc != OK)
    {
        ErrPrintf("Failed unregistering shadow buffer: %s\n", rc.Message());
        return;
    }
    DebugPrintf("SubdeviceShadowFaultBuffer: Unregistered OK.\n");
}

RC SubdeviceShadowFaultBuffer::GetFaultBufferSize(UINT32 *pSize)
{
    RC rc;
    LWB069_CTRL_FAULTBUFFER_GET_SIZE_PARAMS params = {0};
    LwRmPtr pLwRm;
    CHECK_RC(pLwRm->Control(m_pFaultBuffer->GetFaultBufferHandle(),
        LWB069_CTRL_CMD_FAULTBUFFER_GET_SIZE,
        (void*) &params, sizeof(params)));

    const UINT32 faultBufferPacketSize = LWC369_BUF_SIZE;
    const UINT32 faultBufferSize = params.faultBufferSize;
    *pSize = faultBufferSize / faultBufferPacketSize;
    return rc;
}

RC SubdeviceShadowFaultBuffer::UpdateFaultBufferGetPointer(UINT32 offset)
{
    RC rc;
    LwRmPtr pLwRm;
    LWB069_CTRL_FAULTBUFFER_WRITE_GET_PARAMS params = {0};
    params.faultBufferGetOffset = offset;
    params.faultBufferType = LWB069_CTRL_FAULT_BUFFER_NON_REPLAYABLE;
    CHECK_RC(pLwRm->Control(m_pFaultBuffer->GetFaultBufferHandle(),
            LWB069_CTRL_CMD_FAULTBUFFER_WRITE_GET,
            (void*) &params, sizeof(params)));
    return rc;
}

RC SubdeviceShadowFaultBuffer::GetFaultBufferPutPointer(UINT32 *pOffset)
{
    RC rc;
    LwRmPtr pLwRm;
    LWB069_CTRL_FAULTBUFFER_READ_PUT_PARAMS params = {0};
    params.faultBufferType = LWB069_CTRL_FAULT_BUFFER_NON_REPLAYABLE;
    CHECK_RC(pLwRm->Control(m_pFaultBuffer->GetFaultBufferHandle(),
            LWB069_CTRL_CMD_FAULTBUFFER_READ_PUT,
            (void*) &params, sizeof(params)));
    *pOffset = params.faultBufferPutOffset;
    return rc;
}

RC SubdeviceShadowFaultBuffer::IsFaultBufferEmpty(bool *pIsEmpty) const
{
    RC rc;
    *pIsEmpty = queueIsEmpty(m_pShadowBuffer);
    DebugPrintf("Is shadow fault buffer empty? - %d\n", *pIsEmpty);
    return rc;
}

RC SubdeviceShadowFaultBuffer::PopAndCopyNextFaultEntry(FaultData *pData)
{
    RC rc;
    queuePopAndCopyNonManaged(m_pShadowBuffer, m_pShadowBufferContext, pData);
    return rc;
}

void CleanupMMUFaultBuffers()
{
    // Dissociate any shadow buffers from the underlying fault buffers before
    // releasing the fault buffers.
    SubdeviceShadowFaultBuffer::ReleaseAllShadowFaultBuffers();
    SubdeviceFaultBuffer::FreeAllFaultBuffers();
}
