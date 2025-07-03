/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
//! \file rmt_outofmem.cpp
//! \brief OutOfMem functionality test
//!
//!The test starts up with a small fbsize 64MB
//!(run the test with command line argument -override_fb_size 0x40):
//!<a> continues allocating channels
//!<b> for each channel creates a graphics object
//!<c> for each graphics object send some dummy 9097 work to force context load
//! Exits successfully if INSUFFICIENT_RESOURCES error is obtained due to
//!  any of the above allocations,
//! If any other error is obtained, it marks the test as failing.
#include "core/include/tee.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/gputest.h"
#include "core/include/jscript.h"
#include "core/include/channel.h"
#include "core/include/lwrm.h"
#include "gpu/tests/rmtest.h"
#include "gpu/include/notifier.h"
#include "class/cl9097.h" // FERMI_A
#include "class/cl9097sw.h"
#include "class/cl906f.h"
#include "class/cle397.h" //LW_E3_THREED
#include "class/cle497.h" //LWE4_THREED
#include "class/cle26d.h" //LWE2_SYNCPOINT
#include "class/cle26e.h" //LWE2_CHANNEL_DMA
#include "class/cla06fsubch.h"
#include "gpu/utility/surf2d.h"
#include "core/include/rc.h"
#include "core/include/platform.h"
#include "core/include/memcheck.h"
#include "gpu/utility/syncpoint.h"
#include "t11x/t114/dev_sec_pri.h"
#include "class/cle3f1.h" // TEGRA_VASPACE_A
#include "class/cl95a1.h" // LW95A1_TSEC

#define MAXCHANNELS 128   // Max number of channels

#define GR_OBJ_SUBCH LWA06F_SUBCHANNEL_3D

class OutOfMem: public RmTest
{
public:
    OutOfMem();
    virtual ~OutOfMem();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual bool RequiresSoftwareTreeValidation() const override { return true; }

private:
    //OOM class is multi client compliant and should never use the
    // LwRmPtr constructor without specifying which client
    DISALLOW_LWRMPTR_IN_CLASS(OutOfMem);

    RC AllocChannel
    (
        LwRm *pLwRm,
        Surface2D *pPBMem,
        Surface2D *pErrMem,
        LwRm::Handle *pChannelHandle
    );
    RC AllocChannelTegra
    (
        LwRm *pLwRm,
        Surface2D *pPBMem,
        SyncPoint *pSync,
        LwRm::Handle *pChannelHandle,
        Channel **pChannel
    );

    LwRm::Handle            m_ClientChannelHandle[MAXCHANNELS];
    Channel                 *m_pClientChannel[MAXCHANNELS];
    Surface2D               m_ClientChannelPB[MAXCHANNELS];
    Surface2D               m_ClientChannelErr[MAXCHANNELS];
    SyncPoint               m_Sync[MAXCHANNELS];
    LwRm::Handle            hFermiObject[MAXCHANNELS];
    Notifier                m_NotifierClient[MAXCHANNELS];

    FLOAT64                 m_TimeoutMs;
    UINT32                  m_ChnlNo;
    UINT32                  m_ClientCount;
};

//!
//! \brief OutOfMem constructor
//!
//!OutOfMem constructor does not do much. Functionality
//! mostly lies in Setup().
//! \sa Setup
//------------------------------------------------------------------------------
OutOfMem::OutOfMem()
{
    SetName("OutOfMem");
    memset(&m_pClientChannel, 0, sizeof(m_pClientChannel));
    memset(&m_ClientChannelHandle, 0, sizeof(m_ClientChannelHandle));
    m_TimeoutMs   = Tasker::NO_TIMEOUT;
    m_ChnlNo      = 0;
    m_ClientCount = 0;
}

//!
//! \brief OutOfMem destructor
//!
//! OutOfMem destructor does not do much. Functionality
//! mostly lies in Cleanup().
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
OutOfMem::~OutOfMem()
{
}

