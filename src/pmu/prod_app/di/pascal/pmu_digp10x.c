/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_digp10x.c
 * @brief  HAL-interface for the GP10x DI Engine
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_gc6_island.h"
#include "dev_pmgr.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "dev_trim.h"
#include "dev_trim_addendum.h"
#include "dev_therm.h"
#include "fermi/generic_dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "lwosreg.h"
#include "lwuproc.h"
#include "lwos_dma.h"
#include "pmu_objpmu.h"
#include "pmu_objdi.h"
#include "pmu_objtimer.h"
#include "therm/objtherm.h"

#include "config/g_di_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @Brief Step masks for each level of Power up / down sequence for all PLL Types available
 *
 * @Description : Each PLLs core sequence is divided into a number of levels.
 *                Levels are collection of steps in a particular PLL sequence.
 *                Levels are pre-decided to ensure high level of parallelization in DI PLL Sequences.
 *                This optimizes the time needed to power off / on list of PLLs with different PLL Types.
 *
 * @Example : PLL Type XYZ : Step 1 (Write 1 to Register) --> Step 2 (Wait for 5 us) --> Step 3 (Write 2 to register)
 *            PLL Type ABC : Step 1 (Write 1 to Register) --> Step 2 (Write 2 to register) --> Step 3 (Wait for 6 us)
 *                 Each PLL Type will have 2 level with respective Step Masks
 *                 XYZ_LEVEL_0_STEPMASK = 0b'0000 0001
 *                 XYZ_LEVEL_1_STEPMASK = 0b'0000 0110
 *                 ABC_LEVEL_0_STEPMASK = 0b'0000 0011
 *                 ABC_LEVEL_1_STEPMASK = 0b'0000 0100
 *             Power Down Sequence Exelwtion for list of PLLs which has one PLL Of each type
 *             1. Execute Level 0 for both XYZ (Step 1 only) and ABC(Step 1 and 2)
 *             2. Execute Level 1 for both XYZ (Step 2 and 3) and ABC(Step 3 only)
 *                Following Such a power down sequence will parallelize the 5 us and 6 us waits of both PLLs
 *                thus optimizing the DI latency.
 */
#define DI_SEQ_PLL_POWERDOWN_LEGACY_CORE_STEPMASK_LEVEL_0_GP10X                         \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_CORE, _POWERDOWN, _IDDQ_OFF_LCKDET_DISABLE) \
        )

#define DI_SEQ_PLL_POWERUP_LEGACY_CORE_STEPMASK_LEVEL_0_GP10X                           \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_CORE, _POWERUP, _IDDQ_ON)                   \
        )

#define DI_SEQ_PLL_POWERUP_LEGACY_CORE_STEPMASK_LEVEL_1_GP10X                           \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_CORE, _POWERUP, _WAIT_PLL_READY)           |\
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_CORE, _POWERUP, _LCKDET_ENABLE)             \
        )

#define DI_SEQ_PLL_POWERUP_LEGACY_CORE_STEPMASK_LEVEL_2_GP10X                           \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_CORE, _POWERUP, _WAIT_PLL_LOCK)             \
        )

#define DI_SEQ_PLL_POWERDOWN_HPLL_STEPMASK_LEVEL_0_GP10X                                \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_HPLL, _POWERDOWN, _IDDQ_OFF)                       \
        )

#define DI_SEQ_PLL_POWERUP_HPLL_STEPMASK_LEVEL_0_GP10X                                  \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_HPLL, _POWERUP, _IDDQ_ON)                          \
        )

#define DI_SEQ_PLL_POWERUP_HPLL_STEPMASK_LEVEL_1_GP10X                                  \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_HPLL, _POWERUP, _WAIT_CLK_GOOD_TRUE)               \
        )

#define DI_SEQ_PLL_POWERDOWN_PLL16G_STEPMASK_LEVEL_0_GP10X                              \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_PLL16G, _POWERDOWN, _LCKDET_OFF)                   \
        )

#define DI_SEQ_PLL_POWERDOWN_PLL16G_STEPMASK_LEVEL_1_GP10X                              \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_PLL16G, _POWERDOWN, _BYPASSPLL)                   |\
            DI_SEQ_PLL_STEPMASK_GET(_PLL16G, _POWERDOWN, _DISABLE)                     |\
            DI_SEQ_PLL_STEPMASK_GET(_PLL16G, _POWERDOWN, _DEACTIVATE_SSD)              |\
            DI_SEQ_PLL_STEPMASK_GET(_PLL16G, _POWERDOWN, _IDDQ_OFF)                     \
        )

#define DI_SEQ_PLL_POWERUP_PLL16G_STEPMASK_LEVEL_0_GP10X                                \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_PLL16G, _POWERUP, _IDDQ_ON)                        \
        )

#define DI_SEQ_PLL_POWERUP_PLL16G_STEPMASK_LEVEL_1_GP10X                                \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_PLL16G, _POWERUP, _WAIT_PLL_READY)                |\
            DI_SEQ_PLL_STEPMASK_GET(_PLL16G, _POWERUP, _LCKDET_ON)                     |\
            DI_SEQ_PLL_STEPMASK_GET(_PLL16G, _POWERUP, _ENABLE)                         \
        )

#define DI_SEQ_PLL_POWERUP_PLL16G_STEPMASK_LEVEL_2_GP10X                                \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_PLL16G, _POWERUP, _WAIT_PLL_LOCK)                 |\
            DI_SEQ_PLL_STEPMASK_GET(_PLL16G, _POWERUP, _ACTIVATE_SSD)                  |\
            DI_SEQ_PLL_STEPMASK_GET(_PLL16G, _POWERUP, _BYPASSPLL)                      \
        )
#define DI_SEQ_PLL_POWERDOWN_NAFLL_STEPMASK_LEVEL_0_GP10X                               \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_NAFLL, _POWERDOWN, _IDDQ_OFF_DISABLE)              \
        )

#define DI_SEQ_PLL_POWERUP_NAFLL_STEPMASK_LEVEL_0_GP10X                                 \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_NAFLL, _POWERUP, _IDDQ_ON)                         \
        )

#define DI_SEQ_PLL_POWERUP_NAFLL_STEPMASK_LEVEL_1_GP10X                                 \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_NAFLL, _POWERUP, _WAIT_FLL_READY)                 |\
            DI_SEQ_PLL_STEPMASK_GET(_NAFLL, _POWERUP, _ENABLE)                          \
        )

#define DI_SEQ_PLL_POWERUP_NAFLL_STEPMASK_LEVEL_2_GP10X                                 \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_NAFLL, _POWERUP, _WAIT_FLL_LOCK)                   \
        )

/* --------------------------- Prototypes ------------------------------------*/
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Change the Therm VID settings when utilsclk is switched from
 * 108MHz to Xtal and vice versa.
 *
 * @param[in,out]   pCache  Holds cached register values necessary for
 *                          entering into and exiting out of deepidle.
 * @param[in]       bExit   When true; restore vid register settings from cache.
 *
 *   Else; signifies entry. Overwrite the therm vid settings
 *                          with SCI vid settings.
 *
 * @return  LW_TRUE if secondary idle
 *          LW_FALSE is secondary busy
 */
