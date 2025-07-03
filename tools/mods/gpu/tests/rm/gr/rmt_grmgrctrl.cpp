/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// GrMgrCtrl test.
//

#include "lwos.h"
#include "lwmisc.h"
#include "lwRmApi.h"
#include "core/include/jscript.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "core/include/lwrm.h"
#include "gpu/tests/rmtest.h"

#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/xp.h"
#include "core/include/tasker.h"
#include "gpu/tests/rm/utility/changrp.h"
#include "gpu/include/gralloc.h"

#include "class/cl0000.h"
#include "class/cl0080.h" // LW01_DEVICE_0
#include "ctrl/ctrl2080.h" // LW20_SUBDEVICE_XX
#include "class/clc6c0.h" // AMPERE_COMPUTE_A
#include "class/clc7c0.h" // AMPERE_COMPUTE_B
#include "class/clc637.h" // AMPERE_SMC_PARTITION_REF
#include "class/cla06fsubch.h" // LWA06F_SUBCHANNEL_*

#include "core/include/memcheck.h"

#include <vector>
#include <map>
#include <numeric>
#include <functional>

#define ILWALID_SWIZZID     0xFFFFFFFF
#define ILWALID_GRIDX       0xFFFFFFFF
#define MAX_SMC             8
#define MAX_GPC             8
#define MAX_CE              8
#define MAX_VEID            64
#define VEID_PER_GPC        (MAX_VEID/MAX_GPC)

class GrMgrCtrlTest: public RmTest
{
public:
    GrMgrCtrlTest();
    virtual ~GrMgrCtrlTest();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(TargetSwizzIdMask, LwU64); // user supplied mask of swizzids to test

private:
    LwRm           *m_pTestLwRm;
    LwRm::Handle    m_TestClient;
    LwRm::Handle    m_TestDev;
    LwRm::Handle    m_TestSubDev;

    LwBool m_bExhaustive;
    std::vector<LwU32> m_sizeFlags;
    LwU32 m_GrCount;
    LwU32 m_GpcCount;
    LwU32 m_VeidCount;
    LwU32 m_CeCount;
    LwU64 m_TargetSwizzIdMask;

    RC SetupLwRm();
    void TeardownLwRm();

    // Test functions
    RC TestAllSupportedPartitions(LwU32 testFlag);
    RC TestAllSupportedPartitionConfigurations(LwU32 swizzId);
    RC SetPartitioningMode(LwU32 mode);
    RC CreatePartition(LwU32 partitionFlag, LwU32& swizzId);
    RC ConfigurePartition(LwU32 swizzId, std::vector<LwU32> &gpcCount);
    RC DeconfigurePartition(LwU32 swizzId);
    RC VerifyPartition(LwU32 swizzId);
    RC DestroyPartition(LwU32 swizzId);
    RC TestPartition(LwU32 swizzId);

    // Helper Calls
    RC GetPartitionConfig(LwU32 swizzId, LwU32 &grEngCount, LwU32 &veidCount,
                       LwU32 &gpcCount, LwU32 &ceCount);
    RC GetGoldenPartitionConfig(LwU32 swizzId, LwU32 &grEngCount, LwU32 &veidCount,
                                LwU32 &gpcCount, LwU32 &ceCount);
    RC GetEngineConfig(LwU32 swizzId, std::vector<LwU32> &gpcPerGr,
                       std::vector<LwU32> &veidPerGr);
};

class SmcPartitionContext: public GrMgrCtrlTest
{
public:
    SmcPartitionContext(LwU32 swizzId);
    ~SmcPartitionContext();

    RC AllocResources();
    void FreeResources();
    RC SubscribePartition();
    void UnsubscribePartition();
    LwRm* GetPartitionClient();

private:
    LwRm           *m_pSmcLwRm;
    LwRm::Handle    m_hSmcClient;
    LwRm::Handle    m_hSmcDev;
    LwRm::Handle    m_hSmcSubDev;
    LwU32           m_swizzId;
    LwRm::Handle    m_hSmcRef;
    LwRm           *m_pDefaultTestLwRm;
};

SmcPartitionContext::SmcPartitionContext(LwU32 swizzId)
 : m_swizzId(swizzId)
{
}

SmcPartitionContext::~SmcPartitionContext()
{
}

//!
//! \brief  Allocate a new client, device, subdevice and
//!         other client stuff like VA space, context DMA etc.
//------------------------------------------------------------------------------
RC SmcPartitionContext::AllocResources()
{
    m_pSmcLwRm = LwRmPtr::GetFreeClient();
    if (nullptr == m_pSmcLwRm)
    {
        Printf(Tee::PriHigh, "Unable to get LwRm pointer.!\n");
        return RC::SOFTWARE_ERROR;
    }

    GetBoundGpuDevice()->Alloc(m_pSmcLwRm);

    m_hSmcClient = m_pSmcLwRm->GetClientHandle();
    m_hSmcDev = m_pSmcLwRm->GetDeviceHandle(GetBoundGpuDevice());
    m_hSmcSubDev = m_pSmcLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());

    return RC::OK;
}


