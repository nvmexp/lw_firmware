/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_ctag.cpp
//! \brief A sanity test for the allocation of compression tags
//!
//! This test is targeting the straddling compression tag issues we are having on GP10X, but it will
//! also work on older gpus that support compression.
//! NOTE: In order to get the correct test result, please set registry LW_REG_STR_STRADDLING_CTAG_SUPPORT
//! to ENABLE.

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl0080.h"
#include "gpu/include/gpudev.h"
#include <string>
#include "core/include/memcheck.h"    //must be included last for mem leak tracing at DEBUG_TRACE_LEVEL 1

#define TEST_ALLOC_SIZE_MULTIPLIER 3

class CtagTest : public RmTest
{
public:
    CtagTest();
    virtual ~CtagTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    LwBool m_straddling;
    UINT32 m_comptagsPerCacheLine;
    UINT32 m_maxCompbitLine;
    UINT32 m_numCompCacheLines;
    UINT32 m_cacheLineSize;
    UINT32 m_gobsPerComptagPerSlice;
    UINT32 m_cacheLineSizePerSlice;
    UINT32 *m_hwResIdRecord;
    UINT32 m_recordCount;

    RC AllocAndTestCacheLine(LwBool freeCtag);
    RC AllocAndTestSize(LwU32 size, LwBool freeCtag);
    RC AllocAndTestCombo(LwU32 size);
    RC AllocAllCacheLine();
    LwBool IsStraddling(UINT32 ctag);
};

//! \brief Constructor for CtagTest
//!
//! \sa Setup
//------------------------------------------------------------------------------
CtagTest::CtagTest()
{
    SetName("CtagTest");
}

//! \brief Destructor for CtagTest
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
CtagTest::~CtagTest()
{
}

//! \brief This Test Test is supported only on Fermi and Above
//!
//! \return RUN_RMTEST_TRUE if the test can be run in the current environment,
//!         skip reason otherwise
//------------------------------------------------------------------------------
string CtagTest::IsTestSupported()
{
    UINT32  gpuFamily;

    gpuFamily = (GetBoundGpuDevice())->GetFamily();

    if (gpuFamily >= GpuDevice::Fermi)
        return RUN_RMTEST_TRUE;

    return "Test is supported only on Fermi and Above.";
}

//! \brief Get the information needed for the compression tag storage
//
//------------------------------------------------------------------------------
RC CtagTest::Setup()
{
    RC                                           rc;
    LwRmPtr                                      pLwRm;
    LW0080_CTRL_FB_GET_COMPBIT_STORE_INFO_PARAMS Info = {0};
    UINT32                                       gpuFamily;

    CHECK_RC(InitFromJs());

    gpuFamily = (GetBoundGpuDevice())->GetFamily();

    CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                            LW0080_CTRL_CMD_FB_GET_COMPBIT_STORE_INFO,
                            &Info,
                            sizeof(Info) ) );

    m_comptagsPerCacheLine = Info.comptagsPerCacheLine;
    m_maxCompbitLine = Info.MaxCompbitLine;
    m_numCompCacheLines = m_maxCompbitLine / m_comptagsPerCacheLine;
    m_cacheLineSize = Info.cacheLineSize;
    m_gobsPerComptagPerSlice = Info.gobsPerComptagPerSlice;
    m_cacheLineSizePerSlice = Info.cacheLineSizePerSlice;

    if (gpuFamily == GpuDevice::Pascal)
    {
        if(m_cacheLineSizePerSlice % m_gobsPerComptagPerSlice != 0)
            m_straddling = true;
        else
            m_straddling = false;
    }
    else
    {
        m_straddling = false;
    }

    m_hwResIdRecord = new UINT32[m_numCompCacheLines];
    if (m_hwResIdRecord == NULL)
    {
        rc = RC::LWRM_ERROR;
    }
    m_recordCount = 0;
    return rc;
}

