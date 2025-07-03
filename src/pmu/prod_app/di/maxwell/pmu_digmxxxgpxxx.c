/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_digmxxxgpxxx.c
 * @brief  HAL-interface for the MAXWELL and PASCAL Deepidle Engine
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_trim.h"
#include "dev_pwr_pri.h"
#include "pmu/pmumailboxid.h"
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objms.h"
#include "pmu_objdi.h"

#include "config/g_di_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* --------------------------- Prototypes ------------------------------------*/
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
diSeqPllListInitDrampll_GM10X
(
    DI_PLL **ppPllList,
    LwU8    *pllCount
)
{
    static DI_PLL diDrampllList[] = 
    {
        {LW_PTRIM_FBPA_BCAST_DRAMPLL_CFG, 0x0, DI_PLL_TYPE_DRAMPLL},
    };

    *pllCount  = (LwU8) LW_ARRAY_ELEMENTS(diDrampllList);
    *ppPllList = diDrampllList;
}

/*!
 * @Brief Initialization of PLL cache based on chip POR
 *
 * Following Fields are initialized :
 *      { Register Address of PLL, Cached Value, PLL Type }
 *
 * @return Base pointer to the List
 */
void
diSeqPllListInitRefmpll_GM10X
(
    DI_PLL **ppPllList,
    LwU8    *pllCount
)
{
    static DI_PLL diRefmpllList[] = 
    {
        {LW_PTRIM_FBPA_BCAST_REFMPLL_CFG, 0x0, DI_PLL_TYPE_REFMPLL},
    };

    *pllCount  = (LwU8) LW_ARRAY_ELEMENTS(diRefmpllList);
    *ppPllList = diRefmpllList;
}

/*!
 * @Brief Exelwtes Memory PowerDown Sequence
 *
 * Steps :
 *    1. Cache the PLL configuration state
 *    2. Disable Sync mode (only for REFMPLL)
 *    3. Disable Lock detection
 *    4. Disable the PLL
 *    5. Assert DRAMPLL Clamp (only for DRAMPLL)
 *    6. Power off PLL
 *
 * @param[in]   pPll      Cache for PLL that needs to be powered off
 * @param[in]   stepMask  Mask for steps needed to be exelwted
 *                        0 => Skip Step, 1 => Execute Step
 */
void
diSeqPllPowerdownMemory_GM10X
(
    DI_PLL *pPll,
    LwU16   stepMask
)
{
    LwU32 regVal;

    regVal = REG_RD32(FECS, pPll->regAddr);

    // STEP 1. Cache the PLL configuration state
    if (DI_SEQ_PLL_STEPMASK_IS_SET(_MEMORY, _POWERDOWN, _SAVE_CACHE, stepMask))
    {
        pPll->cacheVal = regVal;
    }

    if (FLD_TEST_DRF(_PTRIM_FBPA, _BCAST_REFMPLL_CFG, _IDDQ, _POWER_ON, regVal))
    {
        if (FLD_TEST_DRF(_PTRIM_FBPA, _BCAST_REFMPLL_CFG, _SYNC_MODE, _ENABLE, regVal))
        {
            // STEP 2. Disable Sync mode
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_MEMORY, _POWERDOWN, _SYNC_MODE_DISABLE, stepMask))
            {
                regVal = FLD_SET_DRF(_PTRIM_FBPA, _BCAST_REFMPLL_CFG,
                                     _SYNC_MODE, _DISABLE, regVal);
                REG_WR32(FECS, pPll->regAddr, regVal);
            }
        }

        if (FLD_TEST_DRF(_PTRIM_FBPA, _BCAST_REFMPLL_CFG, _ENABLE, _YES, pPll->cacheVal))
        {
            // STEP 3. Disable Lock detection
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_MEMORY, _POWERDOWN, _LCKDET_OFF, stepMask))
            {
                regVal = REG_RD32(FECS, pPll->regAddr);
                regVal = FLD_SET_DRF(_PTRIM_FBPA, _BCAST_REFMPLL_CFG,
                                        _ENB_LCKDET, _POWER_OFF, regVal);
                REG_WR32(FECS, pPll->regAddr, regVal);
            }

            // STEP 4. Disable the PLL
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_MEMORY, _POWERDOWN, _DISABLE, stepMask))
            {
                regVal = REG_RD32(FECS, pPll->regAddr);
                regVal = FLD_SET_DRF(_PTRIM_FBPA, _BCAST_REFMPLL_CFG,
                                        _ENABLE, _NO, regVal);
                REG_WR32(FECS, pPll->regAddr, regVal);
            }
        }

        if (DI_IS_PLL_IDDQ_ENABLED())
        {
            // STEP 5. Assert DRAMPLL Clamp
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_MEMORY, _POWERDOWN, _DRAMPLL_CLAMP_ASSERT, stepMask))
            {
                diSeqDramPllClampSet_HAL(&Di, LW_TRUE);
            }

            // STEP 6. Power off PLL
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_MEMORY, _POWERDOWN, _IDDQ_OFF, stepMask))
            {
                regVal = REG_RD32(FECS, pPll->regAddr);
                regVal = FLD_SET_DRF(_PTRIM_FBPA, _BCAST_REFMPLL_CFG,
                                        _IDDQ, _POWER_OFF, regVal);
                REG_WR32(FECS, pPll->regAddr, regVal);
            }
        }
    }
}

