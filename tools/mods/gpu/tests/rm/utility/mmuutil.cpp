/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2009,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file mmuutil.cpp
//!
//! \brief MMU allocation flags randomizing utility.
//!
//! Utility for randomizing on different flags used while memory allocation
//! through AllocMemory() and VidHeapAllocSize(), flags used in AllocContextDma()
//! MapMemory(), MapMemoryDma().
//!
//!

#include "mmuutil.h"
#include "core/include/xp.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/memcheck.h"

//!
//! @brief MmuUtil default constructor.
//!
//! Initializes seed and precision with default values.
//!
//! @param[in]  NONE
//! @param[out] NONE
//-----------------------------------------------------------------------------
MmuUtil::MmuUtil()
{
    // query performance counter to use as seed to init Random Class object
    // TODO : Bug 491758: Debug failures oclwring due to random seed
    //m_Seed = (UINT32)Xp::QueryPerformanceCounter();

    // Using constant seed for now
    m_Seed = 0x12345679;  // TODO: Original seed 0x12345678 hits RM_ASSERT
    m_Random.SeedRandom(m_Seed);

    // use default precision value of 1K
    m_Precision = 1000;
}

//!
//! @brief MmuUtil constructor with seed and precision args.
//!
//! Initializes seed and precision with caller specified values.
//!
//! @param[in]  seed       User supplied seed that will be used as seed for
//!                        random class object initialization
//!
//! @param[in]  precision  User supplied precision that will be saved and used
//!                        in callwlation of weighted samples during
//!                        randomzation of flags
//!
//! @param[out] NONE
//-----------------------------------------------------------------------------
MmuUtil::MmuUtil
(
    UINT32 seed,
    UINT32 precision
)
{
    // use client supplied seed
    m_Random.SeedRandom(seed);
    m_Seed = seed;
    m_Precision = precision;
}

//!
//! @brief MmuUtil default destructor.
//!
//! Default destructor, does nothing.
//!
//! @param[in]  NONE
//! @param[out] NONE
//-----------------------------------------------------------------------------
MmuUtil::~MmuUtil()
{
}

//!
//! @brief Returns the seed used in the utility.
//!
//! @param[in]  NONE
//! @param[out] The seed used in the utility (returned as returned value).

//! @returns    The seed used in the utility.
//-----------------------------------------------------------------------------
UINT32 MmuUtil::getSeed()
{
    return m_Seed;
}

//!
//! @brief Algorithm for generating weighted samples.
//!
    //! This function generates the samples/flags basing on the probabilities
//! attached to them. We will be generating i.i.d sequences
//! (independent indentically distributed) and then basing on the
//! probabilites, we grouped them lwmulatively into zones and then generate
//! the independent desired distributed sequences
//!
//! @param[in]  probability
//!             An array of double holding the probabilities of each values,
//!             used as mentioned above.
//!
//! @param[in]  fieldLength
//!             Maximum no. of values for this generation.
//!
//!
//! @param[out] The index that has been chosen randomly (returned as
//!             returned value).

//! @returns    The index that has been chosen randomly.
//-----------------------------------------------------------------------------
UINT32 MmuUtil::getWeightedSamples
(
    const double * probability,
    UINT32 fieldLength
)
{
    UINT32 precision = m_Precision;
    UINT32 number, i, desired;
    UINT32 *zones;
    double x = 0;

    zones = new UINT32[fieldLength + 1];

    number = m_Random.GetRandom(0, precision - 1);
    zones[0] = 0;
    zones[fieldLength] = precision;

    for (i = 1; i < fieldLength; i++)
    {
        x = x + probability[i-1];
        zones[i] = (UINT32)((x) * 1000) ;
    }

    for (i = 0; i < fieldLength; i++)
    {
        if ((zones[i] <= number) && (number < zones[i+1]))
        {
            desired = i;
            delete []zones;
            zones = NULL;
            return desired;
        }
    }
    delete []zones;
    zones = NULL;
    return INVALID;
}

//!
//! @brief Function to randomize on memory classes used by AllocMemory() fn.
//!
//! Returns a randomized memory class, randomized from
//! one of the values in s_AllocMemoryClasses array
//!
//! @param[in]  weights
//!             MmuUtil weights structure, containing all the weights
//!
//! @param[out] The memory class that has been chosen randomly (returned as
//!             returned value).

//! @returns    The memory class that has been chosen randomly.
//-----------------------------------------------------------------------------
UINT32 MmuUtil::RandomizeAllocMemoryClasses
(
    MmuUtilWeights * weights
)
{
    UINT32 classNum = 0;
    classNum = getWeightedSamples(weights[0].sysClassWeights.probability, MAX_CLASSES);
    return s_AllocMemoryClasses[classNum];
}

//!
//! @brief Function to randomize on flags used by AllocMemory() fn.
//!
//! Returns randomized flags for use with AllocMemory()
//! Randomizes flags from possible values stored in s_AllocMemoryFlags array.
//! The caller can specify which flags it wants to randomize in the input
//! parameter 'allocMemoryFlagsToRandomize'
//!
//! @param[in]  weights
//!             MmuUtil weights structure, containing all the weights
//!
//! @param[in]  allocMemoryFlagsToRandomize
//!             This parameter specifies which flags should be randomized upon.
//!             The caller can choose various flags by 'OR'ing the defines like
//!             'MMUUTIL_ALLOC_SYSMEM_FLAGS_TO_RANDOMIZE_PHYSICALITY' etc.
//!             See 'mmuutil.h' for all such defines.
//!             See 'mmutester.cpp' for an example of how to use this.
//!
//! @param[out] The flags value that has been chosen randomly (returned as
//!             returned value).

//! @returns    The flags value that has been chosen randomly.
//-----------------------------------------------------------------------------
UINT32 MmuUtil::RandomizeAllocMemoryFlags
(
    MmuUtilWeights * weights,
    UINT32 allocMemoryFlagsToRandomize
)
{
    UINT32 flags = 0;
    UINT32 flagIndex = 0;
    for(int i = 0; i < MAX_MEMORY_FIELDS; i++)
    {
        switch(i)
        {
            case PHYSICALITY:
            {
                if (allocMemoryFlagsToRandomize & MMUUTIL_ALLOC_SYSMEM_FLAGS_TO_RANDOMIZE_PHYSICALITY)
                {
                    flagIndex = getWeightedSamples(weights[0].sysMemWeights[i].probability, PHYSICALITY_FIELD);
                    flags = flags | (s_AllocMemoryFlags[i][flagIndex]<< DRF_SHIFT(LWOS02_FLAGS_PHYSICALITY));
                }
                break;
            }

            case LOCATION:
            {
                if (allocMemoryFlagsToRandomize & MMUUTIL_ALLOC_SYSMEM_FLAGS_TO_RANDOMIZE_LOCATION)
                {
                    flagIndex = getWeightedSamples(weights[0].sysMemWeights[i].probability, LOCATION_FIELD);
                    flags = flags | (s_AllocMemoryFlags[i][flagIndex]<< DRF_SHIFT(LWOS02_FLAGS_LOCATION));
                }
                break;
            }

            case COHERENCE:
            {
                if (allocMemoryFlagsToRandomize & MMUUTIL_ALLOC_SYSMEM_FLAGS_TO_RANDOMIZE_LOCATION)
                {
                    flagIndex = getWeightedSamples(weights[0].sysMemWeights[i].probability, COHERENCE_FIELD);
                    flags = flags | (s_AllocMemoryFlags[i][flagIndex]<<DRF_SHIFT(LWOS02_FLAGS_COHERENCY));
                }
                break;
            }

            case MAPPING:
            {
                if (allocMemoryFlagsToRandomize & MMUUTIL_ALLOC_SYSMEM_FLAGS_TO_RANDOMIZE_MAPPING)
                {
                    flagIndex = getWeightedSamples(weights[0].sysMemWeights[i].probability, MAPPING_FIELD);
                    flags = flags | (s_AllocMemoryFlags[i][flagIndex]<< DRF_SHIFT(LWOS02_FLAGS_MAPPING));
                }
                break;
            }

            default:
            {
                Printf(Tee::PriHigh,"\nIlwalid SYS_MEMORY Field\n");
                return 0;
            }

        }
    }
    return flags;
}

