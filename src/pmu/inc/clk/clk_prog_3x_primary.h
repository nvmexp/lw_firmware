/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_PROG_3X_PRIMARY_H
#define CLK_PROG_3X_PRIMARY_H

/*!
 * @file clk_prog_3x_primary.h
 * @brief @copydoc clk_prog_3x_primary.c
 */

/* ------------------------ Includes --------------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/*!
 * Accessor macro for CLK_PROG_3X_PRIMARY::freqDelta
 *
 * @return pointer to @ref CLK_PROG_3X_PRIMARY::freqDelta
 */
#define clkProg3XPrimaryFreqDeltaGet(pProg3XPrimary)                            \
    &(pProg3XPrimary)->freqDelta

/*!
 * Accessor macro for CLK_PROG_3X_PRIMARY::pVoltDeltauV[(voltRailIdx)]
 *
 * @param[in] pProg3XPrimary  CLK_PROG_3X_PRIMARY pointer
 * @param[in] voltRailIdx    VOLTAGE_RAIL index
 *
 * @return @ref CLK_PROG_3X_PRIMARY::pVoltDeltauV[(voltRailIdx)]
 */
#define clkProg3XPrimaryVoltDeltauVGet(pProg3XPrimary, voltRailIdx)             \
    (pProg3XPrimary)->pVoltDeltauV[(voltRailIdx)]

/*!
 * Accessor macro for CLK_PROG_3X_PRIMARY::bOCOVEnabled
 *
 * @return @ref CLK_PROG_3X_PRIMARY::bOCOVEnabled
 */
#define clkProg3XPrimaryOVOCEnabled(pProg3XPrimary)                             \
    (Clk.domains.bOverrideOVOC ||                                             \
     (pProg3XPrimary)->bOCOVEnabled)

/*!
 * Helper function Macro to test if RM is required to implement the VF smoothing
 * logic for a given VF point controlled by this primary clock programming entry
 *
 * @return LW_TRUE  if RM is required to implement VF smoothing
 * @return LW_FALSE if RM is NOT required to implement VF smoothing
 */
#define clkProg3XPrimaryIsVFSmoothingRequired(pProg3XPrimary, voltageuV)        \
    ((clkDomainsVfSmootheningEnforced())                                &&    \
     (clkDomainsVfMonotonicityEnforced())                               &&    \
     ((pProg3XPrimary)->sourceData.nafll.maxVFRampRate != 0)             &&    \
     ((voltageuV) >= (pProg3XPrimary)->sourceData.nafll.baseVFSmoothVoltuV))

/*!
 * Accessor macro for CLK_PROG_3X_PRIMARY::maxFreqStepSizeMHz
 *
 * @param[in] pProg3XPrimary  CLK_PROG_3X_PRIMARY pointer
 *
 * @return @ref CLK_PROG_3X_PRIMARY::maxFreqStepSizeMHz
 */
#define clkProg3XPrimaryMaxFreqStepSizeMHzGet(pProg3XPrimary)                   \
    (pProg3XPrimary)->sourceData.nafll.maxFreqStepSizeMHz

/*!
 * Accessor macro for CLK_PROG_3X_PRIMARY::pVfEntries[railIdx]->vfeIdx
 *
 * @param[in] pProg3XPrimary  CLK_PROG_3X_PRIMARY pointer
 * @param[in] railIdx        Voltage rail index
 *
 * @return @ref CLK_PROG_3X_PRIMARY::maxFreqStepSizeMHz
 */
#define clkProg3XPrimaryVfeIdxGet(pProg3XPrimary, railIdx)                      \
    (pProg3XPrimary)->pVfEntries[railIdx].vfeIdx

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock Programming entry.  Defines the how the RM will program a given
 * frequency on a clock domain per the VBIOS specification.
 *
 */
//! https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Clock_Programming_Table/1.0_Spec#Primary
struct CLK_PROG_3X_PRIMARY
{
    /*!
     * BOARDOBJ_INTERFACE super class. Must always be first object in the
     * structure.
     */
    BOARDOBJ_INTERFACE  super;

    /*!
     * Boolean flag indicating whether this entry supports OC/OV when those
     * settings are applied to the corresponding CLK_DOMAIN object.
     */
    LwBool              bOCOVEnabled;

    /*!
     * This will give the deviation of given freq from it's nominal value.
     * NOTE: we have one single freq delta that will apply to all voltage rails
     */
    LW2080_CTRL_CLK_FREQ_DELTA                      freqDelta;

