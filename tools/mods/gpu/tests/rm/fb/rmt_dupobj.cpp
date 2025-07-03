/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_dupobj.cpp
//! \brief Object duplication test
//!
//! This test is written to verify object duplication functionality.
//! Object duplication is moving of memory objects between clients i.e.
//! allocating some primary surface in display driver and let other
//! applications to create a handle to that in their own client space.
//! The object duplication can be done on any platform.

#include "gpu/tests/rmtest.h"

#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "lwRmApi.h"
#include "lwos.h"
#include "gpu/include/gpudev.h"
#include "gpu/tests/rm/utility/changrp.h"
#include "gpu/include/gpusbdev.h"

#include "class/cl0000.h" // LW01_NULL_OBJECT
#include "class/cl0002.h" // LW01_CONTEXT_DMA
#include "class/cl0070.h" // LW01_MEMORY_VIRTUAL
#include "class/cl0080.h" // LW01_DEVICE_0
#include "class/cl003e.h" // LW01_CONTEXT_ERROR_TO_MEMORY
#include "class/cl50a0.h" // LW50_MEMORY_VIRTUAL
#include "class/cl9067.h" // FERMI_CONTEXT_SHARE_A

#include "ctrl/ctrla06c.h"
#include "core/include/memcheck.h"
//
// Defines used in this file for handles
//
#define HDEVICE_HANDLE      0xd34d0000
#define HDEVICEB_HANDLE     0xd34d0001
#define HDEVICEC_HANDLE     0xd34d0002
#define HMEM_HANDLE         0xd34d0010
#define VIRTA_HANDLE        0xd34d0020
#define VIRTB_HANDLE        0xd34d0021
#define HCTXSHARE_HANDLE1   0xd34d0010
#define HCTXSHARE_HANDLE2   0xd34d0011
#define HDUPTSG_HANDLE      0xd34d0003

class DupObjectTest: public RmTest
{
public:
    DupObjectTest();
    virtual ~DupObjectTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    RC DupSysMemTest();
    RC DupVidMemTest();
    RC DupVirtTest(bool);
    RC DupContextShareTest();
    RC DupTsgTest();
    RC TestTwoMemoryRegions(void *ptr1, void *ptr2);

    LwU32 m_hDevice;
    LwU32 m_hClient;
    LwU32 m_hDeviceB;
    LwU32 m_hClientB;
    LwU32 m_hDeviceC;
    LwU32 m_hClientC;
    ChannelGroup *m_pChGrp;
    LwRm::Handle m_systemMemHandle;
    LwRm::Handle m_vidMemHandle;
    LwRm::Handle m_virtHandle;
    LwRm::Handle m_hCtxShare;
    bool contextShareSupported;
    bool contextShareAllocated;

    Random m_rnd;

    UINT32 m_memSize;
};

//! \brief DupObjectTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
DupObjectTest::DupObjectTest()
{
    SetName("DupObjectTest");
    m_hDevice = 0;
    m_hClient = 0;
    m_hDeviceB = 0;
    m_hClientB = 0;
    m_hDeviceC = 0;
    m_hClientC = 0;
    m_systemMemHandle = 0;
    m_vidMemHandle = 0;
    m_virtHandle = 0;
    m_hCtxShare = 0;
    m_pChGrp = NULL;
    contextShareSupported = false;
    contextShareAllocated = false;
}

//! \brief DupObjectTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
DupObjectTest::~DupObjectTest()
{
}

