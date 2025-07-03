/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012,2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// Peer To Peer large memory  functionality test
// Brief on the test :-
// 1. Consider 2 GpuSubdevices say pGpuDevFirst an pGpuDevSecond.
// 2. Allocate 4 surfaces on pGpuDevFirst say m_surf2d[*].
// 3. Allocate 2 surfaces(m_readsurf_beyond16GB, m_writesurf_beyond16GB) on pGpuDevSecond.
// 4. Create Remote mapping from pGpuDevFirst to pGpuDevSecond.
// 5. for each surface in pGpuDevFirst ...
//    a. Test Reads: create DmaWrapper object and copy m_surf2d[i] to m_readsurf_beyond16GB.
//       compare both surfaces and report failure on mismatch.
//    b. Test Writes: create DmaWrapper object and write m_writesurf_beyond16GB to m_surf2d[*]
//       compare both surfaces and report failure on mismatch.
// 6. Unmap all the surfaces to m_surf2d[*].
// 7. Free all resources and exit with pass.

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

#include "class/cl826f.h"   // LW82_CHANNEL_GPFIFO
#include "class/cl906f.h"   // GF100_CHANNEL_GPFIFO
#include "class/cla0b5.h"   // KEPLER_DMA_COPY_A
#include "ctrl/ctrl0000.h"  // LW0000_CTRL_CMD_SYSTEM_GET_P2P_CAPS
#include "core/include/memcheck.h"

#define SURFACE_WIDTH 80
#define SURFACE_HEIGHT 60
#define MAX_NUM_MAILBOXES 4

static bool memdiff(UINT32 *pFirst, UINT32 *pSecond, UINT32 size);

class PeerToPeerLargeMemTest: public RmTest
{
    Surface2D m_surf2d[MAX_NUM_MAILBOXES];
    Surface2D m_readsurf_beyond16GB;
    Surface2D m_writesurf_beyond16GB;

    GpuDevMgr *m_pGpuDevMgr;

    GpuDevice *m_pGpuDevFirst;
    GpuDevice *m_pGpuDevSecond;

    Channel *    m_pChanSrc;
    LwRm::Handle m_hChanSrc;

    GpuTestConfiguration    m_TestConfig;
    UINT32 m_NumDevices;

    UINT32 GetNumSubDevices();
    bool IsP2pEnabled(GpuSubdevice *pSubdev1, GpuSubdevice *pSubdev2);
    bool IsDmaWrapperSupported(GpuDevice *pDev);

public:
    PeerToPeerLargeMemTest();

    virtual ~PeerToPeerLargeMemTest();
    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
};

//! \brief PeerToPeerLargeMemTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
PeerToPeerLargeMemTest::PeerToPeerLargeMemTest()
{
    SetName("PeerToPeerLargeMemTest");
}

//! \brief PeerToPeerLargeMemTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
PeerToPeerLargeMemTest::~PeerToPeerLargeMemTest()
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
string PeerToPeerLargeMemTest::IsTestSupported()
{
// This test should be updated to support modern GPUs or simply removed.
#if 0
    m_pGpuDevMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;

    m_NumDevices = GetNumSubDevices();
    GpuDevice *pDev1 = NULL, *pDev2 = NULL;
    GpuSubdevice *pSubdev1 = NULL, *pSubdev2 = NULL;
    UINT32 gpuId1, gpuId2;

    if (m_NumDevices >= 2)
    {
        pDev1    = m_pGpuDevMgr->GetFirstGpuDevice();
        pSubdev1 = pDev1->GetSubdevice(0);
        gpuId1   = pSubdev1->DeviceId();
        pDev2    = m_pGpuDevMgr->GetNextGpuDevice(pDev1);
        pSubdev2 = pDev2->GetSubdevice(0);
        gpuId2   = pSubdev2->DeviceId();

        // GK110 is able to access beyond 16GB of peer memory. Lets test this capability. (0x400000000 = 16*1024*1024*1024)
        // This test uses hardcoded physical addresses; Vista onwards, memory is managed by OS. So, to get allocation at
        // a specific physical address, we should use WindowsXP
        if ((m_NumDevices >= 2) &&
            (gpuId1 == Gpu::GK110) &&
            (gpuId2 == Gpu::GK110) &&
            ((pSubdev2->GetFB()->GetGraphicsRamAmount()) > 0x400000000ULL) &&
            (IsDmaWrapperSupported(pDev1)) &&
            (IsDmaWrapperSupported(pDev2)) &&
            (IsP2pEnabled(pSubdev1, pSubdev2)))
                return RUN_RMTEST_TRUE;
    }
#endif
    return "This platform does not support this test";
}

