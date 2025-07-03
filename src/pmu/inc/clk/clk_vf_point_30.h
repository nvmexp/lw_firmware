/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_VF_POINT_30_H
#define CLK_VF_POINT_30_H

/*!
 * @file clk_vf_point_30.h
 * @brief @copydoc clk_vf_point_30.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_vf_point_3x.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * @interface CLK_VF_POINT_30
 *
 * Accessor to the LW2080_CTRL_CLK_VF_PAIR of a particular CLK_VF_POINT_30.
 *
 * @param[in]  pVfPoint30   CLK_VF_POINT_30 pointer
 *
 * @return Pointer ot @ref CLK_VF_POINT_30::LW2080_CTRL_CLK_VF_PAIR.
 */
#define clkVfPoint30PairGet(pVfPoint30)                                         \
    (&((pVfPoint30)->pair))

/*!
 * @interface CLK_VF_POINT_30
 *
 * Accessor to the frequency delta of a given CLK_VF_POINT_30.
 *
 * @param[in]  pVfPoint30   CLK_VF_POINT_30 pointer
 *
 * @return CLK_VF_POINT_30's frequency delta (MHz)
 */
#define clkVfPoint30FreqDeltaMHzGet(pVfPoint30)                                    \
    (BOARDOBJ_GET_TYPE(pVfPoint30) == LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_30_VOLT) ? \
        (LW2080_CTRL_CLK_FREQ_DELTA_GET_STATIC(                                    \
            &((CLK_VF_POINT_30_VOLT *)(pVfPoint30))->freqDelta) / 1000) : 0

/*!
 * @interface CLK_VF_POINT_30
 *
 * Accessor to the frequency of a particular CLK_VF_POINT_30.
 *
 * @param[in]  pVfPoint30   CLK_VF_POINT_30 pointer
 *
 * @return CLK_VF_POINT_30's frequency (MHz)
 */
#define clkVfPoint30FreqMHzGet(pVfPoint30)                                      \
    LW2080_CTRL_CLK_VF_PAIR_FREQ_MHZ_GET(clkVfPoint30PairGet(pVfPoint30))

/*!
 * @interface CLK_VF_POINT_30
 *
 * Mutator to the frequency of a particular CLK_VF_POINT_30.
 *
 * @param[in]  pVfPoint30   CLK_VF_POINT_30 pointer
 * @parma[in]  _freqMHz     Frequency (MHz) to set
 */
#define clkVfPoint30FreqMHzSet(pVfPoint30, _freqMHz)                            \
    LW2080_CTRL_CLK_VF_PAIR_FREQ_MHZ_SET(clkVfPoint30PairGet(pVfPoint30), _freqMHz)

/*!
 * @interface CLK_VF_POINT_30
 *
 * Mutator to the voltage of a particular CLK_VF_POINT_30.
 *
 * @param[in]  pVfPoint30   CLK_VF_POINT_30 pointer
 * @param[in]  _voltageuV   Voltage (uV) to set
 */
#define clkVfPoint30VoltageuVSet(pVfPoint30, _voltageuV)                        \
    LW2080_CTRL_CLK_VF_PAIR_VOLTAGE_UV_SET(clkVfPoint30PairGet(pVfPoint30),     \
        _voltageuV)

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock VF Point 30 structure.
 */
typedef struct
{
    /*!
     * CLK_VF_POINT super class. Must always be the first element in the structure.
     */
    CLK_VF_POINT super;

    /*!
     * Voltage and frequency pair for this VF point.  These values will be
     * determined per the semantics of the child class.
     */
    LW2080_CTRL_CLK_VF_PAIR pair;
} CLK_VF_POINT_30;

