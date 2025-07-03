/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_DOMAIN_PROG_H
#define CLK_DOMAIN_PROG_H

#include "g_clk_domain_prog.h"

#ifndef G_CLK_DOMAIN_PROG_H
#define G_CLK_DOMAIN_PROG_H

/*!
 * @file clk_domain_prog.h
 * @brief @copydoc clk_domain_prog.c
 */

/* ------------------------ Includes --------------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/*!
 * This MACRO is used to initialize the @ref pFreqMHz to start iterating the
 * frequencies supported by the clock domain using the function
 * @ref ClkDomainFreqsIterate
 */
#define CLK_DOMAIN_PROG_ITERATE_MAX_FREQ                                     0U


/*!
 * Helper macro to init at @ref CLK_DOMAIN_PROG_FREQ_PROP_IN_OUT_TUPLE struct.
 *
 * @param[in] pFreqPropTuple
 *     Pointer to CLK_DOMAIN_PROG_FREQ_PROP_IN_OUT_TUPLE struct to init.
 */
#define CLK_DOMAIN_PROG_FREQ_PROP_IN_OUT_TUPLE_INIT(pFreqPropTuple)              \
    do {                                                                         \
        (pFreqPropTuple)->pDomainsMaskIn  = NULL;                                \
        (pFreqPropTuple)->pDomainsMaskOut = NULL;                                \
        memset((pFreqPropTuple)->freqMHz, 0x0,                                   \
            sizeof(LwU16) * LW2080_CTRL_CLK_CLK_DOMAIN_CLIENT_MAX_DOMAINS); \
    } while (LW_FALSE)

/*!
 * @brief Helper macro to get the volt rail vmin mask for given programmable
 *        clock domain.
 *
 * @param[in] pDomainProg   Programmable Clock Domain pointer.
 *
 * @return  Pointer to @ref voltRailVminMask
 */
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_PROG_VOLT_RAIL_VMIN_MASK)
#define CLK_DOMAIN_PROG_VOLT_RAIL_VMIN_MASK_GET(pDomainProg)                  \
            (&((pDomainProg)->voltRailVminMask))
#else
#define CLK_DOMAIN_PROG_VOLT_RAIL_VMIN_MASK_GET(pDomainProg)                  \
            ((BOARDOBJGRPMASK_E32 *)NULL)
#endif

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Forward definition of CLK_DOMAIN_PROG structures so that can use the
 * type in interface definitions.
 */
typedef struct CLK_DOMAIN_PROG CLK_DOMAIN_PROG;

/*!
 * Clock Domain Prog structure. Contains information specific to a
 * Programmable Clock Domain.
 */
struct CLK_DOMAIN_PROG
{
    /*!
     * BOARDOBJ_INTERFACE super class. Must always be first object in the
     * structure.
     */
    BOARDOBJ_INTERFACE  super;

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_PROG_VOLT_RAIL_VMIN_MASK)
    /*!
     * Mask of VOLT_RAILs on which the given CLK_DOMAIN_PROG has a
     * Vmin, and for which a VF lwrve has been specified.
     */
    BOARDOBJGRPMASK_E32 voltRailVminMask;
#endif
};

/*!
 * Struct containing in-out tuple of data required for clock propagation.
 */
typedef struct
{
    /*!
     * Mask of input clock domains. At least one programmable clock domain
     * mask bit must be set.
     */
    BOARDOBJGRPMASK_E32 *pDomainsMaskIn;

    /*!
     * Mask of output clock domains.
     */
    BOARDOBJGRPMASK_E32 *pDomainsMaskOut;

    /*!
     * Clock Frequency Tuple. The client must provide frequency value for input
     * clock domains whose mask bit is set in Input Mask @ref pDomainsMaskIn.
     * The clock domain prog interface shall propagate from input clock domains
     * to all other expected output clock domains whose mask bit are set in the
     * @ref pDomainsMaskOut mask to generate complete clock frequency tuple.
     */
    LwU16 freqMHz[LW2080_CTRL_CLK_CLK_DOMAIN_CLIENT_MAX_DOMAINS];
} CLK_DOMAIN_PROG_FREQ_PROP_IN_OUT_TUPLE;

