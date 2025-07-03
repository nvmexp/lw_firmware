/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2006-2009 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef INCLUDED_MMUUTIL_H
#define INCLUDED_MMUUTIL_H

#include "random.h"
#include "lwos.h"
#include "class/cl003e.h"       // LW01_MEMORY_SYSTEM
#include "class/cl0071.h"       // LW01_MEMORY_SYSTEM_OS_DESCRIPTOR

#define DEFAULT_FLAG                0       // symbol for 0, used in place of flags which are not accessible when LW_VERIF_FEATURES is not defined
#define INVALID                     0xFFFFFFFF

// Definitions for NON_HEAP allocations
#define MAX_CLASSES                 2       // Maximum number of memory classes

#define MAX_MEMORY_FIELDS           4       // Maximum fields for memory flags
#define MAX_MEM_PARAMS              6       // Max parameters any MEM field can take

// These defines give maximum values for different flags of AllocMemory.
#define PHYSICALITY_FIELD           2       // possible parameters for PHYSICALITY
#define LOCATION_FIELD              1       // possible parameters for LOCATION_FIELD
#define COHERENCE_FIELD             6       // possible parameters for COHERENCE_FIELD
#define MAPPING_FIELD               2       // possible parameters for MAPPING_FIELD

#define MAX_MAP_FIELDS              4       // Maximum fields for Mapmemory flags
#define MAX_MAP_PARAMS              3       // Max parameters any MAP field can take

// These defines give maximum values for different flags of MapMemory.
#define MAP_ACCESS_FIELD            3       // possible parameters for MAP_ACCESS_FIELD
#define PERSISTANT_FIELD            2       // possible parameters for PERSISTANT_FIELD
#define SIZE_CHECK_FIELD            2       // possible parameters for MEM_SPACE_FIELD
#define MEM_SPACE_FIELD             2       // possible parameters for SIZE_CHECK_FIELD
#define MAPMEMORY_MAPPING_FIELD     3       // possible parameters for MAPMEMORY_MAPPING_FIELD

#define MAX_DMA_FIELDS              7       // Maximum fields for Dma flags
#define MAX_DMA_PARAMS              3       // Max parameters any DMA field can take

// These defines give maximum values for different flags of AllocCtxDma
#define DMA_ACCESS_FIELD            3       // possible parameters for DMA_ACCESS_FIELD
#define PREALLOCATE_FIELD           2       // possible parameters for PREALLOCATE_FIELD
#define MAPPABLE_FIELD              2       // possible parameters for MAPPABLE_FIELD
#define FLAG_TYPE_FIELD             2       // possible parameters for FLAG_TYPE_FIELD
#define ALLOCCTXDMA_MAPPING_FIELD   2       // possible parameters for ALLOCCTXDMA_MAPPING_FIELD
#define CACHE_SNOOP_FIELD           2       // possible parameters for CACHE_SNOOP_FIELD
#define HASH_TABLE_FIELD            2       // possible parameters for HASH_TABLE_FIELD

#define MAX_MAPDMA_FIELDS           9
#define MAX_MAPDMA_PARAMS           9

// These defines give maximum values for different flags of MapMemoryDma.
#define MAPDMA_ACCESS_FIELD               3
#define MAPDMA_CACHE_SNOOP_FIELD          2
#define MAPDMA_PAGE_SIZE_CHECK_FIELD      3
#define MAPDMA_PRIV_FIELD                 2
#define MAPDMA_DMA_OFFSET_GROWS_FIELD     2
#define MAPDMA_DMA_OFFSET_FIXED_FIELD     2
#define MAPDMA_PTE_COALESCE_LEVEL_FIELD   9
#define MAPDMA_FLAGS_P2P_ENABLE_FIELD     3
#define MAPDMA_TLB_LOCK_FIELD             2

// Definitions for HEAP allocation
#define MAX_TYPES                   14      // Max no. of TYPES for allocation on heap
#define MAX_ATTR_FIELDS             13      // MAx no. of ATTRIBUTE fields
#define MAX_ATTR_PARAMS             11      // Max no. of parameters in each field
#define MAX_ATTR2_FIELDS            3       // MAx no. of ATTR2 fields
#define MAX_ATTR2_PARAMS            3       // Max no. of parameters in each field

// These defines give maximum values for different flags (ATTR) of VidHeapAlloc.
#define DEPTH_FIELD                 7       // possible parameters for DEPTH_FIELD
#define COMPR_COVG_FIELD            2       // possible parameters for COMPR_COVG_FIELD
#define AA_SAMPLES_FIELD            11      // possible parameters for AA_SAMPLES_FIELD
#define COMPR_FIELD                 3       // possible parameters for COMPR_FIELD
#define FORMAT_FIELD                3       // possible parameters for FORMAT_FIELD
#define Z_TYPE_FIELD                2       // possible parameters for Z_TYPE_FIELD
#define ZS_PACKING_FIELD            8       // possible parameters for ZS_PACKING_FIELD
#define COLOR_PACKING_FIELD         2       // possible parameters for COLOR_PACKING_FIELD
#define FORMAT_OVERRIDE_FIELD       2       // possible parameters for FORMAT_OVERRIDE
#define PAGE_SIZE_HEAP_FIELD        4       // possible parameters for PAGE_SIZE on heap
#define LOCATION_HEAP_FIELD         3       // possible parameters for LOCATION_FIELD
#define PHYSICALITY_HEAP_FIELD      3       // possible parameters for PHYSICALITY_FIELD
#define COHERENCY_HEAP_FIELD        6       // possible parameters for COHERENCE_FIELD

