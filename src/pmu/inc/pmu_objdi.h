/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJDI_H
#define PMU_OBJDI_H

/*!
 * @file pmu_objdi.h
 * @Brief DI means Deep Idle. 
 *
 * OBJDI is responsible for handling PG_ENG_CTRL
 * based Deep Idle and common Deep Idle HW entry/exit sequence.
 */

/* ------------------------ System includes -------------------------------- */
#include "flcntypes.h"
#include "pmu_objpg.h"

/* ---------------------- Forward Declarations ----------------------------- */
typedef struct DI_PLL        DI_PLL;
typedef struct DI_SEQ_CACHE  DI_SEQ_CACHE;
typedef struct DI_REG_CACHE  DI_REG_CACHE;

/* ------------------------ Application includes --------------------------- */
#include "config/g_di_hal.h"

/* ------------------------ Macros & Defines ------------------------------- */

/*!
 * @Brief List of all Available PLL Types. 
 *
 * Every PLL Type is associated with a unique PLL Sequence.
 * TYPE_NONE => PLL should not be powered down / up.
 */
enum
{
    DI_PLL_TYPE_NONE            ,
    DI_PLL_TYPE_LEGACY_CORE     ,
    DI_PLL_TYPE_REFMPLL         ,
    DI_PLL_TYPE_DRAMPLL         ,
    DI_PLL_TYPE_LEGACY_SPPLL    ,
    DI_PLL_TYPE_LEGACY_XTAL4X   ,
    DI_PLL_TYPE_NAPLL           ,
    DI_PLL_TYPE_NAFLL           ,
    DI_PLL_TYPE_PLL16G          ,
    DI_PLL_TYPE_HPLL            ,
};

/*!
 * @brief Enumeration to represent each DI step
 *
 * _INIT                  - Initial step. No actions have been taken for DI
 *                          entry.
 * _PREPARATIONS          - Preparing GPU for DI state. Here we initialize
 *                          internal data structures of DI.
 * _MEMORY                - Put FB into Self refresh if MSCG haven't activated
 *                          self-refresh. Power down DRAMPLL and REFMPLL.
 * _CHILD_PLLS            - Shutdown child plls. Child PLLs are GPCPLL, LTCPLL,
 *                          XBARPLL, SYSPLL, VPLL, DISPPLL
 * _OSM_CLOCKS            - Stop clocks from OSM. Only PWR, UTILS and SYS clocks
 *                          are alive.
 * _UTILS_STATE           - Saving the state on UTILS clock domain before switch
 *                          UTILS clock from SPPLLs to XTAL.
 * _PARENT_PLL_SHUTDOWN   - Switch SYS, PWR and UTILS clocks to XTAL. Turn off
 *                          SPPLLs and XTAL4X.
 */
enum
{
    DI_SEQ_STEP_INIT        ,
    DI_SEQ_STEP_PREPARATIONS,
    DI_SEQ_STEP_MEMORY      ,
    DI_SEQ_STEP_CHILD_PLLS  ,
    DI_SEQ_STEP_OSM_CLOCKS  ,
    DI_SEQ_STEP_UTILS_STATE ,
    DI_SEQ_STEP_PARENT_PLLS ,
};

/*!
 * @brief Last step in DI.
 * PARENT_PLLS is last sub-state in DI sequence.
 */
#define DI_SEQ_STEP_LAST    (DI_SEQ_STEP_PARENT_PLLS)

/*!
 * @brief Abort reasons for common DI sequence
 *
 * DI abort logging is divided into two groups
 * 1) Aborts in common DI sequence
 * 2) Aborts in state machine - Legacy or PG_ENG
 *
 * DI_SEQ_ABORT_REASON_MASK_xyz represent aborts in common DI sequence.
 *
 * Legacy DI State machine don't have abort logging mechanism. Thus we have
 * reserved one bit in DI_SEQ_ABORT_REASON_MASK_ to log aborts in Legacy DI SM.
 * Ideally like PG_ENG, Legacy SM should have its own infrastructure to log
 * aborts.
 *
 * _DISP_HEAD           - Aborted because display head is active for no-head
 *                        use-case.
 * _DISP_AZALIA         - Aborted because Azalia control is not idle
 * _DISP_ON_VCO_PATH    - Aborted because DISPCLK is on VCO path
 * _HOST                - Aborted because host is not idle
 * _INTR_GPU            - Aborted because interrupt pending on LW_PMC_INTR0/1
 *                        line.
 * _INTR_SCI            - Aborted because SCI interrupt is pending
 * _THERM_I2C_SECPNDARY - Aborted because I2CS is not idle
 * _PTIMER_ALARM        - Aborted because there is pending PTIMER alarm
 * _RTOS_CALLBACK       - Aborted because there is pending RTOS callback
 * _DEEP_L1             - Aborted as DeepL1 exited at end of DI sequence
 * _SW_WAKE             - Aborted because there is event in PG queue
 * _SCI_SM              - Abort in SCI SM
 * _LEGACY              - Aborts from Legacy DI state machine. Legacy DI SM uses
 *                        common DI infra to notify abort. This is HACK to catch
 *                        aborts in Legacy DI. Ideally like PG_ENG, Legacy SM
 *                        should have its own infrastructure to log aborts
 */
