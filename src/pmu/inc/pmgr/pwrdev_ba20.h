/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pwrdev_ba20.h
 * @copydoc pwrdev_ba20.c
 */

#ifndef PWRDEV_BA20_H
#define PWRDEV_BA20_H

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmgr/pwrdev.h"

/* ------------------------- Types Definitions ------------------------------ */
typedef struct PWR_DEVICE_BA20  PWR_DEVICE_BA20;

/*!
 * Number of supported limits.
 */
#define PWR_DEVICE_BA20_LIMIT_COUNT                             4

/*!
 * The maximum voltage in uV required for scaleA computation.
 *
 * TODO - Obtain this from a VOLT API.
 */
#define PWR_DEVICE_BA20_SCALE_EQUATION_MAX_VOLTAGE_UV    0x13D620   // 1.3[V]

/*!
 * Structure holding all chip-specific data required by BA20 power device
 */
typedef struct PWR_DEVICE_BA20_CHIP_CONFIG {

    /*!
     * Number of enabled TPCs on the chip.
     */
    LwU16 numTpc;


    /*!
     * Number of enabled FBPAs on the chip.
     */
    LwU8  numFbpa;


    /*!
     * Number of enabled MXBARs on the chip.
     */
    LwU8  numMxbar;
} PWR_DEVICE_BA20_CHIP_CONFIG;

/*!
 * Structure of static information specific to the BA20 power device which 
 * depends based on the voltage rail which the BA20 instance is monitoring.
 */
typedef struct PWR_DEVICE_BA20_VOLT_RAIL_DATA
{
    /*!
     * Array of volt rail specific static information specific to the 
     * BA20 power device @ref LW2080_CTRL_PMGR_PWR_DEVICE_BA20_VOLT_RAIL_DATA.
     */   
    LW2080_CTRL_PMGR_PWR_DEVICE_BA20_VOLT_RAIL_DATA
          super;
    /*!
     * Parameter used for the voltage rail in computations of scaling A,
     * offset C and thresholds. It represents number of bits that the value
     * needs to be left-shifted before it's programmed into respective HW register.
     * Introduced to increase precision by using all 8-bits available in HW for
     * factorA.
     */
    LwU8   shiftA;
    /*!
     * Cached GPU voltage [uV] for the voltage rail.
     */
    LwU32  voltageuV;
} PWR_DEVICE_BA20_VOLT_RAIL_DATA;

/*!
 * Structure representing a block activity v2.0 power device.
 */
struct PWR_DEVICE_BA20
{
    /*!
     * Must always be the first element.
     */
    PWR_DEVICE  super;
    /*!
     * If set device monitors/estimates GPU current [mA], otherwise power [mW].
     */
    LwBool      bLwrrent;
    /*!
     * Index of associated HW BA averaging window.
     */
    LwU8        windowIdx;
    /*!
     * EDPp - C0 Significand.
     */
    LwU16       c0Significand;
    /*!
     * EDPp - C0 Right Shift.
     */
    LwU8        c0RShift;
    /*!
     * EDPp - C1 Significand.
     */
    LwU16       c1Significand;
    /*!
     * EDPp - C1 Right Shift.
     */
    LwU8        c1RShift;
    /*!
     * EDPp - C2 Significand.
     */
    LwU16       c2Significand;
    /*!
     * EDPp - C2 Right Shift.
     */
    LwU8        c2RShift;
    /*!
     * EDPp - DBA Period.
     */
    LwU8        dbaPeriod;
    /*!
     * Index into Power Sensor Table pointing to GPUADC device.
     */
    LwU8        gpuAdcDevIdx;
    /*!
     * Defines T1 (BA Threshold 1) mode as ADC or Normal.
     */
    LwBool      bIsT1ModeADC;
    /*!
     * Defines T2 (BA Threshold 2) mode as ADC or Normal.
     */
    LwBool      bIsT2ModeADC;

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BA20_FBVDD_MODE))
    /*!
     * Boolean to enable BA operation in FBVDD Mode.
     */
    LwBool      bFBVDDMode;