//! \brief Runs a combination of sanity tests.
//!
//! \return RC::LWRM_ERROR if any of the test fail
//------------------------------------------------------------------------------
RC CtagTest::Run()
{
    RC rc;

    CHECK_RC(AllocAndTestCacheLine(true));
    CHECK_RC(AllocAndTestSize(m_comptagsPerCacheLine * TEST_ALLOC_SIZE_MULTIPLIER, true));
    CHECK_RC(AllocAndTestCombo(m_comptagsPerCacheLine * TEST_ALLOC_SIZE_MULTIPLIER));
    CHECK_RC(AllocAllCacheLine());

    return rc;
}

//! \brief Free all the resource stored in hwResIdRecord array.
//------------------------------------------------------------------------------
RC CtagTest::Cleanup()
{
    RC                                           rc;
    LwRmPtr                                      pLwRm;
    LwRm::Handle                                 hSubdevice =
                                                     pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    LW2080_CTRL_CMD_FB_FREE_TILE_PARAMS paramsFree = {0};
    for (UINT32 i = 0; i < m_recordCount; i++)
    {
        paramsFree.hwResId = m_hwResIdRecord[i];
        pLwRm->Control(hSubdevice,
                        LW2080_CTRL_CMD_FB_FREE_TILE,
                        &paramsFree,
                        sizeof (paramsFree));
    }

    delete []m_hwResIdRecord;
    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//! \brief test the entire cache line allocation
//! can set whether to cleanup after the test
//------------------------------------------------------------------------------
RC CtagTest::AllocAndTestCacheLine(LwBool freeCtag){
    RC                                           rc;
    LwRmPtr                                      pLwRm;
    LwRm::Handle                                 hSubdevice =
                                                     pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    LW2080_CTRL_CMD_FB_ALLOC_COMP_RESOURCE_PARAMS params = {0};

    //set up the arguments
    params.attr2 = FLD_SET_DRF(OS32, _ATTR2, _ALLOC_COMPCACHELINE_ALIGN, _ON,
                               params.attr2);
    params.attr = FLD_SET_DRF(OS32, _ATTR, _COMPR, _REQUIRED, params.attr);
    params.attr = FLD_SET_DRF(OS32, _ATTR, _COMPR_COVG, _DEFAULT, params.attr);
    params.ctagOffset = DRF_DEF(OS32, _ALLOC, _COMPTAG_OFFSET_USAGE, _DEFAULT);

    CHECK_RC(pLwRm->Control(hSubdevice,
                        LW2080_CTRL_CMD_FB_ALLOC_COMP_RESOURCE,
                        &params,
                        sizeof (params)));

    //test to see if the allocation result is legal.
    if (m_straddling)
    {
        if (params.retCompTagLineMax != m_maxCompbitLine - 1)
        {
            if (params.retCompTagLineMax - params.retCompTagLineMin + 1 == m_comptagsPerCacheLine - 1
                || params.retCompTagLineMax - params.retCompTagLineMin + 1 == m_comptagsPerCacheLine)
            {
                if((!IsStraddling(params.retCompTagLineMax + 1) && !IsStraddling(params.retCompTagLineMin - 1))
                    || IsStraddling(params.retCompTagLineMax)
                    || IsStraddling(params.retCompTagLineMin))
                {
                    Printf(Tee::PriNormal,"The result Ctag Line Num is %d, %d\n",
                           params.retCompTagLineMin ,params.retCompTagLineMax );
                    rc = RC::LWRM_ERROR;
                }
            }
            else
            {
                Printf(Tee::PriNormal,"The result Ctag Line Num is %d, %d\n",
                       params.retCompTagLineMin ,params.retCompTagLineMax );
                rc = RC::LWRM_ERROR;
            }
        }
        else if (IsStraddling(params.retCompTagLineMax) || IsStraddling(params.retCompTagLineMin))
        {
            Printf(Tee::PriNormal,"The result Ctag Line Num is %d, %d\n",
                   params.retCompTagLineMin ,params.retCompTagLineMax );
            rc = RC::LWRM_ERROR;
        }
    }
    else{
        if (params.retCompTagLineMax - params.retCompTagLineMin + 1 == m_comptagsPerCacheLine
            && params.retCompTagLineMin % m_comptagsPerCacheLine == 0)
        {
        }
        else if (params.retCompTagLineMin == 1 &&
            params.retCompTagLineMax - params.retCompTagLineMin + 1 == m_comptagsPerCacheLine - 1)
        {
        }
        else
        {
            Printf(Tee::PriNormal,"The result Ctag Line Num is %d, %d\n",
                   params.retCompTagLineMin ,params.retCompTagLineMax );
            rc = RC::LWRM_ERROR;
        }
    }
    if (freeCtag)
    {
        LW2080_CTRL_CMD_FB_FREE_TILE_PARAMS paramsFree = {0};
        paramsFree.hwResId = params.hwResId;
        pLwRm->Control(hSubdevice,
                        LW2080_CTRL_CMD_FB_FREE_TILE,
                        &paramsFree,
                        sizeof (paramsFree));
    }
    else
    {
        m_hwResIdRecord[m_recordCount] = params.hwResId;
        m_recordCount++;
    }
    return rc;
}

//! \brief test allocation based on user specified size
//------------------------------------------------------------------------------
RC CtagTest::AllocAndTestSize(LwU32 NumofPages, LwBool freeCtag)
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());

    LW2080_CTRL_CMD_FB_ALLOC_COMP_RESOURCE_PARAMS params = {0};

    //set up the arguments
    params.attr2 = FLD_SET_DRF(OS32, _ATTR2, _ALLOC_COMPCACHELINE_ALIGN, _OFF,
                               params.attr2);
    params.attr = FLD_SET_DRF(OS32, _ATTR, _COMPR, _REQUIRED, params.attr);
    params.attr = FLD_SET_DRF(OS32, _ATTR, _COMPR_COVG, _DEFAULT, params.attr);
    params.ctagOffset = DRF_DEF(OS32, _ALLOC, _COMPTAG_OFFSET_USAGE, _DEFAULT);
    params.attr = FLD_SET_DRF(OS32, _ATTR, _PAGE_SIZE, _4KB, params.attr);
    params.size = NumofPages;

    CHECK_RC(pLwRm->Control(hSubdevice,
                        LW2080_CTRL_CMD_FB_ALLOC_COMP_RESOURCE,
                        &params,
                        sizeof (params)));
    //test to see if the allocation result is legal.
    if (params.retCompTagLineMax - params.retCompTagLineMin + 1 != NumofPages)
    {
        Printf(Tee::PriNormal,"The result Ctag Line Num is %d, %d\n",
          params.retCompTagLineMin ,params.retCompTagLineMax );
        rc = RC::LWRM_ERROR;
    }
    if (freeCtag)
    {
        LW2080_CTRL_CMD_FB_FREE_TILE_PARAMS paramsFree = {0};
        paramsFree.hwResId = params.hwResId;
        pLwRm->Control(hSubdevice,
                        LW2080_CTRL_CMD_FB_FREE_TILE,
                        &paramsFree,
                        sizeof (paramsFree));
    }
    else
    {
        m_hwResIdRecord[m_recordCount] = params.hwResId;
        m_recordCount++;
    }
    return rc;
}