/*!
 * @memberof CLK_DOMAIN_PROG
 *
 * @brief Main entry point to VF voltage look-up function. Translates an input
 * voltage on a give voltage rail (@ref VOLT_RAIL) to a corresponding frequency
 * for this CLK_DOMAIN_PROG, per the VF lwrve of this CLK_DOMAIN_PROG.
 *
 * Clients to the VF look-up functions should call this function directly, this
 * function will take care of interpreting the various implementation details of
 * how the VF lwrve (e.g. primary-secondary relationships, etc.)
 *
 * @param[in]  pDomainProg  CLK_DOMAIN_PROG pointer
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
 *     look-up.  This tracks all data which must be preserved across this VF
 *     look-up and potentially across subsequent look-ups.  In cases where
 *     subsequent searches are in order of increasing input criteria, each
 *     subsequent look-up can "resume" where the previous look-up finished using
 *     this iteration state.  If no state tracking is desired, client should
 *     specify NULL.
 *     This param is deprecated on VF 4.0+, it will be passed as NULL.
 *
 * @return FLCN_OK
 *     The input voltage was successfully translated to an output frequency.
 * @return Other errors
 *     An unexpected error oclwrred during look-up/translation.
 */
#define ClkDomainProgVoltToFreq(fname) FLCN_STATUS (fname)(CLK_DOMAIN_PROG *pDomainProg, LwU8 voltRailIdx, LwU8 voltageType, LW2080_CTRL_CLK_VF_INPUT *pInput, LW2080_CTRL_CLK_VF_OUTPUT *pOutput, LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState)

/*!
 * @memberof CLK_DOMAIN_PROG
 *
 * @brief Main entry point to VF voltage look-up function.
 *
 * Translates an input frequency for this CLK_DOMAIN_PROG to a corresponding
 * voltage on a given voltage rail (@ref VOLT_RAIL), per the VF lwrve of this
 * CLK_DOMAIN_PROG.
 *
 * Clients to the VF look-up functions should call this function directly, this
 * function will take care of interpreting the various implementation details of
 * how the VF lwrve (e.g. primary-secondary relationships, etc.)
 *
 * @param[in]  pDomainProg  CLK_DOMAIN_PROG pointer
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
 *     this iteration state.  If no state tracking is desired, client should
 *     specify NULL.
 *     This param is deprecated on VF 4.0+, it will be passed as NULL.
 *
 * @return FLCN_OK
 *     The input frequency was successfully translated to an output voltage.
 * @return Other errors
 *     An unexpected error oclwrred during look-up/translation.
 */
#define ClkDomainProgFreqToVolt(fname) FLCN_STATUS (fname)(CLK_DOMAIN_PROG *pDomainProg, LwU8 voltRailIdx, LwU8 voltageType, LW2080_CTRL_CLK_VF_INPUT *pInput, LW2080_CTRL_CLK_VF_OUTPUT *pOutput, LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState)

/*!
 * @memberof CLK_DOMAIN_PROG
 *
 * @brief Main entry point to VF voltage look-up function. Translates an input
 * voltage on a give voltage rail (@ref VOLT_RAIL) to a corresponding frequency
 * tuple for this CLK_DOMAIN_PROG, per the VF lwrve of this CLK_DOMAIN_PROG.
 *
 * Clients to the VF look-up functions should call this function directly, this
 * function will take care of interpreting the various implementation details of
 * how the VF lwrve (e.g. primary-secondary relationships, etc.)
 *
 * @param[in]  pDomainProg    CLK_DOMAIN_PROG pointer
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
 *     subsequent look-up can "resume" where the previous look-up finished using
 *     this iteration state. If no state tracking is desired, client should
 *     specify NULL.
 *
 * @return FLCN_OK
 *     The input voltage was successfully translated to an output frequency.
 * @return Other errors
 *     An unexpected error oclwrred during look-up/translation.
 */
#define ClkDomainProgVoltToFreqTuple(fname) FLCN_STATUS (fname)(CLK_DOMAIN_PROG *pDomainProg, LwU8 voltRailIdx, LwU8 voltageType, LW2080_CTRL_CLK_VF_INPUT *pInput, LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE *pOutput, LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState)

/*!
 * @memberof CLK_DOMAIN_PROG
 *
 * @brief Iterate over the frequency enum (@ref pFreqMHz) from highest to lowest
 * supported on the given Clock Domain (@ref pDomainProg).
 *
 * @param[in]       pDomainProg CLK_DOMAIN_PROG pointer
 * @param[in,out]   pFreqMHz
 *     Pointer to the buffer which is used to iterate over the frequency. The IN
 *     is the last frequency value that we read where as the OUT is the next freq
 *     value that follows the last frequency value represented by @ref pFreqMHz.
 *     Note we are iterating from highest -> lowest freq value.
 *
 * NOTE: If the initial value for freq IN is "CLK_DOMAIN_PROG_ITERATE_MAX_FREQ"
 * then the corresponding OUT value will be the MAX freq supported by the given
 * clock domain.
 *
 * @return FLCN_OK
 *     Frequency successfully updated with the next highest value.
 * @return FLCN_ERR_ITERATION_END
 *     Frequency value reached the minimum freq supported by the clock domain.
 * @return Other errors
 *     An unexpected error oclwrred during iteration.
 */