//!
//! @brief Function to randomize on flags used by AllocContextDma() fn.
//!
//! Returns randomized flags for use with AllocContextDma()
//! Randomizes flags from possible values stored in s_AllocCtxDmaFlags array.
//! The caller can specify which flags it wants to randomize in the input
//! parameter 'allocCtxDmaFlagsToRandomize'
//!
//! @param[in]  weights
//!             MmuUtil weights structure, containing all the weights
//!
//! @param[in]  allocCtxDmaFlagsToRandomize
//!             This parameter specifies which flags should be randomized upon.
//!             The caller can choose various flags by 'OR'ing the defines like
//!             'MMUUTIL_ALLOC_CTX_DMA_FLAGS_TO_RANDOMIZE_ACCESS' etc.
//!             See 'mmuutil.h' for all such defines.
//!             See 'mmutester.cpp' for an example of how to use this.
//!
//! @param[out] The flags value that has been chosen randomly (returned as
//!             returned value).

//! @returns    The flags value that has been chosen randomly.
//-----------------------------------------------------------------------------
UINT32 MmuUtil::RandomizeAllocCtxDmaFlags
(
    MmuUtilWeights * weights,
    UINT32 allocCtxDmaFlagsToRandomize
)
{
    UINT32 ctxFlags  = 0;
    UINT32 flagIndex = 0;

    for(int i = 0; i < MAX_DMA_FIELDS; i++)
    {
        switch(i)
        {
            case ACCESS:
            {
                if (allocCtxDmaFlagsToRandomize & MMUUTIL_ALLOC_CTX_DMA_FLAGS_TO_RANDOMIZE_ACCESS)
                {
                    flagIndex = getWeightedSamples(weights[0].ctxDmaWeights[i].probability, DMA_ACCESS_FIELD);
                    ctxFlags = ctxFlags | (s_AllocCtxDmaFlags[i][flagIndex]<< DRF_SHIFT(LWOS03_FLAGS_ACCESS));
                }
                break;
            }

            case PREALLOCATE:
            {
                if (allocCtxDmaFlagsToRandomize & MMUUTIL_ALLOC_CTX_DMA_FLAGS_TO_RANDOMIZE_PREALLOCATE)
                {
                    flagIndex = getWeightedSamples(weights[0].ctxDmaWeights[i].probability, PREALLOCATE_FIELD);
                    ctxFlags = ctxFlags |(s_AllocCtxDmaFlags[i][flagIndex]<< DRF_SHIFT(LWOS03_FLAGS_PREALLOCATE));
                }
                break;
            }

            case GPU_MAPPABLE:
            {
                if (allocCtxDmaFlagsToRandomize & MMUUTIL_ALLOC_CTX_DMA_FLAGS_TO_RANDOMIZE_GPU_MAPPABLE)
                {
                    flagIndex = getWeightedSamples(weights[0].ctxDmaWeights[i].probability, MAPPABLE_FIELD);
                    ctxFlags = ctxFlags | (s_AllocCtxDmaFlags[i][flagIndex]<< DRF_SHIFT(LWOS03_FLAGS_GPU_MAPPABLE));
                }
                break;
            }

            case TYPE:
            {
                if (allocCtxDmaFlagsToRandomize & MMUUTIL_ALLOC_CTX_DMA_FLAGS_TO_RANDOMIZE_TYPE)
                {
                    flagIndex = getWeightedSamples(weights[0].ctxDmaWeights[i].probability, FLAG_TYPE_FIELD);
                    ctxFlags = ctxFlags | (s_AllocCtxDmaFlags[i][flagIndex]<<DRF_SHIFT(LWOS03_FLAGS_TYPE));
                }
                break;
            }

            case ALLOCCTXDMA_MAPPING:
            {
                if (allocCtxDmaFlagsToRandomize & MMUUTIL_ALLOC_CTX_DMA_FLAGS_TO_RANDOMIZE_MAPPING)
                {
                    flagIndex = getWeightedSamples(weights[0].ctxDmaWeights[i].probability, ALLOCCTXDMA_MAPPING_FIELD);
                    ctxFlags = ctxFlags | (s_AllocCtxDmaFlags[i][flagIndex]<<DRF_SHIFT(LWOS03_FLAGS_MAPPING));
                }
                break;
            }

            case CACHE_SNOOP:
            {
                if (allocCtxDmaFlagsToRandomize & MMUUTIL_ALLOC_CTX_DMA_FLAGS_TO_RANDOMIZE_CACHE_SNOOP)
                {
                    flagIndex = getWeightedSamples(weights[0].ctxDmaWeights[i].probability, CACHE_SNOOP_FIELD);
                    ctxFlags = ctxFlags | (s_AllocCtxDmaFlags[i][flagIndex]<<DRF_SHIFT(LWOS03_FLAGS_CACHE_SNOOP));
                }
                break;
            }

            case HASH_TABLE:
            {
                if (allocCtxDmaFlagsToRandomize & MMUUTIL_ALLOC_CTX_DMA_FLAGS_TO_RANDOMIZE_HASH_TABLE)
                {
                    flagIndex = getWeightedSamples(weights[0].ctxDmaWeights[i].probability, HASH_TABLE_FIELD);
                    ctxFlags = ctxFlags | (s_AllocCtxDmaFlags[i][flagIndex]<< DRF_SHIFT(LWOS03_FLAGS_HASH_TABLE));
                }
                break;
            }

            default:
            {
                Printf(Tee::PriHigh,"\nIlwalid CTXDMA Field\n");
                return 0;
            }
        }
    }

    return ctxFlags;
}

//!
//! @brief Function to randomize on flags used by MapMemory() fn.
//!
//! Returns randomized flags for use with MapMemory()
//! Randomizes flags from possible values stored in s_MapMemoryFlags array.
//! The caller can specify which flags it wants to randomize in the input
//! parameter 'mapMemoryFlagsToRandomize'
//!
//! @param[in]  weights
//!             MmuUtil weights structure, containing all the weights
//!
//! @param[in]  mapMemoryFlagsToRandomize
//!             This parameter specifies which flags should be randomized upon.
//!             The caller can choose various flags by 'OR'ing the defines like
//!             'MMUUTIL_MAP_MEMORY_FLAGS_TO_RANDOMIZE_ACCESS' etc.
//!             See 'mmuutil.h' for all such defines.
//!             See 'mmutester.cpp' for an example of how to use this.
//!
//! @param[out] The flags value that has been chosen randomly (returned as
//!             returned value).