#define DI_SEQ_ABORT_REASON_MASK_DISP_HEAD                      BIT(0)
#define DI_SEQ_ABORT_REASON_MASK_DISP_AZALIA                    BIT(1)
#define DI_SEQ_ABORT_REASON_MASK_DISP_ON_VCO_PATH               BIT(2)
#define DI_SEQ_ABORT_REASON_MASK_HOST                           BIT(3)
#define DI_SEQ_ABORT_REASON_MASK_INTR_GPU                       BIT(4)
#define DI_SEQ_ABORT_REASON_MASK_INTR_SCI                       BIT(5)
#define DI_SEQ_ABORT_REASON_MASK_THERM_I2C_SECONDARY            BIT(6)
#define DI_SEQ_ABORT_REASON_MASK_PTIMER_ALARM                   BIT(7)
#define DI_SEQ_ABORT_REASON_MASK_RTOS_CALLBACK                  BIT(8)
#define DI_SEQ_ABORT_REASON_MASK_DEEP_L1                        BIT(9)
#define DI_SEQ_ABORT_REASON_MASK_SCI_SM                         BIT(10)
#define DI_SEQ_ABORT_REASON_MASK_LEGACY                         BIT(31)

/*!
 * @brief Macro to update abort reason for PG_ENG based DI.
 * Macro accepts name of abort reason.
 */
#define DI_SEQ_ABORT_REASON_UPDATE_NAME(reasonName)                             \
    do                                                                          \
    {                                                                           \
        Di.abortReasonMask |= DI_SEQ_ABORT_REASON_MASK_##reasonName;            \
    } while (LW_FALSE)

/*!
 * @brief Offset to start generic abort reasons
 */
#define DI_PG_ENG_ABORT_REASON_STEP_OFFSET_MAX                          15

/*!
 * @brief Abort reasons for PG_ENG based DI
 *
 * DI abort logging is divided into two groups
 * 1) Aborts in common DI sequence
 * 2) Aborts in state machine - Legacy or PG_ENG
 *
 * DI_PG_ENG_ABORT_REASON_MASK_xyz represent aborts in PG_ENG based DI state
 * machine.
 *
 * _STEP_INIT                   - Aborted before starting core/common DI entry
 *                                sequence.
 * _STEP_PWR_DOWN_PREPARATIONS  - Aborted while exelwting _PWR_DOWN_PREPARATIONS
 *                                step.
 * _STEP_MEMORY                 - Aborted while exelwting _MEMORY step
 * _STEP_CHILD_PLLS             - Aborted while exelwting _CHILD_PLLS step
 * _STEP_OSM_CLOCKS             - Aborted while exelwting _OSM_CLOCKS step
 * _STEP_UTILS_STATE            - Aborted while exelwting _UTILS_STATE step
 * _STEP_PARENT_PLLS            - Aborted while exelwting _PARENT_PLLS step
 * _HW_SM_IDLE_FLIPPED          - Aborted because PG_ENG HW SM notified idle flip
 * _PEX_SLEEP_STATE             - Aborted because we are not in valid pex sleep
 *                                state.
 * _SW_WAKE                     - Aborted because there is event in PG queue
 * _SCI_SEQ                     - Abort SCI entry sequence
 */
