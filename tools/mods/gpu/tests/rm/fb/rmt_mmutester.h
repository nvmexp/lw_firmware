/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2005-2008 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "gpu/tests/rm/utility/mmuutil.h"

// Definitions for NON_HEAP allocations
#define MEM_TYPE_SYSTEM       0
#define MEM_TYPE_HEAP         1
#define RM_PAGE_SIZE          0x1000
#define IN_KB                 0x400
#define LOWER_LIMIT           RM_PAGE_SIZE // lower limit for memory size used
#define UPPER_LIMIT_SYS       0x0200000 // upper limit for system memory used
#define UPPER_LIMIT_VID       0x8000000 // upper limit for video memory size used
#define MAX_LOOPS             1000 // Number of iterations of the test

#define MAP_MEMORY_FAIL_SIZE  0x10000000

// Sometimes large chunks of memory will be allocated by OS at boot time.
// Hence those large chunks will not be allocated
// after the boot. Hence coming up with a hack.
// FEASIBLE_SIZE (1 MB)is assumed to be allocated in the test when there is
// any insufficiency of resources
#define FEASIBLE_SIZE          0x100000 // Allowable size on FB
#define MAX_MEMORY_TYPES       2

#define ALLOCS_TO_KEEP         20 // no. of allocated chuck to hold till end
#define ALLOCS_TO_FREE         5 // no. of chunks to delete at one go

#define TIME_OUT               6000  // in milliseconds

#define MMU_TEST_DEBUG         0
// Definitions for VIRTUAL MEMORY
#define VIRTUAL_TYPES          2
#define DYNAMIC_MEMORY_VIRTUAL 0
#define VIRTUAL_MEMORY_OBJECT  1

//Probability Values
#define ZERO_PROBABILITY       0
#define EQUAL_PROBABILITY      0.5
#define ABSOLUTE_VALUE_ONE     1

#define RC_ERROR             10
#define MINIMUM( a , b ) (((a) < (b)) ? (a) : (b))
#define MIN_SUBDEVICES_FOR_SLI        (2)
static UINT32 HeapMaximum = 0;

// Structure to Store various information of saved allocated memory chunk
typedef struct
{
    bool            MEMTYPE;
    LwRm::Handle    hMemSystem;
    LwRm::Handle    hVideo;
    LwRm::Handle    hVirtual;
    LwRm::Handle    hDma;
    UINT32          flags;
    UINT32          vidAttr;
    UINT32          mapDmaFlags;
    UINT64          gpuAddr;
    UINT64          Size;
}ALLOCED_MEM;

class MMUTester : public RmTest
{
public:
    MMUTester();
    virtual ~MMUTester();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    //@{
    //! Test specific variables
    LwRm::Handle m_hMemSystem, m_hDma, m_hVideo, m_hVirtual;
    LwRm::Handle m_hClient, m_hDevice, m_hSubDev;
    UINT64 m_sys_size;
    UINT64 m_video_size;
    UINT32 m_flags;
    UINT32 m_nofb;
    UINT32 m_MemClass;
    UINT32 m_CtxFlags;
    UINT32 m_MapFlags;
    UINT32 m_MapDmaFlags;
    UINT32 m_VidAttr;
    UINT32 m_VidAttr2;
    UINT32 m_VidType;
    UINT64 m_vidOffset;
    UINT32 m_count;
    UINT32 m_statistic;
    Random m_Random;
    MmuUtil m_MmuUtil;
    bool m_IsChunkSaved;
    list<ALLOCED_MEM> m_memChunkStore;
    UINT32 m_NumSubdevices;
    //@}
    //@{
    //! Test specific functions
    RC AllocateCtxDma(UINT32 m_count, UINT32 memType, bool m_Virtual );
    RC Mapmemory(UINT32 m_count, UINT32 memType);
    RC MapmemoryDma(UINT32 m_count, UINT32 memType, UINT32 virtualType);
    RC VirtualMemTest(UINT32 m_count, UINT32 memType);
    RC AllocFbHeap(bool m_Virtual, UINT32 memType);
    RC FbMapApertureToFail();
    bool SanityCheckerMemParams();
    bool SanityCheckerDmaParams();
    bool SanityCheckerMapParams();
    bool SanityCheckerMapDmaParams(UINT32 memType, UINT32 virtualType);
    bool SanityCheckerHeapParams();
    void SaveMemChunk(UINT32 memType, UINT64 gpuAddress);
    RC DelMemChunk();
    void PrintSavedChunkInfo();
    //@}
};