#endif

    /*!
     * Window_period = winSize * 2^stepSize (utilsclk)
     * winSize gives Window Size to callwlate window period.
     */
    LwU8        winSize;
    /*!
     * Window_period = winSize * 2^stepSize (utilsclk)
     * stepSize gives Step Size (log 2) to callwlate window period.
     */
    LwU8        stepSize;
    /*!
     * Specifies whether the BA from GPC is on LWVDD or MSVDD.
     */
    LwU8        pwrDomainGPC;
    /*!
     * Specifies whether the BA from XBAR is on LWVDD or MSVDD.
     */
    LwU8        pwrDomainXBAR;
    /*!
     * Specifies whether the BA from FBP is on LWVDD or MSVDD.
     */
    LwU8        pwrDomainFBP;
    /*!
     * The scaling factor for Factor A/C (in both SW and HW modes), and also
     * for LW_CPWR_THERM_PEAKPOWER_CONFIG10_WIN_SUM_VALUE register output, but
     * in the reverse direction as compared to scaling of Factor A/C.
     */
    LwUFXP4_12  scaleFactor;
    /*!
     * EDPp - Content of LW_THERM_PEAKPOWER_CONFIG11(w) as precomputed in PMU.
     */
    LwU32       edpPConfigC0C1;
    /*!
     * EDPp - Content of LW_THERM_PEAKPOWER_CONFIG12(w) as precomputed in PMU.
     */
    LwU32       edpPConfigC2Dba;
    /*!
     * BA window configuration (precomputed by RM into HW register format).
     */
    LwU32       configuration;
    /*!
     * Chip-specific data required by the BA20 power device.
     */
    PWR_DEVICE_BA20_CHIP_CONFIG
                chipConfig;
    /*!
     * Array of static information specific to the BA20 power device which 
     * depends based on the voltage rail which the BA20 instance is monitoring.
     * Each entry in the array corresponds to a voltage rail that the BA20
     * instance may monitor.
     */    
    PWR_DEVICE_BA20_VOLT_RAIL_DATA
                voltRailData[LW2080_CTRL_PMGR_PWR_DEVICE_BA_20_MAX_CONFIGS];
};

/* ------------------------- Defines ---------------------------------------- */
/*!
 * Macro to return the number of supported providers.
 *
 * @param[in] pDev Pointer to the PWR_DEVICE object
 *
 * @return Number of PWR_DEVICE providers supported by this PWR_DEVICE class.
 */
#define pwrDevProvNumGet_BA20(pDev)                                            \
            LW2080_CTRL_PMGR_PWR_DEVICE_BA_2X_PROV_NUM

/*!
 * Macro to return the number of thresholds (limits) supported by this device.
 *
 * @param[in] pDev Pointer to the PWR_DEVICE object
 *
 * @return Number of thresholds.
 */
#define pwrDevThresholdNumGet_BA20(pDev)                                       \
            LW2080_CTRL_PMGR_PWR_DEVICE_BA_2X_THRESHOLD_NUM

/*!
 * @brief  Macro to check whether the value to be written may overflow the HW
 *         register field where it will be written to.
 *
 * @param  _regVal    The value to be written to the register field @ref _regField
 * @param  _regField  The HW register field where @ref _regVal will be written to
 * @param  _status    Status variable to be set in case of failure
 * @param  _exitLabel Label to jump to in case of failure
 */
#define PWR_DEVICE_BA20_OVERFLOW_CHECK(_regVal, _regField, _status, _exitLabel) \
do                                                                              \
{                                                                               \
    LwU32 _regValMinWidth = _regVal;                                            \
    HIGHESTBITIDX_32(_regValMinWidth);                                          \
                                                                                \
    PMU_ASSERT_TRUE_OR_GOTO(_status,                                            \
        (_regValMinWidth < DRF_SIZE(_regField)),                                \
        FLCN_ERR_ILWALID_STATE,                                                 \
        _exitLabel);                                                            \
} while (LW_FALSE)

