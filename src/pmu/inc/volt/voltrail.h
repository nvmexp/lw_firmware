/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   voltrail.h
 * @brief  VOLT Voltage Rail Model
 *
 * This module is a collection of functions managing and manipulating state
 * related to the volt rails.
 */

#ifndef VOLTRAIL_H
#define VOLTRAIL_H

#include "g_voltrail.h"

#ifndef G_VOLTRAIL_H
#define G_VOLTRAIL_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------ Forward Definitions ---------------------------- */
typedef struct VOLT_RAIL                     VOLT_RAIL, VOLT_RAIL_BASE;
typedef struct VOLT_RAIL_SENSED_VOLTAGE_DATA VOLT_RAIL_SENSED_VOLTAGE_DATA;

/* ------------------------- Application Includes -------------------------- */
#include "boardobj/boardobjgrp.h"
#include "pmu_objclk.h"
#include "volt/voltdev.h"
#include "perf/3x/vfe.h"
#include "boardobj/boardobjgrp_iface_model_10.h"

/* ------------------------- Macros ---------------------------------------- */
/*!
 * @brief Accessor macro for VOLT_RAILS object.
 */
#define VOLT_RAILS_GET()                                                        \
    (&(Volt.railMetadata))

/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 *
 * @note A follow-up CL will colwert this to reference the BOARDOBJGRP
 * type, not BOARDOBJGRP_E32.
 */
#define BOARDOBJGRP_DATA_LOCATION_VOLT_RAIL                                     \
    (&(VOLT_RAILS_GET())->super.super)

/*!
 * @brief Helper accessor macro to retreive the volt domain HAL.
 */
#define VOLT_RAILS_GET_VOLT_DOMAIN_HAL()                                        \
    ((VOLT_RAILS_GET())->voltDomainHAL)

/*!
 * @copydoc BOARDOBJGRP_IS_VALID
 */
#define VOLT_RAIL_INDEX_IS_VALID(_objIdx) \
    BOARDOBJGRP_IS_VALID(VOLT_RAIL, _objIdx)

/*!
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define VOLT_RAIL_GET(_objIdx) \
    (BOARDOBJGRP_OBJ_GET(VOLT_RAIL, (_objIdx)))

/*!
 * @brief Accessor macro for adcDevMask
 */
#if PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLTAGE_SENSED)
#define voltRailAdcDevMaskGet(_pRail) &((_pRail)->adcDevMask)
#else
#define voltRailAdcDevMaskGet(_pRail) NULL
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLTAGE_SENSED)

/*!
 * @brief Helper macro to retreive the VOLT_DOMAIN for the given VOLT_RAIL.
 *
 * @param[in]  pRail    VOLT_RAIL object pointer
 *
 * @return VOLT_DOMAIN for the given VOLT_RAIL.
 */
#define voltRailDomainGet(pRail) \
    BOARDOBJ_GET_TYPE(pRail)

/*!
 * @brief Helper macro to retreive the default VOLT_DEVICE for the given VOLT_RAIL.
 *
 * @param[in]  pRail    VOLT_RAIL object pointer
 *
 * @return Pointer to the default VOLT_DEVICE  for the given VOLT_RAIL.
 */
#define voltRailDefaultVoltDevGet(pRail)                                    \
    VOLT_DEVICE_GET((pRail)->voltDevIdxDefault)

/*!
 * @brief Helper accessor macro to retreive the VOLT_RAIL's voltage scaling dynamic equation index
 *
 * @note Dynamic Power Equation Table Entry actually describes voltage scaling
 *          exponent. If/when the entry starts describing actual dynamic
 *          power/current, this macro must be updated to return 0xFF and that
 *          needs to be done by adding an enumeration in Voltage Rail Table to
 *          distinguish between voltage scaling exponent and dynamic power/current.
 *          An alternative approach could be to query PWR_EQUATION table
 *          to know it's use case.
 *
 * @param[in]  pRail    VOLT_RAIL object pointer
 *
 * @return an index, 0xFF is considered an INVALID index.
 */