// These defines give maximum values for different flags (ATTR2) of VidHeapAlloc.
#define ZBC_FIELD                   3       // possible parameters for ZBC_FIELD
#define GPU_CACHEABLE_FIELD         3       // possible parameters for GPU_CACHEABLE_FIELD
#define P2P_GPU_CACHEABLE_FIELD     3       // possible parameters for P2P_GPU_CACHEABLE_FIELD

//
// A generic weights structure. The user should create a variable of this
// structure and initialize all the weights. These weights are used when
// randomly generating the various flags.
//
typedef struct _weights {
    //
    // This 'sysClassWeights' struct should hold the weights for MEM classes:
    // LOCAL_USER / SYSTEM / SYSTEM_OS_DESCRIPTOR
    //
    struct {
        double probability[MAX_CLASSES];
    } sysClassWeights;

    // This 'sysMemWeights' struct should hold the weights for different flags of AllocMemory().
    struct {
        double probability[MAX_MEM_PARAMS];
    } sysMemWeights[MAX_MEMORY_FIELDS];

    // This 'vidTypeWeights' should hold the weights for TYPE param in VidHeapAlloc()
    struct {
        double probability[MAX_TYPES];
    } vidTypeWeights;

    //
    // This 'vidAttrWeights' should hold the weights for
    // flags contained in ATTR param in VidHeapAlloc()
    //
    struct {
        double probability[MAX_ATTR_PARAMS];
    } vidAttrWeights[MAX_ATTR_FIELDS];

    //
    // This 'vidAttr2Weights' should hold the weights for
    // flags contained in ATTR2 param in VidHeapAlloc()
    //
    struct {
        double probability[MAX_ATTR2_PARAMS];
    } vidAttr2Weights[MAX_ATTR2_FIELDS];

    // This 'ctxDmaWeights' should hold the weights for flags for AllocateCtxDma()
    struct {
        double probability[MAX_DMA_PARAMS];
    } ctxDmaWeights[MAX_DMA_FIELDS];

    // This 'mapMemWeights' should hold the weights for flags for MapMemory()
    struct {
        double probability[MAX_MAP_PARAMS];
    } mapMemWeights[MAX_MAP_FIELDS];

    // This 'mapDmaWeights' should hold the weights for flags for MapMemoryDma()
    struct {
        double probability[MAX_MAPDMA_PARAMS];
    } mapDmaWeights[MAX_MAPDMA_FIELDS];
} MmuUtilWeights;

class MmuUtil
{
public:
    MmuUtil();
    MmuUtil(UINT32 seed, UINT32 precision);
    ~MmuUtil();
    UINT32      getSeed();
    UINT32      getWeightedSamples(const double * probability, UINT32 fieldLength);

    UINT32      RandomizeAllocMemoryClasses(MmuUtilWeights * weights);
    UINT32      RandomizeAllocMemoryFlags(MmuUtilWeights * weights, UINT32 allocMemoryFlagsToRandomize);
    UINT32      RandomizeAllocCtxDmaFlags(MmuUtilWeights * weights, UINT32 allocCtxDmaFlagsToRandomize);
    UINT32      RandomizeMapMemoryFlags(MmuUtilWeights * weights, UINT32 mapMemoryFlagsToRandomize);
    UINT32      RandomizeMapMemoryDmaFlags(MmuUtilWeights * weights, UINT32 mapMemoryDmaFlagsToRandomize);
    UINT32      RandomizeAllocHeapTypes(MmuUtilWeights * weights);
    UINT32      RandomizeAllocHeapAttrFlags(MmuUtilWeights * weights, UINT32 allocHeapAttrFlagsToRandomize);
    UINT32      RandomizeAllocHeapAttr2Flags(MmuUtilWeights * weights, UINT32 allocHeapAttr2FlagsToRandomize);

    void        PrintAllocMemoryClasses(const UINT32 priority, const char *pMsg, const UINT32 vidType);
    void        PrintAllocMemoryFlags(const UINT32 priority, const char *msg, const UINT32 allocMemoryFlags);
    void        PrintAllocCtxDmaFlags(const UINT32 priority, const char *msg, const UINT32 allocCtxDmaFlags);
    void        PrintMapMemoryFlags(const UINT32 priority, const char *msg, const UINT32 mapMemoryFlags);
    void        PrintMapMemoryDmaFlags(const UINT32 priority, const char *msg, const UINT32 mapMemoryDmaFlags);
    void        PrintAllocHeapVidType(const UINT32 priority, const char *pMsg,const UINT32 vidType);
    void        PrintAllocHeapAttrFlags(const UINT32 priority, const char *msg, const UINT32 allocHeapAttrFlags);
    void        PrintAllocHeapAttr2Flags(const UINT32 priority, const char *msg, const UINT32 allocHeapAttr2Flags);

private:

    UINT32      m_Seed;
    UINT32      m_Precision;
    Random      m_Random;

