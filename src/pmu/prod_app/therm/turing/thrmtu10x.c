/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    thrmtu10x.c
 * @brief   Thermal PMU HAL functions for TU10X+
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"

#include "config/g_therm_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

#ifdef LW_CPWR_THERM_MAXFAN_STATUS_SMARTFAN_SCI
/*!
 * @brief   Gets HW MAXFAN state (active/inactive) for the requested PWM source.
 *
 * @param[in] pwmSource   Requested PWM source as RM_PMU_PMGR_PWM_SOURCE_<xyz>
 *
 * @return  Boolean flag indicating HW MAXFAN status.
 */
LwBool
thermHwMaxFanStatusGet_TU10X
(
    RM_PMU_PMGR_PWM_SOURCE  pwmSource
)
{
    LwBool  bStatus = LW_FALSE;
    LwU32   regVal  = REG_RD32(CSB, LW_CPWR_THERM_MAXFAN);

    if (pwmSource == RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_0)
    {
        bStatus = FLD_TEST_DRF(_CPWR_THERM, _MAXFAN, _STATUS_SMARTFAN_SCI,
                                _PENDING, regVal);
    }
    else if (pwmSource == RM_PMU_PMGR_PWM_SOURCE_THERM_PWM)
    {
        bStatus = FLD_TEST_DRF(_CPWR_THERM, _MAXFAN, _STATUS_SMARTFAN_THERM,
                                _PENDING, regVal);
    }

    return bStatus;
}

/*!
 * @brief   Activate/Deactivate HW MAXFAN status for the requested PWM source.
 *
 * @param[in] pwmSource Requested PWM source as RM_PMU_PMGR_PWM_SOURCE_<xyz>
 * @param[in] bSet      Boolean flag to activate or deactivate.
 */
void
thermHwMaxFanStatusSet_TU10X
(
    RM_PMU_PMGR_PWM_SOURCE  pwmSource,
    LwBool                  bSet
)
{
    LwU32 regVal = REG_RD32(CSB, LW_CPWR_THERM_MAXFAN);

    if (pwmSource == RM_PMU_PMGR_PWM_SOURCE_SCI_FAN_0)
    {
        regVal = bSet ?
                    FLD_SET_DRF(_CPWR_THERM, _MAXFAN, _FORCE, _TRIGGER, regVal) :
                    FLD_SET_DRF(_CPWR_THERM, _MAXFAN, _STATUS_SMARTFAN_SCI,
                                _CLEAR, regVal);
    }
    else if (pwmSource == RM_PMU_PMGR_PWM_SOURCE_THERM_PWM)
    {
        regVal = bSet ?
                    FLD_SET_DRF(_CPWR_THERM, _MAXFAN, _FORCE, _TRIGGER, regVal) :
                    FLD_SET_DRF(_CPWR_THERM, _MAXFAN, _STATUS_SMARTFAN_THERM,
                                _CLEAR, regVal);
    }

    REG_WR32(CSB, LW_CPWR_THERM_MAXFAN, regVal);
}
#endif

/*!
 * @brief Load tach functionality (init HW state).
 *
 * @param[in]   tachSource  Requested TACH source as PMU_PMGR_TACH_SOURCE_<xyz>
 * @param[in]   tachRate    Tachometer Rate (PPR).
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid TACH source passed as an input argument.
 * @return FLCN_OK
 *      Tachometer loaded successfully.
 */
FLCN_STATUS
thermTachLoad_TU10X
(
    PMU_PMGR_TACH_SOURCE    tachSource,
    LwU8                    tachRate
)
{
    FLCN_STATUS status = FLCN_ERR_ILWALID_ARGUMENT;

    if ((tachSource == PMU_PMGR_THERM_TACH_SOURCE_0) ||
        (tachSource == PMU_PMGR_THERM_TACH_SOURCE_1))
    {
        LwU32   idx = (LwU32)tachSource - (LwU32)PMU_PMGR_THERM_TACH_SOURCE_0;
        LwU32   tachA;

        tachA = REG_RD32(CSB, LW_CPWR_THERM_FAN_TACHA(idx));
        HIGHESTBITIDX_32(tachRate)
        tachA = FLD_SET_DRF_NUM(_CPWR_THERM, _FAN_TACHA, _WIN_LENGTH,
                    tachRate, tachA);
        REG_WR32(CSB, LW_CPWR_THERM_FAN_TACHA(idx), tachA);

        status = FLCN_OK;
    }

    return status;
}

/*!
 * @brief   Get tachometer frequency as edgecount per sec.
 *
 * Reading is to be interpreted as numerator / denominator.
 * Returning this way to avoid truncation issues.
 *
 * @param[in]   tachSource      Requested TACH source as PMU_PMGR_TACH_SOURCE_<xyz>
 * @param[out]  pNumerator      Edgecount per sec numerator
 * @param[out]  pDenominator    Edgecount per sec denominator
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid TACH source passed as an input argument.
 * @return FLCN_OK
 *      Tachometer frequency obtained successfully.
 */