#define voltRailVoltScaleExpPwrEquIdxGet(pRail)                               \
    ((pRail)->dynamicPwrEquIdx)

/*!
 * Helper accessor macro for @ref VOLT_RAIL::clkDomainsProgMask.
 *
 * @param[in] _pVoltRail   VOLT_RAIL pointer
 *
 * @return @ref VOLT_RAIL::clkDomainsProgMask
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_CLK_DOMAINS_PROG_MASK))
#define voltRailGetClkDomainsProgMask(_pVoltRail)                             \
    &((_pVoltRail)->clkDomainsProgMask)
#else
#define voltRailGetClkDomainsProgMask(_pVoltRail)                             \
    ((BOARDOBJGRPMASK_E32 *)NULL)
#endif


/* ------------------------- Datatypes ------------------------------------- */
/*!
 * @brief Gets voltage of the default VOLT_DEVICE for the VOLT_RAIL.
 *
 * @param[in]  pRail        VOLT_RAIL object pointer
 * @param[out] pVoltageuV   Current voltage
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Rail object does not support this interface.
 * @return  FLCN_OK
 *      Requested current voltage was obtained successfully.
 */
#define VoltRailGetVoltage(fname) FLCN_STATUS (fname)(VOLT_RAIL *pRail, LwU32 *pVoltageuV)

/*!
 * @brief Gets sensed voltage of the VOLT_RAIL using ADC_DEVICE corresponding to that rail.
 *
 * @param[in]       pRail   VOLT_RAIL object pointer
 * @param[in/out]   VOLT_RAIL_SENSED_VOLTAGE_DATA
 *                      Pointer to VOLT_RAIL_SENSED_VOLTAGE_DATA data
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Rail object does not support this interface.
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *      Sanity check failed on the input data.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 * @return  FLCN_OK
 *      Sensed voltage was obtained successfully.
 */
#define VoltRailGetVoltageSensed(fname) FLCN_STATUS (fname)(VOLT_RAIL *pRail, VOLT_RAIL_SENSED_VOLTAGE_DATA *pData)

/*!
 * @brief Sets voltage on the appropriate VOLT_RAIL.
 *
 * @param[in]  pRail        VOLT_RAIL object pointer
 * @param[in]  voltageuV    Target voltage to set in uV
 * @param[in]  bTrigger     Boolean to trigger the voltage switch into the HW
 * @param[in]  bWait        Boolean to wait for settle time after switch
 * @param[in]  railAction   Control action to be applied on the rail
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Rail object does not support this interface.
 * @return  FLCN_ERR_ILWALID_STATE
 *      Invalid mask of VOLTAGE_DEVICEs for the rail.
 * @return  FLCN_OK
 *      Requested target voltage was set successfully.
 */
#define VoltRailSetVoltage(fname) FLCN_STATUS (fname)(VOLT_RAIL *pRail, LwU32 voltageuV, LwBool bTrigger, LwBool bWait, LwU8 railAction)

/*!
 * @brief Round voltage to a value supported by the default VOLT_DEVICE.
 *
 * @param[in]     pRail         VOLT_RAIL object pointer
 * @param[in/out] pVoltageuV    Rounded value
 * @param[in]     bRoundUp      Boolean to round up or down
 * @param[in]     bBound
 *      Boolean flag indicating whether the rounded value should be bound to
 *      the range of voltages supported on the regulator.  If this flag is
 *      LW_FALSE and the provided value is outside the range, the value will
 *      be rounded (if possible) but outside the range of supported voltages.
 *
 * @return FLCN_OK
 *     Voltage successfully rounded to a supported value.
 */
#define VoltRailRoundVoltage(fname) FLCN_STATUS (fname)(VOLT_RAIL *pRail, LwS32 *pVoltageuV, LwBool bRoundUp, LwBool bBound)