    //! Fields for memory flags
    enum{PHYSICALITY, LOCATION, COHERENCE, MAPPING};
    //! Fields for context dma flags
    enum{ACCESS, PREALLOCATE,
        GPU_MAPPABLE,
        TYPE, ALLOCCTXDMA_MAPPING, CACHE_SNOOP, HASH_TABLE };
    //! Fields for mapmemory flags
    enum{MAP_ACCESS, SKIP_SIZE_CHECK, MEM_SPACE, MAPMEMORY_MAPPING};
    //! Fields for mapmemorydma flags
    enum{MAPDMA_ACCESS, MAPDMA_CACHE_SNOOP,
        MAPDMA_PAGE_SIZE_CHECK, MAPDMA_PRIV,
        MAPDMA_DMA_OFFSET_GROWS, MAPDMA_DMA_OFFSET_FIXED,
        MAPDMA_PTE_COALESCE_LEVEL,
        MAPDMA_FLAGS_P2P_ENABLE, MAPDMA_TLB_LOCK};
    //! Attributes for surfaces to be allocated on heap
    enum{DEPTH, COMPR_COVG, AA_SAMPLES,
        COMPR, FORMAT, Z_TYPE, ZS_PACKING,
        COLOR_PACKING, FORMAT_OVERRIDE,PAGE_SIZE_HEAP, LOCATION_HEAP,
        PHYSICALITY_HEAP, COHERENCY_HEAP};
    enum{ZBC, GPU_CACHEABLE, P2P_GPU_CACHEABLE};
};

//
// for multi threading purpose, you want only one copy of this,
// hence the arrays are static
//

// Array of possible memory classes
static const UINT32 s_AllocMemoryClasses[MAX_CLASSES] = {
        LW01_MEMORY_SYSTEM,
        LW01_MEMORY_SYSTEM_OS_DESCRIPTOR,
};

// Array of possible memory classes (in string form, useful for printing)
static const char s_AllocMemoryClassesStrings[MAX_CLASSES][50] = {
        "LW01_MEMORY_SYSTEM",
        "LW01_MEMORY_SYSTEM_OS_DESCRIPTOR",
};

// Array of possible flags for AllocMemory()
static const UINT32 s_AllocMemoryFlags[MAX_MEMORY_FIELDS][MAX_MEM_PARAMS] = {
    {
        LWOS02_FLAGS_PHYSICALITY_CONTIGUOUS,
        LWOS02_FLAGS_PHYSICALITY_NONCONTIGUOUS,
    },
    {
        LWOS02_FLAGS_LOCATION_PCI,
    },
    {
        LWOS02_FLAGS_COHERENCY_UNCACHED,
        LWOS02_FLAGS_COHERENCY_CACHED,
        LWOS02_FLAGS_COHERENCY_WRITE_COMBINE,
        LWOS02_FLAGS_COHERENCY_WRITE_THROUGH,
        LWOS02_FLAGS_COHERENCY_WRITE_PROTECT,
        LWOS02_FLAGS_COHERENCY_WRITE_BACK,
     },
     {
        LWOS02_FLAGS_MAPPING_DEFAULT,
        LWOS02_FLAGS_MAPPING_NO_MAP,
     },
};

// Array of possible flags for AllocMemory() (in string form, useful for printing)
static const char s_AllocMemoryFlagsStrings[MAX_MEMORY_FIELDS][MAX_MEM_PARAMS][50] = {
    {
        "LWOS02_FLAGS_PHYSICALITY_CONTIGUOUS",
        "LWOS02_FLAGS_PHYSICALITY_NONCONTIGUOUS",

    },
    {
        "LWOS02_FLAGS_LOCATION_PCI",
    },
    {
        "LWOS02_FLAGS_COHERENCY_UNCACHED",
        "LWOS02_FLAGS_COHERENCY_CACHED",
        "LWOS02_FLAGS_COHERENCY_WRITE_COMBINE",
        "LWOS02_FLAGS_COHERENCY_WRITE_THROUGH",
        "LWOS02_FLAGS_COHERENCY_WRITE_PROTECT",
        "LWOS02_FLAGS_COHERENCY_WRITE_BACK",
     },
     {
        "LWOS02_FLAGS_MAPPING_DEFAULT",
        "LWOS02_FLAGS_MAPPING_NO_MAP",
     },

};

// These defines should be used by the caller of RandomizeAllocMemoryFlags()
// to specify which flags should be randomized (PHYSICALITY/LOCATION etc..)
#define MMUUTIL_ALLOC_SYSMEM_FLAGS_TO_RANDOMIZE_PHYSICALITY         0x00000001
#define MMUUTIL_ALLOC_SYSMEM_FLAGS_TO_RANDOMIZE_LOCATION            0x00000002
#define MMUUTIL_ALLOC_SYSMEM_FLAGS_TO_RANDOMIZE_COHERENCY           0x00000004
#define MMUUTIL_ALLOC_SYSMEM_FLAGS_TO_RANDOMIZE_MAPPING             0x00000008

