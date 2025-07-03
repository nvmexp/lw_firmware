/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "gpu/tests/rmtest.h"
#include "core/include/platform.h"
#include "gpu/include/gpudev.h"
#include "lwRmApi.h"
#include "lwos.h"

#include "ctrl/ctrl0002.h"

#include "class/cl0000.h"    // LW01_NULL_OBJECT
#include "class/cl0002.h"    // LW01_CONTEXT_DMA
#include "class/cl003e.h"    // LW01_MEMORY_SYSTEM
#include "class/cl0040.h"    // LW01_MEMORY_LOCAL_USER

// V2.x Maxwell, Pascal
#include "class/cl9470.h"    // LW9470_DISPLAY
#include "class/cl947d.h"    // LW947D_CORE_CHANNEL_DMA
#include "class/cl9570.h"    // LW9570_DISPLAY
#include "class/cl957d.h"    // LW957D_CORE_CHANNEL_DMA
#include "class/cl9770.h"    // LW9770_DISPLAY
#include "class/cl977d.h"    // LW977D_CORE_CHANNEL_DMA
#include "class/cl9870.h"    // LW9870_DISPLAY
#include "class/cl987d.h"    // LW987D_CORE_CHANNEL_DMA

// V3.x Volta
#include "class/clc370.h"   // LWC370_DISPLAY
#include "class/clc37d.h"   // LWC37D_CORE_CHANNEL_DMA

// V4.x Turing, Ampere, Ada
#include "class/clc570.h"   // LWC570_DISPLAY
#include "class/clc57d.h"   // LWC57D_CORE_CHANNEL_DMA
#include "class/clc670.h"   // LWC670_DISPLAY
#include "class/clc770.h"   // LWC770_DISPLAY
#include "class/clc67d.h"   // LWC67D_CORE_CHANNEL_DMA
#include "class/clc77d.h"   // LWC77D_CORE_CHANNEL_DMA

class DispCtxDmaTest : public RmTest
{
public:
    DispCtxDmaTest();
    virtual ~DispCtxDmaTest()
    {
    };
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    LwU32 m_displayClass;
    LwU32 m_coreClass;

    LwHandle m_NextHandle;
    LwHandle m_hClient;
    LwHandle m_hDevice;

    LwHandle m_hDisp;

    LwHandle m_hChannel;
    LwHandle m_hPushBuffer;
    LwHandle m_hPushBufferCD;
    LwU32 *m_pPushBuffer;
    LwHandle m_hErrorNotifier;
    LwHandle m_hErrorNotifierCD;
    LwU32 *m_pErrorNotifier;

    RC testBind();
    RC testErrors();
    RC testUpdate();

    RC allocSysmem(LwU64 size, LwHandle *pHandle, LwU32 **ppCpu);
    RC allocCtxDma(LwHandle hDma, LwHandle hMemory, LwU64 offset, LwU64 size);
};

DispCtxDmaTest::DispCtxDmaTest()
{
    SetName("DispCtxDmaTest");

    m_displayClass = LW01_NULL_OBJECT;
    m_coreClass = LW01_NULL_OBJECT;

    m_NextHandle = 0xabc0000;
    m_hClient = LW01_NULL_OBJECT;
    m_hDevice = LW01_NULL_OBJECT;
    m_hDisp = LW01_NULL_OBJECT;
}

string DispCtxDmaTest::IsTestSupported()
{
    if (IsClassSupported(LW9470_DISPLAY))
    {
        m_displayClass = LW9470_DISPLAY;
        m_coreClass = LW947D_CORE_CHANNEL_DMA;
        return RUN_RMTEST_TRUE;
    }
    if (IsClassSupported(LW9570_DISPLAY))
    {
        m_displayClass = LW9570_DISPLAY;
        m_coreClass = LW957D_CORE_CHANNEL_DMA;
        return RUN_RMTEST_TRUE;
    }
    if (IsClassSupported(LW9770_DISPLAY))
    {
        m_displayClass = LW9770_DISPLAY;
        m_coreClass = LW977D_CORE_CHANNEL_DMA;
        return RUN_RMTEST_TRUE;
    }
    if (IsClassSupported(LW9870_DISPLAY))
    {
        m_displayClass = LW9870_DISPLAY;
        m_coreClass = LW987D_CORE_CHANNEL_DMA;
        return RUN_RMTEST_TRUE;
    }
    if (IsClassSupported(LWC370_DISPLAY))
    {
        m_displayClass = LWC370_DISPLAY;
        m_coreClass = LWC37D_CORE_CHANNEL_DMA;
        return RUN_RMTEST_TRUE;
    }
    if (IsClassSupported(LWC570_DISPLAY))
    {
        m_displayClass = LWC570_DISPLAY;
        m_coreClass = LWC57D_CORE_CHANNEL_DMA;
        return RUN_RMTEST_TRUE;
    }
    if (IsClassSupported(LWC670_DISPLAY))
    {
        m_displayClass = LWC670_DISPLAY;
        m_coreClass = LWC67D_CORE_CHANNEL_DMA;
        return RUN_RMTEST_TRUE;
    }
    if (IsClassSupported(LWC770_DISPLAY))
    {
        m_displayClass = LWC770_DISPLAY;
        m_coreClass = LWC77D_CORE_CHANNEL_DMA;
        return RUN_RMTEST_TRUE;
    }

    return "Unsupported classes";
}