//! @returns    The flags value that has been chosen randomly.
//-----------------------------------------------------------------------------
UINT32 MmuUtil::RandomizeMapMemoryFlags
(
    MmuUtilWeights * weights,
    UINT32 mapMemoryFlagsToRandomize
)
{
    UINT32 mapMemoryFlags = 0;
    UINT32 flagIndex      = 0;

    for (int i = 0; i < MAX_MAP_FIELDS; i++)
    {
        switch(i)
        {
            case MAP_ACCESS:
            {
                if (mapMemoryFlagsToRandomize & MMUUTIL_MAP_MEMORY_FLAGS_TO_RANDOMIZE_ACCESS)
                {
                    flagIndex = getWeightedSamples(weights[0].mapMemWeights[i].probability, MAP_ACCESS_FIELD);
                    mapMemoryFlags = mapMemoryFlags | (s_MapMemoryFlags[i][flagIndex]<<DRF_SHIFT(LWOS33_FLAGS_ACCESS));
                }
                break;
            }

            case SKIP_SIZE_CHECK:
            {
                if (mapMemoryFlagsToRandomize & MMUUTIL_MAP_MEMORY_FLAGS_TO_RANDOMIZE_SKIP_SIZE_CHECK)
                {
                    flagIndex = getWeightedSamples(weights[0].mapMemWeights[i].probability, SIZE_CHECK_FIELD);
                    mapMemoryFlags = mapMemoryFlags | (s_MapMemoryFlags[i][flagIndex]<< DRF_SHIFT(LWOS33_FLAGS_SKIP_SIZE_CHECK));
                }
                break;
            }

            case MEM_SPACE:
            {
                if (mapMemoryFlagsToRandomize & MMUUTIL_MAP_MEMORY_FLAGS_TO_RANDOMIZE_MEM_SPACE)
                {
                    flagIndex = getWeightedSamples(weights[0].mapMemWeights[i].probability, MEM_SPACE_FIELD);
                    mapMemoryFlags = mapMemoryFlags | (s_MapMemoryFlags[i][flagIndex]<< DRF_SHIFT(LWOS33_FLAGS_MEM_SPACE));
                }
                break;
            }

            case MAPMEMORY_MAPPING:
            {
                if (mapMemoryFlagsToRandomize & MMUUTIL_MAP_MEMORY_FLAGS_TO_RANDOMIZE_MAPPING)
                {
                    flagIndex = getWeightedSamples(weights[0].mapMemWeights[i].probability, MAPMEMORY_MAPPING_FIELD);
                    mapMemoryFlags = mapMemoryFlags | (s_MapMemoryFlags[i][flagIndex]<< DRF_SHIFT(LWOS33_FLAGS_MAPPING));
                }
                break;
            }

            default:
            {
                Printf(Tee::PriHigh,"\nIlwalid MAPPING Field\n");
                return 0;
            }
        }
    }
    return mapMemoryFlags;
}

//!
//! @brief Function to randomize on flags used by MapMemoryDma() fn.
//!
//! Returns randomized flags for use with MapMemoryDma()
//! Randomizes flags from possible values stored in s_MapMemoryDmaFlags array.
//! The caller can specify which flags it wants to randomize in the input
//! parameter 'mapMemoryDmaFlagsToRandomize'
//!
//! @param[in]  weights
//!             MmuUtil weights structure, containing all the weights
//!
//! @param[in]  mapMemoryDmaFlagsToRandomize
//!             This parameter specifies which flags should be randomized upon.
//!             The caller can choose various flags by 'OR'ing the defines like
//!             'MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_ACCESS' etc.
//!             See 'mmuutil.h' for all such defines.
//!             See 'mmutester.cpp' for an example of how to use this.
//!
//! @param[out] The flags value that has been chosen randomly (returned as
//!             returned value).

//! @returns    The flags value that has been chosen randomly.
//-----------------------------------------------------------------------------
UINT32 MmuUtil::RandomizeMapMemoryDmaFlags
(
    MmuUtilWeights * weights,
    UINT32 mapMemoryDmaFlagsToRandomize
)
{
    UINT32 mapMemoryDmaFlags = 0;
    UINT32 flagIndex         = 0;

    for (int i = 0; i < MAX_MAPDMA_FIELDS; i++)
    {

        switch(i)
        {
            case MAPDMA_ACCESS:
            {
                if (mapMemoryDmaFlagsToRandomize & MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_ACCESS)
                {
                    flagIndex = getWeightedSamples(weights[0].mapDmaWeights[i].probability, MAPDMA_ACCESS_FIELD + 1);
                    mapMemoryDmaFlags = mapMemoryDmaFlags| (s_MapMemoryDmaFlags[i][flagIndex]<<DRF_SHIFT(LWOS46_FLAGS_ACCESS));
                }
                break;
            }

            case MAPDMA_CACHE_SNOOP:
            {
                if (mapMemoryDmaFlagsToRandomize & MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_CACHE_SNOOP)
                {
                    flagIndex = getWeightedSamples(weights[0].mapDmaWeights[i].probability, MAPDMA_CACHE_SNOOP_FIELD + 1);
                    mapMemoryDmaFlags = mapMemoryDmaFlags|(s_MapMemoryDmaFlags[i][flagIndex]<<DRF_SHIFT(LWOS46_FLAGS_CACHE_SNOOP));
                }
                break;
            }

            case MAPDMA_PAGE_SIZE_CHECK:
            {
                if (mapMemoryDmaFlagsToRandomize & MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_PAGE_SIZE)
                {
                    flagIndex = getWeightedSamples(weights[0].mapDmaWeights[i].probability, MAPDMA_PAGE_SIZE_CHECK_FIELD + 1);
                    mapMemoryDmaFlags = mapMemoryDmaFlags|(s_MapMemoryDmaFlags[i][flagIndex]<<DRF_SHIFT(LWOS46_FLAGS_PAGE_SIZE));
                }
                break;
            }

             case MAPDMA_PRIV:
             {
#ifdef LW_VERIF_FEATURES
                if (mapMemoryDmaFlagsToRandomize & MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_PRIV)
                {
                    flagIndex = getWeightedSamples(weights[0].mapDmaWeights[i].probability, MAPDMA_PRIV_FIELD + 1);
                    mapMemoryDmaFlags = mapMemoryDmaFlags| (s_MapMemoryDmaFlags[i][flagIndex]<<DRF_SHIFT(LWOS46_FLAGS_PRIV));
                }
#endif
                break;
            }

            case MAPDMA_DMA_OFFSET_GROWS:
            {
                if (mapMemoryDmaFlagsToRandomize & MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_DMA_OFFSET_GROWS)
                {
                    flagIndex = getWeightedSamples(weights[0].mapDmaWeights[i].probability, MAPDMA_DMA_OFFSET_GROWS_FIELD + 1);
                    mapMemoryDmaFlags = mapMemoryDmaFlags|(s_MapMemoryDmaFlags[i][flagIndex]<<DRF_SHIFT(LWOS46_FLAGS_DMA_OFFSET_GROWS));
                }
                break;
            }

            case MAPDMA_DMA_OFFSET_FIXED:
            {
                if (mapMemoryDmaFlagsToRandomize & MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_DMA_OFFSET_FIXED)
                {
                    flagIndex = getWeightedSamples(weights[0].mapDmaWeights[i].probability, MAPDMA_DMA_OFFSET_FIXED_FIELD + 1);
                    mapMemoryDmaFlags = mapMemoryDmaFlags|(s_MapMemoryDmaFlags[i][flagIndex]<<DRF_SHIFT(LWOS46_FLAGS_DMA_OFFSET_FIXED));
                }
                break;
            }

            case MAPDMA_PTE_COALESCE_LEVEL:
            {
                 if (mapMemoryDmaFlagsToRandomize & MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_PTE_COALESCE_LEVEL_CAP)
                 {
                    flagIndex = getWeightedSamples(weights[0].mapDmaWeights[i].probability, MAPDMA_PTE_COALESCE_LEVEL_FIELD + 1);
                    mapMemoryDmaFlags = mapMemoryDmaFlags|(s_MapMemoryDmaFlags[i][flagIndex]<<DRF_SHIFT(LWOS46_FLAGS_PTE_COALESCE_LEVEL_CAP));
                 }
                break;
            }

            case MAPDMA_FLAGS_P2P_ENABLE:
            {
                if (mapMemoryDmaFlagsToRandomize & MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_P2P_ENABLE)
                {
                    flagIndex = getWeightedSamples(weights[0].mapDmaWeights[i].probability, MAPDMA_FLAGS_P2P_ENABLE_FIELD + 1);
                    mapMemoryDmaFlags = mapMemoryDmaFlags|(s_MapMemoryDmaFlags[i][flagIndex]<<DRF_SHIFT(LWOS46_FLAGS_P2P_ENABLE));
                }
                break;
            }

            case MAPDMA_TLB_LOCK:
            {
                if (mapMemoryDmaFlagsToRandomize & MMUUTIL_MAP_MEMORY_DMA_FLAGS_TO_RANDOMIZE_TLB_LOCK)
                {
                    flagIndex = getWeightedSamples(weights[0].mapDmaWeights[i].probability, MAPDMA_TLB_LOCK_FIELD + 1);
                    mapMemoryDmaFlags = mapMemoryDmaFlags|(s_MapMemoryDmaFlags[i][flagIndex]<<DRF_SHIFT(LWOS46_FLAGS_TLB_LOCK));
                }
                break;
            }
            default:
            {
                Printf(Tee::PriHigh,"\nIlwalid MAP DMA Field\n");
                return 0;
            }
        }
    }
    return mapMemoryDmaFlags;
}

