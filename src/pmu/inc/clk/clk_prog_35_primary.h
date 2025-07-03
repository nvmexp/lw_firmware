/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_PROG_35_PRIMARY_H
#define CLK_PROG_35_PRIMARY_H

/*!
 * @file clk_prog_35_primary.h
 * @brief @copydoc clk_prog_35_primary.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_prog_35.h"
#include "clk/clk_prog_3x_primary.h"

/*!
 * Forward definition of types so that they can be included in function
 * prototypes below.
 *
 * TODO: Create a separate types header file which includes all forward type
 * definitions.  This will alleviate the problem of cirlwlar dependencies.
 */
typedef struct CLK_VF_POINT_35 CLK_VF_POINT_35;

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Accessor macro for a CLK_PROG_3X_PRIMARY structure from CLK_PROG_35_PRIMARY.
 */
#define CLK_CLK_PROG_3X_PRIMARY_GET_FROM_35_PRIMARY(_pPrimary35)                 \
    &((_pPrimary35)->primary)

/*!
 * Accessor macro for @ref vfPointIdxFirst based on input lwrve index and
 * volt rail index.
 *
 * @param[in] pProg35Primary CLK_PROG_35_PRIMARY pointer
 * @param[in] voltRailIdx   Volt Rail index
 * @param[in] lwrveIdx      CLK DOMAIN VF Lwrve index
 *
 * @return CLK_DOMAIN's VF lwrve count.
 */
#define clklkProg35PrimaryVfPointIdxFirstGet(pProg35Primary, voltRailIdx, lwrveIdx)   \
    (((lwrveIdx) == LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_PRI) ?          \
        ((pProg35Primary)->primary.pVfEntries[(voltRailIdx)].vfPointIdxFirst) :       \
        ((pProg35Primary)->pVoltRailSecVfEntries[(voltRailIdx)].                     \
            secVfEntries[((lwrveIdx) - LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_SEC_0)].vfPointIdxFirst))

/*!
 * Accessor macro for @ref vfPointIdxLast based on input lwrve index and
 * volt rail index.
 *
 * @param[in] pProg35Primary CLK_PROG_35_PRIMARY pointer
 * @param[in] voltRailIdx   Volt Rail index
 * @param[in] lwrveIdx      CLK DOMAIN VF Lwrve index
 *
 * @return CLK_DOMAIN's VF lwrve count.
 */
#define clklkProg35PrimaryVfPointIdxLastGet(pProg35Primary, voltRailIdx, lwrveIdx)    \
    (((lwrveIdx) == LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_PRI) ?          \
        ((pProg35Primary)->primary.pVfEntries[(voltRailIdx)].vfPointIdxLast) :        \
        ((pProg35Primary)->pVoltRailSecVfEntries[(voltRailIdx)].                     \
            secVfEntries[((lwrveIdx) - LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_SEC_0)].vfPointIdxLast))

/*!
 * Accessor macro to get the VFE equation index based on rail index and lwrve index.
 *
 * @param[in] pProg35Primary  CLK_PROG_35_PRIMARY pointer
 * @param[in] railIdx        Voltage rail index
 * @param[in] lwrveIdx       CLK DOMAIN VF Lwrve index
 *
 * @return VFE equation index based on rail index and lwrve index
 */
#define clkProg35PrimaryVfeIdxGet(pProg35Primary, railIdx, lwrveIdx)              \
    (((lwrveIdx) == LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_PRI) ?      \
        ((pProg35Primary)->primary.pVfEntries[(railIdx)].vfeIdx) :                \
        ((pProg35Primary)->pVoltRailSecVfEntries[(railIdx)].                     \
            secVfEntries[((lwrveIdx) - LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_SEC_0)].vfeIdx))

/*!
 * Accessor macro to get the CPM max frequency offset VFE equation index
 * based on rail index.
 *
 * @param[in] pProg35Primary  CLK_PROG_35_PRIMARY pointer
 * @param[in] railIdx        Voltage rail index
 *
 * @return CPM Max VFE equation index based on rail index
 */
#define clkProg35PrimaryCpmMaxFreqOffsetVfeIdxGet(pProg35Primary, railIdx)      \
        ((pProg35Primary)->primary.pVfEntries[(railIdx)].cpmMaxFreqOffsetVfeIdx)

/*!
 * Accessor macro to get the DVCO Offset code VFE equation index based on rail index
 * and lwrve index.
 *
 * @param[in] pProg35Primary  CLK_PROG_35_PRIMARY pointer
 * @param[in] railIdx        Voltage rail index
 * @param[in] lwrveIdx       CLK DOMAIN VF Lwrve index
 *
 * @return DVCO Offset code VFE equation index based on rail index and lwrve index
 */
