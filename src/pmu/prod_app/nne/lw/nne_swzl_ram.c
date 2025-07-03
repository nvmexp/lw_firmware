/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   nne_swzl_ram.c
 * @brief  DLC swizzle RAM interface.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "objnne.h"
#include "nne/nne_swzl_ram.h"
#include "task_nne.h"
#include "main.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
#define NNE_SWZL_STAGING_SIZE                                                (4)

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * @brief Offset into the swizzle RAM that has been allocated for non-inference
 *        specific allocations. Should be reset whenever a new descriptor is loaded
 *        by using @ref nneSwzlRamReset.
 */
static LwU32   nonInferenceSwzlOffset
    GCC_ATTRIB_SECTION("dmem_nne", "nonInferenceSwzlOffset");

/*!
 * @brief Offset into the swizzle RAM that has been allocated for inference
 *        specific allocations. Should be reset when an inference is completed
 *        by using @ref nneSwzlRamInferenceReset.
 */
static LwU32   inferenceSwzlOffset
    GCC_ATTRIB_SECTION("dmem_nne", "inferenceSwzlOffset");

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc nneSwzlRamAlloc
 */
FLCN_STATUS
nneSwzlRamAlloc
(
    LwU32    size,
    LwU32   *pOffset
)
{
    FLCN_STATUS  status = FLCN_OK;

    // Sanity checking.
    if (pOffset == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneSwzlRamAlloc_exit;
    }

    if ((nneSwzlRamSizeGet_HAL() < size) ||
        ((nneSwzlRamSizeGet_HAL() - size) < nonInferenceSwzlOffset))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_NO_FREE_MEM;
        goto nneSwzlRamAlloc_exit;
    }

    //
    // An assumption with this allocator is that clients make all
    // inference-independent allocations first, before making any
    // inference-specific allocations.
    //
    if (inferenceSwzlOffset != 0)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILLEGAL_OPERATION;
    }

    *pOffset                = nonInferenceSwzlOffset;
    nonInferenceSwzlOffset += size;

nneSwzlRamAlloc_exit:
    return status;
}

/*!
 * @copydoc nneSwzlRamReset
 */
FLCN_STATUS
nneSwzlRamReset(void)
{
    nonInferenceSwzlOffset = 0;
    inferenceSwzlOffset    = 0;
    return FLCN_OK;
}

/*!
 * @copydoc nneSwzlRamInferenceAlloc
 */
FLCN_STATUS
nneSwzlRamInferenceAlloc
(
    LwU32    size,
    LwU32   *pOffset)
{
    FLCN_STATUS   status    = FLCN_OK;

    // Sanity checking.
    if (pOffset == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneSwzlRamInferenceAlloc_exit;
    }

    if ((nneSwzlRamSizeGet_HAL() < size) ||
        ((nneSwzlRamSizeGet_HAL() - size) < inferenceSwzlOffset))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_NO_FREE_MEM;
        goto nneSwzlRamInferenceAlloc_exit;
    }

    // If the inference allocator heap hasn't been initialized, initialize it.
    if (inferenceSwzlOffset == 0)
    {
        inferenceSwzlOffset = nonInferenceSwzlOffset;
    }

    *pOffset             = inferenceSwzlOffset;
    inferenceSwzlOffset += size;

nneSwzlRamInferenceAlloc_exit:
    return status;
}

/*!
 * @copydoc nneSwzlInferenceReset
 */
FLCN_STATUS
nneSwzlRamInferenceReset(void)
{
    inferenceSwzlOffset = nonInferenceSwzlOffset;
    return FLCN_OK;
}

/*!
 * @copydoc nneSwzlRamDirectMappingWrite
 */