//!
//! \brief  Free partition client and all resources under it
//------------------------------------------------------------------------------
void SmcPartitionContext::FreeResources()
{
    if (m_pSmcLwRm && !m_pSmcLwRm->IsFreed())
    {
        GetBoundGpuDevice()->Free(m_pSmcLwRm);
    }
}

//!
//! \brief  Subscribe to a partition
//------------------------------------------------------------------------------
RC SmcPartitionContext::SubscribePartition()
{
    RC rc = OK;
    LWC637_ALLOCATION_PARAMETERS params = {0};
    if (nullptr == m_pSmcLwRm)
    {
        Printf(Tee::PriHigh, "No client allocated for the partition!\n");
        return RC::SOFTWARE_ERROR;
    }

    params.swizzId = m_swizzId;
    rc = m_pSmcLwRm->Alloc(m_hSmcSubDev, &m_hSmcRef, AMPERE_SMC_PARTITION_REF, &params);
    return rc;
}

//!
//! \brief  Unsubscribe a partition
//------------------------------------------------------------------------------
void SmcPartitionContext::UnsubscribePartition()
{
    m_pSmcLwRm->Free(m_hSmcRef);
}

//!
//! \brief  Get client subscribed to partition
//------------------------------------------------------------------------------
LwRm* SmcPartitionContext::GetPartitionClient()
{
    MASSERT(m_pSmcLwRm != NULL);
    return m_pSmcLwRm;
}


//!
//! \brief GrMgrCtrlTest constructor doesn't do much
//! \sa Setup
//------------------------------------------------------------------------------
GrMgrCtrlTest::GrMgrCtrlTest()
{
    SetName("GrMgrCtrlTest");
}

//!
//! \brief GrMgrCtrlTest destructor
//!
//! GrMgrCtrlTest destructor does not do much.  Functionality
//! mostly lies in Cleanup().
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
GrMgrCtrlTest::~GrMgrCtrlTest()
{
}

//!
//! \brief Is GrMgrCtrlTest supported?
//!
//! Verify if Ampere compute class is supported in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------
string GrMgrCtrlTest::IsTestSupported()
{
    RC rc;

    if (LW_TRUE
#if LWCFG(GLOBAL_GPU_IMPL_GA100)
        && (GetBoundGpuSubdevice()->DeviceId() != Gpu::GA100)
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA10B)
        && (GetBoundGpuSubdevice()->DeviceId() != Gpu::GA10B)
#endif
       )
    {
        return "Supported only on GA100/GA10B";
    }

    if (OK != SetupLwRm())
        return "Failed to setup RM";

    return RUN_RMTEST_TRUE;
}

RC GrMgrCtrlTest::SetupLwRm()
{
    RC rc;
    LW0080_ALLOC_PARAMETERS allocDeviceParms;

    //
    // Mods core classes need LwRm abstraction, so better use a free Rm client
    // if created by Mods and then create hClient and otherr objects under it
    //
    m_pTestLwRm  = LwRmPtr::GetFreeClient();
    if (nullptr == m_pTestLwRm)
    {
        Printf(Tee::PriHigh, "Unable to get LwRm pointer.!\n");
        return RC::SOFTWARE_ERROR;
    }

    // Allocate a new client
    CHECK_RC_CLEANUP(m_pTestLwRm->AllocRoot());
    m_TestClient = m_pTestLwRm->GetClientHandle();

    // Allocate new device and subdevice under this client
    memset(&allocDeviceParms, 0, sizeof(allocDeviceParms));
    allocDeviceParms.deviceId = GetBoundGpuDevice()->GetDeviceInst();
    allocDeviceParms.flags = 0;
    allocDeviceParms.hClientShare = LW01_NULL_OBJECT;

    CHECK_RC_CLEANUP(m_pTestLwRm->AllocDevice(&allocDeviceParms));
    CHECK_RC_CLEANUP(m_pTestLwRm->AllocSubDevice(GetBoundGpuDevice()->GetDeviceInst(),
                                                 GetBoundGpuSubdevice()->GetSubdeviceInst()));

    m_TestDev = m_pTestLwRm->GetDeviceHandle(GetBoundGpuDevice());
    m_TestSubDev = m_pTestLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());

    return rc;

Cleanup:
    TeardownLwRm();
    return rc;
}

void GrMgrCtrlTest::TeardownLwRm()
{
    if (m_pTestLwRm && !m_pTestLwRm->IsFreed())
    {
        if (m_TestSubDev)
        {
            m_pTestLwRm->FreeSubDevice(GetBoundGpuDevice()->GetDeviceInst(),
                                       GetBoundGpuSubdevice()->GetSubdeviceInst());
        }

        if (m_TestDev)
        {
            m_pTestLwRm->FreeDevice(GetBoundGpuDevice()->GetDeviceInst());
        }

        if (m_TestClient)
        {
            m_pTestLwRm->FreeClient();
        }
    }
}