//!
//! @brief Function to randomize on type param used by VidHeapAllocSize() fn.
//!
//! Returns randomized type for use with VidHeapAllocSize()
//! Randomizes type from possible values stored in s_AllocHeapTypes array.
//!
//! @param[in]  weights
//!             MmuUtil weights structure, containing all the weights
//!
//! @param[out] The type value that has been chosen randomly (returned as
//!             returned value).

//! @returns    The type value that has been chosen randomly.
//-----------------------------------------------------------------------------
UINT32 MmuUtil::RandomizeAllocHeapTypes
(
    MmuUtilWeights * weights
)
{
    UINT32 vidTypeIndex = 0;
    vidTypeIndex = getWeightedSamples(weights[0].vidTypeWeights.probability, MAX_TYPES);
    return s_AllocHeapTypes[vidTypeIndex];
}

//!
//! @brief Function to randomize on attr flags used by VidHeapAllocSize() fn.
//!
//! Returns randomized attr flags for use with VidHeapAllocSize()
//! Randomizes flags from possible values stored in s_AllocHeapAttrFlags array.
//! The caller can specify which flags it wants to randomize in the input
//! parameter 'allocHeapAttrFlagsToRandomize'
//!
//! @param[in]  weights
//!             MmuUtil weights structure, containing all the weights
//!
//! @param[in]  allocHeapAttrFlagsToRandomize
//!             This parameter specifies which attr flags should be randomized upon.
//!             The caller can choose various flags by 'OR'ing the defines like
//!             'MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_DEPTH' etc.
//!             See 'mmuutil.h' for all such defines.
//!             See 'mmutester.cpp' for an example of how to use this.
//!
//! @param[out] The attr flags value that has been chosen randomly (returned as
//!             returned value).

//! @returns    The attr flags value that has been chosen randomly.
//-----------------------------------------------------------------------------
UINT32 MmuUtil::RandomizeAllocHeapAttrFlags
(
    MmuUtilWeights * weights,
    UINT32 allocHeapAttrFlagsToRandomize
)
{
    UINT32 allocHeapAttrFlags = 0;
    UINT32 flagIndex          = 0;

    for(int i = 0; i < MAX_ATTR_FIELDS; i++)
    {
        switch(i)
        {
            case DEPTH:
            {
                if (allocHeapAttrFlagsToRandomize & MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_DEPTH)
                {
                    flagIndex = getWeightedSamples(weights[0].vidAttrWeights[i].probability, DEPTH_FIELD);
                    allocHeapAttrFlags = allocHeapAttrFlags |(s_AllocHeapAttrFlags[i][flagIndex]<< DRF_SHIFT(LWOS32_ATTR_DEPTH ));
                }
                break;
            }

            case COMPR_COVG:
            {
                if (allocHeapAttrFlagsToRandomize & MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_COMPR_COVG)
                {
                    flagIndex = getWeightedSamples(weights[0].vidAttrWeights[i].probability, COMPR_COVG_FIELD);
                    allocHeapAttrFlags = allocHeapAttrFlags | (s_AllocHeapAttrFlags[i][flagIndex]<< DRF_SHIFT(LWOS32_ATTR_COMPR_COVG ));
                }
                break;
            }

            case AA_SAMPLES:
            {
                if (allocHeapAttrFlagsToRandomize & MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_AA_SAMPLES)
                {
                    flagIndex = getWeightedSamples(weights[0].vidAttrWeights[i].probability, AA_SAMPLES_FIELD);
                    allocHeapAttrFlags = allocHeapAttrFlags | (s_AllocHeapAttrFlags[i][flagIndex]<< DRF_SHIFT(LWOS32_ATTR_AA_SAMPLES ));
                }
                break;
            }

            case COMPR:
            {
                if (allocHeapAttrFlagsToRandomize & MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_COMPR)
                {
                    flagIndex = getWeightedSamples(weights[0].vidAttrWeights[i].probability, COMPR_FIELD);
                    allocHeapAttrFlags = allocHeapAttrFlags | (s_AllocHeapAttrFlags[i][flagIndex]<< DRF_SHIFT(LWOS32_ATTR_COMPR ));
                }
                break;
            }

            case FORMAT:
            {
                if (allocHeapAttrFlagsToRandomize & MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_FORMAT)
                {
                    flagIndex = getWeightedSamples(weights[0].vidAttrWeights[i].probability, FORMAT_FIELD);
                    allocHeapAttrFlags = allocHeapAttrFlags | (s_AllocHeapAttrFlags[i][flagIndex]<< DRF_SHIFT(LWOS32_ATTR_FORMAT ));
                }
                break;
            }

            case Z_TYPE:
            {
                if (allocHeapAttrFlagsToRandomize & MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_Z_TYPE)
                {
                    flagIndex = getWeightedSamples(weights[0].vidAttrWeights[i].probability, Z_TYPE_FIELD);
                    allocHeapAttrFlags = allocHeapAttrFlags | (s_AllocHeapAttrFlags[i][flagIndex]<< DRF_SHIFT(LWOS32_ATTR_Z_TYPE ));
                }
                break;
            }

            case ZS_PACKING:
            {
                if (allocHeapAttrFlagsToRandomize & MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_ZS_PACKING)
                {
                    flagIndex = getWeightedSamples(weights[0].vidAttrWeights[i].probability, ZS_PACKING_FIELD);
                    allocHeapAttrFlags = allocHeapAttrFlags | (s_AllocHeapAttrFlags[i][flagIndex]<< DRF_SHIFT(LWOS32_ATTR_ZS_PACKING ));
                }
                break;
            }

            case COLOR_PACKING:
            {
                if (allocHeapAttrFlagsToRandomize & MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_COLOR_PACKING)
                {
                    flagIndex = getWeightedSamples(weights[0].vidAttrWeights[i].probability, COLOR_PACKING_FIELD);
                    allocHeapAttrFlags = allocHeapAttrFlags | (s_AllocHeapAttrFlags[i][flagIndex]<< DRF_SHIFT(LWOS32_ATTR_COLOR_PACKING ));
                }
                break;
            }

            case FORMAT_OVERRIDE:
            {
#ifdef LW_VERIF_FEATURES
                if (allocHeapAttrFlagsToRandomize & MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_FORMAT_OVERRIDE)
                {
                    flagIndex = getWeightedSamples(weights[0].vidAttrWeights[i].probability, FORMAT_OVERRIDE_FIELD);
                    allocHeapAttrFlags = allocHeapAttrFlags |  (s_AllocHeapAttrFlags[i][flagIndex]<< DRF_SHIFT(LWOS32_ATTR_FORMAT_OVERRIDE ));
                }
#endif
                break;
            }

            case PAGE_SIZE_HEAP:
            {
                if (allocHeapAttrFlagsToRandomize & MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_PAGE_SIZE_HEAP)
                {
                    flagIndex = getWeightedSamples(weights[0].vidAttrWeights[i].probability, PAGE_SIZE_HEAP_FIELD);
                    allocHeapAttrFlags = allocHeapAttrFlags | (s_AllocHeapAttrFlags[i][flagIndex]<< DRF_SHIFT(LWOS32_ATTR_PAGE_SIZE ));
                }
                break;
            }

            case LOCATION_HEAP:
            {
                if (allocHeapAttrFlagsToRandomize & MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_LOCATION_HEAP)
                {
                    flagIndex = getWeightedSamples(weights[0].vidAttrWeights[i].probability, LOCATION_HEAP_FIELD);
                    allocHeapAttrFlags = allocHeapAttrFlags | (s_AllocHeapAttrFlags[i][flagIndex]<< DRF_SHIFT(LWOS32_ATTR_LOCATION ));
                }
                break;
            }

            case PHYSICALITY_HEAP:
            {
                if (allocHeapAttrFlagsToRandomize & MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_PHYSICALITY_HEAP)
                {
                    flagIndex = getWeightedSamples(weights[0].vidAttrWeights[i].probability, PHYSICALITY_HEAP_FIELD);
                    allocHeapAttrFlags = allocHeapAttrFlags | (s_AllocHeapAttrFlags[i][flagIndex]<< DRF_SHIFT(LWOS32_ATTR_PHYSICALITY ));
                }
                break;
            }

            case COHERENCY_HEAP:
            {
                if (allocHeapAttrFlagsToRandomize & MMUUTIL_ALLOC_HEAP_ATTR_FLAGS_TO_RANDOMIZE_COHERENCY_HEAP)
                {
                    flagIndex = getWeightedSamples(weights[0].vidAttrWeights[i].probability, COHERENCY_HEAP_FIELD);
                    allocHeapAttrFlags = allocHeapAttrFlags | (s_AllocHeapAttrFlags[i][flagIndex]<< DRF_SHIFT(LWOS32_ATTR_COHERENCY ));
                }
                break;
            }

            default:
            {
                Printf(Tee::PriHigh,"\nIlwalid HEAP Field\n");
                return 0;
            }
        }
    }
    return allocHeapAttrFlags;
}

