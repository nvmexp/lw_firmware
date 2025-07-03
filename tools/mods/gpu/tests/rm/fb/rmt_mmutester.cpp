/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2005-2015,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

//! \file rmt_mmutester.cpp
//! \brief Tests the memory management with all the possible classes and flags
//!
//! This test basically tests memory allocation by selecting randomly between
//! heap and non-heap allocations. In case of heap allocation, it selects
//! randomly the TYPE of surface and then memory size of heap, and the
//! ATTRIBUTES for the heap allocation, and then associate CTX DMA and
//! map it to sysmem. ALso allocate virtual ctx dma handle or virtual memory object
//! and perform MapDma to get GPU virtual address of physical heap back.
//! In case of NON-HEAP allocations allocate system memory and associating ctxdma
//! handles and maps the memory into sysmem locations considering all the
//! possible classes available and all the possible flags every class can take
//! This is a random test which takes up a starting seed and then generates
//! the classes randomly/loop and the flags and even the size of memory to be a
//! allocated also. By this test, we care able to perform every possible alloc
//! on gpu with every possible attribute it can take for alloc. The test also
//! performs sanity checking functions for memory allocs, DMA allocs, mapmemory
//! mapdma functionality so that exceptions can be caught beforehand. From the
//! perspective of making the size of memory allocation random, this is stress
//! test.But the actual stressing will happen while this is multi-threaded.
//! Right now, the emphasis is on the coverage of every possible combination
//! in the foregoing description.
//! Parameters made random: {memory type(Heap/ System),
//!                          memory class(sysmem classes),
//!                          memory flags, dma flags, map flags, mapdma flags, memory size}
//!
//! MMUTester now also include a way to store specified number of allocated chunk
//! which randomly gets deleted and allocated repetedly.
//! Main intension behind this is to fragment the physical as well as virtual memory space.
//!
//-----------------------------------------------------------------------------
//! What will be done in the next version?
//! --> Once the sanity checking fails for any operation, it will be auto-fixed
//! and continues with the modified flags/parameters.
//! --> Also, the offsets in context DMA and mapmem functions will also be made
//! random and perform some limit checks also.
//! -->Even, the test can facilitate the user to pass, the
//! loop count and start seed as command line arguments.
//! --> This will be made multi-threaded
//! --> In case of exceptions, perform negative testing scenario.
//-----------------------------------------------------------------------------

#include <list>
#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "lwos.h"
#include "random.h"
#include "core/include/platform.h"
#include "lwRmApi.h"
#include "class/cl003e.h" // LW01_MEMORY_SYSTEM
#include "class/cl0040.h" // LW01_MEMORY_LOCAL_USER
#include "class/cl0070.h" // LW01_MEMORY_VIRTUAL
#include "class/cl0071.h" // LW01_MEMORY_SYSTEM_OS_DESCRIPTOR
#include "core/include/xp.h"
#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "ctrl/ctrl0000.h"
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl2080.h"
#include "rmt_mmutester.h"
#include "core/include/memcheck.h"

//! \brief Constructor for MMUTester
//!
//! Just does nothing except setting a name for the test..the actual
//! functionality is done by Setup()
//!
//! \sa Setup
//-----------------------------------------------------------------------------
MMUTester::MMUTester()
{
    SetName("MMUTester");
    m_hMemSystem = 0;
    m_hDma       = 0;
    m_hVideo     = 0;
    m_hVirtual   = 0;
    m_sys_size   = 0;
    m_flags      = 0;
    m_CtxFlags   = 0;
    m_MapFlags   = 0;
    m_count      = 0;
    m_statistic  = 0;
    m_nofb       = 0;
    m_IsChunkSaved = false;
}

//! \brief MMUTester destructor
//!
//! All resources should be freed in the Cleanup member function, so
//! destructors shouldn't do much.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
MMUTester::~MMUTester()
{
}

//! \brief IsTestSupported()
//!
//! Only allowed on GPU >= G80
//----------------------------------------------------------------------------
string MMUTester::IsTestSupported()
{
    if (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_PAGING))
    {
        return RUN_RMTEST_TRUE;
    }
    else
    {
        return "GPUSUB_SUPPORTS_PAGING device feature is not supported on current platform";
    }
}

//! \brief Setup()
//!
//! RM CTRL to get the free space on FB heap
//!
//! return->OK
//!
//--------------------------------------------------------------------------
RC MMUTester::Setup()
{
    RC rc;
    LwRmPtr   pLwRm;
    LW2080_CTRL_FB_GET_INFO_PARAMS FbInfoParams = {0};
    LW2080_CTRL_FB_INFO *pCtrlBlock = NULL;
    UINT32 status;
    LW0080_CTRL_GPU_GET_NUM_SUBDEVICES_PARAMS numSubdevicesParams = {0};

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    // Allocate client handle
    m_hClient = pLwRm->GetClientHandle();

    // Allocate Device handle
    m_hDevice = pLwRm->GetDeviceHandle(GetBoundGpuDevice());
    if (!(Memory::Fb == Surface2D::GetActualLocation(Memory::Fb, GetBoundGpuSubdevice())))
    {
        m_nofb = ABSOLUTE_VALUE_ONE;
        //
        // If there is not FB then we dont want to test the scenario for VIRTUAL_MEMORY_OBJECT.
        // As, in that case we call AllocFBHeap to allocate an object in FB.
        //
        virtualWeights[0] = ABSOLUTE_VALUE_ONE; // DYNAMIC_MEMORY_VIRTUAL
        virtualWeights[1] = ZERO_PROBABILITY;   // VIRTUAL_MEMORY_OBJECT
    }
    else
    {
        // Both scenarios have equal probability to be selected for the test, if FB is available.
        virtualWeights[0] = EQUAL_PROBABILITY;  // DYNAMIC_MEMORY_VIRTUAL
        virtualWeights[1] = EQUAL_PROBABILITY;  // VIRTUAL_MEMORY_OBJECT
    }

    // Allocate Subdevice handle
    m_hSubDev = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());

    FbInfoParams.fbInfoListSize = 1;

    pCtrlBlock = (LW2080_CTRL_FB_INFO *)malloc(sizeof(LW2080_CTRL_FB_INFO));
    if (NULL == pCtrlBlock)
    {
        Printf(Tee::PriHigh, "%s, Failed to allocate memory %d",\
                             __FILE__, __LINE__);
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    pCtrlBlock->index = LW2080_CTRL_FB_INFO_INDEX_HEAP_SIZE;

    FbInfoParams.fbInfoList = (LwP64)pCtrlBlock;

    status = LwRmControl(m_hClient, m_hSubDev,
                        LW2080_CTRL_CMD_FB_GET_INFO,
                        &FbInfoParams,
                        sizeof (LW2080_CTRL_FB_GET_INFO_PARAMS));

    if (status != LW_OK)
    {
        CHECK_RC_CLEANUP(RmApiStatusToModsRC(status));
    }

    //HeapMaximum = (pCtrlBlock->data) * IN_KB;

    //
    // Making this to min of (size reported by FB_INFO_INDEX_HEAP_SIE and 100MB) because
    // for greater sizes, MODS shows a resource leakage, suspecting MODS caps
    //
    HeapMaximum = MINIMUM(pCtrlBlock->data * IN_KB, 0x6400000);
    Printf(Tee::PriLow, "Heap max reported by LW2080_CTRL_FB_INFO_INDEX_HEAP_SIZE = 0x%x KB,\
                         capping it to 0x%x KB\n", (UINT32)pCtrlBlock->data , HeapMaximum/1024);

    CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                            LW0080_CTRL_CMD_GPU_GET_NUM_SUBDEVICES,
                            &numSubdevicesParams,
                            sizeof (numSubdevicesParams)));
    m_NumSubdevices = numSubdevicesParams.numSubDevices;

Cleanup:
        free(pCtrlBlock);
        pCtrlBlock = NULL;

    return rc;
}

