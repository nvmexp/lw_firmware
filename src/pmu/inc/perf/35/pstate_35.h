/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pstate_35.h
 * @brief   Declarations, type definitions, and macros for the PSTATE_35 SW module.
 */

#ifndef PSTATE_35_H
#define PSTATE_35_H

#include "g_pstate_35.h"

#ifndef G_PSTATE_35_H
#define G_PSTATE_35_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/pstate_3x.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PSTATE_35 PSTATE_35;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Colwenience macro for getting the vtable for the PSTATE_35 class
 *          type.
 *
 * @return  Pointer to the location of the PSTATE_35 class vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35)
#define PERF_PSTATE_35_VTABLE() &PerfPstate35VirtualTable
extern BOARDOBJ_VIRTUAL_TABLE PerfPstate35VirtualTable;
#else
#define PERF_PSTATE_35_VTABLE() NULL
#endif

/* ------------------------ Interface Definitions --------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
typedef struct PERF_PSTATE_35_CLOCK_ENTRY {
    /*!
     * Clock tuple defining the minimum frequencies for a clock domain's
     * Pstate range.
     */
    LW2080_CTRL_PERF_PSTATE_CLOCK_FREQUENCY min;

    /*!
     * Clock tuple defining the maximum frequencies for a clock domain's
     * Pstate range.
     */
    LW2080_CTRL_PERF_PSTATE_CLOCK_FREQUENCY max;

    /*!
     * Clock tuple defining the nominal frequency in  a clock domain's
     * Pstate range.
     *
     * @note nom should be less or equal to
     * LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY::max and greater or equal to
     * LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY::min
     */
    LW2080_CTRL_PERF_PSTATE_CLOCK_FREQUENCY nom;
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35_FMAX_AT_VMIN)
    /*!
     * CLOCK STATUS data for the maximum frequency supported by Vmin at this PSTATE.
     */
    LwU32                                   freqMaxAtVminkHz;
#endif
} PERF_PSTATE_35_CLOCK_ENTRY;
typedef struct PERF_PSTATE_35_CLOCK_ENTRY *PPERF_PSTATE_35_CLOCK_ENTRY;

/*!
 * @brief PSTATE_35 represents a single PSTATE entry in PSTATE 3.5.
 *
 * @extends PSTATE_3X
 */
struct PSTATE_35
{
    /*!
     * @brief PSTATE_3X parent class.
     *
     * Must be first element of the structure!
     *
     * @protected
     */
    PSTATE_3X   super;

    /*!
     * @brief Index into PCIE Table.
     *
     * Default value = 0xFF (Invalid).
     *
     * @protected
     */
    //! https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/LowPower/LPWR_VBIOS_Table#PCIE_Table
    LwU8        pcieIdx;

    /*!
     * @brief Index into LWLINK Table.
     *
     * Default value = 0xFF (Invalid).
     *
     * @protected
     */
     //! https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/LowPower/LPWR_VBIOS_Table#LWLINK_Table
    LwU8        lwlinkIdx;

    /*!
     * @brief Array of clock entries; one entry per @ref CLK_DOMAIN.
     *
     * See @ref LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY.
     *
     * Bounded in size by Perf.pstates.numClkDomains.
     *
     * @private
     */
    PERF_PSTATE_35_CLOCK_ENTRY *pClkEntries;

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35_FMAX_AT_VMIN)
    /*!
     * @brief Array of volt entries
     *
     * Bounded in size by LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS
     *
     * @private
     */
    LW2080_CTRL_PERF_PSTATE_VOLT_RAIL *pVoltEntries;
#endif
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ BOARDOBJ Interfaces ----------------------------- */
BoardObjGrpIfaceModel10ObjSet   (perfPstateGrpIfaceModel10ObjSet_35)
    GCC_ATTRIB_SECTION("imem_libPstateBoardObj", "perfPstateGrpIfaceModel10ObjSet_35");
BoardObjIfaceModel10GetStatus       (pstateIfaceModel10GetStatus_35)
    GCC_ATTRIB_SECTION("imem_libPstateBoardObj", "pstateIfaceModel10GetStatus_35");

/* ------------------------ PSTATE Interfaces ------------------------------- */
mockable PerfPstateClkFreqGet   (perfPstateClkFreqGet_35)
    GCC_ATTRIB_SECTION("imem_perfVf", "perfPstateClkFreqGet_35");

/* ------------------------- Macros ----------------------------------------- */
/*!
 * Helper macro to get a pointer to the volt rail entries
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LW2080_CTRL_PERF_PSTATE_VOLT_RAIL*
perfPstateVoltEntriesGetIdxRef
(
    PSTATE_35 *pPstate35,
    LwBoardObjIdx idx
)
{
    PMU_HALT_COND(idx < LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS);
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35_FMAX_AT_VMIN)
    return &pPstate35->pVoltEntries[idx];
#else
    return NULL;
#endif
}

/*!
 * Helper macro to get a pointer to freqMaxAtVminkHz
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwU32*
perfPstateClkEntryFmaxAtVminGetRef
(
    PPERF_PSTATE_35_CLOCK_ENTRY pClkEntry
)
{
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35_FMAX_AT_VMIN)
    return &pClkEntry->freqMaxAtVminkHz;
#else
    return NULL;
#endif
}

/* ------------------------ Include Derived Types --------------------------- */
#include "perf/35/pstate_35_infra.h"

/* ------------------------ End of File ------------------------------------- */
#endif // G_PSTATE_35_H
#endif // PSTATE_35_H