// Array of possible flags for AllocCtxDma()..taken care of LW_VERIF_FEATURES
static const UINT32 s_AllocCtxDmaFlags[MAX_DMA_FIELDS][MAX_DMA_PARAMS] = {
    {
        LWOS03_FLAGS_ACCESS_READ_WRITE,
        LWOS03_FLAGS_ACCESS_READ_ONLY,
        LWOS03_FLAGS_ACCESS_WRITE_ONLY,
    },
    {
        LWOS03_FLAGS_PREALLOCATE_DISABLE,
        LWOS03_FLAGS_PREALLOCATE_ENABLE,
    },
    {
       LWOS03_FLAGS_GPU_MAPPABLE_DISABLE,
       LWOS03_FLAGS_GPU_MAPPABLE_ENABLE,
    },
    {
       DEFAULT_FLAG,
       LWOS03_FLAGS_TYPE_NOTIFIER
    },
    {
       LWOS03_FLAGS_MAPPING_NONE,
       LWOS03_FLAGS_MAPPING_KERNEL
    },
    {
       LWOS03_FLAGS_CACHE_SNOOP_ENABLE,
       LWOS03_FLAGS_CACHE_SNOOP_DISABLE
    },
    {
       LWOS03_FLAGS_HASH_TABLE_ENABLE,
       LWOS03_FLAGS_HASH_TABLE_DISABLE
    },
};

// Array of possible flags for AllocCtxDma() (in string form, useful for printing)
static const char s_AllocCtxDmaFlagsStrings[MAX_DMA_FIELDS][MAX_DMA_PARAMS][50] = {
    {
        "LWOS03_FLAGS_ACCESS_READ_WRITE",
        "LWOS03_FLAGS_ACCESS_READ_ONLY",
        "LWOS03_FLAGS_ACCESS_WRITE_ONLY",
    },
    {
        "LWOS03_FLAGS_PREALLOCATE_DISABLE",
        "LWOS03_FLAGS_PREALLOCATE_ENABLE",
    },
    {
       "LWOS03_FLAGS_GPU_MAPPABLE_DISABLE",
       "LWOS03_FLAGS_GPU_MAPPABLE_ENABLE",
    },
    {
       "DEFAULT_FLAG",
       "LWOS03_FLAGS_TYPE_NOTIFIER"
    },
    {
       "LWOS03_FLAGS_MAPPING_NONE",
       "LWOS03_FLAGS_MAPPING_KERNEL"
    },
    {
       "LWOS03_FLAGS_CACHE_SNOOP_ENABLE",
       "LWOS03_FLAGS_CACHE_SNOOP_DISABLE"
    },
    {
       "LWOS03_FLAGS_HASH_TABLE_ENABLE",
       "LWOS03_FLAGS_HASH_TABLE_DISABLE"
    },
};

// These defines should be used by the caller of RandomizeAllocCtxDmaFlags()
// to specify which flags should be randomized (ACCESS/PREALLOCATE etc..)
#define MMUUTIL_ALLOC_CTX_DMA_FLAGS_TO_RANDOMIZE_ACCESS                 0x00000001
#define MMUUTIL_ALLOC_CTX_DMA_FLAGS_TO_RANDOMIZE_PREALLOCATE            0x00000002
#define MMUUTIL_ALLOC_CTX_DMA_FLAGS_TO_RANDOMIZE_GPU_MAPPABLE           0x00000040
#define MMUUTIL_ALLOC_CTX_DMA_FLAGS_TO_RANDOMIZE_TYPE                   0x00000080
#define MMUUTIL_ALLOC_CTX_DMA_FLAGS_TO_RANDOMIZE_MAPPING                0x00000100
#define MMUUTIL_ALLOC_CTX_DMA_FLAGS_TO_RANDOMIZE_CACHE_SNOOP            0x00000200
#define MMUUTIL_ALLOC_CTX_DMA_FLAGS_TO_RANDOMIZE_HASH_TABLE             0x00000400

// Array of possible flags for Mapmemory()
static const UINT32 s_MapMemoryFlags[MAX_MAP_FIELDS][MAX_MAP_PARAMS] = {
    {
        LWOS33_FLAGS_ACCESS_READ_WRITE,
        LWOS33_FLAGS_ACCESS_READ_ONLY,
        LWOS33_FLAGS_ACCESS_WRITE_ONLY,
    },
    {
        LWOS33_FLAGS_SKIP_SIZE_CHECK_DISABLE,
        LWOS33_FLAGS_SKIP_SIZE_CHECK_ENABLE,
    },
    {
        LWOS33_FLAGS_MEM_SPACE_CLIENT,
        LWOS33_FLAGS_MEM_SPACE_USER
    },
    {
        LWOS33_FLAGS_MAPPING_DEFAULT,
        LWOS33_FLAGS_MAPPING_DIRECT,
        LWOS33_FLAGS_MAPPING_REFLECTED
    },

};

// Array of possible flags for Mapmemory() (in string form, useful for printing)
static const char s_MapMemoryFlagsStrings[MAX_MAP_FIELDS][MAX_MAP_PARAMS][50] = {
    {
        "LWOS33_FLAGS_ACCESS_READ_WRITE",
        "LWOS33_FLAGS_ACCESS_READ_ONLY",
        "LWOS33_FLAGS_ACCESS_WRITE_ONLY",
    },
    {
        "LWOS33_FLAGS_SKIP_SIZE_CHECK_DISABLE",
        "LWOS33_FLAGS_SKIP_SIZE_CHECK_ENABLE",
    },
    {
        "LWOS33_FLAGS_MEM_SPACE_CLIENT",
        "LWOS33_FLAGS_MEM_SPACE_USER"
    },
    {
        "LWOS33_FLAGS_MAPPING_DEFAULT",
        "LWOS33_FLAGS_MAPPING_DIRECT",
        "LWOS33_FLAGS_MAPPING_REFLECTED"
    },
};