#define DI_PG_ENG_ABORT_REASON_MASK_STEP_INIT                   BIT(DI_SEQ_STEP_INIT)
#define DI_PG_ENG_ABORT_REASON_MASK_STEP_PREPARATIONS           BIT(DI_SEQ_STEP_PWR_DOWN_PREPARATIONS)
#define DI_PG_ENG_ABORT_REASON_MASK_STEP_MEMORY                 BIT(DI_SEQ_STEP_MEMORY)
#define DI_PG_ENG_ABORT_REASON_MASK_STEP_CHILD_PLLS             BIT(DI_SEQ_STEP_CHILD_PLLS)
#define DI_PG_ENG_ABORT_REASON_MASK_STEP_OSM_CLOCKS             BIT(DI_SEQ_STEP_OSM_CLOCKS)
#define DI_PG_ENG_ABORT_REASON_MASK_STEP_UTILS_STATE            BIT(DI_SEQ_STEP_UTILS_STATE)
#define DI_PG_ENG_ABORT_REASON_MASK_STEP_PARENT_PLLS            BIT(DI_SEQ_STEP_PARENT_PLLS)
#define DI_PG_ENG_ABORT_REASON_MASK_HW_SM_IDLE_FLIPPED          BIT(DI_PG_ENG_ABORT_REASON_STEP_OFFSET_MAX + 1)
#define DI_PG_ENG_ABORT_REASON_MASK_PEX_SLEEP_STATE             BIT(DI_PG_ENG_ABORT_REASON_STEP_OFFSET_MAX + 2)
#define DI_PG_ENG_ABORT_REASON_MASK_SW_WAKE                     BIT(DI_PG_ENG_ABORT_REASON_STEP_OFFSET_MAX + 3)
#define DI_PG_ENG_ABORT_REASON_MASK_SCI_SEQ                     BIT(DI_PG_ENG_ABORT_REASON_STEP_OFFSET_MAX + 4)

/*!
 * @brief Macro to update abort reason for PG_ENG based DI.
 * Macro accepts name of abort reason.
 */
#define DI_PG_ENG_ABORT_REASON_UPDATE_NAME(reasonName)                          \
    do                                                                          \
    {                                                                           \
        GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_DI)->stats.abortReason |=       \
                                    DI_PG_ENG_ABORT_REASON_MASK_##reasonName;   \
    } while (LW_FALSE)

/*!
 * @brief Macro to update abort reason for PG_ENG based DI.
 * Macro accepts value of abort reason.
 */
#define DI_PG_ENG_ABORT_REASON_UPDATE_VAL(reasolwal)                                  \
    do                                                                                \
    {                                                                                 \
        GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_DI)->stats.abortReason |= reasolwal;  \
    } while (LW_FALSE)

/*!
 * @brief PEX Sleep state triggering DI entry
 * DI entry can be trigger by DeepL1, L1.1 or L1.2.
 *
 * _DEEP_L1 - DeepL1 feature engaged
 * _L1_1    - ASPM L1.1 engaged
 * _L1_2    - ASPM L1.2 engaged
 * _NONE    - GPU is not in any of PEX sleep state. PG_ENG(DI) will report
 *            _NONE state if PG_ENG(DI) is ignoring PEX sleep state. Its only
 *            use for debug purpose on GP10X.
 * _ILWALID - Used to invalid state.
 */
#define DI_PEX_SLEEP_STATE_DEEP_L1                  BIT(0)
#define DI_PEX_SLEEP_STATE_L1_1                     BIT(1)
#define DI_PEX_SLEEP_STATE_L1_2                     BIT(2)
#define DI_PEX_SLEEP_STATE_NONE                     BIT(3)
#define DI_PEX_SLEEP_STATE_ILWALID                  BIT(4)

/*!
 * @brief Macro to define L1 Sub-State.
 * PEX is in L1SS if its in L1.1 or L1.2
 */
#define DI_PEX_SLEEP_STATE_L1SS                     \
        (                                           \
            DI_PEX_SLEEP_STATE_L1_1                |\
            DI_PEX_SLEEP_STATE_L1_2                 \
        )

/*!
 * @brief Macro to define mask of valid pex sleep states
 */
#define DI_PEX_SLEEP_STATE_VALID                    \
        (                                           \
            DI_PEX_SLEEP_STATE_DEEP_L1             |\
            DI_PEX_SLEEP_STATE_L1_1                |\
            DI_PEX_SLEEP_STATE_L1_2                 \
        )

/*!
 * @brief Macro to check engagement of PEX sleep state
 */
