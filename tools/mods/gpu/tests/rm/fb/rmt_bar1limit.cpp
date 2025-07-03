/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2004-2009,2019 by LWPU Corporation.  All rights reserved.   All
* information contained herein  is proprietary and confidential to LWPU
* Corporation.   Any use, reproduction, or disclosure without the written
* permission of LWPU  Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file rmt_bar1mapmem.cpp
//! \brief Full Mapping of BAR1 functionality test
//!
//! This test basically tests the functionality of Full BAR 1 mapping of
//! video-heap memory. It is supported only on igt21a as of now.

#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "lwos.h"
#include "lwRmApi.h"
#include "ctrl/ctrl2080.h"
#include "class/cl8697.h" // GT21A_TESLA
#include "core/include/memcheck.h"

#define ALIGN_4K(x)        (x & (~((1<<12)-1)))
#define LSHIFT10(x)         (LwU64(x) << 10)
#define RSHIFT10(x)         (LwU64(x) >> 10)
#define RM_PAGE_SIZE         0x1000

class Bar1LimitTest: public RmTest
{
public:
    Bar1LimitTest();
    virtual ~Bar1LimitTest();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    //@{
    //! Test specific variables
    //
    LwRm::Handle m_hVidMem = 0;
    UINT64 m_bar1Size      = 0;
    UINT64 m_allocSize     = 0;
    UINT64 m_heapSize      = 0;
    UINT32 m_numMapPass    = 0;
    UINT32 m_numMapFail    = 0;
    UINT64 m_heapOffset    = 0;

    //@}
    //@{
    //! Test specific functions
    RC MapAndTest(LwRm::Handle hMem, UINT64 offset,
                  UINT64 m_memSize, UINT32 m_numMaps, bool shouldFail);
    //@}

};

//! \brief Constructor for Bar1LimitTest
//!
//! Just does nothing except setting a name for the test..the actual
//! functionality is done by Setup()
//!
//! \sa Setup
//-----------------------------------------------------------------------------
Bar1LimitTest::Bar1LimitTest()
{
    SetName("Bar1LimitTest");
}

//! \brief Bar1LimitTest destructor
//!
//! All resources should be freed in the Cleanup member function, so
//! destructors shouldn't do much.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
Bar1LimitTest::~Bar1LimitTest()
{
}

//! \brief IsTestSupported() is supported for all
//!
//! Checks whether the hardware classes are supported on the current elwiroment
//! or not
//-----------------------------------------------------------------------------
string Bar1LimitTest::IsTestSupported()
{
    if( IsClassSupported(GT21A_TESLA) )
        return RUN_RMTEST_TRUE;
    return "GT21A_TESLA class is not supported on current platform";
}

