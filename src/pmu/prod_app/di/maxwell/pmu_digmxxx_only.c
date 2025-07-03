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
 * @file   pmu_digmxxx_only.c
 * @brief  HAL-interface for the GM10x & GM20x DI Engine
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_gc6_island.h"
#include "dev_pmgr.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "dev_pwr_pri.h"
#include "dev_lw_xp.h"
#include "dev_top.h"
#include "dev_trim.h"
#include "dev_trim_addendum.h"
#include "clk3/generic_dev_trim.h"
#include "pmu/pmumailboxid.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "lwuproc.h"
#include "flcntypes.h"
#include "lwos_dma.h"
#include "pmu_objhal.h"
#include "pmu_objdi.h"
#include "pmu_objpmu.h"
#include "pmu_objpmgr.h"
#include "pmu_objms.h"
#include "pmu_swasr.h"

#include "config/g_di_private.h"

/* ------------------------- Global Variables ------------------------------- */
LwBool bGpcPllDfsEnabled;

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#ifndef LW_PTRIM_SYS_GPCPLL_DVFS1_EN_DFS_YES
#define LW_PTRIM_SYS_GPCPLL_DVFS1_EN_DFS_YES                         0x1
#define LW_PTRIM_SYS_GPCPLL_DVFS1_EN_DFS_NO                          0x0
#endif

#ifndef LW_PTRIM_SYS_XTAL4X_CTRL_SETUP_3_2
#define LW_PTRIM_SYS_XTAL4X_CTRL_SETUP_3_2                           3:2
#define LW_PTRIM_SYS_XTAL4X_CTRL_SETUP_7_6                           7:6
#endif

#define DI_SEQ_PLL_GET_SPPLL_REG_SIZE                                   \
                    (LW_PVTRIM_SYS_SPPLL1_CFG - LW_PVTRIM_SYS_SPPLL0_CFG)

#define DI_SEQ_PLL_GET_SPPLL_ADDR_DIFF(regAddr)  ((regAddr) - LW_PVTRIM_SYS_SPPLL0_CFG)


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
 *                 Following Such a power down sequence will parallelize the 5 us and 6 us waits of both PLLs
 *                 thus optimizing the DI latency.
 */

#define DI_SEQ_PLL_POWERDOWN_LEGACY_CORE_STEPMASK_LEVEL_0_GMXXX                         \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_CORE, _POWERDOWN, _SYNC_MODE_DISABLE)      |\
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_CORE, _POWERDOWN, _IDDQ_OFF_LCKDET_DISABLE) \
        )

#define DI_SEQ_PLL_POWERUP_LEGACY_CORE_STEPMASK_LEVEL_0_GMXXX                           \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_CORE, _POWERUP, _IDDQ_ON)                   \
        )

#define DI_SEQ_PLL_POWERUP_LEGACY_CORE_STEPMASK_LEVEL_1_GMXXX                           \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_CORE, _POWERUP, _WAIT_PLL_READY)           |\
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_CORE, _POWERUP, _LCKDET_ENABLE)             \
        )

#define DI_SEQ_PLL_POWERUP_LEGACY_CORE_STEPMASK_LEVEL_2_GMXXX                           \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_CORE, _POWERUP, _WAIT_PLL_LOCK)            |\
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_CORE, _POWERUP, _SYNC_MODE_ENABLE)          \
        )

#define DI_SEQ_PLL_POWERDOWN_NAPLL_STEPMASK_LEVEL_0_GMXXX                               \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_NAPLL, _POWERDOWN, _SYNC_MODE_DISABLE)            |\
            DI_SEQ_PLL_STEPMASK_GET(_NAPLL, _POWERDOWN, _IDDQ_OFF_LCKDET_DISABLE)       \
        )

#define DI_SEQ_PLL_POWERUP_NAPLL_STEPMASK_LEVEL_0_GMXXX                                 \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_NAPLL, _POWERUP, _IDDQ_ON)                         \
        )

#define DI_SEQ_PLL_POWERUP_NAPLL_STEPMASK_LEVEL_1_GMXXX                                 \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_NAPLL, _POWERUP, _WAIT_PLL_READY)                 |\
            DI_SEQ_PLL_STEPMASK_GET(_NAPLL, _POWERUP, _OUTPUT_ENABLE)                   \
        )

#define DI_SEQ_PLL_POWERUP_NAPLL_STEPMASK_LEVEL_2_GMXXX                                 \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_NAPLL, _POWERUP, _WAIT_PLL_LOCK)                  |\
            DI_SEQ_PLL_STEPMASK_GET(_NAPLL, _POWERUP, _SYNC_MODE_LCK_OVRD)              \
        )

#define DI_SEQ_PLL_POWERDOWN_LEGACY_XTAL4X_STEPMASK_LEVEL_0_GMXXX                       \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_XTAL4X, _POWERDOWN, _ALL)                   \
        )

#define DI_SEQ_PLL_POWERUP_LEGACY_XTAL4X_STEPMASK_LEVEL_0_GMXXX                         \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_XTAL4X, _POWERUP, _ALL)                     \
        )

#define DI_SEQ_PLL_POWERDOWN_LEGACY_SPPLL_STEPMASK_LEVEL_0_GMXXX                        \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_SPPLL, _POWERDOWN, _LCKDET_OFF)             \
        )

#define DI_SEQ_PLL_POWERDOWN_LEGACY_SPPLL_STEPMASK_LEVEL_1_GMXXX                        \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_SPPLL, _POWERDOWN, _BYPASSPLL)             |\
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_SPPLL, _POWERDOWN, _DISABLE)               |\
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_SPPLL, _POWERDOWN, _DEACTIVATE_SSA)        |\
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_SPPLL, _POWERDOWN, _IDDQ_OFF)               \
        )

#define DI_SEQ_PLL_POWERUP_LEGACY_SPPLL_STEPMASK_LEVEL_0_GMXXX                          \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_SPPLL, _POWERUP, _IDDQ_ON)                  \
        )

#define DI_SEQ_PLL_POWERUP_LEGACY_SPPLL_STEPMASK_LEVEL_1_GMXXX                          \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_SPPLL, _POWERUP, _WAIT_PLL_READY)          |\
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_SPPLL, _POWERUP, _LCKDET_ON)               |\
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_SPPLL, _POWERUP, _ENABLE)                   \
        )

#define DI_SEQ_PLL_POWERUP_LEGACY_SPPLL_STEPMASK_LEVEL_2_GMXXX                          \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_SPPLL, _POWERUP, _WAIT_PLL_LOCK)           |\
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_SPPLL, _POWERUP, _ACTIVATE_SSA)            |\
            DI_SEQ_PLL_STEPMASK_GET(_LEGACY_SPPLL, _POWERUP, _BYPASSPLL)                \
        )

/* --------------------------- Prototypes ------------------------------------*/

static LwBool s_diSeqIsNapllDfsEnabled_GMXXX(void);
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Change the Therm VID settings when utilsclk is switched from
 * 108MHz to Xtal and vice versa.
 *
 * @param[in,out]   pCache  Holds cached register values necessary for
 *                          entering into and exiting out of deepidle.
 * @param[in]       bExit   When true; restore vid register settings from cache.
 *                          Else; signifies entry. Overwrite the therm vid settings.
 *                          with SCI vid settings.
 *
 * @return  LW_TRUE if secondary idle
 *          LW_FALSE is secondary busy
 */
