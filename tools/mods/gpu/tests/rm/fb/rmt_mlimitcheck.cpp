/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2004-2013,2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file rmt_mlimitcheck.cpp
//! \brief MemoryLimitCheckTest functionality test
//!
//! The aim of this test is to check the valid limits and bases when
//! associating context DMA to the memory allocated.
//! This test starts by allocating a DMA channel and
//! then allocating system memory and video memory with handles respectively.
//! Then allocating a VASPACE for the mods client and associating a context
//! DMA to that VASPACE. Now the test proceeds in testing the functionality of
//! assoicating the context DMA to VASPACE by varying the memory limits (sizes)
//! Then the channel is bound to the context DMA handle of the VASPACE.

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/rc.h"
#include "core/include/platform.h"
#include "core/include/lwrm.h"
#include "lwRmApi.h"
#include "core/include/tee.h"
#include "lwos.h"
#include "core/include/channel.h"
#include "core/include/utility.h"

#include "class/cl0000.h" // LW01_NULL_OBJECT
#include "class/cl0002.h" // LW01_CONTEXT_DMA
#include "class/cl003e.h" // LW01_MEMORY_SYSTEM
#include "class/cl0080.h" // LW01_DEVICE_0
#include "class/cl446e.h" // LW44_CHANNEL_DMA
#include "class/cl406e.h" // LW40_CHANNEL_DMA
#include "class/cl206e.h" // LW20_CHANNEL_DMA
#include "class/cl366e.h" // LW36_CHANNEL_DMA
#include "class/cl006e.h" // LW10_CHANNEL_DMA
#include "class/cl006c.h" // LW04_CHANNEL_DMA
#include "core/include/memcheck.h"

#define LIMITCHECK_ID_NOT_MEM           0xF00D0005
#define LIMITCHECK_ID_NOT_CTX           0xF00D0006

#define MAP_TEST_MEM_VID                       0x1
#define MAP_TEST_MEM_SYS                       0x2
#define MAP_TEST_MEM_EXPECT_ALLOC_FAILURE      0x4

#define pushBuffer_BYTESIZE             (64*1024)
#define ERROR_LIMIT                      (4*1024)-1
#define GOOD_TEST_ARR_SIZE              2
#define BAD_TEST_ARR_SIZE               6
// These are the invalid limits enforced in the test such that
// they are greater than m_VALimit
#define CLIENT_VALimit_20b            ((LwU64)1 << 20) -1
#define CLIENT_VALimit_22b            ((LwU64)1 << 22) -1
#define CLIENT_VALimit_26b            ((LwU64)1 << 26) -1
#define CLIENT_VALimit_30b            ((LwU64)1 << 30)-1
#define LOWEST_FIXED_AVAIL_VA          0x20000
#define ALLIGN_64K(x)                  (x & (~((1<<16)-1)))
#define LSHIFT16(x)                    (LwU64(x) << 16)
#define NUMBER_CLASSES                  6

class MemoryLimitCheckTest: public RmTest
{
public:

    MemoryLimitCheckTest();
    virtual ~MemoryLimitCheckTest();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

protected:
    //@{
    //! Test specific variables
    static const LwU64 m_TestMemSize = 64*1024*2; // 2*64KB render page
    static UINT32 m_DmaClasses[NUMBER_CLASSES];
    UINT32 m_DmaClass;
    LwU32 *m_pushBuffer;
    LwU64 m_pushBufferLimit, m_errorBufferLimit;
    void *m_errorBuffer;
    LwU64 m_VALimit;
    LwU64 m_vaTestVidMem; // gpu reference ptr to vidmem
    void * m_pTestVidMem; // cpu reference ptr to vidmem
    LwU64 m_vaTestSysMem; // gpu reference ptr to sysmem
    void * m_pTestSysMem; // cpu reference ptr to sysmem
    LwU64 m_vaTestVidMem2; // gpu reference ptr to vidmem2
    LwU64 m_vaTestSysMem2; // gpu reference ptr to sysmem2
    // Limits are chosen to within allocatable addresses in a
    // VASpace
    LwU64 m_CtxDmaLo;
    LwU64 m_CtxDmaHi;
    bool m_allocedChan,m_allocedErrCtx,m_allocedErrMem,m_allocedPbCtx;
    bool m_isTestMemalloced,m_isTestMemalloced2,m_isVidMemalloced;
    bool m_isVaCtxAlloced,m_isVaSpaceAlloced,m_allocedPbMem,m_isVidMemalloced2;