RC DispCtxDmaTest::Setup()
{
    LW_STATUS status;
    RC rc;

    CHECK_RC(InitFromJs());

    status = LwRmAllocRoot(&m_hClient);
    CHECK_RC(RmApiStatusToModsRC(status));

    LW0080_ALLOC_PARAMETERS allocDeviceParams = { 0 };
    m_hDevice = m_NextHandle++;
    allocDeviceParams.deviceId = GetBoundGpuDevice()->GetDeviceInst();
    allocDeviceParams.hClientShare = m_hClient;
    status = LwRmAlloc(m_hClient,
                       m_hClient,
                       m_hDevice,
                       LW01_DEVICE_0,
                       &allocDeviceParams);
    CHECK_RC(RmApiStatusToModsRC(status));

    LWC370_ALLOCATION_PARAMETERS allocDispParams = { 0 };
    m_hDisp = m_NextHandle++;
    status = LwRmAlloc(m_hClient,
                       m_hDevice,
                       m_hDisp,
                       m_displayClass,
                       &allocDispParams);
    CHECK_RC(RmApiStatusToModsRC(status));

    CHECK_RC(allocSysmem(0x1000, &m_hPushBuffer, &m_pPushBuffer));
    m_hPushBufferCD = m_NextHandle++;
    CHECK_RC(allocCtxDma(m_hPushBufferCD, m_hPushBuffer, 0, 0x1000));
    CHECK_RC(allocSysmem(0x1000, &m_hErrorNotifier, &m_pErrorNotifier));
    m_hErrorNotifierCD = m_NextHandle++;
    CHECK_RC(allocCtxDma(m_hErrorNotifierCD, m_hErrorNotifier, 0, 0x1000));

    LW50VAIO_CHANNELDMA_ALLOCATION_PARAMETERS coreParams = { 0 };
    coreParams.hObjectBuffer = m_hPushBufferCD;
    coreParams.hObjectNotify = m_hErrorNotifierCD;
    m_hChannel = m_NextHandle++;
    status = LwRmAlloc(m_hClient,
                       m_hDisp,
                       m_hChannel,
                       m_coreClass,
                       &coreParams);
    CHECK_RC(RmApiStatusToModsRC(status));

    return rc;
}

/*!
 * @brief Run the Bind context dma tests
 */
RC DispCtxDmaTest::Run()
{
    RC rc = OK;

    CHECK_RC(testBind());
    CHECK_RC(testErrors());
    CHECK_RC(testUpdate());

    return rc;
}

RC DispCtxDmaTest::Cleanup()
{
    LwRmFree(m_hClient, m_hDisp, m_hChannel);
    LwRmFree(m_hClient, m_hDevice, m_hPushBufferCD);
    LwRmFree(m_hClient, m_hDevice, m_hPushBuffer);
    LwRmFree(m_hClient, m_hDevice, m_hErrorNotifierCD);
    LwRmFree(m_hClient, m_hDevice, m_hErrorNotifier);

    LwRmFree(m_hClient, m_hDevice, m_hDisp);

    LwRmFree(m_hClient, m_hDevice, m_hDevice);
    LwRmFree(m_hClient, m_hClient, m_hClient);

    return OK;
}