void
diSeqVidSwitch_GP10X
(
    DI_SEQ_CACHE *pCache,
    LwBool        bExit
)
{
    LwU32 regVal      = 0;
    LwU32 regValOld   = 0;
    LwU32 railIdx     = 0;
    LwU32 vidTblIndex = 0;

    if (!bExit)
    {
            // Disable Voltage Update by writing Steady State Voltage to Sleep LUTs
        if (!PG_IS_SF_ENABLED(Di, DI, VOLTAGE_UPDATE) ||
            !Di.bSleepVoltValid)
        {
            if (PG_VOLT_IS_RAIL_SUPPORTED(PG_VOLT_RAIL_IDX_LOGIC))
            {
                regVal = REG_RD32(FECS, LW_PGC6_SCI_VID_CFG1(
                            PG_VOLT_GET_VOLT_RAIL_IDX(PG_VOLT_RAIL_IDX_LOGIC)));
                REG_WR32(FECS, LW_PGC6_SCI_PWR_SEQ_VID_TABLE(LW_PGC6_SCI_VID_TABLE_IDX_GC5_LWVDDL_SLEEP),
                        DRF_VAL(_PGC6, _SCI_VID_CFG1, _HI, regVal));
            }

            if (PG_VOLT_IS_RAIL_SUPPORTED(PG_VOLT_RAIL_IDX_SRAM))
            {
                regVal = REG_RD32(FECS, LW_PGC6_SCI_VID_CFG1(
                            PG_VOLT_GET_VOLT_RAIL_IDX(PG_VOLT_RAIL_IDX_SRAM)));
                REG_WR32(FECS, LW_PGC6_SCI_PWR_SEQ_VID_TABLE(LW_PGC6_SCI_VID_TABLE_IDX_GC5_LWVDDS_SLEEP),
                        DRF_VAL(_PGC6, _SCI_VID_CFG1, _HI, regVal));
            }
        }
    }

    // Cancel all pending triggers before copying settings to THERM_VID
    regVal = regValOld = REG_RD32(CSB, LW_CPWR_THERM_VID_PWM_TRIGGER_MASK);

    regVal = FLD_SET_DRF(_CPWR_THERM, _VID_PWM_TRIGGER_MASK,
                         _0_SETTING_NEW, _DONE, regVal);
    regVal = FLD_SET_DRF(_CPWR_THERM, _VID_PWM_TRIGGER_MASK,
                         _1_SETTING_NEW, _DONE, regVal);

    if (regValOld != regVal)
    {
        REG_WR32(CSB, LW_CPWR_THERM_VID_PWM_TRIGGER_MASK, regVal);
    }

    // Copy appropriate settings to THERM_VID
    for (railIdx = 0; railIdx < PG_VOLT_RAIL_IDX_MAX; railIdx++)
    {
        if (PG_VOLT_IS_RAIL_SUPPORTED(railIdx))
        {
            if (bExit)
            {
                REG_WR32(CSB, LW_CPWR_THERM_VID0_PWM(railIdx),
                              PG_VOLT_GET_THERM_VID0_CACHE(railIdx));
                regVal = PG_VOLT_GET_THERM_VID1_CACHE(railIdx);

                //
                // Setting new trigger while previous one is pending may cause corruption.
                // Cancel the previous pending trigger.
                // For Pascal, new trigger is applied through TRIGGER_MASK mechanism.
                //
                regVal = FLD_SET_DRF(_CPWR_THERM, _VID1_PWM,
                                     _SETTING_NEW, _DONE, regVal);
                REG_WR32(CSB, LW_CPWR_THERM_VID1_PWM(railIdx), regVal);

                //
                // We would be fine not polling here since the trigger completion
                // may hide behind the latency to reach the next step which does
                // ALT_CTL_SEL and eventually relinquishes gpio control from SCI
                // back to THERM.
                //
            }
            else
            {
                regVal = REG_RD32(FECS, LW_PGC6_SCI_VID_CFG0(railIdx));
                REG_WR32(CSB, LW_CPWR_THERM_VID0_PWM(railIdx), regVal);

                regVal = REG_RD32(FECS, LW_PGC6_SCI_VID_CFG1(railIdx));

                //
                // Setting new trigger while previous one is pending may cause corruption.
                // Cancel the previous pending trigger.
                // For Pascal, new trigger is applied through TRIGGER_MASK mechanism.
                //
                regVal = FLD_SET_DRF(_CPWR_THERM, _VID1_PWM,
                                     _SETTING_NEW, _DONE, regVal);
                REG_WR32(CSB, LW_CPWR_THERM_VID1_PWM(railIdx), regVal);

                //
                // Volt only writes duty cycle to SCI VID CFG register.
                // DI will copy this setting to LPWR LUT for Steady State
                // so that voltage can be restored to steady state level after GC5 Exit.
                //
                if (railIdx == PG_VOLT_RAIL_IDX_LOGIC)
                {
                    vidTblIndex = LW_PGC6_SCI_VID_TABLE_IDX_GC5_LWVDDL_STEADY_STATE;
                }
                else if (railIdx == PG_VOLT_RAIL_IDX_SRAM)
                {
                    vidTblIndex = LW_PGC6_SCI_VID_TABLE_IDX_GC5_LWVDDS_STEADY_STATE;
                }
                else
                {
                    PMU_HALT();
                }

                //
                // Since fmodel does not implement the bit; we will eventually keep polling.
                // WAR: add delay for ~800Khz period
                //
                // In case the poll is not needed;
                // the delay below can be used.
                // OS_PTIMER_SPIN_WAIT_NS(1300);
                //

                // LPWR LUTs only need the duty cycle.
                REG_WR32(FECS, LW_PGC6_SCI_PWR_SEQ_VID_TABLE(vidTblIndex),
                         DRF_VAL(_PGC6, _SCI_VID_CFG1, _HI, regVal));
            }
        }
    }

    thermVidPwmTriggerMaskSet_HAL();

    // poll for trigger completion during GC5 entry
    if (!bExit)
    {
        regVal = REG_RD32(CSB, LW_CPWR_THERM_VID_PWM_TRIGGER_MASK);

        if (PG_VOLT_IS_RAIL_SUPPORTED(PG_VOLT_RAIL_IDX_LOGIC))
        {
            while (FLD_TEST_DRF(_CPWR_THERM, _VID_PWM_TRIGGER_MASK, _0_SETTING_NEW, _PENDING, regVal))
            {
                regVal = REG_RD32(CSB, LW_CPWR_THERM_VID_PWM_TRIGGER_MASK);
            }
        }

        if (PG_VOLT_IS_RAIL_SUPPORTED(PG_VOLT_RAIL_IDX_SRAM))
        {
            while (FLD_TEST_DRF(_CPWR_THERM, _VID_PWM_TRIGGER_MASK, _1_SETTING_NEW, _PENDING, regVal))
            {
                regVal = REG_RD32(CSB, LW_CPWR_THERM_VID_PWM_TRIGGER_MASK);
            }
        }
    }
}

/*!
 * @Brief Initialization of NAFLL cache
 *
 * Following Fields are initialized :
 *      { Register Address of NAFLL, Cached Value, PLL Type }
 *
 * @return Base pointer to the List
 */