//!
//! @brief Function to randomize on ATTR2 flags used by VidHeapAllocSize() fn.
//!
//! Returns randomized attr2 flags for use with VidHeapAllocSize()
//! Randomizes flags from possible values stored in s_AllocHeapAttr2Flags array
//! The caller can specify which flags it wants to randomize in the input
//! parameter 'allocHeapAttr2FlagsToRandomize'
//!
//! @param[in]  weights
//!             MmuUtil weights structure, containing all the weights
//!
//! @param[in]  allocHeapAttr2FlagsToRandomize
//!             This parameter specifies which ATTR2 flags should be randomized upon.
//!             The caller can choose various flags by 'OR'ing the defines like
//!             'MMUUTIL_ALLOC_HEAP_ATTR2_FLAGS_TO_RANDOMIZE_ZBC' etc.
//!             See 'mmuutil.h' for all such defines.
//!             See 'mmutester.cpp' for an example of how to use this.
//!
//! @param[out] The ATTR2 flags value that has been chosen randomly (returned as
//!             returned value).

//! @returns    The ATTR2 flags value that has been chosen randomly.
//-----------------------------------------------------------------------------
UINT32 MmuUtil::RandomizeAllocHeapAttr2Flags
(
    MmuUtilWeights * weights,
    UINT32 allocHeapAttr2FlagsToRandomize
)
{
    UINT32 allocHeapAttr2Flags = 0;
    UINT32 flagIndex           = 0;

    for(int i = 0; i < MAX_ATTR2_FIELDS; i++)
    {
        switch(i)
        {
            case ZBC:
            {
                if (allocHeapAttr2FlagsToRandomize & MMUUTIL_ALLOC_HEAP_ATTR2_FLAGS_TO_RANDOMIZE_ZBC)
                {
                    flagIndex = getWeightedSamples(weights[0].vidAttr2Weights[i].probability, ZBC_FIELD);
                    allocHeapAttr2Flags = allocHeapAttr2Flags |(s_AllocHeapAttr2Flags[i][flagIndex]<< DRF_SHIFT(LWOS32_ATTR2_ZBC));
                }
                break;
            }

            case GPU_CACHEABLE:
            {
                if (allocHeapAttr2FlagsToRandomize & MMUUTIL_ALLOC_HEAP_ATTR2_FLAGS_TO_RANDOMIZE_GPU_CACHEABLE)
                {
                    flagIndex = getWeightedSamples(weights[0].vidAttr2Weights[i].probability, GPU_CACHEABLE_FIELD);
                    allocHeapAttr2Flags = allocHeapAttr2Flags |(s_AllocHeapAttr2Flags[i][flagIndex]<< DRF_SHIFT(LWOS32_ATTR2_GPU_CACHEABLE));
                }
                break;
            }
            case P2P_GPU_CACHEABLE:
            {
                if (allocHeapAttr2FlagsToRandomize & MMUUTIL_ALLOC_HEAP_ATTR2_FLAGS_TO_RANDOMIZE_P2P_GPU_CACHEABLE)
                {
                    flagIndex = getWeightedSamples(weights[0].vidAttr2Weights[i].probability, P2P_GPU_CACHEABLE_FIELD);
                    allocHeapAttr2Flags = allocHeapAttr2Flags |(s_AllocHeapAttr2Flags[i][flagIndex]<< DRF_SHIFT(LWOS32_ATTR2_P2P_GPU_CACHEABLE));
                }
                break;
            }
            default:
            {
                Printf(Tee::PriHigh,"\nIlwalid ATTR2 Field\n");
                return 0;
            }
        }
    }
    return allocHeapAttr2Flags;
}

//!
//! @brief Function to print mem Class used with AllocMemory() in string form.
//!
//! Prints the input param 'memClass' in string form.
//! Also prints any user message that user wants to print. This avoids a
//! separate Printf() call on part of the user.
//!
//! @param[in]  priority
//!             The priority level with with the user wants to print the flags,
//!             i.e. PriLow/PriHigh/PriInfo
//!
//! @param[in]  pMsg
//!             Pointer to the string the user wants to be printed along with
//!             the flags. This can be NULL, in that case no message is printed
//!
//! @param[in]  memClass
//!             The class that need to be printed in string form.
//!
//! @param[out] NONE.
//-----------------------------------------------------------------------------
void MmuUtil::PrintAllocMemoryClasses
(
    const UINT32 priority,
    const char *pMsg,
    const UINT32 memClass
)
{
    // first print user's message if exists
    if (pMsg)
    {
        Printf(priority, "%s", pMsg);
    }
    switch(memClass)
    {
        case LW01_MEMORY_SYSTEM:
        {
            Printf(priority, "%s", s_AllocMemoryClassesStrings[0]);
            break;
        }

        case LW01_MEMORY_SYSTEM_OS_DESCRIPTOR:
        {
            Printf(priority, "%s", s_AllocMemoryClassesStrings[1]);
            break;
        }

        default:
        {
            Printf(Tee::PriHigh,"\nIlwalid SYS_MEMORY Class\n");
            break;
        }
    }
}

