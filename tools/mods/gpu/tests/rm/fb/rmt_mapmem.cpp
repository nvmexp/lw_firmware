/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2004-2013,2019 by LWPU Corporation.  All rights reserved.   All
* information contained herein  is proprietary and confidential to LWPU
* Corporation.   Any use, reproduction, or disclosure without the written
* permission of LWPU  Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file rmt_mapmem.cpp
//! \brief RmMapMemory functionality test
//!
//! This test basically tests the functionality of mapping memory of class
//! "SYSTEM MEMORY", "LOCAL USER" and also video-heap and tiled video heap
//! memory. The flags WRTE_ONLY, READ_WRITE, READ_ONLY flags respectively
//! are used for mapping

#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "lwos.h"
#include "class/cl0040.h" // LW01_MEMORY_LOCAL_USER
#include "class/cl003e.h" // LW01_MEMORY_SYSTEM
#include "core/include/memcheck.h"

#define RM_PAGE_SIZE    0x1000
#define PITCH           0x1024

class MapMemoryTest: public RmTest
{
public:
    MapMemoryTest();
    virtual ~MapMemoryTest();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    //@{
    //! Test specific variables
    Surface2D *m_hSysmem                = nullptr;
    Surface2D *m_hLocalmem              = nullptr;
    Surface2D *m_hVidmem                = nullptr;
    Surface2D *m_hVidmemTiledOrNonTiled = nullptr;
    UINT32 m_height      = 0;
    UINT32 m_pitch       = 0;
    UINT64 m_sys_limit   = 0;
    UINT64 m_local_limit = 0;
    UINT32 m_vid_limit   = 0;
    bool m_bIsTiledHeapNotSupported = false;
    bool is_SysmemMap               = false;
    //@}
    //@{
    //! Test specific functions
    RC MapAndTest(LwRm::Handle hMem, UINT64 offset,
                  UINT32 m_memSize);
    RC MapAndTestAttr(LwRm::Handle hMem, UINT64 offset, UINT32 m_memSize);
    RC MapAndTestAttrI(LwRm::Handle hMem,
                       void *address,
                       UINT64 offset,
                       UINT32 m_memSize);
    //@}

};

//! \brief Constructor for MapMemoryTest
//!
//! Just does nothing except setting a name for the test..the actual
//! functionality is done by Setup()
//!
//! \sa Setup
//-----------------------------------------------------------------------------
MapMemoryTest::MapMemoryTest()
{
    SetName("MapMemoryTest");
}

//! \brief MapMemoryTest destructor
//!
//! All resources should be freed in the Cleanup member function, so
//! destructors shouldn't do much.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
MapMemoryTest::~MapMemoryTest()
{
}