void
diSeqPllListInitCoreNafll_GP10X
(
    DI_PLL **ppPllList,
    LwU8    *pllCount,
    LwU32   **nafllCtrlReg
)
{
    static DI_PLL diNafllPllList[] =
    {
        {LW_PTRIM_GPC_BCAST_GPCNAFLL_CFG1  , 0x0, DI_PLL_TYPE_NAFLL},
        {LW_PTRIM_SYS_NAFLL_XBARNAFLL_CFG1 , 0x0, DI_PLL_TYPE_NAFLL},
        {LW_PTRIM_SYS_NAFLL_SYSNAFLL_CFG1  , 0x0, DI_PLL_TYPE_NAFLL}
    };

    //
    // Always keep the element indexes similar between diNafllPllList
    // and nafllCtrlregList
    //
    static LwU32 diNafllCtrlList[] =
    {
        LW_PTRIM_GPC_BCAST_GPCNAFLL_CTRL1,
        LW_PTRIM_SYS_NAFLL_XBARNAFLL_CTRL1,
        LW_PTRIM_SYS_NAFLL_SYSNAFLL_CTRL1
    };

    *pllCount       = (LwU8) LW_ARRAY_ELEMENTS(diNafllPllList);
    *ppPllList      = diNafllPllList;
    *nafllCtrlReg   = diNafllCtrlList;
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
diSeqPllListInitSpplls_GP10X
(
    DI_PLL **ppPllList,
    LwU8    *pllCount
)
{
    static DI_PLL diSppllsList[] =
    {
        {LW_PVTRIM_SYS_SPPLL_CFG(0), 0x0, DI_PLL_TYPE_PLL16G},
        {LW_PVTRIM_SYS_SPPLL_CFG(1), 0x0, DI_PLL_TYPE_PLL16G},
    };

    *pllCount  = (LwU8) LW_ARRAY_ELEMENTS(diSppllsList);
    *ppPllList = diSppllsList;
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
diSeqPllListInitXtal4x_GP10X
(
    DI_PLL **ppPllList,
    LwU8    *pllCount
)
{
    static DI_PLL diXtal4xpllList[] =
    {
        {LW_PTRIM_SYS_XTAL4X_CFG, 0x0, DI_PLL_TYPE_HPLL}
    };

    *pllCount  = (LwU8) LW_ARRAY_ELEMENTS(diXtal4xpllList);
    *ppPllList = diXtal4xpllList;
}

/*!
 * @Brief : Powerdown / Powerup ADCs during GC5 Sequence
 *
 * @param[in] bPowerdown : Specifies whther Powerdown or Powerup is required
 */
void
diSeqAdcPowerAction_GP10X
(
    LwBool  bPowerdown
)
{
    LwU32 reg1;
    LwU32 reg2;
    LwU32 reg3;

    reg1 = REG_RD32(FECS, LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CTRL);
    reg2 = REG_RD32(FECS, LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CTRL);
    reg3 = REG_RD32(FECS, LW_PTRIM_SYS_NAFLL_SYSNAFLL_SRAMVDD_ADC_CTRL);

    if (bPowerdown)
    {
        reg1 = FLD_SET_DRF(_PTRIM, _GPC_BCAST_GPCNAFLL_ADC_CTRL, _IDDQ,
                           _POWER_OFF, reg1);
        reg1 = FLD_SET_DRF(_PTRIM, _GPC_BCAST_GPCNAFLL_ADC_CTRL, _ENABLE,
                           _NO, reg1);
        reg2 = FLD_SET_DRF(_PTRIM, _SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CTRL, _IDDQ,
                           _POWER_OFF, reg2);
        reg2 = FLD_SET_DRF(_PTRIM, _SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CTRL, _ENABLE,
                           _NO, reg2);
        reg3 = FLD_SET_DRF(_PTRIM, _SYS_NAFLL_SYSNAFLL_SRAMVDD_ADC_CTRL, _IDDQ,
                           _POWER_OFF, reg3);
        reg3 = FLD_SET_DRF(_PTRIM, _SYS_NAFLL_SYSNAFLL_SRAMVDD_ADC_CTRL, _ENABLE,
                           _NO, reg3);
    }
    else
    {
        reg1 = FLD_SET_DRF(_PTRIM, _GPC_BCAST_GPCNAFLL_ADC_CTRL, _IDDQ,
                           _POWER_ON, reg1);
        reg2 = FLD_SET_DRF(_PTRIM, _SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CTRL, _IDDQ,
                           _POWER_ON, reg2);
        reg3 = FLD_SET_DRF(_PTRIM, _SYS_NAFLL_SYSNAFLL_SRAMVDD_ADC_CTRL, _IDDQ,
                           _POWER_ON, reg3);
    }

    REG_WR32(FECS, LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CTRL, reg1);
    REG_WR32(FECS, LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CTRL, reg2);
    REG_WR32(FECS, LW_PTRIM_SYS_NAFLL_SYSNAFLL_SRAMVDD_ADC_CTRL, reg3);

    if (!bPowerdown)
    {
        OS_PTIMER_SPIN_WAIT_US(DI_SEQ_ADC_BGAP_READY_DELAY_MAX_US);

        reg1 = FLD_SET_DRF(_PTRIM, _GPC_BCAST_GPCNAFLL_ADC_CTRL, _ENABLE,
                           _YES, reg1);
        reg2 = FLD_SET_DRF(_PTRIM, _SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CTRL, _ENABLE,
                           _YES, reg2);
        reg3 = FLD_SET_DRF(_PTRIM, _SYS_NAFLL_SYSNAFLL_SRAMVDD_ADC_CTRL, _ENABLE,
                           _YES, reg3);

        REG_WR32(FECS, LW_PTRIM_GPC_BCAST_GPCNAFLL_ADC_CTRL, reg1);
        REG_WR32(FECS, LW_PTRIM_SYS_NAFLL_SYSNAFLL_LWVDD_ADC_CTRL, reg2);
        REG_WR32(FECS, LW_PTRIM_SYS_NAFLL_SYSNAFLL_SRAMVDD_ADC_CTRL, reg3);
    }
}

/*!
 * @Brief Unified PLL Powerdown sequence for DI
 *
 * @Description : 1. Create list of Stepmasks for all PLLs for all possible pll types.
 *                   (For more info on Step Masks and Levels, Refer to pmu_objdi.h)
 *                2. Execute Level X steps for all Plls in pPllList
 *                3. Execute Level X+1 and so on.
 *
 * @param[in, out] pPllList : PLL cache containing list of PLLs to be powered down
 * @param[in]      pllCount : Number of PLLs in passed PLL list
 */
void
diSeqPllPowerdown_GP10X
(
    DI_PLL *pPllList,
    LwU8    pllCount
)
{
    LwU32   lwrrentLevel;
    LwU32   maxLevelCount;
    LwU32   levelCountNafll;
    LwU32   levelCountLegacyCore;
    LwU32   levelCountVpll;
    LwU32   levelCountSorpll;
    LwU32   levelCountRefmpll;
    LwU32   levelCountDrampll;
    LwU32   levelCountPll16G;
    LwU32   levelCountHpll;
    LwS8    i;

    static const LwU16 diPllPowerDownStepmasksNafll[] =
    {
        DI_SEQ_PLL_POWERDOWN_NAFLL_STEPMASK_LEVEL_0_GP10X
    };

    static const LwU16 diPllPowerDownStepmasksLegacyCore[] =
    {
        DI_SEQ_PLL_POWERDOWN_LEGACY_CORE_STEPMASK_LEVEL_0_GP10X
    };

    static const LwU16 diPllPowerDownStepmasksHpll[] =
    {
        DI_SEQ_PLL_POWERDOWN_HPLL_STEPMASK_LEVEL_0_GP10X
    };

    static const LwU16 diPllPowerDownStepmasksVpll[] =
    {
        DI_SEQ_PLL_POWERDOWN_VPLL_STEPMASK_LEVEL_0_GM10X
    };

    static const LwU16 diPllPowerDownStepmasksSorpll[] =
    {
        DI_SEQ_PLL_POWERDOWN_SORPLL_STEPMASK_LEVEL_0_GM10X
    };

    static const LwU16 diPllPowerDownStepmasksRefmpll[] =
    {
        DI_SEQ_PLL_POWERDOWN_REFMPLL_STEPMASK_LEVEL_0_GM10X
    };

    static const LwU16 diPllPowerDownStepmasksDrampll[] =
    {
        DI_SEQ_PLL_POWERDOWN_DRAMPLL_STEPMASK_LEVEL_0_GM10X
    };

    static const LwU16 diPllPowerDownStepmasksPll16G[] =
    {
        DI_SEQ_PLL_POWERDOWN_PLL16G_STEPMASK_LEVEL_0_GP10X,
        DI_SEQ_PLL_POWERDOWN_PLL16G_STEPMASK_LEVEL_1_GP10X
    };

    levelCountNafll         = LW_ARRAY_ELEMENTS(diPllPowerDownStepmasksNafll);
    levelCountLegacyCore    = LW_ARRAY_ELEMENTS(diPllPowerDownStepmasksLegacyCore);
    levelCountHpll          = LW_ARRAY_ELEMENTS(diPllPowerDownStepmasksHpll);
    levelCountVpll          = LW_ARRAY_ELEMENTS(diPllPowerDownStepmasksVpll);
    levelCountSorpll        = LW_ARRAY_ELEMENTS(diPllPowerDownStepmasksSorpll);
    levelCountRefmpll       = LW_ARRAY_ELEMENTS(diPllPowerDownStepmasksRefmpll);
    levelCountDrampll       = LW_ARRAY_ELEMENTS(diPllPowerDownStepmasksDrampll);
    levelCountPll16G        = LW_ARRAY_ELEMENTS(diPllPowerDownStepmasksPll16G);

    maxLevelCount = LW_MAX(levelCountNafll, levelCountLegacyCore);
    maxLevelCount = LW_MAX(levelCountHpll, maxLevelCount);
    maxLevelCount = LW_MAX(levelCountVpll, maxLevelCount);
    maxLevelCount = LW_MAX(levelCountSorpll, maxLevelCount);
    maxLevelCount = LW_MAX(levelCountRefmpll, maxLevelCount);
    maxLevelCount = LW_MAX(levelCountDrampll, maxLevelCount);
    maxLevelCount = LW_MAX(levelCountPll16G, maxLevelCount);

    // Outer For Loop iterates over all the levels
    for (lwrrentLevel = 0; lwrrentLevel < maxLevelCount; lwrrentLevel++)
    {
        //
        // Inner for loop iterates over all PLLs in PLL List
        // Thus first Level 1 is exelwted for all PLLs, the Level 2.. and so on
        //
        for (i = pllCount - 1; i >= 0; i--)
        {
            if (pPllList[i].pllType == DI_PLL_TYPE_NAFLL)
            {
                if (lwrrentLevel < levelCountNafll)
                {
                    diSeqPllPowerdownNafll_HAL(&Di, &pPllList[i], diPllPowerDownStepmasksNafll[lwrrentLevel]);
                }
            }
            else if (pPllList[i].pllType == DI_PLL_TYPE_LEGACY_CORE)
            {
                if (lwrrentLevel < levelCountLegacyCore)
                {
                    diSeqPllPowerdownLegacyCore_HAL(&Di, &pPllList[i], diPllPowerDownStepmasksLegacyCore[lwrrentLevel]);
                }
            }
            else if (pPllList[i].pllType == DI_PLL_TYPE_HPLL)
            {
                if (lwrrentLevel < levelCountHpll)
                {
                    diSeqPllPowerdownHpll_HAL(&Di, &pPllList[i], diPllPowerDownStepmasksHpll[lwrrentLevel]);
                }
            }
            else if (pPllList[i].pllType == DI_PLL_TYPE_PLL16G)
            {
                if (lwrrentLevel < levelCountPll16G)
                {
                    diSeqPllPowerdownPll16g_HAL(&Di, &pPllList[i], diPllPowerDownStepmasksPll16G[lwrrentLevel]);
                }
            }
            else if (pPllList[i].pllType == DI_PLL_TYPE_DRAMPLL)
            {
                if (lwrrentLevel < levelCountDrampll)
                {
                    diSeqPllPowerdownMemory_HAL(&Di, &pPllList[i], diPllPowerDownStepmasksDrampll[lwrrentLevel]);
                }
            }
            else if (pPllList[i].pllType == DI_PLL_TYPE_REFMPLL)
            {
                if (lwrrentLevel < levelCountRefmpll)
                {
                    diSeqPllPowerdownMemory_HAL(&Di, &pPllList[i], diPllPowerDownStepmasksRefmpll[lwrrentLevel]);
                }
            }
        }
    }
}

/*!
 * @Brief Unified PLL Powerup sequence for DI
 *
 * @Description : 1. Create list of Stepmasks for all PLLs for all possible pll types.
 *                   (For more info on Step Masks and Levels, Refer to pmu_objdi.h)
 *                2. Execute Level X steps for all Plls in pPllList
 *                3. Execute Level X+1 and so on.
 *
 * @param[in, out] pPllList : PLL cache containing list of PLLs to be powered up
 * @param[in]      pllCount : Number of PLLs in passed PLL list
 */
void
diSeqPllPowerup_GP10X
(
    DI_PLL *pPllList,
    LwU8    pllCount
)
{
    LwU32   lwrrentLevel;
    LwU32   maxLevelCount;
    LwU32   levelCountNafll;
    LwU32   levelCountLegacyCore;
    LwU32   levelCountVpll;
    LwU32   levelCountSorpll;
    LwU32   levelCountRefmpll;
    LwU32   levelCountDrampll;
    LwU32   levelCountPll16G;
    LwU32   levelCountHpll;
    LwS8    i;

    static const LwU16 diPllPowerupStepmasksNafll[] =
    {
        DI_SEQ_PLL_POWERUP_NAFLL_STEPMASK_LEVEL_0_GP10X,
        DI_SEQ_PLL_POWERUP_NAFLL_STEPMASK_LEVEL_1_GP10X,
        DI_SEQ_PLL_POWERUP_NAFLL_STEPMASK_LEVEL_2_GP10X
    };

    static const LwU16 diPllPowerupStepmasksLegacyCore[] =
    {
        DI_SEQ_PLL_POWERUP_LEGACY_CORE_STEPMASK_LEVEL_0_GP10X,
        DI_SEQ_PLL_POWERUP_LEGACY_CORE_STEPMASK_LEVEL_1_GP10X,
        DI_SEQ_PLL_POWERUP_LEGACY_CORE_STEPMASK_LEVEL_2_GP10X
    };

    static const LwU16 diPllPowerupStepmasksHpll[] =
    {
        DI_SEQ_PLL_POWERUP_HPLL_STEPMASK_LEVEL_0_GP10X,
        DI_SEQ_PLL_POWERUP_HPLL_STEPMASK_LEVEL_1_GP10X
    };

    static const LwU16 diPllPowerupStepmasksVpll[] =
    {
        DI_SEQ_PLL_POWERUP_VPLL_STEPMASK_LEVEL_0_GM10X,
        DI_SEQ_PLL_POWERUP_VPLL_STEPMASK_LEVEL_1_GM10X,
        DI_SEQ_PLL_POWERUP_VPLL_STEPMASK_LEVEL_2_GM10X
    };

    static const LwU16 diPllPowerupStepmasksSorpll[] =
    {
        DI_SEQ_PLL_POWERUP_SORPLL_STEPMASK_LEVEL_0_GM10X,
        DI_SEQ_PLL_POWERUP_SORPLL_STEPMASK_LEVEL_1_GM10X,
        DI_SEQ_PLL_POWERUP_SORPLL_STEPMASK_LEVEL_2_GM10X
    };

    static const LwU16 diPllPowerupStepmasksRefmpll[] =
    {
        DI_SEQ_PLL_POWERUP_REFMPLL_STEPMASK_LEVEL_0_GM10X,
        DI_SEQ_PLL_POWERUP_REFMPLL_STEPMASK_LEVEL_1_GM10X,
        DI_SEQ_PLL_POWERUP_REFMPLL_STEPMASK_LEVEL_2_GM10X
    };

    static const LwU16 diPllPowerupStepmasksDrampll[] =
    {
        DI_SEQ_PLL_POWERUP_DRAMPLL_STEPMASK_LEVEL_0_GM10X,
        DI_SEQ_PLL_POWERUP_DRAMPLL_STEPMASK_LEVEL_1_GM10X,
        DI_SEQ_PLL_POWERUP_DRAMPLL_STEPMASK_LEVEL_2_GM10X
    };

    static const LwU16 diPllPowerupStepmasksPll16G[] =
    {
        DI_SEQ_PLL_POWERUP_PLL16G_STEPMASK_LEVEL_0_GP10X,
        DI_SEQ_PLL_POWERUP_PLL16G_STEPMASK_LEVEL_1_GP10X,
        DI_SEQ_PLL_POWERUP_PLL16G_STEPMASK_LEVEL_2_GP10X
    };

    levelCountNafll         = LW_ARRAY_ELEMENTS(diPllPowerupStepmasksNafll);
    levelCountLegacyCore    = LW_ARRAY_ELEMENTS(diPllPowerupStepmasksLegacyCore);
    levelCountHpll          = LW_ARRAY_ELEMENTS(diPllPowerupStepmasksHpll);
    levelCountVpll          = LW_ARRAY_ELEMENTS(diPllPowerupStepmasksVpll);
    levelCountSorpll        = LW_ARRAY_ELEMENTS(diPllPowerupStepmasksSorpll);
    levelCountRefmpll       = LW_ARRAY_ELEMENTS(diPllPowerupStepmasksRefmpll);
    levelCountDrampll       = LW_ARRAY_ELEMENTS(diPllPowerupStepmasksDrampll);
    levelCountPll16G        = LW_ARRAY_ELEMENTS(diPllPowerupStepmasksPll16G);

    maxLevelCount = LW_MAX(levelCountNafll, levelCountLegacyCore);
    maxLevelCount = LW_MAX(levelCountHpll, maxLevelCount);
    maxLevelCount = LW_MAX(levelCountVpll, maxLevelCount);
    maxLevelCount = LW_MAX(levelCountSorpll, maxLevelCount);
    maxLevelCount = LW_MAX(levelCountRefmpll, maxLevelCount);
    maxLevelCount = LW_MAX(levelCountDrampll, maxLevelCount);
    maxLevelCount = LW_MAX(levelCountPll16G, maxLevelCount);

    // Outer For Loop iterates over all the levels
    for (lwrrentLevel = 0; lwrrentLevel < maxLevelCount; lwrrentLevel++)
    {
        //
        // Inner for loop iterates over all PLLs in PLL List
        // Thus first Level 1 is exelwted for all PLLs, the Level 2.. and so on
        //
        for (i = 0; i < pllCount; i++)
        {
            if (pPllList[i].pllType == DI_PLL_TYPE_NAFLL)
            {
                if (lwrrentLevel < levelCountNafll)
                {
                    diSeqPllPowerupNafll_HAL(&Di, &pPllList[i], diPllPowerupStepmasksNafll[lwrrentLevel],
                                                  Di.pNafllCtrlReg[i]);
                }
            }
            else if (pPllList[i].pllType == DI_PLL_TYPE_LEGACY_CORE)
            {
                if (lwrrentLevel < levelCountLegacyCore)
                {
                    diSeqPllPowerupLegacyCore_HAL(&Di, &pPllList[i], diPllPowerupStepmasksLegacyCore[lwrrentLevel]);
                }
            }
            else if (pPllList[i].pllType == DI_PLL_TYPE_HPLL)
            {
                if (lwrrentLevel < levelCountHpll)
                {
                    diSeqPllPowerupHpll_HAL(&Di, &pPllList[i], diPllPowerupStepmasksHpll[lwrrentLevel]);
                }
            }
            else if (pPllList[i].pllType == DI_PLL_TYPE_PLL16G)
            {
                if (lwrrentLevel < levelCountPll16G)
                {
                    diSeqPllPowerupPll16g_HAL(&Di, &pPllList[i], diPllPowerupStepmasksPll16G[lwrrentLevel]);
                }
            }
            else if (pPllList[i].pllType == DI_PLL_TYPE_DRAMPLL)
            {
                if (lwrrentLevel < levelCountDrampll)
                {
                    diSeqPllPowerupMemory_HAL(&Di, &pPllList[i], diPllPowerupStepmasksDrampll[lwrrentLevel]);
                }
            }
            else if (pPllList[i].pllType == DI_PLL_TYPE_REFMPLL)
            {
                if (lwrrentLevel < levelCountRefmpll)
                {
                    diSeqPllPowerupMemory_HAL(&Di, &pPllList[i], diPllPowerupStepmasksRefmpll[lwrrentLevel]);
                }
            }
        }
    }
}

/*!
 * @Brief Exelwtes Legacy PowerDown Sequence
 *
 * Steps :
 *    1.
 *    2.
 *
 * @param[in]   pPll      Cache for PLL that needs to be powered off
 * @param[in]   stepMask  Mask for steps needed to be exelwted
 *                        0 => Skip Step, 1 => Execute Step
 */
void
diSeqPllPowerdownLegacyCore_GP10X
(
    DI_PLL *pPll,
    LwU16   stepMask
)
{
    LwU32        regVal;

    if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_CORE, _POWERDOWN, _IDDQ_OFF_LCKDET_DISABLE, stepMask))
    {
        // Cache the PLL configuration state
        pPll->cacheVal = REG_RD32(FECS, pPll->regAddr);
        regVal = pPll->cacheVal;

        if (FLD_TEST_DRF(_PTRIM_SYS, _PLL_CFG, _IDDQ, _POWER_ON, pPll->cacheVal))
        {
#if     defined(LW_PTRIM_SYS_PLL_CFG_ENB_LCKDET)
            regVal = FLD_SET_DRF(_PTRIM_SYS, _PLL_CFG, _ENB_LCKDET, _POWER_OFF,
                                 regVal);
#elif   defined(LW_PTRIM_SYS_PLL_CFG_EN_LCKDET)
            regVal = FLD_SET_DRF(_PTRIM_SYS, _PLL_CFG,  _EN_LCKDET, _POWER_OFF,
                                 regVal);
#endif

            // Disable the output and power off the PLL (PLL powerdown Step three)
            regVal = FLD_SET_DRF(_PTRIM_SYS, _PLL_CFG, _IDDQ, _POWER_OFF, regVal);
            regVal = FLD_SET_DRF(_PTRIM_SYS, _PLL_CFG, _ENABLE, _NO, regVal);

            REG_WR32(FECS, pPll->regAddr, regVal);
        }
    }
}