#define DI_IS_PEX_SLEEP_STATE(state, mask)          \
        ((DI_PEX_SLEEP_STATE_##state & mask) != 0)

/*!
 * @brief Returns Common DI Sequence cache
 */
#define DI_SEQ_GET_CAHCE()                          (Di.pCache)

/*!
 * @brief Max number of active PEX lanes in DI mode
 */
#define DI_PEX_LANE_MAX                            16

/*!
 * @brief Maximum number of PLLs to be powered down during PLL powerdown
 */
#define DI_CORE_PLLS_MAX                            4

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
#define DI_SEQ_PLL_POWERDOWN_VPLL_STEPMASK_LEVEL_0_GM10X                                \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_VPLL, _POWERDOWN, _ASSERT_SETUP_STATUS        )   |\
            DI_SEQ_PLL_STEPMASK_GET(_VPLL, _POWERDOWN, _SEL_VCO_LDIV_ENABLE        )   |\
            DI_SEQ_PLL_STEPMASK_GET(_VPLL, _POWERDOWN, _SETUP_OVERRIDE             )   |\
            DI_SEQ_PLL_STEPMASK_GET(_VPLL, _POWERDOWN, _SEL_BYPASS_LDIV            )   |\
            DI_SEQ_PLL_STEPMASK_GET(_VPLL, _POWERDOWN, _BYPASSPLL_SYNC_DISABLE_IDDQ)    \
        )

#define DI_SEQ_PLL_POWERUP_VPLL_STEPMASK_LEVEL_0_GM10X                                  \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_VPLL, _POWERUP, _ASSERT_STATUS)                   |\
            DI_SEQ_PLL_STEPMASK_GET(_VPLL, _POWERUP, _IDDQ_ON      )                    \
        )

#define DI_SEQ_PLL_POWERUP_VPLL_STEPMASK_LEVEL_1_GM10X                                  \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_VPLL, _POWERUP, _WAIT_PLL_READY)                  |\
            DI_SEQ_PLL_STEPMASK_GET(_VPLL, _POWERUP, _ENABLE        )                   \
        )

#define DI_SEQ_PLL_POWERUP_VPLL_STEPMASK_LEVEL_2_GM10X                                  \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_VPLL, _POWERUP, _WAIT_PLL_LOCK         )          |\
            DI_SEQ_PLL_STEPMASK_GET(_VPLL, _POWERUP, _SYNC_BYPASSPLL_SEL_VCO)           \
        )

#define DI_SEQ_PLL_POWERDOWN_SORPLL_STEPMASK_LEVEL_0_GM10X                              \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_SORPLL, _POWERDOWN, _DP_CLK_SAFE         )        |\
            DI_SEQ_PLL_STEPMASK_GET(_SORPLL, _POWERDOWN, _PDPORT_BGAP_PLLCAPPD)        |\
            DI_SEQ_PLL_STEPMASK_GET(_SORPLL, _POWERDOWN, _PWROFF_VCOPD_ASSERT )         \
        )

#define DI_SEQ_PLL_POWERUP_SORPLL_STEPMASK_LEVEL_0_GM10X                                \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_SORPLL, _POWERUP, _BGAP_PDN_DISABLE)               \
        )

#define DI_SEQ_PLL_POWERUP_SORPLL_STEPMASK_LEVEL_1_GM10X                                \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_SORPLL, _POWERUP, _WAIT_BGAP_DELAY      )         |\
            DI_SEQ_PLL_STEPMASK_GET(_SORPLL, _POWERUP, _PLLCAPPD_PWR_ON_VCOPD)          \
        )

#define DI_SEQ_PLL_POWERUP_SORPLL_STEPMASK_LEVEL_2_GM10X                                \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_SORPLL, _POWERUP, _WAIT_PLL_LOCK             )    |\
            DI_SEQ_PLL_STEPMASK_GET(_SORPLL, _POWERUP, _PORT_PDN_DISABLE_DP_NORMAL)     \
        )

#define DI_SEQ_PLL_POWERDOWN_DRAMPLL_STEPMASK_LEVEL_0_GM10X                             \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERDOWN, _SAVE_CACHE          )        |\
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERDOWN, _LCKDET_OFF          )        |\
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERDOWN, _DISABLE             )        |\
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERDOWN, _DRAMPLL_CLAMP_ASSERT)        |\
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERDOWN, _IDDQ_OFF            )         \
        )

#define DI_SEQ_PLL_POWERUP_DRAMPLL_STEPMASK_LEVEL_0_GM10X                               \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERUP, _IDDQ_ON)                       |\
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERUP, _IDDQ_OFF)                       \
        )