/*!
 * @brief  Macro to obtain PWR_DEVICE_VOLT_RAIL_DATA object pointer for the BA
 *         config specified by @ref _configIdx.
 *
 * @param  _pBa20              Pointer to @ref PWR_DEVICE_BA20
 * @param  _configIdx          Index of the volt-rail specific BA config
 * @param  _ppBa20VoltRailData Pointer to Pointer to @ref PWR_DEVICE_BA20_VOLT_RAIL_DATA
 * @param  _status             Status variable to be set in case of failure
 * @param  _exitLabel          Label to jump to in case of failure
 */
#define PWR_DEVICE_BA20_VOLT_RAIL_DATA_FROM_CONFIG_IDX_GET(_pBa20, _ppBa20VoltRailData, _configIdx, _status, _exitLabel) \
do                                                                                                                       \
{                                                                                                                        \
    PMU_ASSERT_TRUE_OR_GOTO(_status,                                                                                     \
        ((_pBa20 != NULL) && (_ppBa20VoltRailData != NULL)),                                                             \
        FLCN_ERR_ILWALID_ARGUMENT,                                                                                       \
        _exitLabel);                                                                                                     \
                                                                                                                         \
    PMU_ASSERT_TRUE_OR_GOTO(_status,                                                                                     \
        (_configIdx < LW2080_CTRL_PMGR_PWR_DEVICE_BA_20_MAX_CONFIGS),                                                    \
        FLCN_ERR_ILWALID_ARGUMENT,                                                                                       \
        _exitLabel);                                                                                                     \
                                                                                                                         \
    *(_ppBa20VoltRailData) = &(_pBa20->voltRailData[_configIdx]);                                                        \
                                                                                                                         \
    PMU_ASSERT_TRUE_OR_GOTO(_status,                                                                                     \
        (*(_ppBa20VoltRailData) != NULL),                                                                                \
        FLCN_ERR_ILWALID_INDEX,                                                                                          \
        _exitLabel);                                                                                                     \
} while (LW_FALSE)

/*!
 * @brief  Macro to obtain VOLT_RAIL object pointer from PWR_DEVICE_BA20_VOLT_RAIL_DATA
 *         object pointer.
 *
 * @param  _pBa20              Pointer to @ref PWR_DEVICE_BA20
 * @param  _pBa20VoltRailData  Pointer to @ref PWR_DEVICE_BA20_VOLT_RAIL_DATA
 * @param  _ppRail             Pointer to Pointer to @ref VOLT_RAIL
 * @param  _status             Status variable to be set in case of failure
 * @param  _exitLabel          Label to jump to in case of failure
 */
#define PWR_DEVICE_BA20_VOLT_RAIL_FROM_VOLT_RAIL_DATA_GET(_pBa20, _pBa20VoltRailData, _ppRail, _status, _exitLabel) \
do                                                                                                                  \
{                                                                                                                   \
    PMU_ASSERT_TRUE_OR_GOTO(_status,                                                                                \
        ((_pBa20 != NULL) && (_pBa20VoltRailData != NULL) && (_ppRail != NULL)),                                    \
        FLCN_ERR_ILWALID_ARGUMENT,                                                                                  \
        _exitLabel);                                                                                                \
                                                                                                                    \
    *(_ppRail) = VOLT_RAIL_GET(_pBa20VoltRailData->super.voltRailIdx);                                              \
                                                                                                                    \
    PMU_ASSERT_TRUE_OR_GOTO(_status,                                                                                \
        (*(_ppRail) != NULL),                                                                                       \
        FLCN_ERR_ILWALID_INDEX,                                                                                     \
        _exitLabel);                                                                                                \
} while (LW_FALSE)