    LwRm::Handle m_hPbMem,m_hErrMem,m_hPbCtx,m_hErrCtx,m_hTestMem;
    LwRm::Handle m_hTestMem2,m_hVidMem,m_hVidMem2,m_pRtnChannelHandle;
    LwRm::Handle m_hVaSpace,m_hVaCtx,m_hVaCtx2;
    //@}
    //@{
    //! Test specific functions
    RC AllocChannel();
    RC AllocClientVASpace();
    RC AllocTestMem();
    //@}
};
UINT32 MemoryLimitCheckTest::m_DmaClasses[NUMBER_CLASSES]={
    LW44_CHANNEL_DMA,
        LW40_CHANNEL_DMA,
        LW36_CHANNEL_DMA,
        LW20_CHANNEL_DMA,
        LW10_CHANNEL_DMA,
        LW04_CHANNEL_DMA};

    //! \brief Constructor for MemoryLimitCheckTest
    //!
    //! Just does nothing except setting a name for the test..the actual
    //! functionality is done by Setup()
    //!
    //! \sa Setup
    //-------------------------------------------------------------------------
    MemoryLimitCheckTest::MemoryLimitCheckTest()
    {
        SetName("MemoryLimitCheckTest");
    }

    //! \brief MemoryLimitCheckTest destructor
    //!
    //! All resources should be freed in the Cleanup member function, so
    //! destructors shouldn't do much.
    //!
    //! \sa Cleanup
    //-------------------------------------------------------------------------
    MemoryLimitCheckTest::~MemoryLimitCheckTest()
    {
    }

    //! \brief IsTestSupported()
    //!
    //!Checks whether the hardware classes are supported or not
    //!
    //! return->true when the class is supported else return false (unsupprted)
    //------------------------------------------------------------------------
    string MemoryLimitCheckTest::IsTestSupported()
    {
        // MODS support for DMA channels is being retired
        return "MemoryLimitCheckTest test relies on obsolete DMA channel functionality";
    }

    //! \brief Setup()
    //!
    //! This function basically allocates a channel with a handle, allocates
    //! Test memory(system and video) The reason for doing this is that we hav
    //! to bind the channel with a DMA handle in Run() and test for memorylimit
    //! check.
    //!
    //! \return RC -> OK if everything is allocated, test-specific RC if
    //!               failed while selwring resources.
    //!
    //! \sa Run
    //! \sa AllocChannel
    //! \sa AllocClientVASpace
    //! \sa AllocTestMem
    //-------------------------------------------------------------------------
    RC MemoryLimitCheckTest::Setup()
    {
        RC rc;

        // Allocate channels for each of the clients
        CHECK_RC(AllocChannel());

        // Allocate Test Mem
        CHECK_RC(AllocTestMem());

        return rc;
    }

    //! \brief This function runs the test
    //!
    //! This function allocates VASPACE by specifying the memory class as
    //! SYSTEM MEMORY of size m_VALimit.After the allocation, the test proceeds
    //! in associating a context DMA handle to the VASPACE where the invalid
    //! invalid are enforced deliberately in the AllocContextDma()thereby catch
    //! the errors thus testing the functionality of AllocContextDma() in
    //! negative sense.Then the offsets and limits of AllocContextDma() are
    //! takenas some functions of m_VALimit and again the ALlocContextDma() is
    //! tested for accuracy of VALID_BASE and VALID_LIMIT cases.
    //! Then binding the context DMA to the channel.
    //!
    //! \return RC -> OK if the test is run successfully, test-specific RC if
    //!               test failed
    //!
    //! \sa MapTestMem
    //-------------------------------------------------------------------------
    RC MemoryLimitCheckTest::Run()
    {
        RC rc;

        // Allocate Client VASpace
        CHECK_RC(AllocClientVASpace());

        return rc;
    }

    //! \brief Cleanup()
    //!
    //! free the resources allocated
    //!
    //! \sa Setup
    //-------------------------------------------------------------------------
    RC MemoryLimitCheckTest::Cleanup()
    {
        LwRmPtr pLwRm;
        pLwRm->Free(m_pRtnChannelHandle);
        pLwRm->Free(m_hErrCtx);
        pLwRm->Free(m_hErrMem);
        pLwRm->Free(m_hPbCtx);
        pLwRm->Free(m_hPbMem);
        pLwRm->Free(m_hTestMem);
        pLwRm->Free(m_hVidMem);
        pLwRm->Free(m_hTestMem2);
        pLwRm->Free(m_hVidMem2);
        pLwRm->Free(m_hVaCtx);
        pLwRm->Free(m_hVaSpace);

        return OK;
    }

    //! \brief AllocChannel()
    //!
    //! The basic operation of this function is to allocate a DMA channel
    //!
    //! \sa Setup
    //----------------------------------------------------------------------
    RC MemoryLimitCheckTest::AllocChannel()
    {

        // Allocate a Push Buffer in system memory
        LwRmPtr pLwRm;
        RC rc;
        m_allocedChan=false;
        m_allocedErrCtx=false;
        m_allocedErrMem=false;
        m_allocedPbCtx=false;
        m_allocedPbMem=false;

        m_pushBufferLimit = pushBuffer_BYTESIZE-1;

        //CHECK_RC_CLEANUP( RmApiStatusToModsRC(rmrc) );
        CHECK_RC_CLEANUP(pLwRm->AllocMemory(&m_hPbMem,LW01_MEMORY_SYSTEM,
            DRF_DEF(OS02,_FLAGS,_PHYSICALITY,_NONCONTIGUOUS),
            (void**)&m_pushBuffer,
            &m_pushBufferLimit,
            GetBoundGpuDevice()));
        m_allocedPbMem = true;

        // Allocate Context DMA for this m_pushBuffer
        CHECK_RC_CLEANUP(pLwRm->AllocContextDma(&m_hPbCtx,
            DRF_DEF(OS03,_FLAGS,_ACCESS,_READ_WRITE),
            m_hPbMem,0,m_pushBufferLimit));

        m_allocedPbCtx = true;

        // Allocate memory for the error notifier
        m_errorBufferLimit = ERROR_LIMIT;
        CHECK_RC_CLEANUP(pLwRm->AllocMemory(&m_hErrMem,LW01_MEMORY_SYSTEM,
            DRF_DEF(OS02,_FLAGS,_PHYSICALITY,_NONCONTIGUOUS),
            &m_errorBuffer,
            &m_errorBufferLimit,
            GetBoundGpuDevice()));
        m_allocedErrMem = true;

        // Allocate Context DMA for the error buffer
        CHECK_RC_CLEANUP(pLwRm->AllocContextDma(&m_hErrCtx,
            DRF_DEF(OS03,_FLAGS,_ACCESS,_READ_WRITE),
            m_hErrMem,0,sizeof(LwNotification) - 1));
        m_allocedErrCtx = true;

        // Should allocate DMA channel here
        rc = RC::UNSUPPORTED_FUNCTION;
        goto Cleanup;

        m_allocedChan = true;
        return OK;

Cleanup:
        return rc;
    }

    //! \brief AllocTestMem()
    //!
    //! The purpose of this function is to allocate test memory(system, video)
    //! and allocate context DMA handles for them which are to be mapped by the
    //! context DMA handle of VASPACE. Ofcourse, this mapping will be done in
    //! Run()
    //!
    //! \sa Run
    //! \sa Setup
    //----------------------------------------------------------------------
    RC MemoryLimitCheckTest::AllocTestMem()
    {
        LwRmPtr pLwRm;
        UINT32 attr;
        UINT32 attr2;
        RC rc;
        UINT64 Offset=0;
        m_isTestMemalloced=false;
        m_isTestMemalloced2=false;
        m_isVidMemalloced=false;
        m_isVidMemalloced2=false;
        //
        // vidmem
        //
        attr = (DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH) |
                DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
                DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
                DRF_DEF(OS32, _ATTR, _TILED, _NONE) |
                DRF_DEF(OS32, _ATTR, _ZLWLL, _NONE) |
                DRF_DEF(OS32, _ATTR, _COMPR, _NONE) |
                DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM) |
                DRF_DEF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS));
        attr2 = LWOS32_ATTR2_NONE;
        CHECK_RC
            (
                pLwRm->VidHeapAllocSizeEx
                (
                LWOS32_TYPE_IMAGE,
                LWOS32_ALLOC_FLAGS_MEMORY_HANDLE_PROVIDED |
                LWOS32_ALLOC_FLAGS_NO_SCANOUT |
                LWOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED,
                &attr,
                &attr2, // pAttr2
                m_TestMemSize,
                1,
                NULL,
                NULL,
                NULL,
                &m_hVidMem,
                &Offset ,
                NULL, NULL, NULL,
                0,0,
                GetBoundGpuDevice()
                )
            );

        m_isVidMemalloced=true;
        //
        // now sysmem
        //
        LwU64 SysmemRgnLimit ;
        SysmemRgnLimit= m_TestMemSize - 1;
        void *dummyAddress;
        dummyAddress = NULL;
        CHECK_RC(pLwRm->AllocMemory(&m_hTestMem,
            LW01_MEMORY_SYSTEM,
            ( DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
            DRF_DEF(OS02, _FLAGS, _MAPPING, _NO_MAP) |
            DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS) ),
            &dummyAddress, &SysmemRgnLimit, GetBoundGpuDevice()));
        m_isTestMemalloced=true;
        //
        // vidmem
        //
        attr = (DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH) |
                DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
                DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
                DRF_DEF(OS32, _ATTR, _TILED, _NONE) |
                DRF_DEF(OS32, _ATTR, _ZLWLL, _NONE) |
                DRF_DEF(OS32, _ATTR, _COMPR, _NONE) |
                DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM) |
                DRF_DEF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS));
        attr2 = LWOS32_ATTR2_NONE;

        CHECK_RC(
                    pLwRm->VidHeapAllocSizeEx
                    (
                    LWOS32_TYPE_IMAGE,
                    LWOS32_ALLOC_FLAGS_MEMORY_HANDLE_PROVIDED |
                    LWOS32_ALLOC_FLAGS_NO_SCANOUT |
                    LWOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED,
                    &attr,
                    &attr2, // pAttr2
                    m_TestMemSize,1,NULL,NULL,NULL,&m_hVidMem2,&Offset,
                    NULL, NULL, NULL,
                    0,0,
                    GetBoundGpuDevice()
                    )
            );
        m_isVidMemalloced2=true;
        //
        // now sysmem
        //
        CHECK_RC(pLwRm->AllocMemory(&m_hTestMem2,
            LW01_MEMORY_SYSTEM,
            ( DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
            DRF_DEF(OS02, _FLAGS, _MAPPING, _NO_MAP) |
            DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS) ),
            &dummyAddress, &SysmemRgnLimit, GetBoundGpuDevice()));
        m_isTestMemalloced2=true;

        return rc;
    }
    //! \brief AllocClientVASpace()
    //!
    //! This function allocates VASPACE by specifying the memory class as
    //! SYSTEM MEMORY and then allocating context DMA handle
    //! Here implicit testing is done on memory limits by allocating context
    //! DMA to the VASPACE forcing the memory limits go out of range.
    //!
    //! \sa Run
    //-----------------------------------------------------------------------
    RC MemoryLimitCheckTest::AllocClientVASpace()
    {
        LwRmPtr pLwRm;
        RC rc,rmrc;
        LwU64 ilwalidVALimit = 0;
        m_isVaCtxAlloced=false;
        m_isVaSpaceAlloced=false;
        m_VALimit=CLIENT_VALimit_22b;
        LwU64 temp=m_VALimit;
        //
        // Begin allocating/mapping memory in the 'client' context
        //
        CHECK_RC_CLEANUP(
            pLwRm->AllocSystemMemory(&m_hVaSpace,
            DRF_DEF(OS02, _FLAGS, _ALLOC, _NONE),
            m_VALimit, GetBoundGpuDevice())
            );
        m_isVaSpaceAlloced=true;
        m_CtxDmaLo = ALLIGN_64K((m_VALimit/16 + LOWEST_FIXED_AVAIL_VA));
        m_CtxDmaHi = ALLIGN_64K((m_VALimit/2 + LOWEST_FIXED_AVAIL_VA));

        // Make sure the ctx dma range are within the range of allocated memory
        if ((m_CtxDmaLo >= m_CtxDmaHi) &&
            (m_CtxDmaHi > m_VALimit)&&(m_CtxDmaLo+m_CtxDmaHi+1>m_VALimit))
        {
            rc = RC::LWRM_ILWALID_LIMIT;
            CHECK_RC_CLEANUP(rc);
        }

        // Make sure VAS limit are enforced by the Context Dma Allocation
        switch (temp)
        {
        case CLIENT_VALimit_20b:
            ilwalidVALimit = CLIENT_VALimit_22b;
            rmrc=pLwRm->AllocContextDma( &m_hVaCtx,
                LWOS03_FLAGS_ACCESS_READ_WRITE,
                m_hVaSpace,
                0,
                ilwalidVALimit );
            if(rmrc==OK)
            {
                rc = RC::LWRM_ILWALID_LIMIT;
                m_isVaCtxAlloced=true;
            }

            break;
        case CLIENT_VALimit_22b:
            ilwalidVALimit = CLIENT_VALimit_26b;
            rmrc=pLwRm->AllocContextDma( &m_hVaCtx,
                LWOS03_FLAGS_ACCESS_READ_WRITE,
                m_hVaSpace,
                0,
                ilwalidVALimit );
            if(rmrc==OK)
            {
                rc = RC::LWRM_ILWALID_LIMIT;
                m_isVaCtxAlloced=true;
            }

            break;
        case CLIENT_VALimit_26b:
            ilwalidVALimit = CLIENT_VALimit_30b;
            rmrc=pLwRm->AllocContextDma( &m_hVaCtx,
                LWOS03_FLAGS_ACCESS_READ_WRITE,
                m_hVaSpace,
                0,
                ilwalidVALimit );
            if(rmrc==OK)
            {
                rc = RC::LWRM_ILWALID_LIMIT;
                m_isVaCtxAlloced=true;
            }

            break;
        case CLIENT_VALimit_30b:
            break;
        default: // Unexpected m_VALimit
            rc = RC::LWRM_ILWALID_LIMIT;
            break;
        }
        CHECK_RC_CLEANUP(rc);
        //
        // Now,testing the offset and limit values in AllocContextDma()checking
        //for the VALID_BASE and VALID_LIMIT cases.
        //

        CHECK_RC_CLEANUP(
                            pLwRm->AllocContextDma
                            (
                            &m_hVaCtx,
                            LWOS03_FLAGS_ACCESS_READ_WRITE,
                            m_hVaSpace,
                            m_CtxDmaLo,
                            m_CtxDmaHi
                            )
                        );
        m_isVaCtxAlloced=true;

Cleanup:
        return rc;
    }

    //-------------------------------------------------------------------------
    // Set up a JavaScript class that creates and owns a C++
    // MemoryLimitCheckTest object.
    //--------------------------------------------------------------------------
    JS_CLASS_INHERIT(MemoryLimitCheckTest, RmTest,
        "Memory Check Limit test.");
