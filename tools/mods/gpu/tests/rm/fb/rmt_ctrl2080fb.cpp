/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011,2015-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_ctrl2080fb.cpp
//! \brief LW2080_CTRL_CMD FB tests
//!
#include "ctrl/ctrl2080.h" // LW20_SUBDEVICE_XX
#include "gpu/tests/rmtest.h"
#include "core/include/platform.h"
#include "core/include/memcheck.h"

class Ctrl2080FbTest: public RmTest
{
public:
    Ctrl2080FbTest();
    virtual ~Ctrl2080FbTest();

    virtual string     IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();

private:
    struct fbInfo
    {
        LW2080_CTRL_FB_INFO tileRegionCnt;
        LW2080_CTRL_FB_INFO tileRegionFreeCnt;
        LW2080_CTRL_FB_INFO compressionSize;
        LW2080_CTRL_FB_INFO dramPageStride;
        LW2080_CTRL_FB_INFO ramSize;
        LW2080_CTRL_FB_INFO totalRamSize;
        LW2080_CTRL_FB_INFO heapSize;
        LW2080_CTRL_FB_INFO mappableHeapSize;
        LW2080_CTRL_FB_INFO ramCfg;
        LW2080_CTRL_FB_INFO ramType;
        LW2080_CTRL_FB_INFO memoryInfoVendorID;
        LW2080_CTRL_FB_INFO bankCount;
        LW2080_CTRL_FB_INFO busWidth;
        LW2080_CTRL_FB_INFO vaddrSpaceSize;
        LW2080_CTRL_FB_INFO vaddrHeapSize;
        LW2080_CTRL_FB_INFO vaddrMappbleSize;
        LW2080_CTRL_FB_INFO fbpMask;
        LW2080_CTRL_FB_INFO fbpCount;
        LW2080_CTRL_FB_INFO ltcCount;
        LW2080_CTRL_FB_INFO ltsCount;
        LW2080_CTRL_FB_INFO mode;
    } fbInfo;

    LW2080_CTRL_FB_GET_INFO_PARAMS fbInfoParams;

    LW2080_CTRL_FB_GET_LTC_INFO_FOR_FBP_PARAMS fbLTCInfoForFBPParams;
};

//! \brief Ctrl2080FbTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
Ctrl2080FbTest::Ctrl2080FbTest()
{
    SetName("Ctrl2080FbTest");
    memset(&fbInfo, 0, sizeof(fbInfo));
    memset(&fbInfoParams, 0, sizeof(fbInfoParams));
    memset(&fbLTCInfoForFBPParams, 0, sizeof(fbLTCInfoForFBPParams));
}