/*!
 * @brief Update VOLT_RAIL dynamic parameters that depend on VFE.
 *
 * @param[in]  pRail            VOLT_RAIL object pointer
 * @param[in]  bVfeEvaluate     Boolean indicating VFE evaluation for
 *                              updating the limit.
 *
 * @return FLCN_OK
 *      Dynamic update successfully completed.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
#define VoltRailDynamilwpdate(fname) FLCN_STATUS (fname)(VOLT_RAIL *pRail, LwBool bVfeEvaluate)

/*!
 * @brief For given voltage, get the given voltage rail's scale factor through the Scaling Power Equation.
 *
 * @param[in]       pRail         VOLT_RAIL object pointer
 * @param[in]       voltageuV     Voltage value to be used to get the scale factor
 * @param[in]       bIsUnitmA     The required unit of the scale factor (mW = LW_FALSE)
 * @param[in, out]  pScaleFactor  Scale Factor in mW/ mA
 *
 * @return FLCN_OK
 *      Successfully got the scale factor.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
#define VoltRailGetScaleFactor(fname) FLCN_STATUS (fname)(VOLT_RAIL *pRail, LwU32 voltageuV, LwBool bIsUnitmA, LwUFXP20_12 *pScaleFactor)

/*!
 * @brief For given voltage, get the given voltage rail's leakage power/current.
 *
 * @param[in]   pRail           VOLT_RAIL object pointer
 * @param[in]   voltageuV       Voltage value to be used to get the leakage power
 * @param[in]   bIsUnitmA       The required unit of leakage power (mW = LW_FALSE)
 * @param[in]   pgRes           Power gating residency - ratio [0,1]
 * @param[out]  pLeakageVal     Leakage power in mW/ current in mA
 *
 * @return FLCN_OK
 *      Successfully got the leakage power/current.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
#define VoltRailGetLeakage(fname) FLCN_STATUS (fname)(VOLT_RAIL *pRail, LwU32 voltageuV, LwBool bIsUnitmA, PMGR_LPWR_RESIDENCIES *pPgRes, LwU32 *pLeakageVal)

/*!
 * @brief Gets the maximum voltage limit of the default VOLT_DEVICE for the VOLT_RAIL.
 *
 * @param[in]  pRail        VOLT_RAIL object pointer
 * @param[out] pVoltageuV   Maximum voltage
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Rail object does not support this interface.
 * @return  FLCN_OK
 *      Requested maximum voltage was obtained successfully.
 */
#define VoltRailGetVoltageMax(fname) FLCN_STATUS (fname)(VOLT_RAIL *pRail, LwU32 *pVoltageuV)

/*!
 * @brief Gets the minimum voltage limit of the default VOLT_DEVICE for the VOLT_RAIL.
 *
 * @param[in]  pRail        VOLT_RAIL object pointer
 * @param[out] pVoltageuV   Minimum voltage
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Rail object does not support this interface.
 * @return  FLCN_OK
 *      Requested minimum voltage was obtained successfully.
 */
#define VoltRailGetVoltageMin(fname) FLCN_STATUS (fname)(VOLT_RAIL *pRail, LwU32 *pVoltageuV)

/*!
 * @brief Gets the noise unaware Vmin value of the VOLT_RAIL.
 *
 * @param[in]  pRail        VOLT_RAIL object pointer
 * @param[out] pVoltageuV   Noise unaware Vmin value
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Rail object does not support this interface.
 * @return  FLCN_OK
 *      Requested noise unaware Vmin value was obtained successfully.
 */
#define VoltRailGetNoiseUnawareVmin(fname) FLCN_STATUS (fname)(VOLT_RAIL *pRail, LwU32 *pVoltageuV)

/*!
 * @brief Colwert chip/board independent LW2080_CTRL_VOLT_VOLT_DOMAIN_<xyz> internal
 * RM/PMU/RMCTRL enum to Voltage Rail Table Index
 *
 * @param[in]  voltDomain   LW2080_CTRL_VOLT_VOLT_DOMAIN_<xyz>
 *
 * @return  Voltage Rail Table Index
 */