//!
//! \brief GrMgrCtrlTest Setup
//!
//! \return RC OK if all's well.
//------------------------------------------------------------------------------
RC GrMgrCtrlTest::Setup()
{
    RC rc = OK;
    LW_STATUS status;
    LW2080_CTRL_GPU_GET_PHYS_SYS_PIPE_IDS_PARAMS getGrMaskParams;
    LW2080_CTRL_GR_GET_GPC_MASK_PARAMS getGpcMaskParams;
    LW2080_CTRL_FIFO_GET_INFO_PARAMS getMaxVeidCountParams;

    m_sizeFlags =
    {
        LW2080_CTRL_GPU_PARTITION_FLAG_ONE_EIGHTHED_GPU
      , LW2080_CTRL_GPU_PARTITION_FLAG_ONE_QUARTER_GPU
      , LW2080_CTRL_GPU_PARTITION_FLAG_ONE_HALF_GPU
      , LW2080_CTRL_GPU_PARTITION_FLAG_FULL_GPU
    };

    memset(&getGrMaskParams, 0, sizeof(getGrMaskParams));
    status = LwRmControl(m_TestClient, m_TestSubDev,
                         LW2080_CTRL_CMD_GPU_GET_PHYS_SYS_PIPE_IDS,
                         &getGrMaskParams,
                         sizeof(getGrMaskParams));
    CHECK_RC(RmApiStatusToModsRC(status));
    m_GrCount = getGrMaskParams.physSysPipeCount;

    memset(&getGpcMaskParams, 0, sizeof(getGpcMaskParams));
    status = LwRmControl(m_TestClient, m_TestSubDev,
                         LW2080_CTRL_CMD_GR_GET_GPC_MASK,
                         &getGpcMaskParams,
                         sizeof(getGpcMaskParams));
    CHECK_RC(RmApiStatusToModsRC(status));
    m_GpcCount = lwPopCount32(getGpcMaskParams.gpcMask);

    memset(&getMaxVeidCountParams, 0, sizeof(getMaxVeidCountParams));
    getMaxVeidCountParams.fifoInfoTblSize = 1;
    getMaxVeidCountParams.fifoInfoTbl[0].index =
        LW2080_CTRL_FIFO_INFO_INDEX_MAX_SUBCONTEXT_PER_GROUP;

    status = LwRmControl(m_TestClient, m_TestSubDev,
                         LW2080_CTRL_CMD_FIFO_GET_INFO,
                         &getMaxVeidCountParams,
                         sizeof(getMaxVeidCountParams));
    CHECK_RC(RmApiStatusToModsRC(status));
    m_VeidCount = getMaxVeidCountParams.fifoInfoTbl[0].data;

    // TODO get async ce count from ctrl call
    m_CeCount = MAX_CE;

    m_bExhaustive = (0x0 == m_TargetSwizzIdMask);

    return rc;
}

//! \brief Run() : The body of this test.
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC GrMgrCtrlTest::Run()
{
    RC rc;

    CHECK_RC_CLEANUP(TestAllSupportedPartitions(0x0));

Cleanup:
    TeardownLwRm();
    return rc;
}

RC GrMgrCtrlTest::TestAllSupportedPartitions(LwU32 testFlag)
{
    LW_STATUS status = LW_OK;
    RC rc = OK;
    std::map<LwU32, LwU32> sizeCounts;
    LW2080_CTRL_GPU_GET_PARTITION_CAPACITY_PARAMS capParams;
    LwU32 swizzId;

    if (!m_bExhaustive && (0x0 == m_TargetSwizzIdMask))
    {
        return OK;
    }

    CHECK_RC(SetPartitioningMode(LW2080_CTRL_GPU_SET_PARTITIONING_MODE_REPARTITIONING_MAX_PERF));

    // Determine the number of partitions we can create of each type.
    for (std::vector<LwU32>::iterator it = m_sizeFlags.begin(); it != m_sizeFlags.end(); ++it)
    {
        memset(&capParams, 0, sizeof(capParams));
        capParams.partitionFlag = *it;

        // Skip configs of sizes tested by upper stack frames
        if ((0x0 != testFlag) && ((*it & testFlag) == testFlag))
            continue;

        status = LwRmControl(m_TestClient, m_TestSubDev,
                             LW2080_CTRL_CMD_GPU_GET_PARTITION_CAPACITY,
                             &capParams,
                             sizeof(capParams));
        CHECK_RC(RmApiStatusToModsRC(status));

        Printf(Tee::PriHigh, "Found capacity for partition size %d as %d!\n",
                capParams.partitionFlag, capParams.partitionCount);

        sizeCounts.insert(std::pair<LwU32, LwU32>(*it, capParams.partitionCount));
    }

    for (std::vector<LwU32>::iterator it = m_sizeFlags.begin(); it != m_sizeFlags.end(); ++it)
    {
        // Skip configs of sizes tested by upper stack frames
        if ((0x0 != testFlag) && ((*it & testFlag) == testFlag))
            continue;

        std::map<LwU32, LwU32>::iterator pPartCount = sizeCounts.find(*it);
        LwU32 partCount;
        std::vector<LwU32> swizzIds;

        if (sizeCounts.end() == pPartCount)
        {
            rc = RC::SOFTWARE_ERROR;
            return rc;
        }

        // If we can't make any partitions of the given size, move on
        partCount = (*pPartCount).second;
        if (0 == partCount)
            continue;

        for (LwU32 i = 0; i < partCount; ++i)
        {
            swizzId = ILWALID_SWIZZID;
            CHECK_RC(CreatePartition(*it, swizzId));

            if (!m_bExhaustive && (0x0 == (m_TargetSwizzIdMask & LWBIT64(swizzId))))
            {
                Printf(Tee::PriHigh, "Skipping duplicate partition ID %d.\n", swizzId);
                CHECK_RC(DestroyPartition(swizzId));
                break;
            }

            m_TargetSwizzIdMask &= ~(LWBIT64(swizzId));
            swizzIds.push_back(swizzId);

            CHECK_RC(VerifyPartition(swizzId));

            CHECK_RC(TestAllSupportedPartitionConfigurations(swizzId));

            // Relwrsively test all partitions of other sizes which can still be created
            TestAllSupportedPartitions(testFlag | *it);
        }

        while (!swizzIds.empty())
        {
            CHECK_RC(DestroyPartition(swizzIds.back()));
            swizzIds.pop_back();
        }
    }

    CHECK_RC(SetPartitioningMode(LW2080_CTRL_GPU_SET_PARTITIONING_MODE_REPARTITIONING_LEGACY));

    return rc;
}