FLCN_STATUS
nneSwzlDirectMappingWrite
(
    LwU32       swzlRamBaseOffset,
    LwU32       parmRamBaseOffset,
    LwU32       numSwzl
)
{
    NNE_SWZL      swzlBuf[NNE_SWZL_STAGING_SIZE];
    LwU32         swzlChunkIdx;
    LwU32         swzlChunkNumSwzl;
    LwU32         swzlSize;
    LwU32         swzlIdx = 0;
    FLCN_STATUS   status  = FLCN_OK;

    // Sanity checking.
    swzlSize = numSwzl * sizeof(LwU32);
    if ((nneSwzlRamSizeGet_HAL() < swzlSize) ||
        ((nneSwzlRamSizeGet_HAL() - swzlSize) < swzlRamBaseOffset))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_OUT_OF_RANGE;
        goto nneSwzlDirectMappingWrite_exit;
    }

    if ((nneParmRamSizeGet_HAL() < swzlSize) ||
        ((nneParmRamSizeGet_HAL() - swzlSize) < parmRamBaseOffset))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_OUT_OF_RANGE;
        goto nneSwzlDirectMappingWrite_exit;
    }

    //
    // Generate and write out the direct-mapping swizzles in chunks, balancing DMEM constraints,
    // and overhead from smaller chunk sizes.
    //
    while (swzlIdx < numSwzl)
    {
        //
        // The number of swizzles to process this iteration is however many swizzles there
        // are remaining to process, capped to the chunk size that the swizzles are being
        // processed in.
        //
        swzlChunkNumSwzl = LW_MIN(NNE_SWZL_STAGING_SIZE, numSwzl - swzlIdx);

        // Generate the mapping.
        for (swzlChunkIdx = 0; swzlChunkIdx < swzlChunkNumSwzl; swzlChunkIdx++)
        {
            swzlBuf[swzlChunkIdx].parmRamOffset = parmRamBaseOffset +
                                                  ((swzlIdx + swzlChunkIdx) * sizeof(LwU32));
            swzlBuf[swzlChunkIdx].stride        = 0;
        }

        // Write it out the mapping to swizzle RAM.
        PMU_ASSERT_OK_OR_GOTO(status,
            nneSwzlWrite(swzlBuf,
                         swzlRamBaseOffset + (swzlIdx * sizeof(LwU32)),
                         swzlChunkNumSwzl),
            nneSwzlDirectMappingWrite_exit);

        swzlIdx += swzlChunkNumSwzl;
    }

nneSwzlDirectMappingWrite_exit:
    return status;
}

/*!
 * @copydoc nneSwzlWrite
 */
FLCN_STATUS
nneSwzlWrite
(
    NNE_SWZL   *pSwzl,
    LwU32       swzlRamBaseOffset,
    LwU32       numSwzl
)
{
    LwU32         swzlSize;
    LwU32         swzlChunkNumSwzl;
    LwU32         swzlBinary[NNE_SWZL_STAGING_SIZE];
    LwU32         swzlIdx = 0;
    FLCN_STATUS   status  = FLCN_OK;

    // Sanity checking.
    if (pSwzl == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneSwzlWrite_exit;
    }

    swzlSize = numSwzl * sizeof(LwU32);
    if ((nneSwzlRamSizeGet_HAL() < swzlSize) ||
        ((nneSwzlRamSizeGet_HAL() - swzlSize) < swzlRamBaseOffset))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_OUT_OF_RANGE;
        goto nneSwzlWrite_exit;
    }

    while (swzlIdx < numSwzl)
    {
        swzlChunkNumSwzl = LW_MIN(NNE_SWZL_STAGING_SIZE, numSwzl - swzlIdx);

        // Get the binary represenation of the swizzles
        PMU_ASSERT_OK_OR_GOTO(status,
            nneSwzlBinaryFormatGet_HAL(&Nne,
                                       &(pSwzl[swzlIdx]),
                                       swzlBinary,
                                       swzlChunkNumSwzl),
            nneSwzlWrite_exit);

        // Write the swizzle out.
        PMU_ASSERT_OK_OR_GOTO(status,
            nneSwzlRamWrite_HAL(&Nne,
                                swzlBinary,
                                swzlRamBaseOffset + (swzlIdx * sizeof(LwU32)),
                                swzlChunkNumSwzl * sizeof(LwU32)),
            nneSwzlWrite_exit);

        swzlIdx += swzlChunkNumSwzl;
    }

nneSwzlWrite_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