#define VoltRailVoltDomainColwertToIdx(fname) LwU8 (fname)(LwU8 voltDomain)

/*!
 * @brief Gets the VBIOS boot voltage value for the VOLT_RAIL.
 *
 * @param[in]  pRail        VOLT_RAIL object pointer
 * @param[out] pVoltageuV   Pointer to VBIOS Boot voltage in uV
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      Rail object does not support this interface.
 * @return  FLCN_OK
 *      Requested VBIOS boot voltage was obtained successfully.
 */
#define VoltRailGetVbiosBootVoltage(fname) FLCN_STATUS (fname)(VOLT_RAIL *pRail, LwU32 *pVoltageuV)

/*!
 * @brief Gets the voltage domains list on which sanity check needs to be performed.
 *
 * @param[in]  voltDomainHAL     Current voltage domain HAL
 * @param[out] pVoltDomainsList  Pointer to voltage domains list
 *
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *      NULL voltage domains list is passed as input.
 * @return  FLCN_OK
 *      Requested operation completed successfully.
 */
#define VoltRailGetVoltDomainSanityList(fname) FLCN_STATUS (fname)(LwU8 voltDomainHAL, VOLT_DOMAINS_LIST *pVoltDomainsList)

/*!
 * @brief Container of voltage rails.
 *
 * Includes additional information to properly function
 */
typedef struct
{
    /*!
     * @brief Board Object Group of all @ref VOLT_RAIL objects.
     */
    BOARDOBJGRP_E32 super;

    /*!
     * @brief The voltage domain HAL type specifies the list of enumerants to use when
     * interpreting the rail entries. Enumerants are listed in
     * @ref LW2080_CTRL_VOLT_VOLT_DOMAIN_HAL_<xyz>
     */
    LwU8    voltDomainHAL;

    /*!
     * @brief Flag to distinguish the first the boardObjSet RPC after initial boot
     */
    LwBool  bFirstTime;
} VOLT_RAIL_METADATA;

/*!
 * @brief RAM Assist Parameters of the Voltage Rail.
 *
 * Includes Critical Voltage thresholds and corresponding VFE indices
 */

typedef struct
{
    /*!
     * @brief Ram Assist Type of the rail.
     */
    LwU8            type;

    /*!
     * @brief Flag indicating whether ram assist control is enabled
     */
    LwBool          bEnabled;

    /*!
     * @brief VFE Equation Index of the entry that specifies the Vcrit Low voltage
     * for engaging ram assist cirlwitory.
     */
    LwVfeEquIdx     vCritLowVfeEquIdx;

    /*!
     * @brief VFE Equation Index of the entry that specifies the Vcrit High voltage
     * for disengaging ram assist cirlwitory.
     */
    LwVfeEquIdx     vCritHighVfeEquIdx;

    /*!
     * @brief Vcrit Low voltage for engaging ram assist cirlwitory.
     */
    LwU32           vCritLowuV;

    /*!
     * @brief Vcrit High voltage for disengaging ram assist cirlwitory.
     */
    LwU32           vCritHighuV;
} VOLT_RAIL_RAM_ASSIST;

/*!
 * @brief Extends BOARDOBJ providing attributes common to all VOLT_RAILS.
 */
struct VOLT_RAIL
{
    /*!
     * @brief @ref BOARDOBJ super-class.
     */
    BOARDOBJ        super;

    /*!
     * @brief VFE Equation Index of the entry that specifies the default maximum
     * reliability limit of the silicon.
     */
    LwVfeEquIdx     relLimitVfeEquIdx;

    /*!
     * @brief VFE Equation Index of the entry that specifies the alternate maximum
     * reliability limit of the silicon.
     */
    LwVfeEquIdx     altRelLimitVfeEquIdx;