RC GrMgrCtrlTest::TestAllSupportedPartitionConfigurations(LwU32 swizzId)
{
    StickyRC rc = OK;
    LwU32 grEngCount;
    LwU32 veidCount;
    LwU32 gpcCount;
    LwU32 ceCount;

    std::vector<std::vector<LwU32>> testConfigs =
    {
        {1, 1, 1, 1, 1, 1, 1, 1}
      , {2, 0, 2, 0, 2, 0, 2, 0}
      , {4, 0, 0, 0, 4, 0, 0, 0}
      , {1, 1, 2, 0, 4, 0, 0, 0}
      , {0, 2, 0, 2, 0, 2, 0, 2}
      , {0, 0, 0, 0, 0, 0, 0, 8}
      , {0, 0, 0, 0, 0, 0, 7, 0}
      , {0, 0, 0, 0, 0, 6, 0, 0}
      , {0, 0, 0, 0, 5, 0, 0, 0}
      , {0, 0, 0, 0, 3, 0, 0, 0}
      , {1, 1, 1, 1, 0, 0, 0, 0}
      , {2, 0, 2, 0, 0, 0, 0, 0}
      , {4, 0, 0, 0, 0, 0, 0, 0}
      , {1, 1, 0, 0, 0, 0, 0, 0}
      , {2, 0, 0, 0, 0, 0, 0, 0}
      , {1, 0, 0, 0, 0, 0, 0, 0}
    };

    CHECK_RC(GetPartitionConfig(swizzId, grEngCount, veidCount, gpcCount, ceCount));

    for (std::vector<std::vector<LwU32>>::iterator it = testConfigs.begin();
         it != testConfigs.end();
         ++it)
    {
        std::vector<LwU32> config = *it;
        std::vector<LwU32> configGpcsPerGr;
        std::vector<LwU32> configVeidsPerGr;
        std::vector<LwU32> goldelweidsPerGr = *it;

        // Each GR gets 8 * numGpcs VEIDs
        std::transform(goldelweidsPerGr.begin(), goldelweidsPerGr.end(),
                       goldelweidsPerGr.begin(),
                       std::bind(std::multiplies<LwU32>(), std::placeholders::_1, 8));

        // Ignore configs with more GPCs than we have
        LwU32 testGpcCount = std::accumulate(config.begin(), config.end(), 0);
        if (testGpcCount > gpcCount)
            continue;

        // Ignore configs which assign GPCs to engines we dont have
        bool bSkip = false;
        for (LwU32 i = grEngCount; i < config.size(); ++i)
        {
            if (config[i] > 0)
            {
                bSkip = true;
            }
        }
        if (bSkip)
            continue;

        CHECK_RC(ConfigurePartition(swizzId, *it));

        CHECK_RC(GetEngineConfig(swizzId, configGpcsPerGr, configVeidsPerGr));

        if (configGpcsPerGr != *it)
        {
            for (LwU32 i = 0; i < configGpcsPerGr.size(); ++i)
            {
                if ((*it)[i] != configGpcsPerGr[i])
                {
                    Printf(Tee::PriHigh,
                           "Test Error : Incorrect Partition GPC Assignment - Engine %d - Expected - %d, Received - %d\n",
                           i, (*it)[i], configGpcsPerGr[i]);
                    rc = RC::SOFTWARE_ERROR;
                }
            }
        }

        //
        // TODO Disabled until we have a good way to handle chip-specific
        // differences between GA100/101 and GA10B.
        //
        //if (configVeidsPerGr != goldelweidsPerGr)
        //{
        //    for (LwU32 i = 0; i < configVeidsPerGr.size(); ++i)
        //    {
        //        if (goldelweidsPerGr[i] != configVeidsPerGr[i])
        //        {
        //            Printf(Tee::PriHigh,
        //                   "Test Error : Incorrect Partition VEID Assignment - Engine %d - Expected - %d, Received - %d\n",
        //                   i, goldelweidsPerGr[i], configVeidsPerGr[i]);
        //            rc = RC::SOFTWARE_ERROR;
        //        }
        //    }
        //}

        CHECK_RC(rc);

        // Test confgured partition
        CHECK_RC(TestPartition(swizzId));

        CHECK_RC(DeconfigurePartition(swizzId));
    }

    return rc;
}