FLCN_STATUS
thermGetTachFreq_TU10X
(
    PMU_PMGR_TACH_SOURCE    tachSource,
    LwU32                  *pNumerator,
    LwU32                  *pDenominator
)
{
    FLCN_STATUS status = FLCN_ERR_ILWALID_ARGUMENT;

    if ((tachSource == PMU_PMGR_THERM_TACH_SOURCE_0) ||
        (tachSource == PMU_PMGR_THERM_TACH_SOURCE_1))
    {
        LwU32   idx = (LwU32)tachSource - (LwU32)PMU_PMGR_THERM_TACH_SOURCE_0;
        LwU32   tachA;

        tachA = REG_RD32(CSB, LW_CPWR_THERM_FAN_TACHA(idx));

        //
        // Reported period is in units of 1 microseconds. From GM20X, HW is capable
        // of monitoring long periods as specified by window length so pre-multiply
        // numerator by window length to account for tach rate division done by SW
        // while callwlating edgecount per second.
        //
        *pNumerator =
            1000000U << DRF_VAL(_CPWR_THERM, _FAN_TACHA, _WIN_LENGTH, tachA);

        if (FLD_TEST_DRF(_CPWR_THERM, _FAN_TACHA, _OVERFLOW, _DETECTED, tachA))
        {
            // Fan is stalled, clear state and report it.
            REG_WR32(CSB, LW_CPWR_THERM_FAN_TACHA(idx),
                FLD_SET_DRF(_CPWR_THERM, _FAN_TACHA, _OVERFLOW, _CLEAR, tachA));
            *pDenominator = 0;
        }
        else if (FLD_TEST_DRF(_CPWR_THERM, _FAN_TACHA, _PERIOD, _MIN, tachA) ||
                 FLD_TEST_DRF(_CPWR_THERM, _FAN_TACHA, _PERIOD, _MAX, tachA))
        {
            //
            // Check for boundary conditions. Fan tachometer reporting MAX
            // can be treated as garbage.
            //
            *pDenominator = 0;
        }
        else
        {
            // Now obtain the legitimate value.
            *pDenominator = DRF_VAL(_CPWR_THERM, _FAN_TACHA, _PERIOD, tachA);
        }

        status = FLCN_OK;
    }

    return status;
}

/*!
 * @brief   Initialize static GPU dependant Event Violation data
 */
void
thermEventViolationPreInit_TU10X(void)
{
    //
    // Declaring within HAL interface to reduce visibility.
    //
    // Size is determined as (in given order):
    //  - 3 EXT/GPIO events:
    //      [0x00] - RM_PMU_THERM_EVENT_EXT_OVERT
    //      [0x01] - RM_PMU_THERM_EVENT_EXT_ALERT
    //      [0x02] - RM_PMU_THERM_EVENT_EXT_POWER
    //  - 12 thermal events:
    //      [0x03] - RM_PMU_THERM_EVENT_THERMAL_0
    //      [0x04] - RM_PMU_THERM_EVENT_THERMAL_1
    //      [0x05] - RM_PMU_THERM_EVENT_THERMAL_2
    //      [0x06] - RM_PMU_THERM_EVENT_THERMAL_3
    //      [0x07] - RM_PMU_THERM_EVENT_THERMAL_4
    //      [0x08] - RM_PMU_THERM_EVENT_THERMAL_5
    //      [0x09] - RM_PMU_THERM_EVENT_THERMAL_6
    //      [0x0A] - RM_PMU_THERM_EVENT_THERMAL_7
    //      [0x0B] - RM_PMU_THERM_EVENT_THERMAL_7
    //      [0x0C] - RM_PMU_THERM_EVENT_THERMAL_9
    //      [0x0D] - RM_PMU_THERM_EVENT_THERMAL_10
    //      [0x0E] - RM_PMU_THERM_EVENT_THERMAL_11
    //  - 5 unused events:
    //      [0x0F]..[0x13]
    //  - 6 BA events:
    //      [0x14] - RM_PMU_THERM_EVENT_BA_W0_T1
    //      [0x15] - RM_PMU_THERM_EVENT_BA_W0_T2
    //      [0x16] - RM_PMU_THERM_EVENT_BA_W1_T1
    //      [0x17] - RM_PMU_THERM_EVENT_BA_W1_T2
    //      [0x18] - RM_PMU_THERM_EVENT_BA_W2_T1
    //      [0x19] - RM_PMU_THERM_EVENT_BA_W2_T2
    //  - 2 derived events:
    //      [0x1A] - RM_PMU_THERM_EVENT_META_ANY_HW_FS
    //      [0x1B] - RM_PMU_THERM_EVENT_META_ANY_EDP
    //  - 3 EDPP events:
    //      [0x1C] - RM_PMU_THERM_EVENT_VOLTAGE_HW_ADC
    //      [0x1D] - RM_PMU_THERM_EVENT_EDPP_VMIN
    //      [0x1E] - RM_PMU_THERM_EVENT_EDPP_FONLY
    //
    // TU10X is no longer packing event indexes.
    //
    #define THERM_PMU_EVENT_ARRAY_SIZE_TU10X    31

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_VIOLATION_TIMERS_EXTENDED)
    static THERM_PMU_EVENT_VIOLATION_ENTRY
        violations_TU10X[THERM_PMU_EVENT_ARRAY_SIZE_TU10X] = {{{ 0 }}};
#else
    static THERM_PMU_EVENT_VIOLATION_ENTRY
        violations_TU10X[THERM_PMU_EVENT_ARRAY_SIZE_TU10X] = {{ 0 }};
#endif

    static FLCN_TIMESTAMP
        notifications_TU10X[THERM_PMU_EVENT_ARRAY_SIZE_TU10X] = {{ 0 }};

    Therm.evt.pViolData       = violations_TU10X;
    Therm.evt.pNextNotif      = notifications_TU10X;
    Therm.evt.maskIntrLevelHw =
        LW_CPWR_THERM_EVENT_MASK_HW_FAILSAFE &
        LW_CPWR_THERM_EVENT_MASK_INTERRUPT;
    Therm.evt.maskSupportedHw = LW_CPWR_THERM_EVENT_MASK_HW_FAILSAFE;
}