#define clkProg35PrimaryDvcoOffsetVfeIdxGet(pProg35Primary, railIdx, lwrveIdx)    \
    (((lwrveIdx) == LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_PRI) ?      \
        PMU_PERF_VFE_EQU_INDEX_ILWALID :                                        \
        ((pProg35Primary)->pVoltRailSecVfEntries[(railIdx)].                     \
            secVfEntries[((lwrveIdx) - LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_SEC_0)].dvcoOffsetVfeIdx))

/* ------------------------ Datatypes -------------------------------------- */
typedef struct CLK_PROG_35_PRIMARY CLK_PROG_35_PRIMARY;

/*!
 * Clock Programming entry.  Defines the how the RM will program a given
 * frequency on a clock domain per the VBIOS specification.
 *
 */
//! https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Clock_Programming_Table/1.0_Spec#Primary
struct CLK_PROG_35_PRIMARY
{
    /*!
     * CLK_PROG_35 super class.  Must always be first object in the
     * structure.
     */
    CLK_PROG_35         super;

    /*!
     * CLK_PROG_3X_PRIMARY super class.
     */
    CLK_PROG_3X_PRIMARY  primary;

    /*!
     * Array of secondary VF entries tuple. Indexed per the voltage rail enumeration.
     *
     * Has valid indexes in the range [0, @ref
     * LW2080_CTRL_CLK_CLK_PROGS_INFO::numVfEntries).
     *
     * @note Not applicable to automotive builds.
     */
    LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_SEC_VF_ENTRY_VOLTRAIL *pVoltRailSecVfEntries;
};

/*!
 * @brief Load CLK_PROG_35_PRIMARY object all its generated VF points.
 *
 * @memberof CLK_PROG_35_PRIMARY
 *
 * @param[in]     pProg35Primary     CLK_PROG_35_PRIMARY pointer
 * @param[in]     pDomain35Primary   CLK_DOMAIN_35_PRIMARY pointer
 * @param[in]     voltRailIdx       Index of the VOLTAGE_RAIL to load.
 * @param[in]     lwrveIdx          VF lwrve index.
 *
 * @return FLCN_OK
 *     CLK_PROG_35_PRIMARY successfully loaded.
 * @return Other errors
 *     Unexpected errors oclwred during loading.
 */
#define ClkProg35PrimaryLoad(fname) FLCN_STATUS (fname)(CLK_PROG_35_PRIMARY *pProg35Primary, CLK_DOMAIN_35_PRIMARY *pDomain35Primary, LwU8 voltRailIdx, LwU8 lwrveIdx)

/*!
 * @brief Caches the dynamically evaluated VFE Equation values for this
 * CLK_PROG_35_PRIMARY object. Expected to be called form the VFE polling loop.
 *
 * Iterates over the set of CLK_VF_POINTs pointed to by this CLK_PROG_35_PRIMARY
 * and caches their VF values.  Ensures monotonically increasing by always
 * returning the last VF value in the @ref pVFPairLast param.
 *
 * @memberof CLK_PROG_35_PRIMARY
 *
 * @param[in]     pProg35Primary     CLK_PROG_35_PRIMARY pointer
 * @param[in]     pDomain35Primary   CLK_DOMAIN_35_PRIMARY pointer
 * @param[in]     voltRailIdx
 *      Index of the VOLTAGE_RAIL to cache.
 * @param[in]     bVFEEvalRequired  Whether VFE Evaluation is required?
 * @param[in]     lwrveIdx          Vf lwrve index.
 * @param[in,out] pVfPoint35Last
 *      Pointer to CLK_VF_POINT_35 structure containing the last VF point
 *      evaluated by previous CLK_PROG_35_PRIMARY objects, and in which this
 *      CLK_PROG_35_PRIMARY object will return latest evaluated VF point.
 *
 * @return FLCN_OK
 *     CLK_PROG_35_PRIMARY successfully cached.
 * @return Other errors
 *     Unexpected errors oclwred during caching.
 */
#define ClkProg35PrimaryCache(fname) FLCN_STATUS (fname)(CLK_PROG_35_PRIMARY *pProg35Primary, CLK_DOMAIN_35_PRIMARY *pDomain35Primary, LwU8 voltRailIdx, LwBool bVFEEvalRequired, LwU8 lwrveIdx, CLK_VF_POINT_35 *pVfPoint35Last)

