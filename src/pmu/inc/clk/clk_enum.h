/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_ENUM_H
#define CLK_ENUM_H

/*!
 * @file clk_enum.h
 * @brief @copydoc clk_enum.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CLK_ENUM CLK_ENUM, CLK_ENUM_BASE;

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Macro to locate CLK_ENUMS BOARDOBJGRP.
 *
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_CLK_ENUM                                    \
    (&Clk.enums.super.super)

/*!
 * Accessor macro for CLK_ENUM::bOCOVEnabled
 *
 * @param[in] pEnum     CLK_ENUM pointer
 *
 * @return @ref CLK_ENUM::bOCOVEnabled
 */
#define clkEnumOVOCEnabled(pEnum)                                           \
    (clkDomainsOverrideOVOCEnabled() || (pEnum)->bOCOVEnabled)

/*!
 * @interface CLK_ENUM
 *
 * Accessor to min frequency of this CLK_ENUM.
 *
 * @param[in] pEnum CLK_ENUM pointer
 *
 * @return CLK_ENUM::freqMinMHz
 */
#define clkEnumFreqMinMHzGet(pEnum)                                           \
    ((pEnum)->freqMinMHz)

/*!
 * @interface CLK_ENUM
 *
 * Accessor to max frequency of this CLK_ENUM.
 *
 * @param[in] pEnum CLK_ENUM pointer
 *
 * @return CLK_ENUM::freqMaxMHz
 */
#define clkEnumFreqMaxMHzGet(pEnum)                                           \
    ((pEnum)->freqMaxMHz)

/*!
 * @interface CLK_ENUM
 *
 * Accessor to offset adjusted min frequency of this CLK_ENUM.
 *
 * @param[in] pEnum CLK_ENUM pointer
 *
 * @return CLK_ENUM::offsetFreqMinMHz
 */
#define clkEnumOffsetFreqMinMHzGet(pEnum)                                     \
    ((pEnum)->offsetFreqMinMHz)

/*!
 * @interface CLK_ENUM
 *
 * Accessor to offset adjusted max frequency of this CLK_ENUM.
 *
 * @param[in] pEnum CLK_ENUM pointer
 *
 * @return CLK_ENUM::offsetFreqMaxMHz
 */
#define clkEnumOffsetFreqMaxMHzGet(pEnum)                                     \
    ((pEnum)->offsetFreqMaxMHz)

/*!
 * @interface CLK_ENUM
 *
 * Accessor to OCOV support of this CLK_ENUM.
 *
 * @param[in] pEnum CLK_ENUM pointer
 *
 * @return CLK_ENUM::bOCOVEnabled
 */
#define clkEnumOCOVEnabled(pEnum)                                             \
    ((pEnum)->bOCOVEnabled)

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock enumerarion class params.
 */
struct CLK_ENUM
{
    /*!
     * BOARDOBJ super class. Must always be the first element in the structure.
     */
    BOARDOBJ    super;
    /*!
     * Boolean flag indicating whether this entry supports OC/OV when those
     * settings are applied to the corresponding CLK_DOMAIN object.
     */
    LwBool      bOCOVEnabled;
    /*!
     * Minimum frequency in MHz which can be programmed on the CLK_DOMAIN.
     */
    LwU16       freqMinMHz;
    /*!
     * Maximum frequency in MHz which can be programmed on the CLK_DOMAIN.
     */
    LwU16       freqMaxMHz;
    /*!
     * Offset adjusted minimum frequency in MHz which can be programmed on
     * the CLK_DOMAIN.
     */
    LwU16       offsetFreqMinMHz;
    /*!
     * Offset adjusted maximum frequency in MHz which can be programmed on
     * the CLK_DOMAIN.
     */
    LwU16       offsetFreqMaxMHz;
};

/*!
 * Clock enumerarion group params.
 */
typedef struct
{
    /*!
     * BOARDOBJGRP_E255 super class. Must always be the first element in the
     * structure.
     */
    BOARDOBJGRP_E255 super;
} CLK_ENUMS;