// These defines should be used by the caller of RandomizeMapMemoryFlags()
// to specify which flags should be randomized (ACCESS/PERSISTENT etc..)
#define MMUUTIL_MAP_MEMORY_FLAGS_TO_RANDOMIZE_ACCESS                0x00000001
#define MMUUTIL_MAP_MEMORY_FLAGS_TO_RANDOMIZE_SKIP_SIZE_CHECK       0x00000002
#define MMUUTIL_MAP_MEMORY_FLAGS_TO_RANDOMIZE_MEM_SPACE             0x00000004
#define MMUUTIL_MAP_MEMORY_FLAGS_TO_RANDOMIZE_MAPPING               0x00000008

// Array of possible flags for MapmemoryDma()
static const UINT32 s_MapMemoryDmaFlags[MAX_MAPDMA_FIELDS][MAX_MAPDMA_PARAMS] = {
    {
        LWOS46_FLAGS_ACCESS_READ_WRITE,
        LWOS46_FLAGS_ACCESS_READ_ONLY,
        LWOS46_FLAGS_ACCESS_WRITE_ONLY,
    },
    {
        LWOS46_FLAGS_CACHE_SNOOP_DISABLE,
        LWOS46_FLAGS_CACHE_SNOOP_ENABLE,
    },
    {
        LWOS46_FLAGS_PAGE_SIZE_DEFAULT,
        LWOS46_FLAGS_PAGE_SIZE_4KB,
        LWOS46_FLAGS_PAGE_SIZE_BIG,
    },
#ifdef LW_VERIF_FEATURES
    {
        LWOS46_FLAGS_PRIV_DISABLE,
        LWOS46_FLAGS_PRIV_ENABLE,
    },
#else
    {
        DEFAULT_FLAG,
        DEFAULT_FLAG,
    },
#endif
    {
        LWOS46_FLAGS_DMA_OFFSET_GROWS_UP,
        LWOS46_FLAGS_DMA_OFFSET_GROWS_DOWN,
    },
    {
        LWOS46_FLAGS_DMA_OFFSET_FIXED_FALSE,
        LWOS46_FLAGS_DMA_OFFSET_FIXED_TRUE,
    },
    {
        LWOS46_FLAGS_PTE_COALESCE_LEVEL_CAP_DEFAULT,
        LWOS46_FLAGS_PTE_COALESCE_LEVEL_CAP_1,
        LWOS46_FLAGS_PTE_COALESCE_LEVEL_CAP_2,
        LWOS46_FLAGS_PTE_COALESCE_LEVEL_CAP_4,
        LWOS46_FLAGS_PTE_COALESCE_LEVEL_CAP_8,
        LWOS46_FLAGS_PTE_COALESCE_LEVEL_CAP_16,
        LWOS46_FLAGS_PTE_COALESCE_LEVEL_CAP_32,
        LWOS46_FLAGS_PTE_COALESCE_LEVEL_CAP_64,
        LWOS46_FLAGS_PTE_COALESCE_LEVEL_CAP_128,
    },
    {
        LWOS46_FLAGS_P2P_ENABLE_NO,
        LWOS46_FLAGS_P2P_ENABLE_YES,
        LWOS46_FLAGS_P2P_ENABLE_NOSLI,
    },
    {
        LWOS46_FLAGS_TLB_LOCK_DISABLE,
        LWOS46_FLAGS_TLB_LOCK_ENABLE,
    },
};

// Array of possible flags for MapmemoryDma() (in string form, useful for printing)
static const char s_MapMemoryDmaFlagsStrings[MAX_MAPDMA_FIELDS][MAX_MAPDMA_PARAMS][50] = {
    {
        "LWOS46_FLAGS_ACCESS_READ_WRITE",
        "LWOS46_FLAGS_ACCESS_READ_ONLY",
        "LWOS46_FLAGS_ACCESS_WRITE_ONLY",
    },
    {
        "LWOS46_FLAGS_CACHE_SNOOP_DISABLE",
        "LWOS46_FLAGS_CACHE_SNOOP_ENABLE",
    },
    {
        "LWOS46_FLAGS_PAGE_SIZE_DEFAULT",
        "LWOS46_FLAGS_PAGE_SIZE_4KB",
        "LWOS46_FLAGS_PAGE_SIZE_BIG",
    },
#ifdef LW_VERIF_FEATURES
    {
        "LWOS46_FLAGS_PRIV_DISABLE",
        "LWOS46_FLAGS_PRIV_ENABLE",
    },
#else
    {
        "DEFAULT_FLAG",
        "DEFAULT_FLAG",
    },
#endif
    {
        "LWOS46_FLAGS_DMA_OFFSET_GROWS_UP",
        "LWOS46_FLAGS_DMA_OFFSET_GROWS_DOWN",
    },
    {
        "LWOS46_FLAGS_DMA_OFFSET_FIXED_FALSE",
        "LWOS46_FLAGS_DMA_OFFSET_FIXED_TRUE",
    },
    {
        "LWOS46_FLAGS_PTE_COALESCE_LEVEL_CAP_DEFAULT",
        "LWOS46_FLAGS_PTE_COALESCE_LEVEL_CAP_1",
        "LWOS46_FLAGS_PTE_COALESCE_LEVEL_CAP_2",
        "LWOS46_FLAGS_PTE_COALESCE_LEVEL_CAP_4",
        "LWOS46_FLAGS_PTE_COALESCE_LEVEL_CAP_8",
        "LWOS46_FLAGS_PTE_COALESCE_LEVEL_CAP_16",
        "LWOS46_FLAGS_PTE_COALESCE_LEVEL_CAP_32",
        "LWOS46_FLAGS_PTE_COALESCE_LEVEL_CAP_64",
        "LWOS46_FLAGS_PTE_COALESCE_LEVEL_CAP_128",
    },
    {
        "LWOS46_FLAGS_P2P_ENABLE_NO",
        "LWOS46_FLAGS_P2P_ENABLE_YES",
        "LWOS46_FLAGS_P2P_ENABLE_NOSLI",
    },
    {
        "LWOS46_FLAGS_TLB_LOCK_DISABLE",
        "LWOS46_FLAGS_TLB_LOCK_ENABLE",
    },
};