//! \brief This test is supported on all platforms
//!
//! Object duplication can be done on any version of the chip so this test
//! is supported for all and is returning true.
//-----------------------------------------------------------------------------
string DupObjectTest::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief Setup all necessary state before running the test.
//!
//! Setup is used to reserve all the required resources used by this test,
//! This test tries to allocate system memory and the video memory and handles
//! for both is returned. These handles will be used to duplicate the memory
//! blocks in the Run ().
//!
//! \return RC -> OK if everything is allocated, test-specific RC if something
//!         failed while selwring resources.
//! \sa Run
//-----------------------------------------------------------------------------
RC DupObjectTest::Setup()
{
    UINT64 heapOffset;
    RC rc;
    LwRmPtr pLwRm;
    UINT32 attr;
    LW0080_CTRL_FIFO_GET_CAPS_PARAMS fifoCapsParams = {0};
    LwU8 *pFifoCaps = NULL;
    pFifoCaps = new LwU8[LW0080_CTRL_FIFO_CAPS_TBL_SIZE];
    if (pFifoCaps == NULL)
    {
        return RC::LWRM_INSUFFICIENT_RESOURCES;
    }
    fifoCapsParams.capsTblSize = LW0080_CTRL_FIFO_CAPS_TBL_SIZE;
    fifoCapsParams.capsTbl = (LwP64) pFifoCaps;

    if (pLwRm->ControlByDevice(GetBoundGpuDevice(), LW0080_CTRL_CMD_FIFO_GET_CAPS,
                                            &fifoCapsParams, sizeof(fifoCapsParams)))
    {
        return RC::SOFTWARE_ERROR;
    }

    // Lwrrently using the constant RM page size, will change if required
    const UINT32 rmPageSize = 0x1000;
    UINT32 memSize = rmPageSize * 2;

    m_memSize = memSize;

    CHECK_RC(pLwRm->AllocSystemMemory(
             &m_systemMemHandle,
             DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
             DRF_DEF(OS02, _FLAGS, _COHERENCY, _CACHED),
             m_memSize, GetBoundGpuDevice()));

    if (Memory::Fb == Surface2D::GetActualLocation(Memory::Fb, GetBoundGpuSubdevice()))
    {
        attr = LWOS32_ATTR_NONE;
    }
    else
    {
        attr  = DRF_DEF(OS32, _ATTR, _LOCATION, _PCI);
    }

    CHECK_RC(pLwRm->VidHeapAllocSize(LWOS32_TYPE_IMAGE,
                                     attr,
                                     LWOS32_ATTR2_NONE,
                                     m_memSize,
                                     &m_vidMemHandle,
                                     &heapOffset,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     GetBoundGpuDevice()));

    //
    // We want to run context duplicate test if we are supporting subcontext
    // Lwrrently skipping ctxshareTest due to TSG duplicate issue. Once TSG
    // duplicate is implemented, we can re-run this test.
    //
    if (LW0080_CTRL_FIFO_GET_CAP(pFifoCaps, LW0080_CTRL_FIFO_CAPS_MULTI_VAS_PER_CHANGRP))
    {
        LW_CTXSHARE_ALLOCATION_PARAMETERS ctxshareParams = {0};
        LW_CHANNEL_GROUP_ALLOCATION_PARAMETERS chGrpParams = {0};

        ctxshareParams.hVASpace = 0;
        ctxshareParams.flags = 0;
        chGrpParams.engineType = LW2080_ENGINE_TYPE_GR(0);

        m_TestConfig.SetAllowMultipleChannels(true);
        m_pChGrp = new ChannelGroup(&m_TestConfig, &chGrpParams);
        if (m_pChGrp == NULL)
        {
            rc = RC::SOFTWARE_ERROR;
        }
        CHECK_RC(m_pChGrp->Alloc());
        CHECK_RC(pLwRm->Alloc(m_pChGrp->GetHandle(),
                &m_hCtxShare,
                FERMI_CONTEXT_SHARE_A,
                &ctxshareParams));
        contextShareSupported = true;
        contextShareAllocated = true;
    }

    // Key-mashed "random" seed.
    m_rnd.SeedRandom(0x2d682);

    return rc;
}