/*!
 * @Brief Exelwtes Legacy PowerUp Sequence
 *
 * Steps :
 *    1.
 *    2.
 *
 * @param[in]   pPll      Cache for PLL that needs to be powered on
 * @param[in]   stepMask  Mask for steps needed to be exelwted
 *                        0 => Skip Step, 1 => Execute Step
 */
void
diSeqPllPowerupLegacyCore_GP10X
(
    DI_PLL *pPll,
    LwU16   stepMask
)
{
    LwU32                 regVal;
    LwU32                 elapsedTime;
    static FLCN_TIMESTAMP diCoreTimestamp;

    // If PLL was powered on then don't power on
    if ((FLD_TEST_DRF(_PTRIM_SYS, _PLL_CFG, _IDDQ, _POWER_ON, pPll->cacheVal)))
    {
        if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_CORE, _POWERUP, _IDDQ_ON, stepMask))
        {
            // Power on the PLL
            regVal = REG_RD32(FECS, pPll->regAddr);
            regVal = FLD_SET_DRF(_PTRIM_SYS, _PLL_CFG, _IDDQ, _POWER_ON, regVal);
            REG_WR32(FECS, pPll->regAddr, regVal);
            osPTimerTimeNsLwrrentGet(&diCoreTimestamp);
        }

        if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_CORE, _POWERUP, _WAIT_PLL_READY, stepMask))
        {
            // Wait for the PLL to be ready to program (Clock in must be on)
            elapsedTime = osPTimerTimeNsElapsedGet(&diCoreTimestamp);

            if (elapsedTime < (DI_SEQ_PLL_READY_DELAY_DEFAULT_MAX_US_GP10X * 1000))
            {
                OS_PTIMER_SPIN_WAIT_NS((DI_SEQ_PLL_READY_DELAY_DEFAULT_MAX_US_GP10X * 1000) - elapsedTime);
            }
        }

        if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_CORE, _POWERUP, _LCKDET_ENABLE, stepMask))
        {
            regVal = REG_RD32(FECS, pPll->regAddr);

            if (FLD_TEST_DRF(_PTRIM_SYS, _PLL_CFG, _ENABLE, _YES, pPll->cacheVal))
            {
                // Enable the PLL output
                regVal = FLD_SET_DRF(_PTRIM_SYS, _PLL_CFG, _ENABLE, _YES, regVal);
            }

            // Enable lock detect
#if     defined(LW_PTRIM_SYS_PLL_CFG_ENB_LCKDET)
            regVal = FLD_SET_DRF(_PTRIM_SYS, _PLL_CFG, _ENB_LCKDET, _POWER_ON, regVal);
#elif   defined(LW_PTRIM_SYS_PLL_CFG_EN_LCKDET)
            regVal = FLD_SET_DRF(_PTRIM_SYS, _PLL_CFG,  _EN_LCKDET, _POWER_ON, regVal);
#endif
            REG_WR32(FECS, pPll->regAddr, regVal);
        }

        if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_CORE, _POWERUP, _WAIT_PLL_LOCK, stepMask))
        {
            if (FLD_TEST_DRF(_PTRIM_SYS, _PLL_CFG, _ENABLE, _YES, pPll->cacheVal))
            {
                // Wait for PLL to Lock
                if (!PMU_REG32_POLL_US(USE_FECS(pPll->regAddr),
                                       DRF_SHIFTMASK(LW_PTRIM_SYS_PLL_CFG_PLL_LOCK),
                                       DRF_DEF(_PTRIM, _SYS_PLL_CFG, _PLL_LOCK, _TRUE),
                                       (LwU32)DI_SEQ_PLL_LOCK_DETECTION_DELAY_DEFAULT_MAX_US,
                                       PMU_REG_POLL_MODE_BAR0_EQ))
                {
                    PMU_HALT();
                }
            }
        }
    }
}