//! \brief Ctrl2080FbTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
Ctrl2080FbTest::~Ctrl2080FbTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return True
//-----------------------------------------------------------------------------
string Ctrl2080FbTest::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsTestSupported
//-----------------------------------------------------------------------------
RC Ctrl2080FbTest::Setup()
{
    RC rc;

    return rc;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC Ctrl2080FbTest::Run()
{
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    UINT32 activeLTCCount;
    UINT32 activeFBPMask;
    UINT08 i;
    RC rc;

    fbInfo.tileRegionCnt.index     = LW2080_CTRL_FB_INFO_INDEX_TILE_REGION_COUNT;
    fbInfo.tileRegionFreeCnt.index = LW2080_CTRL_FB_INFO_INDEX_TILE_REGION_FREE_COUNT;
    fbInfo.compressionSize.index   = LW2080_CTRL_FB_INFO_INDEX_COMPRESSION_SIZE;
    fbInfo.dramPageStride.index    = LW2080_CTRL_FB_INFO_INDEX_DRAM_PAGE_STRIDE;
    fbInfo.ramSize.index           = LW2080_CTRL_FB_INFO_INDEX_RAM_SIZE;
    fbInfo.totalRamSize.index      = LW2080_CTRL_FB_INFO_INDEX_TOTAL_RAM_SIZE;
    fbInfo.heapSize.index          = LW2080_CTRL_FB_INFO_INDEX_HEAP_SIZE;
    fbInfo.mappableHeapSize.index  = LW2080_CTRL_FB_INFO_INDEX_MAPPABLE_HEAP_SIZE;
    fbInfo.ramCfg.index            = LW2080_CTRL_FB_INFO_INDEX_RAM_CFG;
    fbInfo.ramType.index           = LW2080_CTRL_FB_INFO_INDEX_RAM_TYPE;
    fbInfo.memoryInfoVendorID.index = LW2080_CTRL_FB_INFO_INDEX_MEMORYINFO_VENDOR_ID;
    fbInfo.bankCount.index         = LW2080_CTRL_FB_INFO_INDEX_BANK_COUNT;
    fbInfo.busWidth.index          = LW2080_CTRL_FB_INFO_INDEX_BUS_WIDTH;
    fbInfo.vaddrSpaceSize.index    = LW2080_CTRL_FB_INFO_INDEX_GPU_VADDR_SPACE_SIZE_KB;
    fbInfo.vaddrHeapSize.index     = LW2080_CTRL_FB_INFO_INDEX_GPU_VADDR_HEAP_SIZE_KB;
    fbInfo.vaddrMappbleSize.index  = LW2080_CTRL_FB_INFO_INDEX_GPU_VADDR_MAPPBLE_SIZE_KB;
    fbInfo.fbpCount.index          = LW2080_CTRL_FB_INFO_INDEX_FBP_COUNT;
    fbInfo.fbpMask.index           = LW2080_CTRL_FB_INFO_INDEX_FBP_MASK;
    fbInfo.ltcCount.index          = LW2080_CTRL_FB_INFO_INDEX_LTC_COUNT;
    fbInfo.ltsCount.index          = LW2080_CTRL_FB_INFO_INDEX_LTS_COUNT;
    fbInfo.mode.index              = LW2080_CTRL_FB_INFO_INDEX_PSEUDO_CHANNEL_MODE;

    fbInfoParams.fbInfoListSize = sizeof (fbInfo) / sizeof (LW2080_CTRL_FB_INFO);
    fbInfoParams.fbInfoList = LW_PTR_TO_LwP64(&fbInfo);

    CHECK_RC(pLwRm->Control(hSubdevice,
                            LW2080_CTRL_CMD_FB_GET_INFO,
                            (void*)&fbInfoParams,
                            sizeof(fbInfoParams)));

    Printf(Tee::PriNormal,
           "Ctrl2080FbTest: FB_GET_INFO - tileRegionCnt     : 0x%x\n",
           (unsigned int)fbInfo.tileRegionCnt.data);
    Printf(Tee::PriNormal,
           "Ctrl2080FbTest: FB_GET_INFO - tileRegionFreeCnt : 0x%x\n",
           (unsigned int)fbInfo.tileRegionFreeCnt.data);
    Printf(Tee::PriNormal,
           "Ctrl2080FbTest: FB_GET_INFO - compressionSize   : 0x%x\n",
           (unsigned int)fbInfo.compressionSize.data);
    Printf(Tee::PriNormal,
           "Ctrl2080FbTest: FB_GET_INFO - dramPageStride    : 0x%x\n",
           (unsigned int)fbInfo.dramPageStride.data);
    Printf(Tee::PriNormal,
           "Ctrl2080FbTest: FB_GET_INFO - fbRamSize         : 0x%x\n",
           (unsigned int)fbInfo.ramSize.data);
    Printf(Tee::PriNormal,
           "Ctrl2080FbTest: FB_GET_INFO - totalRamSize      : 0x%x\n",
           (unsigned int)fbInfo.totalRamSize.data);
    Printf(Tee::PriNormal,
           "Ctrl2080FbTest: FB_GET_INFO - heapSize          : 0x%x\n",
           (unsigned int)fbInfo.heapSize.data);
    Printf(Tee::PriNormal,
           "Ctrl2080FbTest: FB_GET_INFO - mappableHeapSize  : 0x%x\n",
           (unsigned int)fbInfo.mappableHeapSize.data);
    Printf(Tee::PriNormal,
           "Ctrl2080FbTest: FB_GET_INFO - ramCfg            : 0x%x\n",
           (unsigned int)fbInfo.ramCfg.data);
    Printf(Tee::PriNormal,
           "Ctrl2080FbTest: FB_GET_INFO - ramType           : 0x%x\n",
           (unsigned int)fbInfo.ramType.data);
    Printf(Tee::PriNormal,
           "Ctrl2080FbTest: FB_GET_INFO - memoryInfoVendorID: 0x%x\n",
           (unsigned int)fbInfo.memoryInfoVendorID.data);
    Printf(Tee::PriNormal,
           "Ctrl2080FbTest: FB_GET_INFO - bankCount         : 0x%x\n",
           (unsigned int)fbInfo.bankCount.data);
    Printf(Tee::PriNormal,
           "Ctrl2080FbTest: FB_GET_INFO - busWidth          : 0x%x\n",
           (unsigned int)fbInfo.busWidth.data);
    Printf(Tee::PriNormal,
           "Ctrl2080FbTest: FB_GET_INFO - vaddrSpaceSize    : 0x%x KB\n",
           (unsigned int)fbInfo.vaddrSpaceSize.data);
    Printf(Tee::PriNormal,
           "Ctrl2080FbTest: FB_GET_INFO - vaddrHeapSize     : 0x%x KB\n",
           (unsigned int)fbInfo.vaddrHeapSize.data);
    Printf(Tee::PriNormal,
           "Ctrl2080FbTest: FB_GET_INFO - vaddrMappbleSize  : 0x%x KB\n",
           (unsigned int)fbInfo.vaddrMappbleSize.data);
    Printf(Tee::PriNormal,
           "Ctrl2080FbTest: FB_GET_INFO - fbpMask           : 0x%x\n",
           (unsigned int)fbInfo.fbpMask.data);
    Printf(Tee::PriNormal,
           "Ctrl2080FbTest: FB_GET_INFO - fbpCount          : %d\n",
           (unsigned int)fbInfo.fbpCount.data);
    Printf(Tee::PriNormal,
           "Ctrl2080FbTest: FB_GET_INFO - ltcCount          : %d\n",
           (unsigned int)fbInfo.ltcCount.data);
    Printf(Tee::PriNormal,
           "Ctrl2080FbTest: FB_GET_INFO - ltsCount          : %d\n",
           (unsigned int)fbInfo.ltsCount.data);
    Printf(Tee::PriNormal,
           "Ctrl2080FbTest: FB_GET_INFO - mode              : %d\n",
           (unsigned int)fbInfo.mode.data);

    Printf(Tee::PriNormal, "\n");

    activeLTCCount = 0;
    activeFBPMask  = fbInfo.fbpMask.data;
    for (i = 0; ((UINT32)BIT(i)) <= activeFBPMask; i++)
    {
        //
        // Skip over floorswept FBPs.
        //
        // Floorswept FBPs should have all their LTCs floorswept.
        //
        if (!(activeFBPMask & BIT(i)))
        {
            continue;
        }

        fbLTCInfoForFBPParams.fbpIndex = i;
        CHECK_RC(pLwRm->Control(hSubdevice,
                                LW2080_CTRL_CMD_FB_GET_LTC_INFO_FOR_FBP,
                                (void*)&fbLTCInfoForFBPParams,
                                sizeof(fbLTCInfoForFBPParams)));

        Printf(Tee::PriNormal,
               "Ctrl2080FbTest: FB_GET_LTC_INFO_FOR_FBP - FBP Index : 0x%x\n",
               (unsigned int)fbLTCInfoForFBPParams.fbpIndex);
        Printf(Tee::PriNormal,
               "Ctrl2080FbTest: FB_GET_LTC_INFO_FOR_FBP - LTC Mask  : 0x%x\n",
               (unsigned int)fbLTCInfoForFBPParams.ltcMask);
        Printf(Tee::PriNormal,
               "Ctrl2080FbTest: FB_GET_LTC_INFO_FOR_FBP - LTC Count : 0x%x\n\n",
               (unsigned int)fbLTCInfoForFBPParams.ltcCount);
        activeLTCCount += fbLTCInfoForFBPParams.ltcCount;
    }
    MASSERT(activeLTCCount == fbInfo.ltcCount.data);

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC Ctrl2080FbTest::Cleanup()
{
    return OK;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Ctrl2080FbTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(Ctrl2080FbTest, RmTest,
                 "Ctrl2080FbTest RM test.");