#define ClkDomainProgFreqEnumIterate(fname) FLCN_STATUS (fname)(CLK_DOMAIN_PROG *pDomainProg, LwU16 *pFreqMHz)

/*!
 * @memberof CLK_DOMAIN_PROG
 *
 * @brief Helper interface to get the frequency of a given clock domain based
 * on input index in clock domain frequency enumeration array.
 *
 * @param[in]   pDomainProg     CLK_DOMAIN_PROG pointer
 * @param[in]   idx             Index into clock domain freq enumeration array
 *
 * @return Frequency in MHz
 *
 * @return LW_U16_MAX
 *     Error while finding the frequency corresponding to the input index.
 */
#define ClkDomainProgGetFreqMHzByIndex(fname) LwU16 (fname)(CLK_DOMAIN_PROG *pDomainProg, LwU16 idx)

/*!
 * @memberof CLK_DOMAIN_PROG
 *
 * @brief Helper interface to get the index at which the input frequency is
 * mapped to in the clock domain frequency enumeration array.
 *
 * @param[in]   pDomainProg     CLK_DOMAIN_PROG pointer
 * @param[in]   freqMHz         Frequency in MHz.
 * @param[in]   bFloor
 *     Boolean indicating whether the index should be found via a floor
 *     (LW_TRUE) or ceiling (LW_FALSE) to the next closest supported value.
 *
 * @return Index at which the input frequency is mapped.
 *
 * @return LW_U16_MAX
 *     Error while finding the index for the input frequency value.
 */
#define ClkDomainProgGetIndexByFreqMHz(fname) LwU16 (fname)(CLK_DOMAIN_PROG *pDomainProg, LwU16 freqMHz, LwBool bFloor)

/*!
 * @memberof CLK_DOMAIN_PROG
 *
 * @brief Helper fuction to get the max frequency supported by the given clock
 * domain's VF lwrve.
 *
 * @note The output of this function is the MIN(Each voltrail max freq)
 *
 * @param[in]   pDomainProg     CLK_DOMAIN_PROG pointer
 * @param[out]  pFreqMaxMHz     Output Max Freq value.
 *
 * @return FLCN_OK
 *     Successfully updated the @ref pFreqMaxMHz with max freq value
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     The caller specified an invalid pointer.
 * @return Other errors
 *     An unexpected error oclwrred during VF look-up.
 */
#define ClkDomainProgVfMaxFreqMHzGet(fname) FLCN_STATUS (fname)(CLK_DOMAIN_PROG *pDomainProg, LwU32 *pFreqMaxMHz)

/*!
 * @memberof CLK_DOMAIN_PROG
 *
 * @brief Quantizes a given frequency (pFreqMHz) based on the given settings of
 * this CLK_PROG_1X object.
 *
 * @param[in]     pDomainProg   CLK_DOMAIN_PROG pointer
 * @param[in]     pInputFreq    LW2080_CTRL_CLK_VF_INPUT pointer
 *     Pointer to LW2080_CTRL_CLK_VF_INPUT struct which contains frequency (MHz)
 *     to quantize.
 * @param[out]    pOutputFreq   LW2080_CTRL_CLK_VF_OUTPUT pointer
 *     Pointer to LW2080_CTRL_CLK_VF_OUTPUT struct which has the quantized
 *     frequency (MHz)
 * @param[in]     bFloor
 *     Boolean indicating whether the frequency should be quantized via a floor
 *     (LW_TRUE) or ceiling (LW_FALSE) to the next closest supported value.
 * @param[in]     bBound
 *      Boolean indicating whether the frequency should be bounded by offset
 *      adjusted POR frequency ranges. VF Generation prefer not to bound as
 *      POR frequency range are not adjusted with OC until it is completed.
 *
 * @return FLCN_OK
 *     Frequency value successfully quantized.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Frequency value could not be quantized because there is not corresponding
 *     frequency value specified in the POR - i.e. the requested frequency was
 *     too low.
 */
