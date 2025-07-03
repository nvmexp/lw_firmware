/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file nne_weight_ram.h
 * @brief DLC weight RAM interface.
 */

#ifndef NNE_WEIGHT_RAM_H
#define NNE_WEIGHT_RAM_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Allocator for the weight RAM.
 *
 * @param[IN]  size      Size of the requested allocation, in bytes.
 * @param[OUT] pOffset   Offset of the allocated weight RAM.
 *
 * @return  FLCN_ERR_ILWALID ARGUMENT   If pOffset is NULL.
 * @return  FLCN_ERR_NO_FREE_MEM        If there is insufficient free weight RAM
 *                                      to service the allocation.
 * @return  FLCN_ERR_ILLEGAL_OPERATIOn  If this allocator was called anytime after
 *                                      an inference-specific allocation was made
 *                                      via @ref nneWeightRamInferenceAlloc was made.
 * @return  FLCN_OK                     If the allocation was successful.
 */
FLCN_STATUS nneWeightRamAlloc(LwU32 size, LwU32 *pOffset)
    GCC_ATTRIB_SECTION("imem_nne", "nneWeightRamAlloc");
/*!
 * @brief Reset the weight RAM allocator.
 *
 * This function frees all allocations made to the weight RAM, including inference
 * specific allocations.
 *
 * @return FLCN_OK   If the allocation was successfully deallocated.
 */
FLCN_STATUS nneWeightRamReset(void)
    GCC_ATTRIB_SECTION("imem_nne", "nneWeightRamReset");

/* ------------------------ Include Derived Types --------------------------- */
#endif // NNE_WEIGHT_RAM_H