    /*!
     * Array of VF entries.  Indexed per the voltage rail enumeration.
     *
     * Has valid indexes in the range [0, @ref
     * LW2080_CTRL_CLK_CLK_PROGS_INFO::numVfEntries).
     */
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY    *pVfEntries;

    /*!
     * This will give the deviation of given voltage from it's nominal value.
     */
    LwS32                                          *pVoltDeltauV;

    /*!
     * Union of source-specific data.
     */
    LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_SOURCE_DATA  sourceData;
};

/*!
 * @brief VF look-up function which takes an input voltage value on a given
 * voltage rail (@ref VOLT_RAIL) and finds the corresponding frequency on
 * the requested clock domain (@ref CLK_DOMAIN) using linear search.
 *
 * @memberof CLK_PROG_3X_PRIMARY
 *
 * @param[in]  pProg3XPrimary   CLK_PROG_3X_PRIMARY pointer
 * @param[in]  pDomain3XPrimary
 *     CLK_DOMAIN_3X_PRIMARY pointer to the PRIMARY CLK_DOMAIN which references
 *     this CLK_PROG_3X_PRIMARY object.
 * @param[in]  clkDomIdx
 *     Index of the CLK_DOMAIN for which to look-up.  The special value of @ref
 *     LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID indicates that the requested
 *     domain is the PRIMARY domain of this CLK_PROG_3X_PRIMARY.  Valid indexes
 *     are specified for CLK_DOMAINs which are SECONDARIES to the PRIMARY CLK_DOMAIN.
 * @param[in]  voltRailIdx
 *     Index of the VOLT_RAIL for which to look-up.
 * @param[in]  voltageType
 *     Type of voltage value by which to look-up.  @ref
 *     LW2080_CTRL_CLK_VOLTAGE_TYPE_<xyz>.
 * @param[in]  pInput
 *     Pointer to structure specifying the voltage value to look-up.
 * @param[out] pOutput
 *     Pointer to structure in which to return the frequency value corresponding
 *     to the input voltage value.
 * @param[in,out] pVfIterState
 *     Pointer to structure containing the VF iteration state for the VF
 *     look-up. This tracks all data which must be preserved across this VF
 *     look-up and potentially across subsequent look-ups. In cases where
 *     subsequent searches are in order of increasing input criteria, each
 *     subsequent look-up can "resume" where the previous look-up finished using
 *     this iteration state.
 *
 * @return FLCN_OK
 *     The input voltage was successfully translated to an output frequency.
 * @return FLCN_ERR_ITERATION_END
 *     This CLK_PROG_3X_PRIMARY encountered VF points above the @ref
 *     pInput->value. This means the search is done and can be
 *     short-cirlwited. The calling functions should stop their search. This
 *     is not an error, and the callers should colwert this code back to FLCN_OK.
 * @return Other errors
 *     An unexpected error oclwrred during look-up/translation.
 */
#define ClkProg3XPrimaryVoltToFreqLinearSearch(fname) FLCN_STATUS (fname)(CLK_PROG_3X_PRIMARY *pProg3XPrimary, CLK_DOMAIN_3X_PRIMARY *pDomain3XPrimary, LwU8 clkDomIdx, LwU8 voltRailIdx, LwU8 voltageType, LW2080_CTRL_CLK_VF_INPUT *pInput, LW2080_CTRL_CLK_VF_OUTPUT *pOutput, LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState)