#define DI_SEQ_PLL_POWERUP_DRAMPLL_STEPMASK_LEVEL_1_GM10X                               \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERUP, _WAIT_PLL_READY        )        |\
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERUP, _DRAMPLL_CLAMP_DEASSERT)        |\
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERUP, _ENABLE                )        |\
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERUP, _LCKDET_ON             )         \
        )

#define DI_SEQ_PLL_POWERUP_DRAMPLL_STEPMASK_LEVEL_2_GM10X                               \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERUP, _WAIT_PLL_LOCK)                  \
        )

#define DI_SEQ_PLL_POWERDOWN_REFMPLL_STEPMASK_LEVEL_0_GM10X                             \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERDOWN, _SAVE_CACHE         )         |\
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERDOWN, _SYNC_MODE_DISABLE  )         |\
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERDOWN, _LCKDET_OFF         )         |\
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERDOWN, _DISABLE            )         |\
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERDOWN, _IDDQ_OFF           )          \
        )

#define DI_SEQ_PLL_POWERUP_REFMPLL_STEPMASK_LEVEL_0_GM10X                               \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERUP, _IDDQ_ON)                       |\
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERUP, _IDDQ_OFF)                       \
        )

#define DI_SEQ_PLL_POWERUP_REFMPLL_STEPMASK_LEVEL_1_GM10X                               \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERUP, _WAIT_PLL_READY    )            |\
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERUP, _ENABLE            )            |\
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERUP, _LCKDET_ON         )             \
        )

#define DI_SEQ_PLL_POWERUP_REFMPLL_STEPMASK_LEVEL_2_GM10X                               \
        (                                                                               \
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERUP, _WAIT_PLL_LOCK   )              |\
            DI_SEQ_PLL_STEPMASK_GET(_MEMORY, _POWERUP, _SYNC_MODE_ENABLE)               \
        )

/*!
 * @brief Stepmask Macros for Common PLL Infra
 */

// Core Legacy Plls
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_LEGACY_CORE_POWERDOWN_SYNC_MODE_DISABLE        BIT(0)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_LEGACY_CORE_POWERDOWN_IDDQ_OFF_LCKDET_DISABLE  BIT(1)

#define DI_SEQ_PLL_SEQUENCE_STEPMASK_LEGACY_CORE_POWERUP_IDDQ_ON                    BIT(0)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_LEGACY_CORE_POWERUP_WAIT_PLL_READY             BIT(1)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_LEGACY_CORE_POWERUP_LCKDET_ENABLE              BIT(2)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_LEGACY_CORE_POWERUP_WAIT_PLL_LOCK              BIT(3)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_LEGACY_CORE_POWERUP_SYNC_MODE_ENABLE           BIT(4)

// NAPLLs
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_NAPLL_POWERDOWN_SYNC_MODE_DISABLE              BIT(0)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_NAPLL_POWERDOWN_IDDQ_OFF_LCKDET_DISABLE        BIT(1)

#define DI_SEQ_PLL_SEQUENCE_STEPMASK_NAPLL_POWERUP_IDDQ_ON                          BIT(0)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_NAPLL_POWERUP_WAIT_PLL_READY                   BIT(1)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_NAPLL_POWERUP_OUTPUT_ENABLE                    BIT(2)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_NAPLL_POWERUP_WAIT_PLL_LOCK                    BIT(3)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_NAPLL_POWERUP_SYNC_MODE_LCK_OVRD               BIT(4)

// Core NAFLLs
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_NAFLL_POWERDOWN_IDDQ_OFF_DISABLE               BIT(0)

#define DI_SEQ_PLL_SEQUENCE_STEPMASK_NAFLL_POWERUP_IDDQ_ON                          BIT(0)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_NAFLL_POWERUP_WAIT_FLL_READY                   BIT(1)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_NAFLL_POWERUP_ENABLE                           BIT(2)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_NAFLL_POWERUP_WAIT_FLL_LOCK                    BIT(3)

// Legacy XTAL4X PLL
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_LEGACY_XTAL4X_POWERDOWN_ALL                    BIT(0)

#define DI_SEQ_PLL_SEQUENCE_STEPMASK_LEGACY_XTAL4X_POWERUP_ALL                      BIT(0)

// Legacy Display PLLs (SORPLL, VPLL)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_VPLL_POWERDOWN_ASSERT_SETUP_STATUS             BIT(0)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_VPLL_POWERDOWN_SEL_VCO_LDIV_ENABLE             BIT(1)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_VPLL_POWERDOWN_SETUP_OVERRIDE                  BIT(2)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_VPLL_POWERDOWN_SEL_BYPASS_LDIV                 BIT(3)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_VPLL_POWERDOWN_BYPASSPLL_SYNC_DISABLE_IDDQ     BIT(4)