void
diSeqVidSwitch_GMXXX
(
    DI_SEQ_CACHE *pCache,
    LwBool        bExit
)
{
    LwU32 regVal;

    if (bExit)
    {
        REG_WR32(CSB, LW_CPWR_THERM_VID0_PWM,
                      PG_VOLT_GET_THERM_VID0_CACHE(PG_VOLT_RAIL_IDX_LOGIC));
        regVal = PG_VOLT_GET_THERM_VID1_CACHE(PG_VOLT_RAIL_IDX_LOGIC);
        regVal = FLD_SET_DRF(_CPWR_THERM, _VID1_PWM, _SETTING_NEW, _TRIGGER,
                             regVal);
        REG_WR32(CSB, LW_CPWR_THERM_VID1_PWM, regVal);
        //
        // We would be fine not polling here since the trigger completion
        // may hide behind the latency to reach the next step which does
        // ALT_CTL_SEL and eventually relinquishes gpio control from SCI
        // back to THERM.
        //
    }
    else
    {
        //
        // Copy the SCI VID config to therm before switching to xtal
        //
        REG_WR32(CSB, LW_CPWR_THERM_VID0_PWM,
                 REG_RD32(FECS, LW_PGC6_SCI_VID_CFG0));
        regVal = REG_RD32(FECS, LW_PGC6_SCI_VID_CFG1);
        regVal = FLD_SET_DRF(_CPWR_THERM, _VID1_PWM, _SETTING_NEW, _TRIGGER,
                             regVal);
        REG_WR32(CSB, LW_CPWR_THERM_VID1_PWM, regVal);

        do
        {
            regVal = REG_RD32(CSB, LW_CPWR_THERM_VID1_PWM);
        } while (FLD_TEST_DRF(_CPWR_THERM, _VID1_PWM, _SETTING_NEW, _PENDING, regVal));
        //
        // Since fmodel does not implement the bit; we will eventually keep polling.
        // WAR: add delay for ~800Khz period
        //
        // In case the poll is not needed;
        // the delay below can be used.
        // OS_PTIMER_SPIN_WAIT_NS(1300);
        //
    }
}

/*!
 * @brief DFE value saving during DI entry.
 * Select the lane and save DFE values
 *
 * @param[in,out]   pCache  Holds cached register values necessary for
 *                          entering into and exiting out of deepidle.
 */
void
diSeqDfeValuesSave_GMXXX
(
    DI_SEQ_CACHE *pCache
)
{
    LwU8  laneCount;
    LwU32 regVal;

    regVal = REG_RD32(BAR0, LW_XP_PEX_PAD_CTL_6);
    regVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_6, _MISC_OUT_SEL, 0x32, regVal);
    REG_WR32(BAR0, LW_XP_PEX_PAD_CTL_6, regVal);

    regVal = REG_RD32(BAR0, LW_XP_PEX_PAD_CTL_SEL);
    for (laneCount = 0; laneCount < DI_PEX_LANE_MAX; laneCount++)
    {
        regVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_SEL, _LANE_SELECT, laneCount,
                                 regVal);
        REG_WR32(BAR0, LW_XP_PEX_PAD_CTL_SEL, regVal);

        regVal = REG_RD32(BAR0, LW_XP_PEX_PAD_CTL_6);
        pCache->pexDFEVal[laneCount] = DRF_VAL(_PTOP, _DEVICE_INFO, _ENTRY,
                                                   regVal);
    }
}

/*!
 * @brief DFE value restore during GC5.
 * Select the lane and restore DFE values
 *
 * @param[in]       pCache  Holds cached register values necessary for
 *                          entering into and exiting out of deepidle.
 */
void
diSeqDfeValuesRestore_GMXXX
(
    DI_SEQ_CACHE *pCache
)
{
    LwU8  laneCount;
    LwU32 regVal;

    regVal = REG_RD32(BAR0, LW_XP_PEX_PAD_CTL_SEL);
    for (laneCount = 0; laneCount < DI_PEX_LANE_MAX; laneCount++)
    {
        regVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_SEL, _LANE_SELECT, laneCount,
                                 regVal);
        REG_WR32(BAR0, LW_XP_PEX_PAD_CTL_SEL, regVal);
        REG_WR32(BAR0, LW_XP_PEX_PAD_ECTL_3_R3, pCache->pexDFEVal[laneCount]);
    }
}

/*!
 * @brief Force the auxiliary idle detect logic on the lowest active lane
 *        during the entirety of GC5. SW WAR for bug 1367027.
 *
 * @param[in,out]   pCache                 Holds cached register values necessary for
 *                                         entering into and exiting out of deepidle.
 * @param[in]       lowestPexActiveLaneBit Lowest active lane to force auxiliary
 *                                         idle detect on
 * @return void
 */
void
diSeqAuxRxIdleSet_GMXXX
(
    DI_SEQ_CACHE *pCache,
    LwU32         lowestPexActiveLaneBit
)
{
    LwU32 regVal;

    //
    // Force enable the aux rx idle detect logic on the lowest active lane.
    // Also cache the previous two values.
    //
    regVal = REG_RD32(BAR0, LW_XP_PEX_PAD_CTL_SEL);
    pCache->pexPadCtlLaneSel = regVal;

    // Select the lowest active lane
    regVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_SEL, _LANE_SELECT,
                             lowestPexActiveLaneBit, regVal);
    REG_WR32(BAR0, LW_XP_PEX_PAD_CTL_SEL, regVal);

    // Force enable the aux idle detect logic
    regVal = REG_RD32(BAR0, LW_XP_PEX_PAD_CTL_4);
    pCache->pexPadCtl4 = regVal;

    // 1. Force the override
    regVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_4, _AUX_RX_IDLE_EN_OVRD, _ENABLE,
                         regVal);
    // 2. Set the override value to enable
    regVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_4, _AUX_RX_IDLE_EN, _ENABLE,
                         regVal);

    REG_WR32(BAR0, LW_XP_PEX_PAD_CTL_4, regVal);

    //
    // Add a delay here to make sure the glitch that oclwrs in
    // switching between normal and aux idle detectors has passed
    // before unmasking the RX_STAT idle signal to the BSI. Part
    // of the SW WAR for bug 1367027.
    //
    OS_PTIMER_SPIN_WAIT_NS(500); // 2000/4 ns since we're on xtal
}

/*!
 * @brief Restore the lane mask and aux rx idle override that we forced on
 *        during the entirety of GC5. SW WAR for bug 1367027.
 *
 * @param[in]       pCache  Holds cached register values necessary for
 *                          entering into and exiting out of deepidle.
 *
 * @return void
*/
void
diSeqAuxRxIdleRestore_GMXXX
(
    DI_SEQ_CACHE *pCache
)
{
    REG_WR32(BAR0, LW_XP_PEX_PAD_CTL_SEL, pCache->pexPadCtlLaneSel);
    REG_WR32(BAR0, LW_XP_PEX_PAD_CTL_4,   pCache->pexPadCtl4);
}

/*!
 * Check if GPC PLL is Noise Aware and is actively
 * using the NA feature.
 *
 * @return       LW_TRUE     If GPCPLL is NA active
 */
static LwBool
s_diSeqIsNapllDfsEnabled_GMXXX(void)
{
    return (FLD_TEST_DRF(_PTRIM_SYS, _GPCPLL_CFG, _IDDQ, _POWER_ON,
                         REG_RD32(FECS, LW_PTRIM_SYS_GPCPLL_CFG)) &&
            FLD_TEST_DRF(_PTRIM_SYS, _GPCPLL_DVFS1, _EN_DFS, _YES,
                        REG_RD32(FECS, LW_PTRIM_SYS_GPCPLL_DVFS1)));
}

/*!
 * @brief Power Off the Therm Sensor
 *
 * @param[in,out]   pCache  Holds cached register values necessary for
 *                          entering into and exiting out of deepidle.
 */
void
diSeqThermSensorPowerOff_GMXXX
(
    DI_SEQ_CACHE *pCache
)
{
    LwU32 regVal;

    pCache->thermSensorReg = regVal = REG_RD32(CSB, LW_CPWR_THERM_SENSOR_0);

    // Set POWER to _OFF
    regVal = FLD_SET_DRF(_CPWR_THERM, _SENSOR_0, _POWER, _OFF, regVal);
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_0, regVal);
}