//! \brief IsTestSupported() is supported for all
//!
//! Checks whether the hardware classes are supported on the current elwiroment
//! or not
//-----------------------------------------------------------------------------
string MapMemoryTest::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief Setup()
//!
//! This function basically allocates  memory of class "LW01_MEMORY_SYSTEM",
//! "LW01_MEMORY_LOCAL_USER", video-heap memory tiled or non-tiled. If the
//! memory allocations through the heap interface are not supported, we are
//!  printing  the error and continue to run the test on allocated memory types
//!
//! \return RC -> OK if everything is allocated and even the system memory
//!               allocations through the heap interface are not supported,
//!               test-specific RC if something failed while selwring resources
//!
//! \sa Run
//-----------------------------------------------------------------------------
RC MapMemoryTest::Setup()
{
    RC           rc;
    m_bIsTiledHeapNotSupported=false;
    is_SysmemMap=false;
    m_sys_limit = (UINT64)RM_PAGE_SIZE;
    m_local_limit =  (UINT64)RM_PAGE_SIZE;
    m_vid_limit   = RM_PAGE_SIZE*2;
    UINT64 temp=(UINT64)RM_PAGE_SIZE;
    m_pitch = PITCH;
    CHECK_RC(InitFromJs());

    m_hSysmem = new Surface2D();
    m_hSysmem->SetLayout(Surface2D::Pitch);
    m_hSysmem->SetPitch(UNSIGNED_CAST(UINT32, m_sys_limit));
    m_hSysmem->SetColorFormat(ColorUtils::VOID32);
    m_hSysmem->SetWidth(UNSIGNED_CAST(UINT32, m_sys_limit));
    m_hSysmem->SetHeight(1);
    m_hSysmem->SetLocation(Memory::NonCoherent);
    CHECK_RC(m_hSysmem->Alloc(GetBoundGpuDevice(),m_TestConfig.GetRmClient()));
    CHECK_RC(m_hSysmem->Map());
    if (rc == OK)
        is_SysmemMap = true;

    m_hLocalmem = new Surface2D();
    m_hLocalmem->SetLayout(Surface2D::Pitch);
    m_hLocalmem->SetPitch(UNSIGNED_CAST(UINT32, temp));
    m_hLocalmem->SetColorFormat(ColorUtils::VOID32);
    m_hLocalmem->SetWidth(UNSIGNED_CAST(UINT32, temp));
    m_hLocalmem->SetHeight(1);
    m_hLocalmem->SetLocation(Memory::Fb);
    CHECK_RC(m_hLocalmem->Alloc(GetBoundGpuDevice(),m_TestConfig.GetRmClient()));

    m_hVidmem = new Surface2D();
    m_hVidmem->SetLayout(Surface2D::Pitch);
    m_hVidmem->SetPitch(m_vid_limit);
    m_hVidmem->SetType(LWOS32_TYPE_IMAGE);
    m_hVidmem->SetColorFormat(ColorUtils::VOID32);
    m_hVidmem->SetWidth(m_vid_limit);
    m_hVidmem->SetHeight(1);
    m_hVidmem->SetLocation(Memory::Fb);
    CHECK_RC(m_hVidmem->Alloc(GetBoundGpuDevice(),m_TestConfig.GetRmClient()));

    //
    // Finally test an allocation from the non local video heap.
    // Lwrrently must forced tiled so allocation will occur in mappable
    // portion of BAR1.
    //
    m_height = m_vid_limit/m_pitch;

    m_hVidmemTiledOrNonTiled = new Surface2D();
    m_hVidmemTiledOrNonTiled->SetLayout(Surface2D::Pitch);
    m_hVidmemTiledOrNonTiled->SetPitch(m_pitch);
    m_hVidmemTiledOrNonTiled->SetType(LWOS32_TYPE_IMAGE);
    m_hVidmemTiledOrNonTiled->SetColorFormat(ColorUtils::VOID32);
    m_hVidmemTiledOrNonTiled->SetWidth(m_vid_limit);
    m_hVidmemTiledOrNonTiled->SetHeight(m_height);
    m_hVidmemTiledOrNonTiled->SetTiled(true);
    m_hVidmemTiledOrNonTiled->SetHeight(1);
    m_hVidmemTiledOrNonTiled->SetLocation(Memory::NonCoherent);
    CHECK_RC(m_hVidmemTiledOrNonTiled->Alloc(GetBoundGpuDevice(),m_TestConfig.GetRmClient()));
    //
    // Checking for the error in VidHeapAllocPitchHeight(), if error does
    // not occur, do nothing and proceed which will be enhanced later
    //
    if (rc == OK)
    {

    }
    else if (rc == RC::LWRM_ILWALID_ARGUMENT)
    {
        //
        // Assume now we're on a system that support system
        // memory allocations but not tiling (like LW50).
        // Remove the TILED_REQUIRED flag and try again.
        //

        m_hVidmemTiledOrNonTiled->SetPitch(m_vid_limit);
        m_hVidmemTiledOrNonTiled->SetTiled(false);
        CHECK_RC(m_hVidmemTiledOrNonTiled->Alloc(GetBoundGpuDevice(),m_TestConfig.GetRmClient()));
        //Checking for the error in VidHeapAllocSize(), if error does
        // not occur, do nothing and proceed
        if (rc == OK)
        {

        }
        else if (rc == RC::LWRM_ILWALID_ARGUMENT)
        {
            // At this point, conclude system memory allocations through
            // the heap interface are not supported.
            rc = OK;
            m_bIsTiledHeapNotSupported=true;

        }
        else
        {
            rc = RC::LWRM_ERROR;
            Printf(Tee::PriHigh,
                " %d:MapMemoryTest: Error: unexpected error code broken %s\n",
                __LINE__,rc.Message());
        }
    }
    else
    {
        rc = RC::LWRM_ERROR;
        Printf(Tee::PriHigh,
            " %d:MapMemoryTest: Error: unexpected error code broken %s\n",
            __LINE__,rc.Message());
    }
    return rc;
}

