/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// Peer To Peer functionality test
// Brief on the test :-
// 1. Consider 2 GpuSubdevices say pGpuDevFirst an pGpuDevSecond.
//      Alternatively, these may be the same GpuSubdevice,
//      and their Peer to Peer loopback connection will be tested.
// 2. Allocate 4 surfaces on pGpuDevFirst say m_surf2d[*].
// 3. Allocate 2 surfaces(ReadSurf, WriteSurf) on pGpuDevSecond.
// 4. Create Remote mapping from pGpuDevFirst to pGpuDevSecond.
// 5. for each surface in pGpuDevFirst ...
//    a. Test Reads: create DmaWrapper object and copy m_surf2d[i] to m_readsurf.
//       compare both surfaces and report failure on mismatch.
//    b. Test Writes: create DmaWrapper object and write m_writesurf to m_surf2d[*]
//       compare both surfaces and report failure on mismatch.
// 6. Unmap all the surfaces to m_surf2d[*].
// 7. Free all resources and exit with pass.
//

#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/surf2d.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/dmawrap.h"
#include "gpu/include/gralloc.h"
#include "gpu/tests/rm/lwlink/rmt_lwlinkverify.h"

#include "ctrl/ctrl0000.h"  // LW0000_CTRL_CMD_SYSTEM_GET_P2P_CAPS
#include "core/include/memcheck.h"

#define SURFACE_WIDTH              80
#define SURFACE_HEIGHT             60
#define MAX_NUM_MAILBOXES          4

//
// #Runs of P2P copies if LWLink L2 feature is supported
// Note: Do NOT change value of this define to 1 because it
//       will prevent LWLink L2 path from getting exercised
//
#define MAX_ITERATIONS_LWLINK_L2   3

static bool memdiff(UINT32 *pFirst, UINT32 *pSecond, UINT32 size);

class PeerToPeerTest: public RmTest
{
    Surface2D m_surf2d[MAX_NUM_MAILBOXES];
    Surface2D m_readsurf;
    Surface2D m_writesurf;

    GpuDevMgr *m_pGpuDevMgr;

    GpuDevice *m_pGpuDevFirst;
    GpuDevice *m_pGpuDevSecond;

    Channel     *m_pChanSrc;
    LwRm::Handle m_hChanSrc;

    GpuTestConfiguration m_TestConfig;
    UINT32               m_NumDevices;

    UINT32 GetNumSubDevices();
    bool IsP2pEnabled(GpuSubdevice *pSubdev1, GpuSubdevice *pSubdev2);
    bool IsDmaWrapperSupported(GpuDevice *pDev);
    LwlinkVerify lwlinkVerif;

public:
    PeerToPeerTest();

    virtual ~PeerToPeerTest();
    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
};

//! \brief PeerToPeerTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
PeerToPeerTest::PeerToPeerTest()
{
    SetName("PeerToPeerTest");
}

//! \brief PeerToPeerTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
PeerToPeerTest::~PeerToPeerTest()
{
   // Taken care in CleanUp.
}

//! \brief Whether or not the test is supported in the current environment
//!
//! On the current environment where the test is been exelwted there are
//! chances that the classes required by this test are not supported,
//! IsTestSupported checks to see whether those classes are supported
//! or not and this decides whether current test should
//! be ilwoked or not. If supported then the test will be ilwoked.
//!
//! \return True if the required classes are supported in the current
//!         environment, false otherwise
//-----------------------------------------------------------------------------
string PeerToPeerTest::IsTestSupported()
{

    m_pGpuDevMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;

    m_NumDevices = GetNumSubDevices();
    GpuDevice *pDev1 = NULL, *pDev2 = NULL;
    GpuSubdevice *pSubdev1 = NULL, *pSubdev2 = NULL;

    if (m_NumDevices == 1)
    {
        pDev1 = m_pGpuDevMgr->GetFirstGpuDevice();
        pSubdev1 = pDev1->GetSubdevice(0);

        if ((IsP2pEnabled(pSubdev1, pSubdev1)) && (IsDmaWrapperSupported(pDev1)))
        {
            return RUN_RMTEST_TRUE;
        }
    }
    else
    {
        if (m_NumDevices >= 2)
        {
            pDev1    = m_pGpuDevMgr->GetFirstGpuDevice();
            pSubdev1 = pDev1->GetSubdevice(0);

            pDev2    = m_pGpuDevMgr->GetNextGpuDevice(pDev1);
            pSubdev2 = pDev2->GetSubdevice(0);

            if ( (IsDmaWrapperSupported(pDev1)) &&
                 (IsDmaWrapperSupported(pDev2)) &&
                 (IsP2pEnabled(pSubdev1, pSubdev2)) )
            {
                return RUN_RMTEST_TRUE;
            }
        }
    }

    return "Lwrrently, configuration does not support P2P.";
}