/*!
 * @brief Power On the Therm Sensor
 *
 * @param[in]       pCache  Holds cached register values necessary for
 *                          entering into and exiting out of deepidle.
 */
void
diSeqThermSensorPowerOn_GMXXX
(
    DI_SEQ_CACHE *pCache
)
{
    REG_WR32(CSB, LW_CPWR_THERM_SENSOR_0, pCache->thermSensorReg);
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
diSeqPllListInitXtal4x_GMXXX
(
    DI_PLL **ppPllList,
    LwU8    *pllCount
)
{
    static DI_PLL diXtal4xpllList[] =
    {
        {LW_PTRIM_SYS_XTAL4X_CFG, 0x0, DI_PLL_TYPE_LEGACY_XTAL4X}
    };

    *pllCount  = (LwU8) LW_ARRAY_ELEMENTS(diXtal4xpllList);
    *ppPllList = diXtal4xpllList;
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
diSeqPllListInitSpplls_GMXXX
(
    DI_PLL **ppPllList,
    LwU8    *pllCount
)
{
    static DI_PLL diSppllList[] =
    {
        {LW_PVTRIM_SYS_SPPLL_CFG(0), 0x0, DI_PLL_TYPE_LEGACY_SPPLL},
        {LW_PVTRIM_SYS_SPPLL_CFG(1), 0x0, DI_PLL_TYPE_LEGACY_SPPLL},
    };

    *pllCount  = (LwU8) LW_ARRAY_ELEMENTS(diSppllList);
    *ppPllList = diSppllList;
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
diSeqPllPowerdown_GMXXX
(
    DI_PLL *pPllList,
    LwU8    pllCount
)
{
    LwU32   lwrrentLevel;
    LwU32   maxLevelCount;
    LwU32   levelCountNapll;
    LwU32   levelCountLegacyCore;
    LwU32   levelCountLegacyXtal4x;
    LwU32   levelCountDrampll;
    LwU32   levelCountRefmpll;
    LwU32   levelCountVpll;
    LwU32   levelCountSorpll;
    LwU32   levelCountLegacySppll;
    LwS8    i;

    static const LwU16 diPllPowerDownStepmasksNapll[] =
    {
        DI_SEQ_PLL_POWERDOWN_NAPLL_STEPMASK_LEVEL_0_GMXXX
    };

    static const LwU16 diPllPowerDownStepmasksLegacyCore[] =
    {
        DI_SEQ_PLL_POWERDOWN_LEGACY_CORE_STEPMASK_LEVEL_0_GMXXX
    };

    static const LwU16 diPllPowerDownStepmasksLegacyXtal4x[] =
    {
        DI_SEQ_PLL_POWERDOWN_LEGACY_XTAL4X_STEPMASK_LEVEL_0_GMXXX
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

    static const LwU16 diPllPowerDownStepmasksLegacySppll[] =
    {
        DI_SEQ_PLL_POWERDOWN_LEGACY_SPPLL_STEPMASK_LEVEL_0_GMXXX,
        DI_SEQ_PLL_POWERDOWN_LEGACY_SPPLL_STEPMASK_LEVEL_1_GMXXX
    };

    levelCountNapll             = LW_ARRAY_ELEMENTS(diPllPowerDownStepmasksNapll);
    levelCountLegacyCore        = LW_ARRAY_ELEMENTS(diPllPowerDownStepmasksLegacyCore);
    levelCountLegacyXtal4x      = LW_ARRAY_ELEMENTS(diPllPowerDownStepmasksLegacyXtal4x);
    levelCountVpll              = LW_ARRAY_ELEMENTS(diPllPowerDownStepmasksVpll);
    levelCountSorpll            = LW_ARRAY_ELEMENTS(diPllPowerDownStepmasksSorpll);
    levelCountRefmpll           = LW_ARRAY_ELEMENTS(diPllPowerDownStepmasksRefmpll);
    levelCountDrampll           = LW_ARRAY_ELEMENTS(diPllPowerDownStepmasksDrampll);
    levelCountLegacySppll       = LW_ARRAY_ELEMENTS(diPllPowerDownStepmasksLegacySppll);

    maxLevelCount = LW_MAX(levelCountNapll, levelCountLegacyCore);
    maxLevelCount = LW_MAX(levelCountLegacyXtal4x, maxLevelCount);
    maxLevelCount = LW_MAX(levelCountVpll, maxLevelCount);
    maxLevelCount = LW_MAX(levelCountSorpll, maxLevelCount);
    maxLevelCount = LW_MAX(levelCountRefmpll, maxLevelCount);
    maxLevelCount = LW_MAX(levelCountDrampll, maxLevelCount);
    maxLevelCount = LW_MAX(levelCountLegacySppll, maxLevelCount);

    // Outer For Loop iterates over all the levels
    for (lwrrentLevel = 0; lwrrentLevel < maxLevelCount; lwrrentLevel++)
    {
        //
        // Inner for loop iterates over all PLLs in PLL List
        // Thus first Level 1 is exelwted for all PLLs, the Level 2.. and so on
        //
        for (i = pllCount - 1; i >= 0; i--)
        {
            if (pPllList[i].pllType == DI_PLL_TYPE_NAPLL)
            {
                if (lwrrentLevel < levelCountNapll)
                {
                    diSeqPllPowerdownNapll_HAL(&Di, &pPllList[i], diPllPowerDownStepmasksNapll[lwrrentLevel]);
                }
            }
            else if (pPllList[i].pllType == DI_PLL_TYPE_LEGACY_CORE)
            {
                if (lwrrentLevel < levelCountLegacyCore)
                {
                    diSeqPllPowerdownLegacyCore_HAL(&Di, &pPllList[i], diPllPowerDownStepmasksLegacyCore[lwrrentLevel]);
                }
            }
            else if (pPllList[i].pllType == DI_PLL_TYPE_LEGACY_XTAL4X)
            {
                if (lwrrentLevel < levelCountLegacyXtal4x)
                {
                    diSeqPllPowerdownLegacyXtal4x_HAL(&Di, &pPllList[i], diPllPowerDownStepmasksLegacyXtal4x[lwrrentLevel]);
                }
            }
            else if (pPllList[i].pllType == DI_PLL_TYPE_LEGACY_SPPLL)
            {
                if (lwrrentLevel < levelCountLegacySppll)
                {
                    diSeqPllPowerdownLegacySppll_HAL(&Di, &pPllList[i], diPllPowerDownStepmasksLegacySppll[lwrrentLevel]);
                }
            }
            else if (pPllList[i].pllType == DI_PLL_TYPE_REFMPLL)
            {
                if (lwrrentLevel < levelCountRefmpll)
                {
                    diSeqPllPowerdownMemory_HAL(&Di, &pPllList[i], diPllPowerDownStepmasksRefmpll[lwrrentLevel]);
                }
            }
            else if (pPllList[i].pllType == DI_PLL_TYPE_DRAMPLL)
            {
                if (lwrrentLevel < levelCountDrampll)
                {
                    diSeqPllPowerdownMemory_HAL(&Di, &pPllList[i], diPllPowerDownStepmasksDrampll[lwrrentLevel]);
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
diSeqPllPowerup_GMXXX
(
    DI_PLL *pPllList,
    LwU8    pllCount
)
{
    LwU32   lwrrentLevel;
    LwU32   maxLevelCount;
    LwU32   levelCountNapll;
    LwU32   levelCountLegacyCore;
    LwU32   levelCountLegacyXtal4x;
    LwU32   levelCountDrampll;
    LwU32   levelCountRefmpll;
    LwU32   levelCountVpll;
    LwU32   levelCountSorpll;
    LwU32   levelCountLegacySppll;
    LwS8    i;

    static const LwU16 diPllPowerUpStepmasksNapll[] =
    {
        DI_SEQ_PLL_POWERUP_NAPLL_STEPMASK_LEVEL_0_GMXXX,
        DI_SEQ_PLL_POWERUP_NAPLL_STEPMASK_LEVEL_1_GMXXX,
        DI_SEQ_PLL_POWERUP_NAPLL_STEPMASK_LEVEL_2_GMXXX
    };

    static const LwU16 diPllPowerUpStepmasksLegacyCore[] =
    {
        DI_SEQ_PLL_POWERUP_LEGACY_CORE_STEPMASK_LEVEL_0_GMXXX,
        DI_SEQ_PLL_POWERUP_LEGACY_CORE_STEPMASK_LEVEL_1_GMXXX,
        DI_SEQ_PLL_POWERUP_LEGACY_CORE_STEPMASK_LEVEL_2_GMXXX
    };

    static const LwU16 diPllPowerUpStepmasksLegacyXtal4x[] =
    {
        DI_SEQ_PLL_POWERUP_LEGACY_XTAL4X_STEPMASK_LEVEL_0_GMXXX,
    };

    static const LwU16 diPllPowerUpStepmasksVpll[] =
    {
        DI_SEQ_PLL_POWERUP_VPLL_STEPMASK_LEVEL_0_GM10X,
        DI_SEQ_PLL_POWERUP_VPLL_STEPMASK_LEVEL_1_GM10X,
        DI_SEQ_PLL_POWERUP_VPLL_STEPMASK_LEVEL_2_GM10X
    };

    static const LwU16 diPllPowerUpStepmasksSorpll[] =
    {
        DI_SEQ_PLL_POWERUP_SORPLL_STEPMASK_LEVEL_0_GM10X,
        DI_SEQ_PLL_POWERUP_SORPLL_STEPMASK_LEVEL_1_GM10X,
        DI_SEQ_PLL_POWERUP_SORPLL_STEPMASK_LEVEL_2_GM10X
    };

    static const LwU16 diPllPowerUpStepmasksRefmpll[] =
    {
        DI_SEQ_PLL_POWERUP_REFMPLL_STEPMASK_LEVEL_0_GM10X,
        DI_SEQ_PLL_POWERUP_REFMPLL_STEPMASK_LEVEL_1_GM10X,
        DI_SEQ_PLL_POWERUP_REFMPLL_STEPMASK_LEVEL_2_GM10X
    };

    static const LwU16 diPllPowerUpStepmasksDrampll[] =
    {
        DI_SEQ_PLL_POWERUP_DRAMPLL_STEPMASK_LEVEL_0_GM10X,
        DI_SEQ_PLL_POWERUP_DRAMPLL_STEPMASK_LEVEL_1_GM10X,
        DI_SEQ_PLL_POWERUP_DRAMPLL_STEPMASK_LEVEL_2_GM10X
    };

    static const LwU16 diPllPowerUpStepmasksLegacySppll[] =
    {
        DI_SEQ_PLL_POWERUP_LEGACY_SPPLL_STEPMASK_LEVEL_0_GMXXX,
        DI_SEQ_PLL_POWERUP_LEGACY_SPPLL_STEPMASK_LEVEL_1_GMXXX,
        DI_SEQ_PLL_POWERUP_LEGACY_SPPLL_STEPMASK_LEVEL_2_GMXXX
    };

    levelCountNapll         = LW_ARRAY_ELEMENTS(diPllPowerUpStepmasksNapll);
    levelCountLegacyCore    = LW_ARRAY_ELEMENTS(diPllPowerUpStepmasksLegacyCore);
    levelCountLegacyXtal4x  = LW_ARRAY_ELEMENTS(diPllPowerUpStepmasksLegacyXtal4x);
    levelCountVpll          = LW_ARRAY_ELEMENTS(diPllPowerUpStepmasksVpll);
    levelCountSorpll        = LW_ARRAY_ELEMENTS(diPllPowerUpStepmasksSorpll);
    levelCountRefmpll       = LW_ARRAY_ELEMENTS(diPllPowerUpStepmasksRefmpll);
    levelCountDrampll       = LW_ARRAY_ELEMENTS(diPllPowerUpStepmasksDrampll);
    levelCountLegacySppll   = LW_ARRAY_ELEMENTS(diPllPowerUpStepmasksLegacySppll);

    maxLevelCount = LW_MAX(levelCountNapll, levelCountLegacyCore);
    maxLevelCount = LW_MAX(levelCountLegacyXtal4x, maxLevelCount);
    maxLevelCount = LW_MAX(levelCountVpll, maxLevelCount);
    maxLevelCount = LW_MAX(levelCountSorpll, maxLevelCount);
    maxLevelCount = LW_MAX(levelCountRefmpll, maxLevelCount);
    maxLevelCount = LW_MAX(levelCountDrampll, maxLevelCount);
    maxLevelCount = LW_MAX(levelCountLegacySppll, maxLevelCount);

    // Outer For Loop iterates over all the levels
    for (lwrrentLevel = 0; lwrrentLevel < maxLevelCount; lwrrentLevel++)
    {
        //
        // Inner for loop iterates over all PLLs in PLL List
        // Thus first Level 1 is exelwted for all PLLs, the Level 2.. and so on
        //
        for (i = 0; i < pllCount; i++)
        {
            if (pPllList[i].pllType == DI_PLL_TYPE_NAPLL)
            {
                if (lwrrentLevel < levelCountNapll)
                {
                    diSeqPllPowerupNapll_HAL(&Di, &pPllList[i], diPllPowerUpStepmasksNapll[lwrrentLevel]);
                }
            }
            else if (pPllList[i].pllType == DI_PLL_TYPE_LEGACY_CORE)
            {
                if (lwrrentLevel < levelCountLegacyCore)
                {
                    diSeqPllPowerupLegacyCore_HAL(&Di, &pPllList[i], diPllPowerUpStepmasksLegacyCore[lwrrentLevel]);
                }
            }
            else if (pPllList[i].pllType == DI_PLL_TYPE_LEGACY_XTAL4X)
            {
                if (lwrrentLevel < levelCountLegacyXtal4x)
                {
                    diSeqPllPowerupLegacyXtal4x_HAL(&Di, &pPllList[i], diPllPowerUpStepmasksLegacyXtal4x[lwrrentLevel]);
                }
            }
            else if (pPllList[i].pllType == DI_PLL_TYPE_LEGACY_SPPLL)
            {
                if (lwrrentLevel < levelCountLegacySppll)
                {
                    diSeqPllPowerupLegacySppll_HAL(&Di, &pPllList[i], diPllPowerUpStepmasksLegacySppll[lwrrentLevel]);
                }
            }
            else if (pPllList[i].pllType == DI_PLL_TYPE_DRAMPLL)
            {
                if (lwrrentLevel < levelCountDrampll)
                {
                    diSeqPllPowerupMemory_HAL(&Di, &pPllList[i], diPllPowerUpStepmasksDrampll[lwrrentLevel]);
                }
            }
            else if (pPllList[i].pllType == DI_PLL_TYPE_REFMPLL)
            {
                if (lwrrentLevel < levelCountRefmpll)
                {
                    diSeqPllPowerupMemory_HAL(&Di, &pPllList[i], diPllPowerUpStepmasksRefmpll[lwrrentLevel]);
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
diSeqPllPowerdownLegacyCore_GMXXX
(
    DI_PLL *pPll,
    LwU16   stepMask
)
{
    LwU32        regVal;

    if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_CORE, _POWERDOWN, _SYNC_MODE_DISABLE, stepMask))
    {
        // Cache the PLL configuration state
        pPll->cacheVal = REG_RD32(FECS, pPll->regAddr);

        // Nothing to do if PLL is already powered off
        if (FLD_TEST_DRF(_PTRIM_SYS, _PLL_CFG, _IDDQ, _POWER_ON, pPll->cacheVal) &&
            FLD_TEST_DRF(_PTRIM_SYS, _PLL_CFG, _SYNC_MODE, _ENABLE, pPll->cacheVal))
        {
            // Disable Sync Mode only if its Enabled.(PLL powerdown Step One)
            regVal = FLD_SET_DRF(_PTRIM_SYS, _PLL_CFG, _SYNC_MODE, _DISABLE, pPll->cacheVal);
            REG_WR32(FECS, pPll->regAddr, regVal);
        }
    }

    if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_CORE, _POWERDOWN, _IDDQ_OFF_LCKDET_DISABLE, stepMask))
    {
        if (FLD_TEST_DRF(_PTRIM_SYS, _PLL_CFG, _IDDQ, _POWER_ON, pPll->cacheVal))
        {
            // Disable the Lock Detect for the PLL (PLL powerdown Step Two)
            regVal = REG_RD32(FECS, pPll->regAddr);
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
diSeqPllPowerupLegacyCore_GMXXX
(
    DI_PLL *pPll,
    LwU16   stepMask
)
{
    LwU32          regVal;

    // If PLL was powered on then don't power on
    if ((FLD_TEST_DRF(_PTRIM_SYS, _PLL_CFG, _IDDQ, _POWER_ON, pPll->cacheVal)))
    {
        if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_CORE, _POWERUP, _IDDQ_ON, stepMask))
        {
            // Power on the PLL
            regVal = REG_RD32(FECS, pPll->regAddr);
            regVal = FLD_SET_DRF(_PTRIM_SYS, _PLL_CFG, _IDDQ, _POWER_ON, regVal);
            REG_WR32(FECS, pPll->regAddr, regVal);
        }

        if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_CORE, _POWERUP, _WAIT_PLL_READY, stepMask))
        {
            // Wait for the PLL to be ready to program (Clock in must be on)
            if (!PMU_REG32_POLL_US(USE_FECS(pPll->regAddr),
                                   DRF_SHIFTMASK(LW_PTRIM_SYS_GPCPLL_CFG_PLL_READY),
                                   DRF_DEF(_PTRIM, _SYS_GPCPLL_CFG, _PLL_READY, _TRUE),
                                   (LwU32)DI_SEQ_PLL_READY_DELAY_DEFAULT_MAX_US_GMXXX,
                                   PMU_REG_POLL_MODE_BAR0_EQ))
            {
                PMU_HALT();
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

        if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_CORE, _POWERUP, _SYNC_MODE_ENABLE, stepMask) &&
            FLD_TEST_DRF(_PTRIM_SYS, _PLL_CFG, _ENABLE, _YES, pPll->cacheVal)            &&
            FLD_TEST_DRF(_PTRIM_SYS, _PLL_CFG, _SYNC_MODE, _ENABLE, pPll->cacheVal))
        {
            // Restore SYNC Mode settings
            regVal = REG_RD32(FECS, pPll->regAddr);
            regVal = FLD_SET_DRF(_PTRIM_SYS, _PLL_CFG, _SYNC_MODE, _ENABLE,
                                 regVal);
            REG_WR32(FECS, pPll->regAddr, regVal);
        }
    }
}

/*!
 * @Brief Exelwtes XTAL4X Legacy PowerUp Sequence
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
diSeqPllPowerupLegacyXtal4x_GMXXX
(
    DI_PLL *pPll,
    LwU16   stepMask
)
{
    if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_XTAL4X, _POWERUP, _ALL, stepMask))
    {
        LwU32 regVal;

        //
        // Refer to the seq in bug 1372154 comment #31
        // a.     Set LW_PTRIM_SYS_SYSPLL_CFG_IDDQ_POWER_ON ||
        //        LW_PTRIM_SYS_SYSPLL_CTRL_EN_IREF = 1
        //        (IREF is always ON.)
        // b.     Poll for SYSPLL_CFG_READY = 1 (or wait 5us)
        //        (not needed since IREF is always ON)
        // c.     Set LW_PTRIM_SYS_XTAL4X_CTRL_SETUP = 0x8C
        //        SETUP[3:2]=11 (0x3) SETUP[7:6]=10 (0x2)
        // d.     Wait 20us
        // e.     Set LW_PTRIM_SYS_XTAL4X_CFG_IDDQ = 0
        // f.     Wait 10us
        // g.     Set LW_PTRIM_SYS_XTAL4X_CTRL_SETUP = 0x80
        //        SETUP[3:2]=00 (0x0)
        //

        // Check if the pll was powered on during GC5 entry
        if (FLD_TEST_DRF(_PTRIM, _SYS_PLL_CFG, _IDDQ, _POWER_ON,
                         pPll->cacheVal))
        {
            // step c.
            regVal = REG_RD32(FECS, LW_PTRIM_SYS_XTAL4X_CTRL);
            regVal = FLD_SET_DRF_NUM(_PTRIM_SYS, _XTAL4X_CTRL, _SETUP_3_2, 0x3, regVal);
            regVal = FLD_SET_DRF_NUM(_PTRIM_SYS, _XTAL4X_CTRL, _SETUP_7_6, 0x2, regVal);
            REG_WR32(FECS, LW_PTRIM_SYS_XTAL4X_CTRL, regVal);

            // step d.
            OS_PTIMER_SPIN_WAIT_NS(5000);   // 20000/4 = 5000 ns since utilsclk is still on xtal

            // step e.
            REG_WR32(FECS, pPll->regAddr, pPll->cacheVal);

            // step f.
            OS_PTIMER_SPIN_WAIT_NS(2500);  // 10000/4 ns since utilsclk is still on xtal

            // step g.
            regVal = REG_RD32(FECS, LW_PTRIM_SYS_XTAL4X_CTRL);
            regVal = FLD_SET_DRF_NUM(_PTRIM_SYS, _XTAL4X_CTRL, _SETUP_3_2, 0x0, regVal);
            REG_WR32(FECS, LW_PTRIM_SYS_XTAL4X_CTRL, regVal);
        }
    }
}

/*!
 * @Brief Exelwtes XTAL4X Legacy PowerDown Sequence
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
diSeqPllPowerdownLegacyXtal4x_GMXXX
(
    DI_PLL *pPll,
    LwU16   stepMask
)
{
    if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_XTAL4X, _POWERDOWN, _ALL, stepMask))
    {
        LwU32 regVal;

        regVal = REG_RD32(FECS, pPll->regAddr);
        pPll->cacheVal = regVal;

        // Check if pll is powered on
        if (FLD_TEST_DRF(_PTRIM, _SYS_PLL_CFG, _IDDQ, _POWER_ON, regVal))
        {
            regVal = FLD_SET_DRF(_PTRIM, _SYS_PLL_CFG, _IDDQ, _POWER_OFF, regVal);
            REG_WR32(FECS, pPll->regAddr, regVal);
        }
    }
}

/*!
 * @Brief Exelwtes NAPLL Powerdown Sequence
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
diSeqPllPowerdownNapll_GMXXX
(
    DI_PLL *pPll,
    LwU16   stepMask
)
{
    LwU32 regVal;

    if (!(PMUCFG_FEATURE_ENABLED(PMU_DIDLE_NAPLL_WAR_1371047)))
    {
        diSeqPllPowerdownLegacyCore_HAL(&Di, pPll, stepMask);
        return;
    }

    // Disable SYNC_MODE of PLL
    if (DI_SEQ_PLL_STEPMASK_IS_SET(_NAPLL, _POWERDOWN, _SYNC_MODE_DISABLE, stepMask))
    {
        // Update the cache once for NAPLL enabled bit
        bGpcPllDfsEnabled = s_diSeqIsNapllDfsEnabled_GMXXX();
        diSeqPllPowerdownLegacyCore_HAL(&Di, pPll,
                                        DI_SEQ_PLL_STEPMASK_GET(_LEGACY_CORE, _POWERDOWN, _SYNC_MODE_DISABLE));
    }

    // Power off, disable lock detect and disable output of PLL in single register write
    // Disable Lock Override
    if (DI_SEQ_PLL_STEPMASK_IS_SET(_NAPLL, _POWERDOWN, _IDDQ_OFF_LCKDET_DISABLE, stepMask))
    {
        if (FLD_TEST_DRF(_PTRIM_SYS, _PLL_CFG, _IDDQ, _POWER_ON, pPll->cacheVal))
        {
            regVal = REG_RD32(FECS, pPll->regAddr);
#ifdef  LW_PTRIM_SYS_PLL_CFG_ENB_LCKDET
            regVal = FLD_SET_DRF(_PTRIM_SYS, _PLL_CFG, _ENB_LCKDET, _POWER_OFF,
                                 regVal);
#elif   defined(LW_PTRIM_SYS_PLL_CFG_EN_LCKDET)
            regVal = FLD_SET_DRF(_PTRIM_SYS, _PLL_CFG,  _EN_LCKDET, _POWER_OFF,
                                 regVal);
#endif

            regVal = FLD_SET_DRF(_PTRIM_SYS, _PLL_CFG, _IDDQ, _POWER_OFF, regVal);
            regVal = FLD_SET_DRF(_PTRIM_SYS, _PLL_CFG, _ENABLE, _NO, regVal);
            if (bGpcPllDfsEnabled)
            {
                regVal = FLD_SET_DRF(_PTRIM_SYS, _PLL_CFG, _LOCK_OVERRIDE, _DISABLE, regVal);
            }
            REG_WR32(FECS, pPll->regAddr, regVal);
        }
    }
}

/*!
 * @Brief Exelwtes NAPLL Powerup Sequence
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
diSeqPllPowerupNapll_GMXXX
(
    DI_PLL *pPll,
    LwU16   stepMask
)
{
    LwU32                 regVal;
    static FLCN_TIMESTAMP diNapllTimestamp;
    LwU32                 elapsedTime;

    // If PLL was powered on then don't power on
    if ((FLD_TEST_DRF(_PTRIM_SYS, _PLL_CFG, _IDDQ, _POWER_ON, pPll->cacheVal)))
    {
        if (!(PMUCFG_FEATURE_ENABLED(PMU_DIDLE_NAPLL_WAR_1371047) &&
              bGpcPllDfsEnabled))
        {
            diSeqPllPowerupLegacyCore_HAL(&Di, pPll, stepMask);
            return;
        }

        // Step 0 : Power on the PLL
        if (DI_SEQ_PLL_STEPMASK_IS_SET(_NAPLL, _POWERUP, _IDDQ_ON, stepMask))
        {
            diSeqPllPowerupLegacyCore_HAL(&Di, pPll,
                                          DI_SEQ_PLL_STEPMASK_GET(_LEGACY_CORE, _POWERUP, _IDDQ_ON));
        }

        // Step 1 : Wait for PLL_READY to be asserted
        if (DI_SEQ_PLL_STEPMASK_IS_SET(_NAPLL, _POWERUP, _WAIT_PLL_READY, stepMask))
        {
            diSeqPllPowerupLegacyCore_HAL(&Di, pPll,
                                          DI_SEQ_PLL_STEPMASK_GET(_LEGACY_CORE, _POWERUP, _WAIT_PLL_READY));
        }

        if (DI_SEQ_PLL_STEPMASK_IS_SET(_NAPLL, _POWERUP, _OUTPUT_ENABLE, stepMask))
        {
            if (FLD_TEST_DRF(_PTRIM_SYS, _PLL_CFG, _ENABLE, _YES, pPll->cacheVal))
            {
                regVal = REG_RD32(FECS, pPll->regAddr);
                regVal = FLD_SET_DRF(_PTRIM_SYS, _PLL_CFG, _ENABLE, _YES, regVal);
                REG_WR32(FECS, pPll->regAddr, regVal);
                osPTimerTimeNsLwrrentGet(&diNapllTimestamp);
            }
        }

        if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_CORE, _POWERUP, _WAIT_PLL_LOCK, stepMask))
        {
            elapsedTime = osPTimerTimeNsElapsedGet(&diNapllTimestamp);
            if (elapsedTime < (DI_SEQ_PLL_LOCK_DETECTION_DELAY_NAPLL_MAX_US * 1000))
            {
                OS_PTIMER_SPIN_WAIT_NS((DI_SEQ_PLL_LOCK_DETECTION_DELAY_NAPLL_MAX_US * 1000) - elapsedTime);
            }
        }

        if (DI_SEQ_PLL_STEPMASK_IS_SET(_NAPLL, _POWERUP, _SYNC_MODE_LCK_OVRD, stepMask) &&
            FLD_TEST_DRF(_PTRIM_SYS, _PLL_CFG, _ENABLE, _YES, pPll->cacheVal)            &&
            FLD_TEST_DRF(_PTRIM_SYS, _PLL_CFG, _SYNC_MODE, _ENABLE, pPll->cacheVal))
        {
            // Restore SYNC Mode settings
            regVal = REG_RD32(FECS, pPll->regAddr);
            regVal = FLD_SET_DRF(_PTRIM_SYS, _PLL_CFG, _SYNC_MODE, _ENABLE,
                                 regVal);
            regVal = FLD_SET_DRF(_PTRIM_SYS, _PLL_CFG, _LOCK_OVERRIDE,
                                 _ENABLE, regVal);
            REG_WR32(FECS, pPll->regAddr, regVal);
        }
    }
}

/*!
 * @Brief Exelwtes SPPLL PowerDown Sequence
 *
 * Steps :
 *    1. Disable Lock Detect
 *    2. Bypass Pll
 *    3. Disable Pll
 *    4. Bypass, reset, and disable spread spectrum (SSA)
 *    5. Power off Pll
 *
 * @param[in]   pPll      Cache for PLL that needs to be powered off
 * @param[in]   stepMask  Mask for steps needed to be exelwted
 *                        0 => Skip Step, 1 => Execute Step
 */
void
diSeqPllPowerdownLegacySppll_GMXXX
(
    DI_PLL *pPll,
    LwU16   stepMask
)
{
    LwU32           regVal;
    LwU32           i;
    DI_SEQ_CACHE   *pCache = DI_SEQ_GET_CAHCE();
    static LwBool   bBypassCtrlSet;

    // STEP 1. Disable Lock Detect
    if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_SPPLL, _POWERDOWN, _LCKDET_OFF, stepMask))
    {
        ct_assert (LW_PVTRIM_SYS_NUM_SPPLLS <= DI_SPPLLS_MAX);

        regVal = REG_RD32(FECS, pPll->regAddr);
        regVal = FLD_SET_DRF(_PVTRIM, _SYS_SPPLL_CFG, _ENB_LCKDET, _POWER_OFF,
                             regVal);
        REG_WR32(FECS, pPll->regAddr, regVal);

        // Cache the config register
        pPll->cacheVal = regVal;
        bBypassCtrlSet = LW_FALSE;
    }

    // STEP 2. Bypass Pll
    if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_SPPLL, _POWERDOWN, _BYPASSPLL, stepMask))
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
    if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_SPPLL, _POWERDOWN, _DISABLE, stepMask))
    {
        regVal = REG_RD32(FECS, pPll->regAddr);
        regVal = FLD_SET_DRF(_PVTRIM, _SYS_SPPLL_CFG, _ENABLE, _NO,
                             regVal);
        REG_WR32(FECS, pPll->regAddr, regVal);
    }

    // STEP 4. Bypass, reset, and disable spread spectrum (SSA)
    if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_SPPLL, _POWERDOWN, _DEACTIVATE_SSA, stepMask))
    {
        // Get the index of SPPLL
        i = DI_SEQ_PLL_GET_SPPLL_ADDR_DIFF(pPll->regAddr) /
                DI_SEQ_PLL_GET_SPPLL_REG_SIZE;

        pCache->sppllSSAReg[i] = REG_RD32(FECS, LW_PVTRIM_SYS_SPPLL_SSA(i));

        if (FLD_TEST_DRF(_PVTRIM_SYS, _SPPLL_SSA, _SSCBYP, _ACTIVATE_SPREAD,
                         pCache->sppllSSAReg[i]))
        {
            pCache->sppllCfg2Reg[i] = REG_RD32(FECS, LW_PVTRIM_SYS_SPPLL_CFG2(i));

            // Assert SSA_BYPASS_SS
            regVal = FLD_SET_DRF(_PVTRIM_SYS, _SPPLL_CFG2, _SSA_BYPASS_SS,
                                 _ASSERT, pCache->sppllCfg2Reg[i]);
            REG_WR32(FECS, LW_PVTRIM_SYS_SPPLL_CFG2(i), regVal);

            // Assert SSA_INTERP_RESET
            regVal = FLD_SET_DRF(_PVTRIM_SYS, _SPPLL_CFG2, _SSA_INTERP_RESET,
                                 _ASSERT, regVal);
            REG_WR32(FECS, LW_PVTRIM_SYS_SPPLL_CFG2(i), regVal);

            // Deactivate spread
            regVal = FLD_SET_DRF(_PVTRIM_SYS, _SPPLL_SSA, _SSCBYP,
                                 _DEACTIVATE_SPREAD, pCache->sppllSSAReg[i]);
            REG_WR32(FECS, LW_PVTRIM_SYS_SPPLL_SSA(i), regVal);
        }
    }

    if (DI_IS_PLL_IDDQ_ENABLED())
    {
        // STEP 5. Power off Pll
        if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_SPPLL, _POWERDOWN, _IDDQ_OFF, stepMask))
        {
            regVal = REG_RD32(FECS, pPll->regAddr);
            regVal = FLD_SET_DRF(_PVTRIM, _SYS_SPPLL_CFG, _IDDQ, _POWER_OFF,
                                 regVal);
            REG_WR32(FECS, pPll->regAddr, regVal);
        }
    }
}