//! \brief Setup()
//!
//! This function basically allocates  video-heap memory.
//! \return RC -> OK if everything is allocated
//! \sa Run
//-----------------------------------------------------------------------------
RC Bar1LimitTest::Setup()
{
    LW2080_CTRL_FB_GET_INFO_PARAMS fbGetInfoParams = {0};
    LW2080_CTRL_FB_INFO ctrlBlock;
    LwRmPtr      pLwRm;
    RC           rc;
    UINT32       attr;
    UINT32       attr2;

    GpuSubdevice *pSubdevice = GetBoundGpuSubdevice();

    fbGetInfoParams.fbInfoListSize = 1;

    ctrlBlock.index = LW2080_CTRL_FB_INFO_INDEX_BAR1_SIZE;

    fbGetInfoParams.fbInfoList = (LwP64)&ctrlBlock;

    //
    // Find the BAR1 size
    //
    rc = pLwRm->ControlBySubdevice(pSubdevice,
                        LW2080_CTRL_CMD_FB_GET_INFO,
                        &fbGetInfoParams,
                        sizeof (LW2080_CTRL_FB_GET_INFO_PARAMS));

    if (OK != rc)
    {
        Printf(Tee::PriHigh," %d:failed to get FB size\n",
                           __LINE__);

        CHECK_RC_CLEANUP(rc);
    }

    Printf(Tee::PriLow," %d:Bar1LimitTest: Bar1 size %lld MB\n",
           __LINE__, RSHIFT10(ctrlBlock.data));

    m_bar1Size = LSHIFT10(ctrlBlock.data);

    //
    // Now find the Heap Size
    //
    ctrlBlock.data  = 0;
    ctrlBlock.index = LW2080_CTRL_FB_INFO_INDEX_HEAP_SIZE;

    rc = pLwRm->ControlBySubdevice(pSubdevice,
                        LW2080_CTRL_CMD_FB_GET_INFO,
                        &fbGetInfoParams,
                        sizeof (LW2080_CTRL_FB_GET_INFO_PARAMS));

    if (OK != rc)
    {
        Printf(Tee::PriHigh," %d:failed to get heap size\n",
                           __LINE__);
        CHECK_RC_CLEANUP(rc);
    }

    Printf(Tee::PriLow, "%d:Bar1LimitTest: Available Heap size %lld MB\n",
           __LINE__, RSHIFT10(ctrlBlock.data));

    m_heapSize = LSHIFT10(ctrlBlock.data);

    //
    // allocate memory of size which is a fraction
    // of the total heap size
    //

    m_allocSize =  ALIGN_4K((m_heapSize / 16));

    Printf(Tee::PriLow," %d:Bar1LimitTest: Allocating block of size %lld\n",
           __LINE__, m_allocSize);

    attr  = DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM);
    attr2 = LWOS32_ATTR2_NONE;

    CHECK_RC_CLEANUP(pLwRm->VidHeapAllocSize(LWOS32_TYPE_IMAGE,
        attr,
        attr2,
        m_allocSize,
        &m_hVidMem,
        &m_heapOffset,
        nullptr,
        nullptr,
        nullptr,
        GetBoundGpuDevice()));

Cleanup:
    return rc;
}

//! \brief This function runs the test
//!
//! This test maps and tests the memory allocated in the Setup().
//! First we map the allocate memory m_numMapPass times, so that
//! all the mappings are successful. Then we map it m_numMapFail
//! causing some mappings to fail at the boundary of BAR1 space.
//
//! \return RC -> OK if the test runs successfully, test-specific RC if test
//!               failed
//!
//! \sa MapAndTest
//-----------------------------------------------------------------------------
RC Bar1LimitTest::Run()
{
    RC rc;

    //
    // Callwlate the number of mappings with aggregate size less
    // than the total BAR1 size.
    //
    m_numMapPass = (UINT32)(m_bar1Size / m_allocSize) - 2;
    if (m_numMapPass % 2)
    {
        m_numMapPass -= 1;
    }
    CHECK_RC(MapAndTest(m_hVidMem, 0, m_allocSize, m_numMapPass, false));

    //
    // Now test with number of mapping with aggregate size greater
    // than the total BAR1 size. This will cause the MapMemory
    // to fail just after the mapped space exceeds the BAR1 size
    //

    m_numMapFail = (UINT32)(m_bar1Size / m_allocSize) + 2;
    if (m_numMapFail % 2)
    {
        m_numMapFail += 1;
    }
    CHECK_RC(MapAndTest(m_hVidMem, 0, m_allocSize, m_numMapFail, true));

    return rc;
}

//! \brief Cleanup()
//!
//! free the resources allocated
//!
//! \sa Setup
//-----------------------------------------------------------------------------
//
RC Bar1LimitTest::Cleanup()
{
    LwRmPtr pLwRm;
    pLwRm->Free(m_hVidMem);

    return OK;
}

//-----------------------------------------------------------------------------
//PRIVATE FUNCTIONS
//-----------------------------------------------------------------------------