//! \brief Setup all necessary state before running the test.
//!
//! Idea is to reserve all the resources required by this test so if
//! required multiple tests can be ran in parallel.
//! \return RC -> OK if everything is allocated, test-specific RC if something
//!         failed while selwring resources.
//! \sa IsTestSupported
//-----------------------------------------------------------------------------
RC PeerToPeerTest::Setup()
{
    RC rc = OK;
    UINT32 width, height;

    // 1. Consider 2 GpuSubdevices say pGpuDevFirst and pGpuDevSecond.
    m_pGpuDevFirst = m_pGpuDevMgr->GetFirstGpuDevice();

    if (m_NumDevices == 1)
    {
        // Loopback setting;
        m_pGpuDevSecond = m_pGpuDevFirst;
    }
    else
    {
        m_pGpuDevSecond = m_pGpuDevMgr->GetNextGpuDevice(m_pGpuDevFirst);
    }

    if (lwlinkVerif.IsSupported(m_pGpuDevFirst->GetSubdevice(0)) &&
        lwlinkVerif.IsSupported(m_pGpuDevSecond->GetSubdevice(0)))
    {
        lwlinkVerif.Setup(m_pGpuDevMgr, m_NumDevices);
    }

    // 2. Allocate 4 surfaces on pGpuDevFirst say m_surf2d[*].
    width = SURFACE_WIDTH, height = SURFACE_HEIGHT;
    for (UINT32 i = 0; i < MAX_NUM_MAILBOXES ; i++)
    {
        m_surf2d[i].SetWidth(width);
        m_surf2d[i].SetHeight(height);
        m_surf2d[i].SetLayout(Surface2D::Pitch);
        m_surf2d[i].SetColorFormat(ColorUtils::A8R8G8B8);
        m_surf2d[i].SetLocation(Memory::Fb);
        m_surf2d[i].SetProtect(Memory::ReadWrite);
        m_surf2d[i].SetVirtAlignment(4096);
        CHECK_RC(m_surf2d[i].Alloc(m_pGpuDevFirst));
        CHECK_RC(m_surf2d[i].Map(m_pGpuDevFirst->GetSubdevice(0)->GetSubdeviceInst()));
        CHECK_RC(m_surf2d[i].Fill(0x55555555, m_pGpuDevFirst->GetSubdevice(0)->GetSubdeviceInst()));
    }

    // 3. Allocate 2 surfaces (ReadSurf, WriteSurf) on pGpuDevSecond.
    m_readsurf.SetWidth(width);
    m_readsurf.SetHeight(height);
    m_readsurf.SetLayout(Surface2D::Pitch);
    m_readsurf.SetColorFormat(ColorUtils::A8R8G8B8);
    m_readsurf.SetLocation(Memory::Fb);
    m_readsurf.SetProtect(Memory::ReadWrite);
    m_readsurf.SetVirtAlignment(4096);
    CHECK_RC(m_readsurf.Alloc(m_pGpuDevSecond));
    CHECK_RC(m_readsurf.Map(m_pGpuDevSecond->GetSubdevice(0)->GetSubdeviceInst()));
    CHECK_RC(m_readsurf.Fill(0xAAAAAAAA, m_pGpuDevSecond->GetSubdevice(0)->GetSubdeviceInst()));

    m_writesurf.SetWidth(width);
    m_writesurf.SetHeight(height);
    m_writesurf.SetLayout(Surface2D::Pitch);
    m_writesurf.SetColorFormat(ColorUtils::A8R8G8B8);
    m_writesurf.SetLocation(Memory::Fb);
    m_writesurf.SetProtect(Memory::ReadWrite);
    m_writesurf.SetVirtAlignment(4096);
    CHECK_RC(m_writesurf.Alloc(m_pGpuDevSecond));
    CHECK_RC(m_writesurf.Map(m_pGpuDevSecond->GetSubdevice(0)->GetSubdeviceInst()));
    CHECK_RC(m_writesurf.Fill(0xAAAAAAAA, m_pGpuDevSecond->GetSubdevice(0)->GetSubdeviceInst()));

    return rc;
}

