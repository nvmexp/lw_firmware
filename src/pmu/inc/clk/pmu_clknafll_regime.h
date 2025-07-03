/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_CLKNAFLL_REGIME_H
#define PMU_CLKNAFLL_REGIME_H

/*!
 * @file pmu_clknafll_regime.h
 *
 * @copydoc pmu_clknafll_regime.c
 */

/* ------------------------ System Includes -------------------------------- */
#include "main.h"

/* ------------------------ Application Includes --------------------------- */
#include "pmu_oslayer.h"
#include "pmu_objtimer.h"
#include "perf/3x/vfe.h"

/* ------------------------ Defines ---------------------------------------- */
/*!
 * Macros for PLDIV values
 */
#define CLK_NAFLL_PLDIV_DIV1                                               (0x1U)
#define CLK_NAFLL_PLDIV_DIV2                                               (0x2U)
#define CLK_NAFLL_PLDIV_DIV3                                               (0x3U)
#define CLK_NAFLL_PLDIV_DIV4                                               (0x4U)


/* ------------------------ Types Definitions ------------------------------ */
/*!
 * Structure describing the static state of a DVCO
 */
typedef struct
{
    /*!
     * Enable state of this DVCO.
     */
    LwBool          bEnabled;

    /*!
     * Min Frequency VFE Index
     */
    LwVfeEquIdx     minFreqIdx;

    /*!
     * Flag to indicate whether 1x DVCO is in use. TRUE means 1x DVCO,
     * otherwise 2x DVCO.
     */
    LwBool          bDvco1x;
} CLK_NAFLL_DVCO;

/*!
 * Structure containing the state of regime logic.
 */
typedef struct
{
    /*!
     * The global ID @ref LW2080_CTRL_PERF_NAFLL_ID_<xyz> of this NAFLL device
     */
    LwU8    nafllId;

    /*!
     * Override value for the target regime Id
     */
    LwU8    targetRegimeIdOverride;

    /*!
     * The frequency limit for fixed frequency regime, comes from VBIOS table.
     */
    LwU16   fixedFreqRegimeLimitMHz;
} CLK_NAFLL_REGIME_DESC;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// Routines that belong to the .perfClkAvfsInit overlay
FLCN_STATUS clkNafllRegimeInit(CLK_NAFLL_DEVICE *pNafllDev, RM_PMU_CLK_REGIME_DESC *pRegimeDesc, LwBool bFirstConstruct)
    GCC_ATTRIB_SECTION("imem_perfClkAvfsInit", "clkNafllRegimeInit");

// Routines that belong to the .perfClkAvfs overlay

FLCN_STATUS clkNafllConfig(CLK_NAFLL_DEVICE *pNafllDev, LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus, RM_PMU_CLK_FREQ_TARGET_SIGNAL *pTarget)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllConfig");
FLCN_STATUS clkNafllProgram(CLK_NAFLL_DEVICE *pNafllDev, LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllProgram");
LwU8 clkNafllRegimeByIdGet(LwU8 nafllId)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllRegimeByIdGet");
LwU8 clkNafllRegimeGet(CLK_NAFLL_DEVICE *pNafllDev)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllRegimeGet");
LwU16 clkNafllFreqMHzGet(CLK_NAFLL_DEVICE *pNafllDev)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllFreqMHzGet");
FLCN_STATUS clkNafllTargetRegimeGet(LW2080_CTRL_CLK_CLK_DOMAIN_LIST_ITEM *pClkListItem, LW2080_CTRL_VOLT_VOLT_RAIL_LIST *pVoltList)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllTargetRegimeGet");
LwU8 clkNafllDevTargetRegimeGet(CLK_NAFLL_DEVICE *pNafllDev, LwU16 targetFreqMHz, LwU16 dvcoMinFreqMHz, LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *pVoltListItem)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllDevTargetRegimeGet");
FLCN_STATUS clkNafllGrpDvcoMinCompute(LW2080_CTRL_VOLT_VOLT_RAIL_LIST *pVoltList)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllGrpDvcoMinCompute");
FLCN_STATUS clkNafllPldivConfig(CLK_NAFLL_DEVICE *pNafllDev, LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllPldivConfig");
FLCN_STATUS clkNafllPldivProgram(CLK_NAFLL_DEVICE *pNafllDev, LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllPldivProgram");
FLCN_STATUS clkNafllGetHwRegimeBySwRegime(LwU8 swRegimeId, LwU8 *pHwRegimeId)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllGetHwRegimeBySwRegime");

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/pmu_clknafll_regime_2x.h"
#include "clk/pmu_clknafll_regime_30.h"

#endif // PMU_CLKNAFLL_REGIME_H