//! \brief Set GPU Partitioning mode
//!
//! \sa PartitioningMode
//------------------------------------------------------------------------------
RC GrMgrCtrlTest::SetPartitioningMode(LwU32 mode)
{
    LW_STATUS status;
    RC rc = OK;
    LW2080_CTRL_GPU_SET_PARTITIONING_MODE_PARAMS params = {0};

    params.partitioningMode = FLD_SET_DRF_NUM(2080_CTRL_GPU, _SET_PARTITIONING_MODE,
                                              _REPARTITIONING, mode,
                                              params.partitioningMode);

    status = LwRmControl(m_TestClient, m_TestSubDev,
                         LW2080_CTRL_CMD_GPU_SET_PARTITIONING_MODE,
                         &params,
                         sizeof(params));
    if (status != LW_OK)
    {
        CHECK_RC(RmApiStatusToModsRC(status));
    }

    return rc;
}

//! \brief Create GPU partitions
//!
//! \sa CreatePartition
//------------------------------------------------------------------------------
RC GrMgrCtrlTest::CreatePartition(LwU32 partitionFlag, LwU32& swizzId)
{
    LW_STATUS status = LW_OK;
    RC rc = OK;
    LW2080_CTRL_GPU_SET_PARTITIONS_PARAMS params = {0};

    params.partitionCount = 1;
    params.partitionInfo[0].partitionFlag = partitionFlag;
    params.partitionInfo[0].bValid = LW_TRUE;

    status = LwRmControl(m_TestClient, m_TestSubDev,
                         LW2080_CTRL_CMD_GPU_SET_PARTITIONS,
                         &params,
                         sizeof(params));
    if (status != LW_OK)
    {
        CHECK_RC(RmApiStatusToModsRC(status));
    }
    swizzId = params.partitionInfo[0].swizzId;

    return rc;
}

//! \brief Destroy GPU partitions
//!
//! \sa DestroyPartition
//------------------------------------------------------------------------------
RC GrMgrCtrlTest::DestroyPartition(LwU32 swizzId)
{
    LW_STATUS status = LW_OK;
    RC rc = OK;
    LW2080_CTRL_GPU_SET_PARTITIONS_PARAMS params = {0};

    params.partitionCount = 1;
    params.partitionInfo[0].swizzId = swizzId;
    params.partitionInfo[0].bValid = LW_FALSE;

    status = LwRmControl(m_TestClient, m_TestSubDev,
                         LW2080_CTRL_CMD_GPU_SET_PARTITIONS,
                         &params,
                         sizeof(params));
    if (status != LW_OK)
    {
        CHECK_RC(RmApiStatusToModsRC(status));
    }

    return rc;
}

//! \brief Configures a GPU partitions by attaching specific GPCs with an SMC
//!
//! \sa ConfigurePartition
//------------------------------------------------------------------------------
RC  GrMgrCtrlTest::ConfigurePartition(LwU32 swizzId,
                                      std::vector<LwU32> &gpcPerGr)
{
    LW_STATUS status = LW_OK;
    RC rc = OK;
    LW2080_CTRL_GPU_CONFIGURE_PARTITION_PARAMS params = {0};
    LwU32 i;

    params.swizzId = swizzId;
    for (i = 0; i < gpcPerGr.size(); i++)
    {
        if (gpcPerGr[i] == 0)
            continue;

        params.gpcCountPerSmcEng[i] = gpcPerGr[i];
        params.updateSmcEngMask |= LWBIT32(i);
    }

    status = LwRmControl(m_TestClient, m_TestSubDev,
                         LW2080_CTRL_CMD_GPU_CONFIGURE_PARTITION,
                         &params,
                         sizeof(params));
    if (status != LW_OK)
    {
        CHECK_RC(RmApiStatusToModsRC(status));
    }

    return rc;
}