//! \brief This function runs the test
//!
//! This test maps and tests the memory allocated in the Setup() with
//! attributes via MapAndTestAttr() wherein access flags are set for the
//! mapping like  WRITE_ONLY, READ_WRITE, READ_ONLY
//! and then writes data into the mapped
//! locations and reads via the other mapped locations and without attributes
//! via MapAndTest() in order to test the access rights of the mapped
//! locations
//!
//! \return RC -> OK if the test runs successfully, test-specific RC if test
//!               failed
//!
//! \sa MapAndTest
//! \sa MapAndTestAttr
//-----------------------------------------------------------------------------
RC MapMemoryTest::Run()
{
    RC rc;
    LwRmPtr pLwRm;

    CHECK_RC(MapAndTestAttr(m_hSysmem->GetMemHandle(), 0,(UINT32) m_sys_limit));
    CHECK_RC(MapAndTestAttrI(m_hSysmem->GetMemHandle(),
                             m_hSysmem->GetAddress(),
                             0,
                             (UINT32) m_sys_limit));

    // First test memory mapping with global video memory handle
    CHECK_RC(MapAndTest(m_hLocalmem->GetMemHandle(), 0,(UINT32) m_local_limit));

    // Now test with heap memory handle
    CHECK_RC(MapAndTest(m_hVidmem->GetMemHandle(), 0, m_vid_limit));

    // test mapping attributes
    CHECK_RC(MapAndTestAttr(m_hVidmem->GetMemHandle(), 0, m_vid_limit));

    // if allowed then test for the Tiled surface mapping
    if(!m_bIsTiledHeapNotSupported)
    {
        Printf(Tee::PriHigh,
        " %d:MapMemoryTest:Run->The GPU Vrr Addr mappable size is zero \n",
               __LINE__);

        Printf(Tee::PriHigh,"This seems to be the gpu with ");
        Printf(Tee::PriHigh,"localFBSize==bar1Size so the zero mappable, ");
        Printf(Tee::PriHigh,"Impossible to map this Tiled FB hw resource ");
        Printf(Tee::PriHigh,"through bar1 aperture so skip the mapping \n");
    }
    return rc;
}

//! \brief Cleanup()
//!
//! free the resources allocated
//!
//! \sa Setup
//-----------------------------------------------------------------------------
RC MapMemoryTest::Cleanup()
{
    m_hVidmem->Free();
    delete m_hVidmem;
    m_hLocalmem->Free();
    delete m_hLocalmem;
    if (is_SysmemMap)
        m_hSysmem->Unmap();
    m_hSysmem->Free();
    delete m_hSysmem;
    m_hVidmemTiledOrNonTiled->Free();
    delete m_hVidmemTiledOrNonTiled;

    return OK;
}

//-----------------------------------------------------------------------------
//PRIVATE FUNCTIONS
//-----------------------------------------------------------------------------

//! \brief This function tests the mapping functionality without any attributes
//! by writing into one set of mapped locations and reading from the other set
//! of mapped locations mapped to the same memory handle to check the flow of
//! data.
//!
//! \return RC -> OK if the test runs successfully, test-specific RC if the test
//!               failed
//!
//! \sa Run
//-----------------------------------------------------------------------------
RC MapMemoryTest::MapAndTest(LwRm::Handle hMemory,
                             UINT64      offset,
                             UINT32 m_memSize
                            )
{

    LwRmPtr      pLwRm;
    RC           rc;
    UINT32      *ip;
    UINT32       i;
    void        *pMapAddr[2];
    bool         isMemMapped[2]    = { false, false };

    for (i = 0; i < 2; i++)
    {
        CHECK_RC_CLEANUP(pLwRm->MapMemory(hMemory,
            offset,
            m_memSize,
            &pMapAddr[i],
            0,
            GetBoundGpuSubdevice()));
        isMemMapped[i] = true;
    }

    //
    // Write unique values to the first mapped address and verify
    // the second mapping corresponds to the same heap offset.
    //
    ip = (UINT32 *)pMapAddr[0];
    for (i = 0; i < m_memSize/sizeof(UINT32); i++)
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
        Printf(Tee::PriHigh, "MapMemoryTest: MODS Returned Unsupported function\
                              when Platform::FlushCpuWriteCombineBuffer() was\
                              called..\n");
        CHECK_RC_CLEANUP(rc);
    }

    ip = (UINT32 *)pMapAddr[1];
    for (i = 0; i < m_memSize/sizeof(UINT32); i++)
    {
        if (MEM_RD32(ip) != i)
        {
            rc = RC::LWRM_ERROR;
            Printf(Tee::PriHigh,
                " %d:MapMemoryTest: Error: Incorrect mappings broken %s\n",
                __LINE__,rc.Message());
            goto Cleanup;
        }
        ip++;
    }
    // Unmaps the memory and free the resources
Cleanup:
    for (i = 0; i < 2; i++)
    {
        if (isMemMapped[i])
            pLwRm->UnmapMemory(hMemory, pMapAddr[i], 0, GetBoundGpuSubdevice());
    }

    return rc;
}