//!
//! @brief Function to print flags used with AllocMemory() in string form.
//!
//! Prints the input param 'allocMemoryFlags' in string form. This input param
//! 'allocMemoryFlags' is assumed to be flags for AllocMemory() function, the
//! actual value of various flags is extraced using DRF_VAL macro and this value
//! is then used as the column index in the s_AllocMemoryFlagsStrings array.
//! Also prints any user message that user wants to print. This avoids a
//! separate Printf() call on part of the user.
//!
//! @param[in]  priority
//!             The priority level with with the user wants to print the flags,
//!             i.e. PriLow/PriHigh/PriInfo
//!
//! @param[in]  pMsg
//!             Pointer to the string the user wants to be printed along with
//!             the flags. This can be NULL, in that case no message is printed
//!
//! @param[in]  allocMemoryFlags
//!             The flags that need to be printed in string form.
//!
//! @param[out] NONE.
//-----------------------------------------------------------------------------
void MmuUtil::PrintAllocMemoryFlags
(
    const UINT32 priority,
    const char *pMsg,
    const UINT32 allocMemoryFlags
)
{
    UINT32 flagIndex = 0;

    // first print user's message if exists
    if (pMsg)
    {
        Printf(priority, "%s", pMsg);
    }

    for(int i = 0; i < MAX_MEMORY_FIELDS; i++)
    {
        switch(i)
        {
            case PHYSICALITY:
            {
                flagIndex = DRF_VAL(OS02, _FLAGS, _PHYSICALITY, allocMemoryFlags);
                Printf(priority, "%s | ", s_AllocMemoryFlagsStrings[i][flagIndex]);
                break;
            }

            case LOCATION:
            {
                flagIndex = DRF_VAL(OS02, _FLAGS, _LOCATION, allocMemoryFlags);
                Printf(priority, "%s | ", s_AllocMemoryFlagsStrings[i][flagIndex]);
                break;
            }

            case COHERENCE:
            {
                flagIndex = DRF_VAL(OS02, _FLAGS, _COHERENCY, allocMemoryFlags);
                Printf(priority, "%s | ", s_AllocMemoryFlagsStrings[i][flagIndex]);
                break;
            }

            case MAPPING:
            {
                flagIndex = DRF_VAL(OS02, _FLAGS, _MAPPING, allocMemoryFlags);
                Printf(priority, "%s | ", s_AllocMemoryFlagsStrings[i][flagIndex]);
                break;
            }

            default:
            {
                Printf(Tee::PriHigh,"\nIlwalid SYS_MEMORY Field\n");
                break;
            }

        }
    }
}

//!
//! @brief Function to print flags used with AllocContextDma() in string form.
//!
//! Prints the input param 'allocCtxDmaFlags' in string form. This input param
//! 'allocCtxDmaFlags' is assumed to be flags for AllocContextDma() function, the
//! actual value of various flags is extraced using DRF_VAL macro and this value
//! is then used as the column index in the s_AllocCtxDmaFlagsStrings array.
//! Also prints any user message that user wants to print. This avoids a
//! separate Printf() call on part of the user.
//!
//! @param[in]  priority
//!             The priority level with with the user wants to print the flags,
//!             i.e. PriLow/PriHigh/PriInfo
//!
//! @param[in]  pMsg
//!             Pointer to the string the user wants to be printed along with
//!             the flags. This can be NULL, in that case no message is printed
//!
//! @param[in]  allocCtxDmaFlags
//!             The flags that need to be printed in string form.
//!
//! @param[out] NONE.
//-----------------------------------------------------------------------------
void MmuUtil::PrintAllocCtxDmaFlags
(
    const UINT32 priority,
    const char *pMsg,
    const UINT32 allocCtxDmaFlags
)
{
    UINT32 flagIndex = 0;

    // first print user's message if exists
    if (pMsg)
    {
        Printf(priority, "%s", pMsg);
    }
    for(int i = 0; i < MAX_DMA_FIELDS; i++)
    {
        switch(i)
        {
            case ACCESS:
            {
                flagIndex = DRF_VAL(OS03, _FLAGS, _ACCESS, allocCtxDmaFlags);
                Printf(priority, "%s | ", s_AllocCtxDmaFlagsStrings[i][flagIndex]);
                break;
            }

            case PREALLOCATE:
            {
                flagIndex = DRF_VAL(OS03, _FLAGS, _PREALLOCATE, allocCtxDmaFlags);
                Printf(priority, "%s | ", s_AllocCtxDmaFlagsStrings[i][flagIndex]);
                break;
            }
            case GPU_MAPPABLE:
            {
                flagIndex = DRF_VAL(OS03, _FLAGS, _GPU_MAPPABLE, allocCtxDmaFlags);
                Printf(priority, "%s | ", s_AllocCtxDmaFlagsStrings[i][flagIndex]);
                break;
            }

            case TYPE:
            {
                flagIndex = DRF_VAL(OS03, _FLAGS, _TYPE, allocCtxDmaFlags);
                Printf(priority, "%s | ", s_AllocCtxDmaFlagsStrings[i][flagIndex]);
                break;
            }
            case ALLOCCTXDMA_MAPPING:
            {
                flagIndex = DRF_VAL(OS03, _FLAGS, _MAPPING, allocCtxDmaFlags);
                Printf(priority, "%s | ", s_AllocCtxDmaFlagsStrings[i][flagIndex]);
                break;
            }
            case CACHE_SNOOP:
            {
                flagIndex = DRF_VAL(OS03, _FLAGS, _CACHE_SNOOP, allocCtxDmaFlags);
                Printf(priority, "%s | ", s_AllocCtxDmaFlagsStrings[i][flagIndex]);
                break;
            }

            case HASH_TABLE:
            {
                flagIndex = DRF_VAL(OS03, _FLAGS, _HASH_TABLE, allocCtxDmaFlags);
                Printf(priority, "%s | ", s_AllocCtxDmaFlagsStrings[i][flagIndex]);
                break;
            }

            default:
            {
                Printf(Tee::PriHigh,"\nIlwalid CTXDMA Field\n");
                break;
            }
        }
    }
}

//!
//! @brief Function to print flags used with MapMemory() in string form.
//!
//! Prints the input param 'mapMemoryFlags' in string form. This input param
//! 'mapMemoryFlags' is assumed to be flags for MapMemory() function, the
//! actual value of various flags is extraced using DRF_VAL macro and this value
//! is then used as the column index in the s_MapMemoryFlagsStrings array.
//! Also prints any user message that user wants to print. This avoids a
//! separate Printf() call on part of the user.
//!
//! @param[in]  priority
//!             The priority level with with the user wants to print the flags,
//!             i.e. PriLow/PriHigh/PriInfo
//!
//! @param[in]  pMsg
//!             Pointer to the string the user wants to be printed along with
//!             the flags. This can be NULL, in that case no message is printed
//!
//! @param[in]  mapMemoryFlags
//!             The flags that need to be printed in string form.
//!
//! @param[out] NONE.
//-----------------------------------------------------------------------------
void MmuUtil::PrintMapMemoryFlags
(
    const UINT32 priority,
    const char *pMsg,
    const UINT32 mapMemoryFlags
)
{
    UINT32 flagIndex = 0;

    // first print user's message if exists
    if (pMsg)
    {
        Printf(priority, "%s", pMsg);
    }
    for (int i = 0; i < MAX_MAP_FIELDS; i++)
    {
        switch(i)
        {
            case MAP_ACCESS:
            {
                flagIndex = DRF_VAL(OS33, _FLAGS, _ACCESS, mapMemoryFlags);
                Printf(priority, "%s | ", s_MapMemoryFlagsStrings[i][flagIndex]);
                break;
            }

            case SKIP_SIZE_CHECK:
            {
                flagIndex = DRF_VAL(OS33, _FLAGS, _SKIP_SIZE_CHECK, mapMemoryFlags);
                Printf(priority, "%s | ", s_MapMemoryFlagsStrings[i][flagIndex]);
                break;
            }
            case MEM_SPACE:
            {
                flagIndex = DRF_VAL(OS33, _FLAGS, _MEM_SPACE, mapMemoryFlags);
                Printf(priority, "%s | ", s_MapMemoryFlagsStrings[i][flagIndex]);
                break;
            }
            case MAPMEMORY_MAPPING:
            {
                flagIndex = DRF_VAL(OS33, _FLAGS, _MAPPING, mapMemoryFlags);
                Printf(priority, "%s | ", s_MapMemoryFlagsStrings[i][flagIndex]);
                break;
            }

            default:
            {
                Printf(Tee::PriHigh,"\nIlwalid MAPPING Field\n");
                break;
            }
        }
    }

}