//! \brief Deconfigures a GPU partition, removing all GPCs from all engines
//!
//! \sa DeconfigurePartition
//------------------------------------------------------------------------------
RC  GrMgrCtrlTest::DeconfigurePartition(LwU32 swizzId)
{
    LW_STATUS status = LW_OK;
    LwU32 grEngCount;
    LwU32 unused;
    RC rc = OK;
    LW2080_CTRL_GPU_CONFIGURE_PARTITION_PARAMS params;

    CHECK_RC(GetPartitionConfig(swizzId, grEngCount, unused, unused, unused));

    memset(&params, 0, sizeof(params));
    params.swizzId = swizzId;
    for (LwU32 i = 0; i < grEngCount; ++i)
    {
        params.updateSmcEngMask |= LWBIT32(i);
    }

    status = LwRmControl(m_TestClient, m_TestSubDev,
                         LW2080_CTRL_CMD_GPU_CONFIGURE_PARTITION,
                         &params,
                         sizeof(params));
    if (status != LW_OK)
    {
        CHECK_RC(RmApiStatusToModsRC(status));
    }

    return rc;
}

//! \brief Verifies a GPU partition's config
//!
//! \sa VerifyPartition
//------------------------------------------------------------------------------
RC  GrMgrCtrlTest::VerifyPartition(LwU32 swizzId)
{
    StickyRC rc = OK;
    LwU32 grEngCount, grEngCountGold;
    LwU32 veidCount, veidCountGold;
    LwU32 gpcCount, gpcCountGold;
    LwU32 ceCount, ceCountGold;

    CHECK_RC(GetPartitionConfig(swizzId, grEngCount, veidCount, gpcCount, ceCount));
    CHECK_RC(GetGoldenPartitionConfig(swizzId, grEngCountGold, veidCountGold,
                                      gpcCountGold, ceCountGold));

    if (0 == swizzId)
    {
        // Any nonzero amount of resources is allowed in swizzid 0
        if (0 == grEngCount)
        {
            Printf(Tee::PriHigh,
                   "Test Error : Incorrect Partition GrEngine Count - Expected - >0, Received - %d\n",
                   grEngCount);
            rc = RC::SOFTWARE_ERROR;
        }

        if (0 == veidCount)
        {
            Printf(Tee::PriHigh,
                   "Test Error : Incorrect Partition VEID Count - Expected - >0, Received - %d\n",
                   veidCount);
            rc = RC::SOFTWARE_ERROR;
        }

        if (0 == gpcCount)
        {
            Printf(Tee::PriHigh,
                   "Test Error : Incorrect Partition GPC Count - Expected - >0, Received - %d\n",
                   gpcCount);
            rc = RC::SOFTWARE_ERROR;
        }

        if (0 == ceCount)
        {
            Printf(Tee::PriHigh,
                   "Test Error : Incorrect Partition CeEngine Count - Expected - >0, Received - %d\n",
                   ceCount);
            rc = RC::SOFTWARE_ERROR;
        }

        return rc;
    }

    if (grEngCountGold != grEngCount)
    {
        Printf(Tee::PriHigh,
               "Test Error : Incorrect Partition GrEngine Count - Expected - %d, Received - %d\n",
               grEngCountGold, grEngCount);
        rc = RC::SOFTWARE_ERROR;
    }

    //
    // TODO Disabled until we have a good way to handle chip-specific
    // differences between GA100/101 and GA10B.
    //
    //if (veidCountGold != veidCount)
    //{
    //    Printf(Tee::PriHigh,
    //           "Test Error : Incorrect Partition VEID Count - Expected - %d, Received - %d\n",
    //           veidCountGold, veidCount);
    //    rc = RC::SOFTWARE_ERROR;
    //}

    if (ceCountGold != ceCount)
    {
        Printf(Tee::PriHigh,
               "Test Error : Incorrect Partition CeEngine Count - Expected - %d, Received - %d\n",
               ceCountGold, ceCount);
        rc = RC::SOFTWARE_ERROR;
    }

    if (gpcCountGold != gpcCount)
    {
        Printf(Tee::PriHigh,
               "Test Error : Incorrect Partition GPC Count - Expected - %d, Received - %d\n",
               gpcCountGold, gpcCount);
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

//! \brief Tests a GPU partition by creating channels and doing some work on
//!        each SMC engine inside partition
//!
//! \sa TestPartition
//------------------------------------------------------------------------------
RC GrMgrCtrlTest::TestPartition(LwU32 swizzId)
{
    RC                                    rc = OK;
    Surface2D                             semaSurf;
    std::vector<LwU32>                    configGpcsPerGr;
    std::vector<LwU32>                    configVeidsPerGr;
    SmcPartitionContext                   SmcCtx(swizzId);
    LwRm*                                 pLwRm = NULL;
    LwRm*                                 pDefaultTestLwRm = NULL;
    LwU32 grEngCount;
    LwU32 unused;

    SmcCtx.BindTestDevice(m_pTestDevice);

    CHECK_RC(GetPartitionConfig(swizzId, grEngCount, unused, unused, unused));
    CHECK_RC(GetEngineConfig(swizzId, configGpcsPerGr, configVeidsPerGr));

    Printf(Tee::PriHigh, "\n******\nTesting swizzId = %d with grCount = %d\n******\n", swizzId, grEngCount);
    for (LwU32 i = 0; i < grEngCount; i++)
        Printf(Tee::PriHigh, "GR%d has %d GPCs\n", i, configGpcsPerGr[i]);

    CHECK_RC_LABEL(done, SmcCtx.AllocResources());
    CHECK_RC_LABEL(done, SmcCtx.SubscribePartition());
    pLwRm = SmcCtx.GetPartitionClient();
    //
    // We have to change the default test client because most of the core MODS infrastructure
    // by default uses the bound RM client. Eventually core MODS infra should take a client as input
    //
    pDefaultTestLwRm = GetBoundRmClient();
    BindRmClient(pLwRm);

    // setup semaphore surface to be used by this partition
    semaSurf.SetForceSizeAlloc(true);
    semaSurf.SetArrayPitch(1);
    semaSurf.SetArraySize(0x1000);
    semaSurf.SetColorFormat(ColorUtils::VOID32);
    semaSurf.SetAddressModel(Memory::Paging);
    semaSurf.SetLayout(Surface2D::Pitch);
    semaSurf.SetLocation(Memory::Fb);
    CHECK_RC_LABEL(done, semaSurf.Alloc(GetBoundGpuDevice(), pLwRm));
    CHECK_RC_LABEL(done, semaSurf.Map(0, pLwRm));

    m_TestConfig.SetAllowMultipleChannels(true);

    // Walk thru every engine and allocate channel on each engine and do some work
    for (LwU32 i = 0; i < grEngCount; i++)
    {
        Channel *pCh;
        LwRm::Handle hComputeObj;
        ComputeAlloc computeAllocator;
        ChannelGroup chGrp(&m_TestConfig);
        LwU32 data = 0;

        if (configGpcsPerGr[i] == 0)
            continue;

        MASSERT(configGpcsPerGr[i] != 0);
        MASSERT(configVeidsPerGr[i] != 0);

        MEM_WR32(semaSurf.GetAddress(), 0xdeadbeef);

        chGrp.SetUseVirtualContext(LW_FALSE);
        chGrp.SetEngineId(LW2080_ENGINE_TYPE_GR(i));
        CHECK_RC_CLEANUP(chGrp.Alloc());

        CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh, 0));

        CHECK_RC_CLEANUP(computeAllocator.Alloc(pCh->GetHandle(), GetBoundGpuDevice(), pLwRm));
        hComputeObj = computeAllocator.GetHandle();
        CHECK_RC_CLEANUP(pCh->SetObject(LWA06F_SUBCHANNEL_COMPUTE, hComputeObj));

        CHECK_RC_CLEANUP(chGrp.Schedule(true, false));

        CHECK_RC_CLEANUP(pCh->Write(LWA06F_SUBCHANNEL_COMPUTE, LWC6C0_SET_REPORT_SEMAPHORE_A, LwU64_HI32(semaSurf.GetCtxDmaOffsetGpu())));
        CHECK_RC_CLEANUP(pCh->Write(LWA06F_SUBCHANNEL_COMPUTE, LWC6C0_SET_REPORT_SEMAPHORE_B, LwU64_LO32(semaSurf.GetCtxDmaOffsetGpu())));

        CHECK_RC_CLEANUP(pCh->Write(LWA06F_SUBCHANNEL_COMPUTE, LWC6C0_SET_REPORT_SEMAPHORE_C, 0));
        CHECK_RC_CLEANUP(pCh->Write(LWA06F_SUBCHANNEL_COMPUTE, LWC6C0_SET_REPORT_SEMAPHORE_D,
                             DRF_DEF(C6C0, _SET_REPORT_SEMAPHORE_D, _OPERATION, _RELEASE)));

        CHECK_RC_CLEANUP(chGrp.Flush());
        CHECK_RC_CLEANUP(chGrp.WaitIdle());

        data = MEM_RD32(semaSurf.GetAddress());
        if (data != 0)
        {
            rc = RC::DATA_MISMATCH;
            Printf(Tee::PriHigh, "Partition test failed for swizzId = %d. data expected = 0 data returned = %d!\n", swizzId, data);
            goto Cleanup;
        }
        else
        {
            Printf(Tee::PriHigh, "Partition test passed for swizzId = %d\n", swizzId);
        }
Cleanup:
        computeAllocator.Free();
        chGrp.Free();
        if (rc != OK)
            break;
    }
done:
    semaSurf.Free();
    SmcCtx.UnsubscribePartition();
    SmcCtx.FreeResources();
    BindRmClient(pDefaultTestLwRm);
    return rc;
}

