/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_DOMAIN_3X_PROG_H
#define CLK_DOMAIN_3X_PROG_H

/*!
 * @file clk_domain_3x_prog.h
 * @brief @copydoc clk_domain_3x_prog.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_domain_3x.h"
#include "clk/clk_domain_prog.h"

/*!
 * Forward definition of types so that they can be included in function
 * prototypes below.  clk_prog*.h are already included the clk_domain*.h and
 * using the CLK_DOMAIN types in their function prototypes.
 *
 * TODO: Create a separate types header file which includes all forward type
 * definitions.  This will alleviate the problem of cirlwlar dependencies.
 */
typedef struct CLK_PROG_3X_PRIMARY CLK_PROG_1X_PRIMARY;
typedef struct CLK_PROG_3X_PRIMARY CLK_PROG_3X_PRIMARY;

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Accessor macro for a CLK_DOMAIN_PROG structure from CLK_DOMAIN_3X_PROG.
 */
#define CLK_DOMAIN_PROG_GET_FROM_3X_PROG(_pDomain3xProg)                      \
    &((_pDomain3xProg)->prog)

/*!
 * Helper macro to return a pointer to CLK_DOMAIN object at a given index cast
 * to CLK_DOMAIN_3X_PROG iff that CLK_DOMAIN implements CLK_DOMAIN_3X_PROG.
 *
 * @note This could be improved by with general dynamic casting support in PMU
 * BOARDOBJ infrastructure.
 *
 * @param[in]  _pDomains
 *     Pointer to CLK_DOMAINS structure.
 * @param[in]  _idx
 *     Index  of the CLK_DOMAIN object to retrieve.
 */
#define CLK_CLK_DOMAIN_3X_PROG_GET_BY_IDX(_pDomains, _idx)                    \
    ((boardObjGrpMaskBitGet(&((_pDomains)->progDomainsMask), (_idx))) ?       \
        (CLK_DOMAIN_3X_PROG *)BOARDOBJGRP_OBJ_GET_BY_GRP_PTR(                 \
            &((_pDomains)->super.super), (_idx)) :                            \
         NULL)

/*!
 * Helper macro to check whether frequency delta is non zero.
 *
 * @param[in] freqDelta   Frequency delta pointer.
 *
 * @return TRUE if non zero otherwise return FALSE.
 */
#define clkDomain3XProgIsFreqDeltaNonZero(pFreqDelta)                         \
      ((LW2080_CTRL_CLK_FREQ_DELTA_GET_STATIC(pFreqDelta) != 0) ||            \
       (LW2080_CTRL_CLK_FREQ_DELTA_GET_PCT(pFreqDelta) != 0))

/*!
 * Helper macro which returns whether Factory OC is enabled for the given
 * CLK_DOMAIN_3X_PROG domain and the CLK_PROG_1X_PRIMARY describing this section
 * of its VF lwrve.
 *
 * @param[in] pDomain3XProg   CLK_DOMAIN_3X_PROG pointer
 *
 * @return LW_TRUE   Factory OC enabled
 * @return LW_FALSE  Factory OC disabled
 */
#define clkDomain3XProgFactoryOCEnabled(pDomain3XProg)                        \
     ((!clkDomainsDebugModeEnabled()) &&                                      \
      (clkDomain3XProgIsFreqDeltaNonZero(                                     \
            clkDomain3XProgFactoryDeltaGet(pDomain3XProg))))

/*!
 * Accessor macro for factory OC delta -> CLK_DOMAIN_3X_PROG::factoryDelta.
 *
 * @param[in] pDomain3XProg   CLK_DOMAIN_3X_PROG pointer
 *
 * @return pointer to @ref CLK_DOMAIN_3X_PROG::factoryDelta
 */
#define clkDomain3XProgFactoryDeltaGet(pDomain3XProg)                         \
    &(pDomain3XProg)->factoryDelta

/*!
 * Accessor macro for CLK_DOMAIN_3X_PROG::deltas.freqDelta.
 *
 * @param[in] pDomain3XProg   CLK_DOMAIN_3X_PROG pointer
 *
 * @return pointer to @ref CLK_DOMAIN_3X_PROG::deltas.freqDelta
 */
#define clkDomain3XProgFreqDeltaGet(pDomain3XProg)                            \
    &(pDomain3XProg)->deltas.freqDelta

/*!
 * Accessor macro for CLK_DOMAIN_3X_PROG::deltas.pVoltDeltauV[(voltRailIdx)]
 *
 * @param[in] pDomain3XProg   CLK_DOMAIN_3X_PROG pointer
 * @param[in] voltRailIdx     VOLTAGE_RAIL index
 *
 * @return @ref CLK_DOMAIN_3X_PROG::deltas.pVoltDeltauV[(voltRailIdx)]
 */
#define clkDomain3XProgVoltDeltauVGet(pDomain3XProg, voltRailIdx)             \
    (pDomain3XProg)->deltas.pVoltDeltauV[(voltRailIdx)]

/*!
 * This MACRO is used to initialize the @ref pFreqMHz to start iterating the
 * frequencies supported by the clock domain using the function
 * @ref ClkDomainFreqsIterate
 */
