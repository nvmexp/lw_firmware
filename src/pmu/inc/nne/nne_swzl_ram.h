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
 * @file nne_swzl_ram.h
 * @brief DLC swizzle RAM interface.
 */

#ifndef NNE_SWZL_RAM_H
#define NNE_SWZL_RAM_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
typedef struct NNE_SWZL NNE_SWZL;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief Class representing a single swizzle entry.
 *
 * A swizzle is a mapping of a variable's index to its location in parameter RAM,
 * including any stride that should be applied on a per-inference loop basis to
 * reach its per-inference loop instances.
 */
struct NNE_SWZL
{
    /*!
     * @brief Offset into the parameter RAM that the first instance of the input exists at.
     */
    LwU32 parmRamOffset;

    /*!
     * @brief Stride, in bytes, that should be added to @ref paramRamOffset to get
     *        each per-inference loop instance of the input.
     */
    LwU32 stride;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Allocator for the swizzle RAM.
 *
 * @param[IN]  size      Size of the requested allocation, in bytes.
 * @param[OUT] pOffset   Offset of the allocated swizzle RAM.
 *
 * @return  FLCN_ERR_ILWALID ARGUMENT   If pOffset is NULL.
 * @return  FLCN_ERR_NO_FREE_MEM        If there is insufficient free swizzle RAM
 *                                      to service the allocation.
 * @return  FLCN_ERR_ILLEGAL_OPERATIOn  If this allocator was called anytime after
 *                                      an inference-specific allocation was made
 *                                      via @ref nneSwzlRamInferenceAlloc was made.
 * @return  FLCN_OK                     If the allocation was successful.
 */
FLCN_STATUS nneSwzlRamAlloc(LwU32 size, LwU32 *pOffset)
    GCC_ATTRIB_SECTION("imem_nne", "nneSwzlRamAlloc");
/*!
 * @brief Reset the swizzle RAM allocator.
 *
 * This function frees all allocations made to the swizzle RAM, including inference
 * specific allocations.
 *
 * @return FLCN_OK   If the allocation was successfully deallocated.
 */
FLCN_STATUS nneSwzlRamReset(void)
    GCC_ATTRIB_SECTION("imem_nne", "nneSwzlRamReset");

/*!
 * @brief Allocator for swizzle RAM freed on a per-inference basis.
 *
 * As a first iteration, we implement a secondary allocator for the swizzle RAM
 * whose allocations can all be de-allocated when an inference is complete, without
 * resetting all allocations on the swizzle RAM.
 *
 * @param[IN]  size      Size of the requested allocation, in bytes.
 * @param[OUT] pOffset   Offset of the allocated swizzle RAM.
 *
 * @return  FLCN_ERR_ILWALID ARGUMENT   If pOffset is NULL.
 * @return  FLCN_ERR_NO_FREE_MEM        If there is insufficient free swizzle RAM
 *                                      to service the allocation.
 * @return  FLCN_OK                     If the allocation was successful.
 */
FLCN_STATUS nneSwzlRamInferenceAlloc(LwU32 size, LwU32 *pOffset)
    GCC_ATTRIB_SECTION("imem_nne", "nneSwzlRamInferenceAlloc");

/*!
 * @brief Reset all per-inference allocations of the swizzle RAM.
 *
 * This function only resets all per-inference allocations of the swizzle RAM,
 * without resetting all allocations on the swizzle RAM.
 *
 * @return FLCN_OK   If the allocation was successfully deallocated.
 */
FLCN_STATUS nneSwzlRamInferenceReset(void)
    GCC_ATTRIB_SECTION("imem_nne", "nneSwzlRamInferenceReset");

/*!
 * @brief Generate and write a direct mapping swizzle to swizzle RAM.
 *
 * A direct swizzle mapping is one where an input's index into the swizzle RAM
 * equals it's index into the parameter RAM, and has no stride applied.
 *
 * @param[IN]  swzlRamBaseOffset   Base offset into the swizzle RAM to write the swizzles to.
 * @param[IN]  parmRamBaseOffset   Base offset into the parameter RAM to generate
 *                                 the direct mapping with.
 * @param[IN]  Number of swizzles to generate.
 *
 * @return FLCN_ERR_OUT_OF_RANGE   If the swizzle RAM range to write is not within bounds of the swizzle RAM.
 *                                 If the requested parameter RAM range to direct map to is out of the range
 *                                 of the parameter RAM.
 * @return FLCN_OK                 If the direct mapping was successfully written to swizzle RAM.
 */
FLCN_STATUS nneSwzlDirectMappingWrite(LwU32 swzlRamBaseOffset, LwU32 parmRamBaseOffset, LwU32 numSwzl)
    GCC_ATTRIB_SECTION("imem_nne", "nneSwzlDirectMappingWrite");

/*!
 * @brief Write an array of swizzles to the swizzle RAM.
 *
 * @param[IN] pSwzl           Array of swizzles to write to swizzle RAM.
 * @param[IN] swzlRamOffset   Offset into swizzle RAM to write the swizzles to.
 * @param[IN] numSwzl         Number of swizzles to write to swizzle RAM.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If pSwzl is NULL.
 * @return FLCN_ERR_OUT_OF_RANGE       If the swizzle RAM range to write is not within bounds of the swizzle RAM.
 * @return FLCN_OK                     If the swizzle was successfully written to swizzle RAM.
 */
FLCN_STATUS nneSwzlWrite(NNE_SWZL *pSwzl, LwU32 swzlRamOffset, LwU32 numSwzl)
    GCC_ATTRIB_SECTION("imem_nne", "nneSwzlWrite");

/* ------------------------ Include Derived Types --------------------------- */
#endif // NNE_SWZL_RAM_H