//! \brief Run()
//!
//!   This is the core functionality of the test. As the test deals
//! the allocation of memory (heap/system) and in case of system memory
//! associates a context dma handle to the allocated memory
//! and map it thereafter, the functionality is embedded in Run()
//! rather than in Setup().
//!   This is a random test starts with a initial seed and get the memory type
//! (heap/system), memory class to be allocated
//! as random number. Also the flags that are to be taken
//! in the AllocMemory() are also generated randomly. Then perform allocation.
//! Next, associate the context handle using function AllocateCtxDma() and map
//! the handle using Mapmemory(). Even while ContextDma'ing and Mapping also,
//! the attributes and access rights and every flag is made random.
//!    In case of heap allocation, the TYPE of surface allocated and ATTRIBUTES
//! for the heap allocations are made random. Also the cases where the heap
//! allocation fails are caught beforehand by SanityHeapCheckerParams().
//! After heap is alloc'ed, then ctx dma it and map it to sysmem. Then do mapdma.
//!    We have also come up with sanity checking functions for some exceptions
//! like AGP not supported on PCI cards, OS_DESCRIPTOR fails an assertion
//! while MmProbeAndLockPages(), PHYSICAL memory class cannot be ContextDma'ed
//! for AllocMemory(), sanity checking for AllocContextDma() and
//! sanity checking for Mapmemory() which first checks
//! for any exceptions only after which performs the actual operations.
//!
//! return->OK when every operation succeeded, else return->RC error
//! \sa AllocateContextDma()
//! \sa Mapmemory()
//! \sa SanityCheckerMemParams()
//! \sa SanityCheckerDmaParams()
//! \sa SanityCheckerMapParams()
//! \sa SanityHeapCheckerParams()
//-----------------------------------------------------------------------------
RC MMUTester::Run()
{
    LwRmPtr pLwRm;
    RC rc;
    m_flags = 0;
    UINT32 memType = 0;
    UINT64 startSeed;
    UINT32 allocMemoryFlagsToRandomize;

#ifdef LW_VERIF_FEATURES
    Printf(Tee::PriHigh,"\n LW_VERIF_FEATURE ENABLED\n");
#else
    Printf(Tee::PriHigh,"\n LW_VERIF_FEATURE DISABLED\n");
#endif

    // INITIAL SEED
#if 0
    startSeed = Xp::QueryPerformanceCounter();
    m_Random.SeedRandom((UINT32)startSeed);
#endif

    startSeed = m_TestConfig.Seed();
    m_Random.SeedRandom(m_TestConfig.Seed());

    Printf(Tee::PriHigh,"MMUTester: Using start seed 0x%x\n",
                (UINT32)startSeed);
    Printf(Tee::PriHigh, "MMUUtil using start seed 0x%x\n",
                m_MmuUtil.getSeed());

    if( ALLOCS_TO_KEEP < ALLOCS_TO_FREE )
    {
        Printf(Tee::PriHigh,"\n ALLOCS_TO_KEEP should never be less than ALLOCS_TO_FREE \n");
        return RC::LWRM_ILWALID_DATA;
    }
    for( m_count = 0, m_IsChunkSaved = false ; m_count < MAX_LOOPS ; m_count++, m_IsChunkSaved = false )
    {
        memType = m_MmuUtil.getWeightedSamples(memTypeWeights, MAX_MEMORY_TYPES);

        // If stored chunks reached to desired amount delete few of them
        if ( !(m_memChunkStore.size() < ALLOCS_TO_KEEP) )
        {
            CHECK_RC(DelMemChunk());
        }
        if (m_nofb && memType == MEM_TYPE_HEAP)
            continue;

        switch(memType)
        {
            // NON_HEAP allocations
            case MEM_TYPE_SYSTEM:
                //
                // Filling the FLAGS field and the CLASS field
                //
                m_sys_size = m_Random.GetRandom64(LOWER_LIMIT, UPPER_LIMIT_SYS);
                m_MemClass = m_MmuUtil.RandomizeAllocMemoryClasses(weights);

                // Choose which flags to randomize for AllocMemory()
                allocMemoryFlagsToRandomize = (MMUUTIL_ALLOC_SYSMEM_FLAGS_TO_RANDOMIZE_PHYSICALITY |
                                               MMUUTIL_ALLOC_SYSMEM_FLAGS_TO_RANDOMIZE_LOCATION    |
                                               MMUUTIL_ALLOC_SYSMEM_FLAGS_TO_RANDOMIZE_COHERENCY);

                // Get randomized flags from utility
                m_flags = m_MmuUtil.RandomizeAllocMemoryFlags(weights, allocMemoryFlagsToRandomize);

                // Disabling default mapping so that no mapping unless specified
                // with particular flags
                m_flags = m_flags | DRF_DEF(OS02, _FLAGS, _MAPPING, _NO_MAP);

                //
                // SanityChecking for memory allocations
                //
                if(!SanityCheckerMemParams())
                {
                    //
                    // without losing iteration
                    //
                    m_count--;
                    m_statistic++;
                    continue;
                }

                rc = pLwRm->AllocMemory(&m_hMemSystem, m_MemClass, m_flags, NULL, &m_sys_size, GetBoundGpuDevice());

                //If sysmem allocation failed because of insufficiency of
                //resources try allocating smaller size ( say 1MB ).
                //Even that also fails then return RC error else proceed
                //through the test.This is test hack

                if(rc == RC::LWRM_INSUFFICIENT_RESOURCES)
                {
                    if(m_sys_size > FEASIBLE_SIZE)
                    {
                        m_sys_size = FEASIBLE_SIZE;
                        CHECK_RC_CLEANUP(pLwRm->AllocMemory(
                            &m_hMemSystem, m_MemClass,
                        m_flags, NULL, &m_sys_size,
                        GetBoundGpuDevice()));
                    }
                    else
                    {
                        return rc;
                    }

                }
                if(rc != OK)
                {
                    goto Cleanup;
                }
                //
                // Allocate context dma to the memory handle allocated
                //
                CHECK_RC_CLEANUP(AllocateCtxDma(m_count, memType, false));

                CHECK_RC_CLEANUP(Mapmemory(m_count, memType));

                CHECK_RC_CLEANUP(VirtualMemTest(m_count, memType));

                if ( !m_IsChunkSaved )
                {
                    pLwRm->Free(m_hDma);
                    pLwRm->Free(m_hVideo);
                    pLwRm->Free(m_hMemSystem);
                    pLwRm->Free(m_hVirtual);
                }
                m_hDma = 0;
                m_hVideo = 0;
                m_hMemSystem = 0;
                m_hVirtual = 0;

            break;

            // Heap allocation
            case MEM_TYPE_HEAP:
                //
                // Select randomly the TYPE of surface to be allocated
                //
                CHECK_RC_CLEANUP(AllocFbHeap(false, memType));

                if(!SanityCheckerHeapParams())
                {
                    m_count--;
                    m_statistic++;
                    continue;
                }

                // Allocate context dma to the memory handle allocated
                CHECK_RC_CLEANUP(AllocateCtxDma(m_count, memType, false));

                CHECK_RC_CLEANUP(Mapmemory(m_count, memType));

                CHECK_RC_CLEANUP(VirtualMemTest(m_count, memType));

                if ( !m_IsChunkSaved )
                {
                    pLwRm->Free(m_hDma);
                    pLwRm->Free(m_hVideo);
                    pLwRm->Free(m_hMemSystem);
                    pLwRm->Free(m_hVirtual);
                }
                m_hDma = 0;
                m_hVideo = 0;
                m_hMemSystem = 0;
                m_hVirtual = 0;
            break;

            default:
                Printf(Tee::PriHigh,"\nIlwalid MEMORY TYPE\n");
                goto Cleanup;
            break;

        }

        // Printf(Tee::PriHigh, "Loop is %d\n", m_count); // for debugging
    }

    CHECK_RC_CLEANUP (FbMapApertureToFail());
    pLwRm->Free(m_hVideo);
    m_hVideo = 0;
    return rc;
Cleanup:
    switch(memType)
    {
        case MEM_TYPE_SYSTEM:
            pLwRm->Free(m_hDma);
            pLwRm->Free(m_hMemSystem);
            pLwRm->Free(m_hVirtual);
            pLwRm->Free(m_hVideo);
            m_hMemSystem = 0;
            m_hDma = 0;
            m_hVirtual = 0;
            m_hVideo = 0;

            Printf(Tee::PriHigh,
                   "\nError oclwred when loop is %d and size is %lld",
                   m_count, m_sys_size);
            m_MmuUtil.PrintAllocMemoryClasses(Tee::PriHigh, " and Class is ", m_MemClass);
            m_MmuUtil.PrintAllocMemoryFlags(Tee::PriHigh, " and flags are : ", m_flags);

            return rc;
        break;

        case MEM_TYPE_HEAP:
            pLwRm->Free(m_hDma);
            pLwRm->Free(m_hVideo);
            pLwRm->Free(m_hVirtual);
            pLwRm->Free(m_hMemSystem);
            m_hMemSystem = 0;
            m_hVideo = 0;
            m_hDma = 0;
            m_hVirtual = 0;

            return rc;
        break;

        default:
            Printf(Tee::PriHigh,"\nIlwalid Field\n");
            pLwRm->Free(m_hDma);
            pLwRm->Free(m_hMemSystem);
            pLwRm->Free(m_hVideo);
            pLwRm->Free(m_hVirtual);
            m_hMemSystem = 0;
            m_hDma = 0;
            m_hVideo = 0;
            m_hVirtual = 0;

            return RC::LWRM_ERROR;
        break;
    }
    return rc;
}