/*!
 * @Brief Exelwtes NAFLL PowerDown Sequence
 *
 * Steps :
 *    1.
 *    2.
 *
 * @param[in]   pPll      Cache for PLL that needs to be powered off
 * @param[in]   stepMask  Mask for steps needed to be exelwted
 *                        0 => Skip Step, 1 => Execute Step
 */
void
diSeqPllPowerdownNafll_GP10X
(
    DI_PLL *pPll,
    LwU16   stepMask
)
{
    LwU32        regVal;

    if (DI_SEQ_PLL_STEPMASK_IS_SET(_NAFLL, _POWERDOWN, _IDDQ_OFF_DISABLE, stepMask))
    {
        // Cache the FLL configuration state
        pPll->cacheVal = REG_RD32(FECS, pPll->regAddr);
        regVal = pPll->cacheVal;

        if (FLD_TEST_DRF(_PTRIM, _GPC_BCAST_GPCNAFLL_CFG1, _IDDQ, _POWER_ON, pPll->cacheVal))
        {
            // Disable the output and power off the NAFLL
            regVal = FLD_SET_DRF(_PTRIM, _GPC_BCAST_GPCNAFLL_CFG1, _IDDQ, _POWER_OFF, regVal);
            regVal = FLD_SET_DRF(_PTRIM, _GPC_BCAST_GPCNAFLL_CFG1, _ENABLE, _NO, regVal);

            REG_WR32(FECS, pPll->regAddr, regVal);
        }
    }
}