#define ClkDomainProgFreqQuantize(fname) FLCN_STATUS (fname)(CLK_DOMAIN_PROG *pDomainProg, LW2080_CTRL_CLK_VF_INPUT *pInputFreq, LW2080_CTRL_CLK_VF_OUTPUT *pOutputFreq, LwBool bFloor, LwBool bBound)

/*!
 * @memberof CLK_DOMAIN_PROG
 *
 * @brief Client interface to apply frequency OC adjustment on input frequency.
 *
 * This interface will be used by client's of VF lwrve.
 *
 * @param[in]       pDomainProg     CLK_DOMAIN_PROG pointer
 * @param[in,out]   pFreqMHz        Input frequency to adjust
 *
 * @return FLCN_OK
 *     Frequency successfully adjusted with the OC offsets.
 * @return Other errors
 *     An unexpected error oclwrred during adjustments.
 */
#define ClkDomainProgClientFreqDeltaAdj(fname) FLCN_STATUS (fname)(CLK_DOMAIN_PROG *pDomainProg, LwU16 *pFreqMHz)

/*!
 * @memberof CLK_DOMAIN_PROG
 *
 * @brief Interface to apply factory frequency OC adjustment on input frequency.
 *
 * This interface will be used by client's of VF lwrve.
 *
 * @param[in]       pDomainProg     CLK_DOMAIN_PROG pointer
 * @param[in,out]   pFreqMHz        Input frequency to adjust
 *
 * @return FLCN_OK
 *     Frequency successfully adjusted with the factory OC offsets.
 * @return Other errors
 *     An unexpected error oclwrred during adjustments.
 */
#define ClkDomainProgFactoryDeltaAdj(fname) FLCN_STATUS (fname)(CLK_DOMAIN_PROG *pDomainProg, LwU16 *pFreqMHz)

/*!
 * @memberof CLK_DOMAIN_PROG
 *
 * @brief Interface to check whether the clock domain is noise aware for the
 * given input frequency.
 *
 * @param[in]       pDomainProg     CLK_DOMAIN_PROG pointer
 * @param[in,out]   freqMHz         Input frequency in MHz
 *
 * @return LW_TRUE
 *     If the clock domain is noise aware for the given input frequency.
 * @return LW_FALSE
 *     Otherwise
 */
#define ClkDomainProgIsFreqMHzNoiseAware(fname) LwBool (fname)(CLK_DOMAIN_PROG *pDomainProg, LwU32 freqMHz)

/*!
 * @memberof CLK_DOMAIN_PROG
 *
 * @brief Helper interface to get the pre-volt ordering index.
 *
 * @param[in]       pDomainProg     CLK_DOMAIN_PROG pointer
 *
 * @return pre-volt ordering index.
 */
#define ClkDomainProgPreVoltOrderingIndexGet(fname)     LwU8 (fname)(CLK_DOMAIN_PROG *pDomainProg)

/*!
 * @memberof CLK_DOMAIN_PROG
 *
 * @brief Helper interface to get the post-volt ordering index.
 *
 * @param[in]       pDomainProg     CLK_DOMAIN_PROG pointer
 *
 * @return post-volt ordering index.
 */
#define ClkDomainProgPostVoltOrderingIndexGet(fname)    LwU8 (fname)(CLK_DOMAIN_PROG *pDomainProg)

/*!
 * @interface CLK_DOMAIN_PROG
 *
 * Public interface to propage frequency from the set of specific input
 * clock domain's frequency to the set of requested output clock domain's
 * frequency.
 *
 * @note
 *  The clock propagation will not modify the input clock domain freq.
 *  In cases where there is more than one input clock domain, the clock
 *  propagation will set the output frequency to max of all propagated freq
 *  for that output clock domain.
 *
 * @param[in/out]   pFreqPropTuple  Frequency tuple.
 * @param[in]       clkPropTopIdx   LwBoardObjIdx to CLK_PROP_TOP
 *
 * @return FLCN_OK
 *     Successfully populated @ref pFreqPropTuple with propagated freq values.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     The caller specified an invalid pointer.
 * @return Other errors
 *     An unexpected error oclwrred during function call.
 */
#define ClkDomainsProgFreqPropagate(fname) FLCN_STATUS (fname)(CLK_DOMAIN_PROG_FREQ_PROP_IN_OUT_TUPLE *pFreqPropTuple, LwBoardObjIdx clkPropTopIdx)

/*!
 * @memberof CLK_DOMAIN_PROG
 *
 * @brief Helper interface to get the voltage rail index that drives this clock domain.
 *        If the clock domain is powered by more than one rail it will return INVALID
 *        rail index.
 *
 * @param[in]       pDomainProg     CLK_DOMAIN_PROG pointer
 *
 * @return voltage rail index.
 */