//! \brief This function tests BAR1 limit by mapping the allocated space
//! m_numMaps times. It then writes into one set of mapped locations and reads
//! from the other set of mapped locations mapped to the same memory handle
//! to check the flow of data. The function will fail if the mapping exceeds
//! BAR1, so check the shouldFail flag before actually failing the function
//!
//! \return RC -> OK if the test runs successfully, test-specific RC if the test
//!               failed. The test will fail if the aggregate size of mapping
//!               exceeds the available Bar1 space.
//! \sa Run
//-----------------------------------------------------------------------------
RC Bar1LimitTest::MapAndTest(LwRm::Handle hMemory,
                             UINT64 offset,
                             UINT64 m_memSize,
                             UINT32 m_numMaps,
                             bool shouldFail
                            )
{

    LwRmPtr      pLwRm;
    RC           rc;
    UINT32       *ip;
    UINT32       i = 0;
    UINT32       numMaps;
    void         **pMapAddr;
    bool         *isMemMapped;

    pMapAddr    = new void *[m_numMaps];
    isMemMapped = new bool [m_numMaps];

    for (i = 0; i < m_numMaps; i++)
    {
        isMemMapped[i] = false;
    }

    for (i = 0; i < m_numMaps; i++)
    {
        Printf(Tee::PriLow,
           " %d:Bar1LimitTest: Mapping %d of %d\n",
           __LINE__, i+1, m_numMaps);

        CHECK_RC_CLEANUP(pLwRm->MapMemory(hMemory,
            offset,
            m_memSize,
            &pMapAddr[i],
            0,
            GetBoundGpuSubdevice()));

        isMemMapped[i] = true;
    }

    for (numMaps = 0; numMaps < m_numMaps; numMaps += 2)
    {
        //
        // Write unique values to the nth mapped address and verify
        // the n+1 th mapping corresponds to the same heap offset.
        //
        ip = (UINT32 *)pMapAddr[numMaps];
        for (i = 0; i < RM_PAGE_SIZE ; i++)
        {
            MEM_WR32(ip, i);
            ip++;
        }
        //
        // Call FlushCpuWriteCombineBuffer() which calls sfence instruction
        // This will cause all previous video memory operations to complete
        // before we go ahead and read back the same memory locations.
        //
        rc = Platform::FlushCpuWriteCombineBuffer();
        if (rc == RC::UNSUPPORTED_FUNCTION)
        {
            Printf(Tee::PriHigh, "Bar1LimitTest: MODS Returned Unsupported function\
                                  when Platform::FlushCpuWriteCombineBuffer() was\
                                  called..\n");
            CHECK_RC_CLEANUP(rc);
        }

        ip = (UINT32 *)pMapAddr[numMaps + 1];
        for (i = 0; i < RM_PAGE_SIZE; i++)
        {
            if (MEM_RD32(ip) != i)
            {
                rc = RC::LWRM_ERROR;
                Printf(Tee::PriHigh, "%d:Bar1LimitTest: Error: Incorrect mappings broken %s\n",
                    __LINE__,rc.Message());
                goto Cleanup;
            }
            ip++;
        }
    }

    // Unmaps the memory and free the resources
Cleanup:

    if ((rc == RC::LWRM_INSUFFICIENT_RESOURCES) && (shouldFail == true))
    {
        Printf(Tee::PriHigh, "%d:Bar1LimitTest: Test is supposed to fail with insufficient resources\
                               as the total size of mappings exceed the BAR1 size..\n", __LINE__);

        rc = 0;
    }

    //
    // Unmapped all the mapped memory.
    //
    for (i = 0; i < m_numMaps; i++)
    {
        if (isMemMapped[i] == true)
        {
           Printf(Tee::PriLow,
               " %d:Bar1LimitTest: UnMapping %d of %d\n",
               __LINE__, i+1, m_numMaps);

           pLwRm->UnmapMemory(hMemory, pMapAddr[i], 0, GetBoundGpuSubdevice());

        }
    }

    delete [] pMapAddr;
    delete [] isMemMapped;

    return rc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ Bar1LimitTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Bar1LimitTest, RmTest,
                 "RmMapMemory functionality test.");