//! \brief Run the test!
//!
//! After the setup is successful the allocated resources
//! (Video memory and System memory) are used in Run function.
//! Run uses the handles for both the memories and creates duplicate objects
//! for both in the new client space. To do so run allocates the new client
//! and device. After testing duplication functionality run frees the new
//! client. Freeing the client will also free the device.
//! Functionality is sub-divided in two sub-functions: duplicate System memory
//! object and duplicate Video memory object.
//!
//! \return OK if the test passed, specific RC if it failed
//! \sa DupSysMemTest
//! \sa DupVidMemTest
//-----------------------------------------------------------------------------
RC DupObjectTest::Run()
{
    bool allocatedClient = false;
    bool allocatedClientB = false;
    bool allocatedClientC = false;
    RC rc;
    RC errorRc;
    LwRmPtr pLwRm;
    UINT32 retVal;
    LW0080_ALLOC_PARAMETERS allocDeviceParms;

    m_hDevice = HDEVICE_HANDLE;
    m_hDeviceB = HDEVICEB_HANDLE;
    m_hDeviceC = HDEVICEC_HANDLE;

    //
    // Allocate a client and device, this uses
    // the global address space. The default mods
    // client uses a private address space.
    //
    retVal = LwRmAllocRoot(&m_hClient);
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(retVal));

    memset(&allocDeviceParms, 0, sizeof(allocDeviceParms));
    allocDeviceParms.deviceId = GetBoundGpuDevice()->GetDeviceInst();
    allocDeviceParms.flags = 0;
    allocDeviceParms.hClientShare = 0;
    retVal = LwRmAlloc(m_hClient,
                       m_hClient,
                       m_hDevice,
                       LW01_DEVICE_0,
                       (void*)&allocDeviceParms);
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(retVal));
    allocatedClient = true;

    //
    // Allocate a second client in the global address
    // space.
    //
    retVal = LwRmAllocRoot(&m_hClientB);
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(retVal));
    memset(&allocDeviceParms, 0, sizeof(allocDeviceParms));
    allocDeviceParms.deviceId = GetBoundGpuDevice()->GetDeviceInst();
    allocDeviceParms.flags = 0;
    allocDeviceParms.hClientShare = pLwRm->GetClientHandle();
    retVal = LwRmAlloc(m_hClientB,
                       m_hClientB,
                       m_hDeviceB,
                       LW01_DEVICE_0,
                       (void*)&allocDeviceParms);
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(retVal));
    allocatedClientB = true;

    //
    // Allocate a third client, also  in the global address
    // space.
    //
    retVal = LwRmAllocRoot(&m_hClientC);
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(retVal));
    memset(&allocDeviceParms, 0, sizeof(allocDeviceParms));
    allocDeviceParms.deviceId = GetBoundGpuDevice()->GetDeviceInst();
    allocDeviceParms.flags = 0;
    allocDeviceParms.hClientShare = pLwRm->GetClientHandle();
    retVal = LwRmAlloc(m_hClientC,
                       m_hClientC,
                       m_hDeviceC,
                       LW01_DEVICE_0,
                       (void*)&allocDeviceParms);
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(retVal));
    allocatedClientC = true;

    //
    // Test: Object duplication
    //
    CHECK_RC_CLEANUP(DupSysMemTest());
    CHECK_RC_CLEANUP(DupVidMemTest());

    if (IsClassSupported(LW50_MEMORY_VIRTUAL))
    {
        UINT32 attr = LWOS32_ATTR_NONE;
        UINT32 attr2 = LWOS32_ATTR2_NONE;
        LwU64 virtOffset;

        CHECK_RC(pLwRm->VidHeapAllocSizeEx(LWOS32_TYPE_IMAGE,
                                LWOS32_ALLOC_FLAGS_VIRTUAL,
                                &attr,
                                &attr2,
                                m_memSize,
                                0,
                                NULL,
                                NULL,
                                NULL,
                                &m_virtHandle,
                                &virtOffset,
                                NULL,
                                NULL,
                                NULL,
                                0,
                                0,
                                GetBoundGpuDevice()));

        CHECK_RC_CLEANUP(DupVirtTest(true));
        pLwRm->Free(m_virtHandle);
    }

    if (IsClassSupported(LW01_MEMORY_VIRTUAL))
    {
        LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vasAllocParams = { 0 };

        CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                              &m_virtHandle,
                              LW01_MEMORY_VIRTUAL,
                              &vasAllocParams));

        CHECK_RC_CLEANUP(DupVirtTest(true));
        pLwRm->Free(m_virtHandle);

        // sysmem dynamic stub object does not allow mappings
        LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS dynAllocParams = { 0 };
        dynAllocParams.hVASpace = LW_MEMORY_VIRTUAL_SYSMEM_DYNAMIC_HVASPACE;
        CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                              &m_virtHandle,
                              LW01_MEMORY_VIRTUAL,
                              &dynAllocParams));

        CHECK_RC_CLEANUP(DupVirtTest(false));
        pLwRm->Free(m_virtHandle);
    }

    if (contextShareSupported)
    {
        Printf(Tee::PriHigh,
              "DupObjectTest: Running context share\n");
        CHECK_RC_CLEANUP(DupContextShareTest());
    }

    CHECK_RC_CLEANUP(DupTsgTest());