    /*!
     * @brief VFE Equation Index of the entry that specifies the maximum over-voltage
     * limit of the silicon.
     */
    LwVfeEquIdx     ovLimitVfeEquIdx;

    /*!
     * @brief VFE Equation Index of the entry that specifies the Vmin voltage.
     */
    LwVfeEquIdx     vminLimitVfeEquIdx;

    /*!
     * @brief VFE Equation Index of the entry that specifies the worst case
     * voltage margin.
     */
    LwVfeEquIdx     voltMarginLimitVfeEquIdx;

    /*!
     * @brief Power Equation table index for evaluating this rail's leakage power/current.
     */
    LwU8            leakagePwrEquIdx;

    /*!
     * @brief Power Equation table index for evaluating this rail's dynamic power/current.
     */
    LwU8            dynamicPwrEquIdx;

    /*!
     * @brief Default VOLTAGE_DEVICE for the rail.
     */
    LwU8            voltDevIdxDefault;

    /*!
     * @brief IPC VMIN VOLTAGE_DEVICE for the rail.
     */
    LwU8            voltDevIdxIPCVmin;

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLTAGE_SENSED))
    /*!
     * @brief Mask of ADC_DEVICEs for obtaining sensed voltage of the rail.
     */
    BOARDOBJGRPMASK_E32
                    adcDevMask;
#endif

    /*!
     * @brief Mask of VOLTAGE_DEVICEs for the rail.
     */
    BOARDOBJGRPMASK_E32
                    voltDevMask;

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_CLK_DOMAINS_PROG_MASK))
    /*!
     * Mask of all CLK_DOMAINs which implement the CLK_DOMAIN_PROG
     * interface and have a Vmin on the VOLT_RAIL.
     */
    BOARDOBJGRPMASK_E32
                    clkDomainsProgMask;
#endif

    /*!
     * @brief Current voltage of the default VOLT_DEVICE for the rail.
     * Access to this member is synchronized by PMU_VOLT_SEMAPHORE feature
     * since it is read/written by/from multiple tasks.
     */
    LwU32           lwrrVoltDefaultuV;

    /*!
     * @brief Cached value of default maximum reliability limit of the silicon.
     */
    LwU32           relLimituV;

    /*!
     * @brief Cached value of alternate maximum reliability limit of the silicon.
     */
    LwU32           altRelLimituV;

    /*!
     * @brief Cached value of maximum over-voltage limit of the silicon.
     */
    LwU32           ovLimituV;

    /*!
     * @brief Cached value of minimum voltage limit of the silicon.
     * Access to this member is synchronized by PMU_VOLT_SEMAPHORE feature
     * since it is read/written by/from multiple tasks.
     */
    LwU32           vminLimituV;

    /*!
     * @brief Default value of default maximum reliability limit of the silicon.
     */
    LwU32           defRelLimituV;

    /*!
     * @brief Default value of alternate maximum reliability limit of the silicon.
     */
    LwU32           defAltRelLimituV;

    /*!
     * @brief Default value of maximum over-voltage limit of the silicon.
     */
    LwU32           defOvLimituV;

    /*!
     * @brief Default value of minimum voltage limit of the silicon.
     */
    LwU32           defVminLimituV;

    /*!
     * @brief Cached value of maximum voltage limit of the silicon.
     */
    LwU32           maxLimituV;

    /*!
     * @brief Cached value of worst case voltage margin of the silicon.
     */
    LwS32           voltMarginLimituV;

    /*!
     * @brief Array to store voltage delta to offset different voltage limits.
     */
     LwS32          voltDeltauV[LW2080_CTRL_VOLT_RAIL_VOLT_DELTA_MAX_ENTRIES];

    /*!
     * @brief V_{min, noise-unaware} - The minimum voltage (uV) with respect to
     * noise-unaware constraints on this VOLT_RAIL.
     * Access to this member is synchronized by PMU_VOLT_SEMAPHORE feature
     * since it is read/written by/from multiple tasks.
     */
     LwU32          voltMinNoiseUnawareuV;

    /*!
     * @brief Cached value of VBIOS boot voltage in uV.
     */
     LwU32          vbiosBootVoltageuV;

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_RAM_ASSIST))
    /*!
     * @brief Ram Assist control and status parameters.
     */
     VOLT_RAIL_RAM_ASSIST  
            ramAssist;