/*!
 * @brief VF look-up function which takes an input voltage value on a given
 * voltage rail (@ref VOLT_RAIL) and finds the corresponding frequency on
 * the requested clock domain (@ref CLK_DOMAIN) using binary search.
 *
 * @memberof CLK_PROG_3X_PRIMARY
 *
 * @param[in]  pProg3XPrimary   CLK_PROG_3X_PRIMARY pointer
 * @param[in]  pDomain3XPrimary
 *     CLK_DOMAIN_3X_PRIMARY pointer to the PRIMARY CLK_DOMAIN which references
 *     this CLK_PROG_3X_PRIMARY object.
 * @param[in]  clkDomIdx
 *     Index of the CLK_DOMAIN for which to look-up.  The special value of @ref
 *     LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID indicates that the requested
 *     domain is the PRIMARY domain of this CLK_PROG_3X_PRIMARY.  Valid indexes
 *     are specified for CLK_DOMAINs which are SECONDARIES to the PRIMARY CLK_DOMAIN.
 * @param[in]  voltRailIdx
 *     Index of the VOLT_RAIL for which to look-up.
 * @param[in]  voltageType
 *     Type of voltage value by which to look-up.  @ref
 *     LW2080_CTRL_CLK_VOLTAGE_TYPE_<xyz>.
 * @param[in]  pInput
 *     Pointer to structure specifying the voltage value to look-up.
 * @param[out] pOutput
 *     Pointer to structure in which to return the frequency value corresponding
 *     to the input voltage value.
 *
 * @return FLCN_OK
 *     The input voltage was successfully translated to an output frequency.
 * @return FLCN_ERR_ITERATION_END
 *     This CLK_PROG_3X_PRIMARY encountered VF points above the @ref
 *     pInput->value. This means the search is done and can be
 *     short-cirlwited. The calling functions should stop their search. This
 *     is not an error, and the callers should colwert this code back to FLCN_OK.
 * @return Other errors
 *     An unexpected error oclwrred during look-up/translation.
 */
#define ClkProg3XPrimaryVoltToFreqBinarySearch(fname) FLCN_STATUS (fname)(CLK_PROG_3X_PRIMARY *pProg3XPrimary, CLK_DOMAIN_3X_PRIMARY *pDomain3XPrimary, LwU8 clkDomIdx, LwU8 voltRailIdx, LwU8 voltageType, LW2080_CTRL_CLK_VF_INPUT *pInput, LW2080_CTRL_CLK_VF_OUTPUT *pOutput)

/*!
 * @brief VF look-up function which takes an input frequency values on the
 * requested clock domain (@ref CLK_DOMAIN) and finds the corresponding voltage
 * on a given voltage rail (@ref VOLT_RAIL) using linear search.
 *
 * @memberof CLK_PROG_3X_PRIMARY
 *
 * @param[in]  pProg3XPrimary   CLK_PROG_3X_PRIMARY pointer
 * @param[in]  pDomain3XPrimary
 *     CLK_DOMAIN_3X_PRIMARY pointer to the PRIMARY CLK_DOMAIN which references
 *     this CLK_PROG_3X_PRIMARY object.
 * @param[in]  clkDomIdx
 *     Index of the CLK_DOMAIN for which to look-up.  The special value of @ref
 *     LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID indicates that the requested
 *     domain is the PRIMARY domain of this CLK_PROG_3X_PRIMARY.  Valid indexes
 *     are specified for CLK_DOMAINs which are SECONDARIES to the PRIMARY CLK_DOMAIN.
 * @param[in]  voltRailIdx
 *     Index of the VOLT_RAIL for which to look-up.
 * @param[in]  voltageType
 *     Type of voltage value by which to look-up.  @ref
 *     LW2080_CTRL_CLK_VOLTAGE_TYPE_<xyz>.
 * @param[in]  pInput
 *     Pointer to structure specifying the frequency value to look-up.
 * @param[out] pOutput
 *     Pointer to structure in which to return the voltage value corresponding
 *     to the input frequency value.
 * @param[in,out] pVfIterState
 *     Pointer to structure containing the VF iteration state for the VF
 *     look-up.  This tracks all data which must be preserved across this VF
 *     look-up and potentially across subsequent look-ups.  In cases where
 *     subsequent searches are in order of increasing input criteria, each
 *     subsequent look-up can "resume" where the previous look-up finished using
 *     this iteration state.
 *
 * @return FLCN_OK
 *     The input frequency was successfully translated to an output frequency
 *     voltage.
 * @return FLCN_ERR_ITERATION_END
 *     This CLK_PROG_3X_PRIMARY encountered VF points above the @ref
 *     pInput->value. This means the search is done and can be short-cirlwited.
 *     The calling functions should stop their search. This is not an error, and
 *     the callers should colwert this code back to FLCN_OK.
 * @return Other errors
 *     An unexpected error oclwrred during look-up/translation.
 */
#define ClkProg3XPrimaryFreqToVoltLinearSearch(fname) FLCN_STATUS (fname)(CLK_PROG_3X_PRIMARY *pProg3XPrimary, CLK_DOMAIN_3X_PRIMARY *pDomain3XPrimary, LwU8 clkDomIdx, LwU8 voltRailIdx, LwU8 voltageType, LW2080_CTRL_CLK_VF_INPUT *pInput, LW2080_CTRL_CLK_VF_OUTPUT *pOutput, LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState)