// These defines should be used by the caller of RandomizeMapMemoryDmaFlags()
// to specify which flags should be randomized (ACCESS/CACHE_SNOOP etc..)
#define MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_ACCESS                    0x00000001
#define MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_CACHE_SNOOP               0x00000002
#define MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_PAGE_SIZE                 0x00000004
#define MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_PRIV                      0x00000008
#define MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_DMA_OFFSET_GROWS          0x00000010
#define MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_DMA_OFFSET_FIXED          0x00000020
#define MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_PTE_COALESCE_LEVEL_CAP    0x00000040
#define MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_P2P_ENABLE                0x00000080
#define MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_TLB_LOCK                  0x00000100

// Array of possible surface types
static const UINT32 s_AllocHeapTypes[MAX_TYPES] = {
        LWOS32_TYPE_IMAGE,
        LWOS32_TYPE_DEPTH,
        LWOS32_TYPE_TEXTURE,
        LWOS32_TYPE_VIDEO,
        LWOS32_TYPE_FONT,
        LWOS32_TYPE_LWRSOR,
        LWOS32_TYPE_DMA,
        LWOS32_TYPE_INSTANCE,
        LWOS32_TYPE_PRIMARY,
        LWOS32_TYPE_ZLWLL,
        LWOS32_TYPE_UNUSED,
        LWOS32_TYPE_SHADER_PROGRAM,
        LWOS32_TYPE_OWNER_RM,
        LWOS32_TYPE_RESERVED,
};

// Array of possible surface types (in string form, useful for printing)
static const char s_AllocHeapTypesStrings[MAX_TYPES][50] = {
        "LWOS32_TYPE_IMAGE",
        "LWOS32_TYPE_DEPTH",
        "LWOS32_TYPE_TEXTURE",
        "LWOS32_TYPE_VIDEO",
        "LWOS32_TYPE_FONT",
        "LWOS32_TYPE_LWRSOR",
        "LWOS32_TYPE_DMA",
        "LWOS32_TYPE_INSTANCE",
        "LWOS32_TYPE_PRIMARY",
        "LWOS32_TYPE_ZLWLL",
        "LWOS32_TYPE_UNUSED",
        "LWOS32_TYPE_SHADER_PROGRAM",
        "LWOS32_TYPE_OWNER_RM",
        "LWOS32_TYPE_RESERVED",
};