#endif

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_ACTION))
    /*!
     * This will denote which state the rail is lwrrently in as per LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_<xyz>.
     */
    LwU8            railAction;
#endif

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAIL_SCALING_PWR_EQUATION))
    /*!
     * @brief Power Equation table index for BA Scaling.
     */
    LwU8    baScalingPwrEqnIdx;
#endif
};

/*!
 * @brief Defines the structure that holds data used to execute the
 * @ref voltVoltOffsetRangeGet() API.
 */
typedef struct
{
    /*!
     * @brief Voltage Rail Index corresponding to a VOLT_RAIL.
     */
    LwU8    railIdx;

    /*!
     * @brief Maximum positive voltage offset that can be applied to this voltage rail.
     */
    LwU32   voltOffsetPositiveMaxuV;

    /*!
     * @brief Maximum negative voltage offset that can be applied to this voltage rail.
     */
    LwS32   voltOffsetNegativeMaxuV;
} VOLT_RAIL_OFFSET_RANGE_ITEM;

/*!
 * @brief Defines the structure that holds data used to execute the
 * @ref voltVoltOffsetRangeGet() API.
 */
typedef struct
{
    /*!
     * @brief Number of VOLT_RAILs that require the voltage change.
     */
    LwU8    numRails;

    /*!
     * @brief List of @ref VOLT_RAIL_OFFSET_RANGE_ITEM entries.
     */
    VOLT_RAIL_OFFSET_RANGE_ITEM
            railOffsets[LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS];
} VOLT_RAIL_OFFSET_RANGE_LIST;

/*!
 * @brief Defines the structure that holds data used to execute the
 * @ref voltRailGetVoltageSensed() API.
 */
struct VOLT_RAIL_SENSED_VOLTAGE_DATA
{
    /*!
     * @brief [in/out] Client provided output buffer to store
     * @ref CLK_ADC_ACC_SAMPLE data. Client should provide buffer size that can
     * store at-least (boardObjGrpMaskBitIdxHighest(VOLT_RAIL::adcDevMask) + 1)
     * samples as the buffer is used to directly index into the ADC Device Table
     * without packing/compressing for unused ADC Device Table entries.
     */
    CLK_ADC_ACC_SAMPLE
           *pClkAdcAccSample;

    /*!
     * @brief [in] Number of samples that can be stored in client provided
     * output buffer.
     */
    LwU8    numSamples;

    /*!
     * @brief [in] Sensed voltage mode
     * @ref LW2080_CTRL_VOLT_VOLT_RAIL_SENSED_VOLTAGE_MODE_<xyz>.
     */
    LwU8    mode;

    /*!
     * @brief [out] Actual sensed voltage reading based on
     * @ref LW2080_CTRL_VOLT_VOLT_RAIL_SENSED_VOLTAGE_MODE_<xyz>.
     */
    LwU32   voltageuV;
};