//! \brief Run the test!
//! Create Remote mapping from pGpuDevFirst to pGpuDevSecond.
//! Test P2P Reads and Writes, using DmaWrapper object.
//! After P2P ops the result buffers will be compared, to say success or failure.
//! Note: DmaWrapper chooses the transfer method between mem2mem and ce, depending
//! on which one the GPU supports.
//! mem2mem and ce transfer uses default ctxdma(surf2d), but for remote ops we need remote ctxdma(surf2d)
//! Thus test save default ctxdma and override with remote one's and restore back to default.
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC PeerToPeerTest::Run()
{
    RC rc = OK;
    UINT32 bytesToCompare = 0;
    UINT32 no_iterations  = 1;
    UINT32 gpuSubdInstFirst = m_pGpuDevFirst->GetSubdevice(0)->GetSubdeviceInst();
    UINT32 gpuSubdInstSecond = m_pGpuDevSecond->GetSubdevice(0)->GetSubdeviceInst();

    // 4. Create Remote mapping from pGpuDevFirst to pGpuDevSecond.
    for (UINT32 i = 0 ; i < MAX_NUM_MAILBOXES ; i++)
    {
        if (m_NumDevices == 1 )
        {
            CHECK_RC(m_surf2d[i].MapLoopback());
        }
        else
        {
            CHECK_RC(m_surf2d[i].MapPeer(m_pGpuDevSecond));
        }
    }

    // Allocate channel and bind surfaces
    m_TestConfig.BindGpuDevice(m_pGpuDevSecond);
    CHECK_RC(m_TestConfig.AllocateChannel(&m_pChanSrc, &m_hChanSrc, LW2080_ENGINE_TYPE_GR(0)));

    for (UINT32 i = 0 ; i < MAX_NUM_MAILBOXES ; i++ )
        CHECK_RC(m_surf2d[i].BindGpuChannel(m_hChanSrc));

    CHECK_RC(m_readsurf.BindGpuChannel(m_hChanSrc));
    CHECK_RC(m_writesurf.BindGpuChannel(m_hChanSrc));

    // If LWLink is supported on both the GPUs
    if (lwlinkVerif.IsSupported(m_pGpuDevFirst->GetSubdevice(0)) &&
        lwlinkVerif.IsSupported(m_pGpuDevSecond->GetSubdevice(0)))
    {
        // If both the GPUs support LWLink L2 state
        if (lwlinkVerif.isPowerStateEnabled(m_pGpuDevFirst->GetSubdevice(0),  LW2080_CTRL_LWLINK_POWER_STATE_L0) &&
            lwlinkVerif.isPowerStateEnabled(m_pGpuDevFirst->GetSubdevice(0),  LW2080_CTRL_LWLINK_POWER_STATE_L2) &&
            lwlinkVerif.isPowerStateEnabled(m_pGpuDevSecond->GetSubdevice(0), LW2080_CTRL_LWLINK_POWER_STATE_L0) &&
            lwlinkVerif.isPowerStateEnabled(m_pGpuDevSecond->GetSubdevice(0), LW2080_CTRL_LWLINK_POWER_STATE_L2))
        {
            no_iterations = MAX_ITERATIONS_LWLINK_L2;
        }
    }

    for (UINT32 j = 0; j < no_iterations; j++)
    {
        Printf(Tee::PriLow, "Performing peer to peer copy - Run %d!\n", j);

        // 5. for each surface in pGpuDevFirst ...
        for (UINT32 i = 0 ; i < MAX_NUM_MAILBOXES ; i++)
        {
            // re-init the surface on the 1st gpu
            CHECK_RC(m_surf2d[i].Fill(0x55555555, m_pGpuDevFirst->GetSubdevice(0)->GetSubdeviceInst()));

            // init read and write surface contents
            CHECK_RC(m_readsurf.Fill(0xAAAAAAAA, m_pGpuDevSecond->GetSubdevice(0)->GetSubdeviceInst()));
            CHECK_RC(m_writesurf.Fill(0xAAAAAAAA, m_pGpuDevSecond->GetSubdevice(0)->GetSubdeviceInst()));

            // force usage of remote ctxdma's as default.
            LwRm::Handle saveCtxDmaHandle = m_surf2d[i].GetCtxDmaHandleGpu();
            UINT64 saveCtxDmaOffset = m_surf2d[i].GetCtxDmaOffsetGpu();
            m_surf2d[i].SetCtxDmaHandleGpu(m_surf2d[i].GetCtxDmaHandleGpuPeer(m_pGpuDevSecond));
            m_surf2d[i].SetCtxDmaOffsetGpu(m_surf2d[i].GetCtxDmaOffsetGpuPeer(gpuSubdInstFirst, m_pGpuDevSecond, gpuSubdInstSecond));

            bytesToCompare = ( m_surf2d[i].GetWidth() * m_surf2d[i].GetBytesPerPixel() * m_surf2d[i].GetHeight() / 4 );

            //  a. Test p2p Reads: create DmaWrapper object and write m_surf2d[i] to m_readsurf.
            //     compare both surfaces and report failure on mismatch.
            DmaWrapper *dw_rd = new DmaWrapper(&m_surf2d[i], &m_readsurf, &m_TestConfig);
            CHECK_RC(dw_rd->Copy(0, 0, (SURFACE_WIDTH * m_readsurf.GetBytesPerPixel()), SURFACE_HEIGHT, 0, 0,
                                        m_TestConfig.TimeoutMs() * 10, gpuSubdInstSecond, true /*Blocked*/));

            delete dw_rd;

            // Restore local ctxdma and its pointer as default
            m_surf2d[i].SetCtxDmaHandleGpu(saveCtxDmaHandle);
            m_surf2d[i].SetCtxDmaOffsetGpu(saveCtxDmaOffset);

            if (memdiff((UINT32*)m_readsurf.GetAddress(), (UINT32*)m_surf2d[i].GetAddress(), bytesToCompare))
                return RC::DATA_MISMATCH;

            m_surf2d[i].SetCtxDmaHandleGpu(m_surf2d[i].GetCtxDmaHandleGpuPeer(m_pGpuDevSecond));
            m_surf2d[i].SetCtxDmaOffsetGpu(m_surf2d[i].GetCtxDmaOffsetGpuPeer(gpuSubdInstFirst, m_pGpuDevSecond, gpuSubdInstSecond));

            //  b. Test Writes: create DmaWrapper object and write m_writesurf to m_surf2d[i].
            //     compare both surfaces and report failure on mismatch.
            DmaWrapper *dw_wr = new DmaWrapper(&m_writesurf, &m_surf2d[i], &m_TestConfig);
            CHECK_RC(dw_wr->Copy(0, 0, (SURFACE_WIDTH * m_writesurf.GetBytesPerPixel()), SURFACE_HEIGHT, 0, 0,
                                        m_TestConfig.TimeoutMs() * 10, gpuSubdInstSecond, true /*Blocked*/));

            delete dw_wr;

            // Restore local ctxdma and its pointer as default
            m_surf2d[i].SetCtxDmaHandleGpu(saveCtxDmaHandle);
            m_surf2d[i].SetCtxDmaOffsetGpu(saveCtxDmaOffset);

            if (memdiff((UINT32*)m_writesurf.GetAddress(), (UINT32*)m_surf2d[i].GetAddress(), bytesToCompare))
                return RC::DATA_MISMATCH;
        }

        // If both GPUs are connected by LWLink
        if (lwlinkVerif.IsSupported(m_pGpuDevFirst->GetSubdevice(0)) &&
            lwlinkVerif.IsSupported(m_pGpuDevSecond->GetSubdevice(0)))
        {
            // Verify the LWLink counters to determine traffic was over LWLink
            if (!lwlinkVerif.verifyP2PTraffic(m_pGpuDevFirst->GetSubdevice(0),
                m_pGpuDevSecond->GetSubdevice(0)))
            {
                return RC::LWLINK_BUS_ERROR;
            }
        }

        // If LWLink L2 is supported and this is not the last iteration
        if ((no_iterations == MAX_ITERATIONS_LWLINK_L2) && (j != (no_iterations - 1)))
        {
            // Transition the links to L2 state
            if (!lwlinkVerif.verifyPowerStateTransition(LW2080_CTRL_LWLINK_POWER_STATE_L2))
            {
                Printf(Tee::PriHigh, "Failed to transition the links into L2 state!\n");
                return RC::LWLINK_BUS_ERROR;
            }

            // Transition the links to L0 state
            if (!lwlinkVerif.verifyPowerStateTransition(LW2080_CTRL_LWLINK_POWER_STATE_L0))
            {
                Printf(Tee::PriHigh, "Failed to transition the links out of L2 state!\n");
                return RC::LWLINK_BUS_ERROR;
            }

            // Reset the LWLink throughput counters for the next run
            lwlinkVerif.resetLwlinkCounters();
        }
    }

    // Free channel
    CHECK_RC(m_TestConfig.FreeChannel(m_pChanSrc));
    m_TestConfig.BindGpuDevice(m_pGpuDevFirst);

    return rc;
}

