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
 * @file nne_desc_ram.h
 * @brief DLC descriptor RAM chip-agnostic interfaces.
 */

#ifndef NNE_DESC_RAM_H
#define NNE_DESC_RAM_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Allocator for the descriptor RAM.
 *
 * @param[IN]  size      Size, in bytes, of the requested allocation.
 * @param[OUT] pOffset   Offset of the RAM allocation.
 * @param[OUT} pNnetId   Neural-net ID that was assigned to the allocation.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If size is 0, not dword aligned, or NULL
 *                                     @ref pOffset was passed in.
 * @return FLCN_ERR_NO_FREE_MEM        If there is insufficient memory to fulfill the
 *                                     allocation request.
 * @return FLCN_OK                     If the allocation was successful.
 */
FLCN_STATUS nneDescRamAlloc(LwU32 size, LwU32 * pOffset, LwU8 * pNnetId)
    GCC_ATTRIB_SECTION("imem_nne", "nneDescRamAlloc");

/*!
 * @brief De-allocator for the descriptor RAM.
 *
 * @param[IN] nnetId   neural-net ID to free, along with its allocated descriptor RAM.
 */
void nneDescRamFree(LwU8 nnetId)
    GCC_ATTRIB_SECTION("imem_nne", "nneDescRamFree");

#endif // NNE_DESC_RAM_H