/* ------------------------- Defines ---------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
BoardObjGrpIfaceModel10CmdHandler   (voltRailBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libVoltConstruct", "voltRailBoardObjGrpIfaceModel10Set");

FLCN_STATUS                 voltRailsLoad(void);

BoardObjGrpIfaceModel10CmdHandler       (voltRailBoardObjGrpIfaceModel10GetStatus);

//
// Most PERF code needs VOLT rounding functions.  Placing in "imem_perfVf" so
// that PERF client code doesn't need to attach.
//
mockable VoltRailRoundVoltage        (voltRailRoundVoltage)
    GCC_ATTRIB_SECTION("imem_perfVf", "voltRailRoundVoltage");

mockable VoltRailSetVoltage          (voltRailSetVoltage);

mockable VoltRailGetVoltageMax       (voltRailGetVoltageMax);

mockable FLCN_STATUS        voltRailGetVoltageInternal(VOLT_RAIL *pRail, LwU32 *pVoltageuV);

FLCN_STATUS                 voltRailGetVoltage_RPC(LwU8 railIdx, LwU32 *pVoltageuV)
    GCC_ATTRIB_SECTION("imem_libVoltApi", "voltRailGetVoltage_RPC");

VoltRailGetNoiseUnawareVmin (voltRailGetNoiseUnawareVmin)
    GCC_ATTRIB_SECTION("imem_libVoltApi", "voltRailGetNoiseUnawareVmin");

VoltRailGetVoltageMin       (voltRailGetVoltageMin)
    GCC_ATTRIB_SECTION("imem_libVoltApi", "voltRailGetVoltageMin");

VoltRailGetScaleFactor      (voltRailGetScaleFactor)
    GCC_ATTRIB_SECTION("imem_libVoltApi", "voltRailGetScaleFactor");

VoltRailGetLeakage          (voltRailGetLeakage)
    GCC_ATTRIB_SECTION("imem_libVoltApi", "voltRailGetLeakage");

VoltRailGetVoltageSensed    (voltRailGetVoltageSensed)
    GCC_ATTRIB_SECTION("imem_libVoltApi", "voltRailGetVoltageSensed");

FLCN_STATUS                 voltRailOffsetRangeGet(LW2080_CTRL_VOLT_VOLT_RAIL_LIST *pRailList, VOLT_RAIL_OFFSET_RANGE_LIST *pRailOffsetList, LwBool bSkipVoltRangeTrim)
    GCC_ATTRIB_SECTION("imem_libVoltApi", "voltRailOffsetRangeGet");

FLCN_STATUS                 voltRailSanityCheck(LW2080_CTRL_VOLT_VOLT_RAIL_LIST *pRailList, LwBool bCheckState);

VoltRailVoltDomainColwertToIdx (voltRailVoltDomainColwertToIdx)
    GCC_ATTRIB_SECTION("imem_libVoltApi", "voltRailVoltDomainColwertToIdx");

mockable FLCN_STATUS                 voltRailSetNoiseUnawareVmin(LW2080_CTRL_VOLT_VOLT_RAIL_LIST *pRailList);

mockable VoltRailGetVoltage          (voltRailGetVoltage)
    GCC_ATTRIB_SECTION("imem_libVoltApi", "voltRailGetVoltage");

mockable VoltRailDynamilwpdate       (voltRailDynamilwpdate);

mockable FLCN_STATUS                 voltRailsDynamilwpdate(LwBool bVfeEvaluate);

VoltRailGetVbiosBootVoltage          (voltRailGetVbiosBootVoltage)
    GCC_ATTRIB_SECTION("imem_libVoltApi", "voltRailGetVbiosBootVoltage");

VoltRailGetVoltDomainSanityList      (voltRailGetVoltDomainSanityList);

/*!
 * Board Object interfaces.
 */
