/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_DOMAIN_3X_PRIMARY_H
#define CLK_DOMAIN_3X_PRIMARY_H

/*!
 * @file clk_domain_3x_primary.h
 * @brief @copydoc clk_domain_3x_primary.c
 */

/* ------------------------ Includes --------------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Forward definition of CLK_DOMAIN_3X_PRIMARY structures so that can use the
 * type in interface definitions.
 */
typedef struct CLK_DOMAIN_3X_PRIMARY CLK_DOMAIN_3X_PRIMARY;

/*!
 * Clock Domain 3X Primary structure. Contains information specific to a Primary
 * Clock Domain.
 */
struct CLK_DOMAIN_3X_PRIMARY
{
    /*!
     * BOARDOBJ_INTERFACE super class. Must always be first object in the
     * structure.
     */
    BOARDOBJ_INTERFACE  super;

    /*!
     * Mask indexes of CLK_DOMAINs which are SECONDARIES to this PRIMARY CLK_DOMAIN.
     */
    BOARDOBJGRPMASK_E32 secondaryIdxsMask;
};

/*!
 * @memberof CLK_DOMAIN_3X_PRIMARY
 *
 * Main worker function for VF voltage look-up using linear search. Translates an
 * input voltage on a given voltage rail (@ref VOLT_RAIL) to a corresponding frequency
 * for this CLK_DOMAIN_3X_PROG, per the VF lwrve of this CLK_DOMAIN_3X_PROG.
 *
 * Takes care of handling PRIMARY vs. SECONDARY distinctions per the @ref clkDomIdx
 * argument.
 *
 * @param[in]  pDomain3XProg    CLK_DOMAIN_3X_PROG pointer
 * @param[in]  clkDomIdx
 *     Index of the CLK_DOMAIN for which to look-up.  The special value of @ref
 *     LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID indicates that the requested
 *     domain is the PRIMARY domain of this CLK_PROG_1X_PRIMARY.  Valid indexes
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
 * @param[in/out] pVfIterState
 *     Pointer to structure containing the VF iteration state for the VF
 *     look-up.  This tracks all data which must be preserved across this VF
 *     look-up and potentially across subsequent look-ups.  In cases where
 *     subsequent searches are in order of increasing input criteria, each
 *     subsequent look-up can "resume" where the prevoius look-up finished using
 *     this iteration state.  If no state tracking is desired, client should
 *     specify NULL.
 *
 * @return FLCN_OK
 *     The input voltage was successfully translated to an output frequency.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     The caller specified an invalid voltage (@ref pInput) outside the VF lwrve
 * @return Other errors
 *     An unexpected error oclwrred during look-up/translation.
 */
#define ClkDomain3XPrimaryVoltToFreqLinearSearch(fname) FLCN_STATUS (fname)(CLK_DOMAIN_3X_PRIMARY *pDomain3XPrimary, LwU8 clkDomIdx, LwU8 voltRailIdx, LwU8 voltageType, LW2080_CTRL_CLK_VF_INPUT *pInput, LW2080_CTRL_CLK_VF_OUTPUT *pOutput, LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState)

