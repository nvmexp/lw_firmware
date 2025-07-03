/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_PROP_REGIME_H
#define CLK_PROP_REGIME_H

/*!
 * @file clk_prop_regime.h
 * @brief @copydoc clk_prop_regime.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CLK_PROP_REGIME CLK_PROP_REGIME, CLK_PROP_REGIME_BASE;

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Helper macro to return a pointer to the CLK_PROP_REGIMES object.
 *
 * @return Pointer to the CLK_PROP_REGIMES object.
 */
#define CLK_CLK_PROP_REGIMES_GET()                                            \
    (&Clk.propRegimes)

/*!
 * Macro to locate CLK_PROP_REGIMES BOARDOBJGRP.
 *
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_CLK_PROP_REGIME                             \
    (&Clk.propRegimes.super.super)

/*!
 * @brief       Accesor for a CLK_PROP_REGIME BOARDOBJ by BOARDOBJGRP index.
 *
 * @param[in]   _idx  BOARDOBJGRP index for a CLK_PROP_REGIME BOARDOBJ.
 *
 * @return      Pointer to a CLK_PROP_REGIME object at the provided BOARDOBJGRP index.
 *
 * @memberof    CLK_PROP_REGIME
 *
 * @public
 */
#define CLK_PROP_REGIME_GET(_idx)                                             \
    (BOARDOBJGRP_OBJ_GET(CLK_PROP_REGIME, (_idx)))

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock Propagation Regime class params.
 */
struct CLK_PROP_REGIME
{
    /*!
     * BOARDOBJ super class. Must always be the first element in the structure.
     */
    BOARDOBJ            super;
    /*!
     * Clock Propagation Regime Id.
     */
    LwU8                regimeId;
    /*!
     * Mask of clock domains that must be programmed based on their
     * clock propagation relationship.
     */
    BOARDOBJGRPMASK_E32 clkDomainMask;
};

/*!
 * Clock Propagation Regime group params.
 */
typedef struct
{
    /*!
     * BOARDOBJGRP_E32 super class. Must always be the first element in the
     * structure.
     */
    BOARDOBJGRP_E32 super;

    /*!
     * Clock Propagation regime HAL.
     * @ref LW2080_CTRL_CLK_CLK_PROP_REGIME_HAL_<xyz>
     */
    LwU8            regimeHal;

    /*!
     * Regime id @ref LW2080_CTRL_CLK_CLK_PROP_REGIME_ID_<xyz> to board object
     * index map.
     */
    LwBoardObjIdx   regimeIdToIdxMap[LW2080_CTRL_CLK_CLK_PROP_REGIME_ID_MAX];
} CLK_PROP_REGIMES;

/*!
 * @memberof CLK_PROP_REGIME
 *
 * @brief Accessor function which provides a pointer to the CLK_PROP_REGIME structure
 * for the requested regime id (as represented by the @ref LW2080_CTRL_CLK_CLK_PROP_REGIME_ID<XZY>
 * enumeration).
 *
 * @param[in] regimeId  LW2080_CTRL_CLK_CLK_PROP_REGIME_ID_<XZY> regime id requested
 *
 * @return Pointer to CLK_PROP_REGIME structure
 *     If requested regimeId is supported on this GPU.
 * @return NULL
 *     If requested regimeId is not supported on this GPU.
 */
#define ClkPropRegimesGetByRegimeId(fname) CLK_PROP_REGIME * (fname)(LwU8 regimeId)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler           (clkPropRegimeBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkPropRegimeGrpSet");

// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet               (clkPropRegimeGrpIfaceModel10ObjSet_SUPER)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkPropRegimeGrpIfaceModel10ObjSet_SUPER");

// CLK_PROP_REGIME interfaces
ClkPropRegimesGetByRegimeId     (clkPropRegimesGetByRegimeId)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkPropRegimeGrpIfaceModel10ObjSet_SUPER");
/* ------------------------ Include Derived Types -------------------------- */

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief Helper interface to get the @ref CLK_PROP_REGIME:: clkDomainMask
 *
 * @param[in]   pPropRegime    CLK_PROP_REGIME pointer.
 *
 * @return  @ref @ref CLK_PROP_REGIME:: clkDomainMask
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline BOARDOBJGRPMASK_E32 *
clkPropRegimeClkDomainMaskGet
(
    CLK_PROP_REGIME *pPropRegime
)
{
    if (pPropRegime == NULL)
    {
         PMU_BREAKPOINT();
         return NULL;
    }

    return &pPropRegime->clkDomainMask;
}

#endif // CLK_PROP_REGIME_H