//!
//! @brief Function to print flags used with MapMemoryDma() in string form.
//!
//! Prints the input param 'mapMemoryDmaFlags' in string form. This input param
//! 'mapMemoryDmaFlags' is assumed to be flags for MapMemoryDma() function, the
//! actual value of various flags is extraced using DRF_VAL macro and this value
//! is then used as the column index in the s_MapMemoryDmaFlagsStrings array.
//! Also prints any user message that user wants to print. This avoids a
//! separate Printf() call on part of the user.
//!
//! @param[in]  priority
//!             The priority level with with the user wants to print the flags,
//!             i.e. PriLow/PriHigh/PriInfo
//!
//! @param[in]  pMsg
//!             Pointer to the string the user wants to be printed along with
//!             the flags. This can be NULL, in that case no message is printed
//!
//! @param[in]  mapMemoryDmaFlags
//!             The flags that need to be printed in string form.
//!
//! @param[out] NONE.
//-----------------------------------------------------------------------------
void MmuUtil::PrintMapMemoryDmaFlags
(
    const UINT32 priority,
    const char *pMsg,
    const UINT32 mapMemoryDmaFlags
)
{
    UINT32 flagIndex = 0;

    // first print user's message if exists
    if (pMsg)
    {
        Printf(priority, "%s", pMsg);
    }
    for (int i = 0; i < MAX_MAPDMA_FIELDS; i++)
    {

        switch(i)
        {
            case MAPDMA_ACCESS:
            {
                flagIndex = DRF_VAL(OS46, _FLAGS, _ACCESS, mapMemoryDmaFlags);
                Printf(priority, "%s | ", s_MapMemoryDmaFlagsStrings[i][flagIndex]);
                break;
            }

            case MAPDMA_CACHE_SNOOP:
            {
                flagIndex = DRF_VAL(OS46, _FLAGS, _CACHE_SNOOP, mapMemoryDmaFlags);
                Printf(priority, "%s | ", s_MapMemoryDmaFlagsStrings[i][flagIndex]);
                break;
            }

            case MAPDMA_PAGE_SIZE_CHECK:
            {
                flagIndex = DRF_VAL(OS46, _FLAGS, _PAGE_SIZE, mapMemoryDmaFlags);
                Printf(priority, "%s | ", s_MapMemoryDmaFlagsStrings[i][flagIndex]);
                break;
            }

            case MAPDMA_PRIV:
            {
#ifdef LW_VERIF_FEATURES
                    flagIndex = DRF_VAL(OS46, _FLAGS, _PRIV, mapMemoryDmaFlags);
                    Printf(priority, "%s | ", s_MapMemoryDmaFlagsStrings[i][flagIndex]);
#endif
            break;
            }

            case MAPDMA_DMA_OFFSET_GROWS:
            {
                flagIndex = DRF_VAL(OS46, _FLAGS, _DMA_OFFSET_GROWS, mapMemoryDmaFlags);
                Printf(priority, "%s | ", s_MapMemoryDmaFlagsStrings[i][flagIndex]);
                break;
            }

            case MAPDMA_DMA_OFFSET_FIXED:
            {
                flagIndex = DRF_VAL(OS46, _FLAGS, _DMA_OFFSET_FIXED, mapMemoryDmaFlags);
                Printf(priority, "%s | ", s_MapMemoryDmaFlagsStrings[i][flagIndex]);
                break;
            }

             case MAPDMA_PTE_COALESCE_LEVEL:
             {
#ifdef LW_VERIF_FEATURES
                flagIndex = DRF_VAL(OS46, _FLAGS, _PTE_COALESCE_LEVEL_CAP, mapMemoryDmaFlags);
                Printf(priority, "%s | ", s_MapMemoryDmaFlagsStrings[i][flagIndex]);
#endif
                break;
             }

             case MAPDMA_FLAGS_P2P_ENABLE:
            {
                flagIndex = DRF_VAL(OS46, _FLAGS, _P2P_ENABLE, mapMemoryDmaFlags);
                Printf(priority, "%s | ", s_MapMemoryDmaFlagsStrings[i][flagIndex]);
                break;
            }

            case MAPDMA_TLB_LOCK:
            {
                flagIndex = DRF_VAL(OS46, _FLAGS, _TLB_LOCK, mapMemoryDmaFlags);
                Printf(priority, "%s | ", s_MapMemoryDmaFlagsStrings[i][flagIndex]);
                break;
            }

            default:
            {
                Printf(Tee::PriHigh,"\nIlwalid MAP DMA Field\n");
                break;
            }
        }
    }
}

//!
//! @brief Function to print vidType used with VidHeapAllocSize() in string form.
//!
//! Prints the input param 'vidType' in string form.
//! Also prints any user message that user wants to print. This avoids a
//! separate Printf() call on part of the user.
//!
//! @param[in]  priority
//!             The priority level with with the user wants to print the flags,
//!             i.e. PriLow/PriHigh/PriInfo
//!
//! @param[in]  pMsg
//!             Pointer to the string the user wants to be printed along with
//!             the flags. This can be NULL, in that case no message is printed
//!
//! @param[in]  vidType
//!             The vidType that need to be printed in string form.
//!
//! @param[out] NONE.
//-----------------------------------------------------------------------------
void MmuUtil::PrintAllocHeapVidType
(
    const UINT32 priority,
    const char *pMsg,
    const UINT32 vidType
)
{
    // first print user's message if exists
    if (pMsg)
    {
        Printf(priority, "%s", pMsg);
    }
    switch(vidType)
    {
        case LWOS32_TYPE_IMAGE:
        {
            Printf(priority, " %s", s_AllocHeapTypesStrings[0]);
            break;
        }
        case LWOS32_TYPE_DEPTH:
        {
            Printf(priority, " %s", s_AllocHeapTypesStrings[1]);
            break;
        }
        case LWOS32_TYPE_TEXTURE:
        {
            Printf(priority, " %s", s_AllocHeapTypesStrings[2]);
            break;
        }
        case LWOS32_TYPE_VIDEO:
        {
            Printf(priority, " %s", s_AllocHeapTypesStrings[3]);
            break;
        }
        case LWOS32_TYPE_FONT:
        {
            Printf(priority, " %s", s_AllocHeapTypesStrings[4]);
            break;
        }
        case LWOS32_TYPE_LWRSOR:
        {
            Printf(priority, " %s", s_AllocHeapTypesStrings[5]);
            break;
        }
        case LWOS32_TYPE_DMA:
        {
            Printf(priority, " %s", s_AllocHeapTypesStrings[6]);
            break;
        }
        case LWOS32_TYPE_INSTANCE:
        {
            Printf(priority, " %s", s_AllocHeapTypesStrings[7]);
            break;
        }
        case LWOS32_TYPE_PRIMARY:
        {
            Printf(priority, " %s", s_AllocHeapTypesStrings[8]);
            break;
        }
        case LWOS32_TYPE_ZLWLL:
        {
            Printf(priority, " %s", s_AllocHeapTypesStrings[9]);
            break;
        }
        case LWOS32_TYPE_UNUSED:
        {
            Printf(priority, " %s", s_AllocHeapTypesStrings[10]);
            break;
        }
        case LWOS32_TYPE_SHADER_PROGRAM:
        {
            Printf(priority, " %s", s_AllocHeapTypesStrings[11]);
            break;
        }
        case LWOS32_TYPE_OWNER_RM:
        {
            Printf(priority, " %s", s_AllocHeapTypesStrings[12]);
            break;
        }
        case LWOS32_TYPE_RESERVED:
        {
            Printf(priority, " %s", s_AllocHeapTypesStrings[13]);
            break;
        }
        default:
        {
            Printf(Tee::PriHigh,"\nIlwalid VidType\n");
            break;
        }
    }
}

