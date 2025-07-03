/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_DOMAIN_3X_SECONDARY_H
#define CLK_DOMAIN_3X_SECONDARY_H

/*!
 * @file clk_domain_3x_secondary.h
 * @brief @copydoc clk_domain_3x_secondary.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "boardobj/boardobjgrp.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Forward definition of CLK_DOMAIN_3X_SECONDARY structures so that can use the
 * type in interface definitions.
 */
typedef struct CLK_DOMAIN_3X_SECONDARY CLK_DOMAIN_3X_SECONDARY;

/*!
 * Clock Domain 3X Secondary structure. Contains information specific to a
 * Secondary Clock Domain.
 */
struct CLK_DOMAIN_3X_SECONDARY
{
    /*!
     * BOARDOBJ_INTERFACE super class. Must always be first object in the
     * structure.
     */
    BOARDOBJ_INTERFACE  super;

    /*!
     * CLK_DOMAIN index of primary CLK_DOMAIN.
     */
    LwU8                primaryIdx;
};

/*!
 * @memberof CLK_DOMAIN_3X_SECONDARY
 *
 * @brief Helper interface to adjust the given frequency by the frequency delta
 * of this CLK_DOMAIN_3X_SECONDARY object.
 *
 * Will also include the offsets corresponding to this CLK_DOMAIN_3X_SECONDARY
 * object's PRIMARY if requested. Resulting frequency will always be quantized.

 * @param[in]     pDomain3XSecondary   CLK_DOMAIN_3X_SECONDARY pointer
 * @param[in]     pProg3XPrimary    CLK_PROG_3X_PRIMARY pointer
 * @param[in]     bIncludePrimary
 *     Boolean indiciating whether to include offsets corresponding to this
 *     CLK_DOMAIN_3X_SECONDARY object's PRIMARY.  Will call @ref
 *     ClkDomain3XPrimaryFreqAdjustDeltaMHz().  Certain use-cases will use this
 *     because pFreqMHz does not already include the PRIMARY offsets (e.g. @ref
 *     CLK_PROG_1X_PRIMARY_TABLE), while in other use-cases pFreqMHz will already
 *     include those PRIMARY offsets (e.g. @ref CLK_PROG_1X_PRIMARY_RATIO).
 * @param[in]     bQuantize
 *     Boolean indiciating whether the result should be quantized or not.
 * @param[in,out] pFreqMHz
 *     Pointer in which caller specifies frequency to adjust and in which the
 *     adjusted frequency is returned.
 *
 * @return FLCN_OK
 *     Frequency successfully adjusted.
 * @return Other errors
 *     An unexpected error oclwrred during adjustment.
 */
#define ClkDomain3XSecondaryFreqAdjustDeltaMHz(fname) FLCN_STATUS (fname)(CLK_DOMAIN_3X_SECONDARY *pDomain3XSecondary, CLK_PROG_3X_PRIMARY *pProg3XPrimary, LwBool bIncludePrimary, LwBool bQuantize, LwBool bVFOCAdjReq, LwU16 *pFreqMHz)


/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
BoardObjInterfaceConstruct              (clkDomainConstruct_3X_SECONDARY)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkDomainConstruct_3X_SECONDARY");

// CLK_DOMAIN_3X_PROG interfaces
ClkDomainProgVoltToFreq                 (clkDomainProgVoltToFreq_3X_SECONDARY)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgVoltToFreq_3X_SECONDARY");
ClkDomainProgFreqToVolt                 (clkDomainProgFreqToVolt_3X_SECONDARY)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgFreqToVolt_3X_SECONDARY");
ClkDomain3XProgVoltAdjustDeltauV        (clkDomain3XProgVoltAdjustDeltauV_SECONDARY)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain3XProgVoltAdjustDeltauV_SECONDARY");
ClkDomainProgVfMaxFreqMHzGet            (clkDomainProgMaxFreqMHzGet_3X_SECONDARY)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgMaxFreqMHzGet_3X_SECONDARY");
ClkDomain3XProgFreqAdjustDeltaMHz       (clkDomain3XProgFreqAdjustDeltaMHz_SECONDARY)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain3XProgFreqAdjustDeltaMHz_SECONDARY");

// CLK_DOMAIN_3X_SECONDARY interfaces
ClkDomain3XSecondaryFreqAdjustDeltaMHz   (clkDomain3XSecondaryFreqAdjustDeltaMHz)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain3XSecondaryFreqAdjustDeltaMHz");
ClkDomain3XProgIsOVOCEnabled         (clkDomain3XProgIsOVOCEnabled_3X_SECONDARY)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain3XProgIsOVOCEnabled_3X_SECONDARY");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_DOMAIN_3X_SECONDARY_H