//! \brief Cleanup()
//!
//! Actual freeing takes place in Run()
//! If handles are not NULL, then free them, else do nothing.
//!
//! \sa Setup()
//----------------------------------------------------------------------------
RC MMUTester::Cleanup()
{
    LwRmPtr pLwRm;
    ALLOCED_MEM node;
    std::list<ALLOCED_MEM>::iterator freeAllocedindex;

    PrintSavedChunkInfo();

    while(m_memChunkStore.size())
    {
        freeAllocedindex = m_memChunkStore.begin();
        node = *(freeAllocedindex);
        if (node.MEMTYPE == MEM_TYPE_SYSTEM)
        {
            pLwRm->UnmapMemoryDma(node.hVirtual, node.hMemSystem,
                                  node.mapDmaFlags, node.gpuAddr, GetBoundGpuDevice());
        }
        else
        {
            pLwRm->UnmapMemoryDma(node.hVirtual, node.hVideo,
                                  node.mapDmaFlags, node.gpuAddr, GetBoundGpuDevice());
        }
        pLwRm->Free(node.hDma);
        pLwRm->Free(node.hVideo);
        pLwRm->Free(node.hMemSystem);
        pLwRm->Free(node.hVirtual);
        m_memChunkStore.erase(freeAllocedindex);
    }

    pLwRm->Free(m_hDma);
    pLwRm->Free(m_hMemSystem);
    pLwRm->Free(m_hVideo);
    pLwRm->Free(m_hVirtual);
    m_hMemSystem = 0;
    m_hDma = 0;
    m_hVideo = 0;
    m_hVirtual = 0;

    Printf(Tee::PriHigh,"\nThe loops ran even with regeneration is %d and \
                        loops which actually skipped are %d\n", m_count,
                        m_statistic);

    return OK;
}
//-------------------------------------------------------------------------
// PRIVATE MEMBER FUNCTIONS
//-------------------------------------------------------------------------
//! \brief Associate context dma for the memory allocated
//!
//! Get the flags for the AssignFbHeapAttr() randomly
//! Assign random attributes to VidHeapAlloc call
//! ALso sanity check the assigned attributes
//!
//! return->OK when succeded, else return->RC error
//!
//! \sa SanityCheckerHeapParams()
//----------------------------------------------------------------------------
RC MMUTester::AllocFbHeap(bool m_Virtual, UINT32 memType)
{
    LwRmPtr pLwRm;
    LwRm::Handle *handle = NULL;
    RC rc;
    UINT32 allocHeapAttrFlagsToRandomize;
    UINT32 allocHeapAttr2FlagsToRandomize;

    // Randomize vidType using utility
    m_VidType = m_MmuUtil.RandomizeAllocHeapTypes(weights);

    // Choose which flags in ATTR to be randomized
    allocHeapAttrFlagsToRandomize = (MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_DEPTH           |
                                     MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_COMPR_COVG      |
                                     MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_AA_SAMPLES      |
                                     MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_COMPR           |
                                     MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_FORMAT          |
                                     MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_Z_TYPE          |
                                     MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_ZS_PACKING      |
                                     MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_FORMAT_OVERRIDE |
                                     MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_PAGE_SIZE_HEAP  |
                                     MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_LOCATION_HEAP   |
                                     MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_PHYSICALITY_HEAP|
                                     MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_COHERENCY_HEAP);

    // Get randomized flags from utility
    m_VidAttr = m_MmuUtil.RandomizeAllocHeapAttrFlags(weights, allocHeapAttrFlagsToRandomize);

    // Choose which flags in ATTR2 to be randomized
    allocHeapAttr2FlagsToRandomize= (MMUUTIL_ALLOC_HEAP_ATTR2_FLAGS_TO_RANDOMIZE_ZBC              |
                                     MMUUTIL_ALLOC_HEAP_ATTR2_FLAGS_TO_RANDOMIZE_GPU_CACHEABLE    |
                                     MMUUTIL_ALLOC_HEAP_ATTR2_FLAGS_TO_RANDOMIZE_P2P_GPU_CACHEABLE);

    // Get randomized flags from utility
    m_VidAttr2 = m_MmuUtil.RandomizeAllocHeapAttr2Flags(weights, allocHeapAttr2FlagsToRandomize);

    // Sanity checking the input parameters
    // if false, then skip the iteration
    if(!SanityCheckerHeapParams())
    {
        return rc;
    }

    // Non-contiguous 64KB pages fail in MapMemoryDma(), so change them to 4KB
    if (FLD_TEST_DRF(OS32, _ATTR, _PHYSICALITY, _NONCONTIGUOUS, m_VidAttr))
    {
        if (FLD_TEST_DRF(OS32, _ATTR, _FORMAT, _SWIZZLED, m_VidAttr))
        {
            m_VidAttr = FLD_SET_DRF(OS32, _ATTR, _FORMAT, _PITCH, m_VidAttr);
        }
    }

    if (m_Virtual)
    {
        UINT64 valignment;
        UINT64 vlimit;

        if (memType == MEM_TYPE_SYSTEM)
        {
            valignment = 0;
            vlimit = m_sys_size - 1;
        }
        else
        {
            //
            // Allocation does not return physical alignment.  Infer virtual alignment
            // that may be required based on physical alignment.
            //
            if (m_vidOffset & 0x1fffffff)
                valignment = 0x20000000;    // 512MB
            else if (m_vidOffset & 0x1fffff)
                valignment = 0x200000;      // 2MB
            else if (m_vidOffset & 0x1ffff)
                valignment = 0x20000;       // 128KB
            else if (m_vidOffset & 0xffff)
                valignment = 0x10000;       // 64KB
            else
                valignment = 0;
            vlimit = m_video_size - 1;
        }

        // Non-contiguous 64KB pages fail in MapMemoryDma(), so change them to 4KB
        if (FLD_TEST_DRF(OS32, _ATTR, _PHYSICALITY, _NONCONTIGUOUS, m_VidAttr))
        {
            m_VidAttr = FLD_SET_DRF(OS32, _ATTR, _PAGE_SIZE, _4KB, m_VidAttr);
        }

        CHECK_RC_CLEANUP(pLwRm->VidHeapAllocSizeEx(m_VidType,
                                            LWOS32_ALLOC_FLAGS_VIRTUAL |
                                               (valignment ? LWOS32_ALLOC_FLAGS_ALIGNMENT_HINT : 0),
                                            &m_VidAttr,
                                            &m_VidAttr2, // pAttr2
                                            vlimit,
                                            valignment,
                                            NULL, // pFormat
                                            NULL, // pCoverage
                                            NULL, // pPartitionStride
                                            &m_hVirtual,
                                            NULL, // poffset,
                                            NULL, // pLimit
                                            NULL, // pRtnFree
                                            NULL, // pRtnTotal
                                            0,    // width
                                            0,    // height
                                            GetBoundGpuDevice()));
        return rc;
    }
    //
    // select size based on whether the memory we are trying to allocate is
    // video memory or system memory. _LOCATION_VIDMEM translates to video mem
    // while all others (_PCI/_AGP/_ANY) translate to sysmem
    // We are saving the size of allocation and the allocation handle in
    // m_video_size and m_hVideo always, even though _LOCATION flag translates
    // to sysmem, as these handles are used in the later part of the code
    // and saving to sysmem handles causes conflict later.
    // TODO : Cleanup the code and flow control once we move the randomizing
    // part to the utility.
    //
    if (FLD_TEST_DRF(OS32, _ATTR, _LOCATION, _VIDMEM, m_VidAttr))
    {
        LWOS32_PARAMETERS memInfo = {0};
        UINT64 freeMem = 0;

        UINT64 total, totalfree;

        memInfo.hRoot         = m_hClient;
        memInfo.hObjectParent = m_hDevice;
        memInfo.function = LWOS32_FUNCTION_INFO;
        memInfo.data.Info.attr = DRF_DEF(OS32,_ATTR,_LOCATION,_VIDMEM);
        CHECK_RC_CLEANUP(RmApiStatusToModsRC(LwRmVidHeapControl(&memInfo)));
        freeMem = LW_ALIGN_DOWN64(memInfo.data.Info.size , 64 * 1024);

        //
        // Put a cap on physical allocation to limit mapping overhead
        // and increasing runtime substantially.  Capping the physical
        // allocation allows random sizes to be distributed over
        // the range.
        //
        freeMem = LW_MIN(freeMem, UPPER_LIMIT_VID);

        if(freeMem <= LOWER_LIMIT)
        {
            Printf(Tee::PriHigh,"Free memory is less than the lower limit");
            if(freeMem != 0)
                m_video_size = LOWER_LIMIT;
            else
            {
                total = memInfo.total;
                totalfree= memInfo.free;
                rc = RC::LWRM_INSUFFICIENT_RESOURCES;
                Printf(Tee::PriHigh,"\n Free memory = 0 , total = %lld , totalfree =  %lld \n", total, totalfree);
                goto Cleanup;
            }
        }
        else
        {
            m_video_size = m_Random.GetRandom64(LOWER_LIMIT, freeMem);
        }
    }
    else
    {
        m_video_size = m_Random.GetRandom64(LOWER_LIMIT, UPPER_LIMIT_SYS);
    }
    handle = &m_hVideo;

    rc = pLwRm->VidHeapAllocSizeEx(m_VidType,
                                   0,
                                   &m_VidAttr,
                                   &m_VidAttr2,
                                   &m_video_size,
                                   nullptr,
                                   handle,
                                   &m_vidOffset,
                                   nullptr,
                                   GetBoundGpuDevice(),
                                   0, 0, LW01_NULL_OBJECT);

    if(rc == RC::LWRM_INSUFFICIENT_RESOURCES)
    {
        // try allocating in case of failure with atleast 1MB
        // this is just a hack as dislwssed in the foregoing
        // comments
        Printf(Tee::PriLow,"\n Insufficient resources...trying for the second time\n");
        if(m_video_size > FEASIBLE_SIZE)
        {
            m_video_size = FEASIBLE_SIZE;
            CHECK_RC_CLEANUP(pLwRm->VidHeapAllocSizeEx(m_VidType,
                                0,
                                &m_VidAttr,
                                &m_VidAttr2,
                                &m_video_size,
                                nullptr,
                                handle,
                                &m_vidOffset,
                                nullptr,
                                GetBoundGpuDevice(),
                                0, 0, LW01_NULL_OBJECT));
        }
        else
        {
            goto Cleanup;
        }
    }

    if(rc != OK)
    {
        goto Cleanup;
    }
    return rc;

Cleanup:
    pLwRm->Free(m_hDma);
    pLwRm->Free(m_hVideo);
    pLwRm->Free(m_hVirtual);
    pLwRm->Free(m_hMemSystem);
    m_hMemSystem = 0;
    m_hVideo = 0;
    m_hDma = 0;
    m_hVirtual = 0;
    Printf(Tee::PriHigh,"\n Error oclwred on loop  %d ...",m_count);
    m_MmuUtil.PrintAllocHeapVidType(Tee::PriHigh,"heap with TYPE is :", m_VidType);
    m_MmuUtil.PrintAllocHeapAttrFlags(Tee::PriHigh,"and Attributes are :", m_VidAttr);
    m_MmuUtil.PrintAllocHeapAttr2Flags(Tee::PriHigh,"and Attributes are :", m_VidAttr2);

    return rc;
}