//!
//! \brief IsTestSupported()
//!
//! Verify if Fermi Class9097Test is supported in the current
//! environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------
string OutOfMem::IsTestSupported()
{
    bool    isTestSupported = true;
    RC      rc;

    LW2080_CTRL_FB_GET_INFO_PARAMS fbGetInfoParams = {0};
    LW2080_CTRL_FB_INFO ctrlBlock;

    GpuSubdevice *pSubdevice = GetBoundGpuSubdevice();
    fbGetInfoParams.fbInfoListSize = 1;
    ctrlBlock.index = LW2080_CTRL_FB_INFO_INDEX_FB_IS_BROKEN;
    fbGetInfoParams.fbInfoList = (LwP64)&ctrlBlock;
    //
    // Find the FB broken status
    //
    rc = LwRmPtr(0)->ControlBySubdevice(pSubdevice,
                        LW2080_CTRL_CMD_FB_GET_INFO,
                        &fbGetInfoParams,
                        sizeof (LW2080_CTRL_FB_GET_INFO_PARAMS));
    if (OK != rc)
    {
        Printf(Tee::PriHigh," %d:failed for Getting FB broken status \n",
                           __LINE__);
        return "Not supported for getting FB broken status";
    }
    isTestSupported = ctrlBlock.data ? false : true;

    if (Platform::GetSimulationMode() == Platform::Fmodel && ((IsClassSupported(FERMI_A) || IsClassSupported(LWE4_THREED))
                     && isTestSupported))
    {
        return RUN_RMTEST_TRUE;
    }
    else
        return "Either FERMI_A class is not supported or it's fmodel";
}

//!
//! \brief OutOfMem Setup
//!
//! \Setup all necessary state before running the test.
//-----------------------------------------------------------------------------
RC OutOfMem::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());

    CHECK_RC(InitFromJs());

    m_TimeoutMs = GetTestConfiguration()->TimeoutMs();
    m_ClientCount = LwRmPtr::GetValidClientCount();

    // We will be using multiple channels in this test
    m_TestConfig.SetAllowMultipleChannels(true);
    m_TestConfig.SetPushBufferLocation(Memory::Fb);

    return rc;
}