#define ClkDomainProgVoltRailIdxGet(fname)    LwU8 (fname)(CLK_DOMAIN_PROG *pDomainProg)

/*!
 * @memberof CLK_DOMAIN_PROG
 *
 * @brief Returns whether FBVDD data is available and valid for a given
 *        @ref CLK_DOMAIN_PROG
 *
 * @param[in]   pDomainProg CLK_DOMAIN_PROG pointer
 * @param[out]  pbValid     Whether the data is valid
 *
 * @return @ref FLCN_OK Success
 * @return Others       Implementation-specific errors.
 */
#define ClkDomainProgFbvddDataValid(fname) FLCN_STATUS (fname)(CLK_DOMAIN_PROG *pDomain, LwBool *pbValid)

/*!
 * @memberof CLK_DOMAIN_PROG
 *
 * @brief Adjusts a given FB power measurement from a reference measurement to
 *        a measurement appropriate for the actual memory configuration or
 *        vice-versa
 *
 * @param[in]       pDomainProg     CLK_DOMAIN_PROG pointer
 * @param[in,out]   pFbPwrmW        On input, FB power in milliwatts to be adjusted; on
 *                                  output, adjusted FB power
 * @param[in]       bFromReference  Whether the adjustment is to be made from a
 *                                  reference measurement or to a reference
 *                                  measurement.
 *
 * @return @ref FLCN_OK Success
 * @return Others       Implementation-specific errors.
 */
#define ClkDomainProgFbvddPwrAdjust(fname) FLCN_STATUS (fname)(CLK_DOMAIN_PROG *pDomain, LwU32 *pFbPwrmW, LwBool bFromReference)

/*!
 * @brief Looks up the FBVDD voltage used with a given DRAMCLK frequency.
 *
 * @param[in]   pDomainProg CLK_DOMAIN_PROG pointer
 * @param[in]   pInput      Input structure containing DRAMCLK frequency in kHz
 * @param[out]  pOutput     Output structure into which to place FBVDD voltage
 *                          used with DRAMCLK in uV
 *
 * @return @ref FLCN_OK                 Success
 * @return @ref FLCN_ERR_NOT_SUPPORTED  CLK_DOMAIN_PROG type does not support
 *                                      the interface.
 * @return Others                       Implementation-specific errors.
 */
#define ClkDomainProgFbvddFreqToVolt(fname) FLCN_STATUS (fname)(CLK_DOMAIN_PROG *pDomain, const LW2080_CTRL_CLK_VF_INPUT *pInput, LW2080_CTRL_CLK_VF_OUTPUT *pOutput)

/*!
 * @brief Checks if the CLK_DOMAIN_PROG is noise aware.
 *
 * @param[in]   pDomainProg        CLK_DOMAIN_PROG pointer
 * @param[out]  pBIsNoiseAware Bool indicatig if clock domain is noise aware
 *
 * @return @ref FLCN_OK                 Success
 * @return @ref FLCN_ERR_NOT_SUPPORTED  CLK_DOMAIN type does not support
 *                                      the interface.
 * @return Others                       Implementation-specific errors.
 */
#define ClkDomainProgIsNoiseAware(fname) FLCN_STATUS (fname)(CLK_DOMAIN_PROG *pDomainProg, LwBool *pbIsNoiseAware)

/*!
 * @memberof CLK_DOMAIN_PROG
 *
 * @brief   Helper interface to obtain whether particular
 *          clock domain has secondary VF lwrves enabled
 *          This information taken from Clocks HAL.
 *
 * @param[in]       pDomainProg                CLK_DOMAIN_PROG pointer.
 * @param[out]      pBIsSecVFLwrvesEnabled     Pointer to boolean to indicate
 *                                             whether secondary VF lwrve
 *                                             is enabled.
 *
 * @return @ref FLCN_OK                 Success
 * @return Others                       Implementation-specific errors.
 */
#define ClkDomainProgIsSecVFLwrvesEnabled(fname)    FLCN_STATUS (fname)(CLK_DOMAIN_PROG *pDomainProg, LwBool *pBIsSecVFLwrvesEnabled)