//! \brief This function tests the mapping functionality with attributes
//! by enabling WRITE_ONLY, READ_WRITE, READ_ONLY flags.
//!
//! \return RC -> OK if the test runs successfully, test-specific RC if test
//!               failed
//!
//! \sa MapAndTest
//-----------------------------------------------------------------------------
RC MapMemoryTest::MapAndTestAttr(LwRm::Handle hMemory,
                                 UINT64      offset,
                                 UINT32 m_memSize
                                )
{
    RC           rc;
    LwRmPtr      pLwRm;
    UINT32       *ip;
    UINT32       i;
    void         *pMapAddr[3];
    bool         isMemMapped[3]    = { false, false, false };

    // We create three mappings to the same memory
    UINT32       flags[3] = {LWOS03_FLAGS_ACCESS_WRITE_ONLY,
        LWOS03_FLAGS_ACCESS_READ_WRITE,
        LWOS03_FLAGS_ACCESS_READ_ONLY};

    for (i = 0; i < sizeof(flags)/sizeof(flags[0]); i++)
    {
        CHECK_RC_CLEANUP(pLwRm->MapMemory(hMemory,
            offset,
            m_memSize,
            &pMapAddr[i],
            flags[i],
            GetBoundGpuSubdevice()));
        isMemMapped[i] = true;
    }

    // Now write to the first mapping
    ip = (UINT32 *)pMapAddr[0];
    for (i = 0; i < m_memSize/sizeof(UINT32); i++)
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
        Printf(Tee::PriHigh, "MapMemoryTest: MODS Returned Unsupported function\
                              when Platform::FlushCpuWriteCombineBuffer() was\
                              called..\n");
        CHECK_RC_CLEANUP(rc);
    }

    //
    // Read via the second mapping (R/W) and also write via the same mapping
    // then read via the third mapping (READ_ONLY)
    // Here the test runs as WRITE_ONLY->READ_WRITE->READ_ONLY
    //
    ip = (UINT32 *)pMapAddr[1];
    // Now try to read via READ-WRITE
    for (i = 0; i < m_memSize/sizeof(UINT32); i++)
    {
        if (MEM_RD32(ip) != i)
        {
            rc = RC::LWRM_ERROR;
            Printf(Tee::PriHigh,
                " %d:MapMemoryTest: Error: READ_WRITE broken %s\n",
                __LINE__,rc.Message());
            goto Cleanup;
        }
        ip++;
    }

    // Write through READ-WRITE
    ip = (UINT32 *)pMapAddr[1];
    for (i = 0; i < m_memSize/sizeof(UINT32); i++)
    {
        MEM_WR32(ip, i+1);
        ip++;
    }

    ip = (UINT32 *)pMapAddr[2];
    //
    // Call FlushCpuWriteCombineBuffer() which calls sfence instruction
    // This will cause all previous video memory operations to complete
    // before we go ahead and read back the same memory locations.
    //
    rc = Platform::FlushCpuWriteCombineBuffer();
    if (rc == RC::UNSUPPORTED_FUNCTION)
    {
        Printf(Tee::PriHigh, "MapMemoryTest: MODS Returned Unsupported function\
                              when Platform::FlushCpuWriteCombineBuffer() was\
                              called..\n");
        CHECK_RC_CLEANUP(rc);
    }

    //
    // Now read through the read only mappings
    // If this is broken then READ_ONLY mappings are broken, since we have
    // already tested READ_WRITE mappings.
    //
    for (i = 0; i < m_memSize/sizeof(UINT32); i++)
    {
        if (MEM_RD32(ip) != i+1)
        {
            rc = RC::LWRM_ERROR;
            Printf(Tee::PriHigh,
                " %d:MapMemoryTest: Error: READ_ONLY broken %s\n",
                __LINE__,rc.Message());
            goto Cleanup;

        }
        ip++;

    }

    // Unmaps the memory and free the resources
Cleanup:
    for (i = 0; i < sizeof(flags)/sizeof(flags[0]); i++)
    {
        if (isMemMapped[i])
            pLwRm->UnmapMemory(hMemory, pMapAddr[i], 0, GetBoundGpuSubdevice());
    }
    return rc;
}