/*!
 * @brief VF look-up function which takes an input frequency values on the
 * requested clock domain (@ref CLK_DOMAIN) and finds the corresponding voltage
 * on a given voltage rail (@ref VOLT_RAIL) using binary search.
 *
 * @memberof CLK_PROG_3X_PRIMARY
 *
 * @param[in]  pProg3XPrimary   CLK_PROG_3X_PRIMARY pointer
 * @param[in]  pDomain3XPrimary
 *     CLK_DOMAIN_3X_PRIMARY pointer to the PRIMARY CLK_DOMAIN which references
 *     this CLK_PROG_3X_PRIMARY object.
 * @param[in]  clkDomIdx
 *     Index of the CLK_DOMAIN for which to look-up.  The special value of @ref
 *     LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID indicates that the requested
 *     domain is the PRIMARY domain of this CLK_PROG_3X_PRIMARY.  Valid indexes
 *     are specified for CLK_DOMAINs which are SECONDARIES to the PRIMARY CLK_DOMAIN.
 * @param[in]  voltRailIdx
 *     Index of the VOLT_RAIL for which to look-up.
 * @param[in]  voltageType
 *     Type of voltage value by which to look-up.  @ref
 *     LW2080_CTRL_CLK_VOLTAGE_TYPE_<xyz>.
 * @param[in]  pInput
 *     Pointer to structure specifying the frequency value to look-up.
 * @param[out] pOutput
 *     Pointer to structure in which to return the voltage value corresponding
 *     to the input frequency value.
 *
 * @return FLCN_OK
 *     The input frequency was successfully translated to an output frequency
 *     voltage.
 * @return FLCN_ERR_ITERATION_END
 *     This CLK_PROG_3X_PRIMARY encountered VF points above the @ref
 *     pInput->value. This means the search is done and can be short-cirlwited.
 *     The calling functions should stop their search. This is not an error, and
 *     the callers should colwert this code back to FLCN_OK.
 * @return Other errors
 *     An unexpected error oclwrred during look-up/translation.
 */
#define ClkProg3XPrimaryFreqToVoltBinarySearch(fname) FLCN_STATUS (fname)(CLK_PROG_3X_PRIMARY *pProg3XPrimary, CLK_DOMAIN_3X_PRIMARY *pDomain3XPrimary, LwU8 clkDomIdx, LwU8 voltRailIdx, LwU8 voltageType, LW2080_CTRL_CLK_VF_INPUT *pInput, LW2080_CTRL_CLK_VF_OUTPUT *pOutput)

/*!
 * @brief Translates the frequency on the PRIMARY CLK_DOMAIN to the corresponding
 * value on the SECONDARY CLK_DOMAIN per the semantics of the object's primary-secondary
 * relationship.
 *
 * For example, for the _RATIO class this will do
 * @code
 * freq_{secondary} = freq_{primary} * ratio
 * @endcode
 * and for the _TABLE class this will do the table lookup.
 *
 * @memberof CLK_PROG_3X_PRIMARY
 *
 * @param[in]     pProg3XPrimary   CLK_PROG_3X_PRIMARY pointer
 * @param[in]     pDomain3XPrimary
 *     CLK_DOMAIN_3X_PRIMARY pointer to the PRIMARY CLK_DOMAIN which references
 *     this CLK_PROG_3X_PRIMARY object.
 * @param[in]     clkDomIdx
 *     Index of the CLK_DOMAIN for which to look-up.  The special value of @ref
 *     LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID indicates that the requested
 *     domain is the PRIMARY domain of this CLK_PROG_3X_PRIMARY.  Valid indexes
 *     are specified for CLK_DOMAINs which are SECONDARIES to the PRIMARY CLK_DOMAIN.
 * @param[in,out] pFreqMHz
 *     Pointer in which caller specifies the input frequency to translate and in
 *     which the function will return the translated frequency.
 * @param[in]     bReqFreqDeltaAdj
 *     Request to apply frequency OC adjustment on secondary frequency.
 * @param[in]     bQuantize
 *     Request to quantize secondary frequency.
 *
 * @return FLCN_OK
 *     Frequency successfully translated per the primary->secondary relationship.
 * @return FLCN_ERR_NOT_SUPPORTED
 *     This CLK_PROG_3X_PRIMARY object does not implement this interface.  This
 *     is a coding error.
 */