/*!
 * @memberof CLK_DOMAIN_3X_PRIMARY
 *
 * Main worker function for VF voltage look-up using linear search. Translates
 * an input frequency for this CLK_DOMAIN_3X_PROG to a corresponding voltage on
 * a given voltage rail (@ref VOLT_RAIL), per the VF lwrve of this CLK_DOMAIN_3X_PROG.
 *
 * Takes care of handling PRIMARY vs. SECONDARY distinctions per the @ref clkDomIdx
 * argument.
 *
 * @param[in]  pDomain3XProg    CLK_DOMAIN_3X_PROG pointer
 * @param[in]  clkDomIdx
 *     Index of the CLK_DOMAIN for which to look-up.  The special value of @ref
 *     LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID indicates that the requested
 *     domain is the PRIMARY domain of this CLK_PROG_1X_PRIMARY.  Valid indexes
 *     are specified for CLK_DOMAINs which are SECONDARIES to the PRIMARY CLK_DOMAIN.
 * @param[in]  voltRailIdx
 *     Index of the VOLT_RAIL for which to look-up.
 * @param[in]  voltageType
 *     Type of voltage value by which to look-up.  @ref
 *     LW2080_CTRL_CLK_VOLTAGE_TYPE_<xyz>.
 * @param[in]  pInput
 *     Pointer to structure specifying the frequency values to look-up.
 * @param[out] pOutput
 *     Pointer to structure in which to return the voltage value corresponding
 *     to the input frequency value.
 * @param[in/out] pVfIterState
 *     Pointer to structure containing the VF iteration state for the VF
 *     look-up.  This tracks all data which must be preserved across this VF
 *     look-up and potentially across subsequent look-ups.  In cases where
 *     subsequent searches are in order of increasing input criteria, each
 *     subsequent look-up can "resume" where the prevoius look-up finished using
 *     this iteration state.  If no state tracking is desired, client should
 *     specify NULL.
 *
 * @return FLCN_OK
 *     The input frequency was successfully translated to an output
 *     voltage.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     The caller specified an invalid frequency (@ref pInput) outside the
 *     VF lwrve
 * @return Other errors
 *     An unexpected error oclwrred during look-up/translation.
 */
#define ClkDomain3XPrimaryFreqToVoltLinearSearch(fname) FLCN_STATUS (fname)(CLK_DOMAIN_3X_PRIMARY *pDomain3XPrimary, LwU8 clkDomIdx, LwU8 voltRailIdx, LwU8 voltageType, LW2080_CTRL_CLK_VF_INPUT *pInput, LW2080_CTRL_CLK_VF_OUTPUT *pOutput, LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState)

/*!
 * @memberof CLK_DOMAIN_3X_PRIMARY
 *
 * Main worker function for VF voltage look-up using binary search. Translates an
 * input voltage on a given voltage rail (@ref VOLT_RAIL) to a corresponding frequency
 * for this CLK_DOMAIN_3X_PROG, per the VF lwrve of this CLK_DOMAIN_3X_PROG.
 *
 * Takes care of handling PRIMARY vs. SECONDARY distinctions per the @ref clkDomIdx
 * argument.
 *
 * @param[in]  pDomain3XProg    CLK_DOMAIN_3X_PROG pointer
 * @param[in]  clkDomIdx
 *     Index of the CLK_DOMAIN for which to look-up.  The special value of @ref
 *     LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID indicates that the requested
 *     domain is the PRIMARY domain of this CLK_PROG_1X_PRIMARY.  Valid indexes
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
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     The caller specified an invalid voltage (@ref pInput) outside the VF lwrve
 * @return Other errors
 *     An unexpected error oclwrred during look-up/translation.
 */
#define ClkDomain3XPrimaryVoltToFreqBinarySearch(fname) FLCN_STATUS (fname)(CLK_DOMAIN_3X_PRIMARY *pDomain3XPrimary, LwU8 clkDomIdx, LwU8 voltRailIdx, LwU8 voltageType, LW2080_CTRL_CLK_VF_INPUT *pInput, LW2080_CTRL_CLK_VF_OUTPUT *pOutput)

/*!
 * @memberof CLK_DOMAIN_3X_PRIMARY
 *
 * Main worker function for VF voltage look-up using binary search. Translates
 * an input frequency for this CLK_DOMAIN_3X_PROG to a corresponding voltage on
 * a given voltage rail (@ref VOLT_RAIL), per the VF lwrve of this CLK_DOMAIN_3X_PROG.
 *
 * Takes care of handling PRIMARY vs. SECONDARY distinctions per the @ref clkDomIdx
 * argument.
 *
 * @param[in]  pDomain3XProg    CLK_DOMAIN_3X_PROG pointer
 * @param[in]  clkDomIdx
 *     Index of the CLK_DOMAIN for which to look-up.  The special value of @ref
 *     LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID indicates that the requested
 *     domain is the PRIMARY domain of this CLK_PROG_1X_PRIMARY.  Valid indexes
 *     are specified for CLK_DOMAINs which are SECONDARIES to the PRIMARY CLK_DOMAIN.
 * @param[in]  voltRailIdx
 *     Index of the VOLT_RAIL for which to look-up.
 * @param[in]  voltageType
 *     Type of voltage value by which to look-up.  @ref
 *     LW2080_CTRL_CLK_VOLTAGE_TYPE_<xyz>.
 * @param[in]  pInput
 *     Pointer to structure specifying the frequency values to look-up.
 * @param[out] pOutput
 *     Pointer to structure in which to return the voltage value corresponding
 *     to the input frequency value.
 *
 * @return FLCN_OK
 *     The input frequency was successfully translated to an output
 *     voltage.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     The caller specified an invalid frequency (@ref pInput) outside the
 *     VF lwrve
 * @return Other errors
 *     An unexpected error oclwrred during look-up/translation.
 */