//! \brief test allocation a cache line followed by the allocation specified by size
//------------------------------------------------------------------------------
RC CtagTest::AllocAndTestCombo(LwU32 NumofPages)
{
    RC                                           rc;
    LwRmPtr                                      pLwRm;
    LwRm::Handle                                 hSubdevice =
                                                     pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    LW2080_CTRL_CMD_FB_ALLOC_COMP_RESOURCE_PARAMS paramsFirst = {0};
    LW2080_CTRL_CMD_FB_ALLOC_COMP_RESOURCE_PARAMS paramsSecond = {0};

    //set up the arguments to allocate an entire cache line of ctag
    paramsFirst.attr2 = FLD_SET_DRF(OS32, _ATTR2, _ALLOC_COMPCACHELINE_ALIGN,
                                    _ON, paramsFirst.attr2);
    paramsFirst.attr = FLD_SET_DRF(OS32, _ATTR, _COMPR, _REQUIRED,
                                   paramsFirst.attr);
    paramsFirst.attr = FLD_SET_DRF(OS32, _ATTR, _COMPR_COVG, _DEFAULT,
                                   paramsFirst.attr);
    paramsFirst.ctagOffset = DRF_DEF(OS32, _ALLOC, _COMPTAG_OFFSET_USAGE, _DEFAULT);

    //set up the argument to allocate ctag based on a size
    paramsSecond.attr2 = FLD_SET_DRF(OS32, _ATTR2, _ALLOC_COMPCACHELINE_ALIGN,
                                     _OFF, paramsSecond.attr2);
    paramsSecond.attr = FLD_SET_DRF(OS32, _ATTR, _COMPR, _REQUIRED,
                                    paramsSecond.attr);
    paramsSecond.attr = FLD_SET_DRF(OS32, _ATTR, _COMPR_COVG, _DEFAULT,
                                    paramsSecond.attr);
    paramsSecond.ctagOffset = DRF_DEF(OS32, _ALLOC, _COMPTAG_OFFSET_USAGE, _DEFAULT);
    paramsSecond.attr = FLD_SET_DRF(OS32, _ATTR, _PAGE_SIZE, _4KB,
                                    paramsSecond.attr);
    paramsSecond.size = NumofPages;

    CHECK_RC(pLwRm->Control(hSubdevice,
                        LW2080_CTRL_CMD_FB_ALLOC_COMP_RESOURCE,
                        &paramsFirst,
                        sizeof (paramsFirst)));
    CHECK_RC(pLwRm->Control(hSubdevice,
                        LW2080_CTRL_CMD_FB_ALLOC_COMP_RESOURCE,
                        &paramsSecond,
                        sizeof (paramsSecond)));

    //check the result of the two allocations
    if (paramsFirst.retCompTagLineMax + 1 != paramsSecond.retCompTagLineMin)
    {
        rc = RC::LWRM_ERROR;
    }

    LW2080_CTRL_CMD_FB_FREE_TILE_PARAMS paramsFreeFirst = {0};
    LW2080_CTRL_CMD_FB_FREE_TILE_PARAMS paramsFreeSecond = {0};
    paramsFreeFirst.hwResId = paramsFirst.hwResId;
    paramsFreeSecond.hwResId = paramsSecond.hwResId;
    CHECK_RC(pLwRm->Control(hSubdevice,
                    LW2080_CTRL_CMD_FB_FREE_TILE,
                    &paramsFreeFirst,
                    sizeof (paramsFreeFirst)));
    CHECK_RC(pLwRm->Control(hSubdevice,
                    LW2080_CTRL_CMD_FB_FREE_TILE,
                    &paramsFreeSecond,
                    sizeof (paramsFreeSecond)));
    return  rc;
}