// This cleanup will also be called in case of failure.
Cleanup:
    errorRc = rc;

    if (allocatedClient)
    {
        retVal = LwRmFree(m_hClient, m_hClient, m_hClient);
        CHECK_RC(RmApiStatusToModsRC(retVal));
    }
    if (allocatedClientB)
    {
        retVal = LwRmFree(m_hClientB, m_hClientB, m_hClientB);
        CHECK_RC(RmApiStatusToModsRC(retVal));
    }
    if (allocatedClientC)
    {
        retVal = LwRmFree(m_hClientC, m_hClientC, m_hClientC);
        CHECK_RC(RmApiStatusToModsRC(retVal));
    }

    if (errorRc != OK)
    {
        return errorRc;
    }

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! Cleanup all the allocated resources that the test selwred during Setup.
//! Cleanup can be called immediately if Setup fails or Run fails,
//! so cleaning up the allocated system memory, video memory.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
RC DupObjectTest::Cleanup()
{
    LwRmPtr pLwRm;

    pLwRm->Free(m_systemMemHandle);
    pLwRm->Free(m_vidMemHandle);

    if (m_virtHandle)
    {
        pLwRm->Free(m_virtHandle);
    }

    if (contextShareAllocated)
    {
        pLwRm->Free(m_hCtxShare);
        m_pChGrp->Free();
    }

    return OK;
}

//-----------------------------------------------------------------------------
// Private Member Functions
//-----------------------------------------------------------------------------

//! \brief Function called by Run () to test duplicating of System memory object
//!
//! This function uses the System memory handle to duplicate this system memory
//! object in the allocated client space, this returns the handle to duplicated
//! object.To verify whether the object got duplicated this function
//! maps the original object and the duplicate object. Then writes into
//! the original object and verifies whether the values we wrote are reflected
//! in the duplicated object.
//! As the duplicated object and mapping are used locally, object is freed and
//! unmapped in this function.
//!
//! \return OK if the object duplication passed without error and the values in
//! mapped memory matches, specific RC if it failed.
//! \sa Run
//! \sa Setup
//-----------------------------------------------------------------------------
RC DupObjectTest::DupSysMemTest()
{
    RC rc;
    RC errorRc;
    UINT32 retVal;
    LwRmPtr pLwRm;
    bool isSysMemMap = false;
    bool isDupSysMemMap = false;
    bool isSysMemDuplicated = false;

    void * pMapSysAddr = NULL;
    void * pMapDupSysAddr = NULL;
    UINT32 htoDupMem = HMEM_HANDLE;

    CHECK_RC_CLEANUP(pLwRm->MapMemory(m_systemMemHandle,
                                      0,
                                      m_memSize,
                                      &pMapSysAddr,
                                      0,
                                      GetBoundGpuSubdevice()));

    isSysMemMap = true;

    retVal = LwRmDupObject(m_hClient, m_hDevice, htoDupMem,
                           pLwRm->GetClientHandle(), m_systemMemHandle,
                           LW04_DUP_HANDLE_FLAGS_NONE);
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(retVal));

    isSysMemDuplicated = true;

    retVal = LwRmMapMemory(m_hClient, m_hDevice, htoDupMem, 0, m_memSize,
                           &pMapDupSysAddr, LW04_MAP_MEMORY_FLAGS_NONE);
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(retVal));

    isDupSysMemMap = true;

    CHECK_RC_CLEANUP(TestTwoMemoryRegions(pMapSysAddr, pMapDupSysAddr));

// This cleanup will also be called in case of failure.
Cleanup:
    errorRc = rc;

    if(isSysMemMap)
        pLwRm->UnmapMemory(m_systemMemHandle, pMapSysAddr, 0, GetBoundGpuSubdevice());

    if(isDupSysMemMap)
    {
        retVal = LwRmUnmapMemory(m_hClient, m_hDevice, htoDupMem,
                                 pMapDupSysAddr,
                                 LW04_MAP_MEMORY_FLAGS_NONE);
        CHECK_RC(RmApiStatusToModsRC(retVal));
    }

    if(isSysMemDuplicated)
    {
        retVal = LwRmFree(m_hClient, m_hDevice, htoDupMem);
        CHECK_RC(RmApiStatusToModsRC(retVal));
    }

    if (errorRc != OK)
    {
        return errorRc;
    }

    return rc;
}