/*!
 * @Brief Exelwtes NAFLL PowerUp Sequence
 *
 * Steps :
 *    1.
 *    2.
 *
 * @param[in]   pPll      Cache for PLL that needs to be powered on
 * @param[in]   stepMask  Mask for steps needed to be exelwted
 *                        0 => Skip Step, 1 => Execute Step
 */
void
diSeqPllPowerupNafll_GP10X
(
    DI_PLL *pPll,
    LwU16   stepMask,
    LwU32   ctrlReg
)
{
    LwU32                 regVal;
    LwU32                 elapsedTime;
    static FLCN_TIMESTAMP diNafllTimestamp;

    // If PLL was powered on then don't power on
    if ((FLD_TEST_DRF(_PTRIM, _GPC_BCAST_GPCNAFLL_CFG1, _IDDQ, _POWER_ON, pPll->cacheVal)))
    {
        if (DI_SEQ_PLL_STEPMASK_IS_SET(_NAFLL, _POWERUP, _IDDQ_ON, stepMask))
        {
            // Power on the PLL
            regVal = REG_RD32(FECS, pPll->regAddr);
            regVal = FLD_SET_DRF(_PTRIM, _GPC_BCAST_GPCNAFLL_CFG1, _IDDQ, _POWER_ON, regVal);
            REG_WR32(FECS, pPll->regAddr, regVal);
            osPTimerTimeNsLwrrentGet(&diNafllTimestamp);
        }

        if (DI_SEQ_PLL_STEPMASK_IS_SET(_NAFLL, _POWERUP, _WAIT_FLL_READY, stepMask))
        {
            // Wait for the PLL to be ready to program (Clock in must be on)
            elapsedTime = osPTimerTimeNsElapsedGet(&diNafllTimestamp);

            if (elapsedTime < (DI_SEQ_PLL_READY_DELAY_DEFAULT_MAX_US_GP10X * 1000))
            {
                OS_PTIMER_SPIN_WAIT_NS((DI_SEQ_PLL_READY_DELAY_DEFAULT_MAX_US_GP10X * 1000) - elapsedTime);
            }
        }

        if (DI_SEQ_PLL_STEPMASK_IS_SET(_NAFLL, _POWERUP, _ENABLE, stepMask))
        {
            if (FLD_TEST_DRF(_PTRIM, _GPC_BCAST_GPCNAFLL_CFG1, _ENABLE, _YES, pPll->cacheVal))
            {
                // Enable the PLL output
                regVal = REG_RD32(FECS, pPll->regAddr);
                regVal = FLD_SET_DRF(_PTRIM, _GPC_BCAST_GPCNAFLL_CFG1, _ENABLE, _YES, regVal);
                REG_WR32(FECS, pPll->regAddr, regVal);
            }
        }

        if (DI_SEQ_PLL_STEPMASK_IS_SET(_NAFLL, _POWERUP, _WAIT_FLL_LOCK, stepMask))
        {
            if (FLD_TEST_DRF(_PTRIM, _GPC_BCAST_GPCNAFLL_CFG1, _ENABLE, _YES, pPll->cacheVal))
            {
                OS_PTIMER_SPIN_WAIT_US(DI_SEQ_FLL_LOCK_DETECTION_DELAY_NAFLL_MAX_US);

                regVal = REG_RD32(FECS, ctrlReg);
                regVal = FLD_SET_DRF(_PTRIM, _GPC_BCAST_GPCNAFLL_CTRL1, _SETUP_FORCE_FLL_LOCK, _YES, regVal);
                REG_WR32(FECS, ctrlReg, regVal);

                regVal = REG_RD32(FECS, ctrlReg);
                regVal = FLD_SET_DRF(_PTRIM, _GPC_BCAST_GPCNAFLL_CTRL1, _SETUP_FORCE_FLL_LOCK, _NO, regVal);
                regVal = FLD_SET_DRF(_PTRIM, _GPC_BCAST_GPCNAFLL_CTRL1, _FLL_CTRL_CLEAR_FLL_LOCK, _YES, regVal);
                REG_WR32(FECS, ctrlReg, regVal);
            }
        }
    }
}