//!
//! @brief Function to print flags used with VidHeapAllocSize() in string form.
//!
//! Prints the input param 'allocHeapAttrFlags' in string form. This input param
//! 'allocHeapAttrFlags' is assumed to be flags for VidHeapAllocSize() function, the
//! actual value of various flags is extraced using DRF_VAL macro and this value
//! is then used as the column index in the s_AllocHeapAttrFlagsStrings array.
//! Also prints any user message that user wants to print. This avoids a
//! separate Printf() call on part of the user.
//!
//! @param[in]  priority
//!             The priority level with with the user wants to print the flags,
//!             i.e. PriLow/PriHigh/PriInfo
//!
//! @param[in]  pMsg
//!             Pointer to the string the user wants to be printed along with
//!             the flags. This can be NULL, in that case no message is printed
//!
//! @param[in]  allocHeapAttrFlags
//!             The flags that need to be printed in string form.
//!
//! @param[out] NONE.
//-----------------------------------------------------------------------------
void MmuUtil::PrintAllocHeapAttrFlags
(
    const UINT32 priority,
    const char *pMsg,
    const UINT32 allocHeapAttrFlags
)
{
    UINT32 flagIndex = 0;

    // first print user's message if exists
    if (pMsg)
    {
        Printf(priority, "%s", pMsg);
    }
    for(int i = 0; i < MAX_ATTR_FIELDS; i++)
    {
        switch(i)
        {
            case DEPTH:
            {
                flagIndex = DRF_VAL(OS32, _ATTR, _DEPTH, allocHeapAttrFlags);
                Printf(priority, "%s | ", s_AllocHeapAttrFlagsStrings[i][flagIndex]);
                break;
            }

            case COMPR_COVG:
            {
                flagIndex = DRF_VAL(OS32, _ATTR, _COMPR_COVG, allocHeapAttrFlags);
                Printf(priority, "%s | ", s_AllocHeapAttrFlagsStrings[i][flagIndex]);
                break;
            }

            case AA_SAMPLES:
            {
                flagIndex = DRF_VAL(OS32, _ATTR, _AA_SAMPLES, allocHeapAttrFlags);
                Printf(priority, "%s | ", s_AllocHeapAttrFlagsStrings[i][flagIndex]);
                break;
            }

            case COMPR:
            {
                flagIndex = DRF_VAL(OS32, _ATTR, _COMPR, allocHeapAttrFlags);
                Printf(priority, "%s | ", s_AllocHeapAttrFlagsStrings[i][flagIndex]);
                break;
            }

            case FORMAT:
            {
                flagIndex = DRF_VAL(OS32, _ATTR, _FORMAT, allocHeapAttrFlags);
                Printf(priority, "%s | ", s_AllocHeapAttrFlagsStrings[i][flagIndex]);
                break;
            }

            case Z_TYPE:
            {
                flagIndex = DRF_VAL(OS32, _ATTR, _Z_TYPE, allocHeapAttrFlags);
                Printf(priority, "%s | ", s_AllocHeapAttrFlagsStrings[i][flagIndex]);
                break;
            }

            case ZS_PACKING:
            {
                flagIndex = DRF_VAL(OS32, _ATTR, _ZS_PACKING, allocHeapAttrFlags);
                Printf(priority, "%s | ", s_AllocHeapAttrFlagsStrings[i][flagIndex]);
                break;
            }
            case COLOR_PACKING:
            {
                flagIndex = DRF_VAL(OS32, _ATTR, _COLOR_PACKING, allocHeapAttrFlags);
                Printf(priority, "%s | ", s_AllocHeapAttrFlagsStrings[i][flagIndex]);
                break;
            }

            case FORMAT_OVERRIDE:
            {
#ifdef LW_VERIF_FEATURES
                flagIndex = DRF_VAL(OS32, _ATTR, _FORMAT_OVERRIDE, allocHeapAttrFlags);
                Printf(priority, "%s | ", s_AllocHeapAttrFlagsStrings[i][flagIndex]);
#endif
                break;
            }
            case PAGE_SIZE_HEAP:
            {
                flagIndex = DRF_VAL(OS32, _ATTR, _PAGE_SIZE, allocHeapAttrFlags);
                Printf(priority, "%s | ", s_AllocHeapAttrFlagsStrings[i][flagIndex]);
                break;
            }

            case LOCATION_HEAP:
            {
                flagIndex = DRF_VAL(OS32, _ATTR, _LOCATION, allocHeapAttrFlags);
                Printf(priority, "%s | ", s_AllocHeapAttrFlagsStrings[i][flagIndex]);
                break;
            }

            case PHYSICALITY_HEAP:
            {
                flagIndex = DRF_VAL(OS32, _ATTR, _PHYSICALITY, allocHeapAttrFlags);
                Printf(priority, "%s | ", s_AllocHeapAttrFlagsStrings[i][flagIndex]);
                break;
            }

            case COHERENCY_HEAP:
            {
                flagIndex = DRF_VAL(OS32, _ATTR, _COHERENCY, allocHeapAttrFlags);
                Printf(priority, "%s | ", s_AllocHeapAttrFlagsStrings[i][flagIndex]);
                break;
            }

            default:
            {
                Printf(Tee::PriHigh,"\nIlwalid HEAP Field\n");
                break;
            }
        }
    }
}

//!
//! @brief Function to print ATTR2 flags used with VidHeapAllocSize() in string
//!
//! Prints the input param 'allocHeapAttr2Flags' in string form. This input param
//! 'allocHeapAttr2Flags' is assumed to be flags for VidHeapAllocSize() function, the
//! actual value of various flags is extraced using DRF_VAL macro and this value
//! is then used as the column index in the s_AllocHeapAttr2FlagsStrings array.
//! Also prints any user message that user wants to print. This avoids a
//! separate Printf() call on part of the user.
//!
//! @param[in]  priority
//!             The priority level with with the user wants to print the flags,
//!             i.e. PriLow/PriHigh/PriInfo
//!
//! @param[in]  pMsg
//!             Pointer to the string the user wants to be printed along with
//!             the flags. This can be NULL, in that case no message is printed
//!
//! @param[in]  allocHeapAttr2Flags
//!             The flags that need to be printed in string form.
//!
//! @param[out] NONE.
//-----------------------------------------------------------------------------
void MmuUtil::PrintAllocHeapAttr2Flags
(
    const UINT32  priority,
    const char   *pMsg,
    const UINT32  allocHeapAttr2Flags
)
{
    UINT32 flagIndex = 0;

    // first print user's message if exists
    if (pMsg)
    {
        Printf(priority, "%s", pMsg);
    }
    for(int i = 0; i < MAX_ATTR2_FIELDS; i++)
    {
        switch(i)
        {
            case ZBC:
            {
                flagIndex = DRF_VAL(OS32, _ATTR2, _ZBC, allocHeapAttr2Flags);
                Printf(priority, "%s | ", s_AllocHeapAttr2FlagsStrings[i][flagIndex]);
                break;
            }
            case GPU_CACHEABLE:
            {
                flagIndex = DRF_VAL(OS32, _ATTR2, _GPU_CACHEABLE, allocHeapAttr2Flags);
                Printf(priority, "%s | ", s_AllocHeapAttr2FlagsStrings[i][flagIndex]);
                break;
            }
            case P2P_GPU_CACHEABLE:
            {
                flagIndex = DRF_VAL(OS32, _ATTR2, _P2P_GPU_CACHEABLE, allocHeapAttr2Flags);
                Printf(priority, "%s | ", s_AllocHeapAttr2FlagsStrings[i][flagIndex]);
                break;
            }
            default:
            {
                Printf(Tee::PriHigh,"\nIlwalid ATTR2 Field\n");
                break;
            }
        }
    }
}