/*!
 * @interface CLK_ENUM
 *
 * Quantizes an input frequency (pFreqMHz) based on the given settings of this
 * CLK_ENUM object.
 *
 * @prereq  This interface must be called by CLK_DOMAIN_40_PROG in ascending order
 *          of clock enumeration entries.
 *
 * @param[in]     pEnum            CLK_ENUM pointer
 * @param[in]     pDomain40Prog    CLK_DOMAIN_40_PROG pointer
 * @param[in]     pInputFreq       LW2080_CTRL_CLK_VF_INPUT pointer
 *     Pointer to LW2080_CTRL_CLK_VF_INPUT struct which contains frequency (MHz)
 *     to quantize.
 * @param[out]    pOutputFreq      LW2080_CTRL_CLK_VF_OUTPUT pointer
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
 *     The frequency was successfully quantized by this CLK_ENUM object and
 *     the resulting quantized value has been returned in pFreqMHz.
 * @return FLCN_ERR_OUT_OF_RANGE
 *     The requested frequency could not be quantized within the frequency range
 *     of this CLK_ENUM object. This is an expected failure case which
 *     indicates that the calling code should continue to attempt to quantize
 *     the requested frequency with other CLK_ENUM objects.
 */
#define ClkEnumFreqQuantize(fname) FLCN_STATUS (fname)(CLK_ENUM *pEnum, CLK_DOMAIN_40_PROG *pDomain40Prog, LW2080_CTRL_CLK_VF_INPUT *pInputFreq, LW2080_CTRL_CLK_VF_OUTPUT *pOutputFreq, LwBool bFloor, LwBool bBound)

/*!
 * @interface CLK_ENUM
 *
 * Iterate over the frequencies (@ref pFreqMHz) from highest to lowest supported
 * on the given Clock Domain (@ref pDomain40Prog).
 *
 * @param[in]       pEnum           CLK_ENUM pointer
 * @param[in]       pDomain40Prog   CLK_DOMAIN_40_PROG pointer
 * @param[in/out]   pFreqMHz
 *     Pointer to the buffer which is used to iterate over the frequency. The IN
 *     is the last frequency value that we read where as the OUT is the next freq
 *     value that follows the last frequency value represented by ref@ pFreqMHz.
 *     Note we are iterating from highest -> lowest freq value.
 *
 * NOTE: If the initial value for freq IN is "CLK_DOMAIN_40_PROG_ITERATE_MAX_FREQ"
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
#define ClkEnumFreqsIterate(fname) FLCN_STATUS (fname)(CLK_ENUM *pEnum, CLK_DOMAIN_40_PROG *pDomain40Prog, LwU16 *pFreqMHz)

/*!
 * @interface CLK_ENUM
 *
 * Helper interface to adjust the clock enumeration freq range with freq offsets.
 *
 * @param[in]       pEnum           CLK_ENUM pointer
 * @param[in]       pDomain40Prog   CLK_DOMAIN_40_PROG pointer
 *
 * @return FLCN_OK
 *     The enum frequency range was successfully adjusted.
 * @return Other errors
 *     An unexpected error coming from higher functions.
 */
#define ClkEnumFreqRangeAdjust(fname) FLCN_STATUS (fname)(CLK_ENUM *pEnum, CLK_DOMAIN_40_PROG *pDomain40Prog)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler   (clkEnumBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkEnumGrpSet");

// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet       (clkEnumGrpIfaceModel10ObjSet_SUPER)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkEnumGrpIfaceModel10ObjSet_SUPER");

// CLK_ENUM interfaces
ClkEnumFreqQuantize         (clkEnumFreqQuantize)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkEnumFreqQuantize");
ClkEnumFreqsIterate         (clkEnumFreqsIterate)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkEnumFreqsIterate");
ClkEnumFreqRangeAdjust      (clkEnumFreqRangeAdjust)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkEnumFreqRangeAdjust");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_ENUM_H