/*!
 * @Brief Exelwtes HPLL PowerDown Sequence
 *
 * Steps :
 *    1. Power Down PLL using IDDQ_POWER_OFF
 *
 * @param[in]   pPll      Cache for PLL that needs to be powered off
 * @param[in]   stepMask  Mask for steps needed to be exelwted
 *                        0 => Skip Step, 1 => Execute Step
 */
void
diSeqPllPowerdownHpll_GP10X
(
    DI_PLL *pPll,
    LwU16   stepMask
)
{
    LwU32 regVal;

    // The fuse related to FSM control of XTAL4XPLL is blown.
    if (Di.bXtal4xFuseBlown)
    {
        if (DI_SEQ_PLL_STEPMASK_IS_SET(_HPLL, _POWERDOWN, _IDDQ_OFF, stepMask))
        {
            regVal = REG_RD32(FECS, pPll->regAddr);
            pPll->cacheVal = regVal;

            if (FLD_TEST_DRF(_PTRIM, _SYS_XTAL4X_CFG, _IDDQ, _POWER_ON, pPll->cacheVal))
            {
                // Power Down XTAL4X PLL using IDDQ_POWER_OFF.
                regVal = FLD_SET_DRF(_PTRIM, _SYS_XTAL4X_CFG, _IDDQ,
                                     _POWER_OFF, regVal);
                REG_WR32(FECS, LW_PTRIM_SYS_XTAL4X_CFG, regVal);
            }
        }
    }
}

/*!
 * @Brief Exelwtes HPLL PowerUp Sequence
 *
 * Steps :
 *    1. Power Up PLL using IDDQ_POWER_ON
 *    2. Wait for a maximum period for the HPLL to power up (HW FSM to complete exelwtion)
 *
 * @param[in]   pPll      Cache for PLL that needs to be powered on
 * @param[in]   stepMask  Mask for steps needed to be exelwted
 *                        0 => Skip Step, 1 => Execute Step
 */
void
diSeqPllPowerupHpll_GP10X
(
    DI_PLL *pPll,
    LwU16   stepMask
)
{
    LwU32                   lwrrentTimeLo = 0;
    LwU32                   regVal;
    LwU32                   elapsedTime;
    static FLCN_TIMESTAMP   diHpllTimestamp;
    LwU32                   lwrrentTimeHi;

    // The fuse related to FSM control of XTAL4XPLL is blown.
    if (Di.bXtal4xFuseBlown)
    {
        if (FLD_TEST_DRF(_PTRIM, _SYS_XTAL4X_CFG, _IDDQ, _POWER_ON, pPll->cacheVal))
        {
            // Level 1
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_HPLL, _POWERUP, _IDDQ_ON, stepMask))
            {
                regVal      = REG_RD32(FECS, pPll->regAddr);
                // Power Up PLL using IDDQ_POWER_ON
                regVal = FLD_SET_DRF(_PTRIM, _SYS_XTAL4X_CFG, _IDDQ,
                                     _POWER_ON, regVal);
                REG_WR32(FECS, LW_PTRIM_SYS_XTAL4X_CFG, regVal);

                diSnapshotSciPtimer_HAL(&Di, &diHpllTimestamp.parts.lo,
                                             &diHpllTimestamp.parts.hi);
            }

            // Level 2
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_HPLL, _POWERUP, _WAIT_CLK_GOOD_TRUE, stepMask))
            {
                regVal      = REG_RD32(FECS, pPll->regAddr);
                diSnapshotSciPtimer_HAL(&Di, &lwrrentTimeLo,
                                             &lwrrentTimeHi);
                elapsedTime = lwrrentTimeLo - diHpllTimestamp.parts.lo;

                // Wait for a maximum period for the HPLL to power up (HW FSM to complete exelwtion)
                while (FLD_TEST_DRF(_PTRIM, _SYS_XTAL4X_CFG, _CLK_GOOD, _FALSE, regVal) &&
                       elapsedTime < DI_SEQ_PLL_HPLL_CLK_GOOD_TRUE_DELAY_MAX_NS)
                {
                    diSnapshotSciPtimer_HAL(&Di, &lwrrentTimeLo,
                                                 &lwrrentTimeHi);
                    elapsedTime = lwrrentTimeLo - diHpllTimestamp.parts.lo;
                    regVal = REG_RD32(FECS, pPll->regAddr);
                }

                // If HPLL has not powered up by now, Assert
                PMU_HALT_COND(FLD_TEST_DRF(_PTRIM, _SYS_XTAL4X_CFG, _CLK_GOOD, _TRUE, regVal));
            }
        }
    }
}

/*!
 * @Brief Exelwtes PLL16G PowerDown Sequence
 *
 * Steps :
 *    1. Disable Lock Detect
 *    2. Bypass Pll
 *    3. Disable Pll
 *    4. Disable spread spectrum (SSD)
 *    5. Power off Pll
 *
 * @param[in]   pPll      Cache for PLL that needs to be powered off
 * @param[in]   stepMask  Mask for steps needed to be exelwted
 *                        0 => Skip Step, 1 => Execute Step
 */