BoardObjGrpIfaceModel10SetHeader   (voltVoltRailIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_libVoltConstruct", "voltVoltRailIfaceModel10SetHeader");
BoardObjGrpIfaceModel10SetEntry    (voltVoltRailIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_libVoltConstruct", "voltVoltRailIfaceModel10SetEntry");

/* ------------------------ Include Derived Types -------------------------- */
/* ------------------------ Inline functions ------------------------------- */
/*!
 * @brief Helper inline function to construct the sensed voltage data structure for the
 * given VOLT_RAIL.
 *
 * @param[in]  pData    VOLT_RAIL_SENSED_VOLTAGE_DATA object pointer
 * @param[in]  pRail    VOLT_RAIL object pointer
 * @param[in]  mode     Sensed voltage mode
 * @param[in]  ovl      DMEM overlay from which memory is to be allocated for
 *                       CLK_ADC_ACC_SAMPLEs
 */
static inline FLCN_STATUS
voltRailSensedVoltageDataConstruct
(
    VOLT_RAIL_SENSED_VOLTAGE_DATA *pData, 
    VOLT_RAIL                     *pRail,
    LwU8                           mode, 
    LwU8                           ovlIdx
)
{
    FLCN_STATUS status = FLCN_OK;
    BOARDOBJGRPMASK_E32 *pAdcMask = NULL;

    if ((pRail == NULL) || (pData == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto voltRailSensedVoltageDataConstruct_exit;
    }

    pAdcMask = voltRailAdcDevMaskGet(pRail);
    if (pAdcMask == NULL)
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        PMU_BREAKPOINT();
        goto voltRailSensedVoltageDataConstruct_exit;
    }

    pData->mode = mode;
    pData->numSamples = 
        boardObjGrpMaskBitIdxHighest(pAdcMask) + 1U;
    if (pData->pClkAdcAccSample == NULL)
    {
        pData->pClkAdcAccSample = lwosCallocType(ovlIdx,
                pData->numSamples, CLK_ADC_ACC_SAMPLE);
        if (pData->pClkAdcAccSample == NULL)
        {
            status = FLCN_ERR_NO_FREE_MEM;
            PMU_BREAKPOINT();
            goto voltRailSensedVoltageDataConstruct_exit;
        }
    }

voltRailSensedVoltageDataConstruct_exit:
    return status;
}

/*!
 * @brief Copy-in RMCTRL sensed voltage data into the PMU-only voltage sensed data type.
 *
 * @param[out] pDest   PMU-only sensed voltage structure to copy to.
 * @param[in]  pSrc    RMCTRL structure to copy from.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pDest or @ref pSrc are NULL.
 * @return FLCN_OK                     If the sensed voltage data was successfully copied.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
voltRailSensedVoltageDataCopyIn
(
    VOLT_RAIL_SENSED_VOLTAGE_DATA               *pDest,
    LW2080_CTRL_VOLT_RAIL_SENSED_VOLTAGE_DATA   *pSrc
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       idx;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pDest == NULL) ||
         (pSrc  == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        voltRailSensedVoltageDataCopyIn_exit);

    pDest->mode       = pSrc->mode;
    pDest->voltageuV  = pSrc->voltageuV;
    pDest->numSamples = pSrc->numSamples;

    for (idx = 0; idx < pSrc->numSamples; idx++)
    {
        pDest->pClkAdcAccSample[idx] = pSrc->clkAdcAccSample[idx];
    }

voltRailSensedVoltageDataCopyIn_exit:
    return status;
}

/*!
 * @brief Copy-out PMU-only voltage sensed data to the RMCTRL.
 *
 * @param[out] pDest    RMCTRL structure to copy from.
 * @param[in]  pSrc   PMU-only sensed voltage structure to copy to.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pDest or @ref pSrc are NULL.
 * @return FLCN_OK                     If the sensed voltage data was successfully copied.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
voltRailSensedVoltageDataCopyOut
(
    LW2080_CTRL_VOLT_RAIL_SENSED_VOLTAGE_DATA   *pDest,
    VOLT_RAIL_SENSED_VOLTAGE_DATA               *pSrc
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       idx;

    // Sanity checking.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pDest == NULL) ||
         (pSrc  == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        voltRailSensedVoltageDataCopyOut_exit);

    pDest->mode       = pSrc->mode;
    pDest->voltageuV  = pSrc->voltageuV;
    pDest->numSamples = pSrc->numSamples;

    for (idx = 0; idx < pSrc->numSamples; idx++)
    {
        pDest->clkAdcAccSample[idx] = pSrc->pClkAdcAccSample[idx];
    }

voltRailSensedVoltageDataCopyOut_exit:
    return status;
}

#endif // G_VOLTRAIL_H
#endif // VOLTRAIL_H
