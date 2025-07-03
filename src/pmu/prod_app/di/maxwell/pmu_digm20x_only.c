/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_digm20x_only.c
 * @brief  HAL-interface for GM20x DI Engine
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objdi.h"
#include "pmu_objpmu.h"

#include "config/g_di_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Function Declarations -------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @Brief Initialization of PLL cache based on chip POR
 *
 * Following Fields are initialized :
 *      { Register Address of PLL, Cached Value, PLL Type }
 *
 * @return Base pointer to the List
 */
void
diSeqPllListInitCorePll_GM20X
(
    DI_PLL **ppPllList,
    LwU8    *pllCount
)
{
    static DI_PLL diCorePllList[] = 
    {
        {LW_PTRIM_SYS_GPCPLL_CFG  , 0x0, DI_PLL_TYPE_NAPLL}      ,
        {LW_PTRIM_SYS_LTCPLL_CFG  , 0x0, DI_PLL_TYPE_LEGACY_CORE},
        {LW_PTRIM_SYS_XBARPLL_CFG , 0x0, DI_PLL_TYPE_LEGACY_CORE},
        {LW_PTRIM_SYS_SYSPLL_CFG  , 0x0, DI_PLL_TYPE_LEGACY_CORE}
    };

    *pllCount  = (LwU8) LW_ARRAY_ELEMENTS(diCorePllList);
    *ppPllList = diCorePllList;
}

/*!
 * @brief Assert / De-assert DRAMPLL GC5 Clamp
 *
 * This function controls the clamping of DRAMPLL and CDB clock out for DRAMPLL power down
 *
 * @param[in]   LW_TRUE     Assert the Clamp
 *              LW_FALSE    Deassert the Clamp
 */
void
diSeqDramPllClampSet_GM20X
(
    LwBool bClamp
)
{
    LwU32 regVal = REG_RD32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH);

    if (bClamp)
    {
        regVal = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMPLL_CLAMP_GC5, _ENABLE, regVal);
    }
    else
    {
        regVal = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMPLL_CLAMP_GC5, _DISABLE, regVal);
    }

    REG_WR32(FECS, LW_PTRIM_SYS_FBIO_MODE_SWITCH, regVal);
}