#define DI_SEQ_PLL_SEQUENCE_STEPMASK_VPLL_POWERUP_ASSERT_STATUS                     BIT(0)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_VPLL_POWERUP_IDDQ_ON                           BIT(1)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_VPLL_POWERUP_WAIT_PLL_READY                    BIT(2)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_VPLL_POWERUP_ENABLE                            BIT(3)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_VPLL_POWERUP_WAIT_PLL_LOCK                     BIT(4)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_VPLL_POWERUP_SYNC_BYPASSPLL_SEL_VCO            BIT(5)

#define DI_SEQ_PLL_SEQUENCE_STEPMASK_SORPLL_POWERDOWN_DP_CLK_SAFE                   BIT(0)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_SORPLL_POWERDOWN_PDPORT_BGAP_PLLCAPPD          BIT(1)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_SORPLL_POWERDOWN_PWROFF_VCOPD_ASSERT           BIT(2)

#define DI_SEQ_PLL_SEQUENCE_STEPMASK_SORPLL_POWERUP_BGAP_PDN_DISABLE                BIT(0)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_SORPLL_POWERUP_WAIT_BGAP_DELAY                 BIT(1)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_SORPLL_POWERUP_PLLCAPPD_PWR_ON_VCOPD           BIT(2)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_SORPLL_POWERUP_WAIT_PLL_LOCK                   BIT(3)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_SORPLL_POWERUP_PORT_PDN_DISABLE_DP_NORMAL      BIT(4)

// Legacy Memory Plls (REFMPLL, DRAMPLL)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_MEMORY_POWERDOWN_SAVE_CACHE                    BIT(0)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_MEMORY_POWERDOWN_SYNC_MODE_DISABLE             BIT(1)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_MEMORY_POWERDOWN_LCKDET_OFF                    BIT(2)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_MEMORY_POWERDOWN_DISABLE                       BIT(3)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_MEMORY_POWERDOWN_DRAMPLL_CLAMP_ASSERT          BIT(4)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_MEMORY_POWERDOWN_IDDQ_OFF                      BIT(5)

#define DI_SEQ_PLL_SEQUENCE_STEPMASK_MEMORY_POWERUP_IDDQ_ON                         BIT(0)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_MEMORY_POWERUP_WAIT_PLL_READY                  BIT(1)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_MEMORY_POWERUP_DRAMPLL_CLAMP_DEASSERT          BIT(2)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_MEMORY_POWERUP_ENABLE                          BIT(3)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_MEMORY_POWERUP_LCKDET_ON                       BIT(4)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_MEMORY_POWERUP_WAIT_PLL_LOCK                   BIT(5)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_MEMORY_POWERUP_SYNC_MODE_ENABLE                BIT(6)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_MEMORY_POWERUP_IDDQ_OFF                        BIT(7)

// Legacy SPPLLs
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_LEGACY_SPPLL_POWERDOWN_LCKDET_OFF              BIT(0)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_LEGACY_SPPLL_POWERDOWN_BYPASSPLL               BIT(1)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_LEGACY_SPPLL_POWERDOWN_DISABLE                 BIT(2)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_LEGACY_SPPLL_POWERDOWN_DEACTIVATE_SSA          BIT(3)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_LEGACY_SPPLL_POWERDOWN_IDDQ_OFF                BIT(4)

#define DI_SEQ_PLL_SEQUENCE_STEPMASK_LEGACY_SPPLL_POWERUP_IDDQ_ON                   BIT(0)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_LEGACY_SPPLL_POWERUP_WAIT_PLL_READY            BIT(1)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_LEGACY_SPPLL_POWERUP_LCKDET_ON                 BIT(2)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_LEGACY_SPPLL_POWERUP_ENABLE                    BIT(3)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_LEGACY_SPPLL_POWERUP_WAIT_PLL_LOCK             BIT(4)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_LEGACY_SPPLL_POWERUP_ACTIVATE_SSA              BIT(5)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_LEGACY_SPPLL_POWERUP_BYPASSPLL                 BIT(6)

// HPLL (XTAL4X PLL for GP10X_and_later)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_HPLL_POWERDOWN_IDDQ_OFF                        BIT(0)