//! \brief Map the handle to sysmem locations with a priori sanity check
//!
//! Get the flags for the Mapmemory(Dma)() randomly
//! Perform the sanity check for any exceptions by SanityCheckerMapParams()
//! In case of LOCAL_USER, the size is set as 1MB
//! as it is feasible size for the videomemory.
//!
//! return->OK when succeded, else return->RC error
//!
//! \sa SanityCheckerMapParams()
//! \sa AllocateContextDma()
//----------------------------------------------------------------------------
RC MMUTester::Mapmemory(UINT32 m_count, UINT32 memType)
{
    LwRmPtr pLwRm;
    RC rc;
    UINT32 mapMemoryFlagsToRandomize;
    void *pMapAddr;

    // Choose which flags to randomize for MapMemory()
    mapMemoryFlagsToRandomize = (MMUUTIL_MAP_MEMORY_FLAGS_TO_RANDOMIZE_ACCESS         |
                                 MMUUTIL_MAP_MEMORY_FLAGS_TO_RANDOMIZE_SKIP_SIZE_CHECK );

    // Get randomized flags from utility.
    m_MapFlags = m_MmuUtil.RandomizeMapMemoryFlags(weights, mapMemoryFlagsToRandomize);
    //
    // SanityChecking for Mapping functionality
    //
    if(!SanityCheckerMapParams())
    {
        m_count--;
        m_statistic++;
        return rc;
    }
    if(memType == MEM_TYPE_SYSTEM)
    {
        // Do not want to map more than BAR1 size, if the chip still supports reflection
        LwU64 mapsize = LW_MIN(m_sys_size, FEASIBLE_SIZE);

        CHECK_RC_CLEANUP(pLwRm->MapMemory(m_hMemSystem,
                                        0,
                                        mapsize,
                                        &pMapAddr,
                                        m_MapFlags,
                                        GetBoundGpuSubdevice())
                                        );
        pLwRm->UnmapMemory(m_hMemSystem, pMapAddr, 0, GetBoundGpuSubdevice());
    }
    else
    {
        // Do not want to map more than BAR1 size
        LwU64 mapsize = LW_MIN(m_video_size, FEASIBLE_SIZE);

        CHECK_RC_CLEANUP(pLwRm->MapMemory(m_hVideo,
                                        0,
                                        mapsize,
                                        &pMapAddr,
                                        m_MapFlags,
                                        GetBoundGpuSubdevice())
                                        );
        pLwRm->UnmapMemory(m_hVideo, pMapAddr, 0, GetBoundGpuSubdevice());
    }

    return rc;
Cleanup:
    //
    // If DYNAMIC, free memory and Dma handles
    // else only memory handle
    //
    pLwRm->Free(m_hDma);
    pLwRm->Free(m_hMemSystem);
    pLwRm->Free(m_hVideo);
    pLwRm->Free(m_hVirtual);
    m_hMemSystem = 0;
    m_hDma = 0;
    m_hVideo = 0;
    m_hVirtual = 0;

    if (memType == MEM_TYPE_SYSTEM)
    {
        Printf(Tee::PriHigh,
               "Error oclwred on loop %d because of SYS MEM size is %lld",
               m_count, m_sys_size);
    }
    else
    {
        Printf(Tee::PriHigh,
            "Error oclwred on loop %d because of FB size is %llx",
            m_count, m_video_size);
    }
    m_MmuUtil.PrintMapMemoryFlags(Tee::PriHigh, "and flags are : ", m_MapFlags);
    return rc;
}
//! \brief MapDma to get GPU virtual address back with apriori sanitycheck.
//!
//! Get the flags for the MapmemoryDma() randomly
//! Perform the sanity check for any exceptions by SanityCheckerMapDmaParams()
//! and perform MapMemoryDma() on it.
//!
//! return->OK when succeded, else return->RC error
//!
//! \sa SanityCheckerMapDmaParams()
//! \sa AllocateContextDma()
//----------------------------------------------------------------------------
RC MMUTester::MapmemoryDma(UINT32 m_count, UINT32 memType, UINT32 virtualType)
{
    LwRmPtr pLwRm;
    RC rc;
    UINT64 pGpuAddr = 0;
    m_MapDmaFlags = 0;
    UINT32 mapMemoryDmaFlagsToRandomize;

    // Choose which flags to be randomized upon
    mapMemoryDmaFlagsToRandomize = (MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_ACCESS                 |
                                    MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_CACHE_SNOOP            |
                                    MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_PAGE_SIZE              |
                                    MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_PRIV                   |
                                    MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_DMA_OFFSET_GROWS       |
                                    MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_DMA_OFFSET_FIXED       |
                                    MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_PTE_COALESCE_LEVEL_CAP |
                                    MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_TLB_LOCK);

    if (m_NumSubdevices >= MIN_SUBDEVICES_FOR_SLI)
    {
        mapMemoryDmaFlagsToRandomize |= MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_P2P_ENABLE;
    }

    // Get randomized flags from utility
    m_MapDmaFlags = m_MmuUtil.RandomizeMapMemoryDmaFlags(weights, mapMemoryDmaFlagsToRandomize);
    //
    // SanityChecking for Mapping functionality
    //
    if(!SanityCheckerMapDmaParams(memType, virtualType))
    {
        m_count--;
        m_statistic++;
        return rc;
    }

    if(memType == MEM_TYPE_SYSTEM)
    {
        CHECK_RC_CLEANUP(pLwRm->MapMemoryDma(m_hVirtual,
                                            m_hMemSystem,
                                            0,
                                            m_sys_size,
                                            m_MapDmaFlags,
                                            &pGpuAddr,
                                            GetBoundGpuDevice()));

        // Do we need to store this chunk? If yes save it.
        if(m_memChunkStore.size() < ALLOCS_TO_KEEP)
        {
             SaveMemChunk(memType, pGpuAddr);
        }
        else
        {
            pLwRm->UnmapMemoryDma(m_hVirtual, m_hMemSystem,
                              m_MapDmaFlags, pGpuAddr, GetBoundGpuDevice());
        }
    }
    else
    {
        CHECK_RC_CLEANUP(pLwRm->MapMemoryDma(m_hVirtual,
                                            m_hVideo,
                                            0,
                                            m_video_size,
                                            m_MapDmaFlags,
                                            &pGpuAddr,
                                            GetBoundGpuDevice()));

        // Do we need to save the chunk? If yes, save it
        if(m_memChunkStore.size() < ALLOCS_TO_KEEP)
        {
           SaveMemChunk(memType, pGpuAddr);
        }
        else
        {
            pLwRm->UnmapMemoryDma(m_hVirtual, m_hVideo,
                              m_MapDmaFlags, pGpuAddr, GetBoundGpuDevice());
        }
    }

    return rc;
Cleanup:
    //
    // If DYNAMIC, free memory and Dma handles
    // else only memory handle
    //
    pLwRm->Free(m_hDma);
    pLwRm->Free(m_hMemSystem);
    pLwRm->Free(m_hVideo);
    pLwRm->Free(m_hVirtual);
    m_hMemSystem = 0;
    m_hDma = 0;
    m_hVideo = 0;
    m_hVirtual = 0;

    if (memType == MEM_TYPE_SYSTEM)
    {
        Printf(Tee::PriHigh,
               "Error oclwred on VIRTUAL MAPPING DMA because of SYS MEM size "
               "is %lld", m_sys_size);
    }
    else
    {
        Printf(Tee::PriHigh,
            "Error oclwred on VIRTUAL MAPPING DMA because of FB size is %llx",
            m_video_size);
    }
    m_MmuUtil.PrintMapMemoryDmaFlags(Tee::PriHigh, "\and flags are : ", m_MapDmaFlags);

    return rc;
}
//! \brief Associate context dma for the memory allocated
//!
//! Get the flags for the AllocContextDma() randomly
//! Perform the sanity check for any exceptions by SanityCheckerDmaParams()
//! The size for ContextDma'ing is set as 1MB feasible for frame buffer
//!
//! return->OK when succeded, else return->RC error
//!
//! \sa SanityCheckerDmaParams()
//! \sa Mapmemory()
//----------------------------------------------------------------------------
RC MMUTester::AllocateCtxDma(UINT32 m_count, UINT32 memType, bool m_Virtual)
{
    LwRmPtr pLwRm;
    UINT32 allocCtxDmaFlagsToRandomize;
    RC rc;
    m_CtxFlags = 0;

    // Choose which flags to be randomized upon
    allocCtxDmaFlagsToRandomize = ( MMUUTIL_ALLOC_CTX_DMA_FLAGS_TO_RANDOMIZE_ACCESS                |
                                    MMUUTIL_ALLOC_CTX_DMA_FLAGS_TO_RANDOMIZE_PREALLOCATE           |
                                    MMUUTIL_ALLOC_CTX_DMA_FLAGS_TO_RANDOMIZE_GPU_MAPPABLE          |
                                    MMUUTIL_ALLOC_CTX_DMA_FLAGS_TO_RANDOMIZE_TYPE                  |
                                    MMUUTIL_ALLOC_CTX_DMA_FLAGS_TO_RANDOMIZE_HASH_TABLE);

    // Get randomized flags from utility
    m_CtxFlags = m_MmuUtil.RandomizeAllocCtxDmaFlags(weights, allocCtxDmaFlagsToRandomize);

    //
    // sanityChecking for Dma context allocation
    //
    if(!SanityCheckerDmaParams())
    {
        return rc;
    }

    if (!m_Virtual)
    {
        if (memType == MEM_TYPE_SYSTEM)
        {
            CHECK_RC_CLEANUP(pLwRm->AllocContextDma(&m_hDma, m_CtxFlags,
                                m_hMemSystem, 0, m_sys_size - 1));
        }
        else
        {
            CHECK_RC_CLEANUP(pLwRm->AllocContextDma(&m_hDma, m_CtxFlags,
                                m_hVideo, 0, m_video_size - 1));
        }
    }
    else
    {
        // No longer require a ContextDma, m_hVirtual is initialized already
    }

    return rc;

Cleanup:

    if (!m_Virtual)
    {
        if (memType == MEM_TYPE_SYSTEM)
        {
            Printf(Tee::PriHigh,
                   " Error oclwred when loop is %d and  SYS MEM size is %lld",
                   m_count, m_sys_size);
        }
        else
        {
            Printf(Tee::PriHigh," Error oclwred when loop is %d and FB size is %llx",
                   m_count, m_video_size);
        }
    }
    m_MmuUtil.PrintAllocCtxDmaFlags(Tee::PriHigh,"and flags are :",m_CtxFlags);

    pLwRm->Free(m_hMemSystem);
    pLwRm->Free(m_hVideo);
    m_hMemSystem = 0;
    m_hVideo = 0;
    return rc;
}
//! \brief VirtualMemTest()
//!
//! Does all virtualisation. There are 2 types of getting GPU
//! virtual address back-> DYNAMIC MEMORY HANDLE, VIRTUAL MEMORY OBJECT
//! In the former, we will have  actx dma to a DYNAMIC MEMORY and then use
//! it in MapMemoryDma() while the latter uses a VIRTUAL HEAP to do MapMemoryDma()
//! Selection of these is also random. But before performing any api, first
//! we are passing them through SanityCheckers and then proceed.
//!
//! \sa Run()
//----------------------------------------------------------------------------
RC MMUTester::VirtualMemTest(UINT32 m_count, UINT32 memType)
{
    LwRmPtr pLwRm;
    RC rc;
    UINT32 virtualType;

    virtualType = m_MmuUtil.getWeightedSamples(virtualWeights, VIRTUAL_TYPES);

    switch(virtualType)
    {
        case DYNAMIC_MEMORY_VIRTUAL:
        {
            if(!SanityCheckerMemParams())
            {
                return rc;
            }

            LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vmparams = { 0 };
            CHECK_RC_CLEANUP(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                                          &m_hVirtual, LW01_MEMORY_VIRTUAL, &vmparams));

            CHECK_RC_CLEANUP(AllocateCtxDma(m_count, memType, true));
            if (!SanityCheckerDmaParams())
            {
                return rc;
            }
            break;
        }

        case VIRTUAL_MEMORY_OBJECT:
        {
            CHECK_RC_CLEANUP(AllocFbHeap(true, memType));

            if(!SanityCheckerHeapParams())
            {
                return rc;
            }

            if (rc != OK)
            {
                Printf(Tee::PriHigh,"\n Error oclwred while alloc'ing VIRTUAL HEAP \
                                with TYPE is %d and Attributes are ",
                                m_VidType);
                return rc;
            }
            break;
        }

        default:
            Printf(Tee::PriHigh, "Error in VIRTUAL MEMORY OPTION\n");
            return RC::LWRM_ERROR;
            break;
    }
    CHECK_RC_CLEANUP(MapmemoryDma(m_count, memType, virtualType));

    return rc;