//! \brief Function called by Run () to test duplicating of Video memory object
//!
//! This function uses the Video memory handle to duplicate this video memory
//! object in the allocated client space, this returns the handle to the
//! duplicated object.This function also maps the original video memory object
//! to the CPU address space.
//! To verify whether the video object got duplicated this function maps the
//! duplicated object to the CPU address space, places some values in that
//! and compares those with the mapped original video memory object.
//! As the duplicated object and mapping are used locally, object is freed and
//! unmapped in this function.
//!
//! \return OK if the object duplication passed without error and the values in
//! mapped memory matches, specific RC if it failed.
//! \sa Run
//! \sa Setup
//-----------------------------------------------------------------------------
RC DupObjectTest::DupVidMemTest()
{
    RC rc;
    RC errorRc;
    LwRmPtr pLwRm;
    UINT32 retVal;

    bool isVidMemMap = false;
    bool isDupVidMemMap = false;
    bool isVidMemDuplicated = false;

    void * pMapVidAddr = NULL;
    void * pMapDupVidAddr = NULL;

    UINT32 htoDupVidMem = HMEM_HANDLE;

    CHECK_RC_CLEANUP(pLwRm->MapMemory(m_vidMemHandle,
                                      0,
                                      m_memSize,
                                      &pMapVidAddr,
                                      0,
                                      GetBoundGpuSubdevice()));
    isVidMemMap = true;

    retVal = LwRmDupObject(m_hClient, m_hDevice, htoDupVidMem,
                           pLwRm->GetClientHandle(), m_vidMemHandle,
                           LW04_DUP_HANDLE_FLAGS_NONE);
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(retVal));
    isVidMemDuplicated = true;

    retVal = LwRmMapMemory(m_hClient, m_hDevice, htoDupVidMem, 0, m_memSize,
                           &pMapDupVidAddr, LW04_MAP_MEMORY_FLAGS_NONE);
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(retVal));
    isDupVidMemMap = true;

    CHECK_RC_CLEANUP(TestTwoMemoryRegions(pMapVidAddr, pMapDupVidAddr));

// Cleanup the locally allocated resources,also be called in case of failure.
Cleanup:
    errorRc = rc;

    if(isVidMemMap)
        pLwRm->UnmapMemory(m_vidMemHandle, pMapVidAddr, 0, GetBoundGpuSubdevice());

    if(isDupVidMemMap)
    {
        retVal = LwRmUnmapMemory(m_hClient, m_hDevice, htoDupVidMem,
                                 pMapDupVidAddr,
                                 LW04_MAP_MEMORY_FLAGS_NONE);
        CHECK_RC(RmApiStatusToModsRC(retVal));
    }

    if(isVidMemDuplicated)
    {
        retVal = LwRmFree(m_hClient, m_hDevice, htoDupVidMem);
        CHECK_RC(RmApiStatusToModsRC(retVal));
    }

    if (errorRc != OK)
    {
        return errorRc;
    }

    return rc;
}

//! \brief Function called by Run () to test duplicating of virtual memory object
//!
//! Both the valid case of duplication in one address space, and the illegal
//! case of a dup across different address spaces.
//!
//! \return OK if the object dup work as expected.
//! \sa Run
//! \sa Setup
//-----------------------------------------------------------------------------
RC DupObjectTest::DupVirtTest(bool bMap)
{
    LwRmPtr pLwRm;
    UINT32 retVal;
    UINT64 gpuAddrB = 0;
    UINT64 gpuAddrC = 0;
    bool   dupSysB = false;
    bool   dupVirtB = false;
    bool   mapB = false;
    bool   dupSysC = false;
    bool   dupVirtC = false;
    bool   mapC = false;
    RC errorRc;
    RC rc;

    retVal = LwRmDupObject(m_hClientB, m_hDeviceB, HMEM_HANDLE,
                           pLwRm->GetClientHandle(), m_systemMemHandle,
                           LW04_DUP_HANDLE_FLAGS_NONE);
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(retVal));
    dupSysB = true;
    retVal = LwRmDupObject(m_hClientC, m_hDeviceC, HMEM_HANDLE,
                           pLwRm->GetClientHandle(), m_systemMemHandle,
                           LW04_DUP_HANDLE_FLAGS_NONE);
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(retVal));
    dupSysC = true;

    // This duplication should pass as it is the same address space
    retVal = LwRmDupObject(m_hClientB, m_hDeviceB, VIRTA_HANDLE,
                           pLwRm->GetClientHandle(), m_virtHandle,
                           LW04_DUP_HANDLE_FLAGS_NONE);
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(retVal));
    dupVirtB = true;

    if (bMap)
    {
        // Use new virtual handle for RmMapMemoryDma
        retVal = LwRmMapMemoryDma(m_hClientB,
                                  m_hDeviceB,
                                  VIRTA_HANDLE,
                                  HMEM_HANDLE,
                                  0,
                                  m_memSize - 1,
                                  DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE) |
                                  DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE),
                                  &gpuAddrB);
        CHECK_RC_CLEANUP(RmApiStatusToModsRC(retVal));
        mapB = true;
    }

    // 3rd reference to this virtual object
    retVal = LwRmDupObject(m_hClientC, m_hDeviceC, VIRTA_HANDLE,
                           pLwRm->GetClientHandle(), m_virtHandle,
                           LW04_DUP_HANDLE_FLAGS_NONE);
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(retVal));
    dupVirtC = true;

    if (bMap)
    {
        // Remove mapping B and then dupB
        mapB = false;
        retVal = LwRmUnmapMemoryDma(m_hClientB,
                                    m_hDeviceB,
                                    VIRTA_HANDLE,
                                    HMEM_HANDLE,
                                    0,
                                    gpuAddrB);
        CHECK_RC_CLEANUP(RmApiStatusToModsRC(retVal));
    }
    dupVirtB = false;
    retVal = LwRmFree(m_hClientB, m_hDeviceB, VIRTA_HANDLE);
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(retVal));

    // Free original object so client C has the last reference
    pLwRm->Free(m_virtHandle);
    m_virtHandle = 0;

    if (bMap)
    {
        // Now map into client C. The virtual object should still be alive.
        retVal = LwRmMapMemoryDma(m_hClientC,
                                  m_hDeviceC,
                                  VIRTA_HANDLE,
                                  HMEM_HANDLE,
                                  0,
                                  m_memSize - 1,
                                  DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE) |
                                  DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE),
                                  &gpuAddrC);
        CHECK_RC_CLEANUP(RmApiStatusToModsRC(retVal));
        mapC = true;
    }

    // This call should fail as the VASpaces are different
    retVal = LwRmDupObject(m_hClient, m_hDevice, VIRTB_HANDLE,
                           pLwRm->GetClientHandle(), m_virtHandle,
                           LW04_DUP_HANDLE_FLAGS_NONE);
    if (RmApiStatusToModsRC(retVal) == OK)
    {
        Printf(Tee::PriHigh,
               "DupObjectTest: Dup across VASPACEs passed!\n");
        LwRmFree(m_hClient, m_hDevice, VIRTB_HANDLE);
        rc = RC::LWRM_ERROR;
    }
    else
    {
        rc = OK;
    }