#define DI_SEQ_PLL_SEQUENCE_STEPMASK_HPLL_POWERUP_IDDQ_ON                           BIT(0)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_HPLL_POWERUP_WAIT_CLK_GOOD_TRUE                BIT(1)

// PLL16G (SPPLL, Display PLLs for Pascal)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_PLL16G_POWERDOWN_LCKDET_OFF                    BIT(0)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_PLL16G_POWERDOWN_BYPASSPLL                     BIT(1)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_PLL16G_POWERDOWN_DISABLE                       BIT(2)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_PLL16G_POWERDOWN_DEACTIVATE_SSD                BIT(3)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_PLL16G_POWERDOWN_IDDQ_OFF                      BIT(4)

#define DI_SEQ_PLL_SEQUENCE_STEPMASK_PLL16G_POWERUP_IDDQ_ON                         BIT(0)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_PLL16G_POWERUP_WAIT_PLL_READY                  BIT(1)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_PLL16G_POWERUP_LCKDET_ON                       BIT(2)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_PLL16G_POWERUP_ENABLE                          BIT(3)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_PLL16G_POWERUP_WAIT_PLL_LOCK                   BIT(4)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_PLL16G_POWERUP_ACTIVATE_SSD                    BIT(5)
#define DI_SEQ_PLL_SEQUENCE_STEPMASK_PLL16G_POWERUP_BYPASSPLL                       BIT(6)

/*!
 * @brief : Macros to get / check for Individual StepMasks for different
 *          PLL Powerdown / Powerup Steps.
 */
