/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    vfe_equ_monitor.h
 * @brief   VFE_EQU_MONITOR class interface.
 */

#ifndef VFE_EQU_MONITOR_H
#define VFE_EQU_MONITOR_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_var.h"
#include "perf/3x/vfe_equ.h"
#include "boardobj/boardobjgrpmask.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief Accessor to the PMU VFE_EQU_MONITOR structure to be used by other
 * tasks.
 */
#define PERF_PMU_VFE_EQU_MONITOR_GET()                                         \
    &PmuEquMonitor

/*!
 * Value representing an invalid index into a VFE_EQU_MONITOR object.
 */
#define VFE_EQU_MONITOR_IDX_ILWALID  LW_U8_MAX

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */

/*!
 * @brief The VFE_EQU Monitor structure.
 *
 * A collection of VFE_EQUs to evaluate and cache at every VFE ilwalidation.
 * These are used so that clients can query the results without needing to
 * evaluate VFE equations (nor load all the VFE IMEM and DMEM) inline within
 * their own code, but can rather fetch the cached value directly from this
 * structure.
 */
typedef struct
{
    /*!
     * @brief Mask of VFE_EQU_MONITOR entries which are valid within the @ref
     * monitor array.
     */
    BOARDOBJGRPMASK_E32 equMonitorMask;

    /*!
     * @brief Mask of VFE_EQU_MONITOR entries which are "dirty" - i.e. do not
     * contain a valid cached value. Will always be a subset of @ref
     * equMonitorMask.
     */
    BOARDOBJGRPMASK_E32 equMonitorDirtyMask;

    /*!
     * @brief Array of VFE Equ Monitors requested by the RM.  Has valid entries
     * per @ref equMonitorMask.
     */
    RM_PMU_PERF_VFE_EQU_EVAL monitor[RM_PMU_PERF_VFE_EQU_MONITOR_COUNT_MAX];
} VFE_EQU_MONITOR;

/*!
 * @brief Initializes VFE_EQU_MONITOR structure.
 *
 * @param[in]  pMonitor   VFE_EQU_MONITOR pointer
 */
#define VfeEquMonitorInit(fname) void (fname)(VFE_EQU_MONITOR *pMonitor)

/*!
 * @brief Imports VFE_EQU_MONITOR settings from the RM.
 *
 * @param[in]  pMonitor   VFE_EQU_MONITOR pointer
 * @param[in]  pRmMonitor RM_PMU_PERF_VFE_EQU_MONITOR_SET pointer to structure to import.
 *
 * @return FLCN_OK
 *     RM VFE_EQU_MONITOR successfully imported from RM.
 * @return Other errors
 *     An unexpected error oclwrred.
 */
#define VfeEquMonitorImport(fname) FLCN_STATUS (fname)(VFE_EQU_MONITOR *pMonitor, RM_PMU_PERF_VFE_EQU_MONITOR_SET *pRmMonitor)

/*!
 * @brief Evaluates and caches result of all VFE Equation Monitors.
 *
 * @pre Client MUST grab perf read semaphore
 *
 * @param[in]  pMonitor     VFE_EQU_MONITOR pointer
 * @param[in]  bIlwalidate
 *     Boolean to ilwalidate/dirty the entire VFE_EQU_MONITOR, forcing all
 *     entries to be re-cached.
 *
 * @return FLCN_OK
 *     VFE_EQU_MONITOR successfully updated
 * @return Other errors
 *     An unexpected error oclwrred.
 */
#define VfeEquMonitorUpdate(fname) FLCN_STATUS (fname)(VFE_EQU_MONITOR *pMonitor, LwBool bIlwalidate)

/*!
 * @brief Retrieves the cached result of a VFE_EQU from the index within the
 * VFE_EQU_MONITOR.
 *
 * @param[in]  pMonitor   VFE_EQU_MONITOR pointer
 * @param[in]  idx        Index within VFE_EQU_MONITOR to retrieve.
 * @param[out] pResult
 *     Pointer to RM_PMU_PERF_VFE_EQU_RESULT structure in which to return the
 *     result.
 *
 * @return FLCN_OK
 *     VFE_EQU_MONITOR result successfully retrieved.
 * @return Other errors
 *     An unexpected error oclwrred.
 */
#define VfeEquMonitorResultGet(fname) FLCN_STATUS (fname)(VFE_EQU_MONITOR *pMonitor, LwU8 idx, RM_PMU_PERF_VFE_EQU_RESULT *pResult)

