/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    thrmtesthelperbaga10x.c
 * @brief   PMU HAL helper functions for BA testing.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"

#include "config/g_therm_private.h"

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Set the LWVDD factor A value for a specified BA window
 *
 * @param[IN] windowIdx   Window whose factor A value should be set.
 * @param[IN] leakageC    factor A value to set.
 *
 * @return FLCN_ERR_OUT_OF_RANGE   If @ref windowIdx is out of bounds.
 * @return FLCN_OK                 If the leakage C value was correctly set.
 */
FLCN_STATUS
thermTestSetLwvddFactorA_GA10X
(
    LwU8    windowIdx,
    LwU32   factorA
)
{
    LwU32 reg32;
    LwU32 status = FLCN_OK;

    if (windowIdx >= LW_CPWR_THERM_PEAKPOWER_CONFIG6__SIZE_1)
    {
        status = FLCN_ERR_OUT_OF_RANGE;
        goto thermTestSetLwvddFactorA_GA10X_exit;
    }

    reg32 = REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG6(windowIdx));
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG6, _LWVDD_FACTOR_A, factorA, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG6(windowIdx), reg32);

thermTestSetLwvddFactorA_GA10X_exit:
    return status;
}

/*!
 * @brief Get the LWVDD factor A value for a specified BA window
 *
 * @param[IN] windowIdx   Window whose factor A should be retrieved.
 *
 * @return LWVDD factor a value for the specified window.
 */
LwU32
thermTestGetLwvddFactorA_GA10X
(
    LwU8   windowIdx
)
{
    LwU32 reg32;

    if (windowIdx >= LW_CPWR_THERM_PEAKPOWER_CONFIG6__SIZE_1)
    {
        PMU_BREAKPOINT();
        return 0;
    }

    reg32 = REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG6(windowIdx));
    return DRF_VAL(_CPWR_THERM, _PEAKPOWER_CONFIG6, _LWVDD_FACTOR_A, reg32);
}

/*!
 * @brief Set the LWVDD leakage C value for a specified BA window
 *
 * @param[IN] windowIdx   Window whose leakage C value should be set.
 * @param[IN] leakageC    Leakage C value to set.
 *
 * @return FLCN_ERR_OUT_OF_RANGE   If @ref windowIdx is out of bounds.
 * @return FLCN_OK                 If the leakage C value was correctly set.
 */
FLCN_STATUS
thermTestSetLwvddLeakageC_GA10X
(
    LwU8    windowIdx,
    LwU32   leakageC
)
{
    LwU32 reg32;
    LwU32 status = FLCN_OK;

    if (windowIdx >= LW_CPWR_THERM_PEAKPOWER_CONFIG7__SIZE_1)
    {
        status = FLCN_ERR_OUT_OF_RANGE;
        goto thermTestSetLwvddLeakageC_GA10X_exit;
    }

    reg32 = REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG7(windowIdx));
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG7, _LWVDD_LEAKAGE_C,
                            leakageC, reg32);
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG7(windowIdx), reg32);

thermTestSetLwvddLeakageC_GA10X_exit:
    return status;
}

/*!
 * @brief Get the LWVDD leakage C value for a specified BA window
 *
 * @param[IN] windowIdx   Window whose leakage C should be retrieved.
 *
 * @return LWVDD leakage C value for the specified window.
 */
LwU32
thermTestGetLwvddLeakageC_GA10X
(
    LwU8   windowIdx
)
{
    LwU32 reg32;

    if (windowIdx >= LW_CPWR_THERM_PEAKPOWER_CONFIG7__SIZE_1)
    {
        PMU_BREAKPOINT();
        return 0;
    }

    reg32 = REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG7(windowIdx));
    return DRF_VAL(_CPWR_THERM, _PEAKPOWER_CONFIG7, _LWVDD_LEAKAGE_C, reg32);
}
