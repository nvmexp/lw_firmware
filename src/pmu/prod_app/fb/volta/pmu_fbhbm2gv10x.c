/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_fbhbm2gv10x.c
 * @brief  FB Hal Functions for HBM on GV100+
 *
 * FB Hal Functions implement FB related functionalities for GV100
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "main.h"
#include "dev_fuse.h"
#include "dev_top.h"
#include "dev_fbpa.h"
#include "dev_fbpa_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objfb.h"
#include "pmu_objfuse.h"
#include "config/g_fb_private.h"
#include "lib_mutex.h"
#include "lw_mutex.h"

/* ------------------------- Static function Prototypes ---------------------*/
/* ------------------------- Macros -----------------------------------------*/
#define LW_PFB_FBPA_STRIDE        (DRF_BASE(LW_PFB_FBPA_1) - DRF_BASE(LW_PFB_FBPA_0))

/*!
 * HBM PMGR mutex timeout value, specified in ns. Current value is 50ms.
 */
#define PMGR_HBM_IEEE1500_MUTEX_ACQUIRE_TIMEOUT_NS  50000000

/*!
 * Retry count for re-attempting the temperature read in case HW returns
 * invalid value.
 */
#define LW_PFB_MEMORY_TEMPERATURE_RETRY_CNT                     3

/*!
 * Intermediate read delay between subsequent memory temperature reads.
 */
#define LW_PFB_MEMORY_TEMPERATURE_READ_DELAY_INTERMEDIATE_US    5

/*!
 * @brief Gets the temperature in celsius of a given HBM site.
 *
 * This function gets the temperature in 24.8 fixed point notation in [C]
 * of a given HBM site using IEEE1500 interface.
 * All FBPAs within a HBM site are equivalent in the sense that all of them
 * return the same max temperature although their individual physical
 * temperatures may be different. We choose to use the non floorswept FBPA
 * with the lowest index to get the temperature info.
 *
 * @param[in]  fbpaIndex     Index of first non floorswept FBPA within a HBM site
 * @param[in]  provIdx       Provider index. Ignored on GV100
 * @param[out] pLwTemp       Current temperature
 * 
 * @return FLCN_OK
 *      Temperature successfully obtained.
 * @return FLCN_ERR_ILWALID_STATE
 *      Error to acquire mutex or an invalid temperature data.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
FLCN_STATUS
fbHBMSiteTempGet_GV10X
(
    LwU8    fbpaIndex,
    LwU8    provIdx,
    LwTemp *pLwTemp
)
{
    FLCN_STATUS status  = FLCN_OK;
    LwU8        loopCnt = LW_PFB_MEMORY_TEMPERATURE_RETRY_CNT + 1;
    LwU8        i;
    LwU8        tokenId;
    LwU32       val;

    if (fbpaIndex == FB_FBPA_INDEX_ILWALID)
    {
        *pLwTemp = RM_PMU_LW_TEMP_0_C;
        goto fbHBMSiteTempGet_GV10X_exit;
    }

    //
    // Samsung HBM2 units return invalid temperature values during a
    // suspend/resume cycle if it is read 'too quickly'. We WAR it by
    // waiting for a fixed timeout before reading temperature while
    // returning a bogus temperature value (0 deg. C) until then.
    // See bug 1981429.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_MEMORY_TEMPERATURE_READ_DELAY))
    {
        if (FLD_TEST_DRF(_RM_PMU, _CMD_LINE_ARGS_FLAGS, _BOOT_MODE, _RESUME_S3S4, PmuInitArgs.flags)
                && !fbIsTempReadAvailable_HAL())
        {
            *pLwTemp = RM_PMU_LW_TEMP_0_C;
            goto fbHBMSiteTempGet_GV10X_exit;
        }
    }

    //
    // See https://wiki.lwpu.com/gpuhwvolta/index.php/GV100_HBM_Bringup#Accessing_IEEE1500 for
    // complete sequence.
    // We use a combination of multicast and unicast write here.
    //
    status = mutexAcquire(LW_MUTEX_ID_HBM_IEEE1500, PMGR_HBM_IEEE1500_MUTEX_ACQUIRE_TIMEOUT_NS, &tokenId);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto fbHBMSiteTempGet_GV10X_exit;
    }

    REG_WR32(BAR0, LW_PFB_FBPA_0_FBIO_HBM_TEST_MACRO + (fbpaIndex * LW_PFB_FBPA_STRIDE),
                   DRF_NUM(_PFB, _FBPA_FBIO_HBM_TEST_MACRO, _READ_I1500_REG, 0x0) |
                   DRF_NUM(_PFB, _FBPA_FBIO_HBM_TEST_MACRO, _DRAM_RESET, 0x1) |
                   DRF_NUM(_PFB, _FBPA_FBIO_HBM_TEST_MACRO, _DRAM_WRST_RESET, 0x1));

    for (i = 0; i < loopCnt; i++)
    {
        // Program the _TEMPERATURE instruction in the WIR.
        REG_WR32(BAR0, LW_PFB_FBPA_0_FBIO_HBM_TEST_I1500_INSTR + (fbpaIndex * LW_PFB_FBPA_STRIDE),
                    DRF_DEF(_PFB, _FBPA_FBIO_HBM_TEST_I1500_INSTR, _HBM_WIR_INSTRUCTION, _TEMPERATURE));

        // 8 bits to be read.
        REG_WR32(BAR0, LW_PFB_FBPA_0_FBIO_HBM_TEST_I1500_MODE + (fbpaIndex * LW_PFB_FBPA_STRIDE),
                    DRF_NUM(_PFB, _FBPA_FBIO_HBM_TEST_I1500_MODE, _WDR_LENGTH, 0x8));

        // Reads should be unicast.
        val = REG_RD32(BAR0, LW_PFB_FBPA_0_FBIO_HBM_TEST_I1500_DATA + (fbpaIndex * LW_PFB_FBPA_STRIDE));

        if (FLD_TEST_DRF(_PFB, _FBPA_FBIO_HBM_TEST_I1500_DATA, _TEMPERATURE_STATUS, _VALID, val))
        {
            *pLwTemp = RM_PMU_CELSIUS_TO_LW_TEMP(DRF_VAL(_PFB, 
                            _FBPA_FBIO_HBM_TEST_I1500_DATA, _TEMPERATURE_VAL, val));
            goto fbHBMSiteTempGet_GV10X_release;
        }
        else
        {
            if (i == (loopCnt - 1))
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto fbHBMSiteTempGet_GV10X_release;
            }

            OS_PTIMER_SPIN_WAIT_US(
                LW_PFB_MEMORY_TEMPERATURE_READ_DELAY_INTERMEDIATE_US);
        }
    }

fbHBMSiteTempGet_GV10X_release:
    // Release the mutex.
    mutexRelease(LW_MUTEX_ID_HBM_IEEE1500, tokenId);

fbHBMSiteTempGet_GV10X_exit:
    return status;
}

/*!
 * @brief Gets the total number of HBM sites present on the board.
 *
 * @returns LwU8  Total number of HBM sites.
 */
LwU8
fbHBMSiteCountGet_GV10X(void)
{
    return LW_PFB_HBM_SITE_COUNT;
}
