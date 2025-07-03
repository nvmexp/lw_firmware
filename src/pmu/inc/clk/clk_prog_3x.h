/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_PROG_3X_H
#define CLK_PROG_3X_H

/*!
 * @file clk_prog_3x.h
 * @brief @copydoc clk_prog_3x.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_prog.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * @memberof CLK_PROG_3X
 *
 * Accessor to frequency source of this CLK_PROG_3X.
 *
 * @param[in] pProg3X CLK_PROG_3X pointer
 *
 * @return CLK_PROG_3X::source
 */
#define clkProg3XFreqSourceGet(pProg3X)                                       \
    ((CLK_PROG_3X *)(pProg3X))->source

/*!
 * PROG_30 wrapper to PROG_3X implemenation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
#define clkProgGrpIfaceModel10ObjSet_30(pModel10, ppBoardObj, size, pBoardObjSet)         \
    (clkProgGrpIfaceModel10ObjSet_3X(pModel10, ppBoardObj, size, pBoardObjSet))

/*!
 * @memberof CLK_PROG_3X
 *
 * Accessor to max frequency of this CLK_PROG_3X.
 *
 * @param[in] pProg3X CLK_PROG_3X pointer
 *
 * @return CLK_PROG_3X::freqMaxMHz
 */
#define clkProg3XFreqMaxMHzGet(pProg3X)                                       \
    ((pProg3X)->freqMaxMHz)

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Forward definition of CLK_PROG_3X structures so that can use
 * the type in interface definitions.
 */
typedef struct CLK_PROG_3X CLK_PROG_3X;

/*!
 * Clock Programming entry.  Defines the how the RM will program a given
 * frequency on a clock domain per the VBIOS specification.
 *
 */
//! https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Clock_Programming_Table/1.0_Spec
struct CLK_PROG_3X
{
    /*!
     * CLK_PROG super class.  Must always be first object in the
     * structure.
     */
    CLK_PROG super;

    /*!
     * Source enumeration.  @ref LW2080_CTRL_CLK_CLK_PROG_SOURCE_<XYZ>.
     */
    LwU8  source;
    /*!
     * Maximum frequency for this CLK_PROG entry.  Entries for a given domain
     * need to be specified in ascending maxFreqMhz.
     */
    LwU16 freqMaxMHz;
    /*!
     * Union of source-specific data.
     */
    LW2080_CTRL_CLK_CLK_PROG_1X_SOURCE_DATA sourceData;
};

/*!
 * @brief Obtains the index of the CLK_PROG used to program the given frequency.
 *
 * @memberof CLK_PROG_3X
 *
 * @param[in]     pProg3X           CLK_PROG_3X pointer
 * @param[in]     pDomain3XProg
 *      Pointer to the CLK_DOMAIN_3X_PROG for the requested the CLK_DOMAIN.
 * @param[in]     freqMHz
 *      We need to find the clock prog index that can be used to program
 *      this freq value for a given clock domain.
 * @param[in]     bReqFreqDeltaAdj
 *      Set if client is required to read the frequency value adjusted with
 *      the applied frequency delta.
 *
 * @return clock programming index that can be used to program the given input freq.
 * @return LW2080_CTRL_CLK_CLK_PROG_IDX_ILWALID if an index cannot be found.
 */
#define ClkProg3XGetClkProgIdxFromFreq(fname) LwU8 (fname)(CLK_PROG_3X *pProg3X, CLK_DOMAIN_3X_PROG *pDomain3XProg, LwU32 freqMHz, LwBool bReqFreqDeltaAdj)

/*!
 * @brief Quantizes a given frequency (pFreqMHz) based on the given settings of
 * this CLK_PROG_3X object.
 *
 * @memberof CLK_PROG_3X
 *
 * @param[in]     pProg3X          CLK_PROG_3X pointer
 * @param[in]     pDomain3XProg    CLK_DOMAIN_3X_PROG pointer
 * @param[in]     pInputFreq       LW2080_CTRL_CLK_VF_INPUT pointer
 *     Pointer to LW2080_CTRL_CLK_VF_INPUT struct which contains frequency (MHz)
 *     to quantize.
 * @param[out]    pOutputFreq      LW2080_CTRL_CLK_VF_OUTPUT pointer
 *     Pointer to LW2080_CTRL_CLK_VF_OUTPUT struct which has the quantized
 *     frequency (MHz)
 * @param[in]     bFloor
 *     Boolean indicating whether the frequency should be quantized via a floor
 *     (LW_TRUE) or ceiling (LW_FALSE) to the next closest supported value.
 * @param[in]     bReqFreqDeltaAdj
 *      Set if client is required to quantize the frequency value adjusted with
 *      the applied frequency delta.
 *
 * @return FLCN_OK
 *     The frequency was successfully quantized by this CLK_PROG_3X object and
 *     the resulting quantized value has been returned in pFreqMHz.
 * @return FLCN_ERR_OUT_OF_RANGE
 *     The requested frequency could not be quantized within the frequency range
 *     of this CLK_PROG_3X object.  This is an expected failure case which
 *     indicates that the calling code should continue to attempt to quantize
 *     the requested frequency with other CLK_PROG_3X objects.
 */
