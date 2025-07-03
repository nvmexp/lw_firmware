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
 * @file   pmu_digm10x_only.c
 * @brief  HAL-interface for GM10x(only) DI Engine
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"
#include "dev_trim_addendum.h"

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
diSeqPllListInitCorePll_GM10X
(
    DI_PLL **ppPllList,
    LwU8    *pllCount
)
{
    static DI_PLL diCorePllList[] = 
    {
        {LW_PTRIM_SYS_GPCPLL_CFG  , 0x0, DI_PLL_TYPE_NAPLL}      ,
        {LW_PTRIM_SYS_XBARPLL_CFG , 0x0, DI_PLL_TYPE_LEGACY_CORE},
        {LW_PTRIM_SYS_SYSPLL_CFG  , 0x0, DI_PLL_TYPE_LEGACY_CORE}
    };

    *pllCount  = (LwU8) LW_ARRAY_ELEMENTS(diCorePllList);
    *ppPllList = diCorePllList;
}