#define ClkDomain3XPrimaryFreqToVoltBinarySearch(fname) FLCN_STATUS (fname)(CLK_DOMAIN_3X_PRIMARY *pDomain3XPrimary, LwU8 clkDomIdx, LwU8 voltRailIdx, LwU8 voltageType, LW2080_CTRL_CLK_VF_INPUT *pInput, LW2080_CTRL_CLK_VF_OUTPUT *pOutput)

/*!
 * @memberof CLK_DOMAIN_3X_PRIMARY
 *
 * @brief Helper interface to adjust the given frequency by the frequency delta
 * of this CLK_DOMAIN_3X_PRIMARY object.  Will also include the global CLK_DOMAINS
 * frequency offset and corresponding CLK_PROG_1X_PRIMARY offset.  Will quantize
 * the result, if requested.

 * @param[in]     pDomain3XPrimary  CLK_DOMAIN_3X_PRIMARY pointer
 * @param[in]     pProg3XPrimary    CLK_PROG_3X_PRIMARY pointer
 * @param[in]     bQuantize
 *     Boolean indiciating whether the result should be quantized or not.  When
 *     CLK_DOMAIN_3X_SECONDARY objects call into this interface, they won't want the
 *     intermediate result to be quantized.  All other use-cases should request
 *     quanization.
 * @param[in,out] pFreqMHz
 *     Pointer in which caller specifies frequency to adjust and in which the
 *     adjusted frequency is returned.
 *
 * @return FLCN_OK
 *     Frequency successfully adjusted.
 * @return Other errors
 *     An unexpected error oclwrred during adjustment.
 */
#define ClkDomain3XPrimaryFreqAdjustDeltaMHz(fname) FLCN_STATUS (fname)(CLK_DOMAIN_3X_PRIMARY *pDomain3XPrimary, CLK_PROG_3X_PRIMARY *pProg3XPrimary, LwBool bQuantize, LwBool bVFOCAdjReq, LwU16 *pFreqMHz)

/*!
 * @memberof CLK_DOMAIN_3X_PRIMARY
 *
 * Helper fuction to get the max frequency supported by the given clock
 * domain's VF lwrve.
 * NOTE: The output of this funciton is the MIN(Each voltrail max freq)
 *
 * @param[in]   pDomain3XPrimary   CLK_DOMAIN_3X_PRIMARY pointer
 * @param[in]   clkDomIdx
 *     Index of the CLK_DOMAIN for which to look-up.  The special value of @ref
 *     LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID indicates that the requested
 *     domain is the PRIMARY domain of this CLK_PROG_1X_PRIMARY.  Valid indexes
 *     are specified for CLK_DOMAINs which are SECONDARIES to the PRIMARY CLK_DOMAIN.
 * @param[out]  pFreqMaxMHz       Output Max Freq value.
 *
 * @return FLCN_OK
 *     Successfully updated the ref@ pFreqMaxMHz with max freq value
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     The caller specified an invalid pointer.
 * @return Other errors
 *     An unexpected error oclwrred during VF look-up.
 */
#define ClkDomain3XPrimaryMaxFreqMHzGet(fname) FLCN_STATUS (fname)(CLK_DOMAIN_3X_PRIMARY *pDomain3XPrimary, LwU8 clkDomIdx, LwU32 *pFreqMaxMHz)

