/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_ctrl2080gr.cpp
//! \brief LW2080_CTRL_CMD GR tests
//!
#include "ctrl/ctrl2080.h" // LW20_SUBDEVICE_XX
#include "gpu/tests/rmtest.h"
#include "core/include/platform.h"
#include "core/include/memcheck.h"

typedef struct
{
    LW2080_CTRL_GR_INFO bufferAlignment;
    LW2080_CTRL_GR_INFO swizzleAlignment;
    LW2080_CTRL_GR_INFO vertexCacheSize;
    LW2080_CTRL_GR_INFO vpeCount;
    LW2080_CTRL_GR_INFO shaderPipeCount;
    LW2080_CTRL_GR_INFO threadStackScalingFactor;
    LW2080_CTRL_GR_INFO shaderPipeSubCount;
    LW2080_CTRL_GR_INFO smRegBankCount;
    LW2080_CTRL_GR_INFO smRegBankRegCount;
    LW2080_CTRL_GR_INFO smVersion;
    LW2080_CTRL_GR_INFO maxWarpsPerSm;
    LW2080_CTRL_GR_INFO maxThreadsPerWarp;
    LW2080_CTRL_GR_INFO geomGsObufEntries;
    LW2080_CTRL_GR_INFO geomXbufEntries;
    LW2080_CTRL_GR_INFO fbMemReqGranularity;
    LW2080_CTRL_GR_INFO hostMemReqGranularity;
    LW2080_CTRL_GR_INFO litterNumFbps;
    LW2080_CTRL_GR_INFO litterNumGpcs;
    LW2080_CTRL_GR_INFO litterNumTpcPerGpc;
    LW2080_CTRL_GR_INFO litterNumZlwllBanks;
    LW2080_CTRL_GR_INFO litterNumPesPerGpc;
    LW2080_CTRL_GR_INFO timesliceEnabled;
    LW2080_CTRL_GR_INFO numGpuCores;
} MYGRINFO;

class Ctrl2080GrTest: public RmTest
{
public:
    Ctrl2080GrTest();
    virtual ~Ctrl2080GrTest();

    virtual string     IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();

private:
    MYGRINFO grInfo;
    LW2080_CTRL_GR_GET_INFO_PARAMS grInfoParams;
};

//! \brief Ctrl2080GrTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
Ctrl2080GrTest::Ctrl2080GrTest()
{
    SetName("Ctrl2080GrTest");
    memset(&grInfo, 0, sizeof(grInfo));
    memset(&grInfoParams, 0, sizeof(grInfoParams));
}