#define ClkProg3XPrimaryFreqTranslatePrimaryToSecondary(fname) FLCN_STATUS (fname)(CLK_PROG_3X_PRIMARY *pProg3XPrimary, CLK_DOMAIN_3X_PRIMARY *pDomain3XPrimary, LwU8 clkDomIdx, LwU16 *pFreqMHz, LwBool bReqFreqDeltaAdj, LwBool bQuantize)

/*!
 * @brief Translates the frequency on the SECONDARY CLK_DOMAIN to the corresponding
 * value on the PRIMARY CLK_DOMAIN per the semantics of the object's primary-secondary
 * relationship.
 *
 * @memberof CLK_PROG_3X_PRIMARY
 *
 * @param[in]     pProg3XPrimary   CLK_PROG_3X_PRIMARY pointer
 * @param[in]     pDomain3XPrimary
 *     CLK_DOMAIN_3X_PRIMARY pointer to the PRIMARY CLK_DOMAIN which references
 *     this CLK_PROG_3X_PRIMARY object.
 * @param[in]     clkDomIdx
 *     Index of the CLK_DOMAIN for which to look-up.  The special value of @ref
 *     LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID indicates that the requested
 *     domain is the PRIMARY domain of this CLK_PROG_3X_PRIMARY.  Valid indexes
 *     are specified for CLK_DOMAINs which are SECONDARIES to the PRIMARY CLK_DOMAIN.
 * @param[in,out] pFreqMHz
 *     Pointer in which caller specifies the input frequency to translate and in
 *     which the function will return the translated frequency.
 *
 * @return FLCN_OK
 *     Frequency successfully translated per the primary->secondary relationship.
 * @return FLCN_ERR_NOT_SUPPORTED
 *     This CLK_PROG_3X_PRIMARY object does not implement this interface.  This
 *     is a coding error.
 */
#define ClkProg3XPrimaryFreqTranslateSecondaryToPrimary(fname) FLCN_STATUS (fname)(CLK_PROG_3X_PRIMARY *pProg3XPrimary, CLK_DOMAIN_3X_PRIMARY *pDomain3XPrimary, LwU8 clkDomIdx, LwU16 *pFreqMHz)

/*!
 * @brief Helper function to get the max frequency supported by the given clock
 * programming entry's VF lwrve.
 *
 * @memberof CLK_PROG_3X_PRIMARY
 *
 * @param[in]     pProg3XPrimary   CLK_PROG_3X_PRIMARY pointer
 * @param[in]     pDomain3XPrimary
 *     CLK_DOMAIN_3X_PRIMARY pointer to the PRIMARY CLK_DOMAIN which references
 *     this CLK_PROG_3X_PRIMARY object.
 * @param[in]     clkDomIdx
 *     Index of the CLK_DOMAIN for which to look-up.  The special value of @ref
 *     LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID indicates that the requested
 *     domain is the PRIMARY domain of this CLK_PROG_3X_PRIMARY.  Valid indexes
 *     are specified for CLK_DOMAINs which are SECONDARIES to the PRIMARY CLK_DOMAIN.
 * @param[out]    pFreqMaxMHz     Output Max Freq value.
 * @param[in,out] pVfPair
 *     Pointer to LW2080_CTRL_CLK_VF_PAIR structure in which the caller provides
 *     the last VF pair and in which this interface will return the current VF
 *     pair to be the next last VF pair.
 *
 * @return FLCN_OK
 *     Successfully updated the ref@ pFreqMaxMHz with max freq value
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     The caller specified an invalid pointer.
 * @return Other errors
 *     An unexpected error oclwrred during VF look-up.
 */
#define ClkProg3XPrimaryMaxFreqMHzGet(fname) FLCN_STATUS (fname)(CLK_PROG_3X_PRIMARY *pProg3XPrimary, CLK_DOMAIN_3X_PRIMARY *pDomain3XPrimary, LwU8 clkDomIdx, LwU32 *pFreqMaxMHz, LW2080_CTRL_CLK_VF_PAIR *pVfPair)