//! \brief MapAndTestAttrI()
//!
//! This function tests the mapping functionality with attributes
//! by enabling Read_ONLY, READ_WRITE and WRITE_ONLY flags.
//!
//! \return RC -> OK if the test runs successfully, test-specific RC if test
//!               failed
//!
//! \sa Run
//-----------------------------------------------------------------------------
RC MapMemoryTest::MapAndTestAttrI(LwRm::Handle hMemory,
                                  void * address,
                                  UINT64 offset,
                                  UINT32 m_memSize
                                 )
{
    RC           rc;
    LwRmPtr      pLwRm;
    UINT32       *ip;
    UINT32       i;
    void         *pMapAddr[3];
    bool         isMemMapped[3]    = { false, false, false };

    // Second test case with Read_ONLY, READ_WRITE and WRITE_ONLY.
    UINT32  flags[3] = {LWOS03_FLAGS_ACCESS_READ_ONLY,
        LWOS03_FLAGS_ACCESS_READ_WRITE,
        LWOS03_FLAGS_ACCESS_WRITE_ONLY};

    //Starting the test with first writing into the system memory
    //using MEM_WR32()

    UINT32 *ADDR;
    ADDR=(UINT32 *)address;
    for (i = 0; i < m_sys_limit/sizeof(UINT32); i++)
    {
        MEM_WR32(ADDR,i+5);
        ADDR++;
    }
    //Mapping the three locatins to the same memory

    for (i = 0; i < sizeof(flags)/sizeof(flags[0]); i++)
    {
        CHECK_RC_CLEANUP(pLwRm->MapMemory(hMemory,
            offset,
            m_memSize,
            &pMapAddr[i],
            flags[i],
            GetBoundGpuSubdevice()));
        isMemMapped[i] = true;
    }
    //
    // Call FlushCpuWriteCombineBuffer() which calls sfence instruction
    // This will cause all previous video memory operations to complete
    // before we go ahead and read back the same memory locations.
    //
    rc = Platform::FlushCpuWriteCombineBuffer();
    if (rc == RC::UNSUPPORTED_FUNCTION)
    {
        Printf(Tee::PriHigh, "MapMemoryTest: MODS Returned Unsupported function\
                              when Platform::FlushCpuWriteCombineBuffer() was\
                              called..\n");
        CHECK_RC_CLEANUP(rc);
    }

    // Now read via the first mapping
    ip = (UINT32 *)pMapAddr[0];

    for (i = 0; i < m_memSize/sizeof(UINT32); i++)
    {
        if (MEM_RD32(ip) != i+5)
        {
            rc = RC::LWRM_ERROR;
            Printf(Tee::PriHigh,
                " %d:MapMemoryTest: Error: READ_ONLY broken %s\n",
                __LINE__,rc.Message());
            goto Cleanup;
        }
        ip++;
    }

    // Write through READ-WRITE
    ip = (UINT32 *)pMapAddr[1];

    for (i = 0; i < m_memSize/sizeof(UINT32); i++)
    {
        MEM_WR32(ip, i+3);
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
        Printf(Tee::PriHigh, "MapMemoryTest: MODS Returned Unsupported function\
                              when Platform::FlushCpuWriteCombineBuffer() was\
                              called..\n");
        CHECK_RC_CLEANUP(rc);
    }

    //Now, Read thru READ_WRITE
    ip = (UINT32 *)pMapAddr[1];
    for (i = 0; i < m_memSize/sizeof(UINT32); i++)
    {
        if (MEM_RD32(ip) != i+3)
        {
            rc = RC::LWRM_ERROR;
            Printf(Tee::PriHigh,
                " %d:MapMemoryTest: Error: READ_ONLY broken %s\n",
                __LINE__,rc.Message());
            goto Cleanup;
        }
        ip++;
    }

    //Now, Write thru the third mapping

    ip = (UINT32 *)pMapAddr[2];
    for (i = 0; i < m_memSize/sizeof(UINT32); i++)
    {
        MEM_WR32(ip, i+10);
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
        Printf(Tee::PriHigh, "MapMemoryTest: MODS Returned Unsupported function\
                              when Platform::FlushCpuWriteCombineBuffer() was\
                              called..\n");
        CHECK_RC_CLEANUP(rc);
    }

    //Now read thru the original memory which is mapped
    ADDR=(UINT32 *)address;
    for (i = 0; i < m_sys_limit/sizeof(UINT32); i++)
    {
        if(MEM_RD32(ADDR)!=i+10)
        {
            rc = RC::LWRM_ERROR;
            Printf(Tee::PriHigh,
                " %d:MapMemoryTest: Error: READ_ONLY broken %s\n",
                __LINE__,rc.Message());
            goto Cleanup;
        };
        ADDR++;
    }

    // Unmaps the memory and free the resources
Cleanup:
    for (i = 0; i < sizeof(flags)/sizeof(flags[0]); i++)
    {
        if (isMemMapped[i])
            pLwRm->UnmapMemory(hMemory, pMapAddr[i], 0, GetBoundGpuSubdevice());
    }
    return rc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ MapMemoryTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(MapMemoryTest, RmTest,
                 "RmMapMemory functionality test.");

