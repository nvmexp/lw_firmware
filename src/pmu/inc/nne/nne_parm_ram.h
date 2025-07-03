/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file nne_parm_ram.h
 * @brief DLC parameter RAM interface.
 *
 * @details
 * The parameter RAM is something of a "three-level" allocator. The levels are
 * as follows:
 *  1.) Per-descriptor/non-inference allocations. These allocations are constant
 *  for a given @ref NNE_DESC; as long as inferences continue to be made on the
 *  same @ref NNE_DESC, these allocations remain valid.
 *
 *  2.) Multi-inference allocations. These allocations are not per-descriptor,
 *  but the caller may execute multiple inferences with the same input. Thus,
 *  these allocations can be made once and used across multiple inferences.
 *  Either the client or the allocator can ilwalidate these allocations.
 *
 *  3.) Per-inference allocations. These allocations are specific to each
 *  inference invocation and must be made anew on each invocation.
 *
 *  Allocations must be made in the order of the levels. In other words, all
 *  per-descriptor allocations should be made; then all multi-inference
 *  allocations; and then all per-inference allocations.
 */

#ifndef NNE_PARM_RAM_H
#define NNE_PARM_RAM_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
  * @brief Offset into the parameter RAM that the 1 constant lives at.
  *
  * The DLC does not differentiate neuron weights and neuron bias values. Both
  * are read from the weight RAM, multiplied with an input in the parameter RAM
  * (pointed to via the swizzle RAM), then summed. Bias values must therefore
  * be matched with a hardcoded value of "1" in the parameter RAM so that the
  * the bias value preserves its value and is only added to the weighted sum
  * of the neuron's inputs.
  */
#define NNE_PARM_RAM_1_CONST_OFFSET                                          (0)

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Post-init setup routine for the DLC parameter RAM.
 *
 * @return FLCN_OK   Post-init setup was successful.
 */
FLCN_STATUS nneParmRamPostInit(void)
    GCC_ATTRIB_SECTION("imem_nneInit", "nneParmRamPostInit");

/*!
 * @brief Allocator for the parameter RAM.
 *
 * @param[IN]  size      Size of the requested allocation, in bytes.
 * @param[OUT] pOffset   Offset of the allocated parameter RAM.
 *
 * @return  FLCN_ERR_ILWALID ARGUMENT   If pOffset is NULL.
 * @return  FLCN_ERR_NO_FREE_MEM        If there is insufficient free parameter RAM
 *                                      to service the allocation.
 * @return  FLCN_ERR_ILLEGAL_OPERATIOn  If this allocator was called anytime after
 *                                      an inference-specific allocation was made
 *                                      via @ref nneParmRamInferenceAlloc was made.
 * @return  FLCN_OK                     If the allocation was successful.
 */
FLCN_STATUS nneParmRamAlloc(LwU32 size, LwU32 *pOffset)
    GCC_ATTRIB_SECTION("imem_nne", "nneParmRamAlloc");
/*!
 * @brief Reset the parameter RAM allocator.
 *
 * This function frees all allocations made to the parameter RAM, including inference
 * specific allocations.
 *
 * @return FLCN_OK   If the allocation was successfully deallocated.
 */
FLCN_STATUS nneParmRamReset(void)
    GCC_ATTRIB_SECTION("imem_nne", "nneParmRamReset");

/*!
 * @brief   Allocator for parameter RAM that can be used for allocations across
 *          inferences.
 *
 * This is a secondary allocator that can be used for allocations that may be
 * used across multiple inference ilwocations. This allows for the allocation to
 * be "revoked" by either the client or by NNE.
 *
 * @param[in]   size    Size of the requested allocation, in bytes.
 * @param[out]  pOffset Offset of the allocated parameter RAM.
 * @param[out]  pSeqNum Sequence number used to track whether this allocation is
 *                      still valid.
 *
 * @return  FLCN_ERR_ILWALID ARGUMENT   If pOffset or pSeqNum is NULL.
 * @return  FLCN_ERR_NO_FREE_MEM        If there is insufficient free parameter
 *                                      RAM to service the allocation.
 * @return  FLCN_OK                     If the allocation was successful.
 */
FLCN_STATUS nneParmRamMultiInferenceAlloc(LwU32 size, LwU32 *pOffset, LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PARM_RAM_SEQ_NUM *pSeqNum)
    GCC_ATTRIB_SECTION("imem_nne", "nneParmRamMultiInferenceAlloc");

/*!
 * @brief Reset multi-inference allocations out of the parameter RAM.
 *
 * This function only resets multi-inference allocations out of the parameter
 * RAM, without resetting all allocations from the parameter RAM.
 *
 * @return FLCN_OK   If the allocation was successfully deallocated.
 */
FLCN_STATUS nneParmRamMultiInferenceReset(void)
    GCC_ATTRIB_SECTION("imem_nne", "nneParmRamMultiInferenceReset");

/*!
 * @brief   Determines whether the given sequence number for a multi-inference
 *          allocation is still valid.
 *
 * @param[in]   seqNum  Sequence number for the allocation
 *
 * @return  LW_TRUE     Allocation still valid.
 * @return  LW_FALSE    Allocation invalid.
 */
LwBool nneParmRamMultiInferenceAllocValid(LW2080_CTRL_NNE_NNE_DESC_INFERENCE_PARM_RAM_SEQ_NUM seqNum)
    GCC_ATTRIB_SECTION("imem_nne", "nneParmRamMultiInferenceAllocValid");

/*!
 * @brief Allocator for parameter RAM freed on a per-inference basis.
 *
 * As a first iteration, we implement a tertiary allocator for the parameter RAM
 * whose allocations can all be de-allocated when an inference is complete, without
 * resetting all allocations on the parameter RAM.
 *
 * @param[IN]  size      Size of the requested allocation, in bytes.
 * @param[OUT] pOffset   Offset of the allocated parameter RAM.
 *
 * @return  FLCN_ERR_ILWALID ARGUMENT   If pOffset is NULL.
 * @return  FLCN_ERR_NO_FREE_MEM        If there is insufficient free parameter RAM
 *                                      to service the allocation.
 * @return  FLCN_OK                     If the allocation was successful.
 */
FLCN_STATUS nneParmRamInferenceAlloc(LwU32 size, LwU32 *pOffset)
    GCC_ATTRIB_SECTION("imem_nne", "nneParmRamInferenceAlloc");

/*!
 * @brief Reset all per-inference allocations of the parameter RAM.
 *
 * This function only resets all per-inference allocations of the parameter RAM,
 * without resetting all allocations on the parameter RAM.
 *
 * @return FLCN_OK   If the allocation was successfully deallocated.
 */
FLCN_STATUS nneParmRamInferenceReset(void)
    GCC_ATTRIB_SECTION("imem_nne", "nneParmRamInferenceReset");

/* ------------------------ Include Derived Types --------------------------- */
#endif // NNE_PARM_RAM_H
