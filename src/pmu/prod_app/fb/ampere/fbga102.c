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
 * @file   pmu_fbga102.c
 * @brief  FB Hal Functions for GA102
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_fbfalcon_pri.h"
#include "dev_pri_ringmaster.h"
#include "dev_pmgr_addendum.h"
#include "dev_pmgr.h"
#include "dev_fuse.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_objfb.h"
#include "pmu_objfuse.h"
#include "dbgprintf.h"
#include "config/g_fb_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Retry count for re-attempting the temperature read in case HW returns
 * invalid value.
 */
#define LW_PFBFALCON_MEMORY_TEMPERATURE_RETRY_CNT                     3

/*!
 * Intermediate read delay between subsequent memory temperature reads.
 */
#define LW_PFBFALCON_MEMORY_TEMPERATURE_READ_DELAY_INTERMEDIATE_US    5

/* ------------------------- Prototypes ------------------------------------- */

/*!
 * @brief Gets the active LTC count.
 *
 * @param[in]   pGpu    OBJGPU pointer.
 * @param[in]   pFb     OBJFB  pointer.
 *
 * @return      The active LTC count.
 */
LwU32
fbGetActiveLTCCount_GA102(void)
{
    //
    // Starting with GM20X, there may not be a 1:1 correspondence of LTCs and
    // FBPs. Thus, use the new L2 (LTC) enumeration register to determine
    // the number of LTCs (as opposed to the FBP enumeration register).
    //
    LwU32 val;
    LwU32 numActiveLTCs;

    val           = REG_RD32(FECS, LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_L2);
    numActiveLTCs = DRF_VAL(_PPRIV, _MASTER_RING_ENUMERATE_RESULTS_L2, _COUNT, val);
    PMU_HALT_COND(numActiveLTCs > 0);
    return numActiveLTCs;
}

/*!
 * @brief Checks whether GDDR6/GDDR6X thermal sensor is enabled.
 *
 * @param[out] pBEnabled   Pointer to boolean indicating whether sensor is enabled.
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT   If NULL pointer is passed
 * @return  FLCN_OK                     Otherwise
 */
FLCN_STATUS
fbSensorGDDR6XCombinedIsEnabled_GA10X
(
    LwBool  *pBEnabled
)
{
    FLCN_STATUS status  = FLCN_OK;
    LwU32 temp;

    // Sanity checks.
    if (pBEnabled == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto fbSensorGDDR6XCombinedIsEnabled_GA10X_exit;
    }

    temp = REG_RD32(BAR0, LW_PMGR_SELWRE_WR_SCRATCH_2_FB_TEMPERATURE_TRACKING);
    if (FLD_TEST_DRF(_PMGR, _SELWRE_WR_SCRATCH_2_FB_TEMPERATURE_TRACKING,
            _VAL, _INIT, temp))
    {
        //
        // Read security scratch first.
        // If the value was not set then double check the FBFLCN scratch.
        //
        temp = REG_RD32(BAR0, LW_PFBFALCON_MEMORY_TEMPERATURE);
    }

    *pBEnabled = FLD_TEST_DRF(_PFBFALCON, _MEMORY_TEMPERATURE, _SENSOR, _ENABLED, temp);

fbSensorGDDR6XCombinedIsEnabled_GA10X_exit:
    return status;
}

/*!
 * @brief Gets the GDDR6/GDDR6X combined memory temperature.
 *
 * @param[in]   provIdx Provider index
 * @param[out]  pLwTemp Current temperature
 *
 * @return  FLCN_OK
 *      Temperature successfully obtained
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *      If NULL pointer is passed as input
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Un-supported provider index
 * @return  FLCN_ERR_ILWALID_STATE
 *      Invalid temperature data either due to sensor being disabled or
 *      it providing invalid reading
 */
FLCN_STATUS
fbGDDR6XCombinedTempGet_GA10X
(
    LwU8    provIdx,
    LwTemp *pLwTemp
)
{
    FLCN_STATUS status  = FLCN_OK;
    LwU8        loopCnt = LW_PFBFALCON_MEMORY_TEMPERATURE_RETRY_CNT + 1;
    LwU8        i;
    LwU32       val;

    // Sanity checks.
    if (pLwTemp == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto fbGDDR6XCombinedTempGet_GA10X_exit;
    }

    if (provIdx != LW2080_CTRL_THERMAL_THERM_DEVICE_GDDR6_X_COMBINED_PROV_MAX)
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        PMU_BREAKPOINT();
        goto fbGDDR6XCombinedTempGet_GA10X_exit;
    }

    for (i = 0; i < loopCnt; i++)
    {
        LwBool  bRetry = LW_FALSE;

        // Read the combined MAX temperature from HW.
        val = REG_RD32(BAR0, LW_PMGR_SELWRE_WR_SCRATCH_2_FB_TEMPERATURE_TRACKING);

        if (FLD_TEST_DRF(_PFBFALCON, _MEMORY_TEMPERATURE, _SENSOR, _DISABLED, val))
        {
            bRetry = LW_TRUE;
        }
        else if (FLD_TEST_DRF(_PFBFALCON, _MEMORY_TEMPERATURE, _VALUE, _VALID, val))
        {
            *pLwTemp = RM_PMU_LW_TEMP_EXTRACT_FROM_REG_VAL(_PFBFALCON,
                        _MEMORY_TEMPERATURE, _FIXED_POINT, val);
            break;
        }
        else
        {
            bRetry = LW_TRUE;
        }

        if (bRetry)
        {
            //
            // Apply temperature read delay if HW returns sensor disabled or
            // invalid reading.
            //
            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_MEMORY_TEMPERATURE_READ_DELAY) &&
                !fbIsTempReadAvailable_HAL())
            {
                *pLwTemp = RM_PMU_LW_TEMP_0_C;
                break;
            }
            else
            {
                if (i == (loopCnt - 1))
                {
                    //
                    // TODO: This is a WAR for Temp Register reset to 0 after fbflcn resets.
                    // Needs to fix this and revert. Detais at lwbug #3001909.
                    //
                    *pLwTemp = RM_PMU_LW_TEMP_0_C;
                    break;
                }

                OS_PTIMER_SPIN_WAIT_US(
                    LW_PFBFALCON_MEMORY_TEMPERATURE_READ_DELAY_INTERMEDIATE_US);
            }
        }
    }

fbGDDR6XCombinedTempGet_GA10X_exit:
    return status;
}

/*!
 * @brief Public interface to callwlate the number of FB channels.
 *
 * @return      The FB channel count.
 */
LwU32
fbGetNumChannels_GA10X(void)
{
    LwU32 numSubpartitions = 0;
    LwU32 activeFbpMask    = fbGetActiveFBPMask_HAL();

    LwU32 val = fuseRead(LW_FUSE_STATUS_OPT_HALF_FBPA);
    LwU32 idx;

    FOR_EACH_INDEX_IN_MASK(32, idx, activeFbpMask)
    {
        if (FLD_IDX_TEST_DRF(_FUSE, _STATUS_OPT_HALF_FBPA, _IDX, idx, _ENABLE, val))
        {
            // Single subpartition
            numSubpartitions++;
        }
        else
        {
            // Two subpartitions
            numSubpartitions = numSubpartitions + 2;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // Two channels per subpartition
    return (numSubpartitions * 2);
}

