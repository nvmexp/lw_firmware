/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007,2010-2015,2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// DmaCopy and DecompressClass test.
//

#include "lwos.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "gpu/utility/surf2d.h"
#include "core/include/utility.h"
#include "gpu/include/dmawrap.h"

#include "class/cla0b5.h"
#include "class/cla06fsubch.h"

#include "gpu/include/gralloc.h"
#include "gpu/tests/rmtest.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/memcheck.h"

class VprFaultTest: public RmTest
{
public:
    VprFaultTest();
    virtual ~VprFaultTest();

    virtual string IsTestSupported();

    virtual RC   Setup();
    virtual RC   Run();
    virtual RC Cleanup();

private:

    Surface2D m_vprSurf;
    Surface2D m_surf;
    Channel * m_pCh         = nullptr;
    LwHandle m_hCh          = 0;
    DmaCopyAlloc m_dmaAlloc;

    LwHandle m_hNotifyEvent = 0;
    ModsEvent* m_pModsEvent = nullptr;
};

//------------------------------------------------------------------------------
VprFaultTest::VprFaultTest()
{
    SetName("VprFaultTest");
    m_dmaAlloc.SetOldestSupportedClass(KEPLER_DMA_COPY_A);
}

//------------------------------------------------------------------------------
VprFaultTest::~VprFaultTest()
{
}

//------------------------------------------------------------------------
string VprFaultTest::IsTestSupported()
{
    // TODO
    if (!m_dmaAlloc.IsSupported(GetBoundGpuDevice()))
        return "Requires KEPLER_DMA_COPY_A or later";
    return GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_HAS_VIDEO_PROTECTED_REGION) ? RUN_RMTEST_TRUE : "Requires VPR to run";
}

//------------------------------------------------------------------------------
RC VprFaultTest::Setup()
{

    LwRmPtr pLwRm;
    RC           rc;
    CHECK_RC(InitFromJs());
    StartRCTest();

    m_vprSurf.SetForceSizeAlloc(true);
    m_vprSurf.SetArrayPitch(1);
    m_vprSurf.SetPageSize(4);
    m_vprSurf.SetLayout(Surface2D::Pitch);
    m_vprSurf.SetAddressModel(Memory::Paging);
    m_vprSurf.SetColorFormat(ColorUtils::VOID32);
    m_vprSurf.SetArraySize(0x1000);
    m_vprSurf.SetVideoProtected(true);
    CHECK_RC(m_vprSurf.Alloc(GetBoundGpuDevice()));

    m_surf.SetForceSizeAlloc(true);
    m_surf.SetArrayPitch(1);
    m_surf.SetPageSize(4);
    m_surf.SetLayout(Surface2D::Pitch);
    m_surf.SetAddressModel(Memory::Paging);
    m_surf.SetColorFormat(ColorUtils::VOID32);
    m_surf.SetArraySize(0x1000);
    CHECK_RC(m_surf.Alloc(GetBoundGpuDevice()));

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh));
    CHECK_RC(m_dmaAlloc.AllocOnEngine(m_hCh, LW2080_ENGINE_TYPE_COPY(0), GetBoundGpuDevice(), NULL));

    m_pModsEvent = Tasker::AllocEvent();

    void* pEventAddr = Tasker::GetOsEvent(
            m_pModsEvent,
            pLwRm->GetClientHandle(),
            pLwRm->GetDeviceHandle(GetBoundGpuDevice()));

    CHECK_RC(pLwRm->AllocEvent(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                        &m_hNotifyEvent,
                                        LW01_EVENT_OS_EVENT,
                                        LW2080_NOTIFIERS_PHYSICAL_PAGE_FAULT,
                                        pEventAddr));

    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS eventParams = {0};

    eventParams.event = LW2080_NOTIFIERS_PHYSICAL_PAGE_FAULT;
    eventParams.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT;

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                    LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                                    &eventParams, sizeof(eventParams)));

    return rc;
}

//------------------------------------------------------------------------
RC VprFaultTest::Run()
{
    RC rc;

    Printf(Tee::PriLow, "Copying from 0x%llx to 0x%llx\n", m_vprSurf.GetVidMemOffset(), m_surf.GetVidMemOffset());

    m_pCh->ScheduleChannel(true);

    m_pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_OFFSET_IN_UPPER, LwU64_HI32(m_vprSurf.GetVidMemOffset()));
    m_pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_OFFSET_IN_LOWER, LwU64_LO32(m_vprSurf.GetVidMemOffset()));

    m_pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_OFFSET_OUT_UPPER, LwU64_HI32(m_surf.GetVidMemOffset()));
    m_pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_OFFSET_OUT_LOWER, LwU64_LO32(m_surf.GetVidMemOffset()));

    m_pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_SET_DST_PHYS_MODE,
            LWA0B5_SET_DST_PHYS_MODE_TARGET_LOCAL_FB);
    m_pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_SET_SRC_PHYS_MODE,
            LWA0B5_SET_DST_PHYS_MODE_TARGET_LOCAL_FB);

    m_pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_SET_SRC_DEPTH, 1);
    m_pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_SET_SRC_LAYER, 0);
    m_pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_SET_SRC_WIDTH, 1);
    m_pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_SET_SRC_WIDTH, LwU64_LO32(m_vprSurf.GetSize()));

    m_pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_SET_DST_DEPTH, 1);
    m_pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_SET_DST_LAYER, 0);
    m_pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_SET_DST_WIDTH, 1);
    m_pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_SET_DST_WIDTH, LwU64_LO32(m_surf.GetSize()));

    m_pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_LINE_COUNT, 1);
    m_pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_LINE_LENGTH_IN, LwU64_LO32(m_vprSurf.GetSize()));

    m_pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_PITCH_OUT, m_surf.GetPitch());
    m_pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_PITCH_IN, m_vprSurf.GetPitch());

    m_pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_LAUNCH_DMA,
                 DRF_DEF(A0B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _NONE) |
                 DRF_DEF(A0B5, _LAUNCH_DMA, _INTERRUPT_TYPE, _NONE) |
                 DRF_DEF(A0B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
                 DRF_DEF(A0B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH) |
                 DRF_DEF(A0B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH) |
                 DRF_DEF(A0B5, _LAUNCH_DMA, _SRC_TYPE, _PHYSICAL) |
                 DRF_DEF(A0B5, _LAUNCH_DMA, _DST_TYPE, _PHYSICAL) |
                 DRF_DEF(A0B5, _LAUNCH_DMA, _REMAP_ENABLE, _FALSE) |
                 DRF_DEF(A0B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NON_PIPELINED));

    m_pCh->Flush();

    CHECK_RC(Tasker::WaitOnEvent(m_pModsEvent, m_TestConfig.TimeoutMs()));

    Printf(Tee::PriHigh, "Fault event received\n");

    CHECK_RC(m_pCh->RmcResetChannel(LW2080_ENGINE_TYPE_COPY0));

    return rc;
}

//------------------------------------------------------------------------------
RC VprFaultTest::Cleanup()
{
    LwRmPtr pLwRm;

    pLwRm->Free(m_hNotifyEvent);
    Tasker::FreeEvent(m_pModsEvent);

    m_TestConfig.FreeChannel(m_pCh);
    m_vprSurf.Free();
    EndRCTest();

    return OK;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ VprFaultTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(VprFaultTest, RmTest,
                 "VprFault RM test.");