#define DI_SEQ_PLL_STEPMASK_GET(pllType, powerAction, step)                   \
        ((LwU16) (DI_SEQ_PLL_SEQUENCE_STEPMASK##pllType##powerAction##step))

#define DI_SEQ_PLL_STEPMASK_IS_SET(pllType, powerAction, step, stepMask)      \
        (((DI_SEQ_PLL_STEPMASK_GET(pllType, powerAction, step)) & stepMask) != 0)

/*!
 * @brief Maximum number of SPPLLS supported
 */
#define DI_SPPLLS_MAX                               2

/*!
 * @brief Maximum Delay for various PLL Actions
 * @Description : These Delays are obtained after dislwssion with HW
 *                team. Reference bug : 200019152
 *                PMU should assert if these timeouts are violated.
 */
#define DI_SEQ_PLL_LOCK_DETECTION_DELAY_DEFAULT_MAX_US  (100)
#define DI_SEQ_PLL_LOCK_DETECTION_DELAY_NAPLL_MAX_US    (40)
#define DI_SEQ_FLL_LOCK_DETECTION_DELAY_NAFLL_MAX_US    (5)
#define DI_SEQ_PLL_READY_DELAY_DEFAULT_MAX_US_GMXXX     (20)
#define DI_SEQ_PLL_HPLL_CLK_GOOD_TRUE_DELAY_MAX_NS      (20000)
#define DI_SEQ_PLL_READY_DELAY_DEFAULT_MAX_US_GP10X     (5)
#define DI_SEQ_ADC_BGAP_READY_DELAY_MAX_US              (1)
#define DI_SEQ_VOLT_SETTLE_TIME_US                      (Di.voltageSettleTimeUs)

/*!
 * @brief Macro to check for PLL_IDDQ sub-feature
 * as well as SW_POWER_DOWN PMU config feature
 */
#define DI_IS_PLL_IDDQ_ENABLED()                                         \
        (PMUCFG_FEATURE_ENABLED(PMU_PG_SW_POWER_DOWN) &&                 \
         PG_IS_SF_ENABLED(Di, DI, PLL_IDDQ))

/* ------------------------ Types definitions ------------------------------ */

/*!
 * @Brief Following structure defines a generic PLL.
 *
 * These are WRT the new PLL PDN / PUP Design 
 * introduced for Maxwell and above.
 */
struct DI_PLL
{
    // PLL register address
    LwU32   regAddr;

    // Cached PLL register value to be used across entry/exit
    LwU32   cacheVal;

    // PLL type used to determine the sequence to execute
    LwU8    pllType;
};

/*!
 * @brief Structure defines register address and data pair
 */
struct DI_REG_CACHE
{
    // Register address
    LwU32 regAddr;

    // Cached register value to be used across entry/exit
    LwU32 cacheVal;
};


/*!
 * @brief DI sequence cache
 *
 * It used to cache data across DI cycles. We also use this cache to pass data
 * from one DI step to another.
 */
struct DI_SEQ_CACHE
{
    // Time spent in SCI entry
    LwU32   sciEntryLatencyUs;

    // Time spent in SCI exit
    LwU32   sciExitLatencyUs;

    // DI resident time
    LwU32   residentTimeUs;

    //
    // Stores LW_PMC_INTR0/1 and SCI_SW_INTR0/1. ARCH test use them for post
    // processing.
    //
    RM_PMU_DIDLE_INTR_STATUS    intrStatus;

    // DI Exit Start Time
    LwU32   diExitStartTimeLo;

    //
    // Cache PEX DFE values across DI cycles. We need to cache values in case
    // we are gating PEX rails in DI.
    //
    LwU8    pexDFEVal[DI_PEX_LANE_MAX];

    //
    // Lowest active PEX lane
    //
    // We find Lowest active lane in first stage of DI entry sequence where all
    // clocks are alive and running at full speed. We use value in last stage
    // of DI sequence. where almost all clocks are gated.
    //
    LwU32   lowestPexActiveLaneBit;

    //
    // Cache contents of PEX pad CTRL.
    //
    // CTL_LANE_SEL is used to select lowest active lane
    // CTL_4 is used to force auxiliary idle circuit.
    //
    LwU32   pexPadCtlLaneSel;
    LwU32   pexPadCtl4;

    // Cache settings of DeepL1 Wakeups.
    LwU32   deepL1WakeEn;

    // Cache settings of SPPLL config
    LwU32   sppllCfg2Reg[DI_SPPLLS_MAX];
    LwU32   sppllSSAReg[DI_SPPLLS_MAX];
    LwU32   sppllBypassReg;

    // Cache settings of SYS clock out switch
    LwU32   sys2ClkCfgFinal;

    // Cache settings of thermal sensor. DI shutdown thermal sensor.
    LwU32   thermSensorReg;

    // I2C Secondary Address
    LwU8    i2cSecondaryAddress;
};

/*!
 * @Brief OBJDI manages Deep Idle sequence and PG_ENG_CTRL based Deep Idle
 *        feature
 */
typedef struct
{
    // Cache to save and restore state of GPU across Deep Idle.
    DI_SEQ_CACHE   *pCache;

    // PLL Specific List for each PLL Class
    DI_PLL         *pPllsCore;

    // Register cache for NAFLL_CTRL registers
    LwU32           *pNafllCtrlReg;

    DI_PLL         *pPllsXtal4x;
    DI_PLL         *pPllsRefmpll;
    DI_PLL         *pPllsDrampll;
    DI_PLL         *pPllsSppll;

    // PLL Count for All PLLs
    LwU8            pllCountCore;
    LwU8            pllCountXtal4x;
    LwU8            pllCountRefmpll;
    LwU8            pllCountDrampll;
    LwU8            pllCountSppll;

    // Specifies if wakeup timer is enabled or not
    LwBool          bWakeupTimer;

    // Mode of wakeup timer (AUTO / FORCE)
    LwBool          bModeAuto;

    // LW_TRUE when PMU has set valid Sleep Voltage
    LwBool          bSleepVoltValid;

    // Set when voltage feature is supported.
    LwBool          bVoltSupport;

    // List of clocks running from XTAL when SPPLLs are shut down
    LwU8            clkRoutedToXtalListSize;
    DI_REG_CACHE   *pClkRoutedToXtal;

    // Maximum Sleep time for DI
    LwU32           maxSleepTimeUs;

    // PEX sleep state - DI_PEX_SLEEP_STATE_xyz
    LwU32           pexSleepState;

    // Mask of abort reasons
    LwU32           abortReasonMask;

    // Feature Support Mask in DI
    LwU32           supportMask;

    // Bitmask of enabled Sub features of DI
    LwU32          enabledMask;

    // Clock mask for DI Clock gating
    LwU32           cgMask;

    // Determines wheather XTAL4XPLL is blown or not
    LwBool          bXtal4xFuseBlown;

    // Voltage Settle time for DI
    LwU16           voltageSettleTimeUs;
} OBJDI;
/* ------------------------ External definitions --------------------------- */
extern OBJDI Di;

/* ------------------------ Function Prototypes ---------------------------- */
void diPreInit(void)
    GCC_ATTRIB_SECTION("imem_init", "diPreInit");
void diSeqGpuReadyEventSend(LwBool bSuccessfulEntry, LwU32 abortReasonMaskSm);

#endif // PMU_OBJDI_H