//! Compare memory of some range
static bool memdiff(UINT32 *pFirst, UINT32 *pSecond, UINT32 size)
{
    UINT32 i = 0, *p1, *p2;

    for( p1 = pFirst, p2 = pSecond ; i < size ; p1++, p2++ )
    {
        UINT32 v1 = MEM_RD32(p1);
        UINT32 v2 = MEM_RD32(p2);

        if (v1 != v2)
        {
            Printf(Tee::PriHigh, "Surface2D mapping mismatched at 0x%0x%0x !\n",
                   (UINT32)((UINT64)p1 >> 32), (UINT32)((UINT64)p1 & 0xffffffff));
            return true;
        }
        i += 4;
    }
    return false;
}

//! \brief PeerToPeerTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC PeerToPeerTest::Cleanup()
{
    lwlinkVerif.Cleanup();

    // 6. Unmap all the surfaces to m_surf2d[*].
    // 7. Free all resources and exit with pass.
    for (UINT32 i = 0; i < MAX_NUM_MAILBOXES ; i++)
    {
        m_surf2d[i].Unmap();
        m_surf2d[i].Free();
    }

    m_readsurf.Unmap();
    m_readsurf.Free();

    m_writesurf.Unmap();
    m_writesurf.Free();

    return OK;
}

//!
//! Get the number of devices in the machine.
//!-----------------------------------------------------------------------------
UINT32 PeerToPeerTest::GetNumSubDevices()
{
    LwRmPtr pLwRm;
    UINT32 NumSubdevices = 0;

    LW0000_CTRL_GPU_GET_ATTACHED_IDS_PARAMS gpuAttachedIdsParams = {{0}};
    pLwRm->Control(pLwRm->GetClientHandle(),
                   LW0000_CTRL_CMD_GPU_GET_ATTACHED_IDS,
                   &gpuAttachedIdsParams,
                   sizeof(gpuAttachedIdsParams));

    // Count num devices and subdevices.
    for (UINT32 i = 0; i < LW0000_CTRL_GPU_MAX_ATTACHED_GPUS; i++)
    {
        if (gpuAttachedIdsParams.gpuIds[i] == GpuDevice::ILWALID_DEVICE)
            continue;
        NumSubdevices++;
    }

    return NumSubdevices;
}