Cleanup:

    Printf(Tee::PriHigh," Error oclwred on VIRTUAL MEMORY TESTING\n");

    return rc;
}
//! \brief FbMapApertureToFail()
//!
//! check the failure path of dmaAllocVASpace() if it is silently returning
//! without freeing the unallocated memory hence preventing stomp-thru of PTE's
//! (see Bug 421237)
//!
//! We get a failure by requesting an address below the first value VA.
//!
//! \sa Mapmemory()
//----------------------------------------------------------------------------
RC MMUTester::FbMapApertureToFail()
{
    LwRmPtr pLwRm;
    LwU64 offset = 0x1000;
    LwU32 attr = 0;
    LwU32 attr2 = 0;
    RC rc;

    // Disable breakpoints for negative case.
    Platform::DisableBreakPoints nobp;

    // Allocate virtual surface outside the valid VA range
    rc = pLwRm->VidHeapAllocSizeEx(m_VidType,
                                   LWOS32_ALLOC_FLAGS_VIRTUAL | LWOS32_ALLOC_FLAGS_FIXED_ADDRESS_ALLOCATE,
                                   &attr,
                                   &attr2, // pAttr2
                                   0x100000,
                                   0,
                                   NULL, // pFormat
                                   NULL, // pCoverage
                                   NULL, // pPartitionStride
                                   &m_hVirtual,
                                   &offset, // poffset,
                                   NULL, // pLimit
                                   NULL, // pRtnFree
                                   NULL, // pRtnTotal
                                   0,    // width
                                   0,    // height
                                   GetBoundGpuDevice());

    if (rc != OK)
    {
        Printf(Tee::PriLow,"dmaAllocVASpace() failure path is tested\n");
        rc = OK;
    }
    else
    {
        Printf(Tee::PriHigh, "%s: failure path succeeded unexpectedly, hence FAILING",
               __FUNCTION__);
        rc = RC_ERROR;
    }

    return rc;
}
//! \brief SanityCheckerMemParams()
//!
//! checks for the exceptions for memory classes and flags to be allocated
//!
//! \sa Setup()
//----------------------------------------------------------------------------
bool MMUTester::SanityCheckerMemParams()
{
    if(m_nofb && m_MemClass ==  LW01_MEMORY_LOCAL_USER)
    {
        if (MMU_TEST_DEBUG)
        {
            Printf(Tee::PriLow,
                "There is no FB present.Must be a tAURORA seriers chip.\n");
        }
        return false;
    }
    if(m_MemClass ==  LW01_MEMORY_SYSTEM_OS_DESCRIPTOR)
    {
        if (MMU_TEST_DEBUG)
        {
            Printf(Tee::PriLow,
                "OS_Descriptor is producing exception in MmProbeAndLockPages()\n");
        }
        return false;

    }
    return true;

}