RC DispCtxDmaTest::allocSysmem(LwU64 size, LwHandle *pHandle, LwU32 **ppCpu)
{
    LW_MEMORY_ALLOCATION_PARAMS allocParams = { 0 };
    LW_STATUS status;
    RC rc;

    allocParams.owner = 0xcafe;
    allocParams.type = LWOS32_TYPE_IMAGE;
    allocParams.size = size;
    allocParams.attr = DRF_DEF(OS32, _ATTR, _LOCATION, _PCI) |
                       DRF_DEF(OS32, _ATTR, _COHERENCY, _CACHED) |
                       DRF_DEF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS);
    allocParams.attr2 = 0;

    *pHandle = m_NextHandle++;
    status = LwRmAlloc(m_hClient,
                       m_hDevice,
                       *pHandle,
                       LW01_MEMORY_SYSTEM,
                       &allocParams);
    CHECK_RC(RmApiStatusToModsRC(status));

    return OK;
}

RC DispCtxDmaTest::allocCtxDma(LwHandle hDma, LwHandle hMemory, LwU64 offset, LwU64 size)
{
    LW_STATUS status;
    RC rc;

    status = LwRmAllocContextDma2(m_hClient,
                                  hDma,
                                  LW01_CONTEXT_DMA,
                                  DRF_DEF(OS03, _FLAGS, _HASH_TABLE, _DISABLE) |
                                  DRF_DEF(OS03, _FLAGS, _MAPPING, _KERNEL),
                                  hMemory,
                                  offset,
                                  size - 1);
    CHECK_RC(RmApiStatusToModsRC(status));

    return OK;
}

RC DispCtxDmaTest::testBind()
{
    LwHandle hMemory;
    LwHandle hDma;
    LwU32 *p;
    LW_STATUS status;
    RC rc;

    LW0002_CTRL_BIND_CONTEXTDMA_PARAMS bindParams = { 0 };
    LW0002_CTRL_UNBIND_CONTEXTDMA_PARAMS unbindParams = { 0 };

    CHECK_RC(allocSysmem(0x1000, &hMemory, &p));
    hDma = m_NextHandle++;
    CHECK_RC_CLEANUP(allocCtxDma(hDma, hMemory, 0, 0x1000));

    bindParams.hChannel = m_hChannel;
    status = LwRmControl(m_hClient,
                         hDma,
                         LW0002_CTRL_CMD_BIND_CONTEXTDMA,
                         &bindParams, sizeof(bindParams));
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(status));

    // Free will unbind
    LwRmFree(m_hClient, m_hDevice, hDma);

    // Reallocate context dma with the same handle + rebind
    CHECK_RC_CLEANUP(allocCtxDma(hDma, hMemory, 0, 0x1000));

    bindParams.hChannel = m_hChannel;
    status = LwRmControl(m_hClient,
                         hDma,
                         LW0002_CTRL_CMD_BIND_CONTEXTDMA,
                         &bindParams, sizeof(bindParams));
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(status));

    unbindParams.hChannel = m_hChannel;
    status = LwRmControl(m_hClient,
                         hDma,
                         LW0002_CTRL_CMD_UNBIND_CONTEXTDMA,
                         &unbindParams, sizeof(unbindParams));
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(status));

Cleanup:
    LwRmFree(m_hClient, m_hDevice, hDma);
    LwRmFree(m_hClient, m_hDevice, hMemory);

    return rc;
}

