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
 * @file   nne_parm_ram.c
 * @brief  DLC parameter RAM interface.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "objnne.h"
#include "nne/nne_parm_ram.h"
#include "task_nne.h"
#include "main.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
// Binary of 1 in IEEE-754 format
#define LWF32_ONE                                                    (0x3F800000)

/* ------------------------ Global Variables -------------------------------- */
/*!
 * @brief Offset into the parameter RAM that has been allocated for non-inference
 *        specific allocations. Should be reset whenever a new descriptor is loaded
 *        by using @ref nneParmRamReset.
 */
static LwU32   nonInferenceParmOffset
    GCC_ATTRIB_SECTION("dmem_nne", "nonInferenceParmOffset");

/*!
 * @brief Offset into the parameter RAM that has been allocated for
 *        multi-inference allocations. Should be reset when a new inference does
 *        not need to use allocations from prior inferences by using
 *        @ref nneParmRamMultiInferenceReset.
 */
static LwU32   NneParmRamMultiInferenceParmOffset
    GCC_ATTRIB_SECTION("dmem_nne", "NneParmRamMultiInferenceParmOffset");

/*!
 * @brief   Sequence number used to track validity of multi-inference
 *          allocations. Current value is the lwrrently valid sequence number
 *          (if any).
 */
GCC_ATTRIB_SECTION("dmem_nne", "NneParmRamMultiInferenceSeqNum")
static LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PARM_RAM_SEQ_NUM NneParmRamMultiInferenceSeqNum =
    LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PARM_RAM_SEQ_NUM_INIT;

/*!
 * @brief Offset into the parameter RAM that has been allocated for inference
 *        specific allocations. Should be reset when an inference is completed
 *        by using @ref nneParmRamInferenceReset.
 */
static LwU32   inferenceParmOffset
    GCC_ATTRIB_SECTION("dmem_nne", "inferenceParmOffset");

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc nneParmRamPostInit
 */
FLCN_STATUS
nneParmRamPostInit(void)
{
    LwU32       reg    = LWF32_ONE;
    FLCN_STATUS status = FLCN_OK;

    // Initialize the "1" constant location in parameter RAM.
    PMU_ASSERT_OK_OR_GOTO(status,
        nneParmRamAccess_HAL(&Nne,
                             &reg,
                             NNE_PARM_RAM_1_CONST_OFFSET,
                             sizeof(reg),
                             LW_TRUE),
        nneParmRamPostInit_exit);

nneParmRamPostInit_exit:
    return status;
}

/*!
 * @copydoc nneParmRamAlloc
 */
FLCN_STATUS
nneParmRamAlloc
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
        goto nneParmRamAlloc_exit;
    }

    if ((nneParmRamSizeGet_HAL() < size) ||
        (nneParmRamSizeGet_HAL() - size) < nonInferenceParmOffset)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_NO_FREE_MEM;
        goto nneParmRamAlloc_exit;
    }

    //
    // An assumption with this allocator is that clients make all
    // inference-independent allocations first, before making any
    // inference-specific allocations.
    //
    if (NneParmRamMultiInferenceParmOffset != 0U)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILLEGAL_OPERATION;
        goto nneParmRamAlloc_exit;
    }

    *pOffset                = nonInferenceParmOffset;
    nonInferenceParmOffset += size;

nneParmRamAlloc_exit:
    return status;
}

/*!
 * @copydoc nneParmRamReset
 */
FLCN_STATUS
nneParmRamReset(void)
{
    FLCN_STATUS status;

    nonInferenceParmOffset = NNE_PARM_RAM_1_CONST_OFFSET + sizeof(LwU32);
    
    PMU_ASSERT_OK_OR_GOTO(status,
        nneParmRamMultiInferenceReset(),
        nneParmRamReset_exit);

nneParmRamReset_exit:
    return status;
}

/*!
 * @copydoc nneParmRamMultiInferenceAlloc
 */
FLCN_STATUS
nneParmRamMultiInferenceAlloc
(
    LwU32 size,
    LwU32 *pOffset,
    LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PARM_RAM_SEQ_NUM *pSeqNum
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        (pOffset != NULL) &&
        (pSeqNum != NULL) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        nneParmRamMultiInferenceAlloc_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        (nneParmRamSizeGet_HAL() >= size) &&
        (nneParmRamSizeGet_HAL() - size >= NneParmRamMultiInferenceParmOffset) ?
            FLCN_OK :  FLCN_ERR_NO_FREE_MEM,
        nneParmRamMultiInferenceAlloc_exit);

    //
    // An assumption with this allocator is that clients make all
    // multi-inference allocations first, before making any
    // inference-specific allocations.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        inferenceParmOffset == 0U ? FLCN_OK : FLCN_ERR_ILLEGAL_OPERATION,
        nneParmRamMultiInferenceAlloc_exit);

    // If the inference allocator heap hasn't been initialized, initialize it.
    if (NneParmRamMultiInferenceParmOffset == 0U)
    {
        NneParmRamMultiInferenceParmOffset = nonInferenceParmOffset;
    }

    *pOffset = NneParmRamMultiInferenceParmOffset;
    NneParmRamMultiInferenceParmOffset += size;

    // Assign the current sequence number for this allocation.
    *pSeqNum = NneParmRamMultiInferenceSeqNum;

nneParmRamMultiInferenceAlloc_exit:
    return status;
}

/*!
 * @copydoc nneParmRamMultiInferenceReset
 */
FLCN_STATUS
nneParmRamMultiInferenceReset(void)
{
    FLCN_STATUS status;

    //
    // Reset the offset to 0. It will be properly adjusted on the first call to
    // nneParmRamInferenceAlloc
    //
    NneParmRamMultiInferenceParmOffset = 0U;

    //
    // We're resetting the allocations, so increment to ilwalidate the current
    // allocation.
    //
    LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PARM_RAM_SEQ_NUM_INCREMENT(
        &NneParmRamMultiInferenceSeqNum);

    PMU_ASSERT_OK_OR_GOTO(status,
        nneParmRamInferenceReset(),
        nneParmRamMultiInferenceReset_exit);

nneParmRamMultiInferenceReset_exit:
    return status;
}

/*!
 * @copydoc nneParmRamMultiInferenceAllocValid
 */
LwBool
nneParmRamMultiInferenceAllocValid
(
    LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PARM_RAM_SEQ_NUM seqNum
)
{
    return seqNum == NneParmRamMultiInferenceSeqNum;
}

/*!
 * @copydoc nneParmRamInferenceAlloc
 */
FLCN_STATUS
nneParmRamInferenceAlloc
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
        goto nneParmRamInferenceAlloc_exit;
    }

    if ((nneParmRamSizeGet_HAL() < size) ||
        (nneParmRamSizeGet_HAL() - size < inferenceParmOffset))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_NO_FREE_MEM;
        goto nneParmRamInferenceAlloc_exit;
    }

    // If the inference allocator heap hasn't been initialized, initialize it.
    if (inferenceParmOffset == 0)
    {
        inferenceParmOffset = NneParmRamMultiInferenceParmOffset;
    }

    *pOffset             = inferenceParmOffset;
    inferenceParmOffset += size;

nneParmRamInferenceAlloc_exit:
    return status;
}

/*!
 * @copydoc nneParmRamInferenceReset
 */
FLCN_STATUS
nneParmRamInferenceReset(void)
{
    //
    // Reset the offset to 0. It will be properly adjusted on the first call to
    // nneParmRamInferenceAlloc
    //
    inferenceParmOffset = 0U;
    return FLCN_OK;
}

/* ------------------------ Private Functions ------------------------------- */