// Array of possible attributes for surface allocs on heap
static const UINT32 s_AllocHeapAttrFlags[MAX_ATTR_FIELDS][MAX_ATTR_PARAMS] = {
    {
        LWOS32_ATTR_DEPTH_UNKNOWN,
        LWOS32_ATTR_DEPTH_8,
        LWOS32_ATTR_DEPTH_16,
        LWOS32_ATTR_DEPTH_24,
        LWOS32_ATTR_DEPTH_32,
        LWOS32_ATTR_DEPTH_64,
        LWOS32_ATTR_DEPTH_128,
    },
    {
        LWOS32_ATTR_COMPR_COVG_DEFAULT,
        LWOS32_ATTR_COMPR_COVG_PROVIDED,
    },
    {
        LWOS32_ATTR_AA_SAMPLES_1,
        LWOS32_ATTR_AA_SAMPLES_2,
        LWOS32_ATTR_AA_SAMPLES_4,
        LWOS32_ATTR_AA_SAMPLES_4_ROTATED,
        LWOS32_ATTR_AA_SAMPLES_6,
        LWOS32_ATTR_AA_SAMPLES_8,
        LWOS32_ATTR_AA_SAMPLES_16,
        LWOS32_ATTR_AA_SAMPLES_4_VIRTUAL_8,
        LWOS32_ATTR_AA_SAMPLES_4_VIRTUAL_16,
        LWOS32_ATTR_AA_SAMPLES_8_VIRTUAL_16,
        LWOS32_ATTR_AA_SAMPLES_8_VIRTUAL_32,
    },
    {
        LWOS32_ATTR_COMPR_NONE,
        LWOS32_ATTR_COMPR_REQUIRED,
        LWOS32_ATTR_COMPR_ANY,
    },
    {
        LWOS32_ATTR_FORMAT_PITCH,
        LWOS32_ATTR_FORMAT_SWIZZLED,
        LWOS32_ATTR_FORMAT_BLOCK_LINEAR,
    },
    {
        LWOS32_ATTR_Z_TYPE_FIXED,
        LWOS32_ATTR_Z_TYPE_FLOAT,
    },
    {
        LWOS32_ATTR_ZS_PACKING_Z24S8,
        LWOS32_ATTR_ZS_PACKING_S8Z24,
        LWOS32_ATTR_ZS_PACKING_Z32,
        LWOS32_ATTR_ZS_PACKING_Z24X8,
        LWOS32_ATTR_ZS_PACKING_X8Z24 ,
        LWOS32_ATTR_ZS_PACKING_Z32_X24S8,
        LWOS32_ATTR_ZS_PACKING_X8Z24_X24S8,
        LWOS32_ATTR_ZS_PACKING_Z16,
    },
    {
        LWOS32_ATTR_COLOR_PACKING_A8R8G8B8,
        LWOS32_ATTR_COLOR_PACKING_X8R8G8B8,
    },

   {
#ifdef  LW_VERIF_FEATURES
        LWOS32_ATTR_FORMAT_OVERRIDE_OFF,
        LWOS32_ATTR_FORMAT_OVERRIDE_ON,
#else
       DEFAULT_FLAG,
       DEFAULT_FLAG,
#endif
   },

    {
        LWOS32_ATTR_PAGE_SIZE_DEFAULT,
        LWOS32_ATTR_PAGE_SIZE_4KB,
        LWOS32_ATTR_PAGE_SIZE_BIG,
   },
   {
        LWOS32_ATTR_LOCATION_VIDMEM,
        LWOS32_ATTR_LOCATION_PCI,
        LWOS32_ATTR_LOCATION_ANY,
   },
   {
        LWOS32_ATTR_PHYSICALITY_DEFAULT,
        LWOS32_ATTR_PHYSICALITY_NONCONTIGUOUS,
        LWOS32_ATTR_PHYSICALITY_CONTIGUOUS,
   },
   {
        LWOS32_ATTR_COHERENCY_UNCACHED,
        LWOS32_ATTR_COHERENCY_CACHED,
        LWOS32_ATTR_COHERENCY_WRITE_COMBINE,
        LWOS32_ATTR_COHERENCY_WRITE_THROUGH,
        LWOS32_ATTR_COHERENCY_WRITE_PROTECT,
        LWOS32_ATTR_COHERENCY_WRITE_BACK,
   },
};

// Array of possible attributes for surface allocs on heap (in string form, useful for printing)
static const char s_AllocHeapAttrFlagsStrings[MAX_ATTR_FIELDS][MAX_ATTR_PARAMS][50] = {
    {
        "LWOS32_ATTR_DEPTH_UNKNOWN",
        "LWOS32_ATTR_DEPTH_8",
        "LWOS32_ATTR_DEPTH_16",
        "LWOS32_ATTR_DEPTH_24",
        "LWOS32_ATTR_DEPTH_32",
        "LWOS32_ATTR_DEPTH_64",
        "LWOS32_ATTR_DEPTH_128",
    },
    {
        "LWOS32_ATTR_COMPR_COVG_DEFAULT",
        "LWOS32_ATTR_COMPR_COVG_PROVIDED",
    },
    {
        "LWOS32_ATTR_AA_SAMPLES_1",
        "LWOS32_ATTR_AA_SAMPLES_2",
        "LWOS32_ATTR_AA_SAMPLES_4",
        "LWOS32_ATTR_AA_SAMPLES_4_ROTATED",
        "LWOS32_ATTR_AA_SAMPLES_6",
        "LWOS32_ATTR_AA_SAMPLES_8",
        "LWOS32_ATTR_AA_SAMPLES_16",
        "LWOS32_ATTR_AA_SAMPLES_4_VIRTUAL_8",
        "LWOS32_ATTR_AA_SAMPLES_4_VIRTUAL_16",
        "LWOS32_ATTR_AA_SAMPLES_8_VIRTUAL_16",
        "LWOS32_ATTR_AA_SAMPLES_8_VIRTUAL_32",

    },
    {
        "LWOS32_ATTR_COMPR_NONE",
        "LWOS32_ATTR_COMPR_REQUIRED",
        "LWOS32_ATTR_COMPR_ANY",
    },
    {
        "LWOS32_ATTR_FORMAT_PITCH",
        "LWOS32_ATTR_FORMAT_SWIZZLED",
        "LWOS32_ATTR_FORMAT_BLOCK_LINEAR",
    },
    {
        "LWOS32_ATTR_Z_TYPE_FIXED",
        "LWOS32_ATTR_Z_TYPE_FLOAT",
    },
    {
        "LWOS32_ATTR_ZS_PACKING_Z24S8",
        "LWOS32_ATTR_ZS_PACKING_S8Z24",
        "LWOS32_ATTR_ZS_PACKING_Z32",
        "LWOS32_ATTR_ZS_PACKING_Z24X8",
        "LWOS32_ATTR_ZS_PACKING_X8Z24",
        "LWOS32_ATTR_ZS_PACKING_Z32_X24S8",
        "LWOS32_ATTR_ZS_PACKING_X8Z24_X24S8",
        "LWOS32_ATTR_ZS_PACKING_Z16",
    },
    {
        "LWOS32_ATTR_COLOR_PACKING_A8R8G8B8",
        "LWOS32_ATTR_COLOR_PACKING_X8R8G8B8",
    },

   {
#ifdef  LW_VERIF_FEATURES
        "LWOS32_ATTR_FORMAT_OVERRIDE_OFF",
        "LWOS32_ATTR_FORMAT_OVERRIDE_ON",
#else
       "DEFAULT_FLAG",
       "DEFAULT_FLAG",
#endif
   },

    {
        "LWOS32_ATTR_PAGE_SIZE_DEFAULT",
        "LWOS32_ATTR_PAGE_SIZE_4KB",
        "LWOS32_ATTR_PAGE_SIZE_BIG",
        "LWOS32_ATTR_PAGE_SIZE_BOTH",
   },
   {
        "LWOS32_ATTR_LOCATION_VIDMEM",
        "LWOS32_ATTR_LOCATION_PCI",
        "LWOS32_ATTR_LOCATION_ANY",
   },
   {
        "LWOS32_ATTR_PHYSICALITY_DEFAULT",
        "LWOS32_ATTR_PHYSICALITY_NONCONTIGUOUS",
        "LWOS32_ATTR_PHYSICALITY_CONTIGUOUS",
   },
   {
        "LWOS32_ATTR_COHERENCY_UNCACHED",
        "LWOS32_ATTR_COHERENCY_CACHED",
        "LWOS32_ATTR_COHERENCY_WRITE_COMBINE",
        "LWOS32_ATTR_COHERENCY_WRITE_THROUGH",
        "LWOS32_ATTR_COHERENCY_WRITE_PROTECT",
        "LWOS32_ATTR_COHERENCY_WRITE_BACK",
   },
};