#define CLK_DOMAIN_3X_PROG_ITERATE_MAX_FREQ                                 0U

/*!
 * Accessor macro for the last clock programming index for a given programmable
 * clock domain.
 *
 * @param[in] pDomain3XProg   CLK_DOMAIN_3X_PROG pointer
 *
 * @return @ref CLK_DOMAIN_3X_PROG::clkProgIdxLast
 */
#define clkDomain3xProgLastIdxGet(pDomain3XProg)    (pDomain3XProg)->clkProgIdxLast

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock Domain 3X Prog structure.  Contains information specific to a
 * Programmable Clock Domain.
 */
typedef struct
{
    /*!
     * CLK_DOMAIN_3X super class.  Must always be first object in the
     * structure.
     */
    CLK_DOMAIN_3X   super;

    /*!
     * CLK_DOMAIN_PROG super class.
     */
    CLK_DOMAIN_PROG prog;

    /*!
     * First index into the Clock Programming Table for this CLK_DOMAIN.
     */
    LwU8  clkProgIdxFirst;
    /*!
     * Last index into the Clock Programming Table for this CLK_DOMAIN.
     */
    LwU8  clkProgIdxLast;
    /*!
     * Boolean flag indicating whether Clock Domain should always be changed per
     * the Noise-Unaware Ordering group, even when in the Noise-Aware mode.
     * Applicable only if @ref CLK_DOMAIN_3X::bNoiseAwareCapable == LW_TRUE.
     */
    LwBool bForceNoiseUnawareOrdering;
    /*!
     * Factory OC frequency delta. We always apply this delta regardless of the
     * frequency delta range @ref freqDeltaMinMHz and @ref freqDeltaMaxMHz as
     * long as the clock programming entry has the OC feature enabled in it.
     * @ref CLK_PROG_1X_PRIMARY::bOCOVEnabled
     */
    LW2080_CTRL_CLK_FREQ_DELTA factoryDelta;
    /*!
     * Minimum frequency delta which can be applied to the CLK_DOMAIN.
     */
    LwS16 freqDeltaMinMHz;
    /*!
     * Maximum frequency delta which can be applied to the CLK_DOMAIN.
     */
    LwS16 freqDeltaMaxMHz;
    /*!
     * Frequency and voltage deltas for the CLK_DOMAIN.
     */
    CLK_DOMAIN_DELTA deltas;
} CLK_DOMAIN_3X_PROG;

/*!
 * @memberof CLK_DOMAIN_3X_PROG
 *
 * @brief Helper interface to adjust the given frequency by the frequency delta of this
 * CLK_DOMAIN_3X_PROG object.
 *
 * @param[in]     pDomain3XProg  CLK_DOMAIN_3X_PROG pointer
 * @param[in/out] pFreqMHz
 *     Pointer in which caller specifies frequency to adjust and in which the
 *     adjusted frequency is returned.
 * @param[in]     bQuantize
 *     Boolean indiciating whether the result should be quantized or not.
 *
 * @return FLCN_OK
 *     Frequency successfully adjusted.
 * @return Other errors
 *     An unexpected error oclwrred during adjustment.
 */
#define ClkDomain3XProgFreqAdjustDeltaMHz(fname) FLCN_STATUS (fname)(CLK_DOMAIN_3X_PROG *pDomain3XProg, LwU16 *pFreqMHz, LwBool bQuantize, LwBool bVFOCAdjReq)

/*!
 * @brief Helper function to quantize an adjusted frequency per the frequencies
 * supported on the specified CLK_DOMAIN_3X_PROG.
 *
 * @param[in]     pDomain3XProg  CLK_DOMAIN_3X_PROG pointer
 * @param[in/out] pFreqMHz
 *     Pointer in which caller specifies frequency to quantize.
 * @param[in]    bFloor
 *     Boolean indicating whether the frequency should be quantized via a floor
 *     (LW_TRUE) or ceiling (LW_FALSE) to the next closest supported value.
 * @param[in]     bReqFreqDeltaAdj
 *      Set if client is required to quantize the frequency value adjusted with
 *      the applied frequency delta.
 *
 * @return FLCN_OK
 *     Frequency successfully quantized.
 * @return Other errors
 *     An unexpected error oclwrred during quantization.
 */
#define ClkDomain3XProgFreqAdjustQuantizeMHz(fname) FLCN_STATUS (fname)(CLK_DOMAIN_3X_PROG *pDomain3XProg, LwU16 *pFreqMHz, LwBool bFloor, LwBool bReqFreqDeltaAdj)

/*!
 * @memberof CLK_DOMAIN_3X_PROG
 *
 * @brief Enumerates the frequencies supported on the given @ref CLK_DOMAIN_3X_PROG
 * and get the clock programming index that is used to program the given
 * freq value @ref freqMHz.
 *
 * @param[in]     pDomain3XProg CLK_DOMAIN_3X_PROG pointer
 * @param[in]     freqMHz
 *      We need to find the clock prog index that can be used to program this freq
 *      value for a given clock domain.
 * @param[in]     bReqFreqDeltaAdj
 *      Set if client is required to read the frequency value adjusted with
 *      the applied frequency delta.
 *
 * @return clock programming index that can be used to program the given input freq.
 *
 * @return Other errors
 *     An unexpected error oclwrred during adjustment.
 */