#define ClkProg3XFreqQuantize(fname) FLCN_STATUS (fname)(CLK_PROG_3X *pProg3X, CLK_DOMAIN_3X_PROG *pDomain3XProg, LW2080_CTRL_CLK_VF_INPUT *pInputFreq, LW2080_CTRL_CLK_VF_OUTPUT *pOutputFreq, LwBool bFloor, LwBool bReqFreqDeltaAdj)

/*!
 * @brief Helper function for quantizing the input frequency to the frequency
 * step size of input CLK_PROG_3X object.
 *
 * @memberof CLK_PROG_3X
 *
 * @param[in]     pProg3X          CLK_PROG_3X pointer
 * @param[in]     pDomain3XProg    CLK_DOMAIN_3X_PROG pointer
 * @param[in,out] pFreqMHz
 *     Pointer to frequency (MHz) to quantize. Quantized value will be returned
 *     in this pointer if quantized successfully.
 *
 * @return FLCN_OK
 *     The frequency was successfully quantized by this CLK_PROG_3X object
 *     and the resulting quantized value has been returned in pFreqMHz.
 * @return Other errors
 *     An unexpected error coming from higher functions.
 */
#define ClkProg3XFreqMHzQuantize(fname) FLCN_STATUS (fname)(CLK_PROG_3X *pProg3X, CLK_DOMAIN_3X_PROG *pDomain3XProg, LwU16 *pFreqMHz, LwBool bFloor)

/*!
 * @brief Helper function to update the frequency max value with the applied
 * freq delta followed by the quantization of the resulting value to the
 * frequency step size based on the given settings of this CLK_PROG_3X object.
 *
 * @memberof CLK_PROG_3X
 *
 * @param[in]     pProg3X          CLK_PROG_3X pointer
 * @param[in]     pDomain3XProg    CLK_DOMAIN_3X_PROG pointer
 * @param[in,out] pFreqMHz
 *     Pointer to frequency (MHz) to quantize.  Quantized value will be returned
 *     in this pointer if quantized successfully.
 *
 * @return FLCN_OK
 *     The frequency was successfully updated and quantized by this CLK_PROG_3X
 *      object and the resulting quantized value has been returned in pFreqMHz.
 * @return Other errors
 *     An unexpected error coming from higher functions.
 *     ref@ ClkNafllFreqMHzQuantize
 */
#define ClkProg3XFreqMaxAdjAndQuantize(fname) FLCN_STATUS (fname)(CLK_PROG_3X *pProg3X, CLK_DOMAIN_3X_PROG *pDomain3XProg, LwU16 *pFreqMHz)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkProgGrpIfaceModel10ObjSet_3X)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkProgGrpIfaceModel10ObjSet_3X");

FLCN_STATUS clkProg3XFreqsIterate(CLK_PROG_3X *pProg3X, LwU32 clkDomain, LwU16 *pFreqMHz)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkProg3XFreqsIterate");

ClkProg3XGetClkProgIdxFromFreq      (clkProg3XGetClkProgIdxFromFreq)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkProg3XGetClkProgIdxFromFreq");
ClkProg3XFreqQuantize               (clkProg3XFreqQuantize)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkProg3XFreqQuantize");
ClkProg3XFreqMHzQuantize            (clkProg3XFreqMHzQuantize)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkProg3XFreqMHzQuantize");

ClkProg3XFreqMaxAdjAndQuantize (clkProg3XFreqMaxAdjAndQuantize)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkProg3XFreqMaxAdjAndQuantize");

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/clk_prog_30.h"
#include "clk/clk_prog_35.h"

#endif // CLK_PROG_3X_H