/*!
 * Acquires a VFE_EQU_MONITOR::monitor[] entry for the caller and stores the
 * provided VFE_EQU evaluation parameters in it.  Returns the index of the
 * acquired entry to the caller so that the value can be later retrieved via
 * @ref VfeEquMonitorResultGet().
 *
 * @note VfeEquMonitorResultGet() will return an error for the returned index
 * until the VFE_EQU_MONITOR is updated (i.e. re-cached) via @ref
 * VfeEquMonitorUpdate().
 *
 * @param[in]   pMonitor   VFE_EQU_MONITOR pointer
 * @param[out]  pIdx
 *     Pointer in which to return the index of the acquired
 *     @ref VFE_EQU_MONITOR::monitor[].
 *
 * @return FLCN_OK
 *     VFE_EQU_MONITOR successfully acquired.
 * @return Other errors
 *     An unexpected error oclwrred.
 */
#define VfeEquMonitorAcquire(fname) FLCN_STATUS (fname)(VFE_EQU_MONITOR *pMonitor, LwU8 *pIdx)

/*!
 * @brief Sets the evaluation data (VFE_EQU index, input VFE_VAR values, etc.)
 * of a @ref VFE_EQU_MONITOR::monitor[] entry at the specified @ref idx.
 *
 * @note VfeEquMonitorResultGet() will return an error for the specified index
 * until the VFE_EQU_MONITOR is updated (i.e. re-cached) via @ref
 * VfeEquMonitorUpdate().
 *
 * @param[in]   pMonitor   VFE_EQU_MONITOR pointer
 * @param[in]   idx
 *    Index of the acquired @ref VFE_EQU_MONITOR::monitor[] to be set.
 * @param[in]   equIdx
 *    Index of the @ref VFE_EQU object to monitor with this
 *    VFE_EQU_MONITOR::monitor[] entry.
 * @param[in]   outputType
 *    Expected equation's result type as @ref
 *    LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_<xyz>.
 * @param[in]   varCount
 *    Number of variables specified in the @ref pValues parameter.
 * @param[in]   pVarValues
 *    Pointer to an array of @ref RM_PMU_PERF_VFE_VAR_VALUE structures to be
 *    used as caller-specified @ref VFE_VAR when evaluate the @ref VFE_EQU.
 *    This array will be copied into the @ref
 *    VFE_EQU_MONITOR::monitor[].varValues[] array.  The range of this array is
 *    specified by the @ref varCount parameter.
 *
 * @return FLCN_OK
 *     VFE_EQU_MONITOR successfully acquired.
 * @return Other errors
 *     An unexpected error oclwrred.
 */
#define VfeEquMonitorSet(fname) FLCN_STATUS (fname)(VFE_EQU_MONITOR *pMonitor, LwU8 idx, LwVfeEquIdx equIdx, LwU8 outputType, LwU8 varCount, RM_PMU_PERF_VFE_VAR_VALUE *pVarValues)

/*!
 * @brief Releases a previously acquired a VFE_EQU_MONITOR::monitor[] entry so that it
 * may be reused by another client.
 *
 * @param[in]   pMonitor   VFE_EQU_MONITOR pointer
 * @param[in]   idx
 *    Index of the acquired @ref VFE_EQU_MONITOR::monitor[] to be released.
 *
 * @return FLCN_OK
 *     VFE_EQU_MONITOR successfully released.
 * @return Other errors
 *     An unexpected error oclwrred.
*/
#define VfeEquMonitorRelease(fname) FLCN_STATUS (fname)(VFE_EQU_MONITOR *pMonitor, LwU8 idx)

/* ------------------------ External Definitions ---------------------------- */
extern VFE_EQU_MONITOR PmuEquMonitor;

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// VFE_EQU_MONITOR interfaces
VfeEquMonitorInit      (vfeEquMonitorInit)
    GCC_ATTRIB_SECTION("imem_libPerfInit", "vfeEquMonitorInit");
VfeEquMonitorImport    (vfeEquMonitorImport);
VfeEquMonitorUpdate    (vfeEquMonitorUpdate);
VfeEquMonitorResultGet (vfeEquMonitorResultGet)
    GCC_ATTRIB_SECTION("imem_perfVfeEquMonitor", "vfeEquMonitorResultGet");
VfeEquMonitorAcquire  (vfeEquMonitorAcquire)
    GCC_ATTRIB_SECTION("imem_perfVfeEquMonitor", "vfeEquMonitorAcquire");
VfeEquMonitorSet      (vfeEquMonitorSet)
    GCC_ATTRIB_SECTION("imem_perfVfeEquMonitor", "vfeEquMonitorSet");
VfeEquMonitorRelease  (vfeEquMonitorRelease)
    GCC_ATTRIB_SECTION("imem_perfVfeEquMonitor", "vfeEquMonitorRelease");


#endif // VFE_EQU_MONITOR_H
