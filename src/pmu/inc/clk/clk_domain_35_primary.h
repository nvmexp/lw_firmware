/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_DOMAIN_35_PRIMARY_H
#define CLK_DOMAIN_35_PRIMARY_H

#include "g_clk_domain_35_primary.h"

#ifndef G_CLK_DOMAIN_35_PRIMARY_H
#define G_CLK_DOMAIN_35_PRIMARY_H

/*!
 * @file clk_domain_35_primary.h
 * @brief @copydoc clk_domain_35_primary.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_domain_35_prog.h"
#include "clk/clk_domain_3x_primary.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Accessor macro for a CLK_DOMAIN_3X_PRIMARY structure from CLK_DOMAIN_35_PRIMARY.
 */
#define CLK_CLK_DOMAIN_3X_PRIMARY_GET_FROM_35_PRIMARY(_pPrimary35)               \
    &((_pPrimary35)->primary)

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Forward definition of CLK_DOMAIN_35_PRIMARY structures so that can use the
 * type in interface definitions.
 */
typedef struct CLK_DOMAIN_35_PRIMARY CLK_DOMAIN_35_PRIMARY;

/*!
 * Clock Domain 35 Primary structure.  Contains information specific to a
 * Primary Clock Domain.
 */
struct CLK_DOMAIN_35_PRIMARY
{
    /*!
     * CLK_DOMAIN_35_PROG super class. Must always be first object in the
     * structure.
     */
    CLK_DOMAIN_35_PROG      super;

    /*!
     * CLK_DOMAIN_3X_PRIMARY super class.
     */
    CLK_DOMAIN_3X_PRIMARY   primary;

    /*!
     * Mask of clock domain index of this primary clock domain and
     * all of its secondaries.
     */
    BOARDOBJGRPMASK_E32     primarySecondaryDomainsGrpMask;
};

/*!
 * @memberof CLK_DOMAIN_35_PRIMARY
 *
 * Helper interface containing common code for V -> F tuple look up. Both
 * primary and secondary clock domains will call this interface.
 *
 * @param[in]  pDomain35Primary    CLK_DOMAIN_35_PRIMARY pointer
 * @param[in]  clkDomIdx          Clock domain index.
 * @param[in]  voltRailIdx
 *     Index of the VOLT_RAIL for which to look-up.
 * @param[in]  voltageType
 *     Type of voltage value by which to look-up.  @ref
 *     LW2080_CTRL_CLK_VOLTAGE_TYPE_<xyz>.
 * @param[in]  pInput
 *     Pointer to structure specifying the voltage value to look-up.
 * @param[out] pOutput
 *     Pointer to structure in which to return the frequency value corresponding
 *     to the input voltage value.
 * @param[in/out] pVfIterState
 *     Pointer to structure containing the VF iteration state for the VF
 *     look-up.  This tracks all data which must be preserved across this VF
 *     look-up and potentially across subsequent look-ups.  In cases where
 *     subsequent searches are in order of increasing input criteria, each
 *     subsequent look-up can "resume" where the prevoius look-up finished using
 *     this iteration state. If no state tracking is desired, client should
 *     specify NULL.
 *
 * @return FLCN_OK
 *     The input voltage was successfully translated to an output frequency.
 * @return Other errors
 *     An unexpected error oclwrred during look-up/translation.
 */
#define ClkDomain35PrimaryVoltToFreqTuple(fname) FLCN_STATUS (fname)(CLK_DOMAIN_35_PRIMARY *pDomain35Primary, LwU8 clkDomIdx, LwU8 voltRailIdx, LwU8 voltageType, LW2080_CTRL_CLK_VF_INPUT *pInput, LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE *pOutput, LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet   (clkDomainGrpIfaceModel10ObjSet_35_PRIMARY)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkDomainGrpIfaceModel10ObjSet_35_PRIMARY");

ClkDomainProgClientFreqDeltaAdj     (clkDomainProgClientFreqDeltaAdj_35_PRIMARY)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgClientFreqDeltaAdj_35_PRIMARY");
mockable ClkDomainProgVoltToFreqTuple (clkDomainProgVoltToFreqTuple_35_PRIMARY)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkDomainProgVoltToFreqTuple_35_PRIMARY");
ClkDomain3XProgVoltAdjustDeltauV    (clkDomain3XProgVoltAdjustDeltauV_35_PRIMARY)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain3XProgVoltAdjustDeltauV_35_PRIMARY");

// CLK_DOMAIN interfaces
ClkDomainCache                      (clkDomainCache_35_Primary)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkDomainCache_35_Primary");

ClkDomainLoad                       (clkDomainLoad_35_Primary)
    GCC_ATTRIB_SECTION("imem_vfLoad", "clkDomainLoad_35_Primary");

// CLK_DOMAIN_35_PRIMARY interfaces
ClkDomain35PrimaryVoltToFreqTuple    (clkDomain35PrimaryVoltToFreqTuple)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain35PrimaryVoltToFreqTuple");

/* ------------------------ Include Derived Types -------------------------- */

#endif // G_CLK_DOMAIN_35_PRIMARY_H
#endif // CLK_DOMAIN_35_PRIMARY_H
