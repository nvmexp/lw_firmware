/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file vpstate_3x.h
 * @copydoc vpstate_3x.c
 *
 * @brief   VPSTATE 3X related defines.
 */

#ifndef VPSTATE_3X_H
#define VPSTATE_3X_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/vpstate.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct VPSTATE_3X VPSTATE_3X;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @interface VPSTATE_3X
 *
 * Accessor macro to retrieve @ref VPSTATE_3X::pstateIdx.
 *
 * @note : The naming is NOT correct. It is PSTATE::level and NOT PSTATE::index.
 *
 * @param[in]   _pVpstate3X
 *    Pointer to @ref VPSTATE_3X
 *
 * @return @ref VPSTATE_3X::pstateIdx
 */
#define vpstate3xPstateIdxGet(_pVpstate3x)                                     \
    ((_pVpstate3x)->pstateIdx)

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */

/*!
 * Entry representing a single clock domain inside a @ref VPSTATE_3X entry.
 */
typedef struct
{
    LwU16   targetFreqMHz;                          // Target frequency in MHz.
    LwU16   minEffFreqMHz;                          // Minimum effective frequency in MHz.
} VPSTATE_3X_CLOCK_ENTRY;

/*!
 * The VPSTATE_3X represents a single VPstate entry in Pstate 3.0.
 */
struct VPSTATE_3X
{
    /*!
     * VPSTATE parent class. Must be first element of the structure!
     */
    VPSTATE                 super;
    /*!
     *  Index pointing to the P-state associated with this Vpstate.
     */
    LwU8                    pstateIdx;
    /*!
     * Mask for all supported clock domains.
     * @todo kyleo: change to LW2080_CTRL_BOARDOBJGRP_MASK_E32.
     */
    LwU32                   clkDomainsMask;
    /*!
     * Array of clocks specified by this Vpstate.
     */
    VPSTATE_3X_CLOCK_ENTRY *pClocks;
};

/*!
 * @interface VPSTATE_3X
 *
 * Accessor interface to retrive the @ref VPSTATE_3X::clkDomainsMask.
 *
 * @param[in]      pVpstate3x
 *     Pointer to @ref VPSTATE_3X.
 * @param[out]     pClkDomainsMask
 *     Pointer to @ref BOARDOBJGRPMASK_E32 in which to return the
 *     clkDomainsMask.
 *
 * @return FLCN_OK
 *     @ref VPSTATE_3X::clkDomains successfully retrieved.
 * @return Other errors
 *     Unexpected errors.
 */
#define Vpstate3xClkDomainsMaskGet(fname) FLCN_STATUS (fname)(VPSTATE_3X *pVpstate3x, BOARDOBJGRPMASK_E32 *pClkDomainsMask)

/*!
 * @interface VPSTATE_3X
 *
 * Accessor interface to retrive the @ref VPSTATE_3X_CLOCK_ENTRY for a given
 * CLK_DOMAIN index.
 *
 * @param[in]      pVpstate3x
 *     Pointer to @ref VPSTATE_3X.
 * @param[in]      clkDomainIdx
 *     Index of the CLK_DOMAIN to retrieve.
 * @param[out]     pVpstateClkEntry
 *     Pointer in which to return the VPSTATE_3X_CLOCK_ENTRY.
 *
 * @return FLCN_OK
 *     @ref VPSTATE_3X_CLOCK_ENTRY successfully retrieved.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     @ref VPSTATE_3X::clkDomainsMask does not contain the specified @ref
 *     clkDomainIdx.
 * @return Other errors
 *     Unexpected errors.
 */
#define Vpstate3xClkDomainGet(fname) FLCN_STATUS (fname)(VPSTATE_3X *pVpstate3x, LwU8 clkDomainIdx, VPSTATE_3X_CLOCK_ENTRY *pVpstateClkEntry)

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet   (vpstateGrpIfaceModel10ObjSetImpl_3X)
    GCC_ATTRIB_SECTION("imem_libVpstateConstruct", "vpstateGrpIfaceModel10ObjSetImpl_3X");

// VPSTATE interfaces
VpstateDomGrpGet    (vpstateDomGrpGet_3X)
    GCC_ATTRIB_SECTION("imem_perfVf", "vpstateDomGrpGet_3X");

// VPSTATE_3X interfaces
Vpstate3xClkDomainsMaskGet (vpstate3xClkDomainsMaskGet)
    GCC_ATTRIB_SECTION("imem_perf", "vpstate3xClkDomainsMaskGet");
Vpstate3xClkDomainGet      (vpstate3xClkDomainGet)
    GCC_ATTRIB_SECTION("imem_perfVf", "vpstate3xClkDomainGet");

/* ------------------------ Include Derived Types --------------------------- */

#endif // VPSTATE_3X_H