/*!
 * @interface CLK_VF_POINT_30
 *
 * Caches the dynamically evaluated voltage or frequency value for this
 * CLK_VF_POINT_30 per the semantics of this CLK_VF_POINT_30 object's class.
 *
 * @note This is a virtual interface which the each CLK_VF_POINT_30 child class
 * must implement.
 *
 * @param[in]       pVfPoint30      CLK_VF_POINT_30 pointer
 * @param[in]       pDomain30Primary CLK_DOMAIN_30_PRIMARY pointer
 * @param[in]       pProg30Primary   CLK_PROG_30_PRIMARY Pointer
 * @param[in]       voltRailIdx
 *      Index of VOLT_RAIL of the CLK_VF_POINT which is being cached.
 * @param[in/out]   pVfPairLast
 *      Pointer to LW2080_CTRL_CLK_VF_PAIR structure containing the last VF pair
 *      evaluated by previous CLK_VF_POINT_30 objects, and in which this
 *      CLK_VF_POINT_30 object will return its VF pair.
 * @param[in/out] pBaseVFPairLast
 *      Pointer to LW2080_CTRL_CLK_VF_PAIR structure containing the last base
 *      VF pair evaluated by previous CLK_VF_POINT_30 objects, and in which
 *      this CLK_VF_POINT_30 object will return its last base VF pair.
 *
 * @return FLCN_OK
 *     CLK_VF_POINT_30 successfully cached.
 * @return Other errors
 *     Unexpected errors oclwred during caching.
 */
#define ClkVfPoint30Cache(fname) FLCN_STATUS (fname)(CLK_VF_POINT_30 *pVfPoint30, CLK_DOMAIN_30_PRIMARY *pDomain30Primary, CLK_PROG_30_PRIMARY *pProg30Primary, LwU8 voltRailIdx, LW2080_CTRL_CLK_VF_PAIR *pVfPairLast, LW2080_CTRL_CLK_VF_PAIR *pBaseVFPairLast)

/*!
 * @interface CLK_VF_POINT_30
 *
 * Smoothen the dynamically evaluated voltage or frequency value for this
 * CLK_VF_POINT_30 per the semantics of this CLK_VF_POINT_30 object's class.
 *
 * @param[in]       pVfPoint30      CLK_VF_POINT_30 pointer
 * @param[in]       pDomain30Primary CLK_DOMAIN_30_PRIMARY pointer
 * @param[in]       pProg30Primary   CLK_PROG_30_PRIMARY Pointer
 * @param[in/out]   pFreqMHzExpected
 *      Pointer to expected minimum required freqeuncy value evaluated
 *      by previous CLK_VF_POINT_30 objects, and in which this CLK_VF_POINT_30
 *      object will return updated frequency value.
 *
 * @return FLCN_OK
 *     CLK_VF_POINT_30 successfully smoothened.
 * @return Other errors
 *     Unexpected errors oclwred during VF lwrve smoothing.
 */
#define ClkVfPoint30Smoothing(fname) FLCN_STATUS (fname)(CLK_VF_POINT_30 *pVfPoint30, CLK_DOMAIN_30_PRIMARY *pDomain30Primary, CLK_PROG_30_PRIMARY *pProg30Primary, LwU16 *pFreqMHzExpected)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler  (clkVfPointBoardObjGrpIfaceModel10Set_30)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVfPointBoardObjGrpIfaceModel10Set_30");
BoardObjGrpIfaceModel10CmdHandler  (clkVfPointBoardObjGrpIfaceModel10GetStatus_30)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkVfPointBoardObjGrpIfaceModel10GetStatus_30");

BoardObjIfaceModel10GetStatus     (clkVfPointIfaceModel10GetStatus_30)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkVfPointIfaceModel10GetStatus_30");

// CLK_VF_POINT interfaces
ClkVfPointAccessor      (clkVfPointAccessor_30)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPointAccessor_30");
ClkVfPointVoltageuVGet  (clkVfPointVoltageuVGet_30)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPointVoltageuVGet_30");

// CLK_VF_POINT_30 interfaces
ClkVfPoint30Cache           (clkVfPoint30Cache)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint30Cache");
ClkVfPoint30Smoothing       (clkVfPoint30Smoothing)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint30Smoothing");

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/clk_vf_point_30_freq.h"
#include "clk/clk_vf_point_30_volt.h"

#endif // CLK_VF_POINT_30_H
