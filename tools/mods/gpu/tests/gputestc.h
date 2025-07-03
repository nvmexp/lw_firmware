/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2014,2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_GPUTESTC_H
#define INCLUDED_GPUTESTC_H

#include "core/include/lwrm.h"
#include "core/tests/testconf.h"
#include "gpu/include/dispmgr.h"
#include "core/include/channel.h"
#include "class/cl2080.h"
#include <memory>

class Tsg;
class Subcontext;
class SObject;
class GpuDevice;
class Surface2D;
class GrClassAllocator;
class TestDevice;
typedef shared_ptr<TestDevice> TestDevicePtr;


class GpuTestConfiguration : public TestConfiguration
{
public:
    enum
    {
        PointMaxValue =  32767, // 0x00007fff
        PointMilwalue = -32768, // 0xffff8000
    };

public:
    // CREATORS
    GpuTestConfiguration();
    virtual ~GpuTestConfiguration();

    // MANIPULATORS
    void SetDmaProtocol(Memory::DmaProtocol protocol);
    RC SetSystemMemModel(Memory::SystemModel);

    virtual void SetChannelType(ChannelType ct);

    // We allocate the channel based on the push buffer location
    virtual RC AllocateChannel(Channel ** ppChan, LwRm::Handle * phChan);
    virtual RC AllocateChannel(Channel ** ppChan, LwRm::Handle * phChan, UINT32 engineId);

    // In modern GPUs channels are tightly bound with the engine that they are
    // using
    virtual RC AllocateChannelWithEngine
    (
        Channel         ** ppChan
       ,LwRm::Handle     * phChan
       ,GrClassAllocator * pGrAlloc
       ,UINT32             engineId = LW2080_ENGINE_TYPE_ALLENGINES
    );

    RC AllocateChannel
    (
        Channel     **ppChan,
        LwRm::Handle *phChan,
        Tsg          *pTsg,
        Subcontext   *pSubctx,
        LwRm::Handle  hVASpace,
        LwU32         flags,
        UINT32        engineId
    );
    RC AllocateChannelGr
    (
        Channel **ppChan,
        LwRm::Handle *phChan,
        Tsg *pTsg,
        LwRm::Handle hVASpace,
        GrClassAllocator *pGrAlloc,
        LwU32 flags,
        UINT32 engineId
    );

    virtual RC AllocateErrorNotifier(Surface2D *pErrorNotifier, UINT32 hVASpace);
    virtual RC FreeChannel (Channel * pChan);
    RC IdleChannel(Channel* pChan);
    virtual RC IdleAllChannels();

    RC ApplySystemModelFlags(Memory::Location loc, Surface2D* pSurf) const;

    // ACCESSORS
    Memory::DmaProtocol           DmaProtocol() const;
    UINT32                        GpFifoEntries() const;
    Memory::SystemModel           MemoryModel() const;
    UINT32                        TiledHeapAttr() const;
    void *                        GetPushBufferAddr(LwRm::Handle handle) const;
    Memory::Location              NotifierLocation() const;
    ChannelType                   GetChannelType() const;
    UINT32                        ChannelSize() const;
    bool                          ChannelLogging() const;
    bool                          AllowVIC() const;
    Channel::SemaphorePayloadSize SemaphorePayloadSize() const;
    DisplayMgr::Requirements      DisplayMgrRequirements() const;
    bool                          UseOldRNG() const { return m_UseOldRNG; }
    UINT32                        UphyLogMask() const { return m_UphyLogMask; }

    virtual TestConfigurationType GetTestConfigurationType();

    void BindTestDevice(TestDevicePtr pTestDevice) { m_pTestDevice = pTestDevice; }
    void BindGpuDevice(GpuDevice *gpudev);
    void BindRmClient(LwRm *pLwRm){ m_pLwRm = pLwRm; }

    const UINT32 *GetFifoClasses() const;
    int GetNumFifoClasses() const { return static_cast<int>(Channel::FifoClasses.size()); }

    GpuDevice *GetGpuDevice() const;
    TestDevicePtr GetTestDevice() const { return m_pTestDevice; }
    LwRm *GetRmClient() const;

    virtual void PrintJsProperties(Tee::Priority pri);

    // MANIPULATORS

protected:
    virtual void Reset(bool callBaseClass);
    virtual void Reset();
    virtual RC   GetProperties();
    virtual RC   ValidateProperties();
    virtual RC   GetPerTestOverrides(JSObject *valuesObj);

    TestDevicePtr m_pTestDevice;
    LwRm         *m_pLwRm;

private:
    //
    // Controls (set from JavaScript, per-test)
    //
    Memory::DmaProtocol           m_DmaProtocol;
    Memory::SystemModel           m_SystemMemModel;
    UINT32                        m_GpFifoEntries;
    Memory::Location              m_NotifierLocation;
    bool                          m_AutoFlush;
    UINT32                        m_AutoFlushThresh;
    bool                          m_UseBar1Doorbell;
    bool                          m_ChannelLogging;
    bool                          m_AllowVIC;
    Channel::SemaphorePayloadSize m_SemaphorePayloadSize;
    DisplayMgr::Requirements      m_DisplayMgrRequirements;
    bool                          m_UseOldRNG = false;
    UINT32                        m_UphyLogMask = 0U;

    // Local handles, pointers, etc. to track lwrrently allocated channels.
    struct ChannelStuff;
    ChannelStuff *    m_pChannelStuff;
};

extern SObject TestConfiguration_Object;

#endif // INCLUDED_GPUTESTC_H