/*!
 * @memberof CLK_DOMAIN_PROG
 *
 * @brief   Helper interface to obtain whether particular
 *          clock domain has secondary VF lwrves enabled
 *          This information taken from Clocks HAL.
 *          This is a wrapper around @ref ClkDomainProgIsSecVFLwrvesEnabled
 *          that takes apiDomain instead as an input.
 *
 * @param[in]       apiDomain                  API Domain of the Clock domain
 *                                             of interest.
 * @param[out]      pBIsSecVFLwrvesEnabled     Pointer to boolean to indicate
 *                                             whether secondary VF lwrve
 *                                             is enabled.
 *
 * @return @ref FLCN_OK                 Success
 * @return Others                       Implementation-specific errors.
 */
#define ClkDomainProgIsSecVFLwrvesEnabledByApiDomain(fname)    FLCN_STATUS (fname)(LwU32 apiDomain, LwBool *pBIsSecVFLwrvesEnabled)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// OBJECT interfaces
BoardObjInterfaceConstruct          (clkDomainConstruct_PROG)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkDomainConstruct_PROG");

// CLK_DOMAIN_PROG interfaces
ClkDomainProgVoltToFreq                 (clkDomainProgVoltToFreq)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgVoltToFreq");
ClkDomainProgFreqToVolt                 (clkDomainProgFreqToVolt)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgFreqToVolt");
ClkDomainProgVoltToFreqTuple            (clkDomainProgVoltToFreqTuple)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkDomainProgVoltToFreqTuple");
ClkDomainProgFreqEnumIterate            (clkDomainProgFreqEnumIterate)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgFreqEnumIterate");
ClkDomainProgGetFreqMHzByIndex          (clkDomainProgGetFreqMHzByIndex)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgGetFreqMHzByIndex");
ClkDomainProgGetIndexByFreqMHz          (clkDomainProgGetIndexByFreqMHz)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgGetIndexByFreqMHz");
ClkDomainProgVfMaxFreqMHzGet            (clkDomainProgVfMaxFreqMHzGet)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgVfMaxFreqMHzGet");
ClkDomainProgFreqQuantize               (clkDomainProgFreqQuantize)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgFreqQuantize");
ClkDomainProgClientFreqDeltaAdj         (clkDomainProgClientFreqDeltaAdj)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgClientFreqDeltaAdj");
ClkDomainProgFactoryDeltaAdj            (clkDomainProgFactoryDeltaAdj)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgFactoryDeltaAdj");
ClkDomainProgIsFreqMHzNoiseAware        (clkDomainProgIsFreqMHzNoiseAware)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgIsFreqMHzNoiseAware");
ClkDomainProgPreVoltOrderingIndexGet    (clkDomainProgPreVoltOrderingIndexGet)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgPreVoltOrderingIndexGet");
ClkDomainProgPostVoltOrderingIndexGet   (clkDomainProgPostVoltOrderingIndexGet)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgPostVoltOrderingIndexGet");
ClkDomainsProgFreqPropagate             (clkDomainsProgFreqPropagate)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainsProgFreqPropagate");
ClkDomainProgVoltRailIdxGet             (clkDomainProgVoltRailIdxGet)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgVoltRailIdxGet");
ClkDomainProgFbvddDataValid             (clkDomainProgFbvddDataValid)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgFbvddDataValid");
ClkDomainProgFbvddPwrAdjust             (clkDomainProgFbvddPwrAdjust)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgFbvddPwrAdjust");
ClkDomainProgFbvddFreqToVolt            (clkDomainProgFbvddFreqToVolt)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgFbvddFreqToVolt");
mockable ClkDomainProgIsNoiseAware      (clkDomainProgIsNoiseAware)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgIsNoiseAware");
ClkDomainProgIsSecVFLwrvesEnabled       (clkDomainProgIsSecVFLwrvesEnabled)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgIsSecVFLwrvesEnabled");
ClkDomainProgIsSecVFLwrvesEnabledByApiDomain (clkDomainProgIsSecVFLwrvesEnabledByApiDomain)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgIsSecVFLwrvesEnabledByApiDomain");
/* ------------------------ Include Derived Types -------------------------- */
/*!
 * @copydoc ClkDomainProgFbvddDataValid
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
clkDomainProgFbvddDataValid_SUPER
(
    CLK_DOMAIN_PROG    *pDomainProg,
    LwBool             *pbValid
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDomainProg != NULL) &&
        (pbValid != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        clkDomainProgFbvddDataValid_SUPER_exit);

    *pbValid = LW_FALSE;

clkDomainProgFbvddDataValid_SUPER_exit:
    return status;
}

#endif // G_CLK_DOMAIN_PROG_H
#endif // CLK_DOMAIN_PROG_H