Cleanup:
    errorRc = rc;

    if (mapB)
    {
        retVal = LwRmUnmapMemoryDma(m_hClientB,
                                    m_hDeviceB,
                                    VIRTA_HANDLE,
                                    HMEM_HANDLE,
                                    0,
                                    gpuAddrB);
        CHECK_RC(RmApiStatusToModsRC(retVal));
    }

    if (mapC)
    {
        retVal = LwRmUnmapMemoryDma(m_hClientC,
                                    m_hDeviceC,
                                    VIRTA_HANDLE,
                                    HMEM_HANDLE,
                                    0,
                                    gpuAddrC);
        CHECK_RC(RmApiStatusToModsRC(retVal));
    }

    if (dupVirtB)
    {
        retVal = LwRmFree(m_hClientB, m_hDeviceB, VIRTA_HANDLE);
        CHECK_RC(RmApiStatusToModsRC(retVal));
    }

    if (dupVirtC)
    {
        retVal = LwRmFree(m_hClientC, m_hDeviceC, VIRTA_HANDLE);
        CHECK_RC(RmApiStatusToModsRC(retVal));
    }

    if (dupSysB)
    {
        retVal = LwRmFree(m_hClientB, m_hDeviceB, HMEM_HANDLE);
        CHECK_RC(RmApiStatusToModsRC(retVal));
    }

    if (dupSysC)
    {
        retVal = LwRmFree(m_hClientC, m_hDeviceC, HMEM_HANDLE);
        CHECK_RC(RmApiStatusToModsRC(retVal));
    }

    if (errorRc != OK)
    {
        return errorRc;
    }

    return rc;
}