//! \brief Ctrl2080GrTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
Ctrl2080GrTest::~Ctrl2080GrTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return True
//-----------------------------------------------------------------------------
string Ctrl2080GrTest::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsSupported
//-----------------------------------------------------------------------------
RC Ctrl2080GrTest::Setup()
{
    RC rc;

    return rc;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC Ctrl2080GrTest::Run()
{
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    RC rc = OK;

    // get general gr info
    grInfo.bufferAlignment.index          = LW2080_CTRL_GR_INFO_INDEX_BUFFER_ALIGNMENT;
    grInfo.swizzleAlignment.index         = LW2080_CTRL_GR_INFO_INDEX_SWIZZLE_ALIGNMENT;
    grInfo.vertexCacheSize.index          = LW2080_CTRL_GR_INFO_INDEX_VERTEX_CACHE_SIZE;
    grInfo.vpeCount.index                 = LW2080_CTRL_GR_INFO_INDEX_VPE_COUNT;
    grInfo.shaderPipeCount.index          = LW2080_CTRL_GR_INFO_INDEX_SHADER_PIPE_COUNT;
    grInfo.threadStackScalingFactor.index = LW2080_CTRL_GR_INFO_INDEX_THREAD_STACK_SCALING_FACTOR;
    grInfo.shaderPipeSubCount.index       = LW2080_CTRL_GR_INFO_INDEX_SHADER_PIPE_SUB_COUNT;
    grInfo.smRegBankCount.index           = LW2080_CTRL_GR_INFO_INDEX_SM_REG_BANK_COUNT;
    grInfo.smRegBankRegCount.index        = LW2080_CTRL_GR_INFO_INDEX_SM_REG_BANK_REG_COUNT;
    grInfo.smVersion.index                = LW2080_CTRL_GR_INFO_INDEX_SM_VERSION;
    grInfo.maxWarpsPerSm.index            = LW2080_CTRL_GR_INFO_INDEX_MAX_WARPS_PER_SM;
    grInfo.maxThreadsPerWarp.index        = LW2080_CTRL_GR_INFO_INDEX_MAX_THREADS_PER_WARP;
    grInfo.geomGsObufEntries.index        = LW2080_CTRL_GR_INFO_INDEX_GEOM_GS_OBUF_ENTRIES;
    grInfo.geomXbufEntries.index          = LW2080_CTRL_GR_INFO_INDEX_GEOM_XBUF_ENTRIES;
    grInfo.fbMemReqGranularity.index      = LW2080_CTRL_GR_INFO_INDEX_FB_MEMORY_REQUEST_GRANULARITY;
    grInfo.hostMemReqGranularity.index    = LW2080_CTRL_GR_INFO_INDEX_HOST_MEMORY_REQUEST_GRANULARITY;
    grInfo.litterNumGpcs.index            = LW2080_CTRL_GR_INFO_INDEX_LITTER_NUM_GPCS;
    grInfo.litterNumFbps.index            = LW2080_CTRL_GR_INFO_INDEX_LITTER_NUM_FBPS;
    grInfo.litterNumZlwllBanks.index      = LW2080_CTRL_GR_INFO_INDEX_LITTER_NUM_ZLWLL_BANKS;
    grInfo.litterNumTpcPerGpc.index       = LW2080_CTRL_GR_INFO_INDEX_LITTER_NUM_TPC_PER_GPC;
    grInfo.litterNumPesPerGpc.index       = LW2080_CTRL_GR_INFO_INDEX_LITTER_NUM_PES_PER_GPC;
    grInfo.timesliceEnabled.index         = LW2080_CTRL_GR_INFO_INDEX_TIMESLICE_ENABLED;
    grInfo.numGpuCores.index              = LW2080_CTRL_GR_INFO_INDEX_GPU_CORE_COUNT;

    grInfoParams.grInfoListSize = sizeof (grInfo) / sizeof (LW2080_CTRL_GR_INFO);
    grInfoParams.grInfoList = (LwP64)&grInfo;

    CHECK_RC(pLwRm->Control(hSubdevice,
                            LW2080_CTRL_CMD_GR_GET_INFO,
                            (void*)&grInfoParams,
                            sizeof(grInfoParams)));

    Printf(Tee::PriHigh, "Ctrl2080GrTest: GR INFO - bufferAlignment            : 0x%x\n", (unsigned int)grInfo.bufferAlignment.data);
    Printf(Tee::PriHigh, "Ctrl2080GrTest: GR INFO - swizzleAlignment           : 0x%x\n", (unsigned int)grInfo.swizzleAlignment.data);
    Printf(Tee::PriHigh, "Ctrl2080GrTest: GR INFO - vertexCacheSize            : 0x%x\n", (unsigned int)grInfo.vertexCacheSize.data);
    Printf(Tee::PriHigh, "Ctrl2080GrTest: GR INFO - vpeCount                   : 0x%x\n", (unsigned int)grInfo.vpeCount.data);
    Printf(Tee::PriHigh, "Ctrl2080GrTest: GR INFO - shaderPipeCount            : 0x%x\n", (unsigned int)grInfo.shaderPipeCount.data);
    Printf(Tee::PriHigh, "Ctrl2080GrTest: GR INFO - threadStackScalingFactor   : 0x%x\n", (unsigned int)grInfo.threadStackScalingFactor.data);
    Printf(Tee::PriHigh, "Ctrl2080GrTest: GR INFO - shaderPipeSubCount         : 0x%x\n", (unsigned int)grInfo.shaderPipeSubCount.data);
    Printf(Tee::PriHigh, "Ctrl2080GrTest: GR INFO - smRegBankCount             : 0x%x\n", (unsigned int)grInfo.smRegBankCount.data);
    Printf(Tee::PriHigh, "Ctrl2080GrTest: GR INFO - smRegBankRegCount          : 0x%x\n", (unsigned int)grInfo.smRegBankRegCount.data);
    Printf(Tee::PriHigh, "Ctrl2080GrTest: GR INFO - smVersion                  : 0x%x\n", (unsigned int)grInfo.smVersion.data);
    Printf(Tee::PriHigh, "Ctrl2080GrTest: GR INFO - maxWarpsPerSm              : 0x%x\n", (unsigned int)grInfo.maxWarpsPerSm.data);
    Printf(Tee::PriHigh, "Ctrl2080GrTest: GR INFO - maxThreadsPerWarp          : 0x%x\n", (unsigned int)grInfo.maxThreadsPerWarp.data);
    Printf(Tee::PriHigh, "Ctrl2080GrTest: GR INFO - geomGsObufEntries          : 0x%x\n", (unsigned int)grInfo.geomGsObufEntries.data);
    Printf(Tee::PriHigh, "Ctrl2080GrTest: GR INFO - geomXBufEntries            : 0x%x\n", (unsigned int)grInfo.geomXbufEntries.data);
    Printf(Tee::PriHigh, "Ctrl2080GrTest: GR INFO - fbMemRequestGranularity    : 0x%x\n", (unsigned int)grInfo.fbMemReqGranularity.data);
    Printf(Tee::PriHigh, "Ctrl2080GrTest: GR INFO - hostMemRequestGranularity  : 0x%x\n", (unsigned int)grInfo.hostMemReqGranularity.data);
    Printf(Tee::PriHigh, "Ctrl2080GrTest: GR INFO - litterNumGpcs              : 0x%x\n", (unsigned int)grInfo.litterNumGpcs.data);
    Printf(Tee::PriHigh, "Ctrl2080GrTest: GR INFO - litterNumFbps              : 0x%x\n", (unsigned int)grInfo.litterNumFbps.data);
    Printf(Tee::PriHigh, "Ctrl2080GrTest: GR INFO - litterNumZlwllBanks        : 0x%x\n", (unsigned int)grInfo.litterNumZlwllBanks.data);
    Printf(Tee::PriHigh, "Ctrl2080GrTest: GR INFO - litterNumTpcPerGpc         : 0x%x\n", (unsigned int)grInfo.litterNumTpcPerGpc.data);
    Printf(Tee::PriHigh, "Ctrl2080GrTest: GR INFO - litterNumPesPerGpc         : 0x%x\n", (unsigned int)grInfo.litterNumPesPerGpc.data);
    Printf(Tee::PriHigh, "Ctrl2080GrTest: GR INFO - timesliceEnabled           : 0x%x\n", (unsigned int)grInfo.timesliceEnabled.data);
    Printf(Tee::PriHigh, "Ctrl2080GrTest: GR INFO - numGpuCores                : 0x%x\n", (unsigned int)grInfo.numGpuCores.data);

    LW2080_CTRL_GR_GET_ZLWLL_INFO_PARAMS grZlwllParams = {0};

    rc = pLwRm->Control(hSubdevice,
                        LW2080_CTRL_CMD_GR_GET_ZLWLL_INFO,
                        (void*)&grZlwllParams,
                        sizeof(grZlwllParams));
    if ( OK == rc )
    {
        Printf(Tee::PriHigh, "Ctrl2080GrTest: GR ZLWLL INFO - widthAlignPixels           : 0x%x\n", (unsigned int)grZlwllParams.widthAlignPixels);
        Printf(Tee::PriHigh, "Ctrl2080GrTest: GR ZLWLL INFO - heightAlignPixels          : 0x%x\n", (unsigned int)grZlwllParams.heightAlignPixels);
        Printf(Tee::PriHigh, "Ctrl2080GrTest: GR ZLWLL INFO - pixelSquaresByAliquots     : 0x%x\n", (unsigned int)grZlwllParams.pixelSquaresByAliquots);
        Printf(Tee::PriHigh, "Ctrl2080GrTest: GR ZLWLL INFO - aliquotTotal               : 0x%x\n", (unsigned int)grZlwllParams.aliquotTotal);
        Printf(Tee::PriHigh, "Ctrl2080GrTest: GR ZLWLL INFO - zlwllRegionByteMultiplier  : 0x%x\n", (unsigned int)grZlwllParams.zlwllRegionByteMultiplier);
        Printf(Tee::PriHigh, "Ctrl2080GrTest: GR ZLWLL INFO - zlwllRegionHeaderSize      : 0x%x\n", (unsigned int)grZlwllParams.zlwllRegionHeaderSize);
        Printf(Tee::PriHigh, "Ctrl2080GrTest: GR ZLWLL INFO - subregionCount             : 0x%x\n", (unsigned int)grZlwllParams.subregionCount);
        Printf(Tee::PriHigh, "Ctrl2080GrTest: GR ZLWLL INFO - zlwllSubregionHeaderSize   : 0x%x\n", (unsigned int)grZlwllParams.zlwllSubregionHeaderSize);
        Printf(Tee::PriHigh, "Ctrl2080GrTest: GR ZLWLL INFO - subregionWidthAlignPixels  : 0x%x\n", (unsigned int)grZlwllParams.subregionWidthAlignPixels);
        Printf(Tee::PriHigh, "Ctrl2080GrTest: GR ZLWLL INFO - subregionHeightAlignPixels : 0x%x\n", (unsigned int)grZlwllParams.subregionHeightAlignPixels);
    }
    else if ( RC::LWRM_NOT_SUPPORTED == rc )
    {
        rc = OK; // not supported is OK, just don't print out details from call
    }

    LW2080_CTRL_GR_GET_ROP_INFO_PARAMS grROPInfoParams = {0};

    rc = pLwRm->Control(hSubdevice,
                        LW2080_CTRL_CMD_GR_GET_ROP_INFO,
                        (void*)&grROPInfoParams,
                        sizeof(grROPInfoParams));
    if ( OK == rc )
    {
        Printf(Tee::PriHigh, "Ctrl2080GrTest: GR ROP INFO - ropUnitCount        : %d\n", (unsigned int)grROPInfoParams.ropUnitCount);
        Printf(Tee::PriHigh, "Ctrl2080GrTest: GR ROP INFO - ropOperationsFactor : %d\n", (unsigned int)grROPInfoParams.ropOperationsFactor);
        Printf(Tee::PriHigh, "Ctrl2080GrTest: GR ROP INFO - ropOperationsCount  : %d\n", (unsigned int)grROPInfoParams.ropOperationsCount);
        MASSERT((grROPInfoParams.ropUnitCount * grROPInfoParams.ropOperationsFactor) == grROPInfoParams.ropOperationsCount);
    }
    else if ( RC::LWRM_NOT_SUPPORTED == rc )
    {
        rc = OK; // not supported is OK, just don't print out details from call
    }

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC Ctrl2080GrTest::Cleanup()
{
    return OK;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Ctrl2080GrTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(Ctrl2080GrTest, RmTest,
                 "Ctrl2080GrTest RM test.");