//! \brief SanityCheckerDmaParams()
//!
//! checks for the exceptions for context Dma flags and
//! associated memory classes/flags
//!
//! \sa Setup()
//! \sa AllocateContextDma()
//----------------------------------------------------------------------------
bool MMUTester::SanityCheckerDmaParams()
{
    //
    // No KERNEL_MAPPING flag as it causes an assertion fail
    //
    if(DRF_VAL(OS03, _FLAGS, _MAPPING, m_CtxFlags) ==
                                LWOS03_FLAGS_MAPPING_KERNEL)
    {
        if (MMU_TEST_DEBUG)
        {
            Printf(Tee::PriLow, "KERNEL_MAPPING enabled causes an assertion\n");
        }
        return false;
    }

    if((DRF_VAL(OS03, _FLAGS, _CACHE_SNOOP, m_CtxFlags) ==
                                LWOS03_FLAGS_CACHE_SNOOP_ENABLE) &&
                                m_nofb)
    {
        return false;
    }

    return true;
}

//! \brief SanityCheckerMapParams()
//!
//! checks for the exceptions for Mapmemory() flags and
//! associated memory classes/flags and context handles together
//!
//! \sa Setup()
//! \sa Mapmemory()
//----------------------------------------------------------------------------
bool MMUTester::SanityCheckerMapParams()
{
    if( DRF_VAL(OS02, _FLAGS, _ALLOC, m_flags)== LWOS02_FLAGS_ALLOC_NONE )
    {
        //
        // ALLOC_NONE means no more mapping as assertion will fail
        //
        if (MMU_TEST_DEBUG)
        {
            Printf(Tee::PriLow,"ASSERTION is failing by ALLOC_NONE..skip\n");
        }
        return false;
    }

    if (DRF_VAL(OS33, _FLAGS, _SKIP_SIZE_CHECK, m_MapFlags) == LWOS33_FLAGS_SKIP_SIZE_CHECK_ENABLE)
            return false;

    return true;
}

//! \brief SanityCheckerMapDmaParams()
//!
//! checks for the exceptions for MapmemoryDma() flags and
//! associated memory classes/flags and context handles together
//!
//! \sa Setup()
//! \sa MapmemoryDma()
//----------------------------------------------------------------------------
bool MMUTester::SanityCheckerMapDmaParams(UINT32 memType, UINT32 virtualType)
{

    // If FIXED, then actually the GPU VADDR should be an input from us
    if (DRF_VAL(OS46, _FLAGS, _DMA_OFFSET_FIXED, m_MapDmaFlags) == LWOS46_FLAGS_DMA_OFFSET_FIXED_TRUE)
    {
        return false;
    }

    // If P2P is enabled and MapDma access flags are either RW or R, its not supported
    if (DRF_VAL(OS46, _FLAGS, _P2P_ENABLE, m_MapDmaFlags) == LWOS46_FLAGS_P2P_ENABLE_YES)
    {
        if ((DRF_VAL(OS46, _FLAGS, _ACCESS, m_MapDmaFlags) == LWOS46_FLAGS_ACCESS_READ_WRITE) ||
            (DRF_VAL(OS46, _FLAGS, _ACCESS, m_MapDmaFlags) == LWOS46_FLAGS_ACCESS_READ_ONLY))
        {
            return false;
        }

        if (memType == MEM_TYPE_SYSTEM)
        {
            return false;
        }
    }

    if (DRF_VAL(OS46, _FLAGS, _PAGE_SIZE, m_MapDmaFlags) == LWOS46_FLAGS_PAGE_SIZE_BIG)
    {
        // SYS MEM NON-CONTIGUOUS cannot be used MAP DMA BIG_SIZE
        if (memType == MEM_TYPE_SYSTEM)
        {
            if (DRF_VAL(OS02, _FLAGS, _ALLOC, m_flags) == LWOS02_FLAGS_ALLOC_NONE)
            {
                return false;
            }
            if (DRF_VAL(OS02, _FLAGS, _PHYSICALITY, m_flags) == LWOS02_FLAGS_PHYSICALITY_NONCONTIGUOUS)
            {
                return false;
            }
        }
    }

    if ((memType == MEM_TYPE_SYSTEM) && (virtualType == VIRTUAL_MEMORY_OBJECT))
    {
        //
        // The PAGESIZE int his case inside the driver is > 4K, hence
        // NON-CONTIGUOUS pages of HEAP are not allowed
        //
        if ((DRF_VAL(OS02, _FLAGS, _ALLOC, m_flags) == LWOS02_FLAGS_ALLOC_NONE) ||
            (DRF_VAL(OS02, _FLAGS, _PHYSICALITY, m_flags) == LWOS02_FLAGS_PHYSICALITY_NONCONTIGUOUS))
        {
            return false;
        }
    }

    if(memType == MEM_TYPE_HEAP)
    {
        if (virtualType == DYNAMIC_MEMORY_VIRTUAL)
        {
            if ((DRF_VAL(OS32, _ATTR, _PHYSICALITY, m_VidAttr) != LWOS32_ATTR_PHYSICALITY_CONTIGUOUS) &&
                (DRF_VAL(OS46, _FLAGS, _PAGE_SIZE, m_MapDmaFlags) == LWOS46_FLAGS_PAGE_SIZE_BIG))
            {
                // PAGESIZE-BIG does not allow CONTIGUOUS pages
                return false;
            }
            else if (DRF_VAL(OS32, _ATTR, _PHYSICALITY, m_VidAttr) != LWOS32_ATTR_PHYSICALITY_CONTIGUOUS)
            {
                // PAGESIZE-BIG does not allow CONTIGUOUS pages
                return false;
            }
        }
    }

   if(((DRF_VAL(OS32, _ATTR, _LOCATION, m_VidAttr) != LWOS32_ATTR_LOCATION_VIDMEM) ||
       (m_MemClass == LW01_MEMORY_SYSTEM))&&
       (DRF_VAL(OS46, _FLAGS, _P2P_ENABLE, m_MapDmaFlags) == LWOS46_FLAGS_P2P_ENABLE_YES))
   {
       // No P2P for system memory
       return false;
   }

#ifdef LW_VERIF_FEATURES
   if ((DRF_VAL(OS46, _FLAGS, _PRIV, m_MapDmaFlags) == LWOS46_FLAGS_PRIV_ENABLE) &&
       (DRF_VAL(OS46, _FLAGS, _ACCESS, m_MapDmaFlags) != LWOS46_FLAGS_ACCESS_READ_WRITE))
   {
       // Hopper+ priv pages must be read/write
       return false;
   }
#endif

   // XXX
   if((m_MemClass == LW01_MEMORY_LOCAL_USER) &&
      (DRF_VAL(OS46, _FLAGS, _PAGE_SIZE, m_MapDmaFlags) == LWOS46_FLAGS_PAGE_SIZE_4KB))
   {
       // On Fermi, page size be of 64KB on FB.
       return false;
   }

    return true;
}