//! \brief Setup all necessary state before running the test.
//!
//! Idea is to reserve all the resources required by this test so if
//! required multiple tests can be ran in parallel.
//! \return RC -> OK if everything is allocated, test-specific RC if something
//!         failed while selwring resources.
//! \sa IsTestSupported
//-----------------------------------------------------------------------------
RC PeerToPeerLargeMemTest::Setup()
{
    RC rc = OK;
    UINT32 width, height;
    UINT64 addr1, addr2;

    width = SURFACE_WIDTH, height = SURFACE_HEIGHT;
    // Important note: Make sure that there is FB present at addr1 and addr2. If not, change addr1/2 accordingly.
    addr1 = 0x400000000ULL;  // 0x400000000  = 16 *1024*1024*1024;
    addr2 = 0x1FC0000000ULL; // 0x1FC0000000 = 127*1024*1024*1024;

    // 1. Consider 2 GpuSubdevices say pGpuDevFirst and pGpuDevSecond.
    m_pGpuDevFirst  = m_pGpuDevMgr->GetFirstGpuDevice();
    m_pGpuDevSecond = m_pGpuDevMgr->GetNextGpuDevice(m_pGpuDevFirst);

    // 2. Allocate 4 surfaces on pGpuDevFirst say m_surf2d[*].
    width = SURFACE_WIDTH, height = SURFACE_HEIGHT;
    for (UINT32 i = 0; i < MAX_NUM_MAILBOXES ; i++ )
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

    // 3. Allocate 2 surfaces (ReadSurf_beyond16GB, WriteSurf_beyond16GB) on pGpuDevSecond.
    m_readsurf_beyond16GB.SetWidth(width);
    m_readsurf_beyond16GB.SetHeight(height);
    m_readsurf_beyond16GB.SetLayout(Surface2D::Pitch);
    m_readsurf_beyond16GB.SetColorFormat(ColorUtils::A8R8G8B8);
    m_readsurf_beyond16GB.SetLocation(Memory::Fb);
    m_readsurf_beyond16GB.SetProtect(Memory::ReadWrite);
    m_readsurf_beyond16GB.SetVirtAlignment(4096);
    m_readsurf_beyond16GB.SetGpuPhysAddrHint(addr1); //SetGpuVirtAddrHint(addr1); //trying to access beyond 16GB
    CHECK_RC(m_readsurf_beyond16GB.Alloc(m_pGpuDevSecond));
    CHECK_RC(m_readsurf_beyond16GB.Map(m_pGpuDevSecond->GetSubdevice(0)->GetSubdeviceInst()));
    CHECK_RC(m_readsurf_beyond16GB.Fill(0xAAAAAAAA, m_pGpuDevSecond->GetSubdevice(0)->GetSubdeviceInst()));

    m_writesurf_beyond16GB.SetWidth(width);
    m_writesurf_beyond16GB.SetHeight(height);
    m_writesurf_beyond16GB.SetLayout(Surface2D::Pitch);
    m_writesurf_beyond16GB.SetColorFormat(ColorUtils::A8R8G8B8);
    m_writesurf_beyond16GB.SetLocation(Memory::Fb);
    m_writesurf_beyond16GB.SetProtect(Memory::ReadWrite);
    m_writesurf_beyond16GB.SetVirtAlignment(4096);
    m_writesurf_beyond16GB.SetGpuPhysAddrHint(addr2); //SetGpuVirtAddrHint(addr2); //trying to access beyond 127GB; can access upto 128GB
    CHECK_RC(m_writesurf_beyond16GB.Alloc(m_pGpuDevSecond));
    CHECK_RC(m_writesurf_beyond16GB.Map(m_pGpuDevSecond->GetSubdevice(0)->GetSubdeviceInst()));
    CHECK_RC(m_writesurf_beyond16GB.Fill(0xAAAAAAAA, m_pGpuDevSecond->GetSubdevice(0)->GetSubdeviceInst()));

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
RC PeerToPeerLargeMemTest::Run()
{
    RC rc = OK;
    UINT32 bytesToCompare    = 0;
    UINT32 gpuSubdInstFirst  = m_pGpuDevFirst->GetSubdevice(0)->GetSubdeviceInst();
    UINT32 gpuSubdInstSecond = m_pGpuDevSecond->GetSubdevice(0)->GetSubdeviceInst();

    // 4. Create Remote mapping from pGpuDevFirst to pGpuDevSecond.
    for( UINT32 i = 0 ; i < MAX_NUM_MAILBOXES ; i++ )
    {
       CHECK_RC(m_surf2d[i].MapPeer(m_pGpuDevSecond));
    }

    // Allocate channel and bind surfaces
    m_TestConfig.BindGpuDevice(m_pGpuDevSecond);
    CHECK_RC(m_TestConfig.AllocateChannel(&m_pChanSrc, &m_hChanSrc));

    for( UINT32 i = 0 ; i < MAX_NUM_MAILBOXES ; i++ )
        CHECK_RC(m_surf2d[i].BindGpuChannel(m_hChanSrc));

    CHECK_RC(m_readsurf_beyond16GB.BindGpuChannel(m_hChanSrc));
    CHECK_RC(m_writesurf_beyond16GB.BindGpuChannel(m_hChanSrc));

    // 5. for each surface in pGpuDevFirst ...
    for( UINT32 i = 0 ; i < MAX_NUM_MAILBOXES ; i++ )
    {
        CHECK_RC(m_readsurf_beyond16GB.Fill(0xAAAAAAAA, m_pGpuDevSecond->GetSubdevice(0)->GetSubdeviceInst()));
        CHECK_RC(m_writesurf_beyond16GB.Fill(0xAAAAAAAA, m_pGpuDevSecond->GetSubdevice(0)->GetSubdeviceInst()));

        // force usage of remote ctxdma's as default.
        LwRm::Handle saveCtxDmaHandle = m_surf2d[i].GetCtxDmaHandleGpu();
        UINT64 saveCtxDmaOffset = m_surf2d[i].GetCtxDmaOffsetGpu();
        m_surf2d[i].SetCtxDmaHandleGpu(m_surf2d[i].GetCtxDmaHandleGpuPeer(m_pGpuDevSecond));
        m_surf2d[i].SetCtxDmaOffsetGpu(m_surf2d[i].GetCtxDmaOffsetGpuPeer(gpuSubdInstFirst, m_pGpuDevSecond, gpuSubdInstSecond));

        bytesToCompare = ( m_surf2d[i].GetWidth() * m_surf2d[i].GetBytesPerPixel() * m_surf2d[i].GetHeight() / 4 );

        //  a. Test p2p Reads: create DmaWrapper object and write m_surf2d[i] to m_readsurf.
        //     compare both surfaces and report failure on mismatch.
        DmaWrapper *dw_rd_beyond16GB = new DmaWrapper(&m_surf2d[i], &m_readsurf_beyond16GB, &m_TestConfig);
        CHECK_RC(dw_rd_beyond16GB->Copy(0, 0, (SURFACE_WIDTH * m_readsurf_beyond16GB.GetBytesPerPixel()), SURFACE_HEIGHT, 0, 0,
                                    m_TestConfig.TimeoutMs() * 10, gpuSubdInstSecond, true /*Blocked*/));

        if (memdiff((UINT32*)m_readsurf_beyond16GB.GetAddress(), (UINT32*)m_surf2d[i].GetAddress(), bytesToCompare))
            return RC::DATA_MISMATCH;
        else
            Printf(Tee::PriHigh, "%s \n", "Tested for beyond 16GB read access capability and it Passed! ");

        delete dw_rd_beyond16GB;

        // Restore local ctxdma and its pointer as default
        m_surf2d[i].SetCtxDmaHandleGpu(saveCtxDmaHandle);
        m_surf2d[i].SetCtxDmaOffsetGpu(saveCtxDmaOffset);

        //
        m_surf2d[i].SetCtxDmaHandleGpu(m_surf2d[i].GetCtxDmaHandleGpuPeer(m_pGpuDevSecond));
        m_surf2d[i].SetCtxDmaOffsetGpu(m_surf2d[i].GetCtxDmaOffsetGpuPeer(gpuSubdInstFirst, m_pGpuDevSecond, gpuSubdInstSecond));

        //  b. Test Writes: create DmaWrapper object and write m_writesurf to m_surf2d[i].
        //     compare both surfaces and report failure on mismatch.
        DmaWrapper *dw_wr_beyond16GB = new DmaWrapper(&m_writesurf_beyond16GB, &m_surf2d[i], &m_TestConfig);
        CHECK_RC(dw_wr_beyond16GB->Copy(0, 0, (SURFACE_WIDTH * m_writesurf_beyond16GB.GetBytesPerPixel()), SURFACE_HEIGHT, 0, 0,
                                    m_TestConfig.TimeoutMs() * 10, gpuSubdInstSecond, true /*Blocked*/));

        if (memdiff((UINT32*)m_writesurf_beyond16GB.GetAddress(), (UINT32*)m_surf2d[i].GetAddress(), bytesToCompare))
            return RC::DATA_MISMATCH;
        else
            Printf(Tee::PriHigh, "%s \n", "Tested for beyond 16GB write access capability and it Passed! ");

        delete dw_wr_beyond16GB;

        // Restore local ctxdma and its pointer as default
        m_surf2d[i].SetCtxDmaHandleGpu(saveCtxDmaHandle);
        m_surf2d[i].SetCtxDmaOffsetGpu(saveCtxDmaOffset);
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
        i+=4;
    }
    return false;
}

//! \brief PeerToPeerLargeMemTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC PeerToPeerLargeMemTest::Cleanup()
{
    // 6. Unmap all the surfaces to m_surf2d[*].
    // 7. Free all resources and exit with pass.
    for (UINT32 i = 0; i < MAX_NUM_MAILBOXES ; i++)
    {
        m_surf2d[i].Unmap();
        m_surf2d[i].Free();
    }

    m_readsurf_beyond16GB.Free();
    m_writesurf_beyond16GB.Free();

    return OK;
}

//!
//! Get the number of devices in the machine.
//!-----------------------------------------------------------------------------
UINT32 PeerToPeerLargeMemTest::GetNumSubDevices()
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
bool PeerToPeerLargeMemTest::IsP2pEnabled(GpuSubdevice *pSubdev1, GpuSubdevice *pSubdev2)
{
    LwRmPtr pLwRm;
    LW0000_CTRL_SYSTEM_GET_P2P_CAPS_PARAMS  p2pCapsParams;
    memset(&p2pCapsParams, 0, sizeof(p2pCapsParams));

    p2pCapsParams.gpuIds[0] =  pSubdev1->GetGpuId();
    p2pCapsParams.gpuIds[1] =  pSubdev2->GetGpuId();
    p2pCapsParams.gpuCount = 2;

    RC rc = pLwRm->Control(pLwRm->GetClientHandle(),
                           LW0000_CTRL_CMD_SYSTEM_GET_P2P_CAPS,
                           &p2pCapsParams, sizeof(p2pCapsParams));

    if (OK != rc)
        return false;

    if ((FLD_TEST_DRF(0000_CTRL_SYSTEM, _GET_P2P_CAPS, _READS_SUPPORTED, _FALSE,
         p2pCapsParams.p2pCaps) &&
         FLD_TEST_DRF(0000_CTRL_SYSTEM, _GET_P2P_CAPS, _WRITES_SUPPORTED, _FALSE,
         p2pCapsParams.p2pCaps)))
    {
        Printf(Tee::PriLow, "Peer-to-peer is not supported.\n");
        return false;
    }

    return true;
}

//!
//! Reports the If DmaWrapper class is supported
//!----------------------------------------------------------------------------
bool PeerToPeerLargeMemTest::IsDmaWrapperSupported(GpuDevice *pDev)
{
    return IsClassSupported(KEPLER_DMA_COPY_A);
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ PeerToPeerLargeMemTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(PeerToPeerLargeMemTest, RmTest,
                 "Peer to peer large memory functionality test.");