//! \brief Function called by Run () to test duplicating of context share object
//!
//! Test case 1: Duplicating valid object which should pass.
//! Test case 2: Duplicating different address spaces which should fail.
//! Test case 3: Free original ctx share object, we should still be able
//!              to access ctx share from case 1
//! Test case 4: Duplicate using ctx share from case 1, it should behave like
//!              a original ctx share object
//!
//! \return OK if the object dup work as expected.
//! \sa Run
//! \sa Setup
//-----------------------------------------------------------------------------
RC DupObjectTest::DupContextShareTest()
{
    LwRmPtr pLwRm;
    UINT32 retVal;
    bool isCtxShareBDuplicated = false;
    bool isCtxShareCDuplicated = false;
    LwRm::Handle htoDupCtxShareB = HCTXSHARE_HANDLE1;
    LwRm::Handle htoDupCtxShareC = HCTXSHARE_HANDLE2;
    RC rc;
    RC errorRc;

    // refer to Bug 1676400
    return OK;

    //
    // Test case 1 and Test case 4 must be updated with duplicated TSG to work.
    // For now, these two test cases will be commented out.
    //
    // Test case 1:
    retVal = LwRmDupObject(m_hClientB, m_pChGrp->GetHandle(), htoDupCtxShareB,
                           pLwRm->GetClientHandle(), m_hCtxShare,
                           LW04_DUP_HANDLE_FLAGS_NONE);
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(retVal));
    isCtxShareBDuplicated = true;
    Printf(Tee::PriHigh,
    "DupObjectTest: Dup of Context Share object from Original passed\n");

    // Test case 2:
    retVal = LwRmDupObject(m_hClient, m_pChGrp->GetHandle(), htoDupCtxShareC,
                           pLwRm->GetClientHandle(), m_hCtxShare,
                           LW04_DUP_HANDLE_FLAGS_NONE);
    if (RmApiStatusToModsRC(retVal) == OK)
    {
        Printf(Tee::PriHigh,
        "DupObjectTest: Dup of Context Share objects across VASPACEs passed!\n");
        LwRmFree(m_hClient, m_pChGrp->GetHandle(), htoDupCtxShareC);
        rc = RC::LWRM_ERROR;
    }
    else
    {
        rc = OK;
    }

    // Test Case 3: Let's free the original ctx share, we should
    // still be able to access ctx share B and dup it
    retVal = LwRmFree(pLwRm->GetClientHandle(), m_pChGrp->GetHandle(), m_hCtxShare);
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(retVal));
    contextShareAllocated = false;

    // Test case 4: Dup the duplicated ctx share
    retVal = LwRmDupObject(m_hClientC, m_pChGrp->GetHandle(), htoDupCtxShareC,
                           m_hClientB, htoDupCtxShareB,
                           LW04_DUP_HANDLE_FLAGS_NONE);
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(retVal));
    isCtxShareCDuplicated = true;
    Printf(Tee::PriHigh,
    "DupObjectTest: Dup of Context Share object from Duplicate passed\n");

Cleanup:
    errorRc = rc;

    if (isCtxShareBDuplicated)
    {
        retVal = LwRmFree(m_hClientB, m_pChGrp->GetHandle(), htoDupCtxShareB);
        CHECK_RC(RmApiStatusToModsRC(retVal));
    }
    if (isCtxShareCDuplicated)
    {
        retVal = LwRmFree(m_hClientC, m_pChGrp->GetHandle(), htoDupCtxShareC);
        CHECK_RC(RmApiStatusToModsRC(retVal));
    }
    if (errorRc != OK)
    {
        return errorRc;
    }
    return rc;
}