/*!
 * @Brief Exelwtes SPPLL PowerUp Sequence
 *
 * Steps :
 *    1. Power on Pll
 *    2. Wait for Pll Ready
 *    3. Enable Lock Detect
 *    4. Enable Pll
 *    5. Wait for Pll Lock
 *    6. Enable spread spectrum (SSA)
 *    7. Restore Bypass control
 *
 * @param[in]   pPll      Cache for PLL that needs to be powered off
 * @param[in]   stepMask  Mask for steps needed to be exelwted
 *                        0 => Skip Step, 1 => Execute Step
 */
void
diSeqPllPowerupLegacySppll_GMXXX
(
    DI_PLL *pPll,
    LwU16   stepMask
)
{
    LwU32           regVal;
    LwU32           i;
    DI_SEQ_CACHE   *pCache = DI_SEQ_GET_CAHCE();
    static LwBool   bBypassCtrlRestored = LW_FALSE;

    if (FLD_TEST_DRF(_PVTRIM_SYS, _SPPLL_CFG, _IDDQ, _POWER_ON, pPll->cacheVal))
    {
        if (DI_IS_PLL_IDDQ_ENABLED())
        {
            // STEP 1. Power on Pll
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_SPPLL, _POWERUP, _IDDQ_ON, stepMask))
            {
                regVal = REG_RD32(FECS, pPll->regAddr);
                regVal = FLD_SET_DRF(_PVTRIM, _SYS_SPPLL_CFG, _IDDQ, _POWER_ON,
                                        regVal);
                REG_WR32(FECS, pPll->regAddr, regVal);
            }

            // STEP 2. Wait for Pll Ready
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_SPPLL, _POWERUP, _WAIT_PLL_READY, stepMask))
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
                if (FLD_TEST_DRF(_PVTRIM_SYS, _SPPLL_CFG, _PLL_READY, _FALSE, regVal))
                {
                    if (!PMU_REG32_POLL_US(USE_FECS(pPll->regAddr),
                                            DRF_SHIFTMASK(LW_PVTRIM_SYS_SPPLL_CFG_PLL_READY),
                                            DRF_DEF(_PVTRIM_SYS, _SPPLL_CFG, _PLL_READY, _TRUE),
                                            (LwU32)(DI_SEQ_PLL_LOCK_DETECTION_DELAY_DEFAULT_MAX_US),
                                            PMU_REG_POLL_MODE_BAR0_EQ))
                    {
                        PMU_HALT();
                    }
                }
            }
        }

        // STEP 3. Enable Lock Detect
        if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_SPPLL, _POWERUP, _LCKDET_ON, stepMask))
        {
            //
            // temp. fix for coverity bug id:154722 to avoid issues due to
            // major changes in this part of the code.
            //
            (void)REG_RD32(FECS, pPll->regAddr);
            regVal = FLD_SET_DRF(_PVTRIM_SYS, _SPPLL_CFG, _ENB_LCKDET, _POWER_ON,
                                 pPll->cacheVal);
            REG_WR32(FECS, pPll->regAddr, regVal);

            bBypassCtrlRestored = LW_FALSE;
        }

        if (FLD_TEST_DRF(_PVTRIM_SYS, _SPPLL_CFG, _ENABLE, _YES, pPll->cacheVal))
        {
            // STEP 4. Enable Pll
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_SPPLL, _POWERUP, _ENABLE, stepMask))
            {
                regVal = REG_RD32(FECS, pPll->regAddr);
                regVal = FLD_SET_DRF(_PVTRIM, _SYS_SPPLL_CFG, _ENABLE, _YES,
                                        regVal);
                REG_WR32(FECS, pPll->regAddr, regVal);
            }

            // STEP 5. Wait for Pll Lock
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_SPPLL, _POWERUP, _WAIT_PLL_LOCK, stepMask))
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
                if (FLD_TEST_DRF(_PVTRIM_SYS, _SPPLL_CFG, _PLL_LOCK, _FALSE, regVal))
                {
                    if (!PMU_REG32_POLL_US(USE_FECS(pPll->regAddr),
                                            DRF_SHIFTMASK(LW_PVTRIM_SYS_SPPLL_CFG_PLL_LOCK),
                                            DRF_DEF(_PVTRIM_SYS, _SPPLL_CFG, _PLL_LOCK, _TRUE),
                                            (LwU32)(DI_SEQ_PLL_LOCK_DETECTION_DELAY_DEFAULT_MAX_US),
                                            PMU_REG_POLL_MODE_BAR0_EQ))
                    {
                        PMU_HALT();
                    }
                }
            }

            // STEP 6. Enable spread spectrum (SSA)
            if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_SPPLL, _POWERUP, _ACTIVATE_SSA, stepMask))
            {
                // get the index of SPPLL
                i = DI_SEQ_PLL_GET_SPPLL_ADDR_DIFF(pPll->regAddr) /
                        DI_SEQ_PLL_GET_SPPLL_REG_SIZE;

                // If spread spectrum was previously on, we need to restore it
                if (FLD_TEST_DRF(_PVTRIM_SYS, _SPPLL_SSA, _SSCBYP, _ACTIVATE_SPREAD,
                                    pCache->sppllSSAReg[i]))
                {
                    regVal = REG_RD32(FECS, LW_PVTRIM_SYS_SPPLL_CFG2(i));

                    // Deassert SSA_BYPASS_SS
                    regVal = FLD_SET_DRF(_PVTRIM_SYS, _SPPLL_CFG2, _SSA_BYPASS_SS,
                                            _DEASSERT, regVal);
                    REG_WR32(FECS, LW_PVTRIM_SYS_SPPLL_CFG2(i), regVal);

                    // Activate SS
                    REG_WR32(FECS, LW_PVTRIM_SYS_SPPLL_SSA(i), pCache->sppllSSAReg[i]);

                    // Deassert SSA_INTERP_RESET
                    regVal = FLD_SET_DRF(_PVTRIM_SYS, _SPPLL_CFG2, _SSA_INTERP_RESET,
                                            _DEASSERT, regVal);
                    REG_WR32(FECS, LW_PVTRIM_SYS_SPPLL_CFG2(i), regVal);
                }
            }
        }
    }

    // STEP 7. Restore Bypass control
    if (DI_SEQ_PLL_STEPMASK_IS_SET(_LEGACY_SPPLL, _POWERUP, _BYPASSPLL, stepMask))
    {
        if (!bBypassCtrlRestored)
        {
            REG_WR32(FECS, LW_PVTRIM_SYS_BYPASSCTRL_DISP, pCache->sppllBypassReg);
            bBypassCtrlRestored = LW_TRUE;
        }
    }
}