//! \brief SanityHeapCheckerParams()
//!
//! checks for the exceptions for VidHeapAlloc()
//!
//! \sa Run()
//----------------------------------------------------------------------------
bool MMUTester::SanityCheckerHeapParams()
{
    //
    // We are not guaranteed to get contiguous sysmem allocations on dGPU.
    // So allows big pages only in vidmem on dGPU.
    //
    // CheetAh will support BIG pages with non-contiguous sysmem allocation, using SMMU.
    // TODO: So we need to update the test to support BIG pages in sysmem on CheetAh.
    //
    if (DRF_VAL(OS32, _ATTR, _PAGE_SIZE, m_VidAttr) == LWOS32_ATTR_PAGE_SIZE_BIG &&
        DRF_VAL(OS32, _ATTR, _LOCATION, m_VidAttr) != LWOS32_ATTR_LOCATION_VIDMEM)
    {
        return false;
    }
    // If DEPTH attribute is UNKNOWN
    //
    if(DRF_VAL(OS32, _ATTR, _DEPTH, m_VidAttr) == LWOS32_ATTR_DEPTH_UNKNOWN)
    {
        // If COMPR attribute is REQUIRED/ANY, the heap allocation is
        // bound to fail, hence skipping the allocation
        if((DRF_VAL(OS32, _ATTR, _COMPR, m_VidAttr) == LWOS32_ATTR_COMPR_REQUIRED)||
           (DRF_VAL(OS32, _ATTR, _COMPR, m_VidAttr) == LWOS32_ATTR_COMPR_ANY))
        {
           return false;
        }
    }
    // Tiled is no longer supported by RM
    if (DRF_VAL(OS32, _ATTR, _TILED, m_VidAttr) != LWOS32_ATTR_TILED_NONE)
    {
        return false;
    }
    //
    // compression requires IMAGE|DEPTH|PRIMARY types and block linear
    //
    if((DRF_VAL(OS32, _ATTR, _COMPR, m_VidAttr) &&
        (DRF_VAL(OS32, _ATTR, _FORMAT, m_VidAttr) !=
         LWOS32_ATTR_FORMAT_BLOCK_LINEAR) &&
        (m_VidType != LWOS32_TYPE_DEPTH) &&
        (m_VidType != LWOS32_TYPE_IMAGE) &&
        (m_VidType != LWOS32_TYPE_PRIMARY))||
        ((m_VidType == LWOS32_TYPE_DEPTH)&&
        (DRF_VAL(OS32, _ATTR, _FORMAT, m_VidAttr) !=
         LWOS32_ATTR_FORMAT_BLOCK_LINEAR)))
    {
        return false;
    }
    //
    // COMPRESSION of non-video TYPE is not supported
    //
    if ( DRF_VAL(OS32, _ATTR, _COMPR, m_VidAttr) &&
        (DRF_VAL(OS32, _ATTR, _LOCATION, m_VidAttr)!=
         LWOS32_ATTR_LOCATION_VIDMEM) )
    {
        if ( DRF_VAL(OS32, _ATTR, _COMPR, m_VidAttr) ==
            LWOS32_ATTR_COMPR_REQUIRED )
        {
            if (MMU_TEST_DEBUG)
                Printf(Tee::PriLow,"Compression REQUIRED \
                                but SYS_MEM is not compressible!\n");
            return false;
        }
    }
    //
    // Capturing TYPES > TYPE_ZLWLL which are invalid presently
    //
    if(m_VidType > LWOS32_TYPE_ZLWLL)
    {
        if (MMU_TEST_DEBUG)
            Printf(Tee::PriLow,"Invalid Type!!!!\n");
        return false;
    }
    //
    // If FORMAT_OVERRIDE_OFF and FORMAT is BLOCK_LINEAR
    //
#ifdef  LW_VERIF_FEATURES
    if(DRF_VAL(OS32, _ATTR, _FORMAT_OVERRIDE, m_VidAttr)!= LWOS32_ATTR_FORMAT_OVERRIDE_ON)
#endif
    {
        if(DRF_VAL(OS32, _ATTR, _FORMAT, m_VidAttr)
            == LWOS32_ATTR_FORMAT_BLOCK_LINEAR)
        {
            //
            // If LOCATION is VIDMEM
            //
            if(DRF_VAL(OS32, _ATTR, _LOCATION, m_VidAttr)
                == LWOS32_ATTR_LOCATION_VIDMEM)
            {
                //
                // Capturing Invalid TYPES
                //
                if((m_VidType == LWOS32_TYPE_SHADER_PROGRAM)||
                    (m_VidType == LWOS32_TYPE_FONT)||
                    (m_VidType == LWOS32_TYPE_LWRSOR)||
                    (m_VidType == LWOS32_TYPE_DMA)||
                    (m_VidType == LWOS32_TYPE_INSTANCE)||
                    (m_VidType == LWOS32_TYPE_ZLWLL)||
                    (m_VidType == LWOS32_TYPE_RESERVED))
                {
                    return false;
                }
                else
                {
                    //
                    // If TYPE is DEPTH && Z_TYPE is FIXED &&
                    // if ZS_PACKING is either Z16/ Z32/ Z32_X24S8/ X8Z24_X24S8
                    // the allocation fails, hence skipping
                    //
                    if(m_VidType == LWOS32_TYPE_DEPTH)
                    {
                        if((DRF_VAL(OS32, _ATTR, _Z_TYPE, m_VidAttr) ==
                            LWOS32_ATTR_Z_TYPE_FIXED) &&
                           ((DRF_VAL(OS32, _ATTR, _ZS_PACKING, m_VidAttr) ==
                            LWOS32_ATTR_ZS_PACKING_Z16)||
                           (DRF_VAL(OS32, _ATTR, _ZS_PACKING, m_VidAttr) ==
                            LWOS32_ATTR_ZS_PACKING_Z32)||
                           (DRF_VAL(OS32, _ATTR, _ZS_PACKING, m_VidAttr) ==
                            LWOS32_ATTR_ZS_PACKING_Z32_X24S8)||
                           (DRF_VAL(OS32, _ATTR, _ZS_PACKING, m_VidAttr) ==
                            LWOS32_ATTR_ZS_PACKING_X8Z24_X24S8)))
                        {
                            return false;
                        }
                        else
                        {
                            //
                            // If TYPE is NOT DEPTH && Z_TYPE is FLOAT &&
                            // if ZS_PACKING is either Z24S8/ S8Z24/
                            // Z24X8/ X8Z24/ X8Z24_X24S8/ Z16
                            // the allocation fails, hence skipping
                            //
                            if((DRF_VAL(OS32, _ATTR, _Z_TYPE, m_VidAttr) ==
                                LWOS32_ATTR_Z_TYPE_FLOAT) &&
                              ((DRF_VAL(OS32, _ATTR, _ZS_PACKING, m_VidAttr) ==
                                LWOS32_ATTR_ZS_PACKING_Z24S8)||
                               (DRF_VAL(OS32, _ATTR, _ZS_PACKING, m_VidAttr) ==
                                LWOS32_ATTR_ZS_PACKING_S8Z24)||
                               (DRF_VAL(OS32, _ATTR, _ZS_PACKING, m_VidAttr) ==
                                LWOS32_ATTR_ZS_PACKING_Z24X8)||
                               (DRF_VAL(OS32, _ATTR, _ZS_PACKING, m_VidAttr) ==
                                LWOS32_ATTR_ZS_PACKING_X8Z24 )||
                               (DRF_VAL(OS32, _ATTR, _ZS_PACKING, m_VidAttr) ==
                                LWOS32_ATTR_ZS_PACKING_X8Z24_X24S8 )||
                               (DRF_VAL(OS32, _ATTR, _ZS_PACKING, m_VidAttr) ==
                                LWOS32_ATTR_ZS_PACKING_Z16)))
                            {
                                return false;
                            }
                        }
                    }
                    else
                    {
                        // because the fbCompressibleKind() will be 0
                        //for the following cases
                        // which results in ILWALID_ARGUMENT error
                        if((DRF_VAL(OS32, _ATTR, _DEPTH, m_VidAttr) ==
                            LWOS32_ATTR_DEPTH_24)||
                            ((DRF_VAL(OS32, _ATTR, _DEPTH, m_VidAttr) ==
                            LWOS32_ATTR_DEPTH_16)&&
                            (DRF_VAL(OS32, _ATTR, _COMPR, m_VidAttr) ==
                            LWOS32_ATTR_COMPR_REQUIRED))||
                            ((DRF_VAL(OS32, _ATTR, _DEPTH, m_VidAttr) ==
                            LWOS32_ATTR_DEPTH_128)&&
                            (DRF_VAL(OS32, _ATTR, _COMPR, m_VidAttr) ==
                            LWOS32_ATTR_COMPR_REQUIRED))||
                            ((DRF_VAL(OS32, _ATTR, _DEPTH, m_VidAttr) ==
                            LWOS32_ATTR_DEPTH_UNKNOWN)&&
                            (DRF_VAL(OS32, _ATTR, _COMPR, m_VidAttr) ==
                            LWOS32_ATTR_COMPR_REQUIRED))||
                            ((DRF_VAL(OS32, _ATTR, _DEPTH, m_VidAttr) ==
                            LWOS32_ATTR_DEPTH_8)&&
                            (DRF_VAL(OS32, _ATTR, _COMPR, m_VidAttr) ==
                            LWOS32_ATTR_COMPR_REQUIRED)))

                        {
                            return false;
                        }
                    }
                }
            }

        }
        else
        {
            // if FORMAT is not BLOCK_LINEAR, but OVERRIDE is OFF,
            // then COMPR should be REQUIRED
            // else skipping.
            if(DRF_VAL(OS32, _ATTR, _COMPR, m_VidAttr) ==
                LWOS32_ATTR_COMPR_REQUIRED)
            {
                return false;
            }
        }

    }

    // Global zlwll is no longer supported
    if (DRF_VAL(OS32, _ATTR, _ZLWLL, m_VidAttr) != LWOS32_ATTR_ZLWLL_NONE)
    {
         return false;
    }

    if (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_PAGING))
    {
#ifdef  LW_VERIF_FEATURES
        if(DRF_VAL(OS32, _ATTR, _FORMAT_OVERRIDE, m_VidAttr)!= LWOS32_ATTR_FORMAT_OVERRIDE_ON)
#endif
        {
            if(DRF_VAL(OS32, _ATTR, _FORMAT, m_VidAttr)
                == LWOS32_ATTR_FORMAT_BLOCK_LINEAR)
            {
                if(DRF_VAL(OS32, _ATTR, _COLOR_PACKING, m_VidAttr)!=
                    LWOS32_ATTR_COLOR_PACKING_X8R8G8B8)
                {
                    return false;
                }

                if(DRF_VAL(OS32, _ATTR, _COLOR_PACKING, m_VidAttr)!=
                                                LWOS32_ATTR_DEPTH_32)
                {
                    return false;
                }
                if(m_VidType == LWOS32_TYPE_DEPTH &&
                    ((DRF_VAL(OS32, _ATTR, _Z_TYPE, m_VidAttr)!=
                                LWOS32_ATTR_Z_TYPE_FIXED) ||
                        (DRF_VAL(OS32, _ATTR, _ZS_PACKING, m_VidAttr)!=
                                LWOS32_ATTR_ZS_PACKING_Z16)))
                {
                    return false;
                }
            }
        }
    }

    //
    // Bug Id: 
    // Remove this sanity, once PMA allows non-contiguous allocations when
    // alignment is larger than page size. In Ampere 1:4 comptagline allocation
    // policy, alignment is fixed as 256KB, hence page size lesser than 2MB
    // returns ERROR from PMA
    //
    if ((GetBoundGpuDevice())->GetFamily() >= GpuDevice::Ampere)
    {
        if ((DRF_VAL(OS32,_ATTR, _COMPR, m_VidAttr) !=
                     LWOS32_ATTR_COMPR_NONE) &&
            (DRF_VAL(OS32, _ATTR, _PHYSICALITY, m_VidAttr) !=
                    LWOS32_ATTR_PHYSICALITY_CONTIGUOUS ||
                    DRF_VAL(OS32, _ATTR, _PHYSICALITY, m_VidAttr !=
                    LWOS32_ATTR_PHYSICALITY_DEFAULT)) &&
            (DRF_VAL(OS32, _ATTR, _PAGE_SIZE, m_VidAttr) <=
                    LWOS32_ATTR_PAGE_SIZE_HUGE))
        {
            return false;
        }
    }

    return true;
}