//! \brief Function called by Run () to test duplicating of channel group object
//!
//! Test case 1: Duplicate original valid Tsg object. Time slice on both original and dupe Tsg obj should be same
//! Test case 2: Set new timeslice on original Tsg object.
//!              Get timeslice on dupe Tsg obj. It should be updated to new value as on orginal Tsg obj
//!
//! \return OK if the Tsg object dup work as expected.
//! \sa Run
//! \sa Setup
//-----------------------------------------------------------------------------
RC DupObjectTest::DupTsgTest()
{
    LwRmPtr pLwRm;
    UINT32 retVal;
    RC rc;
    RC errorRc;
    LwRm::Handle hOrgTsgHandle;
    LwRm::Handle hDupTsgHandle = HDUPTSG_HANDLE;
    LWA06C_CTRL_TIMESLICE_PARAMS tsParams = {0};
    LwU64 tsUsOrg, tsUsDup;
    LW_CHANNEL_GROUP_ALLOCATION_PARAMETERS chGrpParams = {0};
    chGrpParams.engineType = LW2080_ENGINE_TYPE_GR(0);

    // Create a tsg object
    ChannelGroup *pChGrp = new ChannelGroup(&m_TestConfig, &chGrpParams);
    if (pChGrp == NULL)
    {
        rc = RC::SOFTWARE_ERROR;
    }
    CHECK_RC_CLEANUP(pChGrp->Alloc());

    hOrgTsgHandle = pChGrp->GetHandle();

    // Dupe the original Tsg object
    retVal = LwRmDupObject(m_hClientB, m_hDeviceB, hDupTsgHandle,
                           pLwRm->GetClientHandle(), hOrgTsgHandle,
                           LW04_DUP_HANDLE_FLAGS_NONE);
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(retVal));

    // Get the timeslice of both Tsg objects
    CHECK_RC_CLEANUP(LwRmControl(pLwRm->GetClientHandle(), hOrgTsgHandle,
            LWA06C_CTRL_CMD_GET_TIMESLICE, &tsParams, sizeof(tsParams)));
    tsUsOrg = tsParams.timesliceUs;

    CHECK_RC_CLEANUP(LwRmControl(m_hClientB, hDupTsgHandle,
            LWA06C_CTRL_CMD_GET_TIMESLICE, &tsParams, sizeof(tsParams)));
    tsUsDup = tsParams.timesliceUs;

    Printf(Tee::PriHigh, "%s: Default timeslice on original and dupe tsg obj is %lld and %lld respectively\n",
                __FUNCTION__, tsUsOrg, tsUsDup);

    // Check if time slices are same
    if (tsUsOrg != tsUsDup)
    {
        Printf(Tee::PriHigh, "%s: Timeslices are different (%lld != %lld). This means object dupe failed.\n",
                __FUNCTION__, tsUsOrg, tsUsDup);

        CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
    }

    // Triple the time slice of original Tsg object and set it
    tsParams.timesliceUs *= 3;
    tsUsOrg = tsParams.timesliceUs;
    CHECK_RC_CLEANUP(LwRmControl(pLwRm->GetClientHandle(), hOrgTsgHandle,
            LWA06C_CTRL_CMD_SET_TIMESLICE, &tsParams, sizeof(tsParams)));

    // Get timeslice of dupe Tsg object
    CHECK_RC_CLEANUP(LwRmControl(m_hClientB, hDupTsgHandle,
            LWA06C_CTRL_CMD_GET_TIMESLICE, &tsParams, sizeof(tsParams)));
    tsUsDup = tsParams.timesliceUs;

    Printf(Tee::PriHigh, "%s: New timeslice on original and dupe tsg obj is %lld and %lld respectively\n",
                __FUNCTION__, tsUsOrg, tsUsDup);

    // Check if time slices are same (since tsgs objects are dupe, changing timeslice of one should change timeslice of other)
    if (tsUsOrg != tsUsDup)
    {
        Printf(Tee::PriHigh, "%s: Updated timeslices are different (%lld != %lld). This means object dupe failed.\n",
                __FUNCTION__, tsUsOrg, tsUsDup);

        CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
    }

Cleanup:
    pChGrp->Free();
    LwRmFree(m_hClientB, m_hDeviceB, HDUPTSG_HANDLE);
    return rc;
}

// \brief Write unique values to the first mapped address and verify the second
// mapping corresponds to the same memory.
//
// It is expected that both regions are at least size m_memSize.
//
// \return OK if the two seem to map to the same memory
//-----------------------------------------------------------------------------
RC DupObjectTest::TestTwoMemoryRegions(void *ptr1, void *ptr2)
{
    UINT32 iLocalVar;
    UINT32 *pMapMemIterator;
    const UINT32 uniq = m_rnd.GetRandom();
    RC rc;

    pMapMemIterator = (UINT32 *)ptr1;

    for (iLocalVar = 0; iLocalVar < m_memSize/sizeof(UINT32); iLocalVar++)
    {
        MEM_WR32(pMapMemIterator, iLocalVar + uniq);
        pMapMemIterator++;
    }

    pMapMemIterator = (UINT32 *)ptr2;

    //
    // Call FlushCpuWriteCombineBuffer() which calls sfence instruction
    // This will cause all previous video memory operations to complete
    // before we go ahead and read back the same memory locations.
    //
    rc = Platform::FlushCpuWriteCombineBuffer();
    if (rc == RC::UNSUPPORTED_FUNCTION)
    {
        Printf(Tee::PriHigh, "DupObjectTest: MODS Returned Unsupported function\
                              when Platform::FlushCpuWriteCombineBuffer() was\
                              called..\n");
        CHECK_RC(rc);
    }

    for (iLocalVar = 0; iLocalVar < m_memSize/sizeof(UINT32); iLocalVar++)
    {
        if (MEM_RD32(pMapMemIterator) != (iLocalVar + uniq))
        {
            Printf(Tee::PriHigh,
                  "DupObjectTest: VidMem incorrect mappings to same memory\n");

            return RC::LWRM_ERROR;
        }
        pMapMemIterator++;
    }

    return OK;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ DupObjectTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(DupObjectTest, RmTest,
                 "DupObject functionality test.");