/*!
 * @Brief Exelwtes Memory PowerUp Sequence
 *
 * Steps :
 *    1. Power on PLL
 *    2. Wait for PLL ready
 *    3. De-Assert DRAMPLL Clamp (only for DRAMPLL)
 *    4. Enable PLL
 *    5. Enable Lock detection
 *    6. Wait for PLL Lock
 *    7. Enable Sync mode (only for REFMPLL)
 *
 * @param[in]   pPll      Cache for PLL that needs to be powered off
 * @param[in]   stepMask  Mask for steps needed to be exelwted
 *                        0 => Skip Step, 1 => Execute Step
 */
void
diSeqPllPowerupMemory_GM10X
(
    DI_PLL *pPll,
    LwU16   stepMask
)
{
    LwU32 regVal;

    if (FLD_TEST_DRF(_PTRIM_FBPA, _BCAST_REFMPLL_CFG, _IDDQ, _POWER_ON, pPll->cacheVal))
    {
        if (DI_IS_PLL_IDDQ_ENABLED())
        {
            // STEP 1. Power on PLL
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_MEMORY, _POWERUP, _IDDQ_ON, stepMask))
            {
                    regVal = REG_RD32(FECS, pPll->regAddr);
                    regVal = FLD_SET_DRF(_PTRIM_FBPA, _BCAST_REFMPLL_CFG,
                                         _IDDQ, _POWER_ON, regVal);
                    REG_WR32(FECS, pPll->regAddr, regVal);
            }

            // STEP 2. Wait for PLL ready
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_MEMORY, _POWERUP, _WAIT_PLL_READY, stepMask))
            {
                if (pPll->pllType == DI_PLL_TYPE_REFMPLL)
                {
                    // Assert if PLL is not ready for a given timeout.
                    if (!PMU_REG32_POLL_US(USE_FECS(LW_PTRIM_SYS_MEM_PLL_STATUS),
                                            DRF_SHIFTMASK(LW_PTRIM_SYS_MEM_PLL_STATUS_REFMPLL_PLL_READY),
                                            DRF_DEF(_PTRIM, _SYS_MEM_PLL_STATUS_REFMPLL, _PLL_READY, _TRUE),
                                            (LwU32)DI_SEQ_PLL_LOCK_DETECTION_DELAY_DEFAULT_MAX_US,
                                            PMU_REG_POLL_MODE_BAR0_EQ))
                    {
                        PMU_HALT();
                    }
                }
                else if (pPll->pllType == DI_PLL_TYPE_DRAMPLL)
                {
                    // Assert if PLL is not ready for a given timeout.
                    if (!PMU_REG32_POLL_US(USE_FECS(LW_PTRIM_SYS_MEM_PLL_STATUS),
                                            DRF_SHIFTMASK(LW_PTRIM_SYS_MEM_PLL_STATUS_DRAMPLL_PLL_READY),
                                            DRF_DEF(_PTRIM, _SYS_MEM_PLL_STATUS_DRAMPLL, _PLL_READY, _TRUE),
                                            (LwU32)DI_SEQ_PLL_LOCK_DETECTION_DELAY_DEFAULT_MAX_US,
                                            PMU_REG_POLL_MODE_BAR0_EQ))
                    {
                        PMU_HALT();
                    }
                }
            }

            // STEP 3. De-Assert DRAMPLL Clamp
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_MEMORY, _POWERUP, _DRAMPLL_CLAMP_DEASSERT, stepMask))
            {
                diSeqDramPllClampSet_HAL(&Di, LW_FALSE);
            }
        }

        if (FLD_TEST_DRF(_PTRIM_FBPA, _BCAST_REFMPLL_CFG, _ENABLE, _YES, pPll->cacheVal))
        {
            // STEP 4. Enable PLL
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_MEMORY, _POWERUP, _ENABLE, stepMask))
            {
                regVal = REG_RD32(FECS, pPll->regAddr);
                regVal = FLD_SET_DRF(_PTRIM_FBPA, _BCAST_REFMPLL_CFG,
                                        _ENABLE, _YES, regVal);
                REG_WR32(FECS, pPll->regAddr, regVal);
            }

            // STEP 5. Enable Lock detection
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_MEMORY, _POWERUP, _LCKDET_ON, stepMask))
            {
                regVal = REG_RD32(FECS, pPll->regAddr);
                regVal = FLD_SET_DRF(_PTRIM_FBPA, _BCAST_REFMPLL_CFG,
                                        _ENB_LCKDET, _POWER_ON, regVal);
                REG_WR32(FECS, pPll->regAddr, regVal);
            }

            // STEP 6. Wait for PLL Lock
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_MEMORY, _POWERUP, _WAIT_PLL_LOCK, stepMask))
            {
                if (pPll->pllType == DI_PLL_TYPE_REFMPLL)
                {
                    // Assert if Lock cannot be acquired for a given timeout.
                    if (!PMU_REG32_POLL_US(USE_FECS(LW_PTRIM_SYS_MEM_PLL_STATUS),
                                            DRF_SHIFTMASK(LW_PTRIM_SYS_MEM_PLL_STATUS_REFMPLL_PLL_LOCK),
                                            DRF_DEF(_PTRIM, _SYS_MEM_PLL_STATUS_REFMPLL, _PLL_LOCK, _TRUE),
                                            (LwU32)DI_SEQ_PLL_LOCK_DETECTION_DELAY_DEFAULT_MAX_US,
                                            PMU_REG_POLL_MODE_BAR0_EQ))
                    {
                        PMU_HALT();
                    }
                }
                else if (pPll->pllType == DI_PLL_TYPE_DRAMPLL)
                {
                    // Assert if Lock cannot be acquired for a given timeout.
                    if (!PMU_REG32_POLL_US(USE_FECS(LW_PTRIM_SYS_MEM_PLL_STATUS),
                                            DRF_SHIFTMASK(LW_PTRIM_SYS_MEM_PLL_STATUS_DRAMPLL_PLL_LOCK),
                                            DRF_DEF(_PTRIM, _SYS_MEM_PLL_STATUS_DRAMPLL, _PLL_LOCK, _TRUE),
                                            (LwU32)DI_SEQ_PLL_LOCK_DETECTION_DELAY_DEFAULT_MAX_US,
                                            PMU_REG_POLL_MODE_BAR0_EQ))
                    {
                        PMU_HALT();
                    }
                }
            }
        }

        if (FLD_TEST_DRF(_PTRIM_FBPA, _BCAST_REFMPLL_CFG, _SYNC_MODE, _ENABLE, pPll->cacheVal))
        {
            // STEP 7. Enable Sync mode
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_MEMORY, _POWERUP, _SYNC_MODE_ENABLE, stepMask))
            {
                regVal = REG_RD32(FECS, pPll->regAddr);
                regVal = FLD_SET_DRF(_PTRIM_FBPA, _BCAST_REFMPLL_CFG, _SYNC_MODE,
                                     _ENABLE, regVal);
                REG_WR32(FECS, pPll->regAddr, regVal);
            }
        }
    }
    else if (DI_IS_PLL_IDDQ_ENABLED())
    {
        //
        // This case oclwrs for Full GC5 where RM puts DRAMPLL in IDDQ Power off mode.
        // Now in Full GC5, we will be shutting down the power rail on which DRAMPLL
        // was running. So when GC5 exit starts, DRAMPLL will be in reset state which
        // is 'IDDQ Power on'. As DRAMPLL was put in IDDQ Power off by RM before we
        // entered GC5, we should put the PLL in the same state before we exit GC5.
        //

        // Power off PLL
        if (DI_SEQ_PLL_STEPMASK_IS_SET(_MEMORY, _POWERUP, _IDDQ_OFF, stepMask))
        {
            regVal = REG_RD32(FECS, pPll->regAddr);
            regVal = FLD_SET_DRF(_PTRIM_FBPA, _BCAST_REFMPLL_CFG,
                                 _IDDQ, _POWER_OFF, regVal);
            REG_WR32(FECS, pPll->regAddr, regVal);
        }
    }
}