//!
//! Checks for P2P capability in a Gpu
//!------------------------------------------------------------------------------
bool PeerToPeerTest::IsP2pEnabled(GpuSubdevice *pSubdev1, GpuSubdevice *pSubdev2)
{
    LwRmPtr pLwRm;
    LW0000_CTRL_SYSTEM_GET_P2P_CAPS_PARAMS  p2pCapsParams;
    memset(&p2pCapsParams, 0, sizeof(p2pCapsParams));

    if (pSubdev1 == pSubdev2)
    {
        p2pCapsParams.gpuIds[0] = pSubdev1->GetGpuId();
        p2pCapsParams.gpuCount  = 1;
    }
    else
    {
        p2pCapsParams.gpuIds[0] = pSubdev1->GetGpuId();
        p2pCapsParams.gpuIds[1] = pSubdev2->GetGpuId();
        p2pCapsParams.gpuCount  = 2;
    }

    RC rc = pLwRm->Control(pLwRm->GetClientHandle(),
                           LW0000_CTRL_CMD_SYSTEM_GET_P2P_CAPS,
                           &p2pCapsParams, sizeof(p2pCapsParams));

    if (OK != rc)
        return false;

    if ( ( FLD_TEST_DRF(0000_CTRL_SYSTEM, _GET_P2P_CAPS, _READS_SUPPORTED,  _FALSE, p2pCapsParams.p2pCaps) &&
           FLD_TEST_DRF(0000_CTRL_SYSTEM, _GET_P2P_CAPS, _WRITES_SUPPORTED, _FALSE, p2pCapsParams.p2pCaps) ) ||
         ( (pSubdev1 == pSubdev2) && FLD_TEST_DRF(0000_CTRL_SYSTEM, _GET_P2P_CAPS, _LOOPBACK_SUPPORTED, _FALSE, p2pCapsParams.p2pCaps)) )
    {
        Printf(Tee::PriLow, "Peer-to-peer not supported.\n");
        return false;
    }

    return true;
}

//!
//! Reports the If DmaWrapper class is supported
//!----------------------------------------------------------------------------
bool PeerToPeerTest::IsDmaWrapperSupported(GpuDevice *pDev)
{
    DmaCopyAlloc allocCE;
    return allocCE.IsSupported(pDev);
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ PeerToPeerTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(PeerToPeerTest, RmTest,
                 "Peer to peer functionality test.");