//! \brief Return engine counts inside a partition
//!
//! \sa GetPartitionConfig
//------------------------------------------------------------------------------
RC GrMgrCtrlTest::GetPartitionConfig
(
    LwU32 swizzId,
    LwU32 &grEngCount,
    LwU32 &veidCount,
    LwU32 &gpcCount,
    LwU32 &ceCount
)
{
    LW_STATUS status;
    RC rc;
    LW2080_CTRL_GPU_GET_PARTITIONS_PARAMS getInfoParams;
    LwU32 i;

    memset(&getInfoParams, 0, sizeof(getInfoParams));

    getInfoParams.bGetAllPartitionInfo = true;
    status = LwRmControl(m_TestClient, m_TestSubDev,
                         LW2080_CTRL_CMD_GPU_GET_PARTITIONS,
                         &getInfoParams,
                         sizeof(getInfoParams));

    if (LW_OK != status)
    {
        CHECK_RC(RmApiStatusToModsRC(status));
    }

    for (i = 0; i < getInfoParams.validPartitionCount; ++i)
    {
        if (getInfoParams.queryPartitionInfo[i].bValid &&
            (getInfoParams.queryPartitionInfo[i].swizzId == swizzId))
        {
            break;
        }
    }

    // Check if this is a valid partition
    if (i >= getInfoParams.validPartitionCount)
    {
        Printf(Tee::PriHigh,"Test Error : Incorrect Partition valid state for SwizzId %u\n",
               swizzId);
        rc = RC::SOFTWARE_ERROR;
        return rc;
    }

    grEngCount = getInfoParams.queryPartitionInfo[i].grEngCount;
    veidCount = getInfoParams.queryPartitionInfo[i].veidCount;
    gpcCount = getInfoParams.queryPartitionInfo[i].gpcCount;
    ceCount = getInfoParams.queryPartitionInfo[i].ceCount;

    return OK;
}