/*!
 * @memberof CLK_DOMAIN_3X_PRIMARY
 *
 * Helper interface to get the primary clock programming index that can be use
 * to program the given input secondary frequency.
 *
 * @param[in]   pDomain3XPrimary   CLK_DOMAIN_3X_PRIMARY pointer
 * @param[in]   secondaryClkDomIdx
 *     Index of the secondary CLK_DOMAIN for which to look-up.
 * @param[in]   freqMHz
 *     Input Frequency supported by the given secondary programming index to
 *     get the corresponding primary programming index.
 *
 * @return Primary clock programming index that can be used to program the given input secondary freq.
 * @return Other _ILWALID
 *     An unexpected error oclwrred during enumeration.
 */
#define ClkDomain3XPrimaryGetPrimaryProgIdxFromSecondaryFreqMHz(fname) LwU8 (fname)(CLK_DOMAIN_3X_PRIMARY *pDomain3XPrimary, LwU8 secondaryClkDomIdx, LwU16 freqMHz)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// OBJECT interfaces
BoardObjInterfaceConstruct              (clkDomainConstruct_3X_PRIMARY)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkDomainConstruct_3X_PRIMARY");

// CLK_DOMAIN_3X_PROG interfaces
ClkDomainProgVoltToFreq                 (clkDomainProgVoltToFreq_3X_PRIMARY)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgVoltToFreq_3X_PRIMARY");
ClkDomainProgFreqToVolt                 (clkDomainProgFreqToVolt_3X_PRIMARY)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgFreqToVolt_3X_PRIMARY");
ClkDomain3XProgVoltAdjustDeltauV        (clkDomain3XProgVoltAdjustDeltauV_PRIMARY)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain3XProgVoltAdjustDeltauV_PRIMARY");
ClkDomainProgVfMaxFreqMHzGet            (clkDomainProgMaxFreqMHzGet_3X_PRIMARY)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgMaxFreqMHzGet_3X_PRIMARY");
ClkDomain3XProgFreqAdjustDeltaMHz       (clkDomain3XProgFreqAdjustDeltaMHz_PRIMARY)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain3XProgFreqAdjustDeltaMHz_PRIMARY");

// CLK_DOMAIN_3X_PRIMARY interfaces
ClkDomain3XPrimaryVoltToFreqLinearSearch     (clkDomain3XPrimaryVoltToFreqLinearSearch)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain3XPrimaryVoltToFreqLinearSearch");
ClkDomain3XPrimaryFreqToVoltLinearSearch     (clkDomain3XPrimaryFreqToVoltLinearSearch)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain3XPrimaryFreqToVoltLinearSearch");
ClkDomain3XPrimaryVoltToFreqBinarySearch     (clkDomain3XPrimaryVoltToFreqBinarySearch)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain3XPrimaryVoltToFreqBinarySearch");
ClkDomain3XPrimaryFreqToVoltBinarySearch     (clkDomain3XPrimaryFreqToVoltBinarySearch)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain3XPrimaryFreqToVoltBinarySearch");
ClkDomain3XPrimaryFreqAdjustDeltaMHz         (clkDomain3XPrimaryFreqAdjustDeltaMHz)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain3XPrimaryFreqAdjustDeltaMHz");
ClkDomain3XPrimaryMaxFreqMHzGet              (clkDomain3XPrimaryMaxFreqMHzGet)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain3XPrimaryMaxFreqMHzGet");
ClkDomain3XPrimaryGetPrimaryProgIdxFromSecondaryFreqMHz
                                            (clkDomain3XPrimaryGetPrimaryProgIdxFromSecondaryFreqMHz)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain3XPrimaryGetPrimaryProgIdxFromSecondaryFreqMHz");
ClkDomain3XProgIsOVOCEnabled                (clkDomain3XProgIsOVOCEnabled_3X_PRIMARY)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain3XProgIsOVOCEnabled_3X_PRIMARY");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_DOMAIN_3X_PRIMARY_H