//! \brief test to allocate all the compression tag cache line
//------------------------------------------------------------------------------
RC CtagTest::AllocAllCacheLine()
{
    RC rc;

    if (m_straddling){
        for (unsigned int i = 0; i < m_numCompCacheLines; ++i)
        {
            rc = (AllocAndTestCacheLine(false));

        }
    }

    //
    //because older code will skip the first cache line
    //we will only test m_numCompCacheLines - 1 allocations
    //
    else{
        for (unsigned int i = 0; i < m_numCompCacheLines - 1; ++i)
        {
            rc = (AllocAndTestCacheLine(false));
        }
    }

    return rc;
}

//! \brief Help function to determine if a ctag is straddling
//------------------------------------------------------------------------------
LwBool CtagTest::IsStraddling(UINT32 ctag){
    LwU32 cacheLine1;
    LwU32 cacheLine2;

   // @ref dev_ltc.ref for GP10x
   cacheLine1 = ((ctag) * m_gobsPerComptagPerSlice) >> BIT_IDX_32(m_cacheLineSizePerSlice);
   cacheLine2 = (((ctag + 1) * m_gobsPerComptagPerSlice)- 1) >> BIT_IDX_32(m_cacheLineSizePerSlice);
   return cacheLine1 != cacheLine2;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(CtagTest, RmTest,
    "TODORMT - Write a good short description here");