//! \brief save info of memory chunk
//!
//! Save the info for allocated memory chunk
//!
//! return->void
//!
//! \sa MapmemoryDma()
//----------------------------------------------------------------------------

void MMUTester::SaveMemChunk(UINT32 memType, UINT64 gpuAddress)
{
    ALLOCED_MEM *temp;
    temp = new ALLOCED_MEM;
    memset(temp, 0, sizeof(ALLOCED_MEM));

    if (memType == MEM_TYPE_SYSTEM)
    {
        temp->MEMTYPE =    MEM_TYPE_SYSTEM;
        temp->hMemSystem = m_hMemSystem;
        temp->flags =      m_flags;
        temp->Size =       m_sys_size;
    }
    else
    {
        temp->MEMTYPE = MEM_TYPE_HEAP;
        temp->hVideo  = m_hVideo;
        temp->vidAttr = m_VidAttr;
        temp->Size    = m_video_size;
    }

    temp->hVirtual =    m_hVirtual;
    temp->hDma =        m_hDma;
    temp->mapDmaFlags = m_MapDmaFlags;
    temp->gpuAddr =     gpuAddress;

    m_memChunkStore.push_back(*temp);
    m_IsChunkSaved = true;
    delete temp;
}

//! \brief Delete Fix amount of memory chunk
//!
//! Free some fixed amount of allocated chunk randomly.
//!
//! return->OK when succeded, else return->RC error
//!
//! \sa MapmemoryDma()
//----------------------------------------------------------------------------

RC MMUTester::DelMemChunk()
{
    RC status;
    LwRmPtr pLwRm;
    ALLOCED_MEM node;
    UINT64 indexToDelete, i;
    std::list<ALLOCED_MEM>::iterator freeAllocedIndex;

    if (m_memChunkStore.empty())
    {
        Printf(Tee::PriError," NO Allocated Chunk Information is Stored \n");
        return RC::LWRM_ILWALID_STATE;
    }

    for (i = 0 ; i < ALLOCS_TO_FREE ; i++)
    {

        freeAllocedIndex = m_memChunkStore.begin();
        indexToDelete = m_Random.GetRandom(0,((UINT32)m_memChunkStore.size() - 1));

        while ( indexToDelete-- )
        {
            freeAllocedIndex++;
        }
        node = *(freeAllocedIndex);
        if ( node.MEMTYPE == MEM_TYPE_SYSTEM)
        {
            pLwRm->UnmapMemoryDma(node.hVirtual, node.hMemSystem,
                                  node.mapDmaFlags, node.gpuAddr, GetBoundGpuDevice());
        }
        else
        {
            pLwRm->UnmapMemoryDma(node.hVirtual, node.hVideo,
                                  node.mapDmaFlags, node.gpuAddr, GetBoundGpuDevice());
        }
        pLwRm->Free(node.hDma);
        pLwRm->Free(node.hVideo);
        pLwRm->Free(node.hMemSystem);
        pLwRm->Free(node.hVirtual);
        m_memChunkStore.erase(freeAllocedIndex);
    }
    return status;
}

//! \brief Print important saved chunk info
//!
//! Print few relevant information of saved chunk
//!
//! return void
//!
//----------------------------------------------------------------------------

void MMUTester::PrintSavedChunkInfo()
{
    UINT32 counter;
    std::list<ALLOCED_MEM>::iterator allocedChunk;
    FILE *csvPtr = NULL;

    csvPtr = fopen("MemAllocedInfo.csv","w");
    fprintf(csvPtr,"INDEX,MEM TYPE,GPU VIRTUAL ADDRESS,SIZE\n");
    allocedChunk = m_memChunkStore.begin();

    for (counter = 0; counter < m_memChunkStore.size(); counter++, allocedChunk++)
    {
        Printf(Tee::PriLow,"\n*******************Chunk %d Info**********************\n",(counter+1));
        fprintf(csvPtr,"%d",counter+1);
        if (allocedChunk->MEMTYPE == MEM_TYPE_SYSTEM)
        {
            Printf(Tee::PriLow," Alloced Mem Aperture : System Memory \n");
            Printf(Tee::PriLow," Alloced Sys Mem Handle : 0x%x \n", allocedChunk->hMemSystem);
            Printf(Tee::PriLow," Alloced Sys Mem Size : %llu \n", allocedChunk->Size);
            Printf(Tee::PriLow," Sys Mem Alloc Flags : 0x%x \n", allocedChunk->flags);
            fprintf(csvPtr,",System Memory");
        }
        else
        {
            Printf(Tee::PriLow," Alloced Mem Aperture : Heap memory \n");
            Printf(Tee::PriLow," Alloced Vid Mem Handle : 0x%x \n", allocedChunk->hVideo);
            Printf(Tee::PriLow," Alloced Vid Mem Size : %llu \n", allocedChunk->Size);
            Printf(Tee::PriLow," Vid Mem Alloc flags : 0x%x \n", allocedChunk->vidAttr);
            fprintf(csvPtr,",Video Memory");
        }
        Printf(Tee::PriLow," Gpu Virtual Address Alloc Flags : 0x%x\n", allocedChunk->mapDmaFlags);
        Printf(Tee::PriLow," Gpu Virtual Address : %llu\n", allocedChunk->gpuAddr);
        fprintf(csvPtr,",%llu",allocedChunk->gpuAddr);
        fprintf(csvPtr,",%llu\n", allocedChunk->Size);
    }
    fclose(csvPtr);
}

//--------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ MMUTester
// !object.
//---------------------------------------------------------------------------
JS_CLASS_INHERIT(MMUTester, RmTest, "GPU MMU test");

