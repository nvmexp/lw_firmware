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
 * @file    objnne.h
 * @brief   Declarations, type definitions, and macros for the
 *          Neural Net Engine (NNE) SW module.
 */

#ifndef OBJNNE_H
#define OBJNNE_H


/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"
#include "nne/nne_var.h"
#include "nne/nne_layer.h"
#include "nne/nne_desc.h"
#include "nne/nne_swzl_ram.h"
#include "nne/nne_sync.h"

/* ------------------------ Application Includes --------------------------- */
#include "config/g_nne_hal.h"
#include "config/pmu-config.h"

/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Types Definitions ------------------------------ */
/*!
 * @brief   Neural Net Engine (NNE) Object definition
 */
typedef struct
{
#if !PMUCFG_FEATURE_ENABLED(PMU_NNE_VAR) && \
    !PMUCFG_FEATURE_ENABLED(PMU_NNE_LAYER) && \
    !PMUCFG_FEATURE_ENABLED(PMU_NNE_DESC)
    /*!
     * @brief   Dummy variable for when this file is included on builds where
     *          all three of the following are not supported.
     */
    LwU8 dummy;
#endif

    /*!
     * @brief Set of all input variables used by all neural-nets
     * @protected
     */
#if (PMUCFG_FEATURE_ENABLED(PMU_NNE_VAR))
    NNE_VARS         vars;
#endif

    /*!
     * @brief Set of all layers used by all neural-nets
     * @protected
     */
#if (PMUCFG_FEATURE_ENABLED(PMU_NNE_LAYER))
    NNE_LAYERS       layers;
#endif

#if (PMUCFG_FEATURE_ENABLED(PMU_NNE_DESC))
    NNE_DESCS        descs;
#endif

} OBJNNE;

/*!
 * Resident Neural Net Engine (NNE) object definition
 */
typedef struct
{
    /*!
     * @brief   Synchronization structure for waiting for an inference to
     *          complete.
     */
    NNE_SYNC    nneSync;
} OBJNNE_RESIDENT;

/* ------------------------ External Definitions --------------------------- */
/*!
 * @brief External reference to @ref OBJNNE.
 */
extern OBJNNE             Nne;
extern OBJNNE_RESIDENT    NneResident;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/*!
 * @brief   NNE pre-init function.
 */
void  nnePreInit(void)
    GCC_ATTRIB_SECTION("imem_init", "nnePreInit");

/*!
 * @brief   NNE post-init function.
 *
 * @return  FLCN_OK     On success
 */
FLCN_STATUS nnePostInit(void)
    GCC_ATTRIB_SECTION("imem_nneInit", "nnePostInit");

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

/*!
 * @brief   Service any pending interrupts belonging to NNE
 */
void nneService(void)
    GCC_ATTRIB_SECTION("imem_resident", "nneService");

#endif // OBJNNE_H