//! \brief Return expected engine counts inside a partition
//!
//! \sa GetGoldenPartitionConfig
//------------------------------------------------------------------------------
RC GrMgrCtrlTest::GetGoldenPartitionConfig
(
    LwU32 swizzId,
    LwU32 &grEngCount,
    LwU32 &veidCount,
    LwU32 &gpcCount,
    LwU32 &ceCount
)
{
    switch (swizzId)
    {
        case 0:
            grEngCount = m_GrCount;
            veidCount = m_VeidCount;
            gpcCount = m_GpcCount;
            ceCount = m_CeCount;
            break;
        case 1:
        case 2:
            grEngCount = MAX_SMC / 2;
            veidCount = MAX_VEID / 2;
            gpcCount = MAX_GPC / 2;
            ceCount = MAX_CE / 2;
            break;
        case 3:
        case 4:
        case 5:
        case 6:
            grEngCount = MAX_SMC / 4;
            veidCount = MAX_VEID / 4;
            gpcCount = MAX_GPC / 4;
            ceCount = MAX_CE / 4;
            break;
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
            grEngCount = MAX_SMC / 8;
            veidCount = MAX_VEID / 8;
            gpcCount = MAX_GPC / 8;
            ceCount = MAX_CE / 8;
            break;
        default:
            Printf(Tee::PriHigh,"Test Error : Invalid Swizzid 0x%x\n", swizzId);
            return RC::SOFTWARE_ERROR;
    }

    return OK;
}

//! \brief Return per-engine counts inside partition
//!
//! \sa GetEngineConfig
//------------------------------------------------------------------------------
RC GrMgrCtrlTest::GetEngineConfig
(
    LwU32 swizzId,
    std::vector<LwU32> &gpcsPerGr,
    std::vector<LwU32> &veidsPerGr
)
{
    LW_STATUS status;
    RC rc;
    LW2080_CTRL_GPU_GET_PARTITIONS_PARAMS getInfoParams;
    LwU32 i;

    memset(&getInfoParams, 0, sizeof(getInfoParams));

    getInfoParams.bGetAllPartitionInfo = true;
    status = LwRmControl(m_TestClient, m_TestSubDev,
                         LW2080_CTRL_CMD_GPU_GET_PARTITIONS,
                         &getInfoParams,
                         sizeof(getInfoParams));

    if (LW_OK != status)
    {
        CHECK_RC(RmApiStatusToModsRC(status));
    }

    for (i = 0; i < getInfoParams.validPartitionCount; ++i)
    {
        if (getInfoParams.queryPartitionInfo[i].bValid &&
            (getInfoParams.queryPartitionInfo[i].swizzId == swizzId))
        {
            break;
        }
    }

    // Check if this is a valid partition
    if (i >= getInfoParams.validPartitionCount)
    {
        Printf(Tee::PriHigh,"Test Error : Incorrect Partition valid state for partition %u\n",
               swizzId);
        rc = RC::SOFTWARE_ERROR;
        return rc;
    }

    gpcsPerGr.clear();
    veidsPerGr.clear();

    for (LwU32 j = 0;  j < MAX_SMC; ++j)
    {
        gpcsPerGr.push_back(getInfoParams.queryPartitionInfo[i].gpcsPerGr[j]);
        veidsPerGr.push_back(getInfoParams.queryPartitionInfo[i].veidsPerGr[j]);
    }

    return OK;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
RC GrMgrCtrlTest::Cleanup()
{
    return OK;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ GrMgrCtrlTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(GrMgrCtrlTest, RmTest,
                 "GrMgrCtrlTest RM test - Test GRMGR control calls");
CLASS_PROP_READWRITE(GrMgrCtrlTest, TargetSwizzIdMask, LwU64,
                    "User supplied mask of swizzids to test. Default = 0x0");