/*!
 * @memberof CLK_PROG_3X_PRIMARY
 *
 * @param[in]  pProg3XPrimary   CLK_PROG_3X_PRIMARY pointer
 * @param[in]  pDomain3XPrimary
 *     CLK_DOMAIN_3X_PRIMARY pointer to the PRIMARY CLK_DOMAIN which references
 *     this CLK_PROG_3X_PRIMARY object.
 * @param[in]  secondaryClkDomIdx  Secondary clock domain index
 * @param[in]  freqMHz
 *     Input Frequency supported by the given secondary programming index to
 *     get the corresponding primary programming index.
 *
 * @return
 *  LW_TRUE  If given Primary clock programming index can be used to program the
 *           given input secondary freq.
 *  LW_FALSE Otherwise.
 */
#define ClkProg3XPrimaryIsPrimaryProgIdxForSecondaryFreqMHz(fname) LwBool (fname)(CLK_PROG_3X_PRIMARY *pProg3XPrimary, CLK_DOMAIN_3X_PRIMARY *pDomain3XPrimary, LwU8 secondaryClkDomIdx, LwU16 freqMHz)

/*!
 * @brief Helper function to update the NAFLL clock domain's frequency max value
 * based on the applied VF point delta.
 *
 * This interface is being used by the PERF PSTATE to update the max freq range
 * with the max valid CLK_VF_POINT_VOLT::freqDeltakzHz.
 *
 * @memberof CLK_PROG_3X_PRIMARY
 *
 * @param[in]  pProg3XPrimary   CLK_PROG_3X_PRIMARY pointer
 * @param[in]  pDomain3XPrimary CLK_DOMAIN_3X_PRIMARY pointer.
 * @param[in]  pFreqMaxMHz
 *      IN :: PSTATE max frequency range value for NAFLL clock domain
 *      OUT:: max valid CLK_VF_POINT_VOLT::freqDeltakzHz adjusted max frequency
 *            range value
 *
 * @return FLCN_OK
 *     Max freq range is successfully adjusted with max valid
 *     CLK_VF_POINT_VOLT::freqDeltakzHz
 * @return Other errors
 *     An unexpected error oclwrred during adjustment.
 */
#define ClkProg3XPrimaryMaxVFPointFreqDeltaAdj(fname) FLCN_STATUS (fname)(CLK_PROG_3X_PRIMARY *pProg3XPrimary, CLK_DOMAIN_3X_PRIMARY *pDomain3XPrimary, LwU16 *pFreqMaxMHz)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// OBJECT interfaces
BoardObjInterfaceConstruct (clkProgConstruct_3X_PRIMARY)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkProgConstruct_3X_PRIMARY");

// CLK_PROG_3X_PRIMARY interfaces
ClkProg3XPrimaryVoltToFreqLinearSearch     (clkProg3XPrimaryVoltToFreqLinearSearch)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkProg3XPrimaryVoltToFreqLinearSearch");
ClkProg3XPrimaryFreqToVoltLinearSearch     (clkProg3XPrimaryFreqToVoltLinearSearch)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkProg3XPrimaryFreqToVoltLinearSearch");
ClkProg3XPrimaryVoltToFreqBinarySearch     (clkProg3XPrimaryVoltToFreqBinarySearch)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkProg3XPrimaryVoltToFreqBinarySearch");
ClkProg3XPrimaryFreqToVoltBinarySearch     (clkProg3XPrimaryFreqToVoltBinarySearch)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkProg3XPrimaryFreqToVoltBinarySearch");
ClkProg3XPrimaryFreqTranslatePrimaryToSecondary (clkProg3XPrimaryFreqTranslatePrimaryToSecondary)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkProg3XPrimaryFreqTranslatePrimaryToSecondary");
ClkProg3XPrimaryFreqTranslateSecondaryToPrimary (clkProg3XPrimaryFreqTranslateSecondaryToPrimary)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkProg3XPrimaryFreqTranslateSecondaryToPrimary");
ClkProg3XPrimaryMaxFreqMHzGet              (clkProg3XPrimaryMaxFreqMHzGet)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkProg3XPrimaryMaxFreqMHzGet");
ClkProg3XPrimaryIsPrimaryProgIdxForSecondaryFreqMHz
                                          (clkProg3XPrimaryIsPrimaryProgIdxForSecondaryFreqMHz)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkProg3XPrimaryIsPrimaryProgIdxForSecondaryFreqMHz");
ClkProg3XPrimaryMaxVFPointFreqDeltaAdj     (clkProg3XPrimaryMaxVFPointFreqDeltaAdj)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkProg3XPrimaryMaxVFPointFreqDeltaAdj");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_PROG_3X_PRIMARY_H