/*!
 * @brief  Private macro that iterates over BA config indices
           starting at a startingIndex
 *
 * @note   Must be called in conjunction with
 *         PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX__PRIVATE_END
 *
 *         If _bConstruct equals LW_TRUE, only _ppBa20VoltRailData
 *         will be modified
 *
 * @param  _pBa20              Pointer to @ref PWR_DEVICE_BA20
 * @param  _ppBa20VoltRailData Pointer to Pointer to @ref PWR_DEVICE_BA20_VOLT_RAIL_DATA
 * @param  _ppRail             Pointer to Pointer to @ref VOLT_RAIL
 * @param  _configIdx          Loop iterator
 * @param  _bConstruct         boolean to indicate whether this macro is being called during PMU construct
 * @param  _status             Status variable to be set in case of failure
 * @param  _exitLabel          Label to jump to in case of failure
 * @param  _startingIndex      Index to start from
 */
#define PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX__PRIVATE_BEGIN(_pBa20, _ppBa20VoltRailData, _ppRail, _configIdx,          \
                                                                 _bConstruct, _status, _exitLabel, _startingIndex)      \
do                                                                                                                      \
{                                                                                                                       \
    CHECK_SCOPE_BEGIN(PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX__PRIVATE);                                                  \
                                                                                                                        \
    PMU_ASSERT_TRUE_OR_GOTO(_status,                                                                                    \
        ((_pBa20 != NULL) && (_ppBa20VoltRailData != NULL)),                                                            \
        FLCN_ERR_ILWALID_ARGUMENT,                                                                                      \
        _exitLabel);                                                                                                    \
                                                                                                                        \
    for ((_configIdx) = (_startingIndex); (_configIdx) < LW2080_CTRL_PMGR_PWR_DEVICE_BA_20_MAX_CONFIGS; (_configIdx)++) \
    {                                                                                                                   \
        PWR_DEVICE_BA20_VOLT_RAIL_DATA_FROM_CONFIG_IDX_GET(_pBa20,                                                      \
            _ppBa20VoltRailData, _configIdx, _status,                                                                   \
            _exitLabel);                                                                                                \
                                                                                                                        \
        if (!_bConstruct)                                                                                               \
        {                                                                                                               \
            PMU_ASSERT_TRUE_OR_GOTO(_status,                                                                            \
                (VOLT_RAIL_INDEX_IS_VALID((*(_ppBa20VoltRailData))->super.voltRailIdx) ||                               \
                    ((*(_ppBa20VoltRailData))->super.voltRailIdx == LW2080_CTRL_VOLT_VOLT_RAIL_INDEX_ILWALID)),         \
                FLCN_ERR_ILWALID_STATE,                                                                                 \
                _exitLabel);                                                                                            \
                                                                                                                        \
            if ((*(_ppBa20VoltRailData))->super.voltRailIdx == LW2080_CTRL_VOLT_VOLT_RAIL_INDEX_ILWALID)                \
            {                                                                                                           \
                continue;                                                                                               \
            }                                                                                                           \
                                                                                                                        \
            if (_ppRail != NULL)                                                                                        \
            {                                                                                                           \
                PWR_DEVICE_BA20_VOLT_RAIL_FROM_VOLT_RAIL_DATA_GET(_pBa20, (*(_ppBa20VoltRailData)),                     \
                    _ppRail, _status,                                                                                   \
                    _exitLabel);                                                                                        \
            }                                                                                                           \
        }

/*!
 * @brief  Macro that must be called in conjunction with
 *         PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX__PRIVATE_BEGIN.
 */
#define PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX__PRIVATE_END               \
        }                                                                \
        CHECK_SCOPE_END(PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX__PRIVATE); \
    } while (LW_FALSE)

/*!
 * @brief  Macro that iterates over BA config indices
 *
 * @note   Must be called in conjunction with
 *         PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX_END
 *
 * @param  _pBa20              Pointer to @ref PWR_DEVICE_BA20
 * @param  _ppBa20VoltRailData Pointer to Pointer to @ref PWR_DEVICE_BA20_VOLT_RAIL_DATA
 * @param  _ppRail             Pointer to Pointer to @ref VOLT_RAIL
 * @param  _configIdx          Loop iterator
 * @param  _bConstruct         boolean to indicate whether this macro is being called during PMU construct
 * @param  _status             Status variable to be set in case of failure
 * @param  _exitLabel          Label to jump to in case of failure
 */