#define ClkDomain3XProgGetClkProgIdxFromFreq(fname) LwU8 (fname)(CLK_DOMAIN_3X_PROG *pDomain3XProg, LwU32 freqMHz, LwBool bReqFreqDeltaAdj)

/*!
 * @memberof CLK_DOMAIN_3X_PROG
 *
 * @brief Helper interface to adjust the given voltage by the voltage delta of
 * this CLK_DOMAIN_3X_PROG object.
 *
 * @param[in]     pDomain3XProg  CLK_DOMAIN_3X_PROG pointer
 * @param[in]     pProg3XPrimary  CLK_PROG_3X_PRIMARY pointer
 * @param[in]     voltRailIdx    VOLTAGE_RAIL index
 * @param[in,out] pVoltuV
 *     Pointer in which caller specifies voltage to adjust and in which the
 *     adjusted voltage is returned.
 *
 * @return FLCN_OK
 *     Voltage successfully adjusted.
 * @return Other errors
 *     An unexpected error oclwrred during adjustment.
 */
#define ClkDomain3XProgVoltAdjustDeltauV(fname) FLCN_STATUS (fname)(CLK_DOMAIN_3X_PROG *pDomain3XProg, CLK_PROG_3X_PRIMARY *pProg3XPrimary, LwU8 voltRailIdx, LwU32 *pVoltuV)

/*!
 * @memberof CLK_DOMAIN_3X_PROG
 *
 * @brief Helper interface which returns whether OV/OC is enabled for the given
 * CLK_DOMAIN_3X_PROG domain.
 *
 * @param[in] pDomain3XProg   CLK_DOMAIN_3X_PROG pointer
 *
 * @return LW_TRUE   OV/OC enabled
 * @return LW_FALSE  OV/OC disabled
 */
#define ClkDomain3XProgIsOVOCEnabled(fname) LwBool (fname)(CLK_DOMAIN_3X_PROG *pDomain3XProg)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkDomainGrpIfaceModel10ObjSet_3X_PROG)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkDomainGrpIfaceModel10ObjSet_3X_PROG");

ClkDomainsCache                 (clkDomainsCache_3X)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkDomainsCache_3X");

// CLK DOMAIN PROG interfaces
ClkDomainProgVoltToFreq                 (clkDomainProgVoltToFreq_3X)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgVoltToFreq_3X");
ClkDomainProgFreqToVolt                 (clkDomainProgFreqToVolt_3X)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgFreqToVolt_3X");
ClkDomainProgFreqEnumIterate            (clkDomainProgFreqsIterate_3X)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgFreqsIterate_3X");
ClkDomainProgVfMaxFreqMHzGet            (clkDomainProgMaxFreqMHzGet_3X)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgMaxFreqMHzGet_3X");
ClkDomainProgFreqQuantize               (clkDomainProgFreqQuantize_3X)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgFreqQuantize_3X");
ClkDomainProgClientFreqDeltaAdj         (clkDomainProgClientFreqDeltaAdj_3X)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgClientFreqDeltaAdj_3X");
ClkDomainProgFactoryDeltaAdj            (clkDomainProgFactoryDeltaAdj_3X)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgFactoryDeltaAdj_3X");
ClkDomainProgIsFreqMHzNoiseAware        (clkDomainProgIsFreqMHzNoiseAware_3X)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainProgIsFreqMHzNoiseAware_3X");

// CLK DOMAIN 3X PROG interfaces
ClkDomain3XProgFreqAdjustDeltaMHz       (clkDomain3XProgFreqAdjustDeltaMHz)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain3XProgFreqAdjustDeltaMHz");
ClkDomain3XProgFreqAdjustDeltaMHz       (clkDomain3XProgFreqAdjustDeltaMHz_SUPER)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain3XProgFreqAdjustDeltaMHz_SUPER");
ClkDomain3XProgFreqAdjustQuantizeMHz    (clkDomain3XProgFreqAdjustQuantizeMHz)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain3XProgFreqAdjustQuantizeMHz");
ClkDomain3XProgVoltAdjustDeltauV        (clkDomain3XProgVoltAdjustDeltauV)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain3XProgVoltAdjustDeltauV");
ClkDomain3XProgGetClkProgIdxFromFreq    (clkDomain3XProgGetClkProgIdxFromFreq)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain3XProgGetClkProgIdxFromFreq");
ClkDomainGetSource                      (clkDomainGetSource_3X_PROG)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainGetSource_3X_PROG");
ClkDomain3XProgIsOVOCEnabled            (clkDomain3XProgIsOVOCEnabled)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomain3XProgIsOVOCEnabled");

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/clk_domain_30_prog.h"
#include "clk/clk_domain_35_prog.h"

#endif // CLK_DOMAIN_3X_PROG_H