/*!
 * @brief Iterates over the set of CLK_VF_POINTs pointed to by this
 * CLK_PROG_35_PRIMARY and smoothen their VF values.
 *
 * Ensures that the discontinuity between two conselwtive VF points are within
 * the max allowed bound.
 *
 * @memberof CLK_PROG_35_PRIMARY
 *
 * @param[in]     pProg35Primary         CLK_PROG_35_PRIMARY pointer
 * @param[in]     pDomain35Primary       CLK_DOMAIN_35_PRIMARY pointer
 * @param[in]     voltRailIdx           Index of the VOLTAGE_RAIL.
 * @param[in]     lwrveIdx              Vf lwrve index.
 * @param[in,out] pVfPoint35Last
 *      Pointer to CLK_VF_POINT_35 structure containing the last VF point
 *      evaluated by previous CLK_PROG_35_PRIMARY objects, and in which this
 *      CLK_PROG_35_PRIMARY object will return latest evaluated VF point.
 *
 * @return FLCN_OK
 *     CLK_PROG_35_PRIMARY successfully smoothen.
 * @return Other errors
 *     Unexpected errors oclwred.
 */
#define ClkProg35PrimarySmoothing(fname) FLCN_STATUS (fname)(CLK_PROG_35_PRIMARY *pProg35Primary, CLK_DOMAIN_35_PRIMARY *pDomain35Primary, LwU8 voltRailIdx, LwU8 lwrveIdx, CLK_VF_POINT_35 *pVfPoint35Last)

/*!
 * @brief Iterates over the set of CLK_VF_POINTs pointed to by this
 * CLK_PROG_35_PRIMARY and trim them to enumeration max.
 *
 * @memberof CLK_PROG_35_PRIMARY
 *
 * @param[in]     pProg35Primary         CLK_PROG_35_PRIMARY pointer
 * @param[in]     pDomain35Primary       CLK_DOMAIN_35_PRIMARY pointer
 * @param[in]     voltRailIdx           Index of the VOLTAGE_RAIL.
 * @param[in]     lwrveIdx              Vf lwrve index.
 *
 * @return FLCN_OK
 *     CLK_PROG_35_PRIMARY successfully trimmed.
 * @return Other errors
 *     Unexpected errors oclwred.
 */
#define ClkProg35PrimaryTrim(fname) FLCN_STATUS (fname)(CLK_PROG_35_PRIMARY *pProg35Primary, CLK_DOMAIN_35_PRIMARY *pDomain35Primary, LwU8 voltRailIdx, LwU8 lwrveIdx)

/*!
 * @memberof CLK_PROG_35_PRIMARY
 *
 * Client interface to apply frequency OC adjustment on input frequency. This
 * interface will be used by client's of VF lwrve.
 *
 * @param[in]       pProg35Primary CLK_PROG_35_PRIMARY pointer
 * @param[in,out]   pFreqMHz      Input frequency to adjust
 *
 * @return FLCN_OK
 *     Frequency successfully adjusted with the OC offsets.
 * @return Other errors
 *     An unexpected error oclwrred during adjustments.
 */
#define ClkProg35PrimaryClientFreqDeltaAdj(fname) FLCN_STATUS (fname)(CLK_PROG_35_PRIMARY *pProg35Primary, CLK_DOMAIN_35_PRIMARY *pDomain35Primary, LwU8 clkDomIdx, LwU16 *pFreqMHz)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkProgGrpIfaceModel10ObjSet_35_PRIMARY)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkProgGrpIfaceModel10ObjSet_35_PRIMARY");

// CLK_PROG_35_PRIMARY interfaces
ClkProg35PrimaryLoad                       (clkProg35PrimaryLoad)
    GCC_ATTRIB_SECTION("imem_vfLoad", "clkProg35PrimaryLoad");
ClkProg35PrimaryCache                      (clkProg35PrimaryCache)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkProg35PrimaryCache");
ClkProg35PrimarySmoothing                  (clkProg35PrimarySmoothing)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkProg35PrimarySmoothing");
ClkProg35PrimaryTrim                       (clkProg35PrimaryTrim)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkProg35PrimaryTrim");

ClkProg35PrimaryClientFreqDeltaAdj     (clkProg35PrimaryClientFreqDeltaAdj)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkProg35PrimaryClientFreqDeltaAdj");

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/clk_prog_35_primary_ratio.h"
#include "clk/clk_prog_35_primary_table.h"

#endif // CLK_PROG_35_PRIMARY_H