RC DispCtxDmaTest::testErrors()
{
    LwHandle hMemory;
    LwHandle hDma;
    LwU32 *p;
    LW_STATUS status;
    RC rc;

    LW0002_CTRL_BIND_CONTEXTDMA_PARAMS bindParams = { 0 };
    LW0002_CTRL_UNBIND_CONTEXTDMA_PARAMS unbindParams = { 0 };

    CHECK_RC(allocSysmem(0x1000, &hMemory, &p));
    hDma = m_NextHandle++;
    CHECK_RC(allocCtxDma(hDma, hMemory, 0, 0x1000));

    bindParams.hChannel = m_hChannel;
    status = LwRmControl(m_hClient,
                         hDma,
                         LW0002_CTRL_CMD_BIND_CONTEXTDMA,
                         &bindParams, sizeof(bindParams));
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(status));

    // Second bind to the same channel should fail
    status = LwRmControl(m_hClient,
                         hDma,
                         LW0002_CTRL_CMD_BIND_CONTEXTDMA,
                         &bindParams, sizeof(bindParams));
    CHECK_RC_CLEANUP((status != LW_OK) ? OK : RC::LWRM_ILWALID_OBJECT_OLD);

    unbindParams.hChannel = m_hChannel;
    status = LwRmControl(m_hClient,
                         hDma,
                         LW0002_CTRL_CMD_UNBIND_CONTEXTDMA,
                         &unbindParams, sizeof(unbindParams));
    CHECK_RC_CLEANUP(RmApiStatusToModsRC(status));

    // Second unbind should fail
    status = LwRmControl(m_hClient,
                         hDma,
                         LW0002_CTRL_CMD_UNBIND_CONTEXTDMA,
                         &unbindParams, sizeof(unbindParams));
    CHECK_RC_CLEANUP((status != LW_OK) ? OK : RC::LWRM_ILWALID_OBJECT_OLD);

    // Bind to null object should fail
    bindParams.hChannel = LW01_NULL_OBJECT;
    status = LwRmControl(m_hClient,
                         hDma,
                         LW0002_CTRL_CMD_BIND_CONTEXTDMA,
                         &bindParams, sizeof(bindParams));
    CHECK_RC_CLEANUP((status != LW_OK) ? OK : RC::LWRM_ILWALID_OBJECT_OLD);

    // Bind to device object should fail
    bindParams.hChannel = m_hDevice;
    status = LwRmControl(m_hClient,
                         hDma,
                         LW0002_CTRL_CMD_BIND_CONTEXTDMA,
                         &bindParams, sizeof(bindParams));
    CHECK_RC_CLEANUP((status != LW_OK) ? OK : RC::LWRM_ILWALID_OBJECT_OLD);

    // Unbind from null object should fail
    unbindParams.hChannel = LW01_NULL_OBJECT;
    status = LwRmControl(m_hClient,
                         hDma,
                         LW0002_CTRL_CMD_UNBIND_CONTEXTDMA,
                         &unbindParams, sizeof(unbindParams));
    CHECK_RC_CLEANUP((status != LW_OK) ? OK : RC::LWRM_ILWALID_OBJECT_OLD);

    // Unbind from client object should fail
    unbindParams.hChannel = m_hClient;
    status = LwRmControl(m_hClient,
                         hDma,
                         LW0002_CTRL_CMD_UNBIND_CONTEXTDMA,
                         &unbindParams, sizeof(unbindParams));
    CHECK_RC_CLEANUP((status != LW_OK) ? OK : RC::LWRM_ILWALID_OBJECT_OLD);

Cleanup:
    LwRmFree(m_hClient, m_hDevice, hDma);
    LwRmFree(m_hClient, m_hDevice, hMemory);

    return rc;
}

RC DispCtxDmaTest::testUpdate()
{
    LwHandle hMemory;
    LwHandle hDma;
    LwU32 *p;
    LW_STATUS status;
    RC rc;

    CHECK_RC(allocSysmem(0x1000, &hMemory, &p));
    hDma = m_NextHandle++;
    CHECK_RC(allocCtxDma(hDma, hMemory, 0, 0x1000));

    LW0002_CTRL_BIND_CONTEXTDMA_PARAMS bindParams = { 0 };
    bindParams.hChannel = m_hChannel;
    status = LwRmControl(m_hClient,
                         hDma,
                         LW0002_CTRL_CMD_BIND_CONTEXTDMA,
                         &bindParams, sizeof(bindParams));
    CHECK_RC(RmApiStatusToModsRC(status));

    LW0002_CTRL_UPDATE_CONTEXTDMA_PARAMS updateParams = { 0 };
    updateParams.limit = (0x1000/2) - 1;
    updateParams.hCtxDma = hDma;
    updateParams.hChannel = m_hChannel;
    updateParams.flags = DRF_DEF(0002, _CTRL_CMD_UPDATE_CONTEXTDMA_FLAGS, _LIMIT, _VALID);
    status = LwRmControl(m_hClient,
                         hDma,
                         LW0002_CTRL_CMD_UPDATE_CONTEXTDMA,
                         &updateParams, sizeof(updateParams));
    CHECK_RC(RmApiStatusToModsRC(status));

    LwRmFree(m_hClient, m_hDevice, hDma);
    LwRmFree(m_hClient, m_hDevice, hMemory);

    return rc;
}

//! \brief Setup the JS hierarchy to match the C++ one.
JS_CLASS_INHERIT(DispCtxDmaTest, RmTest, "Display ContextDma test.\n");