void
diSeqPllPowerdownPll16g_GP10X
(
    DI_PLL *pPll,
    LwU16   stepMask
)
{
    LwU32           regVal;
    DI_SEQ_CACHE   *pCache = DI_SEQ_GET_CAHCE();
    static LwBool   bBypassCtrlSet;

    // STEP 1. Disable Lock Detect
    if (DI_SEQ_PLL_STEPMASK_IS_SET(_PLL16G, _POWERDOWN, _LCKDET_OFF, stepMask))
    {
        regVal = REG_RD32(FECS, pPll->regAddr);
        regVal = FLD_SET_DRF(_PVTRIM, _SYS_SPPLL0_CFG, _EN_LCKDET, _POWER_OFF,
                             regVal);
        REG_WR32(FECS, pPll->regAddr, regVal);

        // Cache the config register
        pPll->cacheVal = regVal;
        bBypassCtrlSet = LW_FALSE;
    }

    // STEP 2. Bypass Pll
    if (DI_SEQ_PLL_STEPMASK_IS_SET(_PLL16G, _POWERDOWN, _BYPASSPLL, stepMask))
    {
        regVal = REG_RD32(FECS, LW_PVTRIM_SYS_BYPASSCTRL_DISP);

        if (!bBypassCtrlSet)
        {
            pCache->sppllBypassReg = regVal;

            regVal = FLD_SET_DRF(_PVTRIM, _SYS_BYPASSCTRL_DISP, _SPPLL0, _BYPASSCLK,
                                 regVal);
            regVal = FLD_SET_DRF(_PVTRIM, _SYS_BYPASSCTRL_DISP, _SPPLL1, _BYPASSCLK,
                                 regVal);
            REG_WR32(FECS, LW_PVTRIM_SYS_BYPASSCTRL_DISP, regVal);

            bBypassCtrlSet = LW_TRUE;
        }
    }

    // STEP 3. Disable Pll
    if (DI_SEQ_PLL_STEPMASK_IS_SET(_PLL16G, _POWERDOWN, _DISABLE, stepMask))
    {
        regVal = REG_RD32(FECS, pPll->regAddr);
        regVal = FLD_SET_DRF(_PVTRIM, _SYS_SPPLL0_CFG, _ENABLE, _NO,
                             regVal);
        REG_WR32(FECS, pPll->regAddr, regVal);
    }

    // STEP 4. Disable spread spectrum (SSD)
    if (DI_SEQ_PLL_STEPMASK_IS_SET(_PLL16G, _POWERDOWN, _DEACTIVATE_SSD, stepMask))
    {
        // TODO : Siddharthg : Get SSD Deactivation Seqeunce from HW and implement
        //                     for SPPLLs
    }

    if (DI_IS_PLL_IDDQ_ENABLED())
    {
        // STEP 5. Power off Pll
        if (DI_SEQ_PLL_STEPMASK_IS_SET(_PLL16G, _POWERDOWN, _IDDQ_OFF, stepMask))
        {
            regVal = REG_RD32(FECS, pPll->regAddr);
            regVal = FLD_SET_DRF(_PVTRIM, _SYS_SPPLL0_CFG, _IDDQ, _POWER_OFF,
                                 regVal);
            REG_WR32(FECS, pPll->regAddr, regVal);
        }
    }
}

/*!
 * @Brief Exelwtes PLL16G PowerUp Sequence
 *
 * Steps :
 *    1. Power on Pll
 *    2. Wait for Pll Ready
 *    3. Enable Lock Detect
 *    4. Enable Pll
 *    5. Wait for Pll Lock
 *    6. Enable spread spectrum (SSD)
 *    7. Restore Bypass control
 *
 * @param[in]   pPll      Cache for PLL that needs to be powered off
 * @param[in]   stepMask  Mask for steps needed to be exelwted
 *                        0 => Skip Step, 1 => Execute Step
 */
void
diSeqPllPowerupPll16g_GP10X
(
    DI_PLL *pPll,
    LwU16   stepMask
)
{
    LwU32                 regVal;
    LwU32                 elapsedTime;
    DI_SEQ_CACHE         *pCache = DI_SEQ_GET_CAHCE();
    static LwBool         diBBypassCtrlRestored = LW_FALSE;
    static FLCN_TIMESTAMP diPll16gTimestamp;

    if (FLD_TEST_DRF(_PVTRIM_SYS, _SPPLL0_CFG, _IDDQ, _POWER_ON, pPll->cacheVal))
    {
        if (DI_IS_PLL_IDDQ_ENABLED())
        {
            // STEP 1. Power on Pll
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_PLL16G, _POWERUP, _IDDQ_ON, stepMask))
            {
                regVal = REG_RD32(FECS, pPll->regAddr);
                regVal = FLD_SET_DRF(_PVTRIM, _SYS_SPPLL0_CFG, _IDDQ, _POWER_ON,
                                        regVal);
                REG_WR32(FECS, pPll->regAddr, regVal);

                // Log time for callwlation regarding PLL Ready
                osPTimerTimeNsLwrrentGet(&diPll16gTimestamp);
            }

            // STEP 2. Wait for Pll Ready
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_PLL16G, _POWERUP, _WAIT_PLL_READY, stepMask))
            {
                elapsedTime = osPTimerTimeNsElapsedGet(&diPll16gTimestamp);

                if (elapsedTime < (DI_SEQ_PLL_READY_DELAY_DEFAULT_MAX_US_GP10X * 1000))
                {
                    OS_PTIMER_SPIN_WAIT_NS((DI_SEQ_PLL_READY_DELAY_DEFAULT_MAX_US_GP10X * 1000) - elapsedTime);
                }
            }
        }

        // STEP 3. Enable Lock Detect
        if (DI_SEQ_PLL_STEPMASK_IS_SET(_PLL16G, _POWERUP, _LCKDET_ON, stepMask))
        {
            regVal = REG_RD32(FECS, pPll->regAddr);
            regVal = FLD_SET_DRF(_PVTRIM_SYS, _SPPLL0_CFG, _EN_LCKDET, _POWER_ON,
                                 pPll->cacheVal);
            REG_WR32(FECS, pPll->regAddr, regVal);

            diBBypassCtrlRestored = LW_FALSE;
        }

        if (FLD_TEST_DRF(_PVTRIM_SYS, _SPPLL0_CFG, _ENABLE, _YES, pPll->cacheVal))
        {
            // STEP 4. Enable Pll
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_PLL16G, _POWERUP, _ENABLE, stepMask))
            {
                regVal = REG_RD32(FECS, pPll->regAddr);
                regVal = FLD_SET_DRF(_PVTRIM, _SYS_SPPLL0_CFG, _ENABLE, _YES,
                                        regVal);
                REG_WR32(FECS, pPll->regAddr, regVal);
            }

            // STEP 4. Wait for Pll Lock
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_PLL16G, _POWERUP, _WAIT_PLL_LOCK, stepMask))
            {
                //
                // The colwentional way for polling is to simply use the PMU_REG32_POLL_US
                // macro. However it was observed that this way might increase latency by 4-6 us.
                // The reason was that, this API queries the Timer, and then checks the register Value.
                // Here we are checking the register value first. So if there are multiple PLLs being powered
                // on, the 2nd or 3rd PLL will save a timer read each.
                // PMU_REG32_POLL_US API read at 27Mhz is quite costly, and so removing it leads to
                // relatively high latency saving.
                //
                regVal = REG_RD32(FECS, pPll->regAddr);
                if (FLD_TEST_DRF(_PVTRIM_SYS, _SPPLL0_CFG, _PLL_LOCK, _FALSE, regVal))
                {
                    if (!PMU_REG32_POLL_US(USE_FECS(pPll->regAddr),
                                            DRF_SHIFTMASK(LW_PVTRIM_SYS_SPPLL0_CFG_PLL_LOCK),
                                            DRF_DEF(_PVTRIM_SYS, _SPPLL0_CFG, _PLL_LOCK, _TRUE),
                                            (LwU32)(DI_SEQ_PLL_LOCK_DETECTION_DELAY_DEFAULT_MAX_US),
                                            PMU_REG_POLL_MODE_BAR0_EQ))
                    {
                        PMU_HALT();
                    }
                }
            }

            // STEP 5. Enable spread spectrum (SSD)
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_PLL16G, _POWERUP, _ACTIVATE_SSD, stepMask))
            {
                // TODO : Siddharthg : Get SSD Activation Sequence from HW and implement
                //                     for SPPLLs
            }
        }
    }

    // STEP 6. Restore Bypass control
    if (DI_SEQ_PLL_STEPMASK_IS_SET(_PLL16G, _POWERUP, _BYPASSPLL, stepMask))
    {
        if (!diBBypassCtrlRestored)
        {
            REG_WR32(FECS, LW_PVTRIM_SYS_BYPASSCTRL_DISP, pCache->sppllBypassReg);
            diBBypassCtrlRestored = LW_TRUE;
        }
    }
}

/* ------------------------ Local Functions  ------------------------------- */