/*!
 * @brief Mem pll actions for GC5.
 *
 * Algorithm: Entry: Assuming priv ring access is enabled
 * Assumption is that MSCG has already done SWASR Entry.
 * STEP A:
 * Enable main regulator
 *
 * STEP B:
 * Follow seq PLL_POWERDOWN
 *
 *
 * Exit: Assuming priv ring access is enabled
 *
 * STEP A:
 * Follow PLL_POWERUP
 *
 * STEP B:
 * Disable main regulator   // MS will powerup regulator later
 *
 *
 * PLL_POWERDOWN
 * 1. Put dramclk into onesource path.
 * 2. Unstop onesource path.
 * 3. Power down pll.
 * 4. Stop onesource path.
 * 5. Disable main regulator.
 * 6. Assert DRAMPLL Clamp.
 *
 * PLL_POWERUP:
 * 1. De-assert DRAMPLL Clamp.
 * 2. Enable main regulator.
 * 3. Unstop onesource path.
 * 4. Power up plls.
 * 5. Put dramclk back into refmpll path.
 * 6. Stop onesource path.
 *
 *
 * Enter: Power off the dram and refmpll.
 * Exit:  Power on the dram and refmpll.
 *
 * @param[in,out]   pCache      Holds cached register values necessary for
 *                              entering into and exiting out of deepidle.
 * @param[in]       bEntering   LW_TRUE if we are entering deepidle
 *                              LW_FALSE otherwise
 */