//! \brief Run() : The body of this test.
//!
//! Allocates mutiple channels,object per channel and forces some 9097work(NOP)
//! till it runs out of memory giving INSUFFICIENT_RESOURCES errors
//! For all other errors the test fails.
//------------------------------------------------------------------------------
RC OutOfMem::Run()
{
    RC rc = 0;
    UINT32 i = 0; //count for the client number
    GpuPtr pGpu;

    // Allocate resources for all test channels
    for (m_ChnlNo = 0; (m_ChnlNo < MAXCHANNELS) && (rc == OK); m_ChnlNo++)
    {
        if (IsClassSupported(FERMI_A))
        {
            //Allocate Channel
            if (!(rc =AllocChannel(LwRmPtr(i).Get(),
                          &m_ClientChannelPB[m_ChnlNo],
                          &m_ClientChannelErr[m_ChnlNo],
                          &m_ClientChannelHandle[m_ChnlNo])))
            {
                m_pClientChannel[m_ChnlNo] = LwRmPtr(i)->GetChannel(
                                             m_ClientChannelHandle[m_ChnlNo]);
                //Allocate object
                if (!(rc =LwRmPtr(i)->Alloc(m_ClientChannelHandle[m_ChnlNo],
                                          &hFermiObject[m_ChnlNo],
                                          FERMI_A, NULL)))
                {
                    if (!(rc = m_NotifierClient[m_ChnlNo].Allocate(
                                                m_pClientChannel[m_ChnlNo],
                                                1,
                                                m_TestConfig.DmaProtocol(),
                                                Memory::Fb, GetBoundGpuDevice())))
                    {
                        CHECK_RC(m_pClientChannel[m_ChnlNo]->SetObject(GR_OBJ_SUBCH,
                                                      hFermiObject[m_ChnlNo]));

                        CHECK_RC(m_NotifierClient[m_ChnlNo].Instantiate(GR_OBJ_SUBCH));
                                m_NotifierClient[m_ChnlNo].Clear(LW9097_NOTIFIERS_NOTIFY);

                        CHECK_RC(m_pClientChannel[m_ChnlNo]->Write(GR_OBJ_SUBCH,
                                                    LW9097_NOTIFY,
                                                    LW9097_NOTIFY_TYPE_WRITE_ONLY));
                        CHECK_RC(m_pClientChannel[m_ChnlNo]->Write(GR_OBJ_SUBCH,
                                                    LW9097_NO_OPERATION,
                                                    0));
                        CHECK_RC(m_pClientChannel[m_ChnlNo]->Flush());
                        CHECK_RC(m_NotifierClient[m_ChnlNo].Wait(
                                                        LW9097_NOTIFIERS_NOTIFY,
                                                        m_TimeoutMs));
                    }
                }
            }
        }
        i++;
        i %= m_ClientCount;
    }

    if (rc == RC::LWRM_INSUFFICIENT_RESOURCES)
    {
        Printf(Tee::PriLow,"\nINSUFFICIENT_RESOURCES, channels allocated = %d \n",m_ChnlNo);
        return OK;
    }
    else
        return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
RC OutOfMem::Cleanup()
{
    RC      rc = OK;
    UINT32  i = 0;
    UINT32  ch;

    for(ch = 0; ch< m_ChnlNo; ch++)
    {
        m_ClientChannelPB[ch].Free();
        if (IsClassSupported(LWE2_CHANNEL_DMA))
        {
            m_Sync[ch].Free();
            m_TestConfig.FreeChannel(m_pClientChannel[ch]);
        }
        else
        {
            m_NotifierClient[ch].Free();
            LwRmPtr(i)->Free(m_ClientChannelHandle[ch]);
            m_ClientChannelErr[ch].Free();
        }
        i++;
        i %= m_ClientCount;
    }
    if(m_ChnlNo != MAXCHANNELS)
    {
        if(m_ClientChannelHandle[m_ChnlNo])
        {
            m_ClientChannelPB[m_ChnlNo].Free();
            if (IsClassSupported(LWE2_CHANNEL_DMA))
            {
                m_Sync[m_ChnlNo].Free();
                m_TestConfig.FreeChannel(m_pClientChannel[m_ChnlNo]);
            }
            else
            {
                LwRmPtr(i)->Free(m_ClientChannelHandle[m_ChnlNo]);
                m_ClientChannelErr[m_ChnlNo].Free();
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC OutOfMem::AllocChannel
(
    LwRm *pLwRm,
    Surface2D *pPBMem,
    Surface2D *pErrMem,
    LwRm::Handle *pChannelHandle
)
{
    GpuDevice *pGpuDevice = GetBoundGpuDevice();
    const UINT32 PBMemBytes = 1024*1024;
    const UINT32 GpFifoEntries = 256;
    UINT32 PBSize;
    RC rc;

    pErrMem->SetWidth(4096);
    pErrMem->SetPitch(4096);
    pErrMem->SetHeight(1);
    pErrMem->SetColorFormat(ColorUtils::Y8);
    pErrMem->SetLocation(Memory::Fb);
    pErrMem->SetProtect(Memory::ReadWrite);
    pErrMem->SetAddressModel(Memory::Paging);
    CHECK_RC(pErrMem->Alloc(pGpuDevice, pLwRm));
    CHECK_RC(pErrMem->Map(Gpu::UNSPECIFIED_SUBDEV, pLwRm));

    PBSize = PBMemBytes + GpFifoEntries * LW906F_GP_ENTRY__SIZE;
    pPBMem->SetAddressModel(Memory::Paging);
    pPBMem->SetWidth(PBSize);
    pPBMem->SetPitch(PBSize);
    pPBMem->SetHeight(1);
    pPBMem->SetColorFormat(ColorUtils::Y8);
    pPBMem->SetLocation(Memory::Fb);
    pPBMem->SetProtect(Memory::Readable);
    CHECK_RC(pPBMem->Alloc(pGpuDevice, pLwRm));
    CHECK_RC(pPBMem->Map(Gpu::UNSPECIFIED_SUBDEV, pLwRm));

    // Allocate the gpfifo channel itself.
    CHECK_RC(pLwRm->AllocChannelGpFifo(
        pChannelHandle,
        GF100_CHANNEL_GPFIFO,
        pErrMem->GetMemHandle(pLwRm),
        pErrMem->GetAddress(),
        pPBMem->GetMemHandle(pLwRm),
        pPBMem->GetAddress(),
        pPBMem->GetCtxDmaOffsetGpu(pLwRm),
        PBMemBytes,
        ((UINT08*)pPBMem->GetAddress()) + PBMemBytes,
        pPBMem->GetCtxDmaOffsetGpu(pLwRm) + PBMemBytes,
        GpFifoEntries,
        0,  // context object
        0,  // verifFlags
        0,  // verifFlags2
        0,  // flags
        pGpuDevice));

    return rc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ Class9097Test object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(OutOfMem, RmTest,
                 "OutOfMem RM test - Test for OOM");