// These defines should be used by the caller of RandomizeAllocHeapAttrFlags()
// to specify which flags should be randomized (DEPTH/COMPR_COVG/AA_SAMPLES etc..)
#define MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_DEPTH                   0x00000001
#define MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_COMPR_COVG              0x00000002
#define MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_AA_SAMPLES              0x00000004
#define MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_COMPR                   0x00000020
#define MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_FORMAT                  0x00000080
#define MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_Z_TYPE                  0x00000100
#define MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_ZS_PACKING              0x00000200
#define MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_COLOR_PACKING           0x00000400
#define MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_FORMAT_OVERRIDE         0x00000800
#define MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_PAGE_SIZE_HEAP          0x00001000
#define MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_LOCATION_HEAP           0x00002000
#define MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_PHYSICALITY_HEAP        0x00004000
#define MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_COHERENCY_HEAP          0x00008000

// Array of possible ATTR2 flags for surface allocs on heap
static const UINT32 s_AllocHeapAttr2Flags[MAX_ATTR_FIELDS][MAX_ATTR_PARAMS] = {
    {
        LWOS32_ATTR2_ZBC_DEFAULT,
        LWOS32_ATTR2_ZBC_PREFER_NO_ZBC,
        LWOS32_ATTR2_ZBC_PREFER_ZBC,
    },
    {
        LWOS32_ATTR2_GPU_CACHEABLE_DEFAULT,
        LWOS32_ATTR2_GPU_CACHEABLE_YES,
        LWOS32_ATTR2_GPU_CACHEABLE_NO,
    },
    {
        LWOS32_ATTR2_P2P_GPU_CACHEABLE_DEFAULT,
        LWOS32_ATTR2_P2P_GPU_CACHEABLE_YES,
        LWOS32_ATTR2_P2P_GPU_CACHEABLE_NO,
    },
};

// Array of possible ATTR2 flags for surface allocs on heap (in string form, useful for printing)
static const char s_AllocHeapAttr2FlagsStrings[MAX_ATTR_FIELDS][MAX_ATTR_PARAMS][50] = {
    {
        "LWOS32_ATTR2_ZBC_DEFAULT",
        "LWOS32_ATTR2_ZBC_PREFER_NO_ZBC",
        "LWOS32_ATTR2_ZBC_PREFER_ZBC",
    },
    {
        "LWOS32_ATTR2_GPU_CACHEABLE_DEFAULT",
        "LWOS32_ATTR2_GPU_CACHEABLE_YES",
        "LWOS32_ATTR2_GPU_CACHEABLE_NO",
    },
    {
        "LWOS32_ATTR2_P2P_GPU_CACHEABLE_DEFAULT",
        "LWOS32_ATTR2_P2P_GPU_CACHEABLE_YES",
        "LWOS32_ATTR2_P2P_GPU_CACHEABLE_NO",
    },
};

// These defines should be used by the caller of RandomizeAllocHeapAttr2Flags()
// to specify which flags should be randomized (ZBC/GPU_CACHEABLE/P2P_GPU_CACHEABLE etc..)
#define MMUUTIL_ALLOC_HEAP_ATTR2_FLAGS_TO_RANDOMIZE_ZBC                     0x00000001
#define MMUUTIL_ALLOC_HEAP_ATTR2_FLAGS_TO_RANDOMIZE_GPU_CACHEABLE           0x00000002
#define MMUUTIL_ALLOC_HEAP_ATTR2_FLAGS_TO_RANDOMIZE_P2P_GPU_CACHEABLE       0x00000004

#endif  // INCLUDED_MMUUTIL_H