#define PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX_BEGIN(_pBa20, _ppBa20VoltRailData, _ppRail, _configIdx,          \
                                                        _bConstruct, _status, _exitLabel)                      \
    do                                                                                                         \
    {                                                                                                          \
        CHECK_SCOPE_BEGIN(PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX);                                              \
        PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX__PRIVATE_BEGIN(_pBa20, _ppBa20VoltRailData, _ppRail, _configIdx, \
                                                                 _bConstruct, _status, _exitLabel, 0U)         \
        {

/*!
 * @brief  Macro that must be called in conjunction with
 *         PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX_BEGIN.
 */
#define PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX_END               \
        }                                                       \
        PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX__PRIVATE_END;     \
        CHECK_SCOPE_END(PWR_DEVICE_BA20_FOR_EACH_CONFIG_INDEX); \
    } while (LW_FALSE)

/* ------------------------- Inline Functions ------------------------------ */
/*!
 * @brief   Get the boolean denoting whether the BA20 power sensor is
 *          operating in FBVDD mode.
 *
 * @param[in]       pBa20               Pointer to @ref PWR_DEVICE_BA20
 * @param[in, out]  pBFBVDDModeIsActive Pointer to the boolean denoting whether
 *                                      the BA20 power sensor is operating in
 *                                      FBVDD mode
 *                  Output parameters:
 *                      *pBFBVDDModeIsActive
 *
 * @return  FLCN_OK if the transaction was successful, error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
pwrDevBa20FBVDDModeIsActiveGet
(
    PWR_DEVICE_BA20  *pBa20,
    LwBool           *pBFBVDDModeIsActive
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pBa20 != NULL) &&
         (pBFBVDDModeIsActive != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrDevBa20FBVDDModeIsActiveGet_done);

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BA20_FBVDD_MODE))
    *pBFBVDDModeIsActive = pBa20->bFBVDDMode;
#else
    *pBFBVDDModeIsActive = LW_FALSE;
#endif

pwrDevBa20FBVDDModeIsActiveGet_done:
    return status;
}

/*!
 * @brief   Set the boolean denoting whether the BA20 power sensor is
 *          operating in FBVDD mode.
 *
 * @param[in]       pBa20      Pointer to @ref PWR_DEVICE_BA20
 * @param[in, out]  bFBVDDMode Boolean denoting whether the BA20 power sensor
 *                             is to operate in FBVDD mode
 *                  Output parameters:
 *                      *pBFBVDDModeIsActive
 *
 * @return  FLCN_OK if the transaction was successful, error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
pwrDevBa20FBVDDModeIsActiveSet
(
    PWR_DEVICE_BA20  *pBa20,
    LwBool            bFBVDDMode
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pBa20 != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrDevBa20FBVDDModeIsActiveSet_done);

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BA20_FBVDD_MODE))
    pBa20->bFBVDDMode = bFBVDDMode;
#endif

pwrDevBa20FBVDDModeIsActiveSet_done:
    return status;
}

/* ------------------------- Function Prototypes  --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- PWR_DEVICE Interfaces -------------------------- */
BoardObjGrpIfaceModel10ObjSet       (pwrDevGrpIfaceModel10ObjSetImpl_BA20)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrDevGrpIfaceModel10ObjSetImpl_BA20");
PwrDevLoad              (pwrDevLoad_BA20)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevLoad_BA20");
PwrDevSetLimit          (pwrDevSetLimit_BA20)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrDevSetLimit_BA20");
PwrDevTupleGet          (pwrDevTupleGet_BA20)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrDevTupleGet_BA20");
PwrDevStateSync         (pwrDevStateSync_BA20)
    GCC_ATTRIB_SECTION("imem_pmgrPwrDeviceStateSync", "pwrDevStateSync_BA20");

/* ------------------------- Debug Macros ----------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // PWRDEV_BA20_H