void
diSeqMemPllAction_GMXXX
(
    DI_SEQ_CACHE *pCache,
    LwBool        bEntering
)
{
    OBJSWASR *pSwAsr = MS_GET_SWASR();

    if (bEntering)     // GC5 entry
    {
        //
        // STEP A:
        // MSCG has already done the SWASR Entry
        // so now enable main regulator
        //
        // Enable main regulator  LW_PTRIM_SYS_FBIO_SPARE_FBIO_SPARE[0] = 1
        //
        REG_WR32(FECS, LW_PTRIM_SYS_FBIO_SPARE, pSwAsr->regs.sysFbioSpare);
        // Wait for 1 us to let it settle
        OS_PTIMER_SPIN_WAIT_NS(1000);

        //
        // STEP B: PLL_POWERDOWN
        //
        // 1. Put dramclk into onesource path.
        // 2. Unstop onesource path.
        // 3. Power down pll.
        // 4. Stop onesource path.
        // 5. Disable main regulator.
        // 6. Assert DRAMPLL Clamp.
        //

        // DRAM Clock should already be in bypass path. Ungate it from bypass path.
        msSwAsrDramClockGateFromBypass_HAL(&Ms, LW_FALSE);

        if (PG_IS_SF_ENABLED(Di, DI, DRAMPLL))
        {
            diSeqPllPowerdown_HAL(&Di, Di.pPllsDrampll,
                                       Di.pllCountDrampll);
        }

        if (PG_IS_SF_ENABLED(Di, DI, REFMPLL))
        {
            diSeqPllPowerdown_HAL(&Di, Di.pPllsRefmpll,
                                       Di.pllCountRefmpll);
        }

        //
        // Do dummy reads to the bcast registers to make sure writes went through
        // before we pull away the clocks on them.
        //
        // TODO: AI on LowPower team to remove this #if
#if (PMUCFG_CHIP_ENABLED(GV10X) || PMUCFG_CHIP_ENABLED(TU10X))
        (void)REG_RD32(FECS, LW_PFB_FBPA_REFMPLL_CFG);
        (void)REG_RD32(FECS, LW_PFB_FBPA_DRAMPLL_CFG);
#else
        (void)REG_RD32(FECS, LW_PTRIM_FBPA_BCAST_REFMPLL_CFG);
        (void)REG_RD32(FECS, LW_PTRIM_FBPA_BCAST_DRAMPLL_CFG);
#endif

        // DRAM Clock should already be in bypass path. Gate it from bypass path.
        msSwAsrDramClockGateFromBypass_HAL(&Ms, LW_TRUE);

        //
        // Disable main regulator LW_PTRIM_SYS_FBIO_SPARE_FBIO_SPARE = 0
        // Clear all the bits of this register to make sure that all the partitions
        // are put on the bypass regulator. Log a bug with HW to add defines
        // for spare bits.
        //
        REG_WR32(FECS, LW_PTRIM_SYS_FBIO_SPARE, 0);
    }
    else          // GC5 exit
    {
        //
        // STEP A:
        // Follow PLL_POWERUP
        //
        // STEP B:
        //   disable main regulator   // MS will powerup regulator later
        //

        //
        // STEP A:
        // PLL_POWERUP
        // 1. De-assert DRAMPLL Clamp.
        // 2. Enable main regulator.
        // 3. Unstop onesource path.
        // 4. Power up plls.
        // 5. Put dramclk back into refmpll path.
        // 6. Stop onesource path.
        //

        // Enable main regulator  LW_PTRIM_SYS_FBIO_SPARE_FBIO_SPARE[0] = 1
        REG_WR32(FECS, LW_PTRIM_SYS_FBIO_SPARE,
                pSwAsr->regs.sysFbioSpare);
        // Wait for 1 us to let it settle
        OS_PTIMER_SPIN_WAIT_NS(1000);

        // DRAM Clock should already be in bypass path. Ungate it from bypass path.
        msSwAsrDramClockGateFromBypass_HAL(&Ms, LW_FALSE);

        if (PG_IS_SF_ENABLED(Di, DI, REFMPLL))
        {
            diSeqPllPowerup_HAL(&Di, Di.pPllsRefmpll,
                                     Di.pllCountRefmpll);

            //
            // Wait for 2 us for above request to go though
            // before doing any other configuration
            // Reference bug, 1285699  #18,#21
            //
            OS_PTIMER_SPIN_WAIT_NS(2000);
        }

        if (PG_IS_SF_ENABLED(Di, DI, DRAMPLL))
        {
            diSeqPllPowerup_HAL(&Di, Di.pPllsDrampll,
                                     Di.pllCountDrampll);
        }

        // DRAM Clock should already be in bypass path. Gate it from bypass path.
        msSwAsrDramClockGateFromBypass_HAL(&Ms, LW_TRUE);

        //
        // STEP B:
        // disable main regulator   // MS will powerup regulator later
        //

        // Disable main regulator LW_PTRIM_SYS_FBIO_SPARE_FBIO_SPARE = 0
        REG_WR32(FECS, LW_PTRIM_SYS_FBIO_SPARE, 0);

    }   // end of GC5 exit

    //
    // Read any sys register to avoid an auto-injected host2fecs fence. This
    // access has to use host path. Exci register used here, can be replaced
    // with any other sys register.
    //
    REG_RD32(BAR0, LW_PPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_EXCI));
}
