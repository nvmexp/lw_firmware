/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_DOMAIN_35_PROG_H
#define CLK_DOMAIN_35_PROG_H

/*!
 * @file clk_domain_35_prog.h
 * @brief @copydoc clk_domain_35_prog.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_domain_3x_prog.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Accessor macro for CLK_DOMAIN's VF lwrve count.
 *
 * @param[in] pDomain35Prog CLK_DOMAIN_35_PROG pointer
 *
 * @return CLK_DOMAIN's VF lwrve count.
 */
#define clkDomain35ProgGetClkDomailwfLwrveCount(pDomain35Prog)                \
    ((pDomain35Prog)->clkVFLwrveCount)

/*!
 * Accessor macro for CLK_DOMAIN position in VF frequency tuple based on
 * input CLK_DOMAIN's clkIdx in its board object group.
 *
 * @param[in] pDomain35Prog CLK_DOMAIN_35_PROG pointer
 * @param[in] lwrveIdx      CLK DOMAIN VF Lwrve index
 *
 * @return CLK_DOMAIN position based on input CLK_DOMAIN index clkIdx
 */
#define clkDomain35ProgGetClkDomainPosByIdx(pDomain35Prog, lwrveIdx)          \
    (((lwrveIdx) < (pDomain35Prog)->clkVFLwrveCount) ?                        \
        ((pDomain35Prog)->clkPos) : LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID)

/*!
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define CLK_DOMAIN_35_PROG_GET(_clkIdx)                                       \
    ((CLK_DOMAIN_35_PROG *)BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, (_clkIdx)))

/*!
 * Accessor macro for CLK_DOMAIN_35_PROG::pPorVoltDeltauV[(voltRailIdx)]
 *
 * @param[in] pProg3xPrimary  CLK_DOMAIN_35_PROG pointer
 * @param[in] voltRailIdx    VOLTAGE_RAIL index
 *
 * @return @ref CLK_DOMAIN_35_PROG::pPorVoltDeltauV[(voltRailIdx)]
 */
#define clkDomain35ProgPorVoltDeltauVGet(pDomain35Prog, voltRailIdx)          \
    (((voltRailIdx) < Clk.domains.voltRailsMax) ?                             \
     ((pDomain35Prog)->pPorVoltDeltauV[(voltRailIdx)]) : 0)

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock Domain 35 Prog structure.  Contains information specific to a
 * Programmable Clock Domain.
 */
typedef struct
{
    /*!
     * CLK_DOMAIN_3X_PROG super class. Must always be first object in the
     * structure.
     */
    CLK_DOMAIN_3X_PROG  super;
    /*!
     * Pre-Volt ordering index for clock programming changes.
     */
    LwU8                preVoltOrderingIndex;
    /*!
     * Post-Volt ordering index for clock programming changes.
     */
    LwU8                postVoltOrderingIndex;
    /*!
     * Position of clock domain in a tightly packed array of primary - secondary clock
     * domain V and/or F tuples.
     */
    LwU8                clkPos;
   /*!
     * Count of total number of (primary + secondary) lwrves supported on this clock domain.
     */
    LwU8                clkVFLwrveCount;
    /*!
     * Clock Monitor specific information used to compute the threshold values.
     */
    LW2080_CTRL_CLK_CLK_DOMAIN_INFO_35_PROG_CLK_MON clkMonInfo;
    /*!
     * Clock Monitors specific information used to override the threshold values.
     */
    LW2080_CTRL_CLK_CLK_DOMAIN_CONTROL_35_PROG_CLK_MON clkMonCtrl;
    /*!
     * Represents voltage delta defined in VBIOS clocks table as per LW POR.
     * This will give the deviation of given voltage from it's nominal value.
     */
    LwS32              *pPorVoltDeltauV;
} CLK_DOMAIN_35_PROG;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet               (clkDomainGrpIfaceModel10ObjSet_35_PROG)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkDomainGrpIfaceModel10ObjSet_35_PROG");

ClkDomainProgVoltToFreqTuple            (clkDomainProgVoltToFreqTuple_35)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkDomainProgVoltToFreqTuple_35");
ClkDomainProgPreVoltOrderingIndexGet    (clkDomainProgPreVoltOrderingIndexGet_35)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgPreVoltOrderingIndexGet_35");
ClkDomainProgPostVoltOrderingIndexGet   (clkDomainProgPostVoltOrderingIndexGet_35)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgPostVoltOrderingIndexGet_35");
ClkDomainsProgFreqPropagate             (clkDomainsProgFreqPropagate_35)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainsProgFreqPropagate_35");
ClkDomainProgIsSecVFLwrvesEnabled       (clkDomainProgIsSecVFLwrvesEnabled_35)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgIsSecVFLwrvesEnabled_35");

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/clk_domain_35_primary.h"
#include "clk/clk_domain_35_secondary.h"

#endif // CLK_DOMAIN_35_PROG_H