// Weights for mem type (SYSTEM/HEAP) and virtual mem type (DYNAMIC/VIRTUAL )
static const double memTypeWeights[MAX_MEMORY_TYPES] = {0.5, 0.5};
static double virtualWeights[VIRTUAL_TYPES];

// Generic weights structure from mmu utility (Initializing weights here).
static MmuUtilWeights weights[] = {
    {
        // Weights for AllocMemory() classes
        { {0.5, 0.5} },

        // Weights for AllocMemory() flags
        {
            {{0.5, 0.5, 0, 0, 0, 0}},
            {{0.5, 0.5, 0, 0, 0, 0}},
            {{0.17, 0.17, 0.16, 0.16, 0.17, 0.17}},
            {{0.5, 0.5, 0, 0, 0, 0}},
        },

        // Weights for type param in VidHeapAlloc()
        { {0.071, 0.071, 0.071, 0.071, 0.071, 0.071, 0.071, 0.071, 0.071, 0.071, 0.071, 0.071, 0.071, 0.077} },

        // Weights for flags in ATTR param in VidHeapAlloc()
        {
            {{0.148,0.142,0.142,0.142,0.142,0.142,0.142, 0, 0, 0, 0}},
            {{0.5, 0.5, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {{0.25, 0.25, 0.25, 0.25, 0, 0, 0, 0, 0, 0, 0}},
            {{0.33, 0.33, 0.34, 0, 0, 0, 0, 0, 0, 0, 0}},
            {{0.33, 0.33, 0.34, 0, 0, 0, 0, 0, 0, 0, 0}},
            {{0.5, 0.5, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {{0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0, 0, 0}},
            {{0.5, 0.5, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
            {{0.33, 0.33, 0.34, 0, 0, 0, 0, 0, 0, 0, 0}},
            {{0.25, 0.25, 0.25, 0.25, 0, 0, 0, 0, 0, 0, 0}},
            {{0.33, 0.33, 0.34, 0, 0, 0, 0, 0, 0, 0, 0}},
            {{0.166, 0.166, 0.166, 0.166, 0.166, 0.17, 0, 0, 0, 0, 0}}
        },

        //
        // Weights for flags in ATTR2 param in VidHeapAlloc()
        // These aren't lwrrently being used in MMU Tester, so zeroing them
        //
        {
            {{0, 0, 0}}, // Weights for ATTR2_ZBC
            {{0, 0, 0}}, // Weights for ATTR_GPU_CACHEABLE
            {{0, 0, 0}}, // Weights for ATTR_P2P_GPU_CACHEABLE
        },

        // Weights for flags in AllocContextDma()
        {
            {{0.33, 0.33, 0.34}},
            {{0.5, 0.5, 0}},
            {{0.5, 0.5, 0}},
            {{0.5, 0.5, 0}},
            {{0, 0}},
            {{0, 0}},
            {{0.0, 1.0, 0}},
        },

        // Weights for flags in MapMemory()
        {
            {{0.33, 0.33, 0.34}},
            {{0.5, 0.5, 0}},
            {{0.5, 0.5}},
            {{0.33, 0.33, 0.34}}
        },

        // Weights for flags in MapMemoryDma()
        {
            {{0.33, 0.33, 0.34, 0, 0, 0, 0, 0, 0}},
            {{0.5, 0.5, 0, 0, 0, 0, 0, 0, 0}},
            {{0.33, 0.33, 0.34, 0, 0, 0, 0, 0, 0}},
            {{0.5, 0.5, 0, 0, 0, 0, 0, 0, 0}},
            {{0.5, 0.5, 0, 0, 0, 0, 0, 0, 0}},
            {{0.5, 0.5, 0, 0, 0, 0, 0, 0, 0}},
            {{0.11, 0.11, 0.11, 0.11, 0.11, 0.11, 0.11, 0.11, 0.12}},
            {{0.5, 0.5, 0, 0, 0, 0, 0, 0, 0}},
            {{0.5, 0.5, 0, 0, 0, 0, 0, 0, 0}},
        }
    }
};

